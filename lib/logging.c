
// logging.c : write in android logcat

#if ANDROID

use strformat;
use android/bionic;

//------------------------------------------------------------------------------
#begin unsafe
//------------------------------------------------------------------------------

package ContextPackage

  packed struct Context
  {
    char                 buffer[512];
    int                  len;
    android_LogPriority  prio;
    bool                 some_written;
  }

  void Flush (ref Context c);
  int Put (PUT_CONTEXT context, string s);

end ContextPackage;

//---------------------------------------------------------------

package body ContextPackage

  public void Flush (ref Context c)
  {
    const string TAG = "Safe-C-Log\0";

    c.buffer[c.len] = nul;

    __android_log_write (prio => c.prio, tag => &TAG, text => &c.buffer);

    c.len = 0;
    c.some_written = true;
  }

  public int Put (PUT_CONTEXT context, string s)
  {
    ref Context c = *((Context *)context);
    int  i;
    char ch;

    for (i=0; i<s'length; i++)
    {
      ch = s[i];
      if (ch == '\n' || ch == '\r')
      {
        if (c.len > 0 || !c.some_written)
          Flush (ref c);
      }
      else
      {
        c.buffer[c.len++] = ch;
        if (c.len == c.buffer'length-1)  // -1 to keep space for final nul character
          Flush (ref c);
      }
    }
    return 0;
  }

end ContextPackage;

//---------------------------------------------------------------

// causes a runtime error if the format string has a bad format.

public void log (string format, object[] arg)
{
  Context c;

  c.len = 0;
  c.some_written = false;
  c.prio = ANDROID_LOG_INFO;
  assert (format_string ((byte *)&c, Put, format, arg) == 0);

  if (c.len > 0)
    Flush (ref c);
}

//---------------------------------------------------------------

public
void log_fatal_error (string format, object[] arg)
{
  Context c;

  c.len = 0;
  c.some_written = false;
  c.prio = ANDROID_LOG_FATAL;
  assert (format_string ((byte *)&c, Put, format, arg) == 0);

  if (c.len > 0)
    Flush (ref c);
}

//---------------------------------------------------------------
#end unsafe
//------------------------------------------------------------------------------

#endif  // ANDROID
