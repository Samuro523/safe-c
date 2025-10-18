
// thread.h : thread, process, synchronisation, timers.

//---------------------------------------------------------------

typedef uint TIMER;

// initialize a timer that will elapse after 'nsecs' seconds
// note: this timer must only be used for small durations (maximum 1 day)
void set_timer (out TIMER t, uint nsecs);

// returns true if timer has elapsed, false otherwise.
bool timer_elapsed (TIMER t);

//---------------------------------------------------------------

#if WINDOWS
  // disable throttle of cores and honor timer resolution requests
  int set_high_performance_process (bool on = true);
#endif

//---------------------------------------------------------------

// get value of low precision timer (incremented TICKS_PER_SEC times per second);
// the timer value overflows after a few days.
uint ticks ();

const uint TICKS_PER_SEC = 1000;

// set granularity of ticks in msecs (default is 16 msec, best possible is 1 msec)
// returns 0 if success, -1 if value not supported.
int set_granularity (uint msecs);

uint get_granularity ();

//---------------------------------------------------------------

// let thread sleep for some duration
void delay (uint milliseconds);

//---------------------------------------------------------------

// returns number of 1.0e-7 sec intervals since January 1, 1601 (UTC).
long clock ();

//---------------------------------------------------------------

// get value of high precision timer
// the timer value overflows after some days and becomes negative.
// known bugs: due to bad hardware, these timers are not reliable on some PCs :
//   - they can jump forth and back a few seconds,
//   - they can get slower/faster when a processor receives a boost,
//   - it's not specified whether they run when the PC is in standby mode.
long performance_timer ();

// returns nb of clock cycles per second.
long performance_timer_frequency ();

//---------------------------------------------------------------

struct SHARED_OBJECT;

// to be the only thread executing a piece of code,
// enclose it between enter_shared_object() and leave_shared_object()

void enter_shared_object (ref SHARED_OBJECT o);
void leave_shared_object (ref SHARED_OBJECT o);

// when the object is no longer useful or before deleting the object,
// you must call destroy_shared_object() to free system resources.

void destroy_shared_object (ref SHARED_OBJECT o);

//---------------------------------------------------------------

// creates a signal object
// returns a signal descriptor s or a negative value if error
int create_signal ();

// wait for a signal or a timeout in milliseconds
// returns -1 if timeout, 0 if signal was raised.
int wait_signal (int s, uint timeout_msecs = uint'max);

// raises a signal, waking up a waiting thread.
void raise_signal (int s);

// closes signal s
void close_signal (int s);

//---------------------------------------------------------------

// returns the old value of variable and sets a new value in an indivisible operation,
// even if several threads call this function at the same time.

uint InterlockedExchange (ref uint variable,
                          uint     new_value);

//---------------------------------------------------------------

// adds delta to variable in an indivisible operation,
// even if several threads call this function at the same time.

void InterlockedAdd (ref int variable, int delta);

//---------------------------------------------------------------

void InterlockedAnd (ref uint variable, uint value);
void InterlockedOr (ref uint variable, uint value);

//---------------------------------------------------------------

// load all cache data from other cores
// to be called after reading a flag signaling new data, before reading the new data.
void fetch_cache ();

// flush writes to cache to make data available on other cores
// to be called after writing a buffer, before setting a flag to signal it's available.
void flush_cache ();

// sync cache with other cores (read + stores)
void sync_cache ();

//---------------------------------------------------------------

// returns unique ID of this process.

uint get_current_process_id ();

//---------------------------------------------------------------

// returns unique ID of this thread.

uint get_current_thread_id ();

//---------------------------------------------------------------

// returns 0 if OK, a negative error if the function failed.

// NOT IMPLEMENTED FOR ANDROID

int start_process (string executable,
                   string arguments,
                   bool   wait = true);   // wait until process has started

// NOT IMPLEMENTED FOR ANDROID

int wstart_process (wstring executable, 
                    wstring arguments, 
                    bool    wait = true);

//---------------------------------------------------------------

// terminates the current process and returns a code to the calling process.
// avoid calling this for android, this causes a bad user alert.

void exit (int code = 0);

//---------------------------------------------------------------

// -2 to +2
#if WINDOWS
int get_thread_priority ();
void set_thread_priority (int priority);
#endif

//---------------------------------------------------------------
