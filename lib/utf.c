
// utf.c : conversion between ascii, utf-8 and utf-16

//===========================================================================

public void ascii_to_utf8_target_length (char[]  source,
                                         int     source_length,
                                         out int target_length)
{
  int i;

  target_length = 0;

  for (i=0; i<source_length; i++)
  {
    if ((byte)source[i] < 128)
      target_length++;
    else
      target_length += 2;
  }
}

//===========================================================================

public void ascii_to_utf8 (char[]      source,
                           int         source_length,
                           out char[]  target,         // must be at least 2X larger than source !
                           out int     target_length)
{
  int  i;
  byte b;

  assert target'length >= 2*source'length;

  clear target;
  target_length = 0;

  for (i=0; i<source_length; i++)
  {
    b = (byte)source[i];
    if (b < 128)
      target[target_length++] = (char)b;
    else
    {
      target[target_length++] = (char)(0b1100_0000 + (b >> 6));
      target[target_length++] = (char)(0b1000_0000 + (b & 0b11_1111));
    }
  }
}

//===========================================================================

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

public int utf8_to_ascii_target_length (char[]  source,
                                        int     source_length,
                                        out int target_length)
{
  int  i, count;
  byte b;

  count = 0;
  target_length = 0;

  for (i=0; i<source_length; )
  {
    b = (byte)source[i];
    if (b < 128)
    {
      target_length++;
      i++;
    }
    else if (b >= 0b1100_0010 &&
             b <= 0b1100_0011 && 
             i+1 < source_length &&
             (byte)source[i+1] >= 0b1000_0000 &&
             (byte)source[i+1] <= 0b1011_1111)
    {
      target_length++;
      i += 2;
    }
    else
    {
      target_length++;
      count++;
      i++;

      while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
        i++;
    }
  }

  return count;
}

//===========================================================================

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

public int utf8_to_ascii (char[]      source,
                          int         source_length,
                          out char[]  target,           // must be at least as long as source !
                          out int     target_length,
                          char        replacement_char = '?')
{
  int  i, count;
  byte b;

  assert target'length >= source'length;

  count = 0;
  clear target;
  target_length = 0;

  for (i=0; i<source_length; )
  {
    b = (byte)source[i];
    if (b < 128)
    {
      target[target_length++] = (char)b;
      i++;
    }
    else if (b >= 0b1100_0010 &&
             b <= 0b1100_0011 && 
             i+1 < source_length &&
             (byte)source[i+1] >= 0b1000_0000 &&
             (byte)source[i+1] <= 0b1011_1111)
    {
      target[target_length++] = (char)(((byte)source[i+1] & 0b0011_1111) + ((b & 3) << 6));
      i += 2;
    }
    else
    {
      target[target_length++] = replacement_char;
      count++;
      i++;

      while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
        i++;
    }
  }

  return count;
}

//===========================================================================

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

public int utf8_to_utf16_target_length (char[]      source,
                                        int         source_length,
                                        out int     target_length,
                                        wchar       replacement_char = L'?')
{
  int i, val, count;

  target_length = 0;
  count = 0;

  for (i=0; i<source_length; )
  {
    val = (int)source[i];

    if (val < 128)
    {
      i++;
    }
    else if ((val & 0xE0) == 0xC0 && i+1 < source_length)
    {
      val = (((val & 31)) << 6) + ((int)source[i+1] & 63);

      if (val < 128 || ((int)source[i+1] & 192) != 128)    // illegal
      {
        val = (int)replacement_char;
        count++;

        i++;
        while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
          i++;
      }
      else  // valid
      {
        i += 2;
      }
    }
    else if ((val & 0xF0) == 0xE0 && i+2 < source_length)
    {
      val = (((val & 15)) << 12)
          + ((((int)source[i+1] & 63)) << 6)
          + ((int)source[i+2] & 63);

      if (val < 2048 ||    // illegal
          ((int)source[i+1] & 192) != 128 ||
          ((int)source[i+2] & 192) != 128 ||
          (val >= 0xD800 && val <= 0xDFFF) ||
          (val >= 0xFFFE && val <= 0xFFFF))
      {
        val = (int)replacement_char;
        count++;

        i++;
        while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
          i++;
      }
      else  // valid
      {
        i += 3;
      }
    }
    else if ((val & 0xF8) == 0xF0 && i+3 < source_length)
    {
      val = (((val & 7)) << 18)
          + ((((int)source[i+1] & 63)) << 12)
          + ((((int)source[i+2] & 63)) << 6)
          + ((int)source[i+3] & 63);

      if (val < 65536 || val > 0x10FFFF ||    // illegal
          ((int)source[i+1] & 192) != 128 ||
          ((int)source[i+2] & 192) != 128 ||
          ((int)source[i+3] & 192) != 128 ||
          ((int)val & 0xFFFF) >= 0xFFFE)
      {
        val = (int)replacement_char;
        count++;

        i++;
        while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
          i++;
      }
      else  // valid
      {
        i += 4;
      }
    }
    else
    {
      val = (int)replacement_char;
      count++;

      i++;
      while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
        i++;
    }


    if (val <= 0xFFFF)
    {
      target_length++;
    }
    else
    {
      target_length += 2;
    }
  }

  return count;
}

//===========================================================================

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

public int utf8_to_utf16 (char[]      source,
                          int         source_length,
                          out wchar[] target,           // must be at least 2X larger than source !
                          out int     target_length,
                          wchar       replacement_char = L'?')
{
  int i, val, count;

  assert target'length >= 2*source'length;

  clear target;
  target_length = 0;
  count = 0;

  for (i=0; i<source_length; )
  {
    val = (int)source[i];

    if (val < 128)
    {
      i++;
    }
    else if ((val & 0xE0) == 0xC0 && i+1 < source_length)
    {
      val = (((val & 31)) << 6) + ((int)source[i+1] & 63);

      if (val < 128 || ((int)source[i+1] & 192) != 128)    // illegal
      {
        val = (int)replacement_char;
        count++;

        i++;
        while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
          i++;
      }
      else  // valid
      {
        i += 2;
      }
    }
    else if ((val & 0xF0) == 0xE0 && i+2 < source_length)
    {
      val = (((val & 15)) << 12)
          + ((((int)source[i+1] & 63)) << 6)
          + ((int)source[i+2] & 63);

      if (val < 2048 ||    // illegal
          ((int)source[i+1] & 192) != 128 ||
          ((int)source[i+2] & 192) != 128 ||
          (val >= 0xD800 && val <= 0xDFFF) ||
          (val >= 0xFFFE && val <= 0xFFFF))
      {
        val = (int)replacement_char;
        count++;

        i++;
        while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
          i++;
      }
      else  // valid
      {
        i += 3;
      }
    }
    else if ((val & 0xF8) == 0xF0 && i+3 < source_length)
    {
      val = (((val & 7)) << 18)
          + ((((int)source[i+1] & 63)) << 12)
          + ((((int)source[i+2] & 63)) << 6)
          + ((int)source[i+3] & 63);

      if (val < 65536 || val > 0x10FFFF ||    // illegal
          ((int)source[i+1] & 192) != 128 ||
          ((int)source[i+2] & 192) != 128 ||
          ((int)source[i+3] & 192) != 128 ||
          ((int)val & 0xFFFF) >= 0xFFFE)
      {
        val = (int)replacement_char;
        count++;

        i++;
        while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
          i++;
      }
      else  // valid
      {
        i += 4;
      }
    }
    else
    {
      val = (int)replacement_char;
      count++;

      i++;
      while (i < source_length && ((byte)source[i] & 0b11000000) == 0b10000000)
        i++;
    }


    if (val <= 0xFFFF)
    {
      target[target_length++] = (wchar)val;
    }
    else
    {
      val -= 0x10000;

      target[target_length++] = (wchar)(0xD800 + (val >> 10));
      target[target_length++] = (wchar)(0xDC00 + (val & 1023));
    }
  }

  return count;
}

//===========================================================================

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

public int utf16_to_utf8_target_length (wchar[]     source,
                                        int         source_length,
                                        out int     target_length,
                                        wchar       replacement_char = L'?')
{
  int i, val, val2, count;

  target_length = 0;
  count = 0;

  for (i=0; i<source_length; )
  {
    val = (int)source[i];

    if (val < 0xD800 || (val > 0xDFFF && val < 0xFFFE))
    {
      i++;  // valid
    }
    else if (val < 0xFFFE && i+1 < source_length)   // two words
    {
      val2 = (int)source[i+1];

      if (val >= 0xD800 && val <= 0xDBFF && val2 >= 0xDC00 && val2 <= 0xDFFF)
      {
        val = 0x10000 + ((val - 0xD800) << 10) + (val2 - 0xDC00);
        i += 2;
      }
      else
      {
        val = (int)replacement_char;
        count++;
        i++;
      }
    }
    else  // illegal FFFE or FFFF
    {
      val = (int)replacement_char;
      count++;
      i++;
    }

    if (val < (1<<7))
    {
      target_length++;
    }
    else if (val < (1<<11))
    {
      target_length += 2;
    }
    else if (val <= 0xFFFF)
    {
      target_length += 3;
    }
    else
    {
      target_length += 4;
    }
  }

  return count;
}

//===========================================================================

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

public int utf16_to_utf8 (wchar[]     source,
                          int         source_length,
                          out char[]  target,           // must be at least 3X larger than source !
                          out int     target_length,
                          wchar       replacement_char = L'?')
{
  int i, val, val2, count;
  
  assert target'length >= 3*source'length;

  clear target;
  target_length = 0;
  count = 0;

  for (i=0; i<source_length; )
  {
    val = (int)source[i];

    if (val < 0xD800 || (val > 0xDFFF && val < 0xFFFE))
    {
      i++;  // valid
    }
    else if (val < 0xFFFE && i+1 < source_length)   // two words
    {
      val2 = (int)source[i+1];

      if (val >= 0xD800 && val <= 0xDBFF && val2 >= 0xDC00 && val2 <= 0xDFFF)
      {
        val = 0x10000 + ((val - 0xD800) << 10) + (val2 - 0xDC00);
        i += 2;
      }
      else
      {
        val = (int)replacement_char;
        count++;
        i++;
      }
    }
    else  // illegal FFFE or FFFF
    {
      val = (int)replacement_char;
      count++;
      i++;
    }

    if (val < (1<<7))
    {
      target[target_length++] = (char)val;
    }
    else if (val < (1<<11))
    {
      target[target_length++] = (char)(0b1100_0000 + (val >> 6));
      target[target_length++] = (char)(0b1000_0000 + (val & 0b11_1111));
    }
    else if (val <= 0xFFFF)
    {
      target[target_length++] = (char)(0b1110_0000 + ((val >> 12) & 0b1111));
      target[target_length++] = (char)(0b1000_0000 + ((val >> 6) & 0b11_1111));
      target[target_length++] = (char)(0b1000_0000 + (val & 0b11_1111));
    }
    else
    {
      target[target_length++] = (char)(0b1111_0000 + ((val >> 18) & 0b111));
      target[target_length++] = (char)(0b1000_0000 + ((val >> 12) & 0b11_1111));
      target[target_length++] = (char)(0b1000_0000 + ((val >> 6) & 0b11_1111));
      target[target_length++] = (char)(0b1000_0000 + (val & 0b11_1111));
    }
  }

  return count;
}

//===========================================================================

public void ascii_to_utf16 (char[]      source,
                            int         source_length,
                            out wchar[] target,         // must be at least as long as source !
                            out int     target_length)  // will be same as source_length
{
  int i;

  assert target'length >= source'length;

  clear target;

  for (i=0; i<source_length; i++)
    target[i] = (wchar)(int)source[i];

  target_length = source_length;
}

//===========================================================================

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

public int utf16_to_ascii (wchar[]    source,
                           int        source_length,
                           out char[] target,           // must be at least as long as source !
                           out int    target_length,    // will be same as source_length
                           char       replacement_char = '?')
{
  int i, count, val;

  assert target'length >= source'length;

  clear target;
  count = 0;

  for (i=0; i<source_length; i++)
  {
    val = (int)source[i];
    if (val <= 255)
      target[i] = (char)val;
    else
    {
      target[i] = replacement_char;
      count++;
    }
  }

  target_length = source_length;
  return count;
}

//===========================================================================
