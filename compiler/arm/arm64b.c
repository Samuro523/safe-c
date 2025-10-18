
// arm64b.c

//-----------------------------------------------------------------

/// Return true if the argument is a non-empty sequence of ones starting at the
/// least significant bit with the remainder zero (64 bit version).
/// 1, 3, 7, 15, 31, ..
bool isMask_64 (int8 Value)
{
  return Value != 0 && ((Value + 1) & Value) == 0;
}

/// Return true if the argument contains a non-empty sequence of ones with the
/// remainder zero (64 bit version.)
bool isShiftedMask_64 (int8 Value)
{
  return Value != 0 && isMask_64((Value - 1) | Value);
}

uint countr_one (long value)
{
  int8 n = value;
  int  i;
  uint count_one = 0;
  for (i=0; i<64; i++)
  {
    if ((n & 1) != 1)
      break;
    count_one++;
    n >>= 1;
  }
  return count_one;
}

uint countr_zero (long value)
{
  int8 n = value;
  int  i;
  uint count_zero = 0;
  for (i=0; i<64; i++)
  {
    if ((n & 1) != 0)
      break;
    count_zero++;
    n >>= 1;
  }
  return count_zero;
}

uint countl_one (long value)
{
  int8 n = value;
  int  i;
  uint count_one = 0;
  for (i=0; i<64; i++)
  {
    if (n >= 0)   // positive sign
      break;
    count_one++;
    n <<= 1;
  }
  return count_one;
}


// unsigned shift right
// note that shift right is different for signed & unsigned

int8 shr (int8 r, uint shifts)
{
  int8 n;

  n = r;
  n'byte[7] &= 127;  // clear sign bit
  n >>= shifts;

  if (r < 0 && shifts < 64)
   n |= (1L << (63-shifts));

  return n;
}

//-----------------------------------------------------------------

/// processLogicalImmediate - Determine if an immediate value can be encoded
/// as the immediate operand of a logical instruction for the given register
/// size.  If so, return true with "encoding" set to the encoded value in
/// the form N:immr:imms (13 bit)

public
bool processLogicalImmediate (    int8 Im,
                                  uint RegSize,   // 32 or 64
                              out uint Encoding)
{
  int8 Imm = Im;
  uint Size;

  if (Imm == 0L || Imm == -1L ||  // all zeroes or all ones
      (RegSize != 64 &&           // for less than 64 bit
        (shr(Imm, RegSize) != 0 ||                  // value too large
               Imm == shr(-1L, (64 - RegSize)))))   // all ones
  {
    clear Encoding;
    return false;
  }

  // First, determine the element size.
  Size = (uint)RegSize;

  for (;;)
  {
    int8 Mask;

    Size >>= 1;
    Mask = (1L << Size) - 1;

    if ((Imm & Mask) != (shr(Imm, Size) & Mask))
    {
      Size *= 2;
      break;
    }

    if (Size <= 2)
      break;
  }

  // Second, determine the rotation to make the element be: 0^m 1^n.
  {
    uint CTO, CLO, I, Immr, NImms, N;
    int8 Mask;

    Mask = shr (-1L, (64 - Size));
    Imm &= Mask;

    if (isShiftedMask_64(Imm))
    {
      I = countr_zero(Imm);
      assert I < 64;
      CTO = countr_one(shr(Imm, I));
    }
    else
    {
      Imm |= ~Mask;
      if (!isShiftedMask_64(~Imm))
      {
        clear Encoding;
        return false;
      }

      CLO = countl_one(Imm);
      I = 64 - CLO;
      CTO = CLO + countr_one(Imm) - (64 - Size);
    }

    // Encode in Immr the number of RORs it would take to get *from* 0^m 1^n
    // to our target value, where I is the number of RORs to go the opposite
    // direction.
    assert Size > I;
    Immr = (Size - I) & (Size - 1);

    // If size has a 1 in the n'th bit, create a value that has zeroes in
    // bits [0, n] and ones above that.
    NImms = ~(Size-1) << 1;

    // Or the CTO value into the low bits, which must be below the Nth bit
    // bit mentioned above.
    NImms |= (CTO-1);

    // Extract the seventh bit and toggle it to create the N field.
    N = (uint)((NImms >> 6) & 1) ^ 1;

    Encoding = (N << 12) | (Immr << 6) | (NImms & 0x3f);
  }

  return true;
}

//-----------------------------------------------------------------
