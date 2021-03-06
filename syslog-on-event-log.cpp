# if !defined( _WIN32 )
#   error This source must be compiled for Windows platform only.
# endif

// syslog-win32.cpp: определяет точку входа для консольного приложения.
//
# include "syslog.h"
# include <windows.h>
# include <string>
# include <mutex>

namespace syslog_win32_impl
{
  class syslog
  {
    using mutex_type = std::recursive_mutex;

    struct ToSuccess
    {
      auto  operator ()( int ) const
        {  return (WORD)EVENTLOG_SUCCESS;  }
    };

    template <int src, WORD map, class next = ToSuccess>
    struct Prio2Type
    {
      auto  operator ()( int priority ) const
        {  return priority == src ? map : next()( priority );  }
    };

  public:
    syslog();
   ~syslog();

  public:
    void  close();
    int   maskl( int );
    void  openl( const char* ident, int option, int facility );
    void  print( int priority, const char* format, va_list );

  protected:
    auto  ident( const char* ) const -> std::string;
    auto  print( const char*, va_list ) const -> std::string;

  private:
    mutex_type  _mtx;
    HANDLE      _log;
    int         _msk;     // allowed priorities mask
    int         _opt;

  };

  syslog::syslog(): _log( (HANDLE)NULL ), _msk( -1 ), _opt( 0 )
  {
  }

  syslog::~syslog()
  {
    close();
  }

  void  syslog::close()
  {
    std::unique_lock<mutex_type>  _mtc;

    if ( _log != nullptr )
      DeregisterEventSource( _log );
    _log = (HANDLE)NULL;
  }

  int   syslog::maskl( int mask )
  {
    std::unique_lock<mutex_type>  _lck( _mtx );
    auto                          omsk( _msk );

    return (_msk = mask, omsk);
  }

  void  syslog::openl( const char* id, int option, int facility )
  {
    auto                          _idn = ident( id );
    std::unique_lock<mutex_type>  _lck( _mtx );

    close();

    _log = RegisterEventSourceA( (LPCSTR)NULL, (LPCSTR)_idn.c_str() );
    _opt = option;
  }

  //
  // 1. check if message priority allowed
  // 2. ensure the log is open
  // 3. create log message
  // 4. copy to stderr if needed
  // 5. ReportEvent
  void  syslog::print( int priority, const char* format, va_list ap )
  {
    std::unique_lock<mutex_type>  _lck( _mtx );

    if ( (_msk & LOG_MASK( priority )) != 0 )
    {
      if ( _log == (HANDLE)NULL )
        openl( nullptr, LOG_PID, 0 );

      auto  msg = print( format, ap );

      if ( (_opt & LOG_PERROR) != 0 )
        fprintf( stderr, "%s\n", msg.c_str() );

      if ( _log != nullptr )
      {
        LPCSTR  msgptr = msg.c_str();
        auto    evtype = Prio2Type<LOG_EMERG,   EVENTLOG_ERROR_TYPE,
                         Prio2Type<LOG_ALERT,   EVENTLOG_ERROR_TYPE,
                         Prio2Type<LOG_CRIT,    EVENTLOG_ERROR_TYPE,
                         Prio2Type<LOG_ERR,     EVENTLOG_ERROR_TYPE,
                         Prio2Type<LOG_WARNING, EVENTLOG_WARNING_TYPE,
                         Prio2Type<LOG_NOTICE,  EVENTLOG_INFORMATION_TYPE,
                         Prio2Type<LOG_INFO,    EVENTLOG_INFORMATION_TYPE,
                         Prio2Type<LOG_DEBUG,   EVENTLOG_AUDIT_SUCCESS>>>>>>>>()( priority );

        if ( !ReportEventA( _log, evtype, (WORD)0, 1, NULL, 1, 0, &msgptr, NULL ) )
        {
          fprintf( stdout, "%s\n", msg.c_str() );
        }
      }
    }
  }

  //
  // 1. ensure identity is defined and has apropriate form for the call to RegisterEventSource
  // 2. replace slashes by underbars
  //
  auto  syslog::ident( const char* sz ) const -> std::string
  {
    std::string out;
    size_t      pos;

    if ( sz == nullptr )
    {
      out.resize( MAX_PATH );

      if ( FAILED( GetModuleFileNameA( GetModuleHandleA( (LPCSTR)NULL ), (LPSTR)out.c_str(), (DWORD)out.size() ) ) )
        out = "__undefined_module__";

      out.resize( strlen( out.c_str() ) );

      if ( (pos = out.rfind( '\\' )) != out.npos )
        out = out.substr( pos + 1 );

      if ( (pos = out.rfind( '/' )) != out.npos )
        out = out.substr( pos + 1 );
    }
      else
    out = std::string( sz );

    while ( (pos = out.find( '/' )) != out.npos )
      out.replace( pos, 1, 1, '_' );
    while ( (pos = out.find( '\\' )) != out.npos )
      out.replace( pos, 1, 1, '_' );

    return (out.shrink_to_fit(), std::move( out ));
  }

  auto  syslog::print( const char* format, va_list ap ) const -> std::string
  {
    std::string msg( 0x100, ' ' );
    size_t      len = msg.size();
    size_t      cch = vsnprintf( (char*)msg.c_str(), len, format, ap );

    if ( cch >= len )
    {
      msg.resize( cch + 1 );
      vsnprintf( (char*)msg.c_str(), msg.size(), format, ap );
    }

    msg.resize( strlen( msg.c_str() ) );

    if ( (_opt & LOG_PID) != 0 )
    {
      std::string pid( 0x20, ' ' );

      snprintf( (char*)pid.c_str(), pid.size(), "%u", GetCurrentProcessId() );
        pid.resize( strlen( pid.c_str() ) );
      return std::move( pid + ": " + msg );
    }

    return std::move( msg );
  }

  syslog  log;

}

void	closelog(void)
{
  syslog_win32_impl::log.close();
}

void	openlog(const char* ident, int option, int facility)
{
  syslog_win32_impl::log.openl( ident, option, facility );
}

int	  setlogmask( int mask )
{
  return syslog_win32_impl::log.maskl( mask );
}

void	syslog(int priority, const char* format, ... )
{
  va_list ap;

  va_start( ap, format );
  syslog_win32_impl::log.print( priority, format, ap );
  va_end( ap );
}

void	vsyslog(int priority, const char* format, va_list ap )
{
  syslog_win32_impl::log.print( priority, format, ap );
}
