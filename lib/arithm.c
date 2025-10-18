
// arithm.c : simple arithmetic

const int1 log2tab32 [32] =
{ 0,  9,  1, 10, 13, 21,  2, 29, 11, 14, 16, 18, 22, 25,  3, 30,
  8, 12, 20, 28, 15, 17, 24,  7, 19, 27, 23,  6, 26,  5,  4, 31};

  
public int abs (int a)
{
  if (a >= 0)
    return a;
  return -a;
}

public int min (int a, int b)
{
  return a < b ? a : b;
}

public int max (int a, int b)
{
  return a > b ? a : b;
}

public int min3 (int a, int b, int c)
{
  int r;
  r = a < b ? a : b;
  return r < c ? r : c;
}

public int max3 (int a, int b, int c)
{
  int r;
  r = a > b ? a : b;
  return r > c ? r : c;
}

public uint umin (uint a, uint b)
{
  return a < b ? a : b;
}

public uint umax (uint a, uint b)
{
  return a > b ? a : b;
}

public uint umin3 (uint a, uint b, uint c)
{
  uint r;
  r = a < b ? a : b;
  return r < c ? r : c;
}

public uint umax3 (uint a, uint b, uint c)
{
  uint r;
  r = a > b ? a : b;
  return r > c ? r : c;
}


public long labs (long a)
{
  if (a >= 0)
    return a;
  return -a;
}

public long lmin (long a, long b)
{
  return a < b ? a : b;
}

public long lmax (long a, long b)
{
  return a > b ? a : b;
}

public long lmin3 (long a, long b, long c)
{
  long r;
  r = a < b ? a : b;
  return r < c ? r : c;
}

public long lmax3 (long a, long b, long c)
{
  long r;
  r = a > b ? a : b;
  return r > c ? r : c;
}

public uint ulog2 (uint a)
{
  uint value = a;

  assert a > 0;
  
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;

  return (uint)log2tab32[(value*0x07C4ACDD) >> 27];
}

public int ilog2 (int a)
{
  assert a > 0;
  return (int)ulog2 ((uint)a);
}
