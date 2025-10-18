
// strformat : low-level routines for string formatting

//---------------------------------------------------------------

int8 fast_unsigned_long_div_10 (int8 i)
{
  if (i <= int'max)
    return (i * 3435973837) >> 35;
  else
    return i / 10;
}

//---------------------------------------------------------------

int fast_unsigned_int_div_10 (int i)
{
  return (int)(((int8)i * 3435973837) >> 35);
}

//---------------------------------------------------------------

int fast_unsigned_int_div_100 (int i)
{
  return (int)(((int8)i * 2748779070) >> 38);
}

//---------------------------------------------------------------

// convert to decimal string.
// returns active length of result string.

public int itoa (long value, out string(20) buffer)
{
  long   l, q;
  int    i, r, len;
  string s(20);
  bool   minus;

  clear s;
  l = value;
  i = s'length;

  minus = false;
  if (l < 0)
  {
    if (l == long'min)
    {
      buffer = "-9223372036854775808";
      return buffer'length;
    }

    minus = true;
    l = -l;
  }

  for (;;)
  {
    q = fast_unsigned_long_div_10 (l);
    r = (int)(l - (q << 1) - (q << 3));
    l = q;

    s[--i] = (char)((int)'0' + r);

    if (l == 0)
      break;
  }

  if (minus)
  {
    s[--i] = '-';
  }

  clear buffer;

  len = s'length - i;
  buffer[0:len] = s[i:len];

  return len;
}

//---------------------------------------------------------------

// convert to unsigned hex string.
// returns active length of result string.

public int itoh (long value, out string(16) buffer)
{
  const string(16) hex = "0123456789abcdef";
  long       l;
  int        i, len;
  string(16) s;

  clear s;

  l = value;
  i = 16;
  for (;;)
  {
    s[--i] = hex[(int)l & 15];
    l = l >> 4;
    if (l == 0 | i == 0)
      break;
  }

  len = s'length - i;

  clear buffer;
  buffer[0:len] = s[i:len];

  return len;
}

//---------------------------------------------------------------

int min (int a, int b)
{
  if (a < b) return a;
  return b;
}

//---------------------------------------------------------------

// mantissa must be at least 5 characters long (for #INF#).

void low_ftoa (double d, out bool negative, out string mantissa, out int exponent)
{
  const int8 power10[9] = {4621819117588971520,   // 1.0e1
                           4636737291354636288,   // 1.0e2
                           4666723172467343360,   // 1.0e4
                           4726483295884279808,   // 1.0e8
                           4846369599423283200,   // 1.0e16
                           5085611494797045271,   // 1.0e32
                           5564284217833028085,   // 1.0e64
                           6521906365687930162,   // 1.0e128
                           8436737289693151036};  // 1.0e256
  double f = d;
  int    e, i, digit;
  double pow, pow0, error, f0;
  bool   correction_needed;

  negative = false;
  if (f < 0.0)
  {
    negative = true;
    f = -f;
  }

  if (f >= 1.0)
  {
    e = 0;
    pow = 1.0;

    for (i=8; i>=0; i--)
    {
      if (e + (1<<i) > 308)
        continue;

      {
        double power10_value;
        power10_value'byte = power10[i]'byte;
        pow0 = power10_value * pow;
      }

      if (f >= pow0)
      {
        pow = pow0;
        e += (1<<i);
      }
    }

    f /= pow;   // yields a number between [1.0 and 10.0[  (theorically)
  }
  else
  {
    correction_needed = false;
    if (f < 1.0E-256)
    {
      f *= 1.0E+128;
      correction_needed = true;
    }

    e = 0;
    pow = 1.0;

    for (i=8; i>=0; i--)
    {
      if (e + (1<<i) > 308)
        continue;

      {
        double power10_value;
        power10_value'byte = power10[i]'byte;
        pow0 = power10_value * pow;
      }

      f0 = f * pow0; // store in local variable to avoid rounding in float stack !
                     // (it can otherwise cause a final f == 10 !)

      if (f0 < 10.0)
      {
        pow = pow0;
        e += (1<<i);
      }
    }

    e = -e;
    if (correction_needed)
      e -= 128;

    f *= pow;   // yields a number between [1.0 and 10.0[, or 0

    if (f == 0.0)   // underflow
      e = 0;
  }

  if (f >= 10.0)   // this can happen due to rounding !  (try 10.0 / 100000000.0)
  {
    f /= 10.0;
    e++;
  }

  exponent = e;


  mantissa = {all => '0'};   // prefill with zero digits

  error = 10.0 * 0x1.0p-56;  // 0.5 of mantissa's last digit (seems best value)

  for (i=0; i<mantissa'length; i++)
  {
    digit = (int)f;
    f -= (double)digit;      // entre [0.0 et 1.0[
    error *= 10.0;

    if (f < error || f > 1.0-error)
    {
      if (f >= 0.5)
      {
        digit++;      // fix last digit
        if (digit > 9)
          digit = 9;  // just in case it overflows (shouldn't occur theorically)
      }

      mantissa[i++] = (char)(digit + 48);

      break;   // we can break immediately, the mantissa was prefilled with zero digits.
    }

    mantissa[i] = (char)(digit + 48);
    f *= 10.0;
  }

  if ((d'byte[7] & 0x7F) == 0x7F && (d'byte[6] & 0xF0) == 0xF0)
    mantissa[0:5] = "#INF#";
}


//---------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------

int write_filler (PUT_CONTEXT context, PUT put, char c, int count)
{
  string s(16);
  int    rest = count;
  int    rc;

  if (count < 0)
    return 0;

  s = {all => c};

  while (rest >= s'length)
  {
    rc = put (context, s);
    if (rc != 0)
      return rc;
    rest -= s'length;
  }

  return put (context, s[0:rest]);
}

//---------------------------------------------------------------

int print_f (PUT_CONTEXT context,
             PUT         put,
             double      d,
             int         width,
             int         precision,
             bool        justify_left,
             bool        plus_sign,
             bool        leading_zeroes)
{
  bool       negative;
  string(16) mantissa;
  int        exponent;
  int        rc, nb_digits, len, actual_width, filler;

  low_ftoa (d, out negative, out mantissa, out exponent);

  // precompute width of result string and thus the required filler
  actual_width = exponent + 1;
  if (actual_width < 1)
    actual_width = 1;
  if (negative || plus_sign)
    actual_width++;
  if (precision > 0)
    actual_width += (1 + precision);
  filler = width - actual_width;

  if ((!justify_left) && (!leading_zeroes))
  {
    rc = write_filler (context, put, ' ', filler);
    if (rc != 0)
      return rc;
  }

  if (negative)
  {
    rc = put (context, "-");
    if (rc != 0)
      return rc;
  }
  else if (plus_sign)
  {
    rc = put (context, "+");
    if (rc != 0)
      return rc;
  }

  if ((!justify_left) && leading_zeroes)
  {
    rc = write_filler (context, put, '0', filler);
    if (rc != 0)
      return rc;
  }

  if (exponent < 0)   // format 0.xxxx
  {
    rc = put (context, "0");
    if (rc != 0)
      return rc;

    if (precision > 0)
    {
      rc = put (context, ".");
      if (rc != 0)
        return rc;

      nb_digits = precision;   // nb digits left to print

      // insert zeroes
      len = min (-exponent-1, nb_digits);
      rc = write_filler (context, put, '0', len);
      if (rc != 0)
        return rc;
      nb_digits -= len;

      if (nb_digits > 0)  // we want more digits (from mantissa)
      {
        // insert mantissa
        len = min (mantissa'length, nb_digits);
        rc = put (context, mantissa[0:len]);
        if (rc != 0)
          return rc;
        nb_digits -= len;
      }

      // insert zeroes
      rc = write_filler (context, put, '0', nb_digits);
      if (rc != 0)
        return rc;
    }
  }
  else   // starts with part or all mantissa
  {
    if (exponent+1 <= mantissa'length)    // dot within or just after mantissa
    {
      rc = put (context, mantissa[0 : exponent+1]);
      if (rc != 0)
        return rc;

      if (precision > 0)
      {
        rc = put (context, ".");
        if (rc != 0)
          return rc;

        nb_digits = precision;   // nb digits left to print

        // insert mantissa
        len = min (mantissa'length-(exponent+1), nb_digits);
        rc = put (context, mantissa[exponent+1 : len]);
        if (rc != 0)
          return rc;
        nb_digits -= len;

        // insert zeroes
        rc = write_filler (context, put, '0', nb_digits);
        if (rc != 0)
          return rc;
      }
    }
    else   // dot after mantissa and zeroes
    {
      rc = put (context, mantissa);
      if (rc != 0)
        return rc;

      // insert zeroes
      nb_digits = (exponent+1) - mantissa'length;
      rc = write_filler (context, put, '0', nb_digits);
      if (rc != 0)
        return rc;

      rc = put (context, ".");
      if (rc != 0)
        return rc;

      // insert zeroes
      rc = write_filler (context, put, '0', precision);
      if (rc != 0)
        return rc;
    }
  }

  if (justify_left)
  {
    rc = write_filler (context, put, ' ', filler);
    if (rc != 0)
      return rc;
  }

  return 0;
}

//---------------------------------------------------------------

int print_e (PUT_CONTEXT context,
             PUT         put,
             double      d,
             int         width,
             int         precision,
             bool        justify_left,
             bool        plus_sign,
             bool        leading_zeroes)
{
  bool       negative;
  string(16) mantissa;
  int        exponent, digit;
  int        rc, nb_digits, len, actual_width, filler, i;

  low_ftoa (d, out negative, out mantissa, out exponent);

  // precompute width of result string and thus the required filler
  actual_width = 4;
  if (negative || plus_sign)
    actual_width++;
  if (precision > 0)
    actual_width += (1 + precision);
  if (exponent < -9 || exponent > +9)
  {
    actual_width++;
    if (exponent < -99 || exponent > +99)
      actual_width++;
  }
  filler = width - actual_width;

  if ((!justify_left) && (!leading_zeroes))
  {
    rc = write_filler (context, put, ' ', filler);
    if (rc != 0)
      return rc;
  }

  if (negative)
  {
    rc = put (context, "-");
    if (rc != 0)
      return rc;
  }
  else if (plus_sign)
  {
    rc = put (context, "+");
    if (rc != 0)
      return rc;
  }

  if ((!justify_left) && leading_zeroes)
  {
    rc = write_filler (context, put, '0', filler);
    if (rc != 0)
      return rc;
  }

  rc = put (context, mantissa[0:1]);
  if (rc != 0)
    return rc;

  if (precision > 0)
  {
    rc = put (context, ".");
    if (rc != 0)
      return rc;

    nb_digits = precision;   // nb digits left to print

    // insert mantissa
    len = min (mantissa'length-1, nb_digits);
    rc = put (context, mantissa[1 : len]);
    if (rc != 0)
      return rc;
    nb_digits -= len;

    // insert zeroes
    rc = write_filler (context, put, '0', nb_digits);
    if (rc != 0)
      return rc;
  }

  mantissa[0] = 'e';

  if (exponent < 0)
  {
    exponent = -exponent;
    mantissa[1] = '-';
  }
  else
  {
    mantissa[1] = '+';
  }

  i = 2;

  digit = fast_unsigned_int_div_100 (exponent);
  if (digit > 0)
    mantissa[i++] = (char)(digit + 48);
  exponent -= digit * 100;

  digit = fast_unsigned_int_div_10 (exponent);
  if (digit > 0)
    mantissa[i++] = (char)(digit + 48);
  exponent -= digit * 10;

  mantissa[i++] = (char)(exponent + 48);

  rc = put (context, mantissa[0 : i]);
  if (rc != 0)
    return rc;

  if (justify_left)
  {
    rc = write_filler (context, put, ' ', filler);
    if (rc != 0)
      return rc;
  }

  return 0;
}

//---------------------------------------------------------------

// returns 0 if OK, -1 if put failed.

public int format_string (PUT_CONTEXT context, PUT put, string format, object[] arg)
{
  int  start, i, rc, width, precision, arg_count;
  bool justify_left, plus_sign, leading_zeroes;
  char c, typ;

  arg_count = 0;

  start = 0;  // start offset
  i     = 0;  // current index

  while (i < format'length)
  {
    c = format[i];

    if (c == nul)
      break;

    i++;

    if (c != '%')
      continue;

    // flush out everything except the %
    rc = put (context, format[start : i-start-1]);
    if (rc != 0)
      return rc;

    c = format[i++];

    if (c == '%')   // %%
    {
      start = i-1;
      continue;
    }

    justify_left   = false;
    plus_sign      = false;
    leading_zeroes = false;

    while (c == '-' | c == '+' | c == '0')
    {
      if (c == '-')
        justify_left = true;
      else if (c == '+')
        plus_sign = true;
      else if (c == '0')
        leading_zeroes = true;

      c = format[i++];
    }

    if (c == '*')
    {
      width'byte = arg[arg_count];   // will crash if argument is not 'int' or not existing
      arg_count++;
      c = format[i++];
    }
    else
    {
      width = 0;
      while (c >= '0' && c <= '9')
      {
        width = width * 10 + ((int)c - (int)'0');
        c = format[i++];
      }
    }
    if (width < 0)
      width = 0;

    precision = -1;  // -1 means no precision was specified
    if (c == '.')
    {
      c = format[i++];

      if (c == '*')
      {
        precision'byte = arg[arg_count];
        arg_count++;
        c = format[i++];
      }
      else
      {
        precision = 0;
        while (c >= '0' && c <= '9')
        {
          precision = precision * 10 + ((int)c - (int)'0');
          c = format[i++];
        }
      }

      if (precision < 0)
        precision = 0;
    }

    typ = c;    

    switch (typ)
    {
      case 'd':    // any signed integer   decimal number (-61)
      {
        long l;

        switch (arg[arg_count]'length)
        {
          case 1:
          {
            tiny t;
            t'byte = arg[arg_count];
            l = t;
          }
          break;

          case 2:
          {
            short s;
            s'byte = arg[arg_count];
            l = s;
          }
          break;

          case 4:
          {
            int ii;
            ii'byte = arg[arg_count];
            l = ii;
          }
          break;

          default:
          {
            l'byte = arg[arg_count];  // will crash if not 8, that's ok
          }
          break;
        }
        arg_count++;

        // now we have l, let's format it according to :
        // width, justify_left, plus_sign, leading_zeroes.
        {
          char str[20];
          int  len, fill;

          len = itoa (l, out str);

          if (plus_sign && l >= 0)
          {
            str[1:len] = str[0:len];
            str[0] = '+';
            len++;
          }

          fill = width - len;

          if (justify_left)
          {
            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;

            // append trailing blanks
            rc = write_filler (context, put, ' ', fill);
            if (rc != 0)
              return rc;
          }
          else   // justify right
          {
            if (leading_zeroes)
            {
              if (str[0] == '+' || str[0] == '-')  // there is a leading sign
              {
                // write leading sign
                rc = put (context, str[0:1]);
                if (rc != 0)
                  return rc;

                // write filling zeroes
                rc = write_filler (context, put, '0', fill);
                if (rc != 0)
                  return rc;

                // write number except leading sign
                rc = put (context, str[1:len-1]);
                if (rc != 0)
                  return rc;
              }
              else  // no sign
              {
                // write leading zeroes
                rc = write_filler (context, put, '0', fill);
                if (rc != 0)
                  return rc;

                rc = put (context, str[0:len]);
                if (rc != 0)
                  return rc;
              }
            }
            else  // leading blanks
            {
              // write blanks
              rc = write_filler (context, put, ' ', fill);
              if (rc != 0)
                return rc;

              rc = put (context, str[0:len]);
              if (rc != 0)
                return rc;
            }
          }
        }
      }
      break;

      case 'u':    // any unsigned integer or enum  unsigned number (12)
      {
        long l;

        switch (arg[arg_count]'length)
        {
          case 1:
          {
            byte b;
            b'byte = arg[arg_count];
            l = b;
          }
          break;

          case 2:
          {
            uint2 u;
            u'byte = arg[arg_count];
            l = u;
          }
          break;

          default:
          {
            uint u;
            u'byte = arg[arg_count];  // will crash if not 4, that's ok
            l = u;
          }
          break;
        }
        arg_count++;

        // now we have l, let's format it according to :
        // width, justify_left.
        {
          char str[20];
          int  len, fill;

          len = itoa (l, out str);
          fill = width - len;

          if (justify_left)
          {
            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;

            // append trailing blanks
            rc = write_filler (context, put, ' ', fill);
            if (rc != 0)
              return rc;
          }
          else   // justify right
          {
            if (leading_zeroes)
            {
              rc = write_filler (context, put, '0', fill);
              if (rc != 0)
                return rc;
            }
            else  // leading blanks
            {
              rc = write_filler (context, put, ' ', fill);
              if (rc != 0)
                return rc;
            }

            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;
          }
        }
      }
      break;

      case 'x':    // any integer or enum           hexadecimal number (7fa)
      {
        long l;

        switch (arg[arg_count]'length)
        {
          case 1:
          {
            byte b;
            b'byte = arg[arg_count];
            l = b;
          }
          break;

          case 2:
          {
            ushort s;
            s'byte = arg[arg_count];
            l = s;
          }
          break;

          case 4:
          {
            uint ii;
            ii'byte = arg[arg_count];
            l = ii;
          }
          break;

          default:
          {
            l'byte = arg[arg_count];  // will crash if not 8, that's ok
          }
          break;
        }
        arg_count++;

        // now we have l, let's format it according to :
        // width, justify_left, leading_zeroes.
        {
          char str[16];
          int  len, fill;

          len = itoh (l, out str);
          fill = width - len;

          if (justify_left)
          {
            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;

            // append trailing blanks
            rc = write_filler (context, put, ' ', fill);
            if (rc != 0)
              return rc;
          }
          else   // justify right
          {
            if (leading_zeroes)
            {
              rc = write_filler (context, put, '0', fill);
              if (rc != 0)
                return rc;
            }
            else  // leading blanks
            {
              rc = write_filler (context, put, ' ', fill);
              if (rc != 0)
                return rc;
            }

            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;
          }
        }
      }
      break;

      case 'e':    // float, double                 floating-point (3.9265e+2)
      case 'f':    // float, double                 floating-point (392.65)
      {
        double d;

        switch (arg[arg_count]'length)
        {
          case 4:
          {
            float f;
            f'byte = arg[arg_count];
            d = f;
          }
          break;

          default:
          {
            d'byte = arg[arg_count];  // will crash if not 8, that's ok
          }
          break;
        }
        arg_count++;

        if (precision == -1)
          precision = 6;

        if (typ == 'e')
        {
          rc = print_e (context, put, d, width, precision, justify_left,
                        plus_sign, leading_zeroes);
        }
        else
        {
          rc = print_f (context, put, d, width, precision, justify_left,
                        plus_sign, leading_zeroes);
        }

        if (rc != 0)
          return rc;
      }
      break;

      case 'c':    // char or string                all char, or 'precision' char
      case 's':    // string                        stops at nul, or 'precision' char
      {
        int   j, len, fill;
        char* param = (char *)&arg[arg_count];

        len = arg[arg_count]'length;
        if (precision >= 0 && precision < len)
          len = precision;

        if (typ == 's')  // must stop at nul
        {
          for (j=0; j<len; j++)
          {
            if (param[j] == nul)
            {
              len = j;
              break;
            }
          }
        }

        arg_count++;

        // now we have param, let's format it according to :
        // width, justify_left.

        fill = width - len;

        if (justify_left)
        {
          rc = put (context, param[0:len]);
          if (rc != 0)
            return rc;

          // append trailing blanks
          rc = write_filler (context, put, ' ', fill);
          if (rc != 0)
            return rc;
        }
        else   // justify right
        {
          rc = write_filler (context, put, ' ', fill);
          if (rc != 0)
            return rc;

          rc = put (context, param[0:len]);
          if (rc != 0)
            return rc;
        }
      }
      break;

      case 'C':    // wchar or wstring              all wchar, or 'precision' wchar
      case 'S':    // wstring                       stops at Lnul, or 'precision' wchar
      {
        int   j, len, fill;
        wchar* param = (wchar *)&arg[arg_count];

        len = arg[arg_count]'length;
        assert ((len & 1) == 0);   // crash if odd nb of bytes
        len >>= 1;

        if (precision >= 0 && precision < len)
          len = precision;

        if (typ == 'S')  // must stop at nul
        {
          for (j=0; j<len; j++)
          {
            if (param[j] == Lnul)
            {
              len = j;
              break;
            }
          }
        }

        arg_count++;

        // now we have param, let's format it according to :
        // width, justify_left.

        fill = width - len;

        if (justify_left)
        {
          for (j=0; j<len; j++)
          {
            string(1) str;
            clear str;
            str[0] = (char)(int)param[j];
            rc = put (context, str);
            if (rc != 0)
              return rc;
          }

          // append trailing blanks
          rc = write_filler (context, put, ' ', fill);
          if (rc != 0)
            return rc;
        }
        else   // justify right
        {
          rc = write_filler (context, put, ' ', fill);
          if (rc != 0)
            return rc;

          for (j=0; j<len; j++)
          {
            string(1) str;
            clear str;
            str[0] = (char)(int)param[j];
            rc = put (context, str);
            if (rc != 0)
              return rc;
          }
        }
      }
      break;

      default:
        return -1;  // bad typ
    }

    start = i;
  }

  return put (context, format[start : i-start]);
}

//---------------------------------------------------------------

bool is_infinite (double d)
{
  int8 h;
  h'byte = d'byte;
  return (h & 0x7FF0000000000000) == 0x7FF0000000000000;
}

//---------------------------------------------------------------

bool f_is_infinite (float f)
{
  uint4 u;
  u'byte = f'byte;
  return (u & 0x7F800000) == 0x7F800000;
}

//---------------------------------------------------------------

// returns 0 if OK, non-zero if an error occured.
// In particular :
// . -1 if a read-error occured during GET.
// . -2 if end-of-stream occured before all arguments received a value;
// . -3 if end-of-format string occured before all arguments received a value;
// . -4 if a syntax error occured in the stream;
// . -5 if an overflow occured while reading a numeric value or storing it in a numeric argument.

// a runtime occurs when the format string has an invalid format.

public int scan_string (GET_CONTEXT context, GET get, UNGET unget, string format, out object[] arg)
{
  int  i, rc, width, arg_count;
  bool store_value_in_arg;
  char c, s, typ;

  arg_count = 0;

  i = 0;  // current index

  while (i < format'length)
  {
    c = format[i++];

    if (c == nul)
      break;

    if (c <= ' ')   // white space
    {
      // skip all white space from stream

      for (;;)
      {
        rc = get (context, out s);        // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc == -1)
          return -1;
        if (rc == -2 || s > ' ')
          break;
      }

      if (rc == -2)   // end of stream -> get next format character
        continue;

      // non white-space
      unget (context);     // cancel read character and continue scanning format string
      continue;
    }

    // non white-space

    if (c != '%')  // must match with stream
    {
      rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
      if (rc == -1)
        return -1;
      if (rc == -2 || s != c)
        return -4;    // -4 if a syntax error occured in the stream;
      continue;
    }

    // c == '%'

    c = format[i++];
    if (c == '%')   // double %
    {
      rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
      if (rc == -1)
        return -1;
      if (rc == -2 || s != '%')
        return -4;    // -4 if a syntax error occured in the stream;
      continue;
    }

    store_value_in_arg = true;
    if (c == '*')
    {
      store_value_in_arg = false;
      c = format[i++];
    }


    // read width >= 0, or int'last if no width specified.

    if (c < '0' || c > '9')  // no width specified
    {
      width = int'max;  // maximum length
    }
    else
    {
      width = (int)c - 48;
      c = format[i++];

      while (c >= '0' && c <= '9' && width < int'max/10 - ((int)c - 48))
      {
        width = width * 10 + ((int)c - 48);
        c = format[i++];
      }
    }

    typ = c;
    switch (typ)
    {
      case 'd':  // any signed integer - decimal number preceded by optional + or - (-61)
      {
        bool negative = false;
        int8 value;
        int  digit, len;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        len = 0;

        if (s == '-' || s == '+')
        {
          if (s == '-')
            negative = true;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc < 0)   // read error or end-of-stream
            return rc;

          len++;   // len becomes 1

          if (width < 2)
            return -4;   // a syntax error occured in the stream
        }

        if (s < '0' || s > '9')
          return -4;   // a syntax error occured in the stream

        value = 0;

        for (;;)     // scan value as negative
        {
          len++;

          if (value < -(long'max / 10))
            return -5;   // an overflow occured

          value *= 10;

          digit = (int)s - 48;

          if (value < long'min + digit)
            return -5;   // an overflow occured

          value -= digit;

          if (len == width)    // width limit reached
            break;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)   // read error
            return -1;
          if (rc == -2)   // end of stream
            break;

          if (s < '0' || s > '9')
          {
            unget (context);     // cancel read character
            break;
          }
        }

        if (!negative)
          value = -value;

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 1:
            {
              tiny x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (tiny)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 2:
            {
              short x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (short)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 4:
            {
              int x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (int)value;
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              arg[arg_count] = value'byte;   // will crash if arglen is not 8, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;

      case 'u':  // u - any unsigned integer or enum - unsigned number (12)
      {
        uint value;
        int  digit, len;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        if (s < '0' || s > '9')
          return -4;   // a syntax error occured in the stream

        len = 0;
        value = 0;

        for (;;)
        {
          len++;

          if (value > uint'max / 10)
            return -5;   // an overflow occured

          value *= 10;

          digit = (int)s - 48;

          if (value > uint'max - (uint)digit)
            return -5;   // an overflow occured

          value += (uint)digit;

          if (len == width)    // width limit reached
            break;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)   // read error
            return -1;
          if (rc == -2)   // end of stream
            break;

          if (s < '0' || s > '9')
          {
            unget (context);     // cancel read character
            break;
          }
        }

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 1:
            {
              byte x;
              if (value > x'max)
                return -5;   // an overflow occured 
              x = (byte)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 2:
            {
              ushort x;
              if (value > x'max)
                return -5;   // an overflow occured 
              x = (ushort)value;
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              arg[arg_count] = value'byte;   // will crash if arglen is not 4, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;

      case 'x':  // same as d or u       hexadecimal number (7fa)
      {
        int8 value;
        int  digit, len, non_zero_len;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        if ((s < '0' || s > '9') && (s < 'a' || s > 'f') && (s < 'A' || s > 'F'))
          return -4;   // a syntax error occured in the stream

        len = 0;
        non_zero_len = 0;
        value = 0;

        for (;;)
        {
          if (s >= '0' && s <= '9')
            digit = (int)s - (int)'0';
          else if (s >= 'A' && s <= 'F')
            digit = (int)s - ((int)'A' - 10);
          else
            digit = (int)s - ((int)'a' - 10);

          value = (value << 4) + digit;

          if (digit > 0 || non_zero_len > 0)
            non_zero_len++;

          len++;
          if (len == width)    // width limit reached
            break;

          if (non_zero_len > 16)   // too long
            return -5;   // an overflow occured

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)   // read error
            return -1;
          if (rc == -2)   // end of stream
            break;

          if ((s < '0' || s > '9') && (s < 'a' || s > 'f') && (s < 'A' || s > 'F'))
          {
            unget (context);     // cancel read character
            break;
          }
        }

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 1:
            {
              byte x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (byte)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 2:
            {
              ushort x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (ushort)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 4:
            {
              uint x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (uint)value;
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              arg[arg_count] = value'byte;   // will crash if arglen is not 8, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;


      //  [-|+] digit_sequence  "."  digit_sequence [("e"|"E") ["+"|"-"] digit_sequence]

      case 'f':  // f   float or double      floating-point number (0.5)  (12.4E+3)
      case 'e':  // same as f
      {
        bool    negative, dot_found, correction_factor;
        int     len;
        int8    mantissa;
        int     digits_before, digits_after, leading_zeroes_after_dot, m_len;
        int     exponent, exp_sign, k;
        double  fexponent, value;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        len = 0;

        negative = false;
        if (s == '-' || s == '+')
        {
          if (s == '-')
            negative = true;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc < 0)   // read error or end-of-stream
            return rc;

          len++;   // len becomes 1

          if (width < 2)
            return -4;   // a syntax error occured in the stream
        }


        dot_found = false;
        digits_before = 0;    // nb non-zero digits before dot
        digits_after  = 0;    // nb digits after dot
        leading_zeroes_after_dot = 0;

        if (s != '.' && (s < '0' || s > '9'))
          return -4;   // a syntax error occured in the stream


        // parse leading zeroes

        while (s == '.' || s == '0')
        {
          if (s == '.')
          {
            if (dot_found)
              break;
            dot_found = true;
          }
          else  // '0'
          {
            if (dot_found)
            {
              digits_after++;
              leading_zeroes_after_dot++;
            }
          }

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)   // end of stream
            break;

          len++;
          if (len == width)
            break;
        }

        // get first 18 digits into mantissa

        mantissa = 0;
        m_len = 0;

        while (len < width && rc == 0 && (s == '.' || (s >= '0' && s <= '9')))
        {
          if (s == '.')
          {
            if (dot_found)
              break;
            dot_found = true;
          }
          else   // digit
          {
            if (m_len < 18)
            {
              mantissa = mantissa * 10 + ((int)s - (int)'0');
              m_len++;
            }

            if (dot_found)
              digits_after++;
            else
              digits_before++;
          }

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)   // end of stream
            break;

          len++;
          if (len == width)
            break;
        }

        exponent = 0;

        if (len < width && rc == 0 && (s == 'e' || s == 'E'))
        {
          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc < 0)
            return rc;

          len++;
          if (len == width)
            return -4;   // a syntax error occured in the stream

          exp_sign = +1;
          if (s == '+' || s == '-')
          {
            if (s == '-')
              exp_sign = -1;

            rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
            if (rc < 0)
              return rc;

            len++;
            if (len == width)
              return -4;   // a syntax error occured in the stream
          }

          if (s < '0' || s > '9')
            return -4;   // a syntax error occured in the stream


          while (s >= '0' && s <= '9')
          {
            exponent = exponent * 10 + ((int)s - (int)'0');

            if (exponent >= 512)
              return -5;   // an overflow occured 

            rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
            if (rc == -1)
              return -1;
            if (rc == -2)
              break;

            len++;
            if (len == width)
              break;
          }

          if (exp_sign < 0)
            exponent = -exponent;
        }

        if (rc == 0)
          unget (context);


        // parsing is finished, now compute double result

        exponent += (digits_before - m_len - leading_zeroes_after_dot);

        correction_factor = false;

        if (exponent < 0)
        {
          exponent = -exponent;
          exp_sign = -1;

          if (exponent > 256)   // fexponent might overflow (at 1.7E+308)
          {
            exponent -= 128;
            correction_factor = true;   // correction factor is 1.0E128
          }
        }
        else
        {
          exp_sign = +1;
        }


        fexponent = 1.0;

        if (exponent >= 512)   // prevent table overflow
          return -5;   // an overflow occured 

        {
          const int8 power10[9] = {4621819117588971520,   // 1.0e1
                                   4636737291354636288,   // 1.0e2
                                   4666723172467343360,   // 1.0e4
                                   4726483295884279808,   // 1.0e8
                                   4846369599423283200,   // 1.0e16
                                   5085611494797045271,   // 1.0e32
                                   5564284217833028085,   // 1.0e64
                                   6521906365687930162,   // 1.0e128
                                   8436737289693151036};  // 1.0e256
          k = 0;
          while (exponent > 0)
          {
            if ((exponent & 1) > 0)
            {
              double power10_value;
              power10_value'byte = power10[k]'byte;
              fexponent *= power10_value;
              exponent--;
            }

            exponent >>= 1;
            k++;
          }
        }


        if (exp_sign >= 0)
        {
          value = (double)mantissa * fexponent;
        }
        else
        {
          if (correction_factor)
          {
            value = ((double)mantissa / 1.0E128) / fexponent;
          }
          else
          {
            value = (double)mantissa / fexponent;
          }
        }

        if (negative)
          value = -value;

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 4:
            {
              float x = (float)value;
              if (f_is_infinite (x))
                return -5;   // an overflow occured 
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              if (is_infinite (value))
                return -5;   // an overflow occured 
              arg[arg_count] = value'byte;   // will crash if arglen is not 8, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;

      case 'c':  // c - char or string - fill arg, or read max 'width' chars.
      case 's':  // same as c but stop at first white-space.
      {
        int x;

        if (store_value_in_arg)
        {
          clear arg[arg_count];
          if (arg[arg_count]'length < width)
            width = arg[arg_count]'length;
        }

        for (x=0; x<width; x++)
        {
          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)
            break;

          if (typ == 's' && s <= ' ')
          {
            unget (context);     // cancel read character
            break;
          }

          if (store_value_in_arg)
            arg[arg_count][x] = (byte)s;
        }

        if (store_value_in_arg)
          arg_count++;
      }
      break;

      case 'C':  // wchar or wstring - fill arg, or read max 'width' wchars.
      case 'S':  // same as C but stop at first white-space.
      {
        int x;

        if (store_value_in_arg)
        {
          clear arg[arg_count];
          if ((arg[arg_count]'length >> 1) < width)
            width = (arg[arg_count]'length >> 1);
        }

        for (x=0; x<width; x++)
        {
          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)
            break;

          if (typ == 'S' && s <= ' ')
          {
            unget (context);     // cancel read character
            break;
          }

          if (store_value_in_arg)
            arg[arg_count][x<<1] = (byte)s;   // leaving high byte at zero
        }

        if (store_value_in_arg)
          arg_count++;
      }
      break;

      default:
        abort;
    }
  }

  if (arg_count < arg'length)
    return -3;      // -3 if end-of-format string occured before all arguments received a value;

  return 0;
}

//---------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------

public int witoa (long value, out wstring(20) buffer)
{
  long    l, q;
  int     i, r, len;
  wstring s(20);
  bool    minus;

  clear s;
  l = value;
  i = s'length;

  minus = false;
  if (l < 0)
  {
    if (l == long'min)
    {
      buffer = L"-9223372036854775808";
      return buffer'length;
    }

    minus = true;
    l = -l;
  }

  for (;;)
  {
    q = fast_unsigned_long_div_10 (l);
    r = (int)(l - (q << 1) - (q << 3));
    l = q;

    s[--i] = (wchar)((int)'0' + r);

    if (l == 0)
      break;
  }

  if (minus)
  {
    s[--i] = L'-';
  }

  clear buffer;

  len = s'length - i;
  buffer[0:len] = s[i:len];

  return len;
}

//---------------------------------------------------------------

// convert to unsigned hex string.
// returns active length of result string.

public int witoh (long value, out wstring(16) buffer)
{
  const wstring(16) hex = L"0123456789abcdef";
  long        l;
  int         i, len;
  wstring(16) s;

  clear s;

  l = value;
  i = 16;
  for (;;)
  {
    s[--i] = hex[(int)l & 15];
    l = l >> 4;
    if (l == 0 | i == 0)
      break;
  }

  len = s'length - i;

  clear buffer;
  buffer[0:len] = s[i:len];

  return len;
}

//---------------------------------------------------------------

// mantissa must be at least 5 characters long (for #INF#).

void wlow_ftoa (double d, out bool negative, out wstring mantissa, out int exponent)
{
  const int8 power10[9] = {4621819117588971520,   // 1.0e1
                           4636737291354636288,   // 1.0e2
                           4666723172467343360,   // 1.0e4
                           4726483295884279808,   // 1.0e8
                           4846369599423283200,   // 1.0e16
                           5085611494797045271,   // 1.0e32
                           5564284217833028085,   // 1.0e64
                           6521906365687930162,   // 1.0e128
                           8436737289693151036};  // 1.0e256
  double f = d;
  int    e, i, digit;
  double pow, pow0, error, f0;
  bool   correction_needed;

  negative = false;
  if (f < 0.0)
  {
    negative = true;
    f = -f;
  }

  if (f >= 1.0)
  {
    e = 0;
    pow = 1.0;

    for (i=8; i>=0; i--)
    {
      if (e + (1<<i) > 308)
        continue;

      {
        double power10_value;
        power10_value'byte = power10[i]'byte;
        pow0 = power10_value * pow;
      } 

      if (f >= pow0)
      {
        pow = pow0;
        e += (1<<i);
      }
    }

    f /= pow;   // yields a number between [1.0 and 10.0[  (theorically)
  }
  else
  {
    correction_needed = false;
    if (f < 1.0E-256)
    {
      f *= 1.0E+128;
      correction_needed = true;
    }

    e = 0;
    pow = 1.0;

    for (i=8; i>=0; i--)
    {
      if (e + (1<<i) > 308)
        continue;

      {
        double power10_value;
        power10_value'byte = power10[i]'byte;
        pow0 = power10_value * pow;
      }
      
      f0 = f * pow0; // store in local variable to avoid rounding in float stack !
                     // (it can otherwise cause a final f == 10 !)

      if (f0 < 10.0)
      {
        pow = pow0;
        e += (1<<i);
      }
    }

    e = -e;
    if (correction_needed)
      e -= 128;

    f *= pow;   // yields a number between [1.0 and 10.0[, or 0

    if (f == 0.0)   // underflow
      e = 0;
  }

  if (f >= 10.0)   // this can happen due to rounding !  (try 10.0 / 100000000.0)
  {
    f /= 10.0;
    e++;
  }

  exponent = e;


  mantissa = {all => L'0'};   // prefill with zero digits

  error = 10.0 * 0x1.0p-56;  // 0.5 of mantissa's last digit (seems best value)

  for (i=0; i<mantissa'length; i++)
  {
    digit = (int)f;
    f -= (double)digit;      // entre [0.0 et 1.0[
    error *= 10.0;

    if (f < error || f > 1.0-error)
    {
      if (f >= 0.5)
      {
        digit++;      // fix last digit
        if (digit > 9)
          digit = 9;  // just in case it overflows (shouldn't occur theorically)
      }

      mantissa[i++] = (wchar)(digit + 48);

      break;   // we can break immediately, the mantissa was prefilled with zero digits.
    }

    mantissa[i] = (wchar)(digit + 48);
    f *= 10.0;
  }

  if ((d'byte[7] & 0x7F) == 0x7F && (d'byte[6] & 0xF0) == 0xF0)
    mantissa[0:5] = L"#INF#";
}


//---------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------

int wwrite_filler (WPUT_CONTEXT context, WPUT put, wchar c, int count)
{
  wstring s(16);
  int     rest = count;
  int     rc;

  if (count < 0)
    return 0;

  s = {all => c};

  while (rest >= s'length)
  {
    rc = put (context, s);
    if (rc != 0)
      return rc;
    rest -= s'length;
  }

  return put (context, s[0:rest]);
}

//---------------------------------------------------------------

int wprint_f (WPUT_CONTEXT context,
              WPUT         put,
              double      d,
              int         width,
              int         precision,
              bool        justify_left,
              bool        plus_sign,
              bool        leading_zeroes)
{
  bool        negative;
  wstring(16) mantissa;
  int         exponent;
  int         rc, nb_digits, len, actual_width, filler;

  wlow_ftoa (d, out negative, out mantissa, out exponent);

  // precompute width of result string and thus the required filler
  actual_width = exponent + 1;
  if (actual_width < 1)
    actual_width = 1;
  if (negative || plus_sign)
    actual_width++;
  if (precision > 0)
    actual_width += (1 + precision);
  filler = width - actual_width;

  if ((!justify_left) && (!leading_zeroes))
  {
    rc = wwrite_filler (context, put, L' ', filler);
    if (rc != 0)
      return rc;
  }

  if (negative)
  {
    rc = put (context, L"-");
    if (rc != 0)
      return rc;
  }
  else if (plus_sign)
  {
    rc = put (context, L"+");
    if (rc != 0)
      return rc;
  }

  if ((!justify_left) && leading_zeroes)
  {
    rc = wwrite_filler (context, put, L'0', filler);
    if (rc != 0)
      return rc;
  }

  if (exponent < 0)   // format 0.xxxx
  {
    rc = put (context, L"0");
    if (rc != 0)
      return rc;

    if (precision > 0)
    {
      rc = put (context, L".");
      if (rc != 0)
        return rc;

      nb_digits = precision;   // nb digits left to print

      // insert zeroes
      len = min (-exponent-1, nb_digits);
      rc = wwrite_filler (context, put, L'0', len);
      if (rc != 0)
        return rc;
      nb_digits -= len;

      if (nb_digits > 0)  // we want more digits (from mantissa)
      {
        // insert mantissa
        len = min (mantissa'length, nb_digits);
        rc = put (context, mantissa[0:len]);
        if (rc != 0)
          return rc;
        nb_digits -= len;
      }

      // insert zeroes
      rc = wwrite_filler (context, put, L'0', nb_digits);
      if (rc != 0)
        return rc;
    }
  }
  else   // starts with part or all mantissa
  {
    if (exponent+1 <= mantissa'length)    // dot within or just after mantissa
    {
      rc = put (context, mantissa[0 : exponent+1]);
      if (rc != 0)
        return rc;

      if (precision > 0)
      {
        rc = put (context, L".");
        if (rc != 0)
          return rc;

        nb_digits = precision;   // nb digits left to print

        // insert mantissa
        len = min (mantissa'length-(exponent+1), nb_digits);
        rc = put (context, mantissa[exponent+1 : len]);
        if (rc != 0)
          return rc;
        nb_digits -= len;

        // insert zeroes
        rc = wwrite_filler (context, put, L'0', nb_digits);
        if (rc != 0)
          return rc;
      }
    }
    else   // dot after mantissa and zeroes
    {
      rc = put (context, mantissa);
      if (rc != 0)
        return rc;

      // insert zeroes
      nb_digits = (exponent+1) - mantissa'length;
      rc = wwrite_filler (context, put, L'0', nb_digits);
      if (rc != 0)
        return rc;

      rc = put (context, L".");
      if (rc != 0)
        return rc;

      // insert zeroes
      rc = wwrite_filler (context, put, L'0', precision);
      if (rc != 0)
        return rc;
    }
  }

  if (justify_left)
  {
    rc = wwrite_filler (context, put, L' ', filler);
    if (rc != 0)
      return rc;
  }

  return 0;
}

//---------------------------------------------------------------

int wprint_e (WPUT_CONTEXT context,
              WPUT         put,
              double       d,
              int          width,
              int          precision,
              bool         justify_left,
              bool         plus_sign,
              bool         leading_zeroes)
{
  bool        negative;
  wstring(16) mantissa;
  int         exponent, digit;
  int         rc, nb_digits, len, actual_width, filler, i;

  wlow_ftoa (d, out negative, out mantissa, out exponent);

  // precompute width of result string and thus the required filler
  actual_width = 4;
  if (negative || plus_sign)
    actual_width++;
  if (precision > 0)
    actual_width += (1 + precision);
  if (exponent < -9 || exponent > +9)
  {
    actual_width++;
    if (exponent < -99 || exponent > +99)
      actual_width++;
  }
  filler = width - actual_width;

  if ((!justify_left) && (!leading_zeroes))
  {
    rc = wwrite_filler (context, put, L' ', filler);
    if (rc != 0)
      return rc;
  }

  if (negative)
  {
    rc = put (context, L"-");
    if (rc != 0)
      return rc;
  }
  else if (plus_sign)
  {
    rc = put (context, L"+");
    if (rc != 0)
      return rc;
  }

  if ((!justify_left) && leading_zeroes)
  {
    rc = wwrite_filler (context, put, L'0', filler);
    if (rc != 0)
      return rc;
  }

  rc = put (context, mantissa[0:1]);
  if (rc != 0)
    return rc;

  if (precision > 0)
  {
    rc = put (context, L".");
    if (rc != 0)
      return rc;

    nb_digits = precision;   // nb digits left to print

    // insert mantissa
    len = min (mantissa'length-1, nb_digits);
    rc = put (context, mantissa[1 : len]);
    if (rc != 0)
      return rc;
    nb_digits -= len;

    // insert zeroes
    rc = wwrite_filler (context, put, L'0', nb_digits);
    if (rc != 0)
      return rc;
  }

  mantissa[0] = L'e';

  if (exponent < 0)
  {
    exponent = -exponent;
    mantissa[1] = L'-';
  }
  else
  {
    mantissa[1] = L'+';
  }

  i = 2;

  digit = fast_unsigned_int_div_100 (exponent);
  if (digit > 0)
    mantissa[i++] = (wchar)(digit + 48);
  exponent -= digit * 100;

  digit = fast_unsigned_int_div_10 (exponent);
  if (digit > 0)
    mantissa[i++] = (wchar)(digit + 48);
  exponent -= digit * 10;

  mantissa[i++] = (wchar)(exponent + 48);

  rc = put (context, mantissa[0 : i]);
  if (rc != 0)
    return rc;

  if (justify_left)
  {
    rc = wwrite_filler (context, put, L' ', filler);
    if (rc != 0)
      return rc;
  }

  return 0;
}

//---------------------------------------------------------------

// returns 0 if OK, -1 if put failed.

public int wformat_string (WPUT_CONTEXT context, WPUT put, wstring format, object[] arg)
{
  int   start, i, rc, width, precision, arg_count;
  bool  justify_left, plus_sign, leading_zeroes;
  wchar c, typ;

  arg_count = 0;

  start = 0;  // start offset
  i     = 0;  // current index

  while (i < format'length)
  {
    c = format[i];

    if (c == Lnul)
      break;

    i++;

    if (c != L'%')
      continue;

    // flush out everything except the %
    rc = put (context, format[start : i-start-1]);
    if (rc != 0)
      return rc;

    c = format[i++];

    if (c == L'%')   // %%
    {
      start = i-1;
      continue;
    }

    justify_left   = false;
    plus_sign      = false;
    leading_zeroes = false;

    while (c == L'-' | c == L'+' | c == L'0')
    {
      if (c == L'-')
        justify_left = true;
      else if (c == L'+')
        plus_sign = true;
      else if (c == L'0')
        leading_zeroes = true;

      c = format[i++];
    }

    if (c == L'*')
    {
      width'byte = arg[arg_count];   // will crash if argument is not 'int' or not existing
      arg_count++;
      c = format[i++];
    }
    else
    {
      width = 0;
      while (c >= L'0' && c <= L'9')
      {
        width = width * 10 + ((int)c - (int)'0');
        c = format[i++];
      }
    }
    if (width < 0)
      width = 0;

    precision = -1;  // -1 means no precision was specified
    if (c == L'.')
    {
      c = format[i++];

      if (c == L'*')
      {
        precision'byte = arg[arg_count];
        arg_count++;
        c = format[i++];
      }
      else
      {
        precision = 0;
        while (c >= L'0' && c <= L'9')
        {
          precision = precision * 10 + ((int)c - (int)L'0');
          c = format[i++];
        }
      }

      if (precision < 0)
        precision = 0;
    }

    typ = c;    

    switch (typ)
    {
      case L'd':    // any signed integer   decimal number (-61)
      {
        long l;

        switch (arg[arg_count]'length)
        {
          case 1:
          {
            tiny t;
            t'byte = arg[arg_count];
            l = t;
          }
          break;

          case 2:
          {
            short s;
            s'byte = arg[arg_count];
            l = s;
          }
          break;

          case 4:
          {
            int ii;
            ii'byte = arg[arg_count];
            l = ii;
          }
          break;

          default:
          {
            l'byte = arg[arg_count];  // will crash if not 8, that's ok
          }
          break;
        }
        arg_count++;

        // now we have l, let's format it according to :
        // width, justify_left, plus_sign, leading_zeroes.
        {
          wchar str[20];
          int   len, fill;

          len = witoa (l, out str);

          if (plus_sign && l >= 0)
          {
            str[1:len] = str[0:len];
            str[0] = L'+';
            len++;
          }

          fill = width - len;

          if (justify_left)
          {
            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;

            // append trailing blanks
            rc = wwrite_filler (context, put, L' ', fill);
            if (rc != 0)
              return rc;
          }
          else   // justify right
          {
            if (leading_zeroes)
            {
              if (str[0] == L'+' || str[0] == L'-')  // there is a leading sign
              {
                // write leading sign
                rc = put (context, str[0:1]);
                if (rc != 0)
                  return rc;

                // write filling zeroes
                rc = wwrite_filler (context, put, L'0', fill);
                if (rc != 0)
                  return rc;

                // write number except leading sign
                rc = put (context, str[1:len-1]);
                if (rc != 0)
                  return rc;
              }
              else  // no sign
              {
                // write leading zeroes
                rc = wwrite_filler (context, put, L'0', fill);
                if (rc != 0)
                  return rc;

                rc = put (context, str[0:len]);
                if (rc != 0)
                  return rc;
              }
            }
            else  // leading blanks
            {
              // write blanks
              rc = wwrite_filler (context, put, L' ', fill);
              if (rc != 0)
                return rc;

              rc = put (context, str[0:len]);
              if (rc != 0)
                return rc;
            }
          }
        }
      }
      break;

      case L'u':    // any unsigned integer or enum  unsigned number (12)
      {
        long l;

        switch (arg[arg_count]'length)
        {
          case 1:
          {
            byte b;
            b'byte = arg[arg_count];
            l = b;
          }
          break;

          case 2:
          {
            uint2 u;
            u'byte = arg[arg_count];
            l = u;
          }
          break;

          default:
          {
            uint u;
            u'byte = arg[arg_count];  // will crash if not 4, that's ok
            l = u;
          }
          break;
        }
        arg_count++;

        // now we have l, let's format it according to :
        // width, justify_left.
        {
          wchar str[20];
          int   len, fill;

          len = witoa (l, out str);
          fill = width - len;

          if (justify_left)
          {
            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;

            // append trailing blanks
            rc = wwrite_filler (context, put, L' ', fill);
            if (rc != 0)
              return rc;
          }
          else   // justify right
          {
            if (leading_zeroes)
            {
              rc = wwrite_filler (context, put, L'0', fill);
              if (rc != 0)
                return rc;
            }
            else  // leading blanks
            {
              rc = wwrite_filler (context, put, L' ', fill);
              if (rc != 0)
                return rc;
            }

            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;
          }
        }
      }
      break;

      case L'x':    // any integer or enum           hexadecimal number (7fa)
      {
        long l;

        switch (arg[arg_count]'length)
        {
          case 1:
          {
            byte b;
            b'byte = arg[arg_count];
            l = b;
          }
          break;

          case 2:
          {
            ushort s;
            s'byte = arg[arg_count];
            l = s;
          }
          break;

          case 4:
          {
            uint ii;
            ii'byte = arg[arg_count];
            l = ii;
          }
          break;

          default:
          {
            l'byte = arg[arg_count];  // will crash if not 8, that's ok
          }
          break;
        }
        arg_count++;

        // now we have l, let's format it according to :
        // width, justify_left, leading_zeroes.
        {
          wchar str[16];
          int   len, fill;

          len = witoh (l, out str);
          fill = width - len;

          if (justify_left)
          {
            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;

            // append trailing blanks
            rc = wwrite_filler (context, put, L' ', fill);
            if (rc != 0)
              return rc;
          }
          else   // justify right
          {
            if (leading_zeroes)
            {
              rc = wwrite_filler (context, put, L'0', fill);
              if (rc != 0)
                return rc;
            }
            else  // leading blanks
            {
              rc = wwrite_filler (context, put, L' ', fill);
              if (rc != 0)
                return rc;
            }

            rc = put (context, str[0:len]);
            if (rc != 0)
              return rc;
          }
        }
      }
      break;

      case L'e':    // float, double                 floating-point (3.9265e+2)
      case L'f':    // float, double                 floating-point (392.65)
      {
        double d;

        switch (arg[arg_count]'length)
        {
          case 4:
          {
            float f;
            f'byte = arg[arg_count];
            d = f;
          }
          break;

          default:
          {
            d'byte = arg[arg_count];  // will crash if not 8, that's ok
          }
          break;
        }
        arg_count++;

        if (precision == -1)
          precision = 6;

        if (typ == L'e')
        {
          rc = wprint_e (context, put, d, width, precision, justify_left,
                         plus_sign, leading_zeroes);
        }
        else
        {
          rc = wprint_f (context, put, d, width, precision, justify_left,
                         plus_sign, leading_zeroes);
        }

        if (rc != 0)
          return rc;
      }
      break;

      case L'c':    // char or string                all char, or 'precision' char
      case L's':    // string                        stops at nul, or 'precision' char
      {
        int    j, len, fill;
        char* param = (char *)&arg[arg_count];

        len = arg[arg_count]'length;
        if (precision >= 0 && precision < len)
          len = precision;

        if (typ == L's')  // must stop at nul
        {
          for (j=0; j<len; j++)
          {
            if (param[j] == nul)
            {
              len = j;
              break;
            }
          }
        }

        arg_count++;

        // now we have param, let's format it according to :
        // width, justify_left.

        fill = width - len;

        if (justify_left)
        {
          for (j=0; j<len; j++)
          {
            wstring(1) str;
            clear str;
            str[0] = (wchar)(int)param[j];
            rc = put (context, str);
            if (rc != 0)
              return rc;
          }

          // append trailing blanks
          rc = wwrite_filler (context, put, L' ', fill);
          if (rc != 0)
            return rc;
        }
        else   // justify right
        {
          rc = wwrite_filler (context, put, L' ', fill);
          if (rc != 0)
            return rc;

          for (j=0; j<len; j++)
          {
            wstring(1) str;
            clear str;
            str[0] = (wchar)(int)param[j];
            rc = put (context, str);
            if (rc != 0)
              return rc;
          }
        }
      }
      break;

      case L'C':    // wchar or wstring              all wchar, or 'precision' wchar
      case L'S':    // wstring                       stops at Lnul, or 'precision' wchar
      {
        int   j, len, fill;
        wchar* param = (wchar *)&arg[arg_count];

        len = arg[arg_count]'length;
        assert ((len & 1) == 0);   // crash if odd nb of bytes
        len >>= 1;

        if (precision >= 0 && precision < len)
          len = precision;

        if (typ == L'S')  // must stop at nul
        {
          for (j=0; j<len; j++)
          {
            if (param[j] == Lnul)
            {
              len = j;
              break;
            }
          }
        }

        arg_count++;

        // now we have param, let's format it according to :
        // width, justify_left.

        fill = width - len;

        if (justify_left)
        {
          rc = put (context, param[0:len]);
          if (rc != 0)
            return rc;

          // append trailing blanks
          rc = wwrite_filler (context, put, L' ', fill);
          if (rc != 0)
            return rc;
        }
        else   // justify right
        {
          rc = wwrite_filler (context, put, L' ', fill);
          if (rc != 0)
            return rc;

          rc = put (context, param[0:len]);
          if (rc != 0)
            return rc;
        }
      }
      break;

      default:
        return -1;  // bad typ
    }

    start = i;
  }

  return put (context, format[start : i-start]);
}

//---------------------------------------------------------------

public int wscan_string (WGET_CONTEXT context, WGET get, WUNGET unget, wstring format, out object[] arg)
{
  int   i, rc, width, arg_count;
  bool  store_value_in_arg;
  wchar c, s, typ;

  arg_count = 0;

  i = 0;  // current index

  while (i < format'length)
  {
    c = format[i++];

    if (c == Lnul)
      break;

    if (c <= L' ')   // white space
    {
      // skip all white space from stream

      for (;;)
      {
        rc = get (context, out s);        // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc == -1)
          return -1;
        if (rc == -2 || s > L' ')
          break;
      }

      if (rc == -2)   // end of stream -> get next format character
        continue;

      // non white-space
      unget (context);     // cancel read character and continue scanning format string
      continue;
    }

    // non white-space

    if (c != L'%')  // must match with stream
    {
      rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
      if (rc == -1)
        return -1;
      if (rc == -2 || s != c)
        return -4;    // -4 if a syntax error occured in the stream;
      continue;
    }

    // c == L'%'

    c = format[i++];
    if (c == L'%')   // double %
    {
      rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
      if (rc == -1)
        return -1;
      if (rc == -2 || s != L'%')
        return -4;    // -4 if a syntax error occured in the stream;
      continue;
    }

    store_value_in_arg = true;
    if (c == L'*')
    {
      store_value_in_arg = false;
      c = format[i++];
    }


    // read width >= 0, or int'last if no width specified.

    if (c < L'0' || c > L'9')  // no width specified
    {
      width = int'max;  // maximum length
    }
    else
    {
      width = (int)c - 48;
      c = format[i++];

      while (c >= L'0' && c <= L'9' && width < int'max/10 - ((int)c - 48))
      {
        width = width * 10 + ((int)c - 48);
        c = format[i++];
      }
    }

    typ = c;
    switch (typ)
    {
      case L'd':  // any signed integer - decimal number preceded by optional + or - (-61)
      {
        bool negative = false;
        int8 value;
        int  digit, len;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        len = 0;

        if (s == L'-' || s == L'+')
        {
          if (s == L'-')
            negative = true;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc < 0)   // read error or end-of-stream
            return rc;

          len++;   // len becomes 1

          if (width < 2)
            return -4;   // a syntax error occured in the stream
        }

        if (s < L'0' || s > L'9')
          return -4;   // a syntax error occured in the stream

        value = 0;

        for (;;)     // scan value as negative
        {
          len++;

          if (value < -(long'max / 10))
            return -5;   // an overflow occured

          value *= 10;

          digit = (int)s - 48;

          if (value < long'min + digit)
            return -5;   // an overflow occured

          value -= digit;

          if (len == width)    // width limit reached
            break;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)   // read error
            return -1;
          if (rc == -2)   // end of stream
            break;

          if (s < L'0' || s > L'9')
          {
            unget (context);     // cancel read character
            break;
          }
        }

        if (!negative)
          value = -value;

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 1:
            {
              tiny x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (tiny)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 2:
            {
              short x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (short)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 4:
            {
              int x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (int)value;
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              arg[arg_count] = value'byte;   // will crash if arglen is not 8, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;

      case L'u':  // u - any unsigned integer or enum - unsigned number (12)
      {
        uint value;
        int  digit, len;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        if (s < L'0' || s > L'9')
          return -4;   // a syntax error occured in the stream

        len = 0;
        value = 0;

        for (;;)
        {
          len++;

          if (value > uint'max / 10)
            return -5;   // an overflow occured

          value *= 10;

          digit = (int)s - 48;

          if (value > uint'max - (uint)digit)
            return -5;   // an overflow occured

          value += (uint)digit;

          if (len == width)    // width limit reached
            break;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)   // read error
            return -1;
          if (rc == -2)   // end of stream
            break;

          if (s < L'0' || s > L'9')
          {
            unget (context);     // cancel read character
            break;
          }
        }

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 1:
            {
              byte x;
              if (value > x'max)
                return -5;   // an overflow occured 
              x = (byte)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 2:
            {
              ushort x;
              if (value > x'max)
                return -5;   // an overflow occured 
              x = (ushort)value;
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              arg[arg_count] = value'byte;   // will crash if arglen is not 4, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;

      case L'x':  // same as d or u       hexadecimal number (7fa)
      {
        int8 value;
        int  digit, len, non_zero_len;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        if ((s < L'0' || s > L'9') && (s < L'a' || s > L'f') && (s < L'A' || s > L'F'))
          return -4;   // a syntax error occured in the stream

        len = 0;
        non_zero_len = 0;
        value = 0;

        for (;;)
        {
          if (s >= L'0' && s <= L'9')
            digit = (int)s - (int)L'0';
          else if (s >= L'A' && s <= L'F')
            digit = (int)s - ((int)L'A' - 10);
          else
            digit = (int)s - ((int)L'a' - 10);

          value = (value << 4) + digit;

          if (digit > 0 || non_zero_len > 0)
            non_zero_len++;

          len++;
          if (len == width)    // width limit reached
            break;

          if (non_zero_len > 16)   // too long
            return -5;   // an overflow occured

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)   // read error
            return -1;
          if (rc == -2)   // end of stream
            break;

          if ((s < L'0' || s > L'9') && (s < L'a' || s > L'f') && (s < L'A' || s > L'F'))
          {
            unget (context);     // cancel read character
            break;
          }
        }

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 1:
            {
              byte x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (byte)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 2:
            {
              ushort x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (ushort)value;
              arg[arg_count] = x'byte;
            }
            break;

            case 4:
            {
              uint x;
              if (value < x'min || value > x'max)
                return -5;   // an overflow occured 
              x = (uint)value;
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              arg[arg_count] = value'byte;   // will crash if arglen is not 8, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;


      //  [-|+] digit_sequence  "."  digit_sequence [("e"|"E") ["+"|"-"] digit_sequence]

      case L'f':  // f   float or double      floating-point number (0.5)  (12.4E+3)
      case L'e':  // same as f
      {
        bool    negative, dot_found, correction_factor;
        int     len;
        int8    mantissa;
        int     digits_before, digits_after, leading_zeroes_after_dot, m_len;
        int     exponent, exp_sign, k;
        double  fexponent, value;

        assert (width >= 1);

        rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
        if (rc < 0)   // read error or end-of-stream
          return rc;

        len = 0;

        negative = false;
        if (s == L'-' || s == L'+')
        {
          if (s == L'-')
            negative = true;

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc < 0)   // read error or end-of-stream
            return rc;

          len++;   // len becomes 1

          if (width < 2)
            return -4;   // a syntax error occured in the stream
        }


        dot_found = false;
        digits_before = 0;    // nb non-zero digits before dot
        digits_after  = 0;    // nb digits after dot
        leading_zeroes_after_dot = 0;


        if (s != L'.' && (s < L'0' || s > L'9'))
          return -4;   // a syntax error occured in the stream


        // parse leading zeroes

        while (s == L'.' || s == L'0')
        {
          if (s == L'.')
          {
            if (dot_found)
              break;
            dot_found = true;
          }
          else  // L'0'
          {
            if (dot_found)
            {
              digits_after++;
              leading_zeroes_after_dot++;
            }
          }

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)   // end of stream
            break;

          len++;
          if (len == width)
            break;
        }

        // get first 18 digits into mantissa

        mantissa = 0;
        m_len = 0;

        while (len < width && rc == 0 && (s == L'.' || (s >= L'0' && s <= L'9')))
        {
          if (s == L'.')
          {
            if (dot_found)
              break;
            dot_found = true;
          }
          else   // digit
          {
            if (m_len < 18)
            {
              mantissa = mantissa * 10 + ((int)s - (int)L'0');
              m_len++;
            }

            if (dot_found)
              digits_after++;
            else
              digits_before++;
          }

          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)   // end of stream
            break;

          len++;
          if (len == width)
            break;
        }

        exponent = 0;

        if (len < width && rc == 0 && (s == L'e' || s == L'E'))
        {
          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc < 0)
            return rc;

          len++;
          if (len == width)
            return -4;   // a syntax error occured in the stream

          exp_sign = +1;
          if (s == L'+' || s == L'-')
          {
            if (s == L'-')
              exp_sign = -1;

            rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
            if (rc < 0)
              return rc;

            len++;
            if (len == width)
              return -4;   // a syntax error occured in the stream
          }

          if (s < L'0' || s > L'9')
            return -4;   // a syntax error occured in the stream


          while (s >= L'0' && s <= L'9')
          {
            exponent = exponent * 10 + ((int)s - (int)L'0');

            if (exponent >= 512)
              return -5;   // an overflow occured 

            rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
            if (rc == -1)
              return -1;
            if (rc == -2)
              break;

            len++;
            if (len == width)
              break;
          }

          if (exp_sign < 0)
            exponent = -exponent;
        }

        if (rc == 0)
          unget (context);


        // parsing is finished, now compute double result

        exponent += (digits_before - m_len - leading_zeroes_after_dot);

        correction_factor = false;

        if (exponent < 0)
        {
          exponent = -exponent;
          exp_sign = -1;

          if (exponent > 256)   // fexponent might overflow (at 1.7E+308)
          {
            exponent -= 128;
            correction_factor = true;   // correction factor is 1.0E128
          }
        }
        else
        {
          exp_sign = +1;
        }


        fexponent = 1.0;

        if (exponent >= 512)   // prevent table overflow
          return -5;   // an overflow occured 

        {
          const int8 power10[9] = {4621819117588971520,   // 1.0e1
                                   4636737291354636288,   // 1.0e2
                                   4666723172467343360,   // 1.0e4
                                   4726483295884279808,   // 1.0e8
                                   4846369599423283200,   // 1.0e16
                                   5085611494797045271,   // 1.0e32
                                   5564284217833028085,   // 1.0e64
                                   6521906365687930162,   // 1.0e128
                                   8436737289693151036};  // 1.0e256
          k = 0;
          while (exponent > 0)
          {
            if ((exponent & 1) > 0)
            {
              double power10_value;
              power10_value'byte = power10[k]'byte;
              fexponent *= power10_value;
              exponent--;
            }

            exponent >>= 1;
            k++;
          }
        }


        if (exp_sign >= 0)
        {
          value = (double)mantissa * fexponent;
        }
        else
        {
          if (correction_factor)
          {
            value = ((double)mantissa / 1.0E128) / fexponent;
          }
          else
          {
            value = (double)mantissa / fexponent;
          }
        }

        if (negative)
          value = -value;

        if (store_value_in_arg)
        {
          switch (arg[arg_count]'length)
          {
            case 4:
            {
              float x = (float)value;
              if (f_is_infinite (x))
                return -5;   // an overflow occured 
              arg[arg_count] = x'byte;
            }
            break;

            default:
            {
              if (is_infinite (value))
                return -5;   // an overflow occured 
              arg[arg_count] = value'byte;   // will crash if arglen is not 8, that's ok
            }
            break;
          }
          arg_count++;
        }
      }
      break;

      case L'c':  // c - char or string - fill arg, or read max 'width' chars.
      case L's':  // same as c but stop at first white-space.
      {
        int x;

        if (store_value_in_arg)
        {
          clear arg[arg_count];
          if (arg[arg_count]'length < width)
            width = arg[arg_count]'length;
        }

        for (x=0; x<width; x++)
        {
          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)
            break;

          if (typ == L's' && s <= L' ')
          {
            unget (context);     // cancel read character
            break;
          }

          if (store_value_in_arg)
            arg[arg_count][x] = (byte)s;
        }

        if (store_value_in_arg)
          arg_count++;
      }
      break;

      case L'C':  // wchar or wstring - fill arg, or read max 'width' wchars.
      case L'S':  // same as C but stop at first white-space.
      {
        int x;

        if (store_value_in_arg)
        {
          clear arg[arg_count];
          if ((arg[arg_count]'length >> 1) < width)
            width = (arg[arg_count]'length >> 1);
        }

        for (x=0; x<width; x++)
        {
          rc = get (context, out s);   // 0 if OK, -1 if read error, -2 if end-of-stream.
          if (rc == -1)
            return -1;
          if (rc == -2)
            break;

          if (typ == L'S' && s <= L' ')
          {
            unget (context);     // cancel read character
            break;
          }

          if (store_value_in_arg)
          {
            arg[arg_count][x<<1]     = (byte)s;                  // low byte
            arg[arg_count][(x<<1)+1] = (byte)(((uint)s) >> 8);   // high byte
          }
        }

        if (store_value_in_arg)
          arg_count++;
      }
      break;

      default:
        abort;
    }
  }

  if (arg_count < arg'length)
    return -3;      // -3 if end-of-format string occured before all arguments received a value;

  return 0;
}

//---------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------
