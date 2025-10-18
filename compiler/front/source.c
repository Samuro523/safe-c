
// source.c : open, read source file, and convert in UTF-16.

from std use files, strings;

/*****************************************************************/

// load source file into heap memory block

public
int load_source_file (string filename, out byte[]^ source)
{
  int     fd, rc;
  long    size;
  byte[]^ p;

  source = null;

  fd = open (filename);
  if (fd < 0)
    return -1;

  size = lseek (fd, 0L, SEEK_END);
  if (size < 0)
  {
    close (fd);
    return -1;
  }

  if (lseek (fd, 0L, SEEK_SET) < 0)
  {
    close (fd);
    return -1;
  }

  p = new byte [(uint)size];

  rc = read (fd, out p^);
  if (rc != size)
  {
    free p;
    close (fd);
    return -1;
  }

  if (close (fd) < 0)
  {
    free (p);
    return -1;
  }

  source = p;

  return 0;
}

/*****************************************************************/

// illegal characters are converted to zero.
// a final zero trailing character is appended.

public
int convert_source_file_to_utf16 (    byte[]   source,
                                  out wchar[]^ target)
{
  const byte UTF8_HEADER[3] = {0xEF, 0xBB, 0xBF};
  const byte UTF16_LE[2] = {0xFF, 0xFE};
  const byte UTF16_BE[2] = {0xFE, 0xFF};

  int      head, i, target_length, k;
  bool     odd, order;
  byte     b;
  wchar    w;
  uint     l;

  target = null;

  if (source'size >= 3 && memcmp (source[0:3], UTF8_HEADER) == 0)
  {
    head = 3;

//  Unicode Character   Byte1      Byte2      Byte3      Byte4
//  -----------------   --------   --------   --------   --------
//      0 to 127        0xxxxxxx
//    128 to 2047       110yyyxx   10xxxxxx
//   2048 to 65535      1110yyyy   10yyyyxx   10xxxxxx
//  65536 to 1114111    11110zzz   10zzyyyy   10yyyyxx   10xxxxxx

    target_length = 0;
    for (i=head; i<source'length; )
    {
      b = source[i];
      if ((b & 0x80) == 0)
      {
        target_length++;
        i++;
      }
      else if ((b & 0xE0) == 0xC0 && i+2 <= source'length)
      {
        target_length++;
        i += 2;
      }
      else if ((b & 0xF0) == 0xE0 && i+3 <= source'length)
      {
        target_length++;
        i += 3;
      }
      else if ((b & 0xF8) == 0xF0 && i+4 <= source'length)
      {
        target_length += 2;
        i += 4;
      }
      else  // illegal character
      {
        target_length++;
        i++;
      }
    }

    // final nul character
    target_length++;

    target = new wchar [target_length];

    {
      ref wchar[] ptarget = target^;



  //  Unicode Character   Byte1      Byte2      Byte3      Byte4
  //  -----------------   --------   --------   --------   --------
  //      0 to 127        0xxxxxxx
  //    128 to 2047       110yyyxx   10xxxxxx
  //   2048 to 65535      1110yyyy   10yyyyxx   10xxxxxx
  //  65536 to 1114111    11110zzz   10zzyyyy   10yyyyxx   10xxxxxx

      k = 0;
      for (i=head; i<source'length; )
      {
        b = source[i];
        if ((b & 0x80) == 0)
        {
          ptarget[k++] = (wchar)b;
          i++;
        }
        else if ((b & 0xE0) == 0xC0 && i+2 <= source'length)
        {
          w = (wchar)((((uint)(b & 31)) << 6) + (source[i+1] & 63));

          if (w < (wchar)128)    // illegal
            w = (wchar)0;

          if ((source[i+1] & 192) != 128)
            w = (wchar)0;   // illegal wchar

          ptarget[k++] = w;
          i += 2;
        }
        else if ((b & 0xF0) == 0xE0 && i+3 <= source'length)
        {
          w = (wchar)((((uint)(b & 15)) << 12)
                      + (((uint)(source[i+1] & 63)) << 6)
                      + (source[i+2] & 63));

          if (w < (wchar)2048)    // illegal
            w = (wchar)0;

          if ((source[i+1] & 192) != 128)
            w = (wchar)0;   // illegal wchar

          if ((source[i+2] & 192) != 128)
            w = (wchar)0;   // illegal wchar

          if (w >= (wchar)0xD800 && w <= (wchar)0xDFFF)
            w = (wchar)0;   // illegal wchar
      
          if (w >= (wchar)0xFFFE && w <= (wchar)0xFFFF)
            w = (wchar)0;   // illegal wchar

          ptarget[k++] = w;
          i += 3;
        }

  //  Unicode Character   Byte1      Byte2      Byte3      Byte4
  //  -----------------   --------   --------   --------   --------
  //      0 to 127        0xxxxxxx
  //    128 to 2047       110yyyxx   10xxxxxx
  //   2048 to 65535      1110yyyy   10yyyyxx   10xxxxxx
  //  65536 to 1114111    11110zzz   10zzyyyy   10yyyyxx   10xxxxxx

        else if ((b & 0xF8) == 0xF0 && i+4 <= source'length)
        {
          l = (((uint)(b & 7)) << 18)
            + (((uint)(source[i+1] & 63)) << 12)
            + (((uint)(source[i+2] & 63)) << 6)
            + (source[i+3] & 63);

          if (l < 65536 || l > 0x10FFFF)    // illegal
            l = 0;

          if ((source[i+1] & 192) != 128)
            l = 0;   // illegal wchar

          if ((source[i+2] & 192) != 128)
            l = 0;   // illegal wchar

          if ((source[i+3] & 192) != 128)
            l = 0;   // illegal wchar

          if ((l & 0xFFFF) >= 0xFFFE)
            l = 0;   // illegal wchar

          if (l == 0)
          {
            ptarget[k++] = (wchar)0;   // set illegal char
            ptarget[k++] = (wchar)0;   // set illegal char
          }
          else
          {
            l -= 0x10000;

            ptarget[k++] = (wchar)(0xD800 + (l >> 10));
            ptarget[k++] = (wchar)(0xDC00 + (l & 1023));
          }

          i += 4;
        }
        else  // illegal char
        {
          ptarget[k++] = (wchar)0;
          i++;
        }
      }

      // final nul character
      ptarget[k++] = (wchar)0;

      if (k != target_length)  // intern error
        return -1;
    }
  }
  else if ((source'length >= 2 && memcmp (source[0:2], UTF16_LE) == 0) ||
           (source'length >= 2 && memcmp (source[0:2], UTF16_BE) == 0) ||
           (source'length >= 1 && source[0] == 0) ||
           (source'length >= 2 && source[1] == 0))
  {
    if ((source'length >= 1 && source[0] == 0) ||
        (source'length >= 2 && source[1] == 0))
      head = 0;
    else
      head = 2;

    if ((source'length >= 2 && memcmp (source[0:2], UTF16_LE) == 0) ||
        (source'length >= 2 && source[1] == 0))
      order = false;  // little-end
    else
      order = true;  // big-end

    target_length = source'length - head;   // length of source text, in bytes
    odd = (target_length & 1) > 0;
    if (odd)
      target_length++;
    target_length >>= 1;    // length of target text, in wchars

    // final nul character
    target_length++;

    target = new wchar [target_length];
    
    {
      ref wchar[] ptarget = target^;

      for (i=head,k=0; i<source'length; i+=2,k++)
      {
        if (order) // big-end
          w = (wchar)((((uint)source[i]) << 8) + source[i+1]);
        else  // little-end
          w = (wchar)(source[i] + (((uint)source[i+1]) << 8));

        if (w >= (wchar)0xD800 && w <= (wchar)0xDFFF)
        {
          if (w >= (wchar)0xD800 && w <= (wchar)0xDBFF)   // first surrogate word
          {
            if (k+1 == target_length)  // second word is cut
              w = (wchar)0;
          }
          else      // second surrogate word
          {
            if (k == 0)  // first word is cut
              w = (wchar)0;
            else if (ptarget[k-1] < (wchar)0xD800 ||
                     ptarget[k-1] > (wchar)0xDBFF)   // first word is out of range
            {
              ptarget[k-1] = (wchar)0;
              w = (wchar)0;
            }
            else  // check that surrogate lower word is not FFFE or FFFF or larger than 0x10FFFF.
            {
              l = 0x10000 + (((uint)ptarget[k-1] - 0xD800) << 10) + ((uint)w - 0xDC00);
              if ((l & 0xFFFF) >= 0xFFFE || l > 0x10FFFF)
              {
                ptarget[k-1] = (wchar)0;
                w = (wchar)0;
              }
            }
          }
        }

        ptarget[k] = w;
      }

      if (odd)
        ptarget[k++] = (wchar)0;

      // trailing nul
      ptarget[k++] = (wchar)0;

      if (k != target_length)  // intern error
        return -1;
    }
  }
  else  // ANSI
  {
    target_length = source'length;

    // final nul character
    target_length++;

    target = new wchar [target_length];
    
    {
      ref wchar[] ptarget = target^;

      i = 0;
      for (i=0,k=0; i<source'length; i++,k++)   // ANSI to UTF-16
        ptarget[k] = (wchar)source[i];

      // trailing nul
      ptarget[k++] = (wchar)0;

      if (k != target_length)  // intern error
        return -1;
    }
  }

  return 0;
}

/*****************************************************************/
