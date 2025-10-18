
// service.c

use calendar, files, strings, thread, tracing, system, win/windows;

//------------------------------------------------------------------
#begin unsafe
//------------------------------------------------------------------

bool          services_available;
SC_HANDLE     h;                   // service status handle
SERVICE_MAIN  user_function;
bool^         pstop;               // is set to true to request stop
SERVICE_STATE state;
uint          trace_file_max_size = 64*1024*1024;

//------------------------------------------------------------------

void set_status ()
{
  SERVICE_STATUS status;

  if (!services_available)
    return;

  status = {dwServiceType      => SERVICE_WIN32_OWN_PROCESS,
            dwCurrentState     => (uint)state,
            dwControlsAccepted => SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN,
            dwWin32ExitCode    => 0,
            dwServiceSpecificExitCode => 0,
            dwCheckPoint       => 0,
            dwWaitHint         => 0};

  if (state == START_PENDING || state == STOP_PENDING)
    status.dwWaitHint = 180 * 1000;   // wait 180 seconds

  if (SetServiceStatus (h, &status) == 0)
    trace ("error %d in SetServiceStatus\n", negative_last_windows_error());
}

//------------------------------------------------------------------

void my_service ()
{
  int       timeout, i;
  DATE_TIME before, after;

  timeout = 0;

  for (;;)
  {
    if (timeout > 0)
    {
      trace ("waiting %d minute(s) before retrying to start service ...\n", timeout);
    }

    for (i=0; i<timeout*60; i++)
    {
      sleep 1;
      if (pstop^)
        break;
    }

    if (pstop^)
      break;

    get_datetime (out before);
    (user_function)(pstop);
    get_datetime (out after);

    if (pstop^)
      break;

    // reset timeout to zero after the service ran fine for a day
    if (after.day != before.day)
    {
      timeout = 0;
    }
    else            // double timeout
    {
      if (timeout == 0)
        timeout = 1;
      else
      {
        timeout *= 2;
        if (timeout > 24*60)  // a day
          timeout = 24*60;
      }
    }
  }
}

//------------------------------------------------------------------

[callback]
void Handler (DWORD control)
{
  switch (control)
  {
    case SERVICE_CONTROL_STOP:
      trace ("Handler receives command STOP\n");
      pstop^ = true;
      state = STOP_PENDING;
      break;

    case SERVICE_CONTROL_SHUTDOWN:
      trace ("Handler receives command SHUTDOWN\n");
      pstop^ = true;
      state = STOP_PENDING;
      break;

    case SERVICE_CONTROL_INTERROGATE:
      // trace ("Handler receives command INTERROGATE\n");
      break;

    default:
      trace ("Handler receives unsupported command %u\n", control);
      break;
  }
  set_status();
}

//------------------------------------------------------------------

// must be called by the user when the initialization is done

public void signal_service_is_ready ()
{
  trace ("signal service is ready.\n");
  state = RUNNING;
  set_status();
}

//------------------------------------------------------------------

// must be called by the user when the service is about to stop

public void signal_service_is_stopping ()
{
  trace ("signal service is stopping ...\n");
  if (pstop != null)
    pstop^ = true;
  state = STOP_PENDING;
  set_status();
}

//------------------------------------------------------------------

[callback]
void ServiceMain (DWORD argc, CHAR** argv)
{
  int i;

  _unused argc;

  h = RegisterServiceCtrlHandlerA (argv[0], Handler);
  if (h == 0)    // some error occured
  {
    trace ("error %d in RegisterServiceCtrlHandler\n", negative_last_windows_error());
    return;
  }

  trace ("==========================================================\n");
  trace ("service '%s' starting ...\n", argv[0][0:260]);

  pstop^ = false;
  state = START_PENDING;
  set_status ();


  // get nb of seconds since Windows started
  if (ticks() < 3*60*TICKS_PER_SEC)   // less than 3 minutes
  {
    state = RUNNING;   // tell windows the service started
    set_status();
    trace ("wait 3 minutes until Windows has fully started.\n");

    for (i=0; i<3*60; i++)
    {
      sleep 1;
      if (pstop^)
        break;
    }
  }

  if (!pstop^)
  {
    my_service ();
  }

  trace ("service '%s' stopped.\n", argv[0][0:260]);
  trace ("==========================================================\n");

  pstop^ = true;
  state = STOPPED;
  set_status();
}

//------------------------------------------------------------------

public void set_service_trace_max_file_size (uint max_file_size)
{
  trace_file_max_size = max_file_size;
}

//------------------------------------------------------------------

// this function does not return until the service stops

public int create_service (SERVICE_MAIN user_service_function)
{
  char                program_filename[MAX_EXECUTABLE_FILENAME_LENGTH];
  char                str[MAX_EXECUTABLE_FILENAME_LENGTH];
  SERVICE_TABLE_ENTRY table[2];
  SC_HANDLE           h;
  int                 i, b, rc;
  const string DUMMY = "\0";


  pstop = new bool ' (false);


  // set current directory as that of the executable

  get_executable_filename (out program_filename);


  // remove last component of program_filename : keep only path
  strcpy (out str, program_filename);
  i = strrchr (str, '\\');
  if (i != -1)
    str[i] = nul;
  if (strrchr (str, '\\') == -1)    // no more backslash in name
    strcat (ref str, "\\");         // -> add at least 1

  if (set_current_directory (str) < 0)
    return -1;


  // open trace file

  strcpy (out str, program_filename);
  if (strlen(str) > 4 && stricmp (str[strlen(str)-4:4], ".EXE") == 0)
    str[strlen(str)-4] = nul;
  strcat (ref str, ".tra");
  open_trace (str, trace_file_max_size, date=>true, time=>true, msec=>true);


  // test if services are available

  h = OpenSCManagerA (null, null, SC_MANAGER_ALL_ACCESS);
  if (h != 0)
  {
    services_available = true;
    CloseServiceHandle (h);
  }
  else
  {
    rc = negative_last_windows_error();
    services_available = false;

    trace ("==========================================================\n");

    if (rc == -120)  // ERROR_CALL_NOT_IMPLEMENTED (probably Windows 95/98)
    {
      trace ("application starting ...\n");
      (user_service_function)(pstop);
      return 0;
    }

    trace ("FATAL ERROR : OpenSCManager() returned %d\n", rc);
    trace ("fatal error : cannot access Windows Services !\n");
    return 0;
  }


  // save address of user function

  user_function = user_service_function;


  // initialize table

  clear table;
  table[0].lpServiceName = &DUMMY;
  table[0].lpServiceProc = ServiceMain;

  b = StartServiceCtrlDispatcherA (&table);
  if (b == 0)   // some error occured
  {
    trace ("error %d in StartServiceCtrlDispatcher\n", negative_last_windows_error());
    return -1;
  }

  return 0;
}

//------------------------------------------------------------------

package OPS
  enum SERVICE_OP
  {
    SERV_ADD, SERV_DEL,
    SERV_START, SERV_STOP, SERV_LIST,
    SERV_GET_MODE, SERV_SET_MODE
  };
end OPS;

//------------------------------------------------------------------

// copy a string and append nul character

void strcpyz (out string dest, string src)
{
  int i;

  clear dest;

  for (i=0; i<src'length && src[i] != nul; i++)
    dest[i] = src[i];

  dest[i] = nul;
}

//----------------------------------------------------------------------------

int neg (uint rc)
{
  if (rc == 0)
    return -1;
  if (rc <= (uint)int'max)
    return -(int)rc;
  return (int)rc;
}

//----------------------------------------------------------------------------

int service_operation (    string          computer,
                           SERVICE_OP      operation,
                           string          service_name,
                           string          executable_name,
                           SERVICE_MODE*   pmode,    // for GET_MODE, SET_MODE
                       out SERVICE_INFO[]^ list)
{
  char[260]      computerZ, service_nameZ, executable_nameZ;
  int            b, rc;
  SC_HANDLE      h, h2;
  SERVICE_STATUS status;

  list = null;

  strcpyz (out computerZ, computer);
  strcpyz (out service_nameZ, service_name);
  strcpyz (out executable_nameZ, executable_name);

  h = OpenSCManagerA (&computerZ, null, SC_MANAGER_ALL_ACCESS);
  if (h == 0)
    return negative_last_windows_error();

  if (operation == SERV_ADD)
  {
    h2 = CreateServiceA(h,
                        &service_nameZ,
                        &service_nameZ,
                        SERVICE_ALL_ACCESS,
                        SERVICE_WIN32_OWN_PROCESS,
                        SERVICE_AUTO_START,
                        SERVICE_ERROR_NORMAL,
                        &executable_nameZ,
                        null,
                        null,
                        null,   // dependences
                        null,   // username
                        null);  // password
    if (h2 == 0)
    {
      rc = negative_last_windows_error();
      CloseServiceHandle (h);
      return rc;
    }
  }
  else if (operation == SERV_LIST)
  {
    byte[]^             table;
    ENUM_SERVICE_STATUS e;
    uint                needed2, i, needed, count, resume=0;
    
    b = EnumServicesStatusA (h,
                             0x00000030,  // SERVICE_WIN32,
                             0x00000003,  // SERVICE_ACTIVE | SERVICE_INACTIVE,
                             null,
                             cbBufSize => 0,  // much too small size
                             &needed,
                             &count,
                             &resume);
    rc = negative_last_windows_error();
    if (b != 0 || rc != -234) // ERROR_MORE_DATA
    {
      CloseServiceHandle (h);
      return rc;
    }

    table = new byte [needed];

    b = EnumServicesStatusA (h,
                             0x00000030,  // SERVICE_WIN32,
                             0x00000003,  // SERVICE_ACTIVE | SERVICE_INACTIVE,
                             (ENUM_SERVICE_STATUS*)&table^,
                             needed,
                             &needed2,
                             &count,
                             &resume);
    if (b == 0)
    {
      rc = negative_last_windows_error();
      free table;
      CloseServiceHandle (h);
      return rc;
    }

    list = new SERVICE_INFO [count];

    for (i=0; i<count; i++)
    {
      e'byte = table^[e'size * i : e'size];

      strcpy (out list^[i].name, e.lpServiceName[0:32]);
      strcpy (out list^[i].display, e.lpDisplayName[0:64]);
      list^[i].state = (SERVICE_STATE)e.ServiceStatus.dwCurrentState;
    }

    free table;
    h2 = 0;
  }
  else   // del, start, stop, get_mode, set_mode
  {
    h2 = OpenServiceA (h, &service_nameZ, SERVICE_ALL_ACCESS);
    if (h2 == 0)
    {
      rc = negative_last_windows_error();
      CloseServiceHandle (h);
      return rc;
    }

    if (operation == SERV_DEL)
    {
      if (DeleteService (h2) == 0)
      {
        rc = negative_last_windows_error();
        CloseServiceHandle (h2);
        CloseServiceHandle (h);
        return rc;
      }
    }
    else if (operation == SERV_START)
    {
      char* table = &service_nameZ;
      if (StartServiceA (h2, 1, &table) == 0)
      {
        rc = negative_last_windows_error();
        CloseServiceHandle (h2);
        CloseServiceHandle (h);
        return rc;
      }
    }
    else if (operation == SERV_STOP)
    {
      if (ControlService (h2, SERVICE_CONTROL_STOP, &status) == 0)
      {
        rc = negative_last_windows_error();
        if (rc != -1062)   // ERROR_SERVICE_NOT_ACTIVE (already stopped)
        {
          CloseServiceHandle (h2);
          CloseServiceHandle (h);
          return rc;
        }
      }
    }
    else if (operation == SERV_GET_MODE)
    {
      union qsc
      {
        QUERY_SERVICE_CONFIGA info;
        char                  buffer[8192];
      }
      qsc u;
      uint dummy;

      clear u;
      if (QueryServiceConfigA (h2, &u.info, u'size, &dummy) == 0)
      {
        rc = negative_last_windows_error();
        if (rc != -1062)   // ERROR_SERVICE_NOT_ACTIVE
        {
          CloseServiceHandle (h2);
          CloseServiceHandle (h);
          return rc;
        }
      }

      *pmode = (SERVICE_MODE)u.info.dwStartType;
    }
    else if (operation == SERV_SET_MODE)
    {
      if (ChangeServiceConfigA (h2, SERVICE_NO_CHANGE, (uint)*pmode,
          SERVICE_NO_CHANGE, null, null, null, null, null, null, null) == 0)
      {
        rc = negative_last_windows_error();
        if (rc != -1062)   // ERROR_SERVICE_NOT_ACTIVE
        {
          CloseServiceHandle (h2);
          CloseServiceHandle (h);
          return rc;
        }
      }
    }
    else   // unknown operation
    {
      CloseServiceHandle (h2);
      CloseServiceHandle (h);
      return -1;
    }
  }

  if (h2 != 0)
  {
    if (CloseServiceHandle (h2) == 0)
    {
      rc = negative_last_windows_error();
      CloseServiceHandle (h);
      return rc;
    }
  }

  if (CloseServiceHandle (h) == 0)
    return negative_last_windows_error();

  return 0;
}

//------------------------------------------------------------------

public int add_service (string computer, string service_name, string executable_name)
{
  SERVICE_INFO[]^ list;
  int             rc;
  rc = service_operation (computer, SERV_ADD, service_name, executable_name,
                          null, out list);
  _unused list;
  return rc;
}

//------------------------------------------------------------------

public int delete_service (string computer, string service_name)
{
  SERVICE_INFO[]^ list;
  int             rc;
  rc = service_operation (computer, SERV_DEL, service_name, "",
                          null, out list);
  _unused list;
  return rc;
}

//------------------------------------------------------------------

public int start_service (string computer, string service_name)
{
  SERVICE_INFO[]^ list;
  int             rc;
  rc = service_operation (computer, SERV_START, service_name, "",
                          null, out list);
  _unused list;
  return rc;
}

//------------------------------------------------------------------

public int stop_service (string computer, string service_name)
{
  SERVICE_INFO[]^ list;
  int             rc;
  rc = service_operation (computer, SERV_STOP, service_name, "",
                          null, out list);
  _unused list;
  return rc;
}

//------------------------------------------------------------------

public int list_services (    string          computer,
                          out SERVICE_INFO[]^ table)

{
  return service_operation (computer, SERV_LIST, "", "", null, out table);
}

//------------------------------------------------------------------

public int get_service_mode (string computer, string service_name, out SERVICE_MODE mode)
{
  SERVICE_INFO[]^ list;
  int             rc;
  rc = service_operation (computer, SERV_GET_MODE, service_name, "",
                          &mode, out list);
  _unused list;
  return rc;
}

//------------------------------------------------------------------

public int set_service_mode (string computer, string service_name, SERVICE_MODE mode)
{
  SERVICE_INFO[]^ list;
  int             rc;
  rc = service_operation (computer, SERV_SET_MODE, service_name, "",
                          &mode, out list);
  _unused list;
  return rc;
}

//------------------------------------------------------------------
#end unsafe
//------------------------------------------------------------------
