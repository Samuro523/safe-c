
// integer.c : large unsigned integers

#define debug 0

use arithm, strings;

#if debug
  use console, exception;
#endif

//===================================================================================

// warning:
// the following two structures must be "castable" one to the other,
// so the fields must start at the same offsets.

struct INTEGER
{
  uint               count;    // range 0 .. INTEGER_SIZE
  byte[INTEGER_SIZE] b;        // least significant to most significant
}

struct DOUBLE_INTEGER
{
  uint                 count;  // range 0 .. 2*INTEGER_SIZE
  byte[2*INTEGER_SIZE] b;
}

//===================================================================================

public void make_integer (uint value, out INTEGER result)
{
#begin unsafe
  uint     i;
  uint     n = value;
  INTEGER* r = &result;
  byte*    b = &result.b;

  assert value'size <= INTEGER_SIZE;

  for (i=0; n>0; i++)
  {
    b[i] = (byte)n;
    n >>= 8;
  }

  r->count = i;
#end unsafe
}

//===================================================================================

void make_large_double_integer (byte[] value, out DOUBLE_INTEGER result)
{
#begin unsafe
  DOUBLE_INTEGER* r = &result;
  byte*           b = &result.b;
  int             i = value'length;

  assert value'size <= 2*INTEGER_SIZE;

  while (i > 0 && value[i-1] == 0)
    i--;
  b[0:i] = value[0:i];
  r->count = (uint)i;
#end unsafe
}

/*************************************************************************/

/* build an integer from a byte sequence (lowest index = lower byte) */

public void make_large_integer (byte[] value, out INTEGER result)
{
  assert value'size <= INTEGER_SIZE;
#begin unsafe
  make_large_double_integer (value, out *(DOUBLE_INTEGER*)&result);
#end unsafe
}

/*************************************************************************/

// retrieve an integer (the function aborts if a > uint'max)

public uint integer_value (INTEGER a)
{
#begin unsafe
  byte*  b = &a.b;
  uint   val, i;

  assert a.count <= INTEGER_SIZE;
  assert a.count <= val'size;

  val = 0;
  for (i=0; i<a.count; i++)
    val |= (b[i] << (i<<3));

  return val;
#end unsafe
}

//===================================================================================

public void copy_integer (INTEGER source, out INTEGER target)
{
#begin unsafe
  INTEGER* p = &target;
  assert source.count <= INTEGER_SIZE;
  p->count = source.count;
  p->b[0:source.count] = source.b[0:source.count];
#end unsafe
}

//===================================================================================

int compare_double_integer (DOUBLE_INTEGER a, DOUBLE_INTEGER b)
{
#begin unsafe
  int    i;
  byte*  pa = &a.b;
  byte*  pb = &b.b;

  assert (a.count <= 2*INTEGER_SIZE && b.count <= 2*INTEGER_SIZE);

  if (a.count < b.count)   // smaller
    return -1;

  if (a.count > b.count)   // larger
    return +1;

  // both mantissa have the same length : let's compare their values
  for (i=(int)a.count-1; i>=0; i--)
  {
    if (pa[i] < pb[i])    // smaller
      return -1;
    if (pa[i] > pb[i])    // larger
      return +1;
  }

  return 0;   // equal
#end unsafe
}

//===================================================================================

public int compare_integer (INTEGER a, INTEGER b)
{
  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE);
#begin unsafe
  return compare_double_integer (*(DOUBLE_INTEGER*)&a, *(DOUBLE_INTEGER*)&b);
#end unsafe
}

//===================================================================================

public int compare_integer_int (INTEGER a, uint b)
{
#begin unsafe
  int    i;
  uint   val;
  byte*  pa = &a.b;

  assert a.count <= INTEGER_SIZE;

  if (a.count > val'size)   // 'a' is larger than any 'val'
    return +1;

  val = 0;
  for (i=0; i<(int)a.count; i++)
    val |= (pa[i] << ((uint)(i<<3)));

  if (val < b)   // smaller
    return -1;

  if (val > b)   // larger
    return +1;

  return 0;      // equal
#end unsafe
}

//===================================================================================

int add_double_integer (DOUBLE_INTEGER a, DOUBLE_INTEGER b, out DOUBLE_INTEGER result, uint result_size)
{
#begin unsafe
  uint            i, min, max;
  int             sum;
  DOUBLE_INTEGER* p = &result;  // suppresses warning
  byte*           pa = &a.b;
  byte*           pb = &b.b;
  byte*           pc = &result.b;

  _unused p;


  /* compute min/max nb of bytes in a and b */

  if (a.count < b.count)
  {
    min = a.count;
    max = b.count;
  }
  else
  {
    min = b.count;
    max = a.count;
  }

  assert max <= 2*INTEGER_SIZE && max <= result_size && result_size <= 2*INTEGER_SIZE;


  /* fast computation of common mantissa [0 .. min[ */

  sum = 0;
  for (i=0; i<min; i++)
  {
    sum += ((int)pa[i] + (int)pb[i]);
    pc[i] = (byte)sum;
    sum >>= 8;
  }


  /* compute remaining bytes in a or b mantissa [min..max[ */

  if (a.count < b.count)         /* some bytes left in b */
  {
    for (i=min; i<max; i++)
    {
      sum += (int)pb[i];
      pc[i] = (byte)sum;
      sum >>= 8;
    }
  }
  else if (a.count > b.count)    /* some bytes left in a */
  {
    for (i=min; i<max; i++)
    {
      sum += (int)pa[i];
      pc[i] = (byte)sum;
      sum >>= 8;
    }
  }


  /* handle the carry */

  if (sum != 0)    // we must use one extra byte
  {
    if (max == result_size)    // overflow (mantissa is full)
      return -1;
    pc[max++] = 1;
  }


  /* set count */

  result.count = max;

  return 0;
#end unsafe
}

//===================================================================================

public int add_integer (INTEGER a, INTEGER b, out INTEGER result)
{
  assert a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE;
#begin unsafe
  return add_double_integer (
      *(DOUBLE_INTEGER*)&a, *(DOUBLE_INTEGER*)&b, out *(DOUBLE_INTEGER*)&result, result.b'size);
#end unsafe
}

//===================================================================================

int subtract_double_integer (DOUBLE_INTEGER a, DOUBLE_INTEGER b, out DOUBLE_INTEGER result)
{
#begin unsafe
  uint            i, min, max;
  int             diff;
  DOUBLE_INTEGER* p = &result;  // suppresses warning
  byte*           pa = &a.b;
  byte*           pb = &b.b;
  byte*           pc = &result.b;

  _unused  p;

  /* compute min/max nb of bytes in a and b */

  if (a.count < b.count)
  {
    min = a.count;
    max = b.count;
  }
  else
  {
    min = b.count;
    max = a.count;
  }

  assert max <= 2*INTEGER_SIZE;


  /* fast computation of common mantissa [0 .. min[ */

  diff = 0;
  for (i=0; i<min; i++)
  {
    diff += ((int)pa[i] - (int)pb[i]);
    pc[i] = (byte)diff;
    diff >>= 8;
  }


  /* compute remaining bytes in a or b mantissa [min..max[ */

  if (a.count < b.count)         /* some bytes left in b */
  {
    for (i=min; i<max; i++)
    {
      diff -= (int)pb[i];
      pc[i] = (byte)diff;
      diff >>= 8;
    }
  }
  else if (a.count > b.count)    /* some bytes left in a */
  {
    for (i=min; i<max; i++)
    {
      diff += (int)pa[i];
      pc[i] = (byte)diff;
      diff >>= 8;
    }
  }


  /* handle the borrow */

  if (diff != 0)      // underflow
    return -1;


  /* set count */

  while (max > 0 && pc[max-1] == 0)
    max--;
  result.count = max;

  return 0;
#end unsafe
}

//===================================================================================

public int subtract_integer (INTEGER a, INTEGER b, out INTEGER result)
{
  assert a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE;
#begin unsafe
  return subtract_double_integer (
      *(DOUBLE_INTEGER*)&a, *(DOUBLE_INTEGER*)&b, out *(DOUBLE_INTEGER*)&result);
#end unsafe
}

//===================================================================================

// warning: a, b must not denote same variable as result

void basic_multiply_integer
             (    INTEGER        a,
                  INTEGER        b,
              out DOUBLE_INTEGER result)
{
#begin unsafe
  uint            c_count, i, j;
  uint            factor, sum;
  DOUBLE_INTEGER* p = &result;   // suppresses warning
  byte*           pc = &result.b;
  _unused p;

  assert a.count > 0                // zero not allowed (would set .count badly)
      && b.count > 0
      && a.count <= INTEGER_SIZE
      && b.count <= INTEGER_SIZE;

  c_count = a.count + b.count;     // max size of product

  clear pc[0:c_count];

  for (i=0; i<a.count; i++)
  {
    factor = a.b[i];

    // add (b.b[] * factor) to result
    sum = 0;
    for (j=0; j<b.count; j++)
    {
      sum += (b.b[j] * factor + pc[i+j]);
      pc[i+j] = (byte)sum;
      sum >>= 8;
    }

    pc[i+j] += (byte)sum;
  }


  // compute size of result : either c_count or c_count-1

  if (pc[c_count-1] == 0)    // MSB byte is zero
    c_count--;

  result.count = c_count;
#end unsafe
}

//===================================================================================

// warning: a, b must not denote same variable as result
void multiply_to_double_integer (    INTEGER a,
                                     INTEGER b,
                                 out DOUBLE_INTEGER result);

//===================================================================================

// assertion: a.count >= 50 && b.count >= 50
// warning: a, b must not denote same variable as result

void karatsuba_multiply_integer
             (    INTEGER        a,
                  INTEGER        b,
              out DOUBLE_INTEGER result)
{
#begin unsafe
  uint            i;
  DOUBLE_INTEGER  t1;
  DOUBLE_INTEGER  t2;
  DOUBLE_INTEGER* p = &result;   // suppresses warning
  _unused p;

  assert a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE;

  i = umin (a.count, b.count) >> 1;  // between 25 and 512

  // t1 = low a * low b
  {
    INTEGER x, y;
    make_large_integer (a.b[0 : i], out x);
    make_large_integer (b.b[0 : i], out y);
    multiply_to_double_integer (x, y, out t1);
  }

  // t2 = high a * high b
  {
    INTEGER x, y;
    make_large_integer (a.b[i : a.count - i], out x);
    make_large_integer (b.b[i : b.count - i], out y);
    multiply_to_double_integer  (x, y, out t2);
  }

  // result = (low a + high a) * (low b + high b) - t1 - t2
  {
    INTEGER x, y, z;

    make_large_integer (a.b[0 : i], out x);
    make_large_integer (a.b[i : a.count - i], out y);
    assert add_integer (x, y, out x) == 0;

    make_large_integer (b.b[0 : i], out y);
    make_large_integer (b.b[i : b.count - i], out z);
    assert add_integer (y, z, out y) == 0;

    multiply_to_double_integer (x, y, out result);

    assert subtract_double_integer (result, t1, out result) == 0;
    assert subtract_double_integer (result, t2, out result) == 0;
  }

  // result = t1 + (result << i) + (t2 << (2*i))

  // shift result left by i
  if (result.count > 0)
  {
    result.b[i : result.count] = result.b[0 : result.count];
    result.b[0 : i] = {all => 0};
    result.count += i;
  }

  // add t1
  assert add_double_integer (result, t1, out result, result.b'size) == 0;

  // add t2 shifted left by 2*i
  if (t2.count > 0)
  {
    DOUBLE_INTEGER  di;
    DOUBLE_INTEGER* q = &di;       // suppresses warning
    _unused q;

    di.count = 2*i + t2.count;
    di.b[0 : 2*i] = {all => 0};
    di.b[2*i : t2.count] = t2.b[0 : t2.count];
    assert add_double_integer (di, result, out result, result.b'size) == 0;
  }

#end unsafe
}

//===================================================================================

// warning: a, b must not denote same variable as result
void multiply_to_double_integer (    INTEGER a,
                                     INTEGER b,
                                 out DOUBLE_INTEGER result)
{
  assert a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE;

  if (a.count == 0 || b.count == 0)    // 'a' or 'b' equals zero
  {
    #begin unsafe
      DOUBLE_INTEGER* p = &result;   // suppresses warning
      _unused p;
    #end unsafe

    result.count = 0;   // result is zero
    return;
  }

  if (a.count >= 50 && b.count >= 50)
  {
    karatsuba_multiply_integer (a, b, out result);
  }
  else
  {
    basic_multiply_integer (a, b, out result);
  }
}

//===================================================================================

public int multiply_integer (INTEGER a, INTEGER b, out INTEGER result)
{
#begin unsafe
  DOUBLE_INTEGER c;
  INTEGER* p =  &result;  // suppresses warning
  _unused p;

  multiply_to_double_integer (a, b, out *(DOUBLE_INTEGER*)&c);

  if (c.count > INTEGER_SIZE)      // overflow
    return -1;

  result.count        = c.count;
  result.b[0:c.count] = c.b[0:c.count];

  return 0;
#end unsafe
}

//===================================================================================

uint double_integer_bit_size (DOUBLE_INTEGER a)
{
  uint r;
  int  power;

  assert (a.count <= 2*INTEGER_SIZE);

  r = a.count * 8;

  if (r != 0)
  {
    for (power=128; power>0; power>>=1)
    {
      if ((a.b[a.count-1] & (uint)power) == 0)
        r--;
      else
        break;
    }
  }

  return r;
}

//===================================================================================

public uint integer_bit_size (INTEGER a)
{
  assert (a.count <= INTEGER_SIZE);
#begin unsafe
  return double_integer_bit_size (*(DOUBLE_INTEGER*)&a);
#end unsafe
}

//===================================================================================

#begin unsafe

// note: 'quotient' and 'remainder' can be null

int divide_double_integer (DOUBLE_INTEGER  a,        INTEGER  b,
                           DOUBLE_INTEGER* quotient, INTEGER* remainder)
{
  DOUBLE_INTEGER  dividend, factor, garbage;
  INTEGER         qb;
  uint            num, den, q;
  byte            quot[2*INTEGER_SIZE];
  int             i;
  DOUBLE_INTEGER* dummy = &dividend;
  byte*           dummy2 = &quot;
  _unused dummy;
  _unused dummy2;
  
  assert (a.count <= 2*INTEGER_SIZE && b.count <= INTEGER_SIZE);

  if (b.count == 0)     // b is zero : division by zero
    return -1;

  if (a.count == 0)     // a is zero -> q = 0, r = 0
  {
    if (quotient != null)
      quotient->count = 0;
    if (remainder != null)
      remainder->count = 0;
    return 0;
  }

  clear num, den, q;
  dividend.count = 0;
  
  den'byte[3] = 0;
  den'byte[2] = b.b[b.count-1];
  den'byte[1] = (byte)((b.count >= 2) ? b.b[b.count-2] : 0);
  den'byte[0] = (byte)((b.count >= 3) ? b.b[b.count-3] : 0);

  for (i=(int)a.count-1; i>=0; i--)
  {
    // shift dividend left and introduce a new digit
    dividend.b[1:dividend.count] = dividend.b[0:dividend.count];
    dividend.b[0] = a.b[i];
    if (dividend.b[dividend.count] != 0)
      dividend.count++;

    if (dividend.count < b.count)  // dividend smaller than b
    {
      q = 0;
    }
    else  // dividend.count >= b.count
    {
      num'byte[3] = (byte)((b.count < dividend.count) ? dividend.b'byte[b.count] : 0);
      num'byte[2] = (byte)dividend.b'byte[b.count-1];
      num'byte[1] = (byte)((b.count >= 2) ? dividend.b'byte[b.count-2] : (i > 0 ? a.b[i-1] : 0));
      num'byte[0] = 0xFF;

      q = num / den;
      
      if (q == 256)
        q = 255;

      assert q <= 255;
    }

    if (q > 0)
    {
      // factor = q x b
      make_large_integer ({(byte)q}, out qb);
      multiply_to_double_integer (qb, b, out factor);

      // test if dividend - factor underflows
      if (subtract_double_integer (dividend, factor, out garbage) != 0)  // underflow (q was too large)
      {
        _unused garbage;

        q--;
        assert q <= 255;

        // factor -= b;
        assert subtract_double_integer (factor, *(DOUBLE_INTEGER*)&b, out factor) == 0;
      }

      // dividend -= factor
      assert subtract_double_integer (dividend, factor, out dividend) == 0;
    }
    
    quot[i] = (byte)q;
  }

  if (quotient != null)
    make_large_double_integer (quot[0:a.count], out *quotient);

  if (remainder != null)
    copy_integer (*(INTEGER*)&dividend, out *remainder);

  return 0;
}

//===================================================================================

public int divide_integer (INTEGER a, INTEGER b, out INTEGER result)
{
  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE);

  return divide_double_integer (*((DOUBLE_INTEGER *)&a), b,
                                (DOUBLE_INTEGER *)&result, null);
}

//===================================================================================

public int modulo_integer (INTEGER a, INTEGER b, out INTEGER result)
{
  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE);

  return divide_double_integer (*((DOUBLE_INTEGER *)&a), b,
                                null, &result);
}

//===================================================================================

/* note: 'quotient' or 'remainder' can be set to NULL */

public int divide_modulo_integer (INTEGER a, INTEGER b, out INTEGER quotient, out INTEGER remainder)
{
  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE);

  return divide_double_integer (*((DOUBLE_INTEGER *)&a), b,
                                (DOUBLE_INTEGER *)&quotient, &remainder);
}

//===================================================================================

public int multiply_modulo_integer (INTEGER a, INTEGER b, INTEGER n, out INTEGER result)
{
  DOUBLE_INTEGER c;
  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE && n.count <= INTEGER_SIZE);
  multiply_to_double_integer (a, b, out c);
  return divide_double_integer (c, n, null, &result);
}

//===================================================================================

public int exponent_modulo_integer (INTEGER a, INTEGER b, INTEGER n, out INTEGER result)
{
  INTEGER        r, exp, base;
  DOUBLE_INTEGER c;
  uint           j;

  INTEGER* p;
  p =  &r;      // suppresses warning
  p =  &exp;    // suppresses warning
  p =  &base;   // suppresses warning
  p =  &result; // suppresses warning
  _unused p;

  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE && n.count <= INTEGER_SIZE);

  /* r = 1 */
  r.count = 1;
  r.b[0] = 1;

  /* exp = b */
  exp.count = b.count;
  exp.b[0:b.count] = b.b[0:b.count];

  /* base = a */
  base.count = a.count;
  base.b[0:base.count] = a.b[0:base.count];

  while (exp.count != 0)
  {
    if ((exp.b[0] & 1) != 0)    /* 'exp' is odd */
    {
      /* r = (r * base) mod n */
      multiply_to_double_integer (r, base, out c);
      if (divide_double_integer (c, n, null, &r) < 0)
        return -1;

      /* decrement exponent (we know that the exponent is odd) */
      exp.b[0]--;
      if (exp.b[0] == 0 && exp.count == 1)
        exp.count = 0;
    }
    else     /* 'exp' is even */
    {
      /* base = (base * base) mod n */
      multiply_to_double_integer (base, base, out c);
      if (divide_double_integer (c, n, null, &base) < 0)
        return -1;

      /* divide exponent by 2 (we know that the exponent is not zero) */
      for (j=0; j<exp.count-1; j++)
      {
        exp.b[j] >>= 1;
        if ((exp.b[j+1] & 1) != 0)
          exp.b[j] |= 128;
      }
      exp.b[j] >>= 1;
      if (exp.b[j] == 0)
        exp.count--;
    }
  }

  /* result = r */
  result.count = r.count;
  result.b[0:r.count] = r.b[0:r.count];

  return 0;
}

#end unsafe

//===================================================================================

public void integer_to_string (INTEGER a, out string buffer)
{
  int     i, len;
  INTEGER n, digit, ten;

#begin unsafe
  char* p;
  p = &buffer;      // suppresses warning
  _unused p;
#end unsafe

  assert (a.count <= INTEGER_SIZE);

  if (a.count == 0)
  {
    strcpy (out buffer, "0");
    return;
  }

  i = (int)buffer'size;

  copy_integer (a, out n);
  make_integer (10, out ten);

  while (n.count != 0)
  {
    assert (divide_modulo_integer (n, ten, out n, out digit) == 0);
    buffer[--i] = (char)((uint)'0' + integer_value (digit));
  }

  len = (int)buffer'size - i;
  buffer[0:len] = buffer[i:len];
  clear buffer[len:(int)buffer'size-len];
}

//===================================================================================

public void integer_to_hex_string (INTEGER a, out string buffer)
{
  const string hex_digit = "0123456789ABCDEF";

  uint i, by;

  assert (a.count <= INTEGER_SIZE);

  if (a.count == 0)
  {
    strcpy (out buffer, "00");
    return;
  }

  clear buffer;

  for (i=0; i<a.count; i++)
  {
    by = a.b[a.count-1-i];
    buffer[i*2]   = hex_digit[by >> 4];
    buffer[i*2+1] = hex_digit[by & 15];
  }
}

//===================================================================================

/* returns 0 if OK, -1 in case of overflow, -2 in case of syntax error */

public int string_to_integer (string str, out INTEGER result)
{
  INTEGER ten, digit;
  int     i;

  make_integer (10, out ten);
  make_integer (0, out result);

  i = 0;

  while (i < str'length && str[i] == ' ')
    i++;

  while (i < str'length && str[i] >= '0' && str[i] <= '9')
  {
    /* multiply result by 10 and add the new digit */
    if (multiply_integer (result, ten, out result) < 0)
      return -1;

    make_integer ((uint)str[i] - (uint)'0', out digit);

    if (add_integer (result, digit, out result) < 0)
      return -1;

    i++;
  }

  while (i < str'length && str[i] == ' ')
    i++;

  if (i == str'length || str[i] == nul)
    return 0;

  return -2;
}

//===================================================================================

public int hex_string_to_integer (string str, out INTEGER result)
{
  INTEGER sixteen, digit;
  int     i, n;
  char    c;

  make_integer (16, out sixteen);
  make_integer (0,  out result);

  i = 0;

  while (i < str'length && str[i] == ' ')
    i++;

  while (i < str'length)
  {
    c = str[i];
    if (c >= '0' && c <= '9')
      n = (int)((uint)c - (uint)'0');
    else if (c >= 'A' && c <= 'F')
      n = 10 + (int)((uint)c - (uint)'A');
    else if (c >= 'a' && c <= 'f')
      n = 10 + (int)((uint)c - (uint)'a');
    else
      break;

    /* multiply result by 16 and add the new digit */
    if (multiply_integer (result, sixteen, out result) < 0)
      return -1;

    make_integer ((uint)n, out digit);

    if (add_integer (result, digit, out result) < 0)
      return -1;

    i++;
  }

  while (i < str'length && str[i] == ' ')
    i++;

  if (i == str'length || str[i] == nul)
    return 0;

  return -2;
}

//===================================================================================

public uint integer_byte_size (INTEGER a)
{
  assert (a.count <= INTEGER_SIZE);
  return a.count;
}

//===================================================================================

public bool integer_is_valid (INTEGER a)
{
  if (a.count > INTEGER_SIZE)
    return false;

  if (a.count > 0)
  {
    if (a.b[a.count-1] == 0)
      return false;
  }

  return true;
}

//===================================================================================

public void check_integer (INTEGER a)
{
  assert integer_is_valid (a);
}

//===================================================================================

/* retrieve lower bytes of integer sequence */

public void extract_integer (INTEGER a, out byte[] value)
{
  uint len = umin (a.count, value'size);
  assert (a.count <= INTEGER_SIZE);
  clear value;
  value[0:len] = a.b[0:len];
}

//===================================================================================

/* computes the largest common divisor of 'a' and 'b'    */
/* 'a' and 'b' must be >= 1, otherwise the program stops */

public void pgcd (INTEGER a, INTEGER b, out INTEGER result)
{
  INTEGER n1, n2, temp;

  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE);

  copy_integer (a, out n1);
  copy_integer (b, out n2);

  assert (n1.count > 0 && n2.count > 0);

  while (n2.count != 0)
  {
    copy_integer (n1, out temp);
    copy_integer (n2, out n1);

    assert modulo_integer (temp, n2, out n2) == 0;
  }

  copy_integer (n1, out result);
}

//===================================================================================

/* computes the smallest common multiple of 'a' and 'b'   */
/* 'a' and 'b' must be >= 1, otherwise the program stops. */
/* returns 0 if OK, -1 in case of overflow.               */

public int ppcm (INTEGER a, INTEGER b, out INTEGER result)
{
  INTEGER temp;

  assert (a.count <= INTEGER_SIZE && b.count <= INTEGER_SIZE);

  pgcd (a, b, out temp);
  assert divide_integer (a, temp, out temp) == 0;   /* no error possible */

  if (multiply_integer (temp, b, out result) < 0)
    return -1;

  return 0;
}

//===================================================================================

/* given the equation: (a * x) mod n = 1  */
/* with  0 < a < n  and  pgcd (a, n) = 1, */
/* the function computes x.               */
/* returns 0 if OK, -1 if the constraints */
/* on 'a' and 'n' are violated.           */

public int inverse_multiply_modulo_integer (INTEGER a, INTEGER n, out INTEGER x)
{
  INTEGER g0, g1, v0, v1, r, temp;

#begin unsafe
  INTEGER* p = &x;    // suppresses warning
  _unused p;
#end unsafe

  assert (a.count <= INTEGER_SIZE && n.count <= INTEGER_SIZE);

  /* check constraints on parameters */

  if (a.count == 0)      /* 'a' equals zero */
    return -1;

  if (compare_integer (a, n) >= 0)   /* 'a' >= 'n' */
    return -1;

  pgcd (a, n, out r);
  if (compare_integer_int (r, 1) != 0)   /* r != 1 */
    return -1;


  copy_integer (n, out g0);
  copy_integer (a, out g1);

  make_integer (0, out v0);
  make_integer (1, out v1);

  for (;;)
  {
    if (g1.count == 0)
    {
      assert subtract_integer (n, v0, out r) == 0;
      break;
    }

    assert divide_modulo_integer (g0, g1, out temp, out g0) == 0;
    assert multiply_integer (temp, v1, out temp) == 0;
    assert add_integer (temp, v0, out v0) == 0;

    if (g0.count == 0)
    {
      copy_integer (v1, out r);
      break;
    }

    assert divide_modulo_integer (g1, g0, out temp, out g1) == 0;
    assert multiply_integer (temp, v0, out temp) == 0;
    assert add_integer (temp, v1, out v1) == 0;
  }

  copy_integer (r, out x);
  return 0;
}

//===================================================================================
