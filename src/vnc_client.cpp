#include "vnc_client.hpp"

#include "des_local.h"
         
#include "cryptoppmin/rng.h"
#include "cryptoppmin/dh.h"
#include "cryptoppmin/aes.h"
#include "cryptoppmin/modes.h"
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include "cryptoppmin/md5.h"  
#include "cryptoppmin/nbtheory.h"  

#include <iostream>

namespace Network
{	
  VncClient::VncClient(const char* hostname, const char* port)
    : RawStream(hostname, port), _state(vnc_waiting_for_version), _width(0), _height(0), _bpp(0), _keep_framebuffer(false), _framebuffer_version(0)
  {
  }

  VncClient::~VncClient()
  {
  }

  bool VncClient::connected() const
  {
    return _state == vnc_connected;
  }

  const char* VncClient::username() const
  {
    return _username.c_str();
  }

  const char* VncClient::password() const
  {
    return _password.c_str();
  }

  void VncClient::set_password(const char* password)
  {
    _password = password;

    if (error_code() == STREAM_VNC_PASSWORD_REQUIRED)
      reset_error();
  }

  void VncClient::set_password(const char* username, const char* password)
  {
    _username = username;
    _password = password;

    if (error_code() == STREAM_VNC_PASSWORD_REQUIRED)
      reset_error();
    if (error_code() == STREAM_VNC_USERNAME_PASSWORD_REQUIRED)
      reset_error();
  }

  void VncClient::set_keep_framebuffer(bool keep)
  {
    _keep_framebuffer = keep;
  }

  int VncClient::framebuffer_width() const
  {
    return _width;
  }

  int VncClient::framebuffer_height() const
  {
    return _height;
  }

  int VncClient::framebuffer_bpp() const
  {
    return _bpp;
  }

  int VncClient::framebuffer_version() const
  {
    return _framebuffer_version;
  }

  const char* VncClient::framebuffer() const
  {
    return _framebuffer.size() > 0 ? &_framebuffer[0] : nullptr;
  }

  bool VncClient::update(float timeout)
  {
    if (!RawStream::update(timeout))
      return false;

    if (state() == state_connected)
    {
      switch (_state)
      {
        case vnc_waiting_for_version:
          rfb_wait_for_version();
          break;
        case vnc_waiting_for_security_server:
          rfb_wait_for_security_server();
          break;
        case vnc_waiting_for_security_handshake:
          rfb_wait_for_security_handshake();
          break;
        case vnc_authenticate:
          rfb_authenticate();
          break;
        case vnc_waiting_for_vnc_challenge:
          rfb_wait_for_vnc_challenge();
          break;
        case vnc_waiting_for_ard_challenge:
          rfb_wait_for_ard_challenge();
          break;
        case vnc_waiting_for_security_result:
          rfb_wait_for_security_result();
          break;
        case vnc_waiting_for_protocol_failure_reason:
          rfb_wait_for_protocol_failure_reason();
          break;
        case vnc_initialize:
          rfb_initialize();
          break;
        case vnc_waiting_for_server_initialization:
          rfb_wait_for_server_initialization();
          break;
        case vnc_setup:
          rfb_setup();
          break;
        case vnc_connected:
          rfb_connected();
          break;
        case vnc_protocol_failure:
          break;
      }
    }

    return true;
  }  

  void VncClient::rfb_wait_for_version()
  {
    std::string& r = response();

    if (r.size() >= 12)
    {
      if (r[0] != 'R' || r[1] != 'F' || r[2] != 'B' || r[3] != ' ')
      {
        set_error(STREAM_VNC_PROTOCOL_ERROR, "Unknown remote control protocol.");

        _state = vnc_protocol_failure;        
      }
      else
      {
        // Simply respond with the same protocol version as we've got from server.
        write(&*r.begin(), &*r.begin() + 12);

        char version_hi[] = { r[4] != '0' ? r[4] : (r[5] != '0' ? r[5] : r[6]), 0 };
        char version_lo[] = { r[8] != '0' ? r[8] : (r[9] != '0' ? r[9] : r[10]), 0 };

        _proto_hi_version = atoi(version_hi);
        _proto_lo_version = atoi(version_lo);

        eat(12);        

        if (_proto_hi_version == 3 && _proto_lo_version < 7)
          _state = vnc_waiting_for_security_server;
        else
          _state = vnc_waiting_for_security_handshake;
      }
    }
  }

  void VncClient::rfb_wait_for_security_server()
  {
    std::string& r = response();

    if (r.length() >= 4)
    {
      int security_protocol = *(int *)&*r.begin();
      if (security_protocol == 0)
      {
        set_error(STREAM_VNC_PROTOCOL_ERROR, "Server refused remote control connection.");

        _state = vnc_protocol_failure;        
      }
      else
      {
        _security_type = security_protocol;
        
        _state = vnc_authenticate;

        eat(4);
      }
    }
  }

  void VncClient::rfb_wait_for_security_handshake()
  {
    std::string& r = response();

    if (r.length() > 0)
    {
      int protocol_count = (int)r[0];

      if (protocol_count == 0)
      {        
        set_error(STREAM_VNC_PROTOCOL_ERROR, "Server refused remote control connection.");

        _state = vnc_waiting_for_protocol_failure_reason;

        eat(1);
      }

      if (protocol_count > 0 && (int)r.length() >= 1 + protocol_count)
      {
        int choosen_protocol = -1;

        // Check for preferred authentication type.
        for (int i = 0; i < protocol_count; ++i)
        {
          if (r[i + 1] == 30 /* ARD, Mac authentication */)
          {
            choosen_protocol = i + 1;
            break;
          }
        }

        // Check for supported authentication types.
        if (choosen_protocol < 0) 
        {
          for (int i = 0; i < protocol_count; ++i)
          {
            if (r[i + 1] == 1 /* No authentication */ || r[i + 1] == 2 /* VNC authentication */ || r[i + 1] == 16 /* Tight authentication */)
            {
              choosen_protocol = i + 1;
              break;
            }
          }
        }

        if (choosen_protocol >= 0)
        {
          write(&*r.begin() + choosen_protocol, &*r.begin() + choosen_protocol + 1);

          _security_type = r[choosen_protocol];

          _state = vnc_authenticate;

          eat(protocol_count + 1);
        }
        else
        {
          set_error(STREAM_VNC_PROTOCOL_ERROR, "Server does not support requested authentication mode.");

          eat(protocol_count + 1);
        }
      }
    }
  }

  void VncClient::rfb_authenticate()
  {
    if (_security_type == 1 /* No authentication */)
    {
      rfb_authenticate_none();
    }
    if (_security_type == 2 /* VNC authentication */)
    {
      rfb_authenticate_vnc();
    }
    if (_security_type == 16 /* Tight authentication */) 
    {
      rfb_authenticate_tight();
    }
    if (_security_type == 30 /* ARD authentication */) 
    {
      rfb_authenticate_ard();
    }
  }

  void VncClient::rfb_authenticate_none()
  {
    if (_proto_hi_version == 3 && _proto_lo_version <= 7)
    {
      _state = vnc_initialize;
    }
    else
    {
      _state = vnc_waiting_for_security_result;
    }
  } 

  void VncClient::rfb_authenticate_vnc()
  {
    _state = vnc_waiting_for_vnc_challenge;
  }

  void VncClient::rfb_authenticate_ard()
  {
    _state = vnc_waiting_for_ard_challenge;
  }

  void VncClient::rfb_wait_for_ard_challenge()
  {
    std::string& r = response();
    //char r[261] = "\x0\x2\x0\x80\xff\xff\xff\xff\xff\xff\xff\xff\xc9\x0f\xda\xa2\x21\x68\xc2\x34\xc4\xc6\x62\x8b\x80\xdc\x1c\xd1\x29\x02\x4e\x08\x8a\x67\xcc\x74\x02\x0b\xbe\xa6\x3b\x13\x9b\x22\x51\x4a\x08\x79\x8e\x34\x04\xdd\xef\x95\x19\xb3\xcd\x3a\x43\x1b\x30\x2b\x0a\x6d\xf2\x5f\x14\x37\x4f\xe1\x35\x6d\x6d\x51\xc2\x45\xe4\x85\xb5\x76\x62\x5e\x7e\xc6\xf4\x4c\x42\xe9\xa6\x37\xed\x6b\x0b\xff\x5c\xb6\xf4\x06\xb7\xed\xee\x38\x6b\xfb\x5a\x89\x9f\xa5\xae\x9f\x24\x11\x7c\x4b\x1f\xe6\x49\x28\x66\x51\xec\xe6\x53\x81\xff\xff\xff\xff\xff\xff\xff\xff\x1d\xf4\xb4\x2d\x58\x30\x75\x27\xc7\x23\x1a\x1d\x52\x9c\x8c\x4a\x67\x10\xa8\x28\x68\x97\x70\xc4\x4d\xd7\x06\x4c\xc3\xe2\xe3\xcf\x0d\x06\xb7\xb6\xc5\x70\x0a\x88\xd8\xa3\xba\xaf\xaa\x51\x93\x58\x9e\x51\x05\xbb\x88\x0d\xb6\xb2\xf4\xbc\xbe\xee\x61\x14\xfa\x7c\x3e\x61\x9d\xe8\x49\x2f\x1c\xf4\xe0\xf1\x3d\xb8\x15\x66\x99\x5f\xcf\x3c\x54\x27\x0a\xc0\x2e\xe2\x05\x22\xde\x73\xf3\x67\x5c\xe0\xe0\xf0\x25\xbc\xce\x45\x6a\x62\xb0\xc1\x85\x2d\x1f\x72\xba\x0b\xc1\x64\xcc\x24\x05\x68\x93\x08\x8d\xd2\xd1\x7e\xe2\x4d\x18\xf3";
    
    if (_password.length() == 0 || _username.length() == 0)
    {
      set_error(STREAM_VNC_USERNAME_PASSWORD_REQUIRED, "Your password is needed.");
      return;
    }

    if (r.length() >= 4)
    {
      int generator_value = 256 * (unsigned char)r[0] + (unsigned char)r[1];
      int key_length = 256 * (unsigned char)r[2] + (unsigned char)r[3];

      if (r.length() >= 4 + key_length + key_length)
      {        
        // Create a random number generator.
        CryptoPP::OFB_Mode<CryptoPP::AES>::Encryption rng;
        std::string seed = CryptoPP::IntToString(time(NULL));
		    seed.resize(16);
		    rng.SetKeyWithIV((byte *)seed.data(), 16, (byte *)seed.data());

        // Get parameters from auth message.
        CryptoPP::Integer generator(generator_value);
        CryptoPP::Integer prime_modulus((unsigned char*)&r[4], key_length);
        CryptoPP::Integer peer_public_key((unsigned char*)&r[4 + key_length], key_length);

        CryptoPP::SecByteBlock remote_public(key_length);
        peer_public_key.Encode(remote_public, key_length);

        // Initialize exchange.
        CryptoPP::DH dh;
        dh.AccessGroupParameters().Initialize(prime_modulus, generator);
        
        // Skip validation as it slows down login process.
        //if (!dh.GetCryptoParameters().Validate(rng, 3))
        //{
        //  set_error(STREAM_VNC_LOGIN_FAILED, "Unable to login to server.");
        //  eat(4 + key_length + key_length);
        //  return;
        //}

        // Generate local private/public key pairs.
        CryptoPP::SecByteBlock local_private(dh.PrivateKeyLength());
    	  CryptoPP::SecByteBlock local_public(dh.PublicKeyLength());

        dh.GenerateKeyPair(rng, local_private, local_public);	

        // Attempt agreement.
        CryptoPP::SecByteBlock shared_secret(dh.AgreedValueLength());
        memset(shared_secret.begin(), 0x0, shared_secret.size());        

        if (!dh.Agree(shared_secret, local_private, remote_public, true))
        {
          set_error(STREAM_VNC_LOGIN_FAILED, "Unable to login to server.");
          eat(4 + key_length + key_length);
          return;        
        }

        // Code above is equivalent of:
        // CryptoPP::Integer local_private(rng, (key_length / 8) * 8);
        // CryptoPP::Integer local_public = CryptoPP::ModularExponentiation(generator, local_private, prime_modulus);
        // CryptoPP::Integer shared_secret = CryptoPP::ModularExponentiation(peer_public_key, local_private, prime_modulus);               

        // Generate AES key using MD5 hash of shared secret.
        CryptoPP::SecByteBlock aes_key(CryptoPP::Weak1::MD5::DIGESTSIZE);
        CryptoPP::Weak1::MD5().CalculateDigest(aes_key, shared_secret, shared_secret.size());

        // Encrypt plain text passwords.
        struct 
        {
          char username[64];
          char password[64];
        } auth_message;

        memset(auth_message.username, 0, 64);
        memset(auth_message.password, 0, 64);
        auth_message.username[0] = '\0';
        auth_message.password[0] = '\0';

        strncat(auth_message.username, username(), 63);
        strncat(auth_message.password, password(), 63);

        CryptoPP::ECB_Mode<CryptoPP::AES>::Encryption encryption(aes_key, CryptoPP::Weak1::MD5::DIGESTSIZE);
        encryption.ProcessData((unsigned char*)&auth_message, (unsigned char*)&auth_message, sizeof(auth_message));

        write((char*)&auth_message, (char*)&auth_message + sizeof(auth_message));

        // Decrypt public key.
        CryptoPP::Integer local_public_key;        
        local_public_key.Decode(local_public.BytePtr(), local_public.SizeInBytes());

        // Send back public key.
        for (int length = local_public_key.ByteCount() - 1, byte = 0; length >= 0; --length, ++byte)
        {
          char ch = local_public_key.GetByte(length);
          write(&ch, &ch + 1);
        }

        // Remove our parameters.
        eat(4 + key_length + key_length);

        _state = vnc_waiting_for_security_result;
      }
    }
  }

  void VncClient::rfb_wait_for_vnc_challenge()
  {
    std::string& r = response();

    if (r.length() >= 16)
    {
      if (_password.length() == 0)
      {
        set_error(STREAM_VNC_PASSWORD_REQUIRED, "Your password is needed.");
      }
      else
      {
        // Encrypt challenge with password.

        // Reverse bit order in the key.
        unsigned char key[8];

        for (size_t i = 0; i < 8; ++i)
        {
          if (i < _password.length())
            key[i] = (unsigned char)_password[i];
          else
            key[i] = 0;
        }        

        // Encrypt.
        deskey(key, EN0);

        unsigned char challenge[16];
        for (int i = 0; i < 16; ++i)
          challenge[i] = (unsigned char)r[i];

        for (int i = 0; i < 16; i += 8)
          des(challenge + i, challenge + i);

        // Send it back.
        write((char *)challenge, (char *)challenge + 16);

        eat(16);

        _state = vnc_waiting_for_security_result;
      }
    }
  }

  void VncClient::rfb_authenticate_tight()
  {
  }

  void VncClient::rfb_wait_for_security_result()
  {
    std::string& r = response();

    if (r.length() >= 4)
    {
      unsigned int security_result = byte_swap(*(unsigned int *)&*r.begin());
      if (security_result == 0)
      { 
        _state = vnc_initialize;

        eat(4);
      }
      else
      {
        if (security_result == 1)
          set_error(STREAM_VNC_LOGIN_FAILED, "Unable to login to server.");
        
        if (security_result == 2)
          set_error(STREAM_VNC_LOGIN_FAILED, "Too many attempts to login to server.");

        _state = vnc_waiting_for_protocol_failure_reason;

        eat(4);
      }
    }
  }

  void VncClient::rfb_wait_for_protocol_failure_reason()
  {
    //TODO: For now, just go into failure mode.
    _state = vnc_protocol_failure;

    if (_proto_hi_version == 3 && _proto_lo_version <= 7)
    {
      _state = vnc_protocol_failure;
    }
    else
    {
      std::string& r = response();

      if (r.length() >= 4)
      {
        unsigned int length = byte_swap(*(unsigned int *)&*r.begin());

        if (r.length() >= 4 + length)
        {
          _message.assign(&r[4], &r[4] + length);

          _state = vnc_protocol_failure;

          eat(4 + length);
        }
      }    
    }
  }

  void VncClient::rfb_initialize()
  {
    // Ask for shared session.
    char shared[] = { 1 };

    write(shared, shared + 1);

    _state = vnc_waiting_for_server_initialization;
  }

  void VncClient::rfb_wait_for_server_initialization()
  {
    std::string& r = response();

    if (r.length() >= 24)
    {
      unsigned int name_length = byte_swap(*(unsigned int *)(&*r.begin() + 20));
      if (r.length() >= 24 + name_length)
      {
        _width = (int)(byte_swap(*(unsigned short *)(&*r.begin() + 0)));
        _height = (int)(byte_swap(*(unsigned short *)(&*r.begin() + 2)));

        _bpp = (int)(*(unsigned char *)(&*r.begin() + 4)) / 8;

        _name.assign(r.begin() + 24, r.begin() + 24 + name_length);

        _state = vnc_setup;

        eat(24 + name_length);
      }
    }
  }

  void VncClient::rfb_setup()
  {
    // Set up raw encoding type. We don't ask for screens anyway.
    char encoding[] = { 2, 0, 0, 1, 0, 0, 0, 0 };

    write(encoding, encoding + sizeof(encoding) / sizeof(char));

    _state = vnc_connected;
  }

  void VncClient::rfb_connected()
  {
    std::string& r = response();

    if (r.length() >= 1)
    {
      switch (r[0])
      {
        case 0: /* Framebuffer update */
          rfb_framebuffer_update();
          break;
        case 1: /* Colormap entries */
          rfb_set_color_map();
          break;
        case 2: /* Bell */
          rfb_bell();
          break;
        case 3: /* Clipboard */
          rfb_set_clipboard();
          break;
        default:
          set_error(STREAM_VNC_UNSUPPORTED, "Server sent unsupported message.");
          _state = vnc_protocol_failure;
          break;
      }
    }    
  }

  void VncClient::rfb_framebuffer_update()
  {
    std::string& r = response();

    if (r.length() >= 4)
    {
      int length = (int)byte_swap(*(unsigned short *)(&*r.begin() + 2));

      bool complete = true;

      int current = 4;

      for (; length > 0; --length)
      {
        if (r.length() >= current + 12)
        { 
          int x = byte_swap(*(unsigned short *)(&*r.begin() + current + 0));
          int y = byte_swap(*(unsigned short *)(&*r.begin() + current + 2));
          int width = byte_swap(*(unsigned short *)(&*r.begin() + current + 4));
          int height = byte_swap(*(unsigned short *)(&*r.begin() + current + 6));
          int type = byte_swap(*(unsigned int *)(&*r.begin() + current + 8));

          if (type != 0)
          {
            set_error(STREAM_VNC_UNSUPPORTED, "Server sent unsupported message.");

            _state = vnc_protocol_failure;

            complete = false;
            
            break;
          }
          else
          {
            int pixel_length = width * height * _bpp;
            if (r.length() >= current + 12 + pixel_length)
            {
              current += 12 + pixel_length;

              // Copy pixel data.
              if (_keep_framebuffer) 
              {
                if (_framebuffer.size() < _width * _height * _bpp)
                  _framebuffer.resize(_width * _height * _bpp);

                int left = std::min(x, _width);
                int bytes_per_line = (std::min(x + width, _width) - left) * _bpp;;

                for (int top = y, last_top = std::min(_height, y + height); top < last_top; ++top)
                  std::memcpy(&_framebuffer[0] + (top * _width + left) * _bpp, r.data() + (top - y) * width * _bpp, bytes_per_line);                

                ++_framebuffer_version;
              }              
            }
            else
            {
              complete = false;

              break;
            }
          }
        }
        else
        {
          complete = false;
          break;
        }
      }

      if (complete)
        eat(current);
    }
  }

  void VncClient::rfb_set_color_map()
  {
    std::string& r = response();

    if (r.length() >= 6)
    {
      unsigned short length = byte_swap(*(unsigned short *)(&*r.begin() + 4));

      if (r.length() >= 6 + length * 6)
      {
        eat(6 + length * 6);
      }
    }
  }
   
  void VncClient::rfb_bell()
  {    
    std::string& r = response();

    if (r.length() >= 8)
    {
      eat(1);
    }
  }
   
  void VncClient::rfb_set_clipboard()
  {
    std::string& r = response();

    if (r.length() >= 8)
    {
      unsigned int length = byte_swap(*(unsigned int *)(&*r.begin() + 4));
      if (r.length() >= 8 + length)
      {
        eat(8 + length);
      }
    }
  }

  void VncClient::pulse_key(unsigned short key)
  {
    send_key(key, true);
    send_key(key, false);
  }
  
  void VncClient::send_key(unsigned short key, bool down)
  {    
    char key_event[] = { 4, (char)(down ? 1 : 0), 0, 0, 0, 0, (char)((key & 0xff00) >> 8), (char)(key & 0x00ff) };

    write(key_event, key_event + sizeof(key_event) / sizeof(char));
  }

  void VncClient::request_screen(bool incremental, int x, int y, int width, int height)
  {
    char frame_event[] = {
      3,
      (char)(incremental ? 1 : 0),
      (char)((x & 0xff00) >> 8),
      (char)(x & 0xff),
      (char)((y & 0xff00) >> 8),
      (char)(y & 0xff),
      (char)((width & 0xff00) >> 8),
      (char)(width & 0xff),
      (char)((height & 0xff00) >> 8),
      (char)(height & 0xff)
    };

    write(frame_event, frame_event + sizeof(frame_event) / sizeof(char));
  }

  unsigned int VncClient::byte_swap(unsigned int v)
  {
    return 
      ((v & 0x000000ff) << 24) |
      ((v & 0x0000ff00) << 8) | 
      ((v & 0x00ff0000) >> 8) |
      ((v & 0xff000000) >> 24);
  }

  unsigned short VncClient::byte_swap(unsigned short v)
  {
    return 
      ((v & 0x00ff) << 8) |
      ((v & 0xff00) >> 8);
  }
}