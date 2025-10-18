
// arm64.c

// https://developer.arm.com/documentation/dui0802/b/A64-General-Instructions/A64-general-instructions-in-alphabetical-order?lang=en

use ../blob, ../blob2;
use ../common, arm64b;

//-----------------------------------------------------------------

package CMP
  // note: condition is reversed (false <-> true) by toggling bit 0
  const byte table_unsigned[7] = {0b1110,   // filler (always AL)
                                  0b0011,   // smaller (CC)
                                  0b0000,   // equal (EQ)
                                  0b1001,   // smaller or equal (LS)
                                  0b1000 ,  // larger (HI)
                                  0b0001,   // not equal (NE)
                                  0b0010};  // larger or equal (CS)

  const byte table_signed[7]   = {0b1110,  // filler (always AL)
                                  0b1011,  // smaller (LT)
                                  0b0000,  // equal (EQ)
                                  0b1101,  // smaller or equal (LE)
                                  0b1100,  // larger (GT)
                                  0b0001,  // not equal (NE)
                                  0b1010}; // larger or equal (GE)
end CMP;

/*
   3 first bits                            bit 4
when '000' result = (PSTATE.Z == '1'); // EQ or NE
when '001' result = (PSTATE.C == '1'); // CS or CC
when '010' result = (PSTATE.N == '1'); // MI or PL
when '011' result = (PSTATE.V == '1'); // VS or VC
when '100' result = (PSTATE.C == '1' && PSTATE.Z == '0'); // HI or LS
when '101' result = (PSTATE.N == PSTATE.V); // GE or LT
when '110' result = (PSTATE.N == PSTATE.V && PSTATE.Z == '0'); // GT or LE
when '111' result = TRUE; // AL
// Condition flag values in the set '111x' indicate always true

0b0100 : sign bit set
0b0101 : sign bit clear
0b0110 : overflow set
0b0111 : overflow clear

*/

//-----------------------------------------------------------------

// can be used to shift values

void shifted_register (uint       base_opcode,
                       REG        target,  // ZERO allowed (SP not allowed)
                       REG        source1, // ZERO allowed (SP not allowed)
                       REG        source2, // ZERO allowed (SP not allowed)
                       SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                       uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                       int        size)    // 4 or 8
{
  uint opcode, t, s1, s2;

  assert size == 4 || size == 8;
  assert target != SP && source1 != SP && source2 != SP;
  assert source2_shift_value < 64;
  if (size == 4)
    assert source2_shift_value < 32;

  t  = (target  == ZERO) ? 31 : (uint)target;
  s1 = (source1 == ZERO) ? 31 : (uint)source1;
  s2 = (source2 == ZERO) ? 31 : (uint)source2;

  opcode = base_opcode
            + t
            + (s1 << 5)
            + ((uint)source2_shift_value << 10)
            + (s2 << 16)
            + ((uint)source2_shift_type << 22);

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// returns false if the imm value is not supported

bool special_immediate (uint opcode_base,
                        REG  target,     // SP allowed (ZERO not allowed)
                        REG  source,     // ZERO allowed (SP not allowed)
                        int8 imm,
                        int  size)       // 4 or 8
{
  uint opcode, t, s;
  uint Encoding;      // N:immr:imms (13 bit)

  assert target != ZERO && source != SP;
  assert size == 4 || size == 8;

  if (!processLogicalImmediate (imm, (uint)size<<3, out Encoding))
    return false;

  t = (target == SP)   ? 31 : (uint)target;
  s = (source == ZERO) ? 31 : (uint)source;

  opcode = opcode_base
         + t
         + (s << 5)
         + (Encoding << 10);

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);

  return true;
}

//-----------------------------------------------------------------

// returns false if the imm value is not supported

bool special_immediate_s (uint opcode_base,
                          REG  target,     // ZERO allowed (SP not allowed)
                          REG  source,     // ZERO allowed (SP not allowed)
                          int8 imm,
                          int  size)       // 4 or 8
{
  uint opcode, t, s;
  uint Encoding;      // N:immr:imms (13 bit)

  assert source != SP && target != SP;
  assert size == 4 || size == 8;

  if (!processLogicalImmediate (imm, (uint)size<<3, out Encoding))
    return false;

  t = (target == ZERO) ? 31 : (uint)target;
  s = (source == ZERO) ? 31 : (uint)source;

  opcode = opcode_base
         + t
         + (s << 5)
         + (Encoding << 10);

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);

  return true;
}

//-----------------------------------------------------------------

void ext_reg (uint opcode_base,
              REG  target,          // SP allowed (ZERO not allowed)
              REG  source1,         // SP allowed (ZERO not allowed)
              REG  source2,         // ZERO allowed (SP not allowed)
              int  source2_size,    // 1, 2, 4 or 8
              bool source2_signed,
              uint source2_shl_imm, // 0 .. 4
              int  size)            // target size (4 or 8)
{
  uint opcode, t, s1, s2, option;

  assert target != ZERO && source1 != ZERO && source2 != SP;
  assert source2_size == 1 || source2_size == 2 || source2_size == 4 || source2_size == 8;
  assert source2_shl_imm <= 4;
  assert size == 4 || size == 8;

  t  = (target  == SP)   ? 31 : (uint)target;
  s1 = (source1 == SP)   ? 31 : (uint)source1;
  s2 = (source2 == ZERO) ? 31 : (uint)source2;

  switch (source2_size)
  {
    case 1: option = 0; break;
    case 2: option = 1; break;
    case 4: option = 2; break;
    case 8: option = 3; break;
    default: abort;
  }

  if (source2_signed)
    option += (1 << 2);

  opcode = opcode_base
         + t
         + (s1 << 5)
         + ((uint)source2_shl_imm << 10)
         + ((uint)option << 13)
         + (s2 << 16);

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void ext_reg_s (uint opcode_base,
                REG  target,          // ZERO allowed (SP not allowed)
                REG  source1,         // SP allowed (ZERO not allowed)
                REG  source2,         // ZERO allowed (SP not allowed)
                int  source2_size,    // 1, 2, 4 or 8
                bool source2_signed,
                uint source2_shl_imm, // 0 .. 4
                int  size)            // target size (4 or 8)
{
  uint opcode, t, s1, s2, option;

  assert target != SP && source1 != ZERO && source2 != SP;
  assert source2_size == 1 || source2_size == 2 || source2_size == 4 || source2_size == 8;
  assert source2_shl_imm <= 4;
  assert size == 4 || size == 8;

  t  = (target  == ZERO) ? 31 : (uint)target;
  s1 = (source1 == SP)   ? 31 : (uint)source1;
  s2 = (source2 == ZERO) ? 31 : (uint)source2;

  switch (source2_size)
  {
    case 1: option = 0; break;
    case 2: option = 1; break;
    case 4: option = 2; break;
    case 8: option = 3; break;
    default: abort;
  }

  if (source2_signed)
    option += (1 << 2);

  opcode = opcode_base
         + t
         + (s1 << 5)
         + ((uint)source2_shl_imm << 10)
         + ((uint)option << 13)
         + (s2 << 16);

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void reg_imm (uint opcode_base,
              REG  target,     // SP allowed (ZERO not allowed)
              REG  source,     // SP allowed (ZERO not allowed)
              int  imm12,      // 0 to 4095
              bool shl_imm_12, // true to shift imm12 << 12
              int  size)       // 4 or 8
{
  uint opcode, t, s;

  assert target != ZERO && source != ZERO;
  assert imm12 >= 0 && imm12 < 4096;
  assert size == 4 || size == 8;

  t = (target == SP) ? 31 : (uint)target;
  s = (source == SP) ? 31 : (uint)source;

  opcode = opcode_base
         + t
         + (s << 5)
         + ((uint)imm12 << 10);

  if (shl_imm_12)
    opcode += (1 << 22);

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void reg_imm_s (uint opcode_base,
                REG  target,     // ZERO allowed (SP not allowed)
                REG  source,     // SP allowed (ZERO not allowed)
                int  imm12,      // 0 to 4095
                bool shl_imm_12, // true to shift imm12 << 12
                int  size)       // 4 or 8
{
  uint opcode, t, s;

  assert target != SP && source != ZERO;

  assert imm12 >= 0 && imm12 < 4096;
  assert size == 4 || size == 8;

  t = (target == ZERO) ? 31 : (uint)target;
  s = (source == SP) ? 31 : (uint)source;

  opcode = opcode_base
         + t
         + (s << 5)
         + ((uint)imm12 << 10);

  if (shl_imm_12)
    opcode += (1 << 22);

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void opto (uint base_opcode,
           REG  target,   // ZERO allowed - register receives loaded initial value
           REG  addr,     // SP allowed   - address of memory to change
           REG  source,   // ZERO allowed - register value to add
           int  size)     // 1, 2, 4 or 8
{
  uint opcode, t, a, s, siz;

  assert target != SP && addr != ZERO && source != SP;
  assert size == 1 || size == 2 || size == 4 || size == 8;

  t = (target == ZERO) ? 31 : (uint)target;
  a = (addr == SP)     ? 31 : (uint)addr;
  s = (source == ZERO) ? 31 : (uint)source;

// For Load-Acquire, Load-AcquirePC, and Store-Release instructions, the address of the data object that is supplied must be aligned to the size of the data element that is being accessed. Otherwise, the access generates an Alignment fault.
// note: we don't use that

  opcode = base_opcode;

  if (size == 1)
    siz = 0;
  else if (size == 2)
    siz = 1;
  else if (size == 4)
    siz = 2;
  else if (size == 8)
    siz = 3;
  else
    abort;

  opcode += t
          + (a << 5)
          + (s << 16)
          + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void xbfm (uint base_opcode,
           REG  target,     // ZERO allowed (SP not allowed)
           REG  source,     // ZERO allowed (SP not allowed)
           uint imms,       // 0 .. 63  (6 bits)
           uint immr,       // 0 .. 63  (6 bits)
           uint N,          // 0 .. 1
           int  size)       // 4 or 8
{
  uint opcode, t, s, siz;

  assert source != SP && target != SP;
  assert imms < 64;
  assert immr < 64;
  assert N <= 1;
  assert size == 4 || size == 8;

  t = (target == ZERO) ? 31 : (uint)target;
  s = (source == ZERO) ? 31 : (uint)source;

  siz = (size == 4) ? 0 : 1;

  opcode = base_opcode
         + t
         + (s << 5)
         + (imms << 10)
         + (immr << 16)
         + (N << 22)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void ubfm (REG  target,     // ZERO allowed (SP not allowed)
           REG  source,     // ZERO allowed (SP not allowed)
           uint imms,       // 0 .. 63  (6 bits)
           uint immr,       // 0 .. 63  (6 bits)
           uint N,          // 0 .. 1
           int  size)       // 4 or 8
{
  xbfm (base_opcode => 0b0_10_100110_0_000000_000000_00000_00000,
        target      => target,
        source      => source,
        imms        => imms,
        immr        => immr,
        N           => N,
        size        => size);
}

//-----------------------------------------------------------------

void sbfm (REG  target,     // ZERO allowed (SP not allowed)
           REG  source,     // ZERO allowed (SP not allowed)
           uint imms,       // 0 .. 63  (6 bits)
           uint immr,       // 0 .. 63  (6 bits)
           uint N,          // 0 .. 1
           int  size)       // 4 or 8
{
  xbfm (base_opcode => 0b0_00_100110_0_000000_000000_00000_00000,
        target      => target,
        source      => source,
        imms        => imms,
        immr        => immr,
        N           => N,
        size        => size);
}

//-----------------------------------------------------------------

void c_mult_add_sub (uint  base_opcode,
                     REG   target,      // ZERO allowed
                     REG   term,        // ZERO allowed
                     REG   mul1,        // ZERO allowed
                     REG   mul2,        // ZERO allowed
                     int   data_size)   // 4 or 8
{
  uint opcode, t, m1, m2, s, siz;

  assert target != SP && mul1 != SP && mul2 != SP && term != SP;
  assert data_size == 4 || data_size == 8;

  t  = (target == ZERO) ? 31 : (uint)target;
  m1 = (mul1   == ZERO) ? 31 : (uint)mul1;
  m2 = (mul2   == ZERO) ? 31 : (uint)mul2;
  s  = (term   == ZERO) ? 31 : (uint)term;

  siz = (data_size == 4) ? 0 : 1;

  opcode = base_opcode
         + t
         + (m1 << 5)
         + (s << 10)
         + (m2 << 16)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void c_cbranch (uint base_opcode,
                REG  source,  // register being tested
                int  size,    // 4 or 8 bytes
                int  offset)  // 21 bits (+/- 1<<20)  +/- 1MB
{
  uint opcode, siz;

  assert source != SP && source != ZERO;
  assert size == 4 || size == 8;
  assert offset >= -(1<<20) && offset < (1<<20);
  assert (offset & 3) == 0;

  siz = (size == 4) ? 0 : 1;

  opcode = base_opcode
         + (uint)source
         + (((uint)(offset >> 2) & ((1<<19)-1)) << 5)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void c_testbit (uint base_opcode,
                REG  source,         // register being tested
                int  bit,            // bit number (0 to 63)
                int  branch_offset)  // 16 bits (-32768 .. +32764)  +/- 32K
{
  uint opcode, s, siz;

  assert source != SP;
  assert branch_offset >= -32768 && branch_offset <= 32764;
  assert (branch_offset & 3) == 0;
  assert bit >= 0 && bit <= 63;

  s = (source == ZERO) ? 31 : (uint)source;
  siz = (bit <= 31) ? 0 : 1;

  opcode = base_opcode
         + s
         + (((uint)(branch_offset >> 2) & ((1<<14)-1)) << 5)
         + ((uint)bit << 19)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------
//-----------------------------------------------------------------

// ADD (extended register) : t = s1 + (ext(s2) << shift)

public
void c_add_ext_reg (REG  target,          // SP allowed (ZERO not allowed)
                    REG  source1,         // SP allowed (ZERO not allowed)
                    REG  source2,         // ZERO allowed (SP not allowed)
                    int  source2_size,    // 1, 2, 4 or 8
                    bool source2_signed,
                    uint source2_shl_imm, // 0 .. 4
                    int  size)            // target size (4 or 8)
{
  ext_reg (opcode_base     => 0b00001011001000000000000000000000,
           target          => target,
           source1         => source1,
           source2         => source2,
           source2_size    => source2_size,
           source2_signed  => source2_signed,
           source2_shl_imm => source2_shl_imm,
           size            => size);
}

//-----------------------------------------------------------------

// ADD (immediate)

public
void c_add_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095
                    bool shl_imm_12, // true to shift imm12 << 12
                    int  size)       // 4 or 8
{
  reg_imm (opcode_base  => 0b00_010001_000000000000000000000000,
           target       => target,
           source       => source,
           imm12        => imm12,
           shl_imm_12   => shl_imm_12,
           size         => size);
}

//-----------------------------------------------------------------

// ADDS (immediate) (sets flags)

public
void c_adds_reg_imm (REG  target,     // ZERO allowed (SP not allowed)
                     REG  source,     // SP allowed (ZERO not allowed)
                     int  imm12,      // 0 to 4095
                     bool shl_imm_12, // true to shift imm12 << 12
                     int  size)       // 4 or 8
{
  reg_imm_s (opcode_base  => 0b00_110001_000000000000000000000000,
             target       => target,
             source       => source,
             imm12        => imm12,
             shl_imm_12   => shl_imm_12,
             size         => size);
}

//-----------------------------------------------------------------

// ADD (shifted register)
// can be used to shift values

public
void c_add_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b00001011000000000000000000000000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// SUB (extended register) : t = s1 + (ext(s2) << shift)

public
void c_sub_ext_reg (REG  target,          // SP allowed (ZERO not allowed)
                    REG  source1,         // SP allowed (ZERO not allowed)
                    REG  source2,         // ZERO allowed (SP not allowed)
                    int  source2_size,    // 1, 2, 4 or 8
                    bool source2_signed,
                    uint source2_shl_imm, // 0 .. 4
                    int  size)            // target size (4 or 8)
{
  ext_reg (opcode_base     => 0b0_1_0_01011_00_1_00000_000_000_00000_00000,
           target          => target,
           source1         => source1,
           source2         => source2,
           source2_size    => source2_size,
           source2_signed  => source2_signed,
           source2_shl_imm => source2_shl_imm,
           size            => size);
}

//-----------------------------------------------------------------

// SUB (extended register) : t = s1 + (ext(s2) << shift)

public
void c_subs_ext_reg (REG  target,          // ZERO allowed (SP not allowed)
                     REG  source1,         // SP allowed (ZERO not allowed)
                     REG  source2,         // ZERO allowed (SP not allowed)
                     int  source2_size,    // 1, 2, 4 or 8
                     bool source2_signed,
                     uint source2_shl_imm, // 0 .. 4
                     int  size)            // target size (4 or 8)
{
  ext_reg_s (opcode_base     => 0b0_1_1_01011_00_1_00000_000_000_00000_00000,
             target          => target,
             source1         => source1,
             source2         => source2,
             source2_size    => source2_size,
             source2_signed  => source2_signed,
             source2_shl_imm => source2_shl_imm,
             size            => size);
}

//-----------------------------------------------------------------

// SUB (immediate)

public
void c_sub_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095
                    bool shl_imm_12, // true to shift imm12 << 12
                    int  size)       // 4 or 8
{
  reg_imm (opcode_base  => 0b0_1_0_100010_0_000000000000_00000_00000,
           target       => target,
           source       => source,
           imm12        => imm12,
           shl_imm_12   => shl_imm_12,
           size         => size);
}

//-----------------------------------------------------------------

// SUBS (immediate) (sets flags)

public
void c_subs_reg_imm (REG  target,     // ZERO allowed (SP not allowed)
                     REG  source,     // SP allowed (ZERO not allowed)
                     int  imm12,      // 0 to 4095
                     bool shl_imm_12, // true to shift imm12 << 12
                     int  size)       // 4 or 8
{
  reg_imm_s (opcode_base  => 0b0_1_1_100010_0_000000000000_00000_00000,
             target       => target,
             source       => source,
             imm12        => imm12,
             shl_imm_12   => shl_imm_12,
             size         => size);
}

//-----------------------------------------------------------------

// SUB (shifted register)
// can be used to shift values

public
void c_sub_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_1_0_01011_00_0_00000_000000_00000_00000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// SUB (shifted register)
// can be used to shift values

public
void c_subs_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                     REG        source1, // ZERO allowed (SP not allowed)
                     REG        source2, // ZERO allowed (SP not allowed)
                     SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                     uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                     int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_1_1_01011_00_0_00000_000000_00000_00000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// NEG (shifted register)

public
void c_neg_reg_reg (REG target,  // ZERO allowed (SP not allowed)
                    REG source,  // ZERO allowed (SP not allowed)
                    int size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_1_0_01011_00_0_00000_000000_00000_00000,   // SUB
                    target, ZERO, source,
                    LSL, 0,
                    size);
}

//-----------------------------------------------------------------

// MADD
// t = sum + mul1 x mul2
// unsigned multiplication and signed multiplication are exactly the same (ignoring flags).

public
void c_mult_add (REG   target,      // ZERO allowed
                 REG   sum,         // ZERO allowed
                 REG   mul1,        // ZERO allowed
                 REG   mul2,        // ZERO allowed
                 int   data_size)   // 4 or 8
{
  c_mult_add_sub (base_opcode => 0b0_00_11011_000_00000_0_00000_00000_00000,
                  target      => target,
                  term        => sum,
                  mul1        => mul1,
                  mul2        => mul2,
                  data_size   => data_size);
}

//-----------------------------------------------------------------

// MUL
// unsigned multiplication and signed multiplication are exactly the same (ignoring flags).

public
void c_mult (REG   target,      // ZERO allowed
             REG   mul1,        // ZERO allowed
             REG   mul2,        // ZERO allowed
             int   data_size)   // 4 or 8
{
  c_mult_add_sub (base_opcode => 0b0_00_11011_000_00000_0_00000_00000_00000,  // MADD
                  target      => target,
                  term        => ZERO,
                  mul1        => mul1,
                  mul2        => mul2,
                  data_size   => data_size);
}

//-----------------------------------------------------------------

// MSUB: Multiply-Subtract
// t = term - mul1 x mul2
// unsigned multiplication and signed multiplication are exactly the same (ignoring flags).

public
void c_mult_sub (REG   target,      // ZERO allowed
                 REG   term,        // ZERO allowed
                 REG   mul1,        // ZERO allowed
                 REG   mul2,        // ZERO allowed
                 int   data_size)   // 4 or 8
{
  c_mult_add_sub (base_opcode => 0b0_00_11011_000_00000_1_00000_00000_00000,
                  target      => target,
                  term        => term,
                  mul1        => mul1,
                  mul2        => mul2,
                  data_size   => data_size);
}

//-----------------------------------------------------------------

// UMADDL
// Unsigned Multiply-Add Long multiplies two 32-bit register values, adds a 64-bit register value, and writes the result to the 64-bit destination register.
// This instruction is used by the alias UMULL.

public
void c_umul_32_32_64_add_64 (REG   xtarget,      // ZERO allowed
                             REG   xsum,         // ZERO allowed
                             REG   wmul1,        // ZERO allowed
                             REG   wmul2)        // ZERO allowed
{
  c_mult_add_sub (base_opcode => 0b0_00_11011_1_01_00000_0_00000_00000_00000,
                  target      => xtarget,
                  term        => xsum,
                  mul1        => wmul1,
                  mul2        => wmul2,
                  data_size   => 8);
}

//-----------------------------------------------------------------

// SMADDL
// Signed Multiply-Add Long multiplies two 32-bit register values, adds a 64-bit register value, and writes the result to the 64-bit destination register.
// This instruction is used by the alias SMULL

public
void c_smul_32_32_64_add_64 (REG   xtarget,      // ZERO allowed
                             REG   xsum,         // ZERO allowed
                             REG   wmul1,        // ZERO allowed
                             REG   wmul2)        // ZERO allowed
{ 
  c_mult_add_sub (base_opcode => 0b0_00_11011_0_01_00000_0_00000_00000_00000,
                  target      => xtarget,
                  term        => xsum,
                  mul1        => wmul1,
                  mul2        => wmul2,
                  data_size   => 8);
}

//-----------------------------------------------------------------

// UMULH
// Unsigned Multiply High multiplies two 64-bit register values, and writes bits[127:64] of the 128-bit result to the 64-bit destination register.
// -> use for 64 bit unsigned div by constant

public
void c_umul_64_64_high_64 (REG xtarget,      // ZERO allowed
                           REG xmul1,        // ZERO allowed
                           REG xmul2)        // ZERO allowed
{ 
  c_mult_add_sub (base_opcode => 0b0_00_11011_1_10_00000_0_00000_00000_00000,
                  target      => xtarget,
                  term        => ZERO,
                  mul1        => xmul1,
                  mul2        => xmul2,
                  data_size   => 8);
}

//-----------------------------------------------------------------
    
// SMULH
// Signed Multiply High multiplies two 64-bit register values, 
// and writes bits[127:64] of the 128-bit result to the 64-bit destination register.

public
void c_smul_64_64_high_64 (REG xtarget,      // ZERO allowed
                           REG xmul1,        // ZERO allowed
                           REG xmul2)        // ZERO allowed
{ 
  c_mult_add_sub (base_opcode => 0b0_00_11011_0_10_00000_0_00000_00000_00000,
                  target      => xtarget,
                  term        => ZERO,
                  mul1        => xmul1,
                  mul2        => xmul2,
                  data_size   => 8);
}

//-----------------------------------------------------------------

// CSEL
// If the condition is true, Conditional Select writes the value of the first source register to the destination register. 
// If the condition is false, it writes the value of the second source register to the destination register.

public
void csel (REG             target,
           REG             source1,
           REG             source2,
           COMPARISON_FLAG cmp,
           bool            signed,
           int             data_size)   // 4 or 8
{
  uint opcode, t, s1, s2, siz;
  byte mask;

  assert target != SP && source1 != SP && source2 != SP;
  assert data_size == 4 || data_size == 8;

  mask = (byte)(signed ? table_signed[(uint)cmp] : table_unsigned[(uint)cmp]);

  t  = (target  == ZERO) ? 31 : (uint)target;
  s1 = (source1 == ZERO) ? 31 : (uint)source1;
  s2 = (source2 == ZERO) ? 31 : (uint)source2;

  siz = (data_size == 4) ? 0 : 1;

  opcode = 0b0_00_11010100_00000_0000_00_00000_00000
         + t
         + (s1 << 5)
         + (mask << 12)
         + (s2 << 16)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------
 
// SDIV / UDIV

public
void c_div (REG   target,      // ZERO allowed
            REG   source1,     // ZERO allowed
            REG   source2,     // ZERO allowed
            bool  signed,
            int   data_size)   // 4 or 8
{
  uint opcode, t, s1, s2, siz;

  assert target != SP && source1 != SP && source2 != SP;
  assert data_size == 4 || data_size == 8;

  t  = (target == ZERO) ? 31 : (uint)target;
  s1 = (source1 == ZERO) ? 31 : (uint)source1;
  s2 = (source2 == ZERO) ? 31 : (uint)source2;

  siz = (data_size == 4) ? 0 : 1;

  opcode = 0b0_0_0_11010110_00000_00001_0_00000_00000
         + t
         + (s1 << 5)
         + (((uint)signed) << 10)
         + (s2 << 16)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// ADR : target = PC + offset (+/- 1 MB range)
// load 21 bit unsigned value in register

public
void c_adr (REG target,  // ZERO allowed (SP not allowed)
            int imm)     // 21 bit
{
  uint opcode, t;

  assert target != SP;
  assert imm >= -0b100000000000000000000 && imm < 0b100000000000000000000;

  t  = (target  == ZERO) ? 31 : (uint)target;

  opcode = 0b00010000000000000000000000000000
            + t
            + (uint)(((imm >> 2) & 0b1111111111111111111) << 5)
            + (uint)((imm & 0b11) << 29);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// ADRP : target = PC + 4K-offset (+/- 4 GB range)
// load 4K page address in register

public
void c_adrp (REG target,  // ZERO allowed (SP not allowed)
             int imm)     // 21 bit (will be shifted 12 bits to the left)
{
  uint opcode, t;

  assert target != SP;
  assert imm >= -0b100000000000000000000 && imm < 0b100000000000000000000;

  t  = (target  == ZERO) ? 31 : (uint)target;

  opcode = 0b10010000000000000000000000000000
            + t
            + (uint)(((imm >> 2) & 0b1111111111111111111) << 5)
            + (uint)((imm & 0b11) << 29);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// AND (immediate)
// returns false if the imm value is not supported

public
bool c_and_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // ZERO allowed (SP not allowed)
                    int8 imm,
                    int  size)       // 4 or 8
{
  return special_immediate
     (opcode_base => 0b0_00_100100_0_000000_000000_00000_00000,
      target, source, imm, size);
}

//-----------------------------------------------------------------

// ANDS (immediate)
// returns false if the imm value is not supported

public
bool c_ands_reg_imm (REG  target,     // ZERO allowed (SP not allowed)
                     REG  source,     // ZERO allowed (SP not allowed)
                     int8 imm,
                     int  size)       // 4 or 8
{
  return special_immediate_s
     (opcode_base => 0b0_11_100100_0_000000_000000_00000_00000,
      target, source, imm, size);
}

//-----------------------------------------------------------------

// AND (shifted register)
// can be used to shift values

public
void c_and_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_00_01010_00_0_00000_000000_00000_00000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// ANDS (shifted register)
// can be used to shift values

public
void c_ands_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                     REG        source1, // ZERO allowed (SP not allowed)
                     REG        source2, // ZERO allowed (SP not allowed)
                     SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                     uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                     int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_11_01010_00_0_00000_000000_00000_00000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// EOR (immediate)
// returns false if the imm value is not supported

public
bool c_eor_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // ZERO allowed (SP not allowed)
                    int8 imm,
                    int  size)       // 4 or 8
{
  return special_immediate
     (opcode_base => 0b01010010000000000000000000000000,
      target, source, imm, size);
}

//-----------------------------------------------------------------

// EOR (shifted register)
// can be used to shift values

public
void c_eor_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b01001010000000000000000000000000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// OR (immediate)
// returns false if the imm value is not supported

public
bool c_or_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                   REG  source,     // ZERO allowed (SP not allowed)
                   int8 imm,
                   int  size)       // 4 or 8
{
  return special_immediate
     (opcode_base => 0b0_01_100100_0_000000_000000_00000_00000,
      target, source, imm, size);
}

//-----------------------------------------------------------------

// OR (shifted register)
// can be used to shift values

public
void c_or_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                   REG        source1, // ZERO allowed (SP not allowed)
                   REG        source2, // ZERO allowed (SP not allowed)
                   SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                   uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                   int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_01_01010_00_0_00000_000000_00000_00000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// ORN (shifted register)
// OR of a register and the complement of an optionally shifted register
// can be used to shift values

public
void c_orn_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_01_01010_00_1_00000_000000_00000_00000,
                    target, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// NOT

public
void c_not_reg_reg (REG target,    // ZERO allowed (SP not allowed)
                    REG source,    // ZERO allowed (SP not allowed)
                    int data_size) // 4 or 8
{
  c_orn_reg_reg (target              => target,
                 source1             => ZERO,
                 source2             => source,
                 source2_shift_type  => LSL,  // LSL, LSR, ASR
                 source2_shift_value => 0, // range 0..31 (or 0..63 for size==8)
                 size                => data_size);   // 4 or 8
}

//-----------------------------------------------------------------

// LSL (immediate)
// multiply by power of 2
// signed or unsigned

public
void c_lsl_imm (REG   target,      // ZERO allowed
                REG   source,      // ZERO allowed (base effective address)(64 bit address)
                int   shifts,      // 1 to 63  (or 1 to 31 for size 4)
                int   data_size)   // 4 or 8
{
  uint imms, immr, N;

  if (data_size == 4)
    assert shifts >= 1 && shifts <= 31;
  else
    assert shifts >= 1 && shifts <= 63;

  if (data_size == 4)
    imms = 31 - (uint)shifts;
  else
    imms = 63 - (uint)shifts;

  immr = imms + 1;

  N = (data_size == 4) ? 0 : 1;

  ubfm (target => target,
        source => source,
        imms   => imms,
        immr   => immr,
        N      => N,             // 0 .. 1
        size   => data_size);    // 4 or 8
}

//-----------------------------------------------------------------

// ASR (immediate)
// divide by power of 2
// signed

public
void c_asr_imm (REG  target,     // ZERO allowed (SP not allowed)
                REG  source,     // ZERO allowed (SP not allowed)
                int  shifts,     // 1 to 63  (or 1 to 31 for size 4)
                int  data_size)  // 4 or 8
{
  uint imms, N;

  if (data_size == 4)
    assert shifts >= 1 && shifts <= 31;
  else
    assert shifts >= 1 && shifts <= 63;

  if (data_size == 4)
    imms = 31;
  else
    imms = 63;

  N = (data_size == 4) ? 0 : 1;

  sbfm (target => target,
        source => source,
        imms   => imms,
        immr   => (uint)shifts,        // 0 .. 63  (6 bits)
        N      => N,                   // 0 .. 1
        size   => data_size);          // 4 or 8
}

//-----------------------------------------------------------------

// LSR (immediate)
// divide by power of 2
// unsigned

public
void c_lsr_imm (REG   target,      // ZERO allowed
                REG   source,      // SP allowed (base effective address)(64 bit address)
                int   shifts,      // 1 to 63  (or 1 to 31 for size 4)
                int   data_size)   // 4 or 8
{
  uint imms, N;

  if (data_size == 4)
    assert shifts >= 1 && shifts <= 31;
  else
    assert shifts >= 1 && shifts <= 63;

  if (data_size == 4)
    imms = 31;
  else
    imms = 63;

  N = (data_size == 4) ? 0 : 1;

  ubfm (target => target,
        source => source,
        imms   => imms,
        immr   => (uint)shifts,
        N      => N,             // 0 .. 1
        size   => data_size);    // 4 or 8
}

//-----------------------------------------------------------------

// LSL (register)
// multiply by power of 2
// signed or unsigned

public
void c_lsl_reg (REG   target,      // ZERO allowed
                REG   source,      // ZERO allowed (base effective address)(64 bit address)
                REG   shifts,      // 0 to 63  (or 0 to 31 for size 4)
                int   data_size)   // 4 or 8
{
  uint opcode, t, s, sh, siz;

  assert target != SP && source != SP && shifts != SP;
  assert data_size == 4 || data_size == 8;

  t  = (target  == ZERO) ? 31 : (uint)target;
  s  = (source  == ZERO) ? 31 : (uint)source;
  sh = (shifts  == ZERO) ? 31 : (uint)shifts;

  siz = (data_size == 4) ? 0 : 1;

  opcode = 0b0_0_11010110_00000_0010_00_00000_00000
         + t
         + (s << 5)
         + (sh << 16)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// ASR (register)
// divide by power of 2
// signed

public
void c_asr_reg (REG  target,     // ZERO allowed (SP not allowed)
                REG  source,     // ZERO allowed (SP not allowed)
                REG  shifts,     // ZERO allowed (SP not allowed) (0 .. 31, or 0 .. 63)
                int  data_size)  // 4 or 8
{
  uint opcode, t, s, sh, siz;

  assert target != SP && source != SP && shifts != SP;
  assert data_size == 4 || data_size == 8;

  t  = (target == ZERO) ? 31 : (uint)target;
  s  = (source == ZERO) ? 31 : (uint)source;
  sh = (shifts == ZERO) ? 31 : (uint)shifts;

  siz = (data_size == 4) ? 0 : 1;

  opcode = 0b0_00_11010110_00000_001010_00000_00000
         + t
         + (s << 5)
         + (sh << 16)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LSR (register)
// divide by power of 2
// unsigned

public
void c_lsr_reg (REG   target,      // ZERO allowed
                REG   source,      // ZERO allowed (base effective address)(64 bit address)
                REG   shifts,      // 0 to 63  (or 0 to 31 for size 4)
                int   data_size)   // 4 or 8
{
  uint opcode, t, s, sh, siz;

  assert target != SP && source != SP && shifts != SP;
  assert data_size == 4 || data_size == 8;

  t  = (target  == ZERO) ? 31 : (uint)target;
  s  = (source  == ZERO) ? 31 : (uint)source;
  sh = (shifts  == ZERO) ? 31 : (uint)shifts;

  siz = (data_size == 4) ? 0 : 1;

  opcode = 0b0_0_11010110_00000_0010_01_00000_00000
         + t
         + (s << 5)
         + (sh << 16)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// B  (branch PC +/- 128 MB)

public
void c_jmp (int offset)  // 28 bits (+/- 1<<27)  +/- 128 MB
{
  uint opcode;

  assert offset >= -(1<<27) && offset < (1<<27);
  assert (offset & 3) == 0;

  opcode = 0b0_0010100000_00000_000000_00000_00000
         + ((uint)(offset >> 2) & ((1<<26)-1));

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// BR  (branch to register)

public
void c_jmp_reg (REG target)
{
  uint opcode;

  assert target != SP && target != ZERO;

  opcode = 0b11010110000111110000000000000000
         + ((uint)target << 5);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// BL  (branch to subroutine relative to PC +/- 128 MB)
// store PC+4 in X30 then branch

public
void c_jsr (int offset)  // 28 bits (+/- 1<<27)  +/- 128 MB
{
  uint opcode;

  assert offset >= -(1<<27) && offset < (1<<27);
  assert (offset & 3) == 0;

  opcode = 0b1_0010100000_00000_000000_00000_00000
         + ((uint)(offset >> 2) & ((1<<26)-1));

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// BL  (branch to subroutine in register)
// store PC+4 in X30 then branch

public
void c_jsr_reg (REG target)
{
  uint opcode;

  assert target != SP && target != ZERO;

  opcode = 0b11010110001111110000000000000000
         + ((uint)target << 5);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

public void c_ret (REG target = X30)
{
  assert target != SP && target != ZERO;

  blob_put_uint4 (ref g_blob_code,
                      0b1101011_00_10_11111_0000_00_00000_00000
                      + ((uint)target << 5));
}

//-----------------------------------------------------------------

// B.cond  (conditional branch PC +/- 1MB)

public
void c_cond_branch_raw (byte mask,
                        int  offset,  // 21 bits (+/- 1<<20)  +/- 1MB
                        bool often_same_choice = true)
{
  uint opcode;

  assert offset >= -(1<<20) && offset < (1<<20);
  assert (offset & 3) == 0;

  _unused often_same_choice;

  opcode = 0b01010100_0000000000000000000_0_0000
         + (((uint)(offset >> 2) & ((1<<19)-1)) << 5)
//         + ((uint)often_same_choice << 4)   // FEAT_HBC, NOT SUPPORTED ON Samsung A15, only high-end devices !
         + mask;

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// B.cond  (conditional branch PC +/- 1MB)

public
void c_cond_branch (COMPARISON_FLAG cmp,
                    bool            signed,
                    int  offset,  // 21 bits (+/- 1<<20)  +/- 1MB
                    bool often_same_choice = true)
{
  c_cond_branch_raw (mask              => (byte)(signed ? table_signed[(uint)cmp] : table_unsigned[(uint)cmp]),
                     offset            => offset,  // 21 bits (+/- 1<<20)  +/- 1MB
                     often_same_choice => often_same_choice);
}

//-----------------------------------------------------------------

// CBNZ (branch if not zero)

public
void c_bnz_reg (REG source,  // register being tested
                int size,    // 4 or 8 bytes
                int offset)  // 21 bits (+/- 1<<20)  +/- 1MB
{
  c_cbranch (base_opcode => 0b00110101_0000000000000000000_00000,
             source      => source,
             size        => size,
             offset      => offset);
}

//-----------------------------------------------------------------

// CBZ (branch if zero)

public
void c_bz_reg (REG source,  // register being tested
               int size,    // 4 or 8 bytes
               int offset)  // 21 bits (+/- 1<<20)  +/- 1MB
{
  c_cbranch (base_opcode => 0b00110100_0000000000000000000_00000,
             source      => source,
             size        => size,
             offset      => offset);
}

//-----------------------------------------------------------------

// TBNZ: Test bit and Branch if Nonzero.

public
void c_tbnz (REG source,         // register being tested
             int bit,            // bit number (0 to 63)
             int branch_offset)  // 16 bits (-32768 .. +32764)  +/- 32K
{
  c_testbit (base_opcode   => 0b0_011011_1_00000_00000000000000_00000,
             source        => source,
             bit           => bit,
             branch_offset => branch_offset);
}

//-----------------------------------------------------------------

// TBZ: Test bit and Branch if Zero.

public
void c_tbz (REG source,         // register being tested
            int bit,            // bit number (0 to 63)
            int branch_offset)  // 16 bits (-32768 .. +32764)  +/- 32K
{
  c_testbit (base_opcode   => 0b0_011011_0_00000_00000000000000_00000,
             source        => source,
             bit           => bit,
             branch_offset => branch_offset);
}

//-----------------------------------------------------------------

// CMP (extended register)

public
void c_cmp_ext_reg (REG  source1,         // SP allowed (ZERO not allowed)
                    REG  source2,         // ZERO allowed (SP not allowed)
                    int  source2_size,    // 1, 2, 4 or 8
                    bool source2_signed,
                    uint source2_shl_imm, // 0 .. 4
                    int  size)            // compare size (4 or 8)
{
  ext_reg (opcode_base     => 0b01101011001000000000000000011111,
           target          => (REG)0,
           source1         => source1,
           source2         => source2,
           source2_size    => source2_size,
           source2_signed  => source2_signed,
           source2_shl_imm => source2_shl_imm,
           size            => size);
}

//-----------------------------------------------------------------

// CMP (immediate)

public
void c_cmp_reg_imm (REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095
                    bool shl_imm_12, // true to shift imm12 << 12
                    int  size)       // 4 or 8
{
  reg_imm (opcode_base  => 0b01110001000000000000000000011111,
           target       => (REG)0,
           source       => source,
           imm12        => imm12,
           shl_imm_12   => shl_imm_12,
           size         => size);
}

//-----------------------------------------------------------------

// CMP (shifted register)
// can be used to shift values

public
void c_cmp_reg_reg (REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size)    // 4 or 8
{
  shifted_register (base_opcode => 0b01101011000000000000000000011111,
                    (REG)0, source1, source2,
                    source2_shift_type, source2_shift_value,
                    size);
}

//-----------------------------------------------------------------

// CMN (immediate) : compare reg with negative value

public
void c_cmn_reg_imm (REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095 (used as negative value)
                    bool shl_imm_12, // true to shift imm12 << 12
                    int  size)       // 4 or 8
{
  reg_imm (opcode_base  => 0b00_110001_000000000000000000000000,
           target       => SP,   // we use SP here but in fact we mean ZERO to ignore the result !
           source       => source,
           imm12        => imm12,
           shl_imm_12   => shl_imm_12,
           size         => size);
}

//-----------------------------------------------------------------

// TST (shifted register)
// sets condition flags

public
void c_tst_reg (REG source, // ZERO allowed (SP not allowed)
                int size)    // 4 or 8
{
  shifted_register (base_opcode => 0b0_11_01010_00_0_00000_000000_00000_00000,  // ANDS
                    ZERO, source, source,
                    LSL, 0,
                    size);
}

//-----------------------------------------------------------------

// CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
// used for: b = (x < y);

public
void c_cset (COMPARISON_FLAG cmp,
             bool            signed,
             REG             target,
             int             size)       // 4 or 8
{
  uint opcode, t;

  assert target != SP;
  assert size == 4 || size == 8;

  t = (target == ZERO) ? 31 : (uint)target;

  opcode = 0b00011010100111110000011111100000
         + (((signed ? table_signed[(uint)cmp] : table_unsigned[(uint)cmp]) ^ 1) << 12)
         + t;

  if (size == 8)
    opcode += (1<<31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LDADD : atomic ADD to

public
void c_addto (REG target,   // ZERO allowed - register receives loaded initial value
              REG addr,     // SP allowed   - address of memory to change
              REG source,   // ZERO allowed - register value to add
              int size)     // 1, 2, 4 or 8
{
  opto (base_opcode => 0b00111000001000000000000000000000,
        target      => target,
        addr        => addr,
        source      => source,
        size        => size);
}

//-----------------------------------------------------------------

// LDCLR : atomic AND to

public
void c_andto (REG target,    // ZERO allowed - register receives loaded initial value
              REG addr,     // SP allowed   - address of memory to change
              REG source,   // ZERO allowed - register value to add
              int size)     // 1, 2, 4 or 8
{
  opto (base_opcode => 0b00_111_0_00_0_0_1_00000_0_001_00_0000000000,
        target      => target,
        addr        => addr,
        source      => source,
        size        => size);
}

//-----------------------------------------------------------------

// LDSET : atomic OR to

public
void c_orto (REG target,    // ZERO allowed - register receives loaded initial value
             REG addr,     // SP allowed   - address of memory to change
             REG source,   // ZERO allowed - register value to add
             int size)     // 1, 2, 4 or 8
{
  opto (base_opcode => 0b00111000001000000011000000000000,
        target      => target,
        addr        => addr,
        source      => source,
        size        => size);
}

//-----------------------------------------------------------------

// LDEOR : Atomic Exclusive-OR

public
void c_eorto (REG target,   // ZERO allowed - register receives loaded initial value
              REG addr,     // SP allowed   - address of memory to change
              REG source,   // ZERO allowed - register value to add
              int size)     // 1, 2, 4 or 8
{
  opto (base_opcode => 0b00111000001000000010000000000000,
        target      => target,
        addr        => addr,
        source      => source,
        size        => size);
}

//-----------------------------------------------------------------

// LDUR (immediate), LDURB LDURH LDURSB LDURSH LDURSW
// returns false if offset is not encodable

public
bool c_load_ofs8 (REG   target,      // ZERO allowed
                  REG   base,        // SP allowed (effective address)
                  int   offset,      // 9 bits (-256 to 255) to be added to base address
                  bool  data_signed, // data to load is signed
                  int   data_size)   // size of data to load : 1, 2, 4, 8
{
  uint opcode, t, s, siz, opc;

  assert target != SP && base != ZERO;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  if (offset < -256 || offset > 255)
    return false;

  t = (target == ZERO) ? 31 : (uint)target;
  s = (base == SP)   ? 31 : (uint)base;

/*
       siz  opc
uint1   00  01  -> load in 4 bytes only   LDURB  (zero extends to 8 bytes anyway)
uint2   01  01  -> load in 4 bytes only   LDURH  (zero extends to 8 bytes anyway)
uint4   10  01  -> load in 4 bytes only   LDUR   (zero extends to 8 bytes anyway)

int1    00  1x  -> 11=load in 4 bytes,    LDURSB
                   10=load in 8 bytes
int2    01  1x  -> 11=load in 4 bytes,    LDURSH
                   10=load in 8 bytes
int4    10  1x  ->                        LDURSW
                   10=load in 8 bytes

uint8   11  01  -> load in 8 bytes        LDUR
*/

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  if (data_size == 8 || !data_signed)   // uint1, uint2, uint4, uint8, int8
    opc = 1;
  else
    opc = 2;  // int1, int2, int4

  opcode = 0b00_111_0_00_00_0_000000000000000000000
         + t
         + (s << 5)
         + (((uint)offset & 511) << 12)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// LDR (immediate), LDRB, LDRH, LDRSB, LDRSH, LDRSW
// load item at address (base + offset)
// returns false if offset is not encodable

public
bool c_load_ofs12 (REG   target,      // ZERO allowed
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // 12 bits (0 to 4095 * size) to be added to base address
                   bool  data_signed,
                   int   data_size)   // 1, 2, 4 or 8
{
  uint opcode, t, s, siz, opc, imm;

  assert target != SP && base != ZERO;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  if (offset < 0)
    return false;

  imm = (uint)(offset / data_size);
  if (imm > 4095)
    return false;
  if ((int)imm * data_size != offset)
    return false;

  t = (target == ZERO) ? 31 : (uint)target;
  s = (base == SP) ? 31 : (uint)base;

/*
       siz  opc
uint1   00  01  -> load in 4 bytes only   LDRB  (zero extends to 8 bytes anyway)
uint2   01  01  -> load in 4 bytes only   LDRH  (zero extends to 8 bytes anyway)
uint4   10  01  -> load in 4 bytes only   LDR   (zero extends to 8 bytes anyway)

int1    00  1x  -> 11=load in 4 bytes,    LDRSB
                   10=load in 8 bytes
int2    01  1x  -> 11=load in 4 bytes,    LDRSH
                   10=load in 8 bytes
int4    10  1x  ->                        LDRSW
                   10=load in 8 bytes

uint8   11  01  -> load in 8 bytes        LDR
*/

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  if (data_size == 8 || !data_signed)   // uint1, uint2, uint4, uint8, int8
    opc = 1;
  else
    opc = 2;  // int1, int2, int4

  opcode = 0b00_111_0_01_00_000000000000_00000_00000
         + t
         + (s << 5)
         + (imm << 10)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);

  return true;
}

//-----------------------------------------------------------------

// LDR (immediate), LDRB, LDRH, LDRSH, LDRSH, LDRSW
// load item at address (base + offset)
// base will be increased by offset before or after the loading.

public
void c_load_add (REG   target,      // ZERO allowed
                 REG   base,        // SP allowed (effective address)
                 int   offset,      // 9 bits (-256 to 255) to be added to base address
                 bool  pre_add,     // false : post_add base by offset, true : pre_add base by offset
                 bool  data_signed,
                 int   data_size)   // 1, 2, 4 or 8
{
  uint opcode, t, s, siz, opc, post;

  assert target != SP && base != ZERO;
  assert offset >= -256 && offset <= 255;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  t = (target == ZERO) ? 31 : (uint)target;
  s = (base == SP)   ? 31 : (uint)base;

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  if (data_size == 8 || !data_signed)   // uint1, uint2, uint4, uint8, int8
    opc = 1;
  else
    opc = 2;  // int1, int2, int4

  post = pre_add ? 3 : 1;

  opcode = 0b00_111_0_00_00_0_000000000_00_00000_00000
         + t
         + (s << 5)
         + (post << 10)
         + (((uint)offset & 511) << 12)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LDR (register), LDRB, LDRH, LDRSB, LDRSH, LDRSW

public
void c_load_reg_reg (REG   target,         // ZERO allowed
                     REG   base,           // SP allowed (base effective address)(64 bit address)
                     REG   index,          // ZERO allowed
                     bool  index_signed,
                     int   index_size,     // 4 or 8
                     bool  mult_index_by_data_size,
                     bool  data_signed,
                     int   data_size)      // 1, 2, 4 or 8
{
  uint opcode, t, s, S, option, idx, opc, siz;

  assert target != SP && base != ZERO && index != SP;
  assert index_size == 4 || index_size == 8;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  t   = (target == ZERO) ? 31 : (uint)target;
  s   = (base   == SP)   ? 31 : (uint)base;
  idx = (index  == ZERO) ? 31 : (uint)index;

/*
       siz  opc
uint1   00  01  -> load in 4 bytes only   LDRB  (zero extends to 8 bytes anyway)
uint2   01  01  -> load in 4 bytes only   LDRH  (zero extends to 8 bytes anyway)
uint4   10  01  -> load in 4 bytes only   LDR   (zero extends to 8 bytes anyway)

int1    00  1x  -> 11=load in 4 bytes,    LDRSB
                   10=load in 8 bytes
int2    01  1x  -> 11=load in 4 bytes,    LDRSH
                   10=load in 8 bytes
int4    10  1x  ->                        LDRSW
                   10=load in 8 bytes

uint8   11  01  -> load in 8 bytes        LDR
*/

  S = (mult_index_by_data_size && data_size > 1) ? 1 : 0;

  if (index_size == 4)
  {
    if (index_signed)
      option = 6;     // SXTW
    else
      option = 2;     // UXTW
  }
  else  // index_size == 8
  {
    if (index_signed)
      option = 7;    // SXTX
    else
      option = 3;    // LSL
  }

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  if (data_size == 8 || !data_signed)   // uint1, uint2, uint4, uint8, int8
    opc = 1;
  else
    opc = 2;  // int1, int2, int4

  opcode = 0b00_111_0_00_00_1_00000_000_0_10_00000_00000
         + t
         + (s << 5)
         + (S << 12)
         + (option << 13)
         + (idx << 16)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LDP
// returns false if offset could not be encoded

public
bool c_load_pair (REG   target1,     // ZERO allowed
                  REG   target2,     // ZERO allowed
                  REG   base,        // SP allowed (effective address)
                  int   offset,      // -512 to +504 to be added to base address
                  LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                  bool  data_signed,  // for data_size 4 only
                  int   data_size)   // 4 or 8
{
  int  imm;
  uint opcode, uimm, t1, t2, b, mode, opc;

  assert target1 != SP && target2 != SP && base != ZERO;
  assert data_size == 4 || data_size == 8;
  assert ldp_mode != INVALID_MODE;

  imm = offset / data_size;
  if (imm * data_size != offset)
    return false;
  if (imm < -64 || imm > 63)
    return false;
  uimm = ((uint)imm) & 0b1111111;

  t1 = (target1 == ZERO) ? 31 : (uint)target1;
  t2 = (target2 == ZERO) ? 31 : (uint)target2;
  b  = (base    == SP)   ? 31 : (uint)base;

  if (data_size == 4)
  {
    if (data_signed)
      opc = 1;
    else
      opc = 0;
  }
  else if (data_size == 8)
    opc = 2;
  else
    abort;

  mode = (uint)ldp_mode;  // 1=post, 2=nop, 3=pre

  opcode = 0b00_101_0_000_1_0000000_00000_00000_00000
         + t1
         + (b << 5)
         + (t2 << 10)
         + (uimm << 15)
         + (mode << 23)
         + (opc << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// STUR STURB STURH
// returns false if offset is not encodable

public
bool c_store_ofs8 (REG   source,      // ZERO allowed
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // 9 bits (-256 to 255) to be added to base address
                   int   data_size)   // size of data to load : 1, 2, 4, 8
{
  uint opcode, s, b, siz;

  assert source != SP && base != ZERO;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  if (offset < -256 || offset > 255)
    return false;

  s = (source == ZERO) ? 31 : (uint)source;
  b = (base == SP)     ? 31 : (uint)base;

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  opcode = 0b00_111_0_00_00_0_000000000_00_00000_00000
         + s
         + (b << 5)
         + (((uint)offset & 511) << 12)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// STR (immediate) STRB STRH
// store item at address (base + offset)
// returns false if offset is not encodable

public
bool c_store_ofs12 (REG   source,      // ZERO allowed
                    REG   base,        // SP allowed (effective address)
                    int   offset,      // 12 bits (0 to 4095 * size) to be added to base address
                    int   data_size)   // 1, 2, 4 or 8
{
  uint opcode, s, b, siz, imm;

  assert source != SP && base != ZERO;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  if (offset < 0)
    return false;

  imm = (uint)(offset / data_size);
  if (imm > 4095)
    return false;
  if ((int)imm * data_size != offset)
    return false;

  s = (source == ZERO) ? 31 : (uint)source;
  b = (base == SP)     ? 31 : (uint)base;

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  opcode = 0b00_111_0_01_00_000000000000_00000_00000
         + s
         + (b << 5)
         + (imm << 10)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);

  return true;
}

//-----------------------------------------------------------------

// STR (immediate) STRB STRH
// store item at address (base + offset)
// base will be increased by offset before or after the storing.

public
void c_store_add (REG  source,      // ZERO allowed
                  REG  base,        // SP allowed
                  int  offset,      // 9 bits (-256 to 255) to be added to base address
                  bool pre_add,     // false : post_add base by offset, true : pre_add base by offset
                  int  data_size)   // 1, 2, 4 or 8
{
  uint opcode, s, b, siz, post;

  assert source != SP && base != ZERO;
  assert offset >= -256 && offset <= 255;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  s = (source == ZERO) ? 31 : (uint)source;
  b = (base == SP)     ? 31 : (uint)base;

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  post = pre_add ? 3 : 1;

  opcode = 0b00_111_0_00_00_0_000000000_00_00000_00000
         + s
         + (b << 5)
         + (post << 10)
         + (((uint)offset & 511) << 12)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// STP
// returns false if offset could not be encoded

public
bool c_store_pair (REG   source1,     // ZERO allowed
                   REG   source2,     // ZERO allowed
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // -512 to +504 to be added to base address
                   LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                   int   data_size)   // 4 or 8
{
  int  imm;
  uint opcode, uimm, s1, s2, b, mode, opc;

  assert source1 != SP && source2 != SP && base != ZERO;
  assert data_size == 4 || data_size == 8;
  assert ldp_mode != INVALID_MODE;

  imm = offset / data_size;
  if (imm * data_size != offset)
    return false;
  if (imm < -64 || imm > 63)
    return false;
  uimm = ((uint)imm) & 0b1111111;

  s1 = (source1 == ZERO) ? 31 : (uint)source1;
  s2 = (source2 == ZERO) ? 31 : (uint)source2;
  b  = (base    == SP)   ? 31 : (uint)base;

  if (data_size == 4)
    opc = 0;
  else if (data_size == 8)
    opc = 2;
  else
    abort;

  mode = (uint)ldp_mode;  // 1=post, 2=nop, 3=pre

  opcode = 0b00_101_0_000_0_0000000_00000_00000_00000
         + s1
         + (b << 5)
         + (s2 << 10)
         + (uimm << 15)
         + (mode << 23)
         + (opc << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// STR (register) STRB STRH

public
void c_store_reg_reg (REG   source,         // ZERO allowed
                      REG   base,           // SP allowed (base effective address)(64 bit address)
                      REG   index,          // ZERO allowed
                      bool  index_signed,
                      int   index_size,     // 4 or 8
                      bool  mult_index_by_data_size,
                      int   data_size)      // 1, 2, 4 or 8
{
  uint opcode, s, b, S, option, idx, siz;

  assert source != SP && base != ZERO && index != SP;
  assert index_size == 4 || index_size == 8;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  s   = (source == ZERO) ? 31 : (uint)source;
  b   = (base   == SP)   ? 31 : (uint)base;
  idx = (index  == ZERO) ? 31 : (uint)index;

  S = (mult_index_by_data_size && data_size > 1) ? 1 : 0;

  if (index_size == 4)
  {
    if (index_signed)
      option = 6;     // SXTW
    else
      option = 2;     // UXTW
  }
  else  // index_size == 8
  {
    if (index_signed)
      option = 7;    // SXTX
    else
      option = 3;    // LSL
  }

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  opcode = 0b00_111_0_00_00_1_00000_000_0_10_00000_00000
         + s
         + (b << 5)
         + (S << 12)
         + (option << 13)
         + (idx << 16)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// MOV (bitmask immediate)
// alias of ORR (immediate) with zero
// returns false if this imm value is not supported

public
bool move_imm (REG  target,     // SP allowed
               int8 imm,
               int  data_size)  // 4 or 8
{
  return c_or_reg_imm (target => target,
                       source => ZERO,
                       imm    => imm,
                       size   => data_size);
}

//-----------------------------------------------------------------

// MOV (register)
// move register to register

public
void c_mov_reg_reg (REG  target,     // ZERO allowed (SP not allowed)
                    REG  source,     // ZERO allowed (SP not allowed)
                    int  data_size)  // 4 or 8
{
  shifted_register (base_opcode         => 0b0_01_01010_00_0_00000_000000_00000_00000,  // OR
                    target              => target,
                    source1             => source,
                    source2             => ZERO,
                    source2_shift_type  => LSL,
                    source2_shift_value => 0,
                    size                => data_size);
}

//-----------------------------------------------------------------

void c_movzn (uint  base_opcode,
              REG   target,
              uint2 imm,         // 0 to 65535
              int   shifts_left, // 0, 16, 32 or 48
              int   data_size)   // 4 or 8
{
  uint opcode, t, siz;

  assert target != SP;
  assert shifts_left == 0 || shifts_left == 16 || shifts_left == 32 || shifts_left == 48;
  assert data_size == 4 || data_size == 8;

  if (data_size == 4)
    assert shifts_left <= 16;

  t = (target == ZERO) ? 31 : (uint)target;

  siz = (data_size == 4) ? 0 : 1;

  opcode = base_opcode
         + t
         + (((uint)(shifts_left >> 4)) << 21)
         + (imm << 5)
         + (siz << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// MOVZ
// move 16 bit value in 2 bytes of the register, setting the rest to zero.

public
void c_movz (REG   target,
             uint2 imm,         // 0 to 65535
             int   shifts_left, // 0, 16, 32 or 48
             int   data_size)   // 4 or 8
{
  c_movzn (base_opcode  => 0b0_10_100101_00_0000000000000000_00000,
           target       => target,
           imm          => imm,
           shifts_left  => shifts_left,
           data_size    => data_size);
}

//-----------------------------------------------------------------

// MOVN
// move 16 bit value in 2 bytes of the register, setting the rest to zero,
// then inverting all bits.

public
void c_movn (REG   target,
             uint2 imm,         // 0 to 65535
             int   shifts_left, // 0, 16, 32 or 48
             int   data_size)   // 4 or 8
{
  c_movzn (base_opcode  => 0b0_00_100101_00_0000000000000000_00000,
           target       => target,
           imm          => imm,
           shifts_left  => shifts_left,
           data_size    => data_size);
}

//-----------------------------------------------------------------

// MOVK
// move 16 bit value in 2 bytes of the register, leaving other bits unchanged.

public
void c_movk (REG   target,
             uint2 imm,         // 0 to 65535
             int   shifts_left, // 0, 16, 32 or 48
             int   data_size)   // 4 or 8
{
  c_movzn (base_opcode  => 0b0_11_100101_00_0000000000000000_00000,
           target       => target,
           imm          => imm,
           shifts_left  => shifts_left,
           data_size    => data_size);
}

//-----------------------------------------------------------------

// SXTB SXTH SXTW

public
void c_extend_signed (REG  target,       // 8 bytes
                      REG  source,
                      int  source_size)  // 1, 2 or 4 bytes
{
  assert source_size == 1 || source_size == 2 || source_size == 4;

  sbfm (target  => target,
        source  => source,
        imms    => (uint)source_size*8 - 1,  // 0 .. 63  (6 bits)
        immr    => 0,                        // 0 .. 63  (6 bits)
        N       => 1,    // 0 .. 1
        size    => 8);   // 4 or 8
}

//-----------------------------------------------------------------

// UXTB UXTH

public
void c_extend_unsigned (REG  target,       // 8 bytes
                        REG  source,
                        int  source_size)  // 1 or 2 bytes
{
  assert source_size == 1 || source_size == 2;

  ubfm (target  => target,
        source  => source,
        imms    => (uint)source_size*8 - 1,  // 0 .. 63  (6 bits)
        immr    => 0,                        // 0 .. 63  (6 bits)
        N       => 0,    // 0 .. 1 !always 0 here!
        size    => 4);   // 4 or 8 !always 4 here!
}

//-----------------------------------------------------------------

// SVC supervisor call

public
void c_svc (int imm)  // 0 to 65535
{
  uint opcode;

  assert imm >= 0 && imm < 65536;

  opcode = 0b11010100_000_0000000000000000_000_01
         + ((uint)imm << 5);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// SMC Secure Monitor Call

public
void c_smc (int imm)  // 0 to 65535
{
  uint opcode;

  assert imm >= 0 && imm < 65536;

  opcode = 0b11010100_000_0000000000000000_000_11
         + ((uint)imm << 5);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

public
void c_nop ()
{
  uint opcode;

  opcode = 0b11010101000000110010000000011111;

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// YIELD (give up time slice)

public
void c_yield ()
{
  uint opcode;

  opcode = 0b11010101000000110010_0000_001_11111;

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

public
void c_fneg (FREG target,
             FREG source,
             int  size)          // 4 or 8
{
  uint opcode, ftype;

  assert size == 4 || size == 8;

  ftype = (size == 4) ? 0 : 1;

  opcode = 0b0_0_0_11110_00_10000_10_10000_00000_00000
         + (uint)target
         + ((uint)source << 5)
         + ((uint)ftype << 22);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

void f_binop (uint base_opcode,
              FREG target,
              FREG source1,
              FREG source2,
              int  size)      // 4 or 8
{
  uint opcode, ftype;

  assert size == 4 || size == 8;

  ftype = (size == 4) ? 0 : 1;

  opcode = base_opcode
         + (uint)target
         + ((uint)source1 << 5)
         + ((uint)source2 << 16)
         + ((uint)ftype << 22);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

public
void c_fadd (FREG target,
             FREG source1,
             FREG source2,
             int  size)          // 4 or 8
{
  f_binop (base_opcode => 0b0_0_0_11110_00_1_00000_001_0_10_00000_00000,
           target      => target,
           source1     => source1,
           source2     => source2,
           size        => size);
}

//-----------------------------------------------------------------

public
void c_fsub (FREG target,
             FREG source1,
             FREG source2,
             int  size)          // 4 or 8
{
  f_binop (base_opcode => 0b0_0_0_11110_00_1_00000_001_1_10_00000_00000,
           target      => target,
           source1     => source1,
           source2     => source2,
           size        => size);
}

//-----------------------------------------------------------------

public
void c_fmul (FREG target,
             FREG source1,
             FREG source2,
             int  size)          // 4 or 8
{
  f_binop (base_opcode => 0b0_0_0_11110_00_1_00000_0_00010_00000_00000,
           target      => target,
           source1     => source1,
           source2     => source2,
           size        => size);
}

//-----------------------------------------------------------------

public
void c_fdiv (FREG target,
             FREG source1,
             FREG source2,
             int  size)          // 4 or 8
{
  f_binop (base_opcode => 0b0_0_0_11110_00_1_00000_0001_10_00000_00000,
           target      => target,
           source1     => source1,
           source2     => source2,
           size        => size);
}

//-----------------------------------------------------------------

public
void c_fcmp (FREG source1,
             FREG source2,
             int  size)      // 4 or 8
{
  f_binop (base_opcode => 0b0_0_0_11110_00_1_00000_00_1000_00000_00_000,
           target      => F0,
           source1     => source1,
           source2     => source2,
           size        => size);
}

//-----------------------------------------------------------------

public
void c_fcmp_zero (FREG source1,
                  int  size)      // 4 or 8
{
  f_binop (base_opcode => 0b0_0_0_11110_00_1_00000_00_1000_00000_01_000,
           target      => F0,
           source1     => source1,
           source2     => F0,
           size        => size);
}

//-----------------------------------------------------------------

// LDUR (SIMD&FP): Load SIMD&FP Register (unscaled offset)
// returns false if offset is not encodable

public
bool c_fload_ofs8 (FREG  target,
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // 9 bits (-256 to 255) to be added to base address
                   int   data_size)   // size of data to load : 4 or 8
{
  uint opcode, s, siz, opc;

  assert base != ZERO;
  assert data_size == 4 || data_size == 8;

  if (offset < -256 || offset > 255)
    return false;

  s = (base == SP) ? 31 : (uint)base;

  siz = (data_size == 4) ? 2 : 3;
  opc = 1;

  opcode = 0b00_111_1_00_00_0_000000000_00_00000_00000
         + (uint)target
         + (s << 5)
         + (((uint)offset & 511) << 12)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// LDR (immediate, SIMD&FP)
// load item at address (base + offset)
// returns false if offset is not encodable

public
bool c_fload_ofs12 (FREG target,
                    REG  base,        // SP allowed (effective address)
                    int  offset,      // 12 bits (0 to 4095 * size) to be added to base address
                    int  data_size)   // 4 or 8
{
  uint opcode, b, siz, opc, imm;

  assert base != ZERO;
  assert data_size == 4 || data_size == 8;

  if (offset < 0)
    return false;

  imm = (uint)(offset / data_size);
  if (imm > 4095)
    return false;
  if ((int)imm * data_size != offset)
    return false;

  b = (base == SP) ? 31 : (uint)base;

  opc = 1;
  siz = (data_size == 4) ? 2 : 3;

  opcode = 0b00_111_1_01_00_000000000000_00000_00000
         + (uint)target
         + (b << 5)
         + (imm << 10)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);

  return true;
}

//-----------------------------------------------------------------

// LDR (immediate, SIMD&FP)
// load item at address (base + offset)
// base will be increased by offset before or after the loading.

public
void c_fload_add (FREG  target,
                  REG   base,        // SP allowed (effective address)
                  int   offset,      // 9 bits (-256 to 255) to be added to base address
                  bool  pre_add,     // false : post_add base by offset, true : pre_add base by offset
                  int   data_size)   // 4 or 8
{
  uint opcode, b, siz, opc, post;

  assert base != ZERO;
  assert offset >= -256 && offset <= 255;
  assert data_size == 4 || data_size == 8;

  b = (base == SP) ? 31 : (uint)base;

  siz = (data_size == 4) ? 2 : 3;
  opc = 1;

  post = pre_add ? 3 : 1;

  opcode = 0b00_111_1_00_00_0_000000000_00_00000_00000
         + (uint)target
         + (b << 5)
         + (post << 10)
         + (((uint)offset & 511) << 12)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LDP (SIMD&FP): Load Pair of SIMD&FP registers
// returns false if offset could not be encoded

public
bool c_fload_pair (FREG   target1,
                   FREG   target2,
                   REG    base,        // SP allowed (effective address)
                   int    offset,      // -512 to +504 to be added to base address
                   LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                   int    data_size)   // 4 or 8
{
  int  imm;
  uint opcode, uimm, b, mode, opc;

  assert base != ZERO;
  assert data_size == 4 || data_size == 8;
  assert ldp_mode != INVALID_MODE;

  imm = offset / data_size;
  if (imm * data_size != offset)
    return false;
  if (imm < -64 || imm > 63)
    return false;
  uimm = ((uint)imm) & 0b1111111;

  b = (base == SP) ? 31 : (uint)base;

  opc = (data_size == 4) ? 0 : 1;
  mode = (uint)ldp_mode;  // 1=post, 2=nop, 3=pre

  opcode = 0b00_101_1_000_1_0000000_00000_00000_00000
         + (uint)target1
         + (b << 5)
         + ((uint)target2 << 10)
         + (uimm << 15)
         + (mode << 23)
         + (opc << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// LDR (register, SIMD&FP): Load SIMD&FP Register (register offset).

public
void c_fload_reg_reg (FREG  target,
                      REG   base,           // SP allowed (base effective address)(64 bit address)
                      REG   index,          // ZERO allowed
                      bool  index_signed,
                      int   index_size,     // 4 or 8
                      bool  mult_index_by_data_size,
                      int   data_size)      // 4 or 8
{
  uint opcode, b, S, option, idx, opc, siz;

  assert base != ZERO && index != SP;
  assert index_size == 4 || index_size == 8;
  assert data_size == 4 || data_size == 8;

  b   = (base  == SP)   ? 31 : (uint)base;
  idx = (index == ZERO) ? 31 : (uint)index;

  S = (mult_index_by_data_size && data_size > 1) ? 1 : 0;

  if (index_size == 4)
  {
    if (index_signed)
      option = 6;     // SXTW
    else
      option = 2;     // UXTW
  }
  else  // index_size == 8
  {
    if (index_signed)
      option = 7;    // SXTX
    else
      option = 3;    // LSL
  }

  siz = (data_size == 4) ? 2 : 3;
  opc = 1;

  opcode = 0b00_111_1_00_00_1_00000_000_0_10_00000_00000
         + (uint)target
         + (b << 5)
         + (S << 12)
         + (option << 13)
         + (idx << 16)
         + (opc << 22)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// STUR (SIMD&FP): Store SIMD&FP register (unscaled offset).
// returns false if offset is not encodable

public
bool c_fstore_ofs8 (FREG  source,
                    REG   base,        // SP allowed (effective address)
                    int   offset,      // 9 bits (-256 to 255) to be added to base address
                    int   data_size)   // size of data to load : 4, 8
{
  uint opcode, b, siz;

  assert base != ZERO;
  assert data_size == 4 || data_size == 8;

  if (offset < -256 || offset > 255)
    return false;

  b = (base == SP) ? 31 : (uint)base;
  siz = (data_size == 4) ? 2 : 3;

  opcode = 0b00_111_1_00_00_0_000000000_00_00000_00000
         + (uint)source
         + (b << 5)
         + (((uint)offset & 511) << 12)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// STR (immediate, SIMD&FP): Store SIMD&FP register (immediate offset).
// store item at address (base + offset)
// returns false if offset is not encodable

public
bool c_fstore_ofs12 (FREG  source,
                     REG   base,        // SP allowed (effective address)
                     int   offset,      // 12 bits (0 to 4095 * size) to be added to base address
                     int   data_size)   // 4 or 8
{
  uint opcode, b, siz, imm;

  assert base != ZERO;
  assert data_size == 4 || data_size == 8;

  if (offset < 0)
    return false;

  imm = (uint)(offset / data_size);
  if (imm > 4095)
    return false;
  if ((int)imm * data_size != offset)
    return false;

  b = (base == SP) ? 31 : (uint)base;
  siz = (data_size == 4) ? 2 : 3;

  opcode = 0b00_111_1_01_00_000000000000_00000_00000
         + (uint)source
         + (b << 5)
         + (imm << 10)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);

  return true;
}

//-----------------------------------------------------------------

// STR (immediate, SIMD&FP): Store SIMD&FP register (immediate offset).
// store item at address (base + offset)
// base will be increased by offset before or after the storing.

public
void c_fstore_add (FREG source,
                   REG  base,        // SP allowed
                   int  offset,      // 9 bits (-256 to 255) to be added to base address
                   bool pre_add,     // false : post_add base by offset, true : pre_add base by offset
                   int  data_size)   // 4 or 8
{
  uint opcode, b, siz, post;

  assert base != ZERO;
  assert offset >= -256 && offset <= 255;
  assert data_size == 4 || data_size == 8;

  b = (base == SP) ? 31 : (uint)base;

  siz = (data_size == 4) ? 2 : 3;

  post = pre_add ? 3 : 1;

  opcode = 0b00_111_1_00_00_0_000000000_00_00000_00000
         + (uint)source
         + (b << 5)
         + (post << 10)
         + (((uint)offset & 511) << 12)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// STP (SIMD&FP): Store Pair of SIMD&FP registers.
// returns false if offset could not be encoded

public
bool c_fstore_pair (FREG  source1,
                    FREG  source2,
                    REG   base,        // SP allowed (effective address)
                    int   offset,      // -512 to +504 to be added to base address
                    LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                    int   data_size)   // 4 or 8
{
  int  imm;
  uint opcode, uimm, b, mode, opc;

  assert base != ZERO;
  assert data_size == 4 || data_size == 8;
  assert ldp_mode != INVALID_MODE;

  imm = offset / data_size;
  if (imm * data_size != offset)
    return false;
  if (imm < -64 || imm > 63)
    return false;
  uimm = ((uint)imm) & 0b1111111;

  b = (base == SP) ? 31 : (uint)base;
  opc = (data_size == 4) ? 1 : 2;

  mode = (uint)ldp_mode;  // 1=post, 2=nop, 3=pre

  opcode = 0b00_101_1_000_0_0000000_00000_00000_00000
         + (uint)source1
         + (b << 5)
         + ((uint)source2 << 10)
         + (uimm << 15)
         + (mode << 23)
         + (opc << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
  return true;
}

//-----------------------------------------------------------------

// STR (register, SIMD&FP): Store SIMD&FP register (register offset).

public
void c_fstore_reg_reg (FREG  source,
                       REG   base,           // SP allowed (base effective address)(64 bit address)
                       REG   index,          // ZERO allowed
                       bool  index_signed,
                       int   index_size,     // 4 or 8
                       bool  mult_index_by_data_size,
                       int   data_size)      // 4 or 8
{
  uint opcode, b, S, option, idx, siz;

  assert base != ZERO && index != SP;
  assert index_size == 4 || index_size == 8;
  assert data_size == 4 || data_size == 8;

  b   = (base   == SP)   ? 31 : (uint)base;
  idx = (index  == ZERO) ? 31 : (uint)index;

  S = (mult_index_by_data_size && data_size > 1) ? 1 : 0;

  if (index_size == 4)
  {
    if (index_signed)
      option = 6;     // SXTW
    else
      option = 2;     // UXTW
  }
  else  // index_size == 8
  {
    if (index_signed)
      option = 7;    // SXTX
    else
      option = 3;    // LSL
  }

  siz = (data_size == 4) ? 2 : 3;

  opcode = 0b00_111_1_00_00_1_00000_000_0_10_00000_00000
         + (uint)source
         + (b << 5)
         + (S << 12)
         + (option << 13)
         + (idx << 16)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// FMOV (general) Floating-point to general-purpose register without conversion.

public
void c_regf_to_reg (REG  target,
                    FREG source,
                    int  data_size)  // 4 or 8
{
  uint opcode, ftype, sf;

  assert data_size == 4 || data_size == 8;
  assert target != ZERO && target != SP;

  sf    = (data_size == 4) ? 0 : 1;
  ftype = (data_size == 4) ? 0 : 1;

  opcode = 0b0_0_0_11110_00_1_00_000_000000_00000_00000
         + (uint)target
         + ((uint)source << 5)
         + (0b110 << 16)
         + (ftype << 22)
         + (sf << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// FMOV (general) general-purpose register to Floating-point without conversion.
// can be used to set float register to zero

public
void c_reg_to_regf (FREG target,
                    REG  source,     // can be ZERO
                    int  data_size)  // 4 or 8
{
  uint opcode, s, ftype, sf;

  assert data_size == 4 || data_size == 8;
  assert source != SP;

  s     = (source == ZERO) ? 31 : (uint)source;
  sf    = (data_size == 4) ? 0 : 1;
  ftype = (data_size == 4) ? 0 : 1;

  opcode = 0b0_0_0_11110_00_1_00_000_000000_00000_00000
         + (uint)target
         + ((uint)s << 5)
         + (0b111 << 16)
         + (ftype << 22)
         + (sf << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// FMOV (register): float to float without conversion

public
void c_regf_to_regf (FREG target,
                     FREG source,
                     int  data_size)  // 4 or 8
{
  uint opcode, ftype;

  assert data_size == 4 || data_size == 8;

  ftype = (data_size == 4) ? 0 : 1;

  opcode = 0b0_0_0_11110_00_10000_00_10000_00000_00000
         + (uint)target
         + ((uint)source << 5)
         + (ftype << 22);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// return a 8-bit compressed value of the 32-bit floating-point value.
// If it cannot be represented as an 8-bit value, then return 256.

uint compress_float_to_imm (float cte)
{
  uint f, Sign, uExp, Mantissa;
  int  Exp;

  f'byte = cte'byte;

  Sign     = (f >> 31) & 1;
  Exp      = (int)((f >> 23) & 0xff) - 127;  // -127 to 128
  Mantissa = f & 0x7fffff;                   // 23 bits

  // We can handle 4 bits of mantissa.
  // mantissa = (16+UInt(e:f:g:h))/16.
  if ((Mantissa & 0x7ffff) != 0)
    return 256;
  Mantissa >>= 19;
  if ((Mantissa & 0xf) != Mantissa)
    return 256;

  // We can handle 3 bits of exponent: exp == UInt(NOT(b):c:d)-3
  if (Exp < -3 || Exp > 4)
    return 256;
  uExp = (uint)(((Exp+3) & 0x7) ^ 4);

  return (Sign << 7) | (uExp << 4) | Mantissa;
}

//-----------------------------------------------------------------

// FMOV (scalar, immediate): Floating-point move immediate (scalar).
// returns false if immediate value could not be encoded.

public
bool c_fmov (FREG  target,
             float cte,
             int   size)   // 4 or 8
{
  uint opcode, ftype, imm;

  assert size == 4 || size == 8;

  imm = compress_float_to_imm (cte);
  if (imm == 256)
    return false;

  ftype = (size == 4) ? 0 : 1;

  opcode = 0b0_0_0_11110_00_1_00000000_100_00000_00000
         + (uint)target
         + (imm << 13)
         + (ftype << 22);

  blob_put_uint4 (ref g_blob_code, opcode);

  return true;
}

//-----------------------------------------------------------------

// FCVT  Floating-point Convert precision (scalar).
// single <-> double

public
void c_fconv_precision (FREG  target,
                        int   target_size,   // 4 or 8
                        FREG  source,
                        int   source_size)   // 4 or 8
{
  uint opcode, ftype, opc;

  assert target_size == 4 || target_size == 8;
  assert source_size == 4 || source_size == 8;
  assert target_size != source_size;

  ftype = (uint)source_size >> 3;
  opc   = (uint)target_size >> 3;

  opcode = 0b0_0_0_11110_00_10001_00_10000_00000_00000
         + (uint)target
         + ((uint)source << 5)
         + (opc << 15)
         + (ftype << 22);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// FCVTZS (scalar, integer): Floating-point Convert to Signed integer, rounding toward Zero (scalar).
// FCVTZU (scalar, integer): Floating-point Convert to Unsigned integer, rounding toward Zero (scalar).
// truncate float to int

public
void c_conv_float_to_int (REG   target,        // can be ZERO
                          bool  target_signed,
                          int   target_size,   // 4 or 8
                          FREG  source,
                          int   source_size)   // 4 or 8
{
  uint opcode, t, ftype, sf;

  assert target != SP;
  assert target_size == 4 || target_size == 8;
  assert source_size == 4 || source_size == 8;

  t     = (target == ZERO) ? 31 : (uint)target;
  ftype = (uint)source_size >> 3;
  sf    = (uint)target_size >> 3;

  opcode = target_signed ? 0 : 1;

  opcode = 0b0_0_0_11110_00_1_11_000_000000_00000_00000
         + t
         + ((uint)source << 5)
         + (opcode << 16)
         + (ftype << 22)
         + (sf << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// SCVTF (scalar, integer)	Signed integer convert to floating-point
// UCVTF (scalar, integer)	Unsigned integer convert to floating-point
// int to float

public
void c_conv_int_to_float (FREG  target,
                          int   target_size,   // 4 or 8
                          REG   source,        // can be ZERO
                          bool  source_signed,
                          int   source_size)   // 4 or 8
{
  uint opcode, s, ftype, sf;

  assert source != SP;
  assert target_size == 4 || target_size == 8;
  assert source_size == 4 || source_size == 8;

  s     = (source == ZERO) ? 31 : (uint)source;
  ftype = (uint)target_size >> 3;
  sf    = (uint)source_size >> 3;

  opcode = source_signed ? 2 : 3;

  opcode = 0b0_0_0_11110_00_1_00_000_000000_00000_00000
         + (uint)target
         + (s << 5)
         + (opcode << 16)
         + (ftype << 22)
         + (sf << 31);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LDR (literal) LDRSW
// load int4/uint4/int8 constant from PC + offset (+/- 1MB) into 8 byte register

public
void c_load_cte (REG  target,
                 int  offset,    // +/- 1 MB (must be aligned to 4)
                 bool signed,
                 int  cte_size)  // 4 or 8 bytes
{
  uint opcode, t, siz;

  assert target != SP;
  assert (offset & 3) == 0;
  assert offset >= -(1<<20) && offset < (1<<20);
  assert cte_size == 4 || cte_size == 8;

  t = (target == ZERO) ? 31 : (uint)target;

  if (cte_size == 4)
  {
    if (signed)
      siz = 2;
    else
      siz = 0;
  }
  else
  {
    siz = 1;
  }

  opcode = 0b00_011_0_00_0000000000000000000_00000
         + t
         + ((uint)(offset >> 2) << 5)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LDR (literal, SIMD&FP)
// load float/double constant from PC + offset (+/- 1 MB) into float register

public
void c_fload_cte (FREG  target,
                  int  offset,    // +/- 1 MB (must be aligned to 4)
                  int  cte_size)  // 4 or 8 bytes
{
  uint opcode, siz;

  assert (offset & 3) == 0;
  assert offset >= -(1<<20) && offset < (1<<20);
  assert cte_size == 4 || cte_size == 8;

  siz = (cte_size == 4) ? 0 : 1;

  opcode = 0b00_011_1_00_0000000000000000000_00000
         + (uint)target
         + ((uint)(offset >> 2) << 5)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// SWP

public
void c_swap (REG  address,   // can be SP
             REG  new_value, // can be ZERO
             REG  old_value, // can be ZERO
             int  size)      // 4 or 8
{
  uint opcode, ad, ov, nv, siz;

  assert address != ZERO && new_value != SP && old_value != SP;
  assert size == 4 || size == 8;

  ad = (address   == SP)   ? 31 : (uint)address;
  ov = (old_value == ZERO) ? 31 : (uint)old_value;
  nv = (new_value == ZERO) ? 31 : (uint)new_value;

  siz = (size == 4) ? 2 : 3;

  opcode = 0b00_111_0_00_0_0_1_00000_1_000_00_00000_00000
         + ov
         + (ad << 5)
         + (nv << 16)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// LDAXRB/LDAXRH/LDAXR : Load Exclusive

public
void c_load_excl (REG   target,         // ZERO allowed (data_size bytes)
                  REG   base,           // SP allowed (base effective address)(64 bit address)
                  int   data_size)      // 1, 2, 4 or 8
{
  uint opcode, t, b, siz;

  assert target != SP && base != ZERO;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  t = (target == ZERO) ? 31 : (uint)target;
  b = (base   == SP)   ? 31 : (uint)base;

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  opcode = 0b00_00100001011111111111_00000_00000
         + t
         + (b << 5)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// STLXRB/STLXRH/STLXR : Store Exclusive

public
void c_store_excl (REG  target,     // 32-bit register will contain status result : 0 = ok, 1 = failed
                   REG  base,       // SP allowed (base effective address)(64 bit address)
                   REG  source,     // ZERO allowed (data_size bytes)
                   int  data_size)  // 1, 2, 4 or 8
{
  uint opcode, s, t, b, siz;

  assert source != SP && target != SP && base != ZERO;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  s = (source == ZERO) ? 31 : (uint)source;
  t = (target == ZERO) ? 31 : (uint)target;
  b = (base   == SP)   ? 31 : (uint)base;

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  opcode = 0b00_001000000_00000_111111_00000_00000
         + s
         + (b << 5)
         + (t << 16)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// STLR/STLRB/STLRH
// store item at address, and release

public
void c_store_release (REG   source,      // ZERO allowed
                      REG   base,        // SP allowed (effective address)
                      int   data_size)   // 1, 2, 4 or 8
{
  uint opcode, s, b, siz;

  assert source != SP && base != ZERO;
  assert data_size == 1 || data_size == 2 || data_size == 4 || data_size == 8;

  s = (source == ZERO) ? 31 : (uint)source;
  b = (base == SP)     ? 31 : (uint)base;

  if (data_size == 1)
    siz = 0;
  else if (data_size == 2)
    siz = 1;
  else if (data_size == 4)
    siz = 2;
  else if (data_size == 8)
    siz = 3;
  else
    abort;

  opcode = 0b00_0010001_0_011111_1_11111_00000_00000
         + s
         + (b << 5)
         + (siz << 30);

  blob_put_uint4 (ref g_blob_code, opcode);
}

//-----------------------------------------------------------------

// memory barrier

public void c_dmb (uint crm)
{
  uint opcode;

  assert crm < 16;
  
  opcode = 0b11010101_00000011_00110000_10111111 + (crm << 8);

  blob_put_uint4 (ref g_blob_code, opcode);
}

/************************************************************************/


public
void c_code (byte b)
{
  blob_put_byte (ref g_blob_code, b);
}

//-----------------------------------------------------------------

/*
https://developer.arm.com/documentation/dui0802/b/A64-Data-Transfer-Instructions/A64-data-transfer-instructions-in-alphabetical-order?lang=en

stp fp, lr, [sp, #-0x60]!
stp x19, x20, [sp, #0x10]
stp x21, x22, [sp, #0x20]
stp x23, x24, [sp, #0x30]
stp x25, x26, [sp, #0x40]
stp x27, x28, [sp, #0x50]
mov fp, sp

And place this at the end of your exploit/cheat...
ldp x27, x28, [sp, #0x50]
ldp x25, x26, [sp, #0x40]
ldp x23, x24, [sp, #0x30]
ldp x21, x22, [sp, #0x20]
ldp x19, x20, [sp, #0x10]
ldp fp, lr, [sp], #0x60

*/


//-----------------------------------------------------------------

// atomic:
// LDADD, LDADDA, LDADDAL, LDADDL   a += b; ???? useful
// Compare and Swap instructions, CAS and CASP
// Atomic memory operation instructions, LD<op> and ST<op>,
//   where <op> is one of ADD, CLR, EOR, SET, SMAX, SMIN, UMAX, and UMIN

/*

FSQRT (scalar): Floating-point Square Root (scalar).

SETP, SETM, SETE
Memory Set. These instructions perform a memory set using the value in the bottom byte of the source register. The prologue, main, and epilogue
instructions are expected to be run in succession and to appear consecutively in memory: SETP, then SETM, and then SETE.

STADD, STADDL Atomic add on word or doubleword in memory, without return, atomically loads a 32-bit word or 64-bit doubleword from memory, adds the value held
in a register to it, and stores the result back to memory.

// CAS : Compare and Swap word or doubleword in memory reads a 32-bit word or 64-bit doubleword from memory

*/
