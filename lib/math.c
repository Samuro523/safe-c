
// math.c

#if ANDROID
  use android/libmath;
#endif

/***************************************************************************************/

#if WINDOWS
public double fabs (double x)    // absolute value
{
#begin unsafe
#if MEM32
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xE1 };       // fabs
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [ebp+16]
  _asm { 0xD9 0xE1 };       // fabs
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif


#if ANDROID
public double fabs (double x)
{
#begin unsafe
  return libmath.fabs (x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double sqrt (double x)
{
#begin unsafe
#if MEM32
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xFA };       // sqrt
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xFA };       // sqrt
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif


#if ANDROID
public double sqrt (double x)
{
#begin unsafe
  return libmath.sqrt (x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double round (double x)   // round to nearest integer
{
#begin unsafe
#if MEM32
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xFC };       // frndint
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xFC };       // frndint
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif

#if ANDROID
public double round (double x)
{
#begin unsafe
  return libmath.round (x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double sin (double x)
{
#begin unsafe
#if MEM32
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xFE };       // fsin
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xFE };       // fsin
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif

#if ANDROID
public double sin (double x)
{
#begin unsafe
  return libmath.sin (x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double cos (double x)
{
#begin unsafe
#if MEM32
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xFF };       // fcos
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xFF };       // fcos
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif

#if ANDROID
public double cos (double x)
{
#begin unsafe
  return libmath.cos (x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double atan2 (double y, double x)   // arctan(y/x)
{
#begin unsafe
#if MEM32
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [ebp+10]
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xF3 };       // fpatan
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x10 0x00 };  // ret 16
#else
  _asm { 0xDD 0x45 0x18 };  // fld qword ptr [rbp+24]
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xF3 };       // fpatan
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x10 0x00 };  // ret 16
#endif  
#end unsafe
}
#endif

#if ANDROID
public double atan2 (double y, double x)   // arctan(y/x)
{
#begin unsafe
  return libmath.atan2 (y, x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double pi ()
{
#begin unsafe
#if MEM32
  _asm { 0xD9 0xEB }; // fldpi
  _asm { 0xC9 };      // leave
  _asm { 0xC3 };      // ret
#else
  _asm { 0xD9 0xEB }; // fldpi
  _asm { 0xC9 };      // leave
  _asm { 0xC3 };      // ret
#endif  
#end unsafe
}
#endif

#if ANDROID
public double pi ()
{
  return PI;
}
#endif

/***************************************************************************************/

#if 0  // unused at the moment

double a_log2b (double a, double b)   // computes a*log2(b) with b > 0.0
{
#begin unsafe
#if MEM32
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [ebp+10]  a
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]   b
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x10 0x00 };  // ret 16
#else
  _asm { 0xDD 0x45 0x18 };  // fld qword ptr [rbp+24]
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x10 0x00 };  // ret 16
#endif  
#end unsafe
}

#endif

/***************************************************************************************/

#if WINDOWS
public double log2 (double x)   // with x > 0.0
{
#begin unsafe
#if MEM32
  _asm { 0xD9 0xE8 };       // fld1
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xD9 0xE8 };       // fld1
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif

#if ANDROID
public double log2 (double x)
{
#begin unsafe
  return libmath.log2 (x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double log10 (double x)   // with x > 0.0
{
#begin unsafe
#if MEM32
  _asm { 0xD9 0xEC };       // fldlg2   (0.30102999566398119521373889472449)
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xD9 0xEC };       // fldlg2   (0.30102999566398119521373889472449)
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif

#if ANDROID
public double log10 (double x)
{
#begin unsafe
  return libmath.log10 (x);
#end unsafe
}
#endif

/***************************************************************************************/

#if WINDOWS
public double ln (double x)   // with x > 0.0
{
#begin unsafe
#if MEM32
  _asm { 0xD9 0xED };       // fldln2   (0.69314718055994530941723212145818)
  _asm { 0xDD 0x45 0x08 };  // fld qword ptr [ebp+8]
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#else
  _asm { 0xD9 0xED };       // fldln2   (0.69314718055994530941723212145818)
  _asm { 0xDD 0x45 0x10 };  // fld qword ptr [rbp+16]
  _asm { 0xD9 0xF1 };       // blog2a
  _asm { 0xC9 };            // leave
  _asm { 0xC2 0x08 0x00 };  // ret 8
#endif  
#end unsafe
}
#endif

#if ANDROID
public double ln (double x)
{
#begin unsafe
  return libmath.log (x);
#end unsafe
}
#endif

/***************************************************************************************/

public double tan (double x)
{
  return sin(x) / cos(x);
}

/***************************************************************************************/

// 52 bits mantissa, 11 bits exponent, 1 bit sign.
// infinity -> mantissa is zero and exponent is maximum.

public bool isINF (double x)
{
  return ((x'byte[7] & 0x7F) == 0x7F && (x'byte[6] & 0xF0) == 0xF0);
}

/***************************************************************************************/

public double atan (double x)
{
  return atan2 (x, 1.0);
}

/***************************************************************************************/

public double floor (double x)
{
  double r = round (x);

  if (r > x)
    r -= 1.0;

  return r;
}

/***************************************************************************************/

public double ceil (double x)
{
  double r = round (x);

  if (r < x)
    r += 1.0;

  return r;
}

/***************************************************************************************/

public double fmin (double a, double b)
{
  return a < b ? a : b;
}

public double fmax (double a, double b)
{
  return a > b ? a : b;
}

public double fmin3 (double a, double b, double c)
{
  double r;
  r = a < b ? a : b;
  return r < c ? r : c;
}

public double fmax3 (double a, double b, double c)
{
  double r;
  r = a > b ? a : b;
  return r > c ? r : c;
}

/***************************************************************************************/

// exp(x) = 1 + x^1/1! + x^2/2! + x^3/3! + x^4/4! + ...

public double exp (double x)
{
  double r, num, den, i, prev_r;

  if (x < 0.0)
    return 1.0 / exp (-x);
  else
  {
    r = 1.0;
    num = x;
    den = 1.0;

    for (i=2.0; ; i+=1.0)
    {
      prev_r = r;

      r += num/den;

      if (r == prev_r)
        break;

      num *= x;
      den *= i;
    }

    return r;
  }
}

/***************************************************************************************/

// if base == 0, exponent must be >= 0;
// if base < 0, exponent must be an integer value.

public double pow (double base, double exponent)
{
  double r;

  if (exponent == 0.0)
    return 1.0;
  else if (exponent < 0.0)
    return 1.0 / pow (base, -exponent);
  else
  {
    if (base == 0.0)
    {
      assert exponent >= 0.0;
      return 0.0;
    }
    else if (base > 0.0)
      return exp (exponent * ln (base));
    else
    {
      assert exponent == round(exponent);  // make sure exponent is integral
      r = exp (exponent * ln (-base));
      if (((long)exponent & 1L) != 0L)     // exponent odd
        r = -r;
      return r;
    }
  }
}

/***************************************************************************************/

public double asin (double x)
{
  return atan2 (x, sqrt (1.0 - x*x));
}

public double acos (double x)
{
  return -atan2 (x, sqrt(1.0-x*x)) + pi()/2.0;
}

public double sinh (double x)
{
  return (exp(x)-exp(-x))/2.0;
}

public double cosh (double x)
{
  return (exp(x)+exp(-x))/2.0;
}

public double tanh (double x)
{
  return exp(-x)/(exp(x)+exp(-x))*2.0+1.0;
}

public double asinh (double x)
{
  return ln(x+sqrt(x*x+1.0));
}

public double acosh (double x)
{
  return ln(x+sqrt(x*x-1.0));
}

public double atanh (double x)
{
  return ln((1.0+x)/(1.0-x))/2.0;
}

/***************************************************************************************/

// returns (int)floor(log2(f)), for f > 0.0

public int log2i (float f)
{
  #begin unsafe
  return ((*(int2 *)&(f'byte[2])) >> 7) - 127;
  #end unsafe
}

//-----------------------------------------------------------------------------------------------------------------------
