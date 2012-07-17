#include "raw_query.hpp"

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define	TCP_NODELAY	 1	/* Don't delay send to coalesce packets  */
#define closesocket close
#include <time.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <algorithm>

namespace Network
{	
#	define REQUEST_SIZE 1024
#	define SEND_BUFFER_SIZE 2048
#	define RECV_BUFFER_SIZE 2048

  inline bool would_block(int r)
  {
#ifdef WIN32
    int e = WSAGetLastError();
    return r == -1 && (e == WSAEWOULDBLOCK || e == WSAEINPROGRESS);
#else
    int e = errno;
    return r == -1 && (e == EAGAIN || e == EINPROGRESS);
#endif
  }	

  RawStream::RawStream(const char* hostname, const char* port)
    : _socket(0), _resolved(0), _state(state_none), _error(STREAM_NO_ERROR), _hostname(hostname), _port(port), _no_more_data(false), _timeout(-1)
  {
  }

  RawStream::~RawStream()
  {
    close();
  }

  void RawStream::disconnect()
  {
    close();
  }

  void RawStream::close()
  {
    if (_resolved)
    {
      freeaddrinfo((addrinfo*)_resolved);
      _resolved = 0;
    }

    if (_socket)
    {
      ::shutdown(_socket, 2);
      ::closesocket(_socket);
      _socket = 0;
    }
  }  

  bool RawStream::resolve()
  {
    _state = state_none;

    close();

    if (!_resolved)
    {
      addrinfo hints = { 0 }, *resolved = 0;
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;

      if (getaddrinfo(_hostname.c_str(), _port.c_str(), &hints, &resolved)) 
      {
        _resolved = 0;

        set_error(STREAM_TCP_ERROR, "Could not resolve destination address");
      }
      else      
      {
        _resolved = (addrinfo*)resolved;
      }
    }

    if (_resolved)
    {
      addrinfo* resolved = (addrinfo*)_resolved;
      _socket = ::socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);

#ifdef WIN32
      unsigned long mode = 1;
      if (ioctlsocket( _socket, FIONBIO, &mode )!= 0)
      {
        _resolved = 0;

        set_error(STREAM_TCP_ERROR, "Could not initiate network connection.");
      }
#else
      int mode;
      mode = fcntl(_socket, F_GETFL, 0);
      fcntl(_socket, F_SETFL, mode | O_NONBLOCK);
#endif
      
      int nodelay = 1;
      setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(int));	
    }

    return _resolved != 0;
  }

  bool RawStream::connect()
  {
    if (_resolved)
    {
      int r = ::connect( _socket, ((addrinfo*)_resolved)->ai_addr, ((addrinfo*)_resolved)->ai_addrlen);

      bool error = r < 0 && !would_block(r);
      if (error)
      {
        set_error(STREAM_TCP_ERROR, strerror(r));
      }
    }
    else
      set_error(STREAM_TCP_ERROR, "Operation out of sequence.");
      
    return _error == STREAM_NO_ERROR;
  }

  bool RawStream::update(float timeout)
  {
    if (tcp_error())
      return false;

    if (timeout > 0)
    {
      if (_timeout < 0)
      {
        _timeout = timeout;
        _start = clock();
      }

      float delta = 0.0f;
      delta = (clock() - _start) / (float)CLOCKS_PER_SEC;

      if (delta > _timeout)
      {
        set_error(STREAM_TCP_ERROR, "Request timed out.");

        return false;
      }
    }

    if (_state != state_none) 
    {
      int poll_status = poll();
      if (poll_status & poll_error)
      {
        set_error(STREAM_TCP_ERROR, "Could not check network status.");

        return false;
      }
      
      if (poll_status & poll_send)
      {
        write();

        if (tcp_error())
        {
          set_error(STREAM_TCP_ERROR, "Could not send data.");
        }
      }
      if (poll_status & poll_receive)
      {
        read();

        if (tcp_error())
        {
          set_error(STREAM_TCP_ERROR, "Could not read data.");
        }
      }
    }

    if (_error == STREAM_NO_ERROR && _state == state_none)
    {
      if (resolve() && connect())
      {
        _state = state_connected;
      }
      else
        set_error(STREAM_TCP_ERROR, "Operation out of sequence.");
    }    

    return !tcp_error();
  }

  int RawStream::poll()
  {
    timeval tv;
    tv.tv_sec = 0; tv.tv_usec = 1;

    fd_set read_fds, write_fds, error_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);
#pragma warning(push)
#pragma warning(disable:4127)
    FD_SET(_socket, &read_fds);
    FD_SET(_socket, &write_fds);
    FD_SET(_socket, &error_fds);
#pragma warning(pop)

    int result = select(_socket + 1, &read_fds, &write_fds, &error_fds, &tv);

    int status = 0;
    
    if (result >= 0)
    {
      status |= FD_ISSET(_socket, &read_fds) ? poll_receive : 0;
      status |= FD_ISSET(_socket, &write_fds) ? poll_send : 0;
      status |= FD_ISSET(_socket, &error_fds) ? poll_error : 0;
    }

    return status;
  }

  void RawStream::write(const char* data)
  {
    _request.insert(_request.end(), data, data + strlen(data));
  }

  void RawStream::write(const char* data, const char* end)
  {
    _request.insert(_request.end(), data, end);
  }

  void RawStream::write()
  {
    if (tcp_error())
      return;

    int remaining = _request.size();
    int chunk = std::min(SEND_BUFFER_SIZE, remaining);

    if (chunk)
    {
      int result = ::send(_socket, &_request[0], chunk, 0);
      if (!would_block(result))
      {
        bool error = result < 0;

        if (error)
        { 
          set_error(STREAM_TCP_ERROR, strerror(result));          
        }
      }
        
      if (!tcp_error())
        _request.erase(_request.begin(), _request.begin() + result);
    }    
  }

  void RawStream::read()
  {
    if (tcp_error() || _no_more_data)
      return;

    _buffer.resize(RECV_BUFFER_SIZE);

    int result = recv(_socket, &_buffer[0], _buffer.size(), 0 );   

    if (would_block(result))
      return;

    if (result < 0 )
    {
      set_error(STREAM_TCP_ERROR, strerror(result));

      return;
    }

    if (result == 0)
    {
      _no_more_data = true;
      return;
    }
    
    if ((size_t)result <= _buffer.size())
    {
      _response.insert(_response.end(), _buffer.begin(), _buffer.begin() + result);
    }
  }

  void RawStream::eat(int bytes)
  {
    bytes = std::min(bytes, (int)_response.length());

    _response.erase(_response.begin(), _response.begin() + bytes);
  }

  void initialize()
  {
#ifdef WIN32
    WSADATA info;
    WSAStartup( MAKEWORD(1,1), &info );
#endif
  }

  void deinitialize()
  {
#ifdef WIN32
    WSACleanup();
#endif
  }
}