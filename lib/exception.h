
// exception.h

//--------------------------------------------------------------------------

// enables exception handler that shows the error location after a crash.
// the location is shown as source file if the .dbg file is present,
// otherwise it is shown in hex.

void arm_exception_handler (bool create_crash_report_file        = true,
                            bool display_fatal_error_message_box = true);  // unused for android

//--------------------------------------------------------------------------

void log_in_crash_report (string format, object[] arg);

//--------------------------------------------------------------------------
