
//----------------------------------------------------------------------------
// tracing.h : create a trace file to log application events.
//----------------------------------------------------------------------------

// create a new or append to an existing trace file.
//
// 'max_file_size' : maximum allowed size of trace file before it is renamed
//                   into '.old' and a new empty file is created.
//
// 'date' : prefix "YY/MM/DD" to each line
// 'time' : prefix "HH:MM:SS" to each line
// 'msec' : prefix ".MMM" to each line (milliseconds)

void open_trace (string filename,                      // for example "trace.tra"
                 long   max_file_size = 64*1024*1024,  // default is 64 MB
                 bool   date = true,
                 bool   time = true,
                 bool   msec = false);

//----------------------------------------------------------------------------

// if 'format' ends with '\n', then the next line will start with
// a date/time prefix (if this was asked in the open_trace call)
//
// causes a runtime error if the format string has a bad format.

void trace (string format, object[] arg);

//----------------------------------------------------------------------------

// trace hexadecimal dump of a block of data
void trace_block (byte[] block);

//----------------------------------------------------------------------------

void close_trace ();

//----------------------------------------------------------------------------

bool g_trace_is_open;

//----------------------------------------------------------------------------
