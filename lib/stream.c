
// stream.c

use files, strings;

//-------------------------------------------------------------------------
#begin unsafe
//-------------------------------------------------------------------------

struct READ_STREAM
{
  bool  use_memory;
  
  // for file
  int   fd;
  
  // for memory
  uint  pos;     // file position
  byte* buffer;  // buffer to read
  uint  size;    // size of buffer
}

//-------------------------------------------------------------------------

struct WRITE_STREAM
{
  bool  use_memory;
  
  // for file
  int      fd;
  wstring^ pfilename;
  
  // for memory
  uint    pos;     // file position
  uint    size;    // largest size written
  byte[]^ buffer;  // buffer to read
}

//-------------------------------------------------------------------------

void wcstrcpy (out wstring dest, string src)
{
  int i;

  clear dest;

  for (i=0; i<src'length && src[i] != nul; i++)
    dest[i] = (wchar)(uint)src[i];
}

//----------------------------------------------------------------------------

public int wopen_file_stream (out READ_STREAM stream, wstring filename)
{
  clear stream;
  stream.fd = wopen (filename);
  if (stream.fd < 0)
    return stream.fd;
  return 0;
}

public int open_file_stream (out READ_STREAM stream, string filename)
{
  wchar[MAX_FILENAME_LENGTH]  wfilename;
  wcstrcpy (out wfilename, filename);
  return wopen_file_stream (out stream, wfilename);
}

//-------------------------------------------------------------------------

public void open_memory_stream (out READ_STREAM stream, byte[] memory)
{
  clear stream;
  stream.use_memory = true;
  stream.buffer = &memory;
  stream.size = memory'size;
}

//-------------------------------------------------------------------------

public bool is_ropen (READ_STREAM stream)
{
  return stream.fd > 0 || stream.buffer != null;
}

//-------------------------------------------------------------------------

public int read (ref READ_STREAM stream, out byte[] buffer)
{
  if (stream.use_memory)
  {
    uint len = stream.size - stream.pos;   // remaining bytes in memory
    byte* dummy = &buffer;
    _unused dummy;
    if (buffer'size < len)
      len = buffer'size;
    buffer[0:len] = stream.buffer[stream.pos:len];
    stream.pos += len;
    return (int)len;
  }
  else
  {
    return files.read (stream.fd, out buffer);
  }
}

//-------------------------------------------------------------------------

public long lseekr (ref READ_STREAM stream, long offset, SEEK_MODE mode = SEEK_SET)
{
  if (stream.use_memory)
  {
    long pos;
    
    switch (mode)
    {
      case SEEK_SET:  // absolute position
        pos = offset;
        break;
        
      case SEEK_CUR:  // relative from current position
        pos = stream.pos + offset;
        break;
        
      case SEEK_END:  // relative from end of file
        pos = stream.size + offset;
        break;
        
      default:
        abort;
    }
    
    if (pos < 0 || pos > stream.size)
      return -1;
      
    stream.pos = (uint)pos;
    return pos;
  }
  else
  {
    return lseek (stream.fd, offset, (files.SEEK_MODE)(uint)mode);
  }
}

//-------------------------------------------------------------------------

public void rclose (ref READ_STREAM stream)
{
  if (!stream.use_memory)
    assert close (stream.fd) == 0;
  clear stream;
}

//-------------------------------------------------------------------------

public int wcreate_file_stream (out WRITE_STREAM stream, wstring filename)
{
  clear stream;
  stream.fd = wcreate (filename);
  if (stream.fd < 0)
    return stream.fd;
  stream.pfilename = new wstring ' (filename[0:wstrlen(filename)]);
  return 0;
}

public int create_file_stream (out WRITE_STREAM stream, string filename)
{
  wchar[MAX_FILENAME_LENGTH]  wfilename;
  wcstrcpy (out wfilename, filename);
  return wcreate_file_stream (out stream, wfilename);
}

//-------------------------------------------------------------------------

public void create_memory_stream (out WRITE_STREAM stream)
{
  clear stream;
  stream.use_memory = true;
  stream.buffer = new byte[1024];
}

//-------------------------------------------------------------------------

public bool is_wopen (WRITE_STREAM stream)
{
  return stream.fd > 0 || stream.buffer != null;
}

//-------------------------------------------------------------------------

void enlarge (ref WRITE_STREAM stream, uint by)
{
  byte[]^  old = stream.buffer;
  uint     new_size = ((old^'size + by + 4095) >> 12) << 13;  // round to 4K, mult by 2.
  stream.buffer = new byte[new_size];
  stream.buffer^[0:stream.size] = old^[0 : stream.size];
  free old;
}

//-------------------------------------------------------------------------

public int write (ref WRITE_STREAM stream, byte[] buffer)
{
  if (stream.use_memory)
  {
    uint len = stream.buffer^'size - stream.pos;   // remaining bytes to write
    if (buffer'size > len)  // not enough space left
      enlarge (ref stream, buffer'size - len);
    stream.buffer^[stream.pos:buffer'size] = buffer;
    stream.pos += buffer'size;
    if (stream.pos > stream.size)
      stream.size = stream.pos;
    return (int)buffer'size;
  }
  else
  {
    return files.write (stream.fd, buffer);
  }
}

//-------------------------------------------------------------------------

public long lseekw (ref WRITE_STREAM stream, long offset, SEEK_MODE mode = SEEK_SET)
{
  if (stream.use_memory)
  {
    long pos;
    
    switch (mode)
    {
      case SEEK_SET:  // absolute position
        pos = offset;
        break;
        
      case SEEK_CUR:  // relative from current position
        pos = stream.pos + offset;
        break;
        
      case SEEK_END:  // relative from end of file
        pos = stream.buffer'size + offset;
        break;
        
      default:
        abort;
    }
    
    if (pos < 0)
      return -1;

    if ((uint)pos > stream.buffer'size)   // we need to enlarge the memory zone
      enlarge (ref stream, (uint)pos - stream.buffer'size);
      
    stream.pos = (uint)pos;
    return pos;
  }
  else
  {
    return lseek (stream.fd, offset, (files.SEEK_MODE)(uint)mode);
  }
}

//-------------------------------------------------------------------------

// returns null for file stream

public byte[]^ wclose_and_get_memory_stream (ref WRITE_STREAM stream, ref int rc)
{
  if (stream.use_memory)
  {
    if (rc < 0)  // image creation failed
    {
      free stream.buffer;
      clear stream;
      return null;
    }
    else
    {
      byte[]^ p = new byte[] ' (stream.buffer^[0:stream.size]);
      free stream.buffer;
      clear stream;
      return p;
    }
  }
  else
  {
    if (rc < 0)  // image creation failed
    {
      close (stream.fd);
      wdelete_file (stream.pfilename^);  // can fail if antivirus locks the file or windows is slow releasing the handle.
    }
    else
    {
      rc = close (stream.fd);
    }
    free stream.pfilename;
    clear stream;
    return null;
  }
}

//-------------------------------------------------------------------------
#end unsafe
//-------------------------------------------------------------------------
