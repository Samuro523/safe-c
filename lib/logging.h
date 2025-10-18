
// logging.h : write in android logcat

#if ANDROID

// causes a runtime error if the format string has a bad format.
void log             (string format, object[] arg);
void log_fatal_error (string format, object[] arg);

#endif
