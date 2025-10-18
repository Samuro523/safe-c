
// service.h : background processes (windows-specific)

//------------------------------------------------------------------

// change default max size of trace file for the service.
// must be called before(!) calling create_service.
// default is 64*1024*1024 for 64MB

void set_service_trace_max_file_size (uint max_file_size);

//------------------------------------------------------------------

// this function must be provided by the user. It is used
// to operate the service. It must terminate when 'service_stop^'
// is set to true by windows.

typedef void SERVICE_MAIN (bool^ service_stop);

//------------------------------------------------------------------

// this function should be called immediately within main().
// it sets the current directory using the executable's path
// and opens a trace file.
// it will not return until the service stops.

int create_service (SERVICE_MAIN user_service_function);

//------------------------------------------------------------------

// this function must be called by the user when the
// initialization is done and the service is ready to accept work.

void signal_service_is_ready ();

//------------------------------------------------------------------

// this function must be called by the user when the service is
// about to stop. If this function is not called, then the
// function 'user_service_function' will be called again
// automatically after some delay.

void signal_service_is_stopping ();

//------------------------------------------------------------------

// returns 0 if OK, or a negative value if error.

int add_service    (string computer, string service_name, string executable_name);
int delete_service (string computer, string service_name);
int start_service  (string computer, string service_name);
int stop_service   (string computer, string service_name);

//------------------------------------------------------------------

enum SERVICE_MODE {BOOT, SYSTEM, AUTO, MANUAL, DISABLED};

// returns 0 if OK, or a negative value if error.

int get_service_mode (string computer, string service_name, out SERVICE_MODE mode);
int set_service_mode (string computer, string service_name, SERVICE_MODE mode);

//------------------------------------------------------------------

enum SERVICE_STATE {UNDEFINED, STOPPED, START_PENDING, STOP_PENDING, RUNNING,
                    CONTINUE_PENDING, PAUSE_PENDING, PAUSED};

struct SERVICE_INFO
{
  char          name[32];
  char          display[64];
  SERVICE_STATE state;
}

// returns 0 if OK, or a negative value if error.
// table must be freed after the call

int list_services (    string          computer,
                   out SERVICE_INFO[]^ table);

//------------------------------------------------------------------

#if 0

Example:

  void service_main (bool^ stop_service)
  {
    ... initialize service ...

    signal_service_is_ready ();

    while (!stop_service^)
    {
      ... operate service ...
    }

    signal_service_is_stopping ();

    ... shutdown service ...
  }

  int main ()
  {
    return create_service (service_main);
  }

#endif

//------------------------------------------------------------------
