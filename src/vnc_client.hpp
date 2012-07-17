#ifndef header_92665f2e_2fa1_4d1c_9394_5746d6d04aeb
#define header_92665f2e_2fa1_4d1c_9394_5746d6d04aeb

#include "raw_query.hpp"

#define STREAM_VNC_PROTOCOL_ERROR (STREAM_TCP_RANGE + 1)
#define STREAM_VNC_PASSWORD_REQUIRED (STREAM_TCP_RANGE + 2)
#define STREAM_VNC_USERNAME_PASSWORD_REQUIRED (STREAM_TCP_RANGE + 5)
#define STREAM_VNC_LOGIN_FAILED (STREAM_TCP_RANGE + 3)
#define STREAM_VNC_UNSUPPORTED (STREAM_TCP_RANGE + 4)

namespace Network
{
  class VncClient: public RawStream
  {
  public: 
    enum VncState
    {      
      vnc_waiting_for_version,
      vnc_waiting_for_security_server,
      vnc_waiting_for_security_handshake,
      vnc_authenticate,
      vnc_waiting_for_vnc_challenge,
      vnc_waiting_for_security_result,
      vnc_initialize,
      vnc_waiting_for_protocol_failure_reason,
      vnc_waiting_for_server_initialization,
      vnc_waiting_for_ard_challenge,
      vnc_setup,
      vnc_connected,
      vnc_protocol_failure
    };

  public:
    VncClient(const char* hostname, const char* port);
    virtual ~VncClient();

    bool update(float timeout = -1);

    const char* password() const;
    const char* username() const;

    void set_password(const char* password);
    void set_password(const char* username, const char* password);

    bool connected() const;

    void pulse_key(unsigned short key);
    void send_key(unsigned short key, bool down);

    void request_screen(bool incremental, int x, int y, int width, int height);

    void set_keep_framebuffer(bool keep);

    int framebuffer_width() const;

    int framebuffer_height() const;

    int framebuffer_bpp() const;

    int framebuffer_version() const;

    const char* framebuffer() const;

  private:
    void rfb_wait_for_version();
    void rfb_wait_for_security_server();
    void rfb_wait_for_security_handshake();
    void rfb_authenticate();
    void rfb_authenticate_none();
    void rfb_authenticate_vnc();
    void rfb_authenticate_tight();
    void rfb_authenticate_ard();
    void rfb_wait_for_vnc_challenge();
    void rfb_wait_for_ard_challenge();
    void rfb_wait_for_security_result();
    void rfb_wait_for_protocol_failure_reason();
    void rfb_initialize();
    void rfb_wait_for_server_initialization();
    void rfb_setup();
    void rfb_connected();
    void rfb_framebuffer_update();
    void rfb_set_color_map();
    void rfb_bell();
    void rfb_set_clipboard();

    unsigned int byte_swap(unsigned int v);
    unsigned short byte_swap(unsigned short v);

  private:
    VncState _state;

    int _proto_lo_version;
    int _proto_hi_version;

    int _security_type;

    bool _keep_framebuffer;

    int _width;
    int _height;
    int _bpp;
    int _framebuffer_version;

    std::string _name;

    std::string _username;
    std::string _password;

    std::string _message;

    std::vector<char> _framebuffer;
  }; 
}

#endif