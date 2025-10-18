
// tracing.c

use strings, files, thread, strformat;

#if WINDOWS
  use win/windows;
#endif

#if ANDROID
  use logging;
#endif

//----------------------------------------------------------------------------

int     fd;
string^ trace_filename, old_filename;
bool    prefix_date, prefix_time, prefix_msec;
long    max_size;
bool    newline;

SHARED_OBJECT o;

//----------------------------------------------------------------------------

public void open_trace (string filename,                      // for example "trace.tra"
                        long   max_file_size = 64*1024*1024,  // default is 64 MB
                        bool   date = true,
                        bool   time = true,
                        bool   msec = false)
{
#if WINDOWS
  int i;

  enter_shared_object (ref o);

  if (fd > 0)   // already open
  {
    leave_shared_object (ref o);
    return;
  }

  free (trace_filename);
  free (old_filename);

  trace_filename = new string ' (filename);

  old_filename = new string (strlen(filename) + 4);
  strcpy (out old_filename^, trace_filename^);

  // cut the extension of old_filename, if any
  for (i=strlen(old_filename^)-1; i>=0; i--)
  {
    char c;

    c = old_filename^[i];

    if (c == '/' || c == '\\')
      break;

    if (c == '.')
    {
      old_filename^[i] = nul;
      break;
    }
  }

  // append extension
  strcat (ref old_filename^, ".old");

  fd = files.open (filename, WRITE);
  if (fd < 0)
    fd = files.create (filename);

  files.lseek (fd, 0L, SEEK_END);

  prefix_date = date;
  prefix_time = time;
  prefix_msec = msec;
  max_size    = max_file_size;
  newline     = true;

  g_trace_is_open = true;

  leave_shared_object (ref o);
#endif

#if ANDROID
  _unused filename, max_file_size, date, time, msec;
  fd = 1;
#endif
}

//----------------------------------------------------------------------------
#begin unsafe
//----------------------------------------------------------------------------

package WriteToFile

  packed struct ContextWriteToFile
  {
    int  len;
    char buffer[1024];
  }

  void Flush (ref ContextWriteToFile c);
  int Put (PUT_CONTEXT context, string s);

end WriteToFile;

//---------------------------------------------------------------

package body WriteToFile

  public void Flush (ref ContextWriteToFile c)
  {
    files.write (fd, c.buffer[0:c.len]);
    c.len = 0;
  }

  public int Put (PUT_CONTEXT context, string s)
  {
    ref ContextWriteToFile c = *((ContextWriteToFile *)context);
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

end WriteToFile;

//---------------------------------------------------------------

int fast_unsigned_int_div_100 (int i)
{
  return (int)(((int8)i * 2748779070) >> 38);
}

//---------------------------------------------------------------

int fast_unsigned_int_mod_100 (int i)
{
  return i - 100 * fast_unsigned_int_div_100 (i);
}

//---------------------------------------------------------------

public void trace (string format, object[] arg)
{
#if WINDOWS
  ContextWriteToFile ct;
  int                f_len;

  if (fd <= 0)   // not open
    return;

  enter_shared_object (ref o);

  if (fd <= 0)   // not open
  {
    leave_shared_object (ref o);
    return;
  }

  ct.len = 0;

  if (newline && (prefix_date || prefix_time || prefix_msec))
  {
    ref char   buf[24] = ct.buffer[0:24];
    int        len;

    f_len = (int)(&ct - &ct);  // dummy statement to avoid error ct non-initialized

    len = 0;

    {
      SYSTEMTIME info;

      GetLocalTime (out info);

      if (prefix_date)
        len += sprintf (out buf[0:9], "%02u/%02u/%02u ", (uint)fast_unsigned_int_mod_100 (info.wYear), info.wMonth, info.wDay);

      if (prefix_time)
        len += sprintf (out buf[len:8], "%02u:%02u:%02u", info.wHour, info.wMinute, info.wSecond);

      if (prefix_msec)
        len += sprintf (out buf[len:4], ".%03u", info.wMilliseconds);
    }

    buf[len++] = ' ';
    buf[len++] = ':';
    buf[len++] = ' ';

    ct.len = len;
  }

  assert (format_string ((byte *)&ct, WriteToFile.Put, format, arg) == 0);
  if (ct.len > 0)
    Flush (ref ct);

  f_len = strlen(format);
  newline = f_len > 0 && format[f_len-1] == '\n';

  if (filesize(fd) >= max_size)
  {
    // we don't care if the following operations fail
    files.close (fd);
    files.delete_file (old_filename^);
    files.move_file (trace_filename^, old_filename^);
    fd = files.create (trace_filename^);
  }

  leave_shared_object (ref o);
#endif // WINDOWS

#if ANDROID
  if (fd != 0)
    logging.log (format, arg);
#endif
}

#end unsafe

//----------------------------------------------------------------------------

public void close_trace ()
{
#if WINDOWS
  enter_shared_object (ref o);

  if (fd > 0)   // open
  {
    files.close (fd);
    fd = 0;
  }

  g_trace_is_open = false;

  leave_shared_object (ref o);
#endif  

#if ANDROID
  fd = 0;
#endif  
}

//----------------------------------------------------------------------------

public void trace_block (byte[] block)
{
  uint offset, i;
  byte ch;
  char line[80], item[32];

  if (fd <= 0)   // not open
    return;

  for (offset=0; offset<(uint)block'length; offset+=16)
  {
    sprintf (out line, "%06x |", (offset & 0xFFFFFF));  // 8 chars

    for (i=0; i<16; i++)        // 16 x 3 = 48 chars
    {
      if (offset + i < (uint)block'length)
        sprintf (out item, " %02x", block[offset+i]);
      else
        strcpy (out item, "   ");
      strcat (ref line, item);
    }

    strcat (ref line, " | ");    // 3 chars

    for (i=0; i<16; i++)       // 16 chars
    {
      if (offset + i < (uint)block'length)
      {
        ch = block[offset+i];
        if (ch >= 32 && ch != 127)
          sprintf (out item, "%c", (char)ch);
        else
          strcpy (out item, ".");
        strcat (ref line, item);
      }
      else
      {
        strcat (ref line, " ");
      }
    }

    trace ("%s\n", line);    // 8 + 48 + 3 + 16 = 75 chars
  }
}

//----------------------------------------------------------------------------
