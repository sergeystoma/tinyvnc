#ifndef header_57369b1d_4c85_4166_ad72_023684e1cd4d
#define header_57369b1d_4c85_4166_ad72_023684e1cd4d

#include <time.h>

#include <string>

#include <vector>

#define STREAM_NO_ERROR 0
#define STREAM_TCP_ERROR 1
#define STREAM_TCP_RANGE 1000

namespace Network
{
#ifdef WIN32
  typedef size_t Socket;
#else
  typedef int Socket;
#endif

  class RawStream
  {
  public: 
    enum State
    {
      state_none = 0,
      state_handshake = 1,      
      state_connected = 2
    };

    enum PollStatus
    {
      poll_receive = 1,
      poll_send = 2,
      poll_error = 4
    };

  public:
    RawStream(const char* hostname, const char* port);
    virtual ~RawStream();

    bool update(float timeout = -1);

    int error_code() const
    {
      return _error;
    }    

    const char* error_description() const
    {
      return _error_description.c_str();
    }

  protected:
    State state() const
    {
      return _state;
    }

    const char* hostname() const
    {
      return _hostname.c_str();
    }

    bool no_more_data() const
    {
      return _no_more_data;
    }    

    std::string& response()
    {
      return _response;
    }

    void set_error(int code, const char* description)
    {
      if (!_error_description.length())
        _error_description = description;

      _error = code;
    }

    void reset_error()
    {
      _error = STREAM_NO_ERROR;

      _error_description.clear();
    }

    bool tcp_error()
    {
      return _error > 0 && _error <= STREAM_TCP_ERROR;
    }

    void write(const char* data);
    void write(const char* data, const char* end);

    void eat(int bytes);

  private:
    bool resolve();

    void disconnect();

    void close();

    bool connect();

    int poll();

    void write();

    void read();
    
  private:
    State _state; 

    void* _resolved;

    std::string _hostname;
    std::string _port;

    Socket _socket;

    int _error;
    bool _no_more_data;
    
    std::string _error_description;
    std::string _request;
    std::string _buffer;
    std::string _response;    

    float _timeout;
    long long _start;
  };    

  void initialize();
  void deinitialize();
}

#endif