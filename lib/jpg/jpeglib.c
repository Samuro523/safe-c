

public int RIGHT_SHIFT (int x, int shft)
{
  return ((x) >> (shft));
}


#begin unsafe

public uint MIN (uint a, uint b)
{
  return ((a) < (b) ? (a) : (b));
}

public uint MAX (uint a, uint b)
{
  return ((a) > (b) ? (a) : (b));
}

public int iMIN (int a, int b)
{
  return ((a) < (b) ? (a) : (b));
}

public int iMAX (int a, int b)
{
  return ((a) > (b) ? (a) : (b));
}

#end unsafe
