
// thread.c

#if WINDOWS
  use strings, win/windows, tracing;
#endif

#if ANDROID
  use android/bionic;
#endif

//---------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------

#if WINDOWS
struct SHARED_OBJECT    // full type
{
  CRITICAL_SECTION cs;
  uint             init;    // 0 = new, 1 = initialization in progress, 2 = initialized
}
#endif


#if ANDROID
struct SHARED_OBJECT    // full type
{
  pthread_mutex_t     mutex;
  pthread_mutexattr_t attr;
  uint                init;    // 0 = new, 1 = initialization in progress, 2 = initialized
}
#endif

uint pid;   // current process id

uint g_granularity;

//---------------------------------------------------------------

// returns old value of variable
#if WINDOWS
public uint InterlockedExchange (ref uint variable,      // ebp+12  RBP+24
                                 uint     new_value)     // ebp+8   RBP+16
{
#if MEM32
  _asm { 0x8B 0x45 0x08 };  // mov   eax,ebp+8   ; load new value
  _asm { 0x8B 0x75 0x0C };  // mov   esi,ebp+12  ; load addr of variable
  _asm { 0x87 0x06 };       // xchg  eax,[esi]   ; exchange with variable
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0x8B 0x45 0x10 };       // mov   eax,rbp+16  ; load new value
  _asm { 0x48 0x8B 0x75 0x18 };  // mov   rsi,rbp+24  ; load addr of variable
  _asm { 0x87 0x06 };            // xchg  eax,[rsi]   ; exchange with variable
  _asm { 0xC9 };                 // leave
  _asm { 0xC2 0x10 0x00 };       // ret 16
#endif
}
#endif  // WINDOWS

//---------------------------------------------------------------

/*
#if ANDROID
public uint InterlockedExchange (ref uint variable,
                                     uint new_value)
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp
  _asm { 0x00 0x80 0x21 0xb8 };  // swp	w1, w0, [x0]        (store x1, load x0)
  _asm { 0xfd 0x7b 0xc2 0xa8 };  // ldp	x29, x30, [sp], #0x20
  _asm { 0xc0 0x03 0x5f 0xd6 };  // ret
}
#endif  // ANDROID
*/

#if ANDROID
public uint InterlockedExchange (ref uint variable,
                                     uint new_value)
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp

/*
  loop:
      LDAXR  w2, [x0]       // Load exclusive from address in x0 into w2
      STLXR  w3, w1, [x0]   // Try to store w1; w3 == 0 if successful
      CBNZ   w3, loop       // If not successful, try again
      MOV    w0,w2          // return old value
*/
  _asm { 0x02 0xfc 0x5f 0x88 
         0x01 0xfc 0x03 0x88 
         0xc3 0xff 0xff 0x35 
         0xe0 0x03 0x02 0x2a };

  _asm { 0xfd 0x7b 0xc2 0xa8 };  // ldp	x29, x30, [sp], #0x20
  _asm { 0xc0 0x03 0x5f 0xd6 };  // ret
}
#endif  // ANDROID


//---------------------------------------------------------------

// adds delta to variable in an indivisible operation,
// even if several threads call this function at the same time.

#if WINDOWS
public void InterlockedAdd (ref int variable,    // ebp+12  RBP+24
                                int delta)       // ebp+8   RBP+16
{
#if MEM32
  _asm { 0x8B 0x45 0x08 };       // mov  eax,ebp+8   ; load new value
  _asm { 0x8B 0x75 0x0C };       // mov  esi,ebp+12  ; load addr of variable
  _asm { 0xF0 0x01 0x06 };       // lock  add  dword ptr [esi],eax
  _asm { 0xC9 };                 // leave
  _asm { 0xC2 0x08 0x00 };       // ret 8
#else
  _asm { 0x8B 0x45 0x10 };       // mov   eax,rbp+16  ; load new value
  _asm { 0x48 0x8B 0x75 0x18 };  // mov   rsi,rbp+24  ; load addr of variable
  _asm { 0xF0 0x01 0x06 };       // lock  add  dword ptr [rsi],eax
  _asm { 0xC9 };                 // leave
  _asm { 0xC2 0x10 0x00 };       // ret 16
#endif
}
#endif  // WINDOWS

/*
#if ANDROID
public void InterlockedAdd (ref int variable,    // ebp+12  RBP+24
                                int delta)       // ebp+8   RBP+16
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp
  _asm { 0x00 0x00 0x21 0xb8 };  // at-add	w1, w0, [x0]        (store x1, load x0)
//   0xfd 0x7b 0xc2 0xa8            ldp	x29, x30, [sp], #0x20
//   0xc0 0x03 0x5f 0xd6            ret
}
#endif  // ANDROID
*/

#if ANDROID
public void InterlockedAdd (ref int variable,    // ebp+12  RBP+24
                                int delta)       // ebp+8   RBP+16
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp

/*
  loop:
      LDAXR   w2, [x0]       // Load exclusive from address in x0 into w2
      ADD     w2, w2, w1     // add w1
      STLXR   w3, w2, [x0]   // Try to store w2; w3 == 0 if successful
      CBNZ    w3, loop       // If not successful, try again
*/

  _asm { 0x02 0xfc 0x5f 0x88 
         0x42 0x00 0x01 0x0b 
         0x02 0xfc 0x03 0x88 
         0xa3 0xff 0xff 0x35 };

//   0xfd 0x7b 0xc2 0xa8            ldp	x29, x30, [sp], #0x20
//   0xc0 0x03 0x5f 0xd6            ret
}
#endif  // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public void InterlockedAnd (ref uint variable, uint value)
{
#if MEM32
  _asm { 0x8B 0x45 0x08 };       // mov  eax,ebp+8   ; load new value
  _asm { 0x8B 0x75 0x0C };       // mov  esi,ebp+12  ; load addr of variable
  _asm { 0xF0 0x21 0x06 };       // lock  and  dword ptr [esi],eax
  _asm { 0xC9 };                 // leave
  _asm { 0xC2 0x08 0x00 };       // ret 8
#else
  _asm { 0x8B 0x45 0x10 };       // mov   eax,rbp+16  ; load new value
  _asm { 0x48 0x8B 0x75 0x18 };  // mov   rsi,rbp+24  ; load addr of variable
  _asm { 0xF0 0x21 0x06 };       // lock  and  dword ptr [rsi],eax
  _asm { 0xC9 };                 // leave
  _asm { 0xC2 0x10 0x00 };       // ret 16
#endif
}
#endif  // WINDOWS

/*
#if ANDROID
public void InterlockedAnd (ref uint variable, uint value)
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp
  _asm { 0xe1 0x03 0x21 0x2a };    // mvn w1,w1
  _asm { 0x00 0x10 0x21 0xb8 };    // at-clr	w1, w0, [x0]        (store x1, load x0)
//   0xfd 0x7b 0xc2 0xa8            ldp	x29, x30, [sp], #0x20
//   0xc0 0x03 0x5f 0xd6            ret
}
#endif  // ANDROID
*/

#if ANDROID
public void InterlockedAnd (ref uint variable, uint value)
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp

/*
  loop:
      LDAXR   w2, [x0]
      AND     w2, w2, w1
      STLXR   w3, w2, [x0]
      CBNZ    w3, loop    
*/
  _asm { 0x02 0xfc 0x5f 0x88 
         0x42 0x00 0x01 0x0a 
         0x02 0xfc 0x03 0x88 
         0xa3 0xff 0xff 0x35 };

//   0xfd 0x7b 0xc2 0xa8            ldp	x29, x30, [sp], #0x20
//   0xc0 0x03 0x5f 0xd6            ret
}
#endif  // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public void InterlockedOr (ref uint variable, uint value)
{
#if MEM32
  _asm { 0x8B 0x45 0x08 };       // mov  eax,ebp+8   ; load new value
  _asm { 0x8B 0x75 0x0C };       // mov  esi,ebp+12  ; load addr of variable
  _asm { 0xF0 0x09 0x06 };       // lock  or  dword ptr [esi],eax
  _asm { 0xC9 };                 // leave
  _asm { 0xC2 0x08 0x00 };       // ret 8
#else
  _asm { 0x8B 0x45 0x10 };       // mov   eax,rbp+16  ; load new value
  _asm { 0x48 0x8B 0x75 0x18 };  // mov   rsi,rbp+24  ; load addr of variable
  _asm { 0xF0 0x09 0x06 };       // lock  or  dword ptr [rsi],eax
  _asm { 0xC9 };                 // leave
  _asm { 0xC2 0x10 0x00 };       // ret 16
#endif
}
#endif  // WINDOWS

/*
#if ANDROID
public void InterlockedOr (ref uint variable, uint value)
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp
  _asm { 0x00 0x30 0x21 0xb8 };  // at-or	w1, w0, [x0]        (store x1, load x0)
//   0xfd 0x7b 0xc2 0xa8            ldp	x29, x30, [sp], #0x20
//   0xc0 0x03 0x5f 0xd6            ret
}
#endif  // ANDROID
*/

#if ANDROID
public void InterlockedOr (ref uint variable, uint value)
{
//       a9be7bfd                   stp x29, x30, [sp, #-0x20]!
//       910003fd                   mov x29, sp

/*
  loop:
      LDAXR   w2, [x0]
      ORR     w2, w2, w1
      STLXR   w3, w2, [x0]
      CBNZ    w3, loop    
*/
  _asm { 0x02 0xfc 0x5f 0x88 
         0x42 0x00 0x01 0x2a 
         0x02 0xfc 0x03 0x88 
         0xa3 0xff 0xff 0x35 };

//   0xfd 0x7b 0xc2 0xa8            ldp	x29, x30, [sp], #0x20
//   0xc0 0x03 0x5f 0xd6            ret
}
#endif  // ANDROID

//---------------------------------------------------------------

// load all cache data from other cores
// to be called after reading a flag signaling new data, before reading the new data.

public void fetch_cache ()
{
#if WINDOWS
//  _asm { 0x0F 0xAE 0xE8 };  // LFENCE  load fence
#elif ANDROID
  _asm { 0xbf 0x39 0x03 0xd5 };   //  dmb  ishld
#else
  bad
#endif
}

//---------------------------------------------------------------

// flush writes to cache to make data available on other cores
// to be called after writing a buffer, before setting a flag to signal it's available.

public void flush_cache ()
{
#if WINDOWS
//  _asm { 0x0F 0xAE 0xF8 };  // SFENCE  store fence
#elif ANDROID
  _asm { 0xbf 0x3a 0x03 0xd5 };   //  dmb  ishst   
#else
  bad
#endif
}

//---------------------------------------------------------------

// sync cache with other cores (read + stores)

public void sync_cache ()
{
#if WINDOWS
//  _asm { 0x0F 0xAE 0xF0 };   // MFENCE	Full memory fence
#elif ANDROID
  _asm { 0xbf 0x3b 0x03 0xd5 };   //  dmb ish   
#else
  bad
#endif
}

//---------------------------------------------------------------

// system timer incremented TICKS_PER_SEC times per second;
// the timer may overflow after some days.

#if WINDOWS
public uint ticks ()
{
  return GetTickCount();
}
#endif // WINDOWS

#if ANDROID
public uint ticks ()   // 1e-3 sec
{
  TIME time;
  bionic.clock_gettime (CLOCK_MONOTONIC, out time);

  // nsec is 1e-9, so div by 1e6
  return (uint)time.sec * 1000 + (uint)((time.nsec * 2251799814L) >> (32+19));
}
#endif // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public void set_timer (out TIMER t, uint nsecs)
{
  t = GetTickCount() + nsecs * 1000;
}
#endif // WINDOWS

#if ANDROID
public void set_timer (out TIMER t, uint nsecs)
{
  t = ticks() + nsecs * 1000;
}
#endif // ANDROID

//---------------------------------------------------------------

// returns true if timer has elapsed, false otherwise.

#if WINDOWS
public bool timer_elapsed (TIMER t)
{
  // note: GetTickCount() wraps to zero after 49.7 days
  // so we check if the timer elapsed at least 10 days ago.
  return GetTickCount() - t < 864000000;   /* 10 days */
}
#endif // WINDOWS

#if ANDROID
public bool timer_elapsed (TIMER t)
{
  // note: GetTickCount() wraps to zero after 49.7 days
  // so we check if the timer elapsed at least 10 days ago.
  return ticks() - t < 864000000;   /* 10 days */
}
#endif // ANDROID

//---------------------------------------------------------------

// disable throttle of cores and honor timer resolution requests

#if WINDOWS
public int set_high_performance_process (bool on = true)
{
  const string       dll_name  = "KERNEL32\0";
  const string       func_name = "SetProcessInformation\0";
  HMODULE            hinstLib;
  LPVOID             func;
  FSetProcessInformation SetProcessInformation;

  hinstLib = LoadLibraryA (&dll_name);
  if (hinstLib == 0)
  {
    trace ("error: LoadLibraryA(KERNEL32) failed\n");
    return -1;
  }

  func = GetProcAddress (hinstLib, &func_name);
  *(LPVOID*)&SetProcessInformation = *(LPVOID*)&func;
  if (SetProcessInformation == null)
  {
    trace ("error: GetProcAddress(SetProcessInformation) failed\n");
    return -1;
  }

  {
    PROCESS_POWER_THROTTLING_STATE state;
    BOOL                           success;
    
    clear state;
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;

    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED
                      | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;

    // If enableThrottling is true, set the flag; otherwise, disable it
    state.StateMask = on ? 0 : (PROCESS_POWER_THROTTLING_EXECUTION_SPEED
                                | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION);

    // Apply the setting to the current process
    success = SetProcessInformation (GetCurrentProcess(), ProcessPowerThrottling, (byte*)&state, state'size);
    if (success == 0)  // failed
      return negative_last_windows_error();
      
    return 0;
  }
}
#endif

//---------------------------------------------------------------

// set granularity of ticks in msecs (default is 16 msec, best possible is 1 msec)
// returns 0 if success, -1 if value not supported.

#if WINDOWS
public int set_granularity (uint msecs)
{
  if (g_granularity != 0)
    timeEndPeriod (g_granularity);    // undo old granularity
  else
  {
    int rc = set_high_performance_process (on => true);
    trace ("info: set_high_performance_process() rc = %d\n", rc);
  }

  if (timeBeginPeriod (msecs) != 0)  // not supported
  {
    g_granularity = 0;
    return -1;
  }

  g_granularity = msecs;
  return 0;
}
#endif // WINDOWS

#if ANDROID
public int set_granularity (uint msecs)
{
  _unused msecs;
  return -1;  // not supported
}
#endif // ANDROID


//---------------------------------------------------------------

public uint get_granularity ()
{
  return g_granularity;
}

//---------------------------------------------------------------

#if WINDOWS
public void delay (uint milliseconds)
{
  Sleep (milliseconds);
}
#endif // WINDOWS

#if ANDROID
public void delay (uint milliseconds)
{
  int  sec, msec;
  TIME t;

  clock_gettime (CLOCK_MONOTONIC, out t);

  sec  = (int)((milliseconds * 2199023256L) >> (32+9));  // divide by 1000
  msec = (int)(milliseconds - 1000 * (uint)sec);

  t.sec += sec;
  t.nsec += msec * 1_000_000;  // msec to nsec (X 10^6)

  if (t.nsec >= 1_000_000_000)
  {
    t.nsec -= 1_000_000_000;
    t.sec++;
  }

  while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, ref t, 0) != 0)
    ;
}
#endif // ANDROID

//---------------------------------------------------------------

// returns number of 100-nanosecond intervals since January 1, 1601 (UTC) (1e-7)

#if WINDOWS
public long clock ()
{
  long time;
  GetSystemTimeAsFileTime (&time);
  return time;
}
#endif // WINDOWS

#if ANDROID
public long clock ()
{
  TIME time;
  clock_gettime (CLOCK_REALTIME, out time);

  // nsec is 1e-9, so divide by 100
  return time.sec * 10_000_000L + ((time.nsec * 2748779070L) >> 38) + 11644473600L * 10_000_000;
}
#endif // ANDROID

//---------------------------------------------------------------

// very high precision timer
// returns value of intern cpu clock

#if WINDOWS
public long performance_timer ()
{
  long l;
  QueryPerformanceCounter (&l);
  return l;
}
#endif // WINDOWS

#if ANDROID
public long performance_timer ()
{
  TIME time;
  clock_gettime (CLOCK_MONOTONIC, out time);
  return time.sec * 1_000_000_000L + time.nsec;
}
#endif // ANDROID

//---------------------------------------------------------------

// returns nb of cpu clock increments per second

#if WINDOWS
public long performance_timer_frequency ()
{
  long l;
  if (QueryPerformanceFrequency (&l) == 0)
    return 0;
  return l;
}
#endif // WINDOWS

#if ANDROID
public long performance_timer_frequency ()
{
  return 1_000_000_000L;
}
#endif // ANDROID

//---------------------------------------------------------------

// 1) object must be aligned on 32-bit boundary.
// 2) object must be cleared before use.

// to be the only thread executing a piece of code,
// enclose it between enter_shared_object() and leave_shared_object().

#if WINDOWS
public void enter_shared_object (ref SHARED_OBJECT o)
{
  if (o.init != 2)  // not initialized yet
  {
    uint old = InterlockedExchange (ref o.init, 1);

    switch (old)
    {
      case 0:   // we're the first caller
        InitializeCriticalSection (&o.cs);
        (void)InterlockedExchange (ref o.init, 2);  // set it to 2 (initialized)
        break;

      case 1:   // someone else is taking care of initialization
        // wait until it switches to 2 ..
        while (o.init != 2)
          sleep 0;
        break;

      case 2:   // it was already initialized but we did set it back to 1 !
        (void)InterlockedExchange (ref o.init, 2);  // set it back to 2 (initialized)
        break;

      default:
        abort;
    }
  }

  EnterCriticalSection (&o.cs);
}
#endif  // WINDOWS


#if ANDROID
public void enter_shared_object (ref SHARED_OBJECT o)
{
  if (o.init != 2)  // not initialized yet
  {
    uint old = InterlockedExchange (ref o.init, 1);

    switch (old)
    {
      case 0:   // we're the first caller
        assert pthread_mutexattr_init    (&o.attr) == 0;
        assert pthread_mutexattr_settype (&o.attr, PTHREAD_MUTEX_RECURSIVE) == 0;
        assert pthread_mutex_init        (&o.mutex, &o.attr) == 0;
        assert pthread_mutexattr_destroy (&o.attr) == 0;

        (void)InterlockedExchange (ref o.init, 2);  // set it to 2 (initialized)
        break;

      case 1:   // someone else is taking care of initialization
        // wait until it switches to 2 ..
        while (o.init != 2)
          sleep 0;
        break;

      case 2:   // it was already initialized but we did set it back to 1 !
        (void)InterlockedExchange (ref o.init, 2);  // set it back to 2 (initialized)
        break;

      default:
        abort;
    }
  }

  assert pthread_mutex_lock (&o.mutex) == 0;
}
#endif  // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public void leave_shared_object (ref SHARED_OBJECT o)
{
  assert (o.init != 0);
  LeaveCriticalSection (&o.cs);
}
#endif  // WINDOWS

#if ANDROID
public void leave_shared_object (ref SHARED_OBJECT o)
{
  assert pthread_mutex_unlock (&o.mutex) == 0;
}
#endif  // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public void destroy_shared_object (ref SHARED_OBJECT o)
{
  if (o.init != 0)
  {
    DeleteCriticalSection (&o.cs);
    o.init = 0;
  }
}
#endif  // WINDOWS

#if ANDROID
public void destroy_shared_object (ref SHARED_OBJECT o)
{
  if (o.init == 2)
    assert pthread_mutex_destroy (&o.mutex) == 0;
  o.init = 0;
}
#endif  // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public uint get_current_process_id ()
{
  if (pid == 0)
    pid = GetCurrentProcessId ();
  return pid;
}
#endif  // WINDOWS

#if ANDROID
public uint get_current_process_id ()
{
  if (pid == 0)
    pid = (uint)bionic.getpid();
  return pid;
}
#endif  // ANDROID


//---------------------------------------------------------------

#if WINDOWS
public uint get_current_thread_id ()
{
  return GetCurrentThreadId ();
}
#endif  // WINDOWS

#if ANDROID
public uint get_current_thread_id ()
{
  return (uint)bionic.gettid ();
}
#endif  // ANDROID

//---------------------------------------------------------------

public int start_process (string executable, string arguments, bool wait = true)
{
#if WINDOWS

  int                 rc;
  STARTUPINFOA        s;
  PROCESS_INFORMATION p;
  char                cmdline[260+260+64];

  clear s;
  s.cb = s'size;
  s.wShowWindow = SW_SHOWDEFAULT;

  if (strchr (executable, ' ') != -1)   /* contains spaces */
  {
    sprintf (out cmdline, "\"%s\"", executable);
  }
  else
  {
    sprintf (out cmdline, "%s", executable);
  }

  if (strlen(arguments) > 0)
  {
    strcat (ref cmdline, " ");
    strcat (ref cmdline, arguments);
  }

  rc = CreateProcessA (null, &cmdline, null, null, FALSE, 0, null, null, &s, &p);
  if (rc != 0)
  {
    if (wait)
      WaitForInputIdle (p.hProcess, 30000);   /* timeout: 30 secs */
    CloseHandle (p.hProcess);
    CloseHandle (p.hThread);
    return 0;
  }

  return negative_last_windows_error();

#elif ANDROID
  _unused executable, arguments, wait;
  return -1;

#else
  add

#endif
}

//---------------------------------------------------------------

public int wstart_process (wstring executable, wstring arguments, bool wait = true)
{
#if WINDOWS

  int                 rc;
  STARTUPINFOA        s;
  PROCESS_INFORMATION p;
  wchar               cmdline[260+260+64];

  clear s;
  s.cb = s'size;
  s.wShowWindow = SW_SHOWDEFAULT;

  if (wstrchr (executable, L' ') != -1)   /* contains spaces */
  {
    wsprintf (out cmdline, L"\"%S\"", executable);
  }
  else
  {
    wsprintf (out cmdline, L"%S", executable);
  }

  if (wstrlen(arguments) > 0)
  {
    wstrcat (ref cmdline, L" ");
    wstrcat (ref cmdline, arguments);
  }

  rc = CreateProcessW (null, &cmdline, null, null, FALSE, 0, null, null, &s, &p);
  if (rc != 0)
  {
    if (wait)
      WaitForInputIdle (p.hProcess, 30000);   /* timeout: 30 secs */
    CloseHandle (p.hProcess);
    CloseHandle (p.hThread);
    return 0;
  }

  return negative_last_windows_error();

#elif ANDROID
  _unused executable, arguments, wait;
  return -1;

#else
  add

#endif
}

//---------------------------------------------------------------

// terminates the current process and returns a code to the calling process

public void exit (int code = 0)
{
#if WINDOWS
  ExitProcess ((uint)code);
#elif ANDROID
  bionic.exit (code);
#else
  bad
#endif  
}

//---------------------------------------------------------------

#if WINDOWS
public int create_signal ()
{
  int rc = (int)CreateSemaphoreA (null, lInitialCount => 0, lMaximumCount => 1, null);
  return (rc == 0) ? -1 : rc;
}
#endif // WINDOWS

//---------------------------------------------------------------

#if ANDROID
public int create_signal ()
{
  return bionic.eventfd (initval => 0);
}
#endif // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public void raise_signal (int s)
{
  (void)ReleaseSemaphore (s, lReleaseCount => 1, lpPreviousCount => null);
}
#endif // WINDOWS

#if ANDROID
public void raise_signal (int s)
{
  long value = 1;
  assert bionic.write (s, (byte*)&value, 8) == 8;
}
#endif // ANDROID

//---------------------------------------------------------------

// returns -1 if timeout, 0 if signal was raised.

#if WINDOWS
public int wait_signal (int s, uint timeout_msecs = uint'max)
{
  DWORD rc;
  HANDLE hs = s;
  rc = WaitForMultipleObjectsEx (1, &hs, fWaitAll => 0, timeout_msecs, alertable => 0);
  if (rc == WAIT_TIMEOUT)
    return -1;
  else if (rc == 0)
    return 0;
  else
    abort;
}
#endif // WINDOWS

#if ANDROID
public int wait_signal (int s, uint timeout_msecs = uint'max)
{
  pollfd fds[1];

  clear fds;
  fds[0].fd = s;
  fds[0].events = POLLIN;

  if (bionic.poll (&fds, nfds => 1, (int)timeout_msecs) >= 1)
  {
    long value;
    assert bionic.read (s, (byte*)&value, 8) == 8;
    return 0;   // signal was raised
  }
  else
  {
    return -1;  // timeout
  }
}
#endif // ANDROID

//---------------------------------------------------------------

#if WINDOWS
public void close_signal (int s)
{
  assert CloseHandle(s) != 0;
}
#endif // WINDOWS

#if ANDROID
public void close_signal (int s)
{
  assert bionic.close (s) == 0;
}
#endif // ANDROID

//---------------------------------------------------------------

// -2 to +2
#if WINDOWS
public int get_thread_priority ()
{
  return GetThreadPriority (GetCurrentThread());
}
#endif // WINDOWS

//---------------------------------------------------------------

// -2 to +2
#if WINDOWS
public void set_thread_priority (int priority)
{
  SetThreadPriority (GetCurrentThread(), priority);
}
#endif // WINDOWS

//---------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------
