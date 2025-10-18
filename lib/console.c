
// console.c

#if WINDOWS
  use strformat;
  use win/windows;
#endif

#if ANDROID
  use logging;
#endif


#begin unsafe

//------------------------------------------------------------------------------

#if WINDOWS

package WriteToStdOut

  packed struct ContextWriteToStdOut
  {
    char buffer[1024];
    int  len;
  }

  void Flush (ref ContextWriteToStdOut c);
  int Put (PUT_CONTEXT context, string s);

end WriteToStdOut;

//---------------------------------------------------------------

package body WriteToStdOut

  public void Flush (ref ContextWriteToStdOut c)
  {
    uint written;
    WriteFile (GetStdHandle(-11), (byte *)&c.buffer, (uint)c.len, &written, null);
    // for info: -10 is stdin.
    c.len = 0;
  }

  public int Put (PUT_CONTEXT context, string s)
  {
    ref ContextWriteToStdOut c = *((ContextWriteToStdOut *)context);
    int  i;
    char ch;

    for (i=0; i<s'length; i++)
    {
      ch = s[i];
      if (ch == '\n')
      {
        // insert '\r'
        if (c.len == c.buffer'length)
          Flush (ref c);
        c.buffer[c.len++] = '\r';
      }

      if (c.len == c.buffer'length)
        Flush (ref c);
      c.buffer[c.len++] = ch;
    }
    return 0;
  }

end WriteToStdOut;

#endif   // WINDOWS

//---------------------------------------------------------------

// causes a runtime error if the format string has a bad format.

public void printf (string format, object[] arg)
{
#if WINDOWS
  ContextWriteToStdOut c;

  c.len = 0;
  assert (format_string ((byte *)&c, Put, format, arg) == 0);

  if (c.len > 0)
    Flush (ref c);
#endif

#if ANDROID
  logging.log (format, arg);
#endif  
}

//---------------------------------------------------------------

#if WINDOWS

package ReadFromStdIn

  packed struct ContextReadFromStdIn
  {
    bool  buffered;
    char  c;
  }

  ContextReadFromStdIn g_RdCt;  // global read context

  int Get (GET_CONTEXT context, out char ch);
  void UnGet (GET_CONTEXT context);

end ReadFromStdIn;

//---------------------------------------------------------------

package body ReadFromStdIn

  // must return 0 if OK, -1 if read error, -2 if end-of-stream.
  public int Get (GET_CONTEXT context, out char ch)
  {
    ref ContextReadFromStdIn ct = *((ContextReadFromStdIn *)context);
    uint read;
    char buf;

    clear ch;

    if (ct.buffered)
    {
      ct.buffered = false;
      ch = ct.c;
      return 0;
    }

    if (ReadFile (GetStdHandle(-10), (byte *)&buf, (uint)1, &read, null) == 0)
      return -1;  // i/oerror
    if (read == 0)
      return -2;  // end of stream

    if (buf == '\r')   // get following '\n'    ("\r\n" -> "\n")
    {
      if (ReadFile (GetStdHandle(-10), (byte *)&buf, (uint)1, &read, null) == 0)
        return -1;  // i/o error
      if (read == 0)
        return -2;  // end of stream
    }

    ct.c = buf;  // save for UnGet
    ch = buf;
    return 0;
  }

  public void UnGet (GET_CONTEXT context)
  {
    ref ContextReadFromStdIn ct = *((ContextReadFromStdIn *)context);
    ct.buffered = true;
  }

end ReadFromStdIn;

#endif // WINDOWS

//---------------------------------------------------------------

#if WINDOWS

// bugs: cannot be called from several threads simultaneously
//    as it uses a global variable for sharing buffered input.

public int scanf (string format, out object[] arg)
{
  return scan_string ((byte *)&g_RdCt, Get, UnGet, format, out arg);
}

#endif // WINDOWS

//---------------------------------------------------------------
#end unsafe
