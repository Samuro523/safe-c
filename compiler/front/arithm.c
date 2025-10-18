
// arithm.c : arithmetic operations with overflow check.

/***************************************************************************/

// returns 0 if OK or -1 if overflow

public int add_int (INTEGER a, INTEGER b, out INTEGER r)
{
  INTEGER c;

  c = a + b;

  if (((a < 0) ^ (b >= 0)) && ((c < 0) ^ (a < 0)))
  {
    r = 0;
    return -1;
  }

  r = c;
  return 0;
}

/***************************************************************************/

// returns 0 if OK or -1 if overflow

public int sub_int (INTEGER a, INTEGER b, out INTEGER r)
{
  INTEGER c;

  c = a - b;

  if (((a < 0) ^ (b < 0)) && ((c < 0) ^ (a < 0)))
  {
    r = 0;
    return -1;
  }

  r = c;
  return 0;
}

/***************************************************************************/

// returns 0 if OK or -1 if overflow

public int mul_int (INTEGER a, INTEGER b, out INTEGER r)
{
  INTEGER c;

  c = a * b;

  if (b != 0 && (c / b) != a)
  {
    r = 0;
    return -1;
  }

  r = c;
  return 0;
}

/***************************************************************************/

// returns 0 if OK or -1 if overflow

public int div_int (INTEGER a, INTEGER b, out INTEGER r)
{
  if ((b == 0) || (a == MIN_INTEGER && b == -1))
  {
    r = 0;
    return -1;
  }

  r = a / b;
  return 0;
}

/***************************************************************************/

public int mod_int (INTEGER a, INTEGER b, out INTEGER r)
{
  if (b == 0)
  {
    r = 0;
    return -1;
  }

  r = a % b;
  return 0;
}

/***************************************************************************/

public int neg_int (INTEGER a, out INTEGER r)
{
  if (a == MIN_INTEGER)
  {
    r = 0;
    return -1;
  }

  r = -a;
  return 0;
}

/***************************************************************************/


#if 0

#include <stdio.h>

main()
{
  int x, y, z, ov1, ov2, op;
  INTEGER xx, yy, zz;

  for (op=0; op<6; op++)
  {
    for (x=MIN_INTEGER; x<=MAX_INTEGER; x++)
    {
      for (y=MIN_INTEGER; y<=MAX_INTEGER; y++)
      {
        xx = x;
        yy = y;

        if (y == 0 && (op==3||op==4))
          ov1 = -1;
        else
        {
          if (op==0)
            z = x + y;
          else if (op==1)
            z = x - y;
          else if (op==2)
            z = x * y;
          else if (op==3)
            z = x / y;
          else if (op==4)
            z = x % y;
          else if (op==5)
            z = -x;

          ov1 = -(z < MIN_INTEGER || z > MAX_INTEGER);
        }

        if (op==0)
          ov2 = add_int (xx, yy, &zz);
        else if (op==1)
          ov2 = sub_int (xx, yy, &zz);
        else if (op==2)
          ov2 = mul_int (xx, yy, &zz);
        else if (op==3)
          ov2 = div_int (xx, yy, &zz);
        else if (op==4)
          ov2 = mod_int (xx, yy, &zz);
        else if (op==5)
          ov2 = neg_int (xx, &zz);

        if (ov1 != ov2)
          printf ("error op=%d %d %d %d\n", op, xx, yy, zz);

        if ((!ov1) && z != zz)
          printf ("mismatch op=%d %d %d %d\n", op, xx, yy, zz);
      }
    }
  }

  printf ("ok\n");
  return 0;
}

#endif
