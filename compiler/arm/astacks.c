
// astacks.c

use ../goptions, ../error, ../codout, ../fixup, ../blob, ../blob2, ../dllnames;
use arm64, asm_arm;

// -------------------------------------------------------------------------------------

REG  hint_x;
FREG hint_f;

// -------------------------------------------------------------------------------------

public
void swap_nodes (ref NODE n1, ref NODE n2)
{
  NODE n;
  n  = n1;
  n1 = n2;
  n2 = n;
}

/***********************************************************************************/

public
int size_of_operand (NODE n)
{
  char typ = n.typ;
  if (typ == 'b')
    return 1;
  if (typ == 'i' || typ == 'f')
    return 4;
  if (typ == 'l' || typ == 'd')
    return 8;
  if (typ == 'a')
    return address_size;
  fatal_compiler_error0 ("size_of_operand(1)");
  return 0;
}

/***********************************************************************************/

// an EA with kind == MEMORY and base==FP and offset < -(int)g_min_extra_frame_size
// indicates a temporary storage location

int addr_after_temporary (NODE n)
{
  if (n.kind == MEMORY && n.ea.base == FP && n.ea.offset < -(int)g_min_extra_frame_size)
    return n.ea.offset;
  return 0;
}

/***********************************************************************************/

// We compute a free address for a temporary after all other temporaries.
// Note that this uses the current astacks situation,
// so all already allocated temporary variables must be in the 3 astacks.

int addr_after_all_temporaries ()
{
  int offset, i, ofs;

  offset = -(int)g_min_extra_frame_size;

  for (i=0; i<istack_count; i++)
  {
    ofs = addr_after_temporary (istack[i]);
    if (ofs < offset)
      offset = ofs;
  }

  for (i=0; i<fstack_count; i++)
  {
    ofs = addr_after_temporary (fstack[i]);
    if (ofs < offset)
      offset = ofs;
  }

  for (i=0; i<astack_count; i++)
  {
    ofs = addr_after_temporary (astack[i]);
    if (ofs < offset)
      offset = ofs;
  }

  return offset;
}

/***********************************************************************************/

// allocates an offset for a small temporary zone on the stack.
// Note that this uses the current astacks situation,
// so all already allocated temporary variables must already be in the 3 astacks.
// Consequently, for each pcode, a temporary zone should be allocated only once and in one block !

int4 allocate_temporary (int size)  // 1, 4, 8, ..
{
  int offset = addr_after_all_temporaries ();   // negative value

  offset -= size;  // allocate temporary storage

  // align negative offset
  if (size > 8)       // 9 ..
    offset &= (-16); // align at 16
  else if (size > 4)  // 5 .. 8
    offset &= (-8);  // align at 8
  else if (size > 2)  // 3 .. 4
    offset &= (-4);  // align at 4
  else if (size > 1)  // 2
    offset &= (-2);  // align at 2


  // allocate in current stack frame (note that g_extra_frame_size is positive)
  if (offset < -(int)g_extra_frame_size)
    g_extra_frame_size = (uint)(-offset);

  // make sure SP stays aligned at M16 (note that g_extra_frame_size is positive)
  g_extra_frame_size = (g_extra_frame_size + 15) & (uint'max - 15);  // align M16

  return offset;
}

/***********************************************************************************/

// counts how many times each register is used in a node.
// note that a register can be used several times if a node was cloned.

void count_registers_used_in_node (NODE n, ref int count[68], int inc)
{
  switch (n.kind)
  {
    case INT_REGISTER:
      count[(int)n.reg] += inc;
      break;

    case FLOAT_REGISTER:
      count[34+(int)n.freg] += inc;
      break;

    case EFFECTIVE_ADDRESS:
    case MEMORY:
      count[(int)n.ea.base] += inc;
      count[(int)n.ea.index] += inc;
      break;

    default:
      break;
  }
}

/***********************************************************************************/

// counts how many times each register is used in all astacks.
// note that a register can be used several times, if a node was cloned.

void count_register_usage (out int count[68])
{
  int i;

  clear count;

  for (i=0; i<istack_count; i++)
    count_registers_used_in_node (istack[i], ref count, +1);

  for (i=0; i<fstack_count; i++)
    count_registers_used_in_node (fstack[i], ref count, +1);

  for (i=0; i<astack_count; i++)
    count_registers_used_in_node (astack[i], ref count, +1);
}

/***********************************************************************************/

// counts how many times a specific register is used in all astacks.
// note that a register can be used several times, if a node was cloned.

public int register_usage_count (REG r)
{
  int count[68];

  count_register_usage (out count);

  return count[(uint)r];
}

/***********************************************************************************/

// counts how many times a specific register is used in all astacks.
// note that a register can be used several times, if a node was cloned.

public int fregister_usage_count (FREG r)
{
  int count[68];

  count_register_usage (out count);

  return count[34+(uint)r];
}

/***********************************************************************************/

// computes a list of 'nb_registers' registers not used in any stack's nodes.
// returns 0 if all registers allocated, -1 if we're out of free registers.

int get_free_registers (out REG  regs[],                         // OUT : list of registers
                            NODE crash_node = NULL_CRASH_NODE)   // allow reuse of registers of this node as it will be erased
{
  int count[68];
  int nb_reg, i;

  clear regs;

  if (regs'length < 1)
    fatal_compiler_error0 ("get_free_registers(1)");

  count_register_usage (out count);

  count_registers_used_in_node (crash_node, ref count, -1);  // subtract crash_node


  i = (int)hint_x;
  nb_reg = 0;

  while (nb_reg < regs'length)
  {
    if (i == 15)
      return -1;   // could not reserve enough registers (only X0 to X14 are allowed)

    if (count[i] > 0)   // register is used
    {
      i++;
      continue;
    }

    regs[nb_reg++] = (REG)i;  // store in output
    i++;
  }

  return 0;   // all registers could be allocated
}

/***********************************************************************************/

// computes a list of 'nb_registers' registers not used in any stack's nodes.
// returns 0 if all registers allocated, -1 if we're out of free registers.

int get_free_fregisters (out FREG fregs[],                        // OUT : list of registers
                             NODE crash_node = NULL_CRASH_NODE)   // allow reuse of registers of this node as it will be erased
{
  int count[68];
  int nb_reg, i;

  clear fregs;

  if (fregs'length < 1)
    fatal_compiler_error0 ("get_free_fregisters(1)");

  count_register_usage (out count);

  count_registers_used_in_node (crash_node, ref count, -1);  // subtract crash_node


  i = (int)hint_f;
  nb_reg = 0;

  while (nb_reg < fregs'length)
  {
    if (i == 8)
      return -1;   // could not reserve enough registers (only F0 to F7 are allowed)

    if (count[34+i] > 0)   // register is used
    {
      i++;
      continue;
    }

    fregs[nb_reg++] = (FREG)i;  // store in output
    i++;
  }

  return 0;   // all registers could be allocated
}

/***********************************************************************************/

void set_node_to_new_temporary_memory (ref NODE n, int size)
{
  int ofs = allocate_temporary (size);   // this must be done BEFORE changing the node !!

  n = {typ   => n.typ,   // typ is unchanged
       kind  => MEMORY,
       icte  => 0,
       fcte  => 0.0,
       reg   => X0,
       freg  => F0,
       ea    => {base    => FP,
                 index   => ZERO,
                 scale   => 1,
                 offset  => ofs,
                 reloc   => {kind => RELOC_NONE, nr => 0}}};
}

/***********************************************************************************/

public
void move_register_immediate (REG  target,
                              long imm,
                              int  size)   // 4 or 8
{
  int count_zero, count_ones;
  int shift;

  assert size == 4 || size == 8;

  if (size == 4)
  {
    assert (imm >> 32) == 0 || (imm >> 32) == -1;
  }


  // MOV (bitmask immediate)
  // alias of ORR (immediate) with zero
  // returns false if this imm value is not supported

  if (move_imm (target, imm, size))
    return;


  count_zero = 0;
  count_ones = 0;

  for (shift=0; shift<size*8; shift+=16)
  {
    uint2 value = (uint2)(imm >> shift);

    if (value == 0)
      count_zero++;
    else if (value == 65535)
      count_ones++;
  }

  if (count_zero >= count_ones)   // more zeroes than ones
  {
    for (shift=0; shift<size*8; shift+=16)  // find first 2 bytes different from zero
    {
      uint2 value = (uint2)(imm >> shift);
      if (value != 0)
        break;
    }
    if (shift == size*8)  // value is zero
      shift = 0;          // -> first 2bytes

    // MOVZ
    // move 16 bit value in 2 bytes of the register, setting the rest to zero.
    c_movz (target      => target,
            imm         => (uint2)(imm >> shift),  // 0 to 65535
            shifts_left => shift,                  // 0, 16, 32 or 48
            data_size   => size);                  // 4 or 8

    shift += 16;

    for (; shift<size*8; shift+=16)
    {
      if (((imm >> shift) & 65535) != 0)
      {
        // MOVK
        // move 16 bit value in 2 bytes of the register, leaving other bits unchanged.
        c_movk (target      => target,
                imm         => (uint2)(imm >> shift),  // 0 to 65535
                shifts_left => shift,                  // 0, 16, 32 or 48
                data_size   => size);                  // 4 or 8
      }
    }
  }
  else  // more ones than zeroes
  {
    for (shift=0; shift<size*8; shift+=16)  // find first 2 bytes different from ones
    {
      uint2 value = (uint2)(imm >> shift);
      if (value != 65535)
        break;
    }
    if (shift == size*8)  // value is -1
      shift = 0;          // -> first 2bytes

    // MOVN
    // move 16 bit value in 2 bytes of the register, setting the rest to zero,
    // then inverting all bits.
    c_movn (target      => target,
            imm         => (uint2)(~imm >> shift),  // 0 to 65535
            shifts_left => shift,                   // 0, 16, 32 or 48
            data_size   => size);                   // 4 or 8

    shift += 16;

    for (; shift<size*8; shift+=16)
    {
      if (((imm >> shift) & 65535) != 65535)
      {
        // MOVK
        // move 16 bit value in 2 bytes of the register, leaving other bits unchanged.
        c_movk (target      => target,
                imm         => (uint2)(imm >> shift),  // 0 to 65535
                shifts_left => shift,                  // 0, 16, 32 or 48
                data_size   => size);                  // 4 or 8
      }
    }
  }
}

/************************************************************************/

// uses X15

public
void move_fregister_immediate (FREG   target,
                               float8 imm,
                               int    size)   // 4 or 8
{
  if (imm == 0.0)
  {
    c_reg_to_regf (target    => target,
                   source    => ZERO,     // can be ZERO
                   data_size => size);    // 4 or 8
  }
  else
  {
    float imm4 = (float4)imm;

    // FMOV (scalar, immediate): Floating-point move immediate (scalar).
    // returns false if immediate value could not be encoded.
    if (imm != imm4 ||
        !c_fmov (target => target,
                 cte    => imm4,
                 size   => size))  // 4 or 8
    {
      if (size == 4)
      {
        int4 cte;
        cte'byte = imm4'byte;
        move_register_immediate (target => X15,
                                 imm    => cte,
                                 size   => 4);   // 4 or 8
      }
      else
      {
        int8 cte;
        cte'byte = imm'byte;
        move_register_immediate (target => X15,
                                 imm    => cte,
                                 size   => 8);   // 4 or 8
      }

      // FMOV (general) general-purpose register to Floating-point without conversion.
      // can be used to set float register to zero
      c_reg_to_regf (target    => target,
                     source    => X15,     // can be ZERO
                     data_size => size);   // 4 or 8
    }
  }
}

/************************************************************************/

// uses X15-X17

public
void move_memory_immediate (EA     target,
                            int8   imm,
                            int    size)   // 1, 2, 4 or 8
{
  if (imm == 0)
  {
    c_store_register_in_memory (r => ZERO, ea => target, size => size);    // size = 1, 2, 4, 8
  }
  else
  {
    move_register_immediate (target  => X15,
                             imm     => imm,
                             size    => size <= 4 ? 4 : 8);    // 4 or 8

    c_store_register_in_memory (r    => X15,
                                ea   => target,
                                size => size); // size = 1, 2, 4, 8
  }
}

/***********************************************************************************/

// uses X15-X17

public
void move_memory_fimmediate (EA     target,
                             float8 value,
                             int    size)   // 4 or 8
{
  if (value == 0.0)
  {
    c_store_register_in_memory (r => ZERO, ea => target, size => size);    // size = 1, 2, 4, 8
  }
  else
  {
    if (size == 4)
    {
      float fvalue4 = (float)value;
      int4  ivalue4;

      ivalue4'byte = fvalue4'byte;

      move_register_immediate (target => X15,
                               imm    => ivalue4,
                               size   => 4);      // 4 or 8
    }
    else
    {
      int8 ivalue8;

      ivalue8'byte = value'byte;

      move_register_immediate (target => X15,
                               imm    => ivalue8,
                               size   => 8);      // 4 or 8
    }

    c_store_register_in_memory (r    => X15,
                                ea   => target,
                                size => size);    // size = 1, 2, 4, 8
  }
}

/***********************************************************************************/

// source cannot be X17
// generates 1 to 3 instructions for size 4, upto 5 instructions for size 8

public
void add_offset_using_x17 (REG  target,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                           REG  source,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                           int8 offset,
                           int  size)      // 4 or 8
{
  assert target != ZERO && target != SP;
  assert source != ZERO && source != X17;
  assert size == 4 || size == 8;

  if (offset >= 0 && offset < 4096*4096)   // til 16 MB
  {
    if (offset < 4096)
    {
      if (source != target || offset != 0)
      {
        c_add_reg_imm (target     => target,   // SP allowed (ZERO not allowed)
                       source     => source,   // SP allowed (ZERO not allowed)
                       imm12      => (int)offset,   // 0 to 4095
                       shl_imm_12 => false,    // true to shift imm12 << 12
                       size       => size);    // 4 or 8
      }
    }
    else if ((offset & 4095) == 0)   // multiple of 4K
    {
      c_add_reg_imm (target     => target,       // SP allowed (ZERO not allowed)
                     source     => source,       // SP allowed (ZERO not allowed)
                     imm12      => (int)offset >> 12, // 0 to 4095
                     shl_imm_12 => true,         // true to shift imm12 << 12
                     size       => size);        // 4 or 8
    }
    else   // til 16 MB
    {
      c_add_reg_imm (target     => target,       // SP allowed (ZERO not allowed)
                     source     => source,       // SP allowed (ZERO not allowed)
                     imm12      => (int)offset >> 12, // 0 to 4095
                     shl_imm_12 => true,         // true to shift imm12 << 12
                     size       => size);        // 4 or 8

      c_add_reg_imm (target     => target,         // SP allowed (ZERO not allowed)
                     source     => target,         // SP allowed (ZERO not allowed)
                     imm12      => (int)offset & 4095,  // 0 to 4095
                     shl_imm_12 => false,          // true to shift imm12 << 12
                     size       => size);          // 4 or 8
    }
  }
  else if (offset > -4096*4096 && offset <= 0)   // til 16 MB
  {
    if (offset > -4096)
    {
      c_sub_reg_imm (target     => target,    // SP allowed (ZERO not allowed)
                     source     => source,    // SP allowed (ZERO not allowed)
                     imm12      => -(int)offset,   // 0 to 4095
                     shl_imm_12 => false,     // true to shift imm12 << 12
                     size       => size);     // 4 or 8
    }
    else if (((-offset) & 4095) == 0)   // multiple of 4K
    {
      c_sub_reg_imm (target     => target,          // SP allowed (ZERO not allowed)
                     source     => source,          // SP allowed (ZERO not allowed)
                     imm12      => (-(int)offset) >> 12, // 0 to 4095
                     shl_imm_12 => true,            // true to shift imm12 << 12
                     size       => size);           // 4 or 8
    }
    else   // til 16 MB
    {
      c_sub_reg_imm (target     => target,           // SP allowed (ZERO not allowed)
                     source     => source,           // SP allowed (ZERO not allowed)
                     imm12      => (-(int)offset) >> 12,  // 0 to 4095
                     shl_imm_12 => true,             // true to shift imm12 << 12
                     size       => size);            // 4 or 8

      c_sub_reg_imm (target     => target,           // SP allowed (ZERO not allowed)
                     source     => target,           // SP allowed (ZERO not allowed)
                     imm12      => (-(int)offset) & 4095, // 0 to 4095
                     shl_imm_12 => false,            // true to shift imm12 << 12
                     size       => size);            // 4 or 8
    }
  }
  else   // any offset
  {
    int  s;
    int8 ofs;

    // try smallest possible value (it could take 1 instruction instead of 2)
    for (s=4; (offset & ((1L<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
      ;

    ofs = offset >> s;

    if (ofs >= -(1L<<31) && ofs < (1L<<31))   // signed
    {
      // should generate 1 or 2 instructions
      move_register_immediate (target  => X17,
                               imm     => ofs,
                               size    => 4);     // 4 or 8

      c_add_ext_reg (target          => target,   // SP allowed (ZERO not allowed)
                     source1         => source,   // SP allowed (ZERO not allowed)
                     source2         => X17,      // ZERO allowed (SP not allowed)
                     source2_size    => 4,        // 1, 2, 4 or 8
                     source2_signed  => true,
                     source2_shl_imm => (uint)s,  // 0 .. 4
                     size            => size);    // target size (4 or 8)
    }
    else if (ofs >= 0 && ofs < (1L<<32))   // unsigned
    {
      // should generate 1 or 2 instructions
      move_register_immediate (target  => X17,
                               imm     => ofs,
                               size    => 4);     // 4 or 8

      c_add_ext_reg (target          => target,   // SP allowed (ZERO not allowed)
                     source1         => source,   // SP allowed (ZERO not allowed)
                     source2         => X17,      // ZERO allowed (SP not allowed)
                     source2_size    => 4,        // 1, 2, 4 or 8
                     source2_signed  => false,
                     source2_shl_imm => (uint)s,  // 0 .. 4
                     size            => size);    // target size (4 or 8)
    }
    else if (ofs > -(1L<<32) && ofs <= 0)   // unsigned, negative
    {
      // should generate 1 or 2 instructions
      move_register_immediate (target  => X17,
                               imm     => -ofs,
                               size    => 4);     // 4 or 8

      c_sub_ext_reg (target          => target,   // SP allowed (ZERO not allowed)
                     source1         => source,   // SP allowed (ZERO not allowed)
                     source2         => X17,      // ZERO allowed (SP not allowed)
                     source2_size    => 4,        // 1, 2, 4 or 8
                     source2_signed  => false,
                     source2_shl_imm => (uint)s,  // 0 .. 4
                     size            => size);    // target size (4 or 8)
    }
    else
    {
      if (source != SP)
      {
        if (size == 4)
          s = 31;
        else
          s = 63;

        for (; (offset & ((1L<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
          ;

        ofs = offset >> s;

        // should generate 1 to 4 instructions
        move_register_immediate (target  => X17,
                                 imm     => ofs,
                                 size    => size);     // 4 or 8

        // ADD (shifted register)
        // can be used to shift values
        c_add_reg_reg (target              => target,  // ZERO allowed (SP not allowed)
                       source1             => source,  // ZERO allowed (SP not allowed)
                       source2             => X17,     // ZERO allowed (SP not allowed)
                       source2_shift_type  => LSL,     // LSL, LSR, ASR
                       source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                       size                => size);   // 4 or 8
      }
      else   // for source == SP
      {
        // should generate 1 to 4 instructions
        move_register_immediate (target  => X17,
                                 imm     => ofs,
                                 size    => size);     // 4 or 8

        c_add_ext_reg (target          => target,   // SP allowed (ZERO not allowed)
                       source1         => source,   // SP allowed (ZERO not allowed)
                       source2         => X17,      // ZERO allowed (SP not allowed)
                       source2_size    => size,     // 1, 2, 4 or 8
                       source2_signed  => true,
                       source2_shl_imm => (uint)s,  // 0 .. 4
                       size            => size);    // target size (4 or 8)
      }
    }
  }
}

/************************************************************************/

// value must be >= 1
// returns 0 to 63

public
int lshifts_of (int8 value)
{
  int8 n = value;
  int  r = 0;

  while (n > 1)
  {
    r++;
    n >>= 1;
  }
  return r;
}

/************************************************************************/

void load_store_ofs8 (int   source,      // ZERO allowed (SP not allowed)
                      REG   base,        // SP allowed (ZERO not allowed)
                      int   offset,      // 9 bits (-256 to 255) to be added to base address
                      int   data_size,   // size of data to load : 1, 2, 4, 8)
                      bool  data_signed,
                      bool  store,       // false = load, true = store
                      bool  is_int)      // true = X, false = F
{
  if (is_int)
  {
    if (store)
    {
      // STUR STURB STURH
      // returns false if offset is not encodable
      assert c_store_ofs8 (source        => (REG)source, // ZERO allowed (SP not allowed)
                           base          => base,        // SP allowed (ZERO not allowed)
                           offset        => offset,      // 9 bits (-256 to 255) to be added to base address
                           data_size     => data_size);  // size of data to load : 1, 2, 4, 8
    }
    else
    {
      assert c_load_ofs8 (target        => (REG)source, // ZERO allowed (SP not allowed)
                          base          => base,        // SP allowed (ZERO not allowed)
                          offset        => offset,      // 9 bits (-256 to 255) to be added to base address
                          data_signed   => data_signed,
                          data_size     => data_size);  // size of data to load : 1, 2, 4, 8
    }
  }
  else
  {
    if (store)
    {
      assert c_fstore_ofs8 (source        => (FREG)source, // ZERO allowed (SP not allowed)
                            base          => base,         // SP allowed (ZERO not allowed)
                            offset        => offset,       // 9 bits (-256 to 255) to be added to base address
                            data_size     => data_size);   // size of data to load : 1, 2, 4, 8
    }
    else
    {
      assert c_fload_ofs8 (target        => (FREG)source, // ZERO allowed (SP not allowed)
                           base          => base,         // SP allowed (ZERO not allowed)
                           offset        => offset,       // 9 bits (-256 to 255) to be added to base address
                           data_size     => data_size);   // size of data to load : 1, 2, 4, 8
    }
  }
}

/************************************************************************/

void load_store_ofs12 (int   source,      // ZERO allowed (SP not allowed)
                       REG   base,        // SP allowed (ZERO not allowed)
                       int   offset,      // 12 bits (0 to 4095 * size) to be added to base address
                       int   data_size,   // 1, 2, 4 or 8
                       bool  data_signed,
                       bool  store,       // false = load, true = store
                       bool  is_int)      // true = X, false = F
{
  if (is_int)
  {
    if (store)
    {
      // STR (immediate) STRB STRH
      // store item at address (base + offset)
      // returns false if offset is not encodable
      assert c_store_ofs12 (source     => (REG)source, // ZERO allowed (SP not allowed)
                            base       => base,        // SP allowed (ZERO not allowed)
                            offset     => offset,      // 12 bits (0 to 4095 * size) to be added to base address
                            data_size  => data_size);  // 1, 2, 4 or 8
    }
    else
    {
      assert c_load_ofs12 (target       => (REG)source, // ZERO allowed (SP not allowed)
                           base         => base,        // SP allowed (ZERO not allowed)
                           offset       => offset,      // 12 bits (0 to 4095 * size) to be added to base address
                           data_signed  => data_signed,
                           data_size    => data_size);  // 1, 2, 4 or 8
    }
  }
  else
  {
    if (store)
    {
      assert c_fstore_ofs12 (source     => (FREG)source, // ZERO allowed (SP not allowed)
                             base       => base,         // SP allowed (ZERO not allowed)
                             offset     => offset,       // 12 bits (0 to 4095 * size) to be added to base address
                             data_size  => data_size);   // 1, 2, 4 or 8
    }
    else
    {
      assert c_fload_ofs12 (target       => (FREG)source,  // ZERO allowed (SP not allowed)
                            base         => base,          // SP allowed (ZERO not allowed)
                            offset       => offset,        // 12 bits (0 to 4095 * size) to be added to base address
                            data_size    => data_size);    // 1, 2, 4 or 8
    }
  }
}

/************************************************************************/

void load_store_reg_reg (int  source,      // ZERO allowed (SP not allowed)
                         REG  base,        // SP allowed (ZERO not allowed)
                         REG  index,       // ZERO allowed (SP not allowed)
                         bool index_signed,
                         int  index_size,  // 4 or 8
                         bool mult_index_by_data_size,
                         int  data_size,   // 1, 2, 4 or 8
                         bool data_signed,
                         bool store,       // false = load, true = store
                         bool is_int)      // true = X, false = F
{
  if (is_int)
  {
    if (store)
    {
      c_store_reg_reg (source                  => (REG)source,  // ZERO allowed (SP not allowed)
                       base                    => base,         // SP allowed (ZERO not allowed)
                       index                   => index,        // ZERO allowed (SP not allowed)
                       index_signed            => index_signed,
                       index_size              => index_size,   // 4 or 8
                       mult_index_by_data_size => mult_index_by_data_size,
                       data_size               => data_size);   // 1, 2, 4 or 8
    }
    else
    {
      c_load_reg_reg (target                  => (REG)source,   // ZERO allowed (SP not allowed)
                      base                    => base,          // SP allowed (ZERO not allowed)
                      index                   => index,         // ZERO allowed (SP not allowed)
                      index_signed            => index_signed,
                      index_size              => index_size,    // 4 or 8
                      mult_index_by_data_size => mult_index_by_data_size,
                      data_signed             => data_signed,
                      data_size               => data_size);    // 1, 2, 4 or 8
    }
  }
  else
  {
    if (store)
    {
      c_fstore_reg_reg (source                  => (FREG)source,  // ZERO allowed (SP not allowed)
                        base                    => base,          // SP allowed (ZERO not allowed)
                        index                   => index,         // ZERO allowed (SP not allowed)
                        index_signed            => index_signed,
                        index_size              => index_size,    // 4 or 8
                        mult_index_by_data_size => mult_index_by_data_size,
                        data_size               => data_size);    // 1, 2, 4 or 8
    }
    else
    {
      c_fload_reg_reg (target                  => (FREG)source,   // ZERO allowed (SP not allowed)
                       base                    => base,           // SP allowed (ZERO not allowed)
                       index                   => index,          // ZERO allowed (SP not allowed)
                       index_signed            => index_signed,
                       index_size              => index_size,     // 4 or 8
                       mult_index_by_data_size => mult_index_by_data_size,
                       data_size               => data_size);     // 1, 2, 4 or 8
    }
  }
}

/************************************************************************/

// >>>>>>>>>>>>> X16 & X17 used as very temporary registers <<<<<<<<<<<<<<<
// X16 : used for index
// X17 : used for large offset

void load_store_register_from_to_memory (int  r,            // Xr or Fr
                                         EA   ea,
                                         bool data_signed,  // only for load + is_int
                                         int  size,         // 1, 2, 4, 8 for int, 4 or 8 for float
                                         bool store,        // false = load, true = store
                                         bool is_int)       // true = X, false = F
{
  if (ea.reloc.kind == RELOC_NONE)
  {
    if (ea.index == ZERO)
    {
      if (ea.base == ZERO)   // offset only
      {
        move_register_immediate (target  => X17,
                                 imm     => ea.offset,
                                 size    => 8);     // 4 or 8

        load_store_ofs8 (source      => r,         // ZERO allowed (SP not allowed)
                         base        => X17,       // SP allowed (ZERO not allowed)
                         offset      => 0,         // 9 bits (-256 to 255) to be added to base address
                         data_size   => size,      // size of data to load : 1, 2, 4, 8)
                         data_signed => data_signed,
                         store       => store,     // false = load, true = store
                         is_int      => is_int);   // true = X, false = F
      }
      else   // base + offset
      {
        if (ea.offset >= -256 && ea.offset <= 255)   // small, possibly unaligned
        {
          load_store_ofs8 (source      => r,         // ZERO allowed (SP not allowed)
                           base        => ea.base,   // SP allowed (ZERO not allowed)
                           offset      => ea.offset, // 9 bits (-256 to 255) to be added to base address
                           data_size   => size,      // size of data to load : 1, 2, 4, 8)
                           data_signed => data_signed,
                           store       => store,     // false = load, true = store
                           is_int      => is_int);   // true = X, false = F
        }
        else if (ea.offset >= 0 && ea.offset <= 4095 * size && (ea.offset & (size-1)) == 0)  // aligned
        {
          load_store_ofs12 (source      => r,          // ZERO allowed (SP not allowed)
                            base        => ea.base,    // SP allowed (ZERO not allowed)
                            offset      => ea.offset,  // 12 bits (0 to 4095 * size) to be added to base address
                            data_size   => size,       // 1, 2, 4 or 8
                            data_signed => data_signed,
                            store       => store,     // false = load, true = store
                            is_int      => is_int);   // true = X, false = F
        }
        else  // any signed 2GB offset
        {
          if ((ea.offset & (size-1)) == 0)   // ea.offset is aligned at data size
          {
            int shifts = lshifts_of(size);

            // load offset in X17
            move_register_immediate (target  => X17,
                                     imm     => ea.offset >> shifts,  // use smaller constant
                                     size    => 4);     // 4 or 8

            load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                                base                    => ea.base,   // SP allowed (ZERO not allowed)
                                index                   => X17,       // ZERO allowed (SP not allowed)
                                index_signed            => true,
                                index_size              => 4,         // 4 or 8
                                mult_index_by_data_size => true,
                                data_size               => size,      // 1, 2, 4 or 8
                                data_signed             => data_signed,
                                store                   => store,     // false = load, true = store
                                is_int                  => is_int);   // true = X, false = F
          }
          else
          {
            // load offset in X17
            move_register_immediate (target  => X17,
                                     imm     => ea.offset,
                                     size    => 4);     // 4 or 8

            load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                                base                    => ea.base,   // SP allowed (ZERO not allowed)
                                index                   => X17,       // ZERO allowed (SP not allowed)
                                index_signed            => true,
                                index_size              => 4,         // 4 or 8
                                mult_index_by_data_size => false,
                                data_size               => size,      // 1, 2, 4 or 8
                                data_signed             => data_signed,
                                store                   => store,     // false = load, true = store
                                is_int                  => is_int);   // true = X, false = F
          }
        }
      }
    }
    else   // index != ZERO
    {
      // base + scale*index + offset     base can be ZERO or SP or Xi,  index is not ZERO

      if (ea.scale == 1)   // base + index + offset       base can be ZERO or SP or Xi
      {
        if (ea.base == ZERO)   // index + offset
        {
abort;

#if 0        
          move_register_immediate (target  => X17,
                                   imm     => ea.offset,
                                   size    => 8);     // 4 or 8

          load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                              base                    => X17,       // SP allowed (ZERO not allowed)
                              index                   => ea.index,  // ZERO allowed (SP not allowed)
                              index_signed            => true,
                              index_size              => 4,         // 4 or 8
                              mult_index_by_data_size => false,
                              data_size               => size,      // 1, 2, 4 or 8
                              data_signed             => data_signed,
                              store                   => store,     // false = load, true = store
                              is_int                  => is_int);   // true = X, false = F
#endif                              
        }
        else   // base (can be SP) + index + offset
        {
          if (ea.offset == 0)    // base + index
          {
            load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                                base                    => ea.base,   // SP allowed (ZERO not allowed)
                                index                   => ea.index,  // ZERO allowed (SP not allowed)
                                index_signed            => true,
                                index_size              => 4,         // 4 or 8
                                mult_index_by_data_size => false,
                                data_size               => size,      // 1, 2, 4 or 8
                                data_signed             => data_signed,
                                store                   => store,     // false = load, true = store
                                is_int                  => is_int);   // true = X, false = F
          }
          else   // base + index + offset
          {
            add_offset_using_x17 (target => X17,        // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => ea.index,   // size 4    ZERO NOT ALLOWED !  X17 not allowed
                                  offset => ea.offset,
                                  size   => 4);

            load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                                base                    => ea.base,   // SP allowed (ZERO not allowed)
                                index                   => X17,       // ZERO allowed (SP not allowed)
                                index_signed            => true,
                                index_size              => 4,         // 4 or 8
                                mult_index_by_data_size => false,
                                data_size               => size,      // 1, 2, 4 or 8
                                data_signed             => data_signed,
                                store                   => store,     // false = load, true = store
                                is_int                  => is_int);   // true = X, false = F
          }
        }
      }
      else if (ea.scale == size)    // special case : scale == size
      {
        // base + scale*index + offset    base can be ZERO or SP or Xi,  index != ZERO,  scale == size

        if (ea.base == ZERO)   // scale*index + offset
        {
abort;

#if 0        
          move_register_immediate (target  => X17,
                                   imm     => ea.offset,
                                   size    => 8);     // 4 or 8

          load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                              base                    => X17,       // SP allowed (ZERO not allowed)
                              index                   => ea.index,  // ZERO allowed (SP not allowed)
                              index_signed            => true,
                              index_size              => 4,         // 4 or 8
                              mult_index_by_data_size => true,
                              data_size               => size,      // 1, 2, 4 or 8
                              data_signed             => data_signed,
                              store                   => store,     // false = load, true = store
                              is_int                  => is_int);   // true = X, false = F
#endif                              
        }
        else    // base + scale*index + offset     index != ZERO,  scale == size
        {
          if (ea.offset == 0)     // base + scale*index      no offset,  scale == size
          {
            load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                                base                    => ea.base,   // SP allowed (ZERO not allowed)
                                index                   => ea.index,  // ZERO allowed (SP not allowed)
                                index_signed            => true,
                                index_size              => 4,         // 4 or 8
                                mult_index_by_data_size => true,
                                data_size               => size,      // 1, 2, 4 or 8
                                data_signed             => data_signed,
                                store                   => store,     // false = load, true = store
                                is_int                  => is_int);   // true = X, false = F
          }
          else     // base + scale*index + offset     index != ZERO,  scale == size
          {
            add_offset_using_x17 (target => X17,       // size 8                  ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => ea.base,   // size 8    SP ALLOWED    ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => ea.offset,
                                  size   => 8);

            load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                                base                    => X17,       // SP allowed (ZERO not allowed)
                                index                   => ea.index,  // ZERO allowed (SP not allowed)
                                index_signed            => true,
                                index_size              => 4,         // 4 or 8
                                mult_index_by_data_size => true,
                                data_size               => size,      // 1, 2, 4 or 8
                                data_signed             => data_signed,
                                store                   => store,     // false = load, true = store
                                is_int                  => is_int);   // true = X, false = F
          }
        }
      }
      else    // index != ZERO,  scale != 1 and scale != size
      {
        // base + scale*index + offset    base can be ZERO or SP or Xi,  index != ZERO,  scale != 1 and scale != size

        if ((ea.scale & (ea.scale-1)) == 0)   // power of 2
        {
          // compute w17 = scale * index + offset
          move_register_immediate (target  => X17,
                                   imm     => ea.offset,
                                   size    => 4);     // 4 or 8

          c_add_reg_reg (target              => X17,                          // ZERO allowed (SP not allowed)
                         source1             => X17,                          // ZERO allowed (SP not allowed)
                         source2             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,                          // LSL, LSR, ASR
                         source2_shift_value => (uint)lshifts_of(ea.scale),   // range 0..31 (or 0..63 for size==8)
                         size                => 4);                           // 4 or 8
        }
        else if (((ea.scale-1) & (ea.scale-2)) == 0)   // (power of 2) + 1
        {
          // compute w17 = scale * index + offset
          c_add_reg_reg (target              => X16,                          // ZERO allowed (SP not allowed)
                         source1             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,                          // LSL, LSR, ASR
                         source2_shift_value => (uint)lshifts_of(ea.scale-1), // range 0..31 (or 0..63 for size==8)
                         size                => 4);                           // 4 or 8

          add_offset_using_x17 (target  => X17,    // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                source  => X16,    // size 4    ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                offset  => ea.offset,
                                size    => 4);
        }
        else   // scale is not power of 2
        {
          move_register_immediate (target => X16,
                                   imm    => ea.scale,
                                   size   => 4);      // 4 or 8

          move_register_immediate (target  => X17,
                                   imm     => ea.offset,
                                   size    => 4);     // 4 or 8

          c_mult_add (target    => X17,      // ZERO allowed (SP not allowed)
                      sum       => X17,      // ZERO allowed (SP not allowed)
                      mul1      => ea.index, // ZERO allowed (SP not allowed)
                      mul2      => X16,      // ZERO allowed (SP not allowed)
                      data_size => 4);       // 4 or 8
        }

        if (ea.base == ZERO)   // W17 = scale * index + offset
        {
abort;

#if 0
          // note that there is no real base address here, this should never happen. X17 is signed.
        
          load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                              base                    => X17,       // SP allowed (ZERO not allowed)
                              index                   => ZERO,      // ZERO allowed (SP not allowed)
                              index_signed            => true,
                              index_size              => 4,         // 4 or 8
                              mult_index_by_data_size => false,
                              data_size               => size,      // 1, 2, 4 or 8
                              data_signed             => data_signed,
                              store                   => store,     // false = load, true = store
                              is_int                  => is_int);   // true = X, false = F
#endif                              
        }
        else
        {
          load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                              base                    => ea.base,   // SP allowed (ZERO not allowed)
                              index                   => X17,       // ZERO allowed (SP not allowed)
                              index_signed            => true,
                              index_size              => 4,         // 4 or 8
                              mult_index_by_data_size => false,
                              data_size               => size,      // 1, 2, 4 or 8
                              data_signed             => data_signed,
                              store                   => store,     // false = load, true = store
                              is_int                  => is_int);   // true = X, false = F
        }
      }
    }
  }
  else if (ea.reloc.kind == RELOC_FUNC   || ea.reloc.kind == RELOC_POOL ||
           ea.reloc.kind == RELOC_GLOBAL || ea.reloc.kind == RELOC_DLL)
  {
    int8 nr = ea.reloc.nr;

    if (ea.reloc.kind == RELOC_DLL)    // register DLL in elf file
    {
      string^ dll, func;
      get_dll_name ((uint4)ea.reloc.nr, out dll, out func);
      nr = fixup.register_imported_shared_object_and_func (dll^, func^);
    }

    /* example for global/constant:
         adrp x0,page21        ; load page rel to PC (for code/data/bss segment)
         ldrb x0,[x0+offset12] ; load byte */

    register_reloc (kind                => ea.reloc.kind,  // RELOC_FUNC, RELOC_POOL, RELOC_GLOBAL or RELOC_DLL
                    nr                  => nr,
                    typ                 => ADRP_PAGE_4K,
                    extra_offset        => ea.offset,
                    data_size_shifts    => 0,
                    fill_position       => blob_index (g_blob_code));

    // .base + .index * .scale + .offset

    // ADRP : target = PC + 4K-offset (+/- 4 GB range)
    // load 4K page address in register
    c_adrp (target => X17,   // ZERO allowed (SP not allowed)
            imm    => 0);    // 21 bit (will be shifted 12 bits to the left)

    // there is always an offset, and it's always in range 0 .. 4095 because the page was set earlier

    if (ea.base == ZERO && ea.index == ZERO)    // .offset only (simple global variable or pool constant)
    {
      if ((ea.offset & (size-1)) == 0)  // aligned
      {
        register_reloc (kind                => ea.reloc.kind,
                        nr                  => nr,
                        typ                 => LOAD_STORE_OFFSET_4095,
                        extra_offset        => ea.offset,
                        data_size_shifts    => lshifts_of(size),
                        fill_position       => blob_index (g_blob_code));

        load_store_ofs12 (source      => r,          // ZERO allowed (SP not allowed)
                          base        => X17,        // SP allowed (ZERO not allowed)
                          offset      => 0,          // 12 bits (0 to 4095 * size) to be added to base address
                          data_size   => size,       // 1, 2, 4 or 8
                          data_signed => data_signed,
                          store       => store,      // false = load, true = store
                          is_int      => is_int);    // true = X, false = F
        return;
      }
    }

    // 12 bits unsigned 0 to 4096,  patched << 10; NOT scaled by data_size

    register_reloc (kind                => ea.reloc.kind,
                    nr                  => nr,
                    typ                 => ADD_OFFSET_4095,
                    extra_offset        => ea.offset,
                    data_size_shifts    => 0,
                    fill_position       => blob_index (g_blob_code));

    c_add_reg_imm (target     => X17,        // SP allowed (ZERO not allowed)
                   source     => X17,        // SP allowed (ZERO not allowed)
                   imm12      => 0,          // 0 to 4095
                   shl_imm_12 => false,      // true to shift imm12 << 12
                   size       => 8);         // 4 or 8

    // base + index * scale

    if (ea.index == ZERO)
    {
      if (ea.base == ZERO)
      {
        // just X17, unaligned (for aligned cases the shorter form above is used)
        load_store_ofs12 (source      => r,          // ZERO allowed (SP not allowed)
                          base        => X17,        // SP allowed (ZERO not allowed)
                          offset      => 0,          // 12 bits (0 to 4095 * size) to be added to base address
                          data_size   => size,       // 1, 2, 4 or 8
                          data_signed => data_signed,
                          store       => store,     // false = load, true = store
                          is_int      => is_int);   // true = X, false = F
      }
      else   // X17 + base
      {
        load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                            base                    => ea.base,   // SP allowed (ZERO not allowed)
                            index                   => X17,       // ZERO allowed (SP not allowed)
                            index_signed            => false,
                            index_size              => 8,         // 4 or 8
                            mult_index_by_data_size => false,
                            data_size               => size,      // 1, 2, 4 or 8
                            data_signed             => data_signed,
                            store                   => store,     // false = load, true = store
                            is_int                  => is_int);   // true = X, false = F
      }
    }
    else   // X17 + base + index * scale   with index != ZERO
    {
      if (ea.base != ZERO)
      {
        c_add_ext_reg (target          => X17,      // SP allowed (ZERO not allowed)
                       source1         => ea.base,  // SP allowed (ZERO not allowed)
                       source2         => X17,      // ZERO allowed (SP not allowed)
                       source2_size    => 8,        // 1, 2, 4 or 8
                       source2_signed  => false,
                       source2_shl_imm => 0,        // 0 .. 4
                       size            => 8);       // target size (4 or 8)
      }

      if (ea.scale == 1)   // X17 + index
      {
        load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                            base                    => X17,       // SP allowed (ZERO not allowed)
                            index                   => ea.index,  // ZERO allowed (SP not allowed)
                            index_signed            => true,
                            index_size              => 4,         // 4 or 8
                            mult_index_by_data_size => false,
                            data_size               => size,      // 1, 2, 4 or 8
                            data_signed             => data_signed,
                            store                   => store,     // false = load, true = store
                            is_int                  => is_int);   // true = X, false = F
      }
      else if (ea.scale == size)   // X17 + scale*index, with scale == size
      {
        load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                            base                    => X17,       // SP allowed (ZERO not allowed)
                            index                   => ea.index,  // ZERO allowed (SP not allowed)
                            index_signed            => true,
                            index_size              => 4,         // 4 or 8
                            mult_index_by_data_size => true,
                            data_size               => size,      // 1, 2, 4 or 8
                            data_signed             => data_signed,
                            store                   => store,     // false = load, true = store
                            is_int                  => is_int);   // true = X, false = F
      }
      else   // X17 + scale*index, any scale
      {
        if ((ea.scale & (ea.scale-1)) == 0)   // power of 2
        {
          c_add_reg_reg (target              => X16,                          // ZERO allowed (SP not allowed)
                         source1             => ZERO,                         // ZERO allowed (SP not allowed)
                         source2             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,                          // LSL, LSR, ASR
                         source2_shift_value => (uint)lshifts_of(ea.scale),   // range 0..31 (or 0..63 for size==8)
                         size                => 4);                           // 4 or 8
        }
        else if (((ea.scale-1) & (ea.scale-2)) == 0)   // (power of 2) + 1
        {
          // compute w17 = scale * index + offset
          c_add_reg_reg (target              => X16,                          // ZERO allowed (SP not allowed)
                         source1             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,                          // LSL, LSR, ASR
                         source2_shift_value => (uint)lshifts_of(ea.scale-1), // range 0..31 (or 0..63 for size==8)
                         size                => 4);                           // 4 or 8
        }
        else
        {
          move_register_immediate (target => X16,
                                   imm    => ea.scale,
                                   size   => 4);      // 4 or 8

          c_mult_add (target    => X16,      // ZERO allowed (SP not allowed)
                      sum       => ZERO,     // ZERO allowed (SP not allowed)
                      mul1      => ea.index, // ZERO allowed (SP not allowed)
                      mul2      => X16,      // ZERO allowed (SP not allowed)
                      data_size => 4);       // 4 or 8
        }

        load_store_reg_reg (source                  => r,         // ZERO allowed (SP not allowed)
                            base                    => X17,       // SP allowed (ZERO not allowed)
                            index                   => X16,       // ZERO allowed (SP not allowed)
                            index_signed            => true,
                            index_size              => 4,         // 4 or 8
                            mult_index_by_data_size => false,
                            data_size               => size,      // 1, 2, 4 or 8
                            data_signed             => data_signed,
                            store                   => store,     // false = load, true = store
                            is_int                  => is_int);   // true = X, false = F
      }
    }
  }
  else  // not allowed : RELOC_SYSCALL
  {
    abort;
  }
}

/************************************************************************/

// >>>>>>>>>>>>> X16 & X17 used as very temporary registers <<<<<<<<<<<<<<<
// X16 for index
// X17 for large offset

public
void c_store_register_in_memory (REG r,
                                 EA  ea,
                                 int size)  // 1, 2, 4, 8
{
  load_store_register_from_to_memory (r           => (int)r,
                                      ea          => ea,
                                      data_signed => true,
                                      size        => size,
                                      store       => true,
                                      is_int      => true);
}

/************************************************************************/

// >>>>>>>>>>>>> X16 & X17 used as very temporary registers <<<<<<<<<<<<<<<
// X16 for index
// X17 for large offset

public
void c_load_register_from_memory (REG  r,
                                  EA   ea,
                                  bool data_signed,
                                  int  size)  // 1, 2, 4, 8
{
  load_store_register_from_to_memory (r           => (int)r,
                                      ea          => ea,
                                      data_signed => data_signed,
                                      size        => size,
                                      store       => false,
                                      is_int      => true);
}

/************************************************************************/

// >>>>>>>>>>>>> X16 & X17 used as very temporary registers <<<<<<<<<<<<<<<
// X16 for index
// X17 for large offset

public
void c_store_fregister_in_memory (FREG r,
                                  EA   ea,
                                  int  size)  // 4, 8
{
  load_store_register_from_to_memory (r           => (int)r,
                                      ea          => ea,
                                      data_signed => true,
                                      size        => size,
                                      store       => true,
                                      is_int      => false);
}

/************************************************************************/

// >>>>>>>>>>>>> X16 & X17 used as very temporary registers <<<<<<<<<<<<<<<
// X16 for index
// X17 for large offset

public
void c_load_fregister_from_memory (FREG  r,
                                   EA    ea,
                                   int   size)  // 4, 8
{
  load_store_register_from_to_memory (r           => (int)r,
                                      ea          => ea,
                                      data_signed => false,
                                      size        => size,
                                      store       => false,
                                      is_int      => false);
}

/************************************************************************/

public
bool is_null_ea (EA ea)
{
  return ea.base == ZERO &&
         ea.index == ZERO &&
         ea.offset == 0 &&
         ea.reloc.kind == RELOC_NONE;
}

/************************************************************************/

// compute effective address ea and store result in register

// >>>>>>>>>>>>> X16 & X17 used as very temporary registers <<<<<<<<<<<<<<<
// X16 for index
// X17 for large offset

// ! target register must not be used as intermediate register
//   because it can be identical to base or index !

public
void compute_effective_address_in_register (EA ea, REG r)
{
  if (ea.reloc.kind == RELOC_NONE)   // base + scale*index + offset
  {
    if (ea.index == ZERO)   // base + offset
    {
      if (ea.base == ZERO)   // offset
      {
        move_register_immediate (target => r,
                                 imm    => ea.offset,
                                 size   => address_size);   // 4 or 8
      }
      else   // base + offset
      {
        if (r != ea.base || ea.offset != 0)
        {
          add_offset_using_x17 (target => r,         // size 8                  ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                source => ea.base,   // size 8    SP ALLOWED    ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                offset => ea.offset,
                                size   => 8);
        }
      }
    }
    else   // has an index  (base + scale*index + offset)
    {
      if (ea.base == ZERO)   // scale*index + offset,  index != ZERO
      {
abort;

#if 0            
        if (ea.offset == 0)    // scale*index,  index != ZERO
        {
          if (ea.scale == 1)     // index,  index != ZERO
          {
            c_extend_signed (target      => r,       // 8 bytes
                             source      => ea.index,
                             source_size => 4);
          }
          else if (ea.scale < (1<<31) && (ea.scale & (ea.scale-1)) == 0)   // power of 2    scale*index,  index != ZERO
          {
            c_add_reg_reg (target              => r,                            // ZERO allowed (SP not allowed)
                           source1             => ZERO,                         // ZERO allowed (SP not allowed)
                           source2             => ea.index,                     // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,                          // LSL, LSR, ASR
                           source2_shift_value => (uint)lshifts_of(ea.scale),   // range 0..31 (or 0..63 for size==8)
                           size                => 4);                           // 4 or 8
/*
            c_extend_signed (target      => r,     // 8 bytes
                             source      => r,
                             source_size => 4);
*/
          }
          else if (ea.scale < (1<<31) && ((ea.scale-1) & (ea.scale-2)) == 0)   // (power of 2) + 1,  scale*index,  index != ZERO
          {
            c_add_reg_reg (target              => r,                            // ZERO allowed (SP not allowed)
                           source1             => ea.index,                     // ZERO allowed (SP not allowed)
                           source2             => ea.index,                     // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,                          // LSL, LSR, ASR
                           source2_shift_value => (uint)lshifts_of(ea.scale-1), // range 0..31 (or 0..63 for size==8)
                           size                => 4);                           // 4 or 8
/*
            c_extend_signed (target      => r,     // 8 bytes
                             source      => r,
                             source_size => 4);
*/
          }
          else   // any scale    scale*index
          {
            move_register_immediate (target => X16,
                                     imm    => ea.scale,
                                     size   => 4);      // 4 or 8

            c_mult_add (target    => r,        // ZERO allowed (SP not allowed)
                        sum       => ZERO,     // ZERO allowed (SP not allowed)
                        mul1      => ea.index, // ZERO allowed (SP not allowed)
                        mul2      => X16,      // ZERO allowed (SP not allowed)
                        data_size => 4);       // 4 or 8
/*
            c_extend_signed (target      => r,     // 8 bytes
                             source      => r,
                             source_size => 4);
*/
          }
        }
        else     // scale*index + offset,   index != ZERO, offset != 0
        {
          if (ea.scale == 1)     // index + offset,   index != ZERO, offset != 0
          {
            add_offset_using_x17 (target => r,          // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => ea.index,   // size 4    ZERO NOT ALLOWED !  X17 not allowed
                                  offset => ea.offset,
                                  size   => 4);
          }
          else if (ea.scale < (1<<31) && (ea.scale & (ea.scale-1)) == 0)   // power of 2    scale*index + offset
          {
            c_add_reg_reg (target              => r,                            // ZERO allowed (SP not allowed)
                           source1             => ZERO,                         // ZERO allowed (SP not allowed)
                           source2             => ea.index,                     // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,                          // LSL, LSR, ASR
                           source2_shift_value => (uint)lshifts_of(ea.scale),   // range 0..31 (or 0..63 for size==8)
                           size                => 4);                           // 4 or 8

            add_offset_using_x17 (target => r,       // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r,       // size 4    ZERO NOT ALLOWED !  X17 not allowed
                                  offset => ea.offset,
                                  size   => 4);
          }
          else if (ea.scale < (1<<31) && ((ea.scale-1) & (ea.scale-2)) == 0)   // (power of 2) + 1
          {
            c_add_reg_reg (target              => r,                            // ZERO allowed (SP not allowed)
                           source1             => ea.index,                     // ZERO allowed (SP not allowed)
                           source2             => ea.index,                     // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,                          // LSL, LSR, ASR
                           source2_shift_value => (uint)lshifts_of(ea.scale-1), // range 0..31 (or 0..63 for size==8)
                           size                => 4);                           // 4 or 8

            add_offset_using_x17 (target => r,          // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r,          // size 4    ZERO NOT ALLOWED !  X17 not allowed
                                  offset => ea.offset,
                                  size   => 4);
          }
          else   // scale*index + offset,   index != ZERO, offset != 0, any scale
          {
            move_register_immediate (target => X16,
                                     imm    => ea.scale,
                                     size   => 4);      // 4 or 8

            move_register_immediate (target  => X17,
                                     imm     => ea.offset,
                                     size    => 4);     // 4 or 8

            c_mult_add (target    => r,        // ZERO allowed (SP not allowed)
                        sum       => X17,      // ZERO allowed (SP not allowed)
                        mul1      => ea.index, // ZERO allowed (SP not allowed)
                        mul2      => X16,      // ZERO allowed (SP not allowed)
                        data_size => 4);       // 4 or 8
/*
            c_extend_signed (target      => r,     // 8 bytes
                             source      => r,
                             source_size => 4);
*/
          }
        }
#endif        
        
      }
      else   //  (base + scale*index + offset)    base != ZERO, index != ZERO
      {
        if (ea.scale == 1)     // base + index + offset,   base != ZERO, index != ZERO, offset != 0
        {
          add_offset_using_x17 (target => X17,        // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                source => ea.index,   // size 4    ZERO NOT ALLOWED !  X17 not allowed
                                offset => ea.offset,
                                size   => 4);

          c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                         source1         => ea.base,  // SP allowed (ZERO not allowed)
                         source2         => X17,      // ZERO allowed (SP not allowed)
                         source2_size    => 4,        // 1, 2, 4 or 8
                         source2_signed  => true,
                         source2_shl_imm => 0,        // 0 .. 4
                         size            => 8);       // target size (4 or 8)
        }
        else if ((ea.scale & (ea.scale-1)) == 0)   // power of 2    scale*index + offset
        {
          c_add_reg_reg (target              => X16,                          // ZERO allowed (SP not allowed)
                         source1             => ZERO,                         // ZERO allowed (SP not allowed)
                         source2             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,                          // LSL, LSR, ASR
                         source2_shift_value => (uint)lshifts_of(ea.scale),   // range 0..31 (or 0..63 for size==8)
                         size                => 4);                           // 4 or 8

          add_offset_using_x17 (target => X17,     // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                source => X16,     // size 4    ZERO NOT ALLOWED !  X17 not allowed
                                offset => ea.offset,
                                size   => 4);

          c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                         source1         => ea.base,  // SP allowed (ZERO not allowed)
                         source2         => X17,      // ZERO allowed (SP not allowed)
                         source2_size    => 4,        // 1, 2, 4 or 8
                         source2_signed  => true,
                         source2_shl_imm => 0,        // 0 .. 4
                         size            => 8);       // target size (4 or 8)
        }
        else if (((ea.scale-1) & (ea.scale-2)) == 0)   // (power of 2) + 1
        {
          c_add_reg_reg (target              => X16,                          // ZERO allowed (SP not allowed)
                         source1             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2             => ea.index,                     // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,                          // LSL, LSR, ASR
                         source2_shift_value => (uint)lshifts_of(ea.scale-1), // range 0..31 (or 0..63 for size==8)
                         size                => 4);                           // 4 or 8

          add_offset_using_x17 (target => X17,          // size 4    ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                source => X16,          // size 4    ZERO NOT ALLOWED !  X17 not allowed
                                offset => ea.offset,
                                size   => 4);

          c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                         source1         => ea.base,  // SP allowed (ZERO not allowed)
                         source2         => X17,      // ZERO allowed (SP not allowed)
                         source2_size    => 4,        // 1, 2, 4 or 8
                         source2_signed  => true,
                         source2_shl_imm => 0,        // 0 .. 4
                         size            => 8);       // target size (4 or 8)
        }
        else   // base + scale*index + offset,   base != ZERO, index != ZERO, offset != 0, any scale
        {
          move_register_immediate (target => X16,
                                   imm    => ea.scale,
                                   size   => 4);      // 4 or 8

          move_register_immediate (target  => X17,
                                   imm     => ea.offset,
                                   size    => 4);     // 4 or 8

          c_mult_add (target    => X17,      // ZERO allowed (SP not allowed)
                      sum       => X17,      // ZERO allowed (SP not allowed)
                      mul1      => ea.index, // ZERO allowed (SP not allowed)
                      mul2      => X16,      // ZERO allowed (SP not allowed)
                      data_size => 4);       // 4 or 8

          c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                         source1         => ea.base,  // SP allowed (ZERO not allowed)
                         source2         => X17,      // ZERO allowed (SP not allowed)
                         source2_size    => 4,        // 1, 2, 4 or 8
                         source2_signed  => true,
                         source2_shl_imm => 0,        // 0 .. 4
                         size            => 8);       // target size (4 or 8)
        }
      }
    }
  }
  else if (ea.reloc.kind == RELOC_FUNC   || ea.reloc.kind == RELOC_POOL ||
           ea.reloc.kind == RELOC_GLOBAL || ea.reloc.kind == RELOC_DLL)
  {
    int8 nr = ea.reloc.nr;

    if (ea.reloc.kind == RELOC_DLL)    // register DLL in elf file
    {
      string^ dll, func;
      get_dll_name ((uint4)ea.reloc.nr, out dll, out func);
      nr = fixup.register_imported_shared_object_and_func (dll^, func^);  // nr means now slot nr
    }

    register_reloc (kind                => ea.reloc.kind,  // RELOC_FUNC, RELOC_POOL, RELOC_GLOBAL or RELOC_DLL
                    nr                  => nr,
                    typ                 => ADRP_PAGE_4K,
                    extra_offset        => ea.offset,
                    data_size_shifts    => 0,
                    fill_position       => blob_index (g_blob_code));

    // .base + .index * .scale + .offset

    // ADRP : target = PC + 4K-offset (+/- 4 GB range)
    // load 4K page address in register
    c_adrp (target => X17,   // ZERO allowed (SP not allowed)
            imm    => 0);    // 21 bit (will be shifted 12 bits to the left)

    // there is always an offset, and it's always in range 0 .. 4095 because the page was set earlier
    // 12 bits unsigned 0 to 4096,  patched << 10; NOT scaled by data_size

    // X17 + base + scale*index

    // add offset

    if (ea.index == ZERO && ea.base == ZERO)
    {
      register_reloc (kind                => ea.reloc.kind,
                      nr                  => nr,
                      typ                 => ADD_OFFSET_4095,
                      extra_offset        => ea.offset,
                      data_size_shifts    => 0,
                      fill_position       => blob_index (g_blob_code));

      c_add_reg_imm (target     => r,          // SP allowed (ZERO not allowed)
                     source     => X17,        // SP allowed (ZERO not allowed)
                     imm12      => 0,          // 0 to 4095
                     shl_imm_12 => false,      // true to shift imm12 << 12
                     size       => 8);         // 4 or 8
      return;
    }


    // add offset

    register_reloc (kind                => ea.reloc.kind,
                    nr                  => nr,
                    typ                 => ADD_OFFSET_4095,
                    extra_offset        => ea.offset,
                    data_size_shifts    => 0,
                    fill_position       => blob_index (g_blob_code));

    c_add_reg_imm (target     => X17,        // SP allowed (ZERO not allowed)
                   source     => X17,        // SP allowed (ZERO not allowed)
                   imm12      => 0,          // 0 to 4095
                   shl_imm_12 => false,      // true to shift imm12 << 12
                   size       => 8);         // 4 or 8


    // X17 + base + scale*index

    if (ea.index == ZERO & ea.base != ZERO)    // X17 + base
    {
      c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                     source1         => ea.base,  // SP allowed (ZERO not allowed)
                     source2         => X17,      // ZERO allowed (SP not allowed)
                     source2_size    => 8,        // 1, 2, 4 or 8
                     source2_signed  => false,
                     source2_shl_imm => 0,        // 0 .. 4
                     size            => 8);       // target size (4 or 8)
      return;
    }


    // X17 + base + scale*index,     index != ZERO

    if (ea.base != ZERO)
    {
      c_add_ext_reg (target          => X17,        // SP allowed (ZERO not allowed)
                     source1         => ea.base,  // SP allowed (ZERO not allowed)
                     source2         => X17,      // ZERO allowed (SP not allowed)
                     source2_size    => 8,        // 1, 2, 4 or 8
                     source2_signed  => false,
                     source2_shl_imm => 0,        // 0 .. 4
                     size            => 8);       // target size (4 or 8)
    }

    // X17 + scale*index,     index != ZERO

    if (ea.scale == 1)     // X17 + index,    index != ZERO
    {
      c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                     source1         => X17,      // SP allowed (ZERO not allowed)
                     source2         => ea.index, // ZERO allowed (SP not allowed)
                     source2_size    => 4,        // 1, 2, 4 or 8
                     source2_signed  => true,
                     source2_shl_imm => 0,        // 0 .. 4
                     size            => 8);       // target size (4 or 8)
    }
    else if ((ea.scale & (ea.scale-1)) == 0)   // power of 2 : X17 + scale*index
    {
      c_add_reg_reg (target              => X16,                          // ZERO allowed (SP not allowed)
                     source1             => ZERO,                         // ZERO allowed (SP not allowed)
                     source2             => ea.index,                     // ZERO allowed (SP not allowed)
                     source2_shift_type  => LSL,                          // LSL, LSR, ASR
                     source2_shift_value => (uint)lshifts_of(ea.scale),   // range 0..31 (or 0..63 for size==8)
                     size                => 4);                           // 4 or 8

      c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                     source1         => X17,      // SP allowed (ZERO not allowed)
                     source2         => X16,      // ZERO allowed (SP not allowed)
                     source2_size    => 4,        // 1, 2, 4 or 8
                     source2_signed  => true,
                     source2_shl_imm => 0,        // 0 .. 4
                     size            => 8);       // target size (4 or 8)
    }
    else if (((ea.scale-1) & (ea.scale-2)) == 0)   // (power of 2) + 1 : X17 + scale*index
    {
      c_add_reg_reg (target              => X16,                          // ZERO allowed (SP not allowed)
                     source1             => ea.index,                     // ZERO allowed (SP not allowed)
                     source2             => ea.index,                     // ZERO allowed (SP not allowed)
                     source2_shift_type  => LSL,                          // LSL, LSR, ASR
                     source2_shift_value => (uint)lshifts_of(ea.scale-1), // range 0..31 (or 0..63 for size==8)
                     size                => 4);                           // 4 or 8

      c_add_ext_reg (target          => r,        // SP allowed (ZERO not allowed)
                     source1         => X17,      // SP allowed (ZERO not allowed)
                     source2         => X16,      // ZERO allowed (SP not allowed)
                     source2_size    => 4,        // 1, 2, 4 or 8
                     source2_signed  => true,
                     source2_shl_imm => 0,        // 0 .. 4
                     size            => 8);       // target size (4 or 8)
    }
    else   // X17 + scale*index,   index != ZERO, any scale
    {
      move_register_immediate (target => X16,
                               imm    => ea.scale,
                               size   => 4);      // 4 or 8

      c_mult_add (target    => X16,      // ZERO allowed (SP not allowed)
                  sum       => ZERO,     // ZERO allowed (SP not allowed)
                  mul1      => ea.index, // ZERO allowed (SP not allowed)
                  mul2      => X16,      // ZERO allowed (SP not allowed)
                  data_size => 4);       // 4 or 8

      c_add_ext_reg (target          => r,      // SP allowed (ZERO not allowed)
                     source1         => X17,    // SP allowed (ZERO not allowed)
                     source2         => X16,    // ZERO allowed (SP not allowed)
                     source2_size    => 4,      // 1, 2, 4 or 8
                     source2_signed  => true,
                     source2_shl_imm => 0,      // 0 .. 4
                     size            => 8);     // target size (4 or 8)
    }
  }
  else  // not allowed : RELOC_SYSCALL
  {
    abort;
  }
}

/************************************************************************/

// for istack or astack.
// returns true if at least one register was freed.

bool store_xnode_in_temp (ref NODE n)
{
  if (n.kind == INT_REGISTER)
  {
    REG r;
    int size;

    r    = n.reg;
    size = size_of_operand (n);   // 1, 4 or 8

    set_node_to_new_temporary_memory (ref n, size);

    c_store_register_in_memory (r, n.ea, size);

    return true;  // the node does not use any registers anymore (except fp)
  }
  else if (n.kind == EFFECTIVE_ADDRESS)
  {
    if (n.ea.base <= X28 || n.ea.index != ZERO)   // some register is used for indexing (it's not a simple FP or SP)
    {
      compute_effective_address_in_register (n.ea, X16);
      set_node_to_new_temporary_memory (ref n, size => 8);
      c_store_register_in_memory (X16, n.ea, size => 8);   // X16 can be used because ea.index is ZERO

      return true;  // the node does not use any registers anymore (except fp)
    }
  }
  else if (n.kind == MEMORY)    // load operand and store it in temp
  {
    if (n.ea.base <= X28 || n.ea.index != ZERO)   // some register is used for indexing (it's not a simple FP or SP)
    {
      int size = size_of_operand (n);   // 1, 4 or 8;

      c_load_register_from_memory (r           => X16,
                                   ea          => n.ea,
                                   data_signed => true,
                                   size        => size);   // 1, 4 or 8

      set_node_to_new_temporary_memory (ref n, size => size);

      c_store_register_in_memory (r    => X16,    // X16 can be used because n.ea.index is ZERO
                                  ea   => n.ea,
                                  size => size);

      return true;  // the node does not use any registers anymore (except none, rbp, rsp)
    }
  }

  return false;  // the node DOES NOT use any registers (except fp, sp)
}

/***********************************************************************************/

// for fstack.
// returns true if at least one register was freed.

bool store_fnode_in_temp (ref NODE n)
{
  if (n.kind == FLOAT_REGISTER)
  {
    FREG r;
    int  size;

    r    = n.freg;
    size = size_of_operand (n);   // 4 or 8

    set_node_to_new_temporary_memory (ref n, size);

    c_store_fregister_in_memory (r, n.ea, size);

    return true;  // the node does not use any registers anymore (except fp)
  }

  return false;  // the node DOES NOT use any registers (except fp, sp)
}

/***********************************************************************************/

void compute_dont_touch_indexes (out int pdont_touch_i, out int pdont_touch_f, out int pdont_touch_a)
{
  pdont_touch_i = 0;
  pdont_touch_f = 0;
  pdont_touch_a = 0;

  {
    ref string input = pcode_table[(int)g_current_pcode].input;
    int  j = 0;

    while (j < input'length)
    {
      char typ = input[j];

      if (typ == 'b' || typ == 'i' || typ == 'l')
        pdont_touch_i++;
      else if (typ == 'f' || typ == 'd')
        pdont_touch_f++;
      else if (typ == 'a')
        pdont_touch_a++;
      else
        abort;

      j++;
    }
  }
}

/***********************************************************************************/

public void set_hint_x (REG r)
{
  hint_x = r;
}

/***********************************************************************************/

public void set_hint_f (FREG r)
{
  hint_f = r;
}

/***********************************************************************************/

public void clear_hints ()
{
  hint_x = X0;
  hint_f = F0;
}

/***********************************************************************************/

// allocate registers except those used in operands of current pcode.
// free some registers of other nodes if needed.

public
void allocate_registers (out REG  reg[],
                             NODE crash_node = NULL_CRASH_NODE) // allow reuse of registers of this node as it will be erased
{
  int dont_touch_i, dont_touch_f, dont_touch_a;

  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);

  {
    int i;

    i = 0;   // start at oldest nodes

    for (;;)
    {
      if (get_free_registers (out reg, crash_node) == 0)
        return;

      // there are no free registers :
      // we have to get rid of a register of any node outside the current pcode's arguments.

      if (i >= istack_count - dont_touch_i &&
          i >= fstack_count - dont_touch_f &&
          i >= astack_count - dont_touch_a)
      {
        abort;   // we stored all registers and there is still nothing free ? not possible
      }

      if (i < istack_count - dont_touch_i)
      {
        if (store_xnode_in_temp (ref istack[i]))
          continue;
      }

      if (i < fstack_count - dont_touch_f)
      {
        if (store_xnode_in_temp (ref fstack[i]))   // ea can use Xn registers
          continue;
      }

      if (i < astack_count - dont_touch_a)
      {
        if (store_xnode_in_temp (ref astack[i]))
          continue;
      }

      i++;   // try next triplet of nodes
    }
  }
}

/***********************************************************************************/

public
REG allocate_register (NODE crash_node = NULL_CRASH_NODE)
{
  REG[1] reg;
  allocate_registers (out reg, crash_node);
  return reg[0];
}

/***********************************************************************************/

// allocate registers except those used in operands of current pcode.
// free some registers of other nodes if needed.

public
void allocate_fregisters (out FREG reg[],
                              NODE crash_node = NULL_CRASH_NODE) // allow reuse of registers of this node as it will be erased
{
  int dont_touch_i, dont_touch_f, dont_touch_a;

  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);

  _unused dont_touch_i, dont_touch_a;

  {
    int i;

    i = 0;   // start at oldest nodes

    for (;;)
    {
      if (get_free_fregisters (out reg, crash_node) == 0)
        return;

      // there are no free registers :
      // we have to get rid of a register of any node outside the current pcode's arguments.

      if (i >= fstack_count - dont_touch_f)
      {
        abort;   // we stored all registers and there is still nothing free ? not possible
      }

      if (i < fstack_count - dont_touch_f)
      {
        if (store_fnode_in_temp (ref fstack[i]))
          continue;
      }

      i++;   // try next triplet of nodes
    }
  }
}

/***********************************************************************************/

public
FREG allocate_fregister (NODE crash_node = NULL_CRASH_NODE)
{
  FREG[1] reg;
  allocate_fregisters (out reg, crash_node);
  return reg[0];
}

/***********************************************************************************/

// save all registers (Xn and Fn) to temporaries except the number of node entries given as parameters.

public
void store_all_registers_in_temporaries_except_some_nodes (int dont_touch_i, int dont_touch_f, int dont_touch_a)
{
  int i;

  i = 0;

  for (i=0; i<istack_count - dont_touch_i; i++)
    (void)store_xnode_in_temp (ref istack[i]);

  for (i=0; i<fstack_count - dont_touch_f; i++)
  {
    (void)store_xnode_in_temp (ref fstack[i]);
    (void)store_fnode_in_temp (ref fstack[i]);
  }

  for (i=0; i<astack_count - dont_touch_a; i++)
    (void)store_xnode_in_temp (ref astack[i]);
}

/***********************************************************************************/

// save all registers (Xn and Fn) to temporaries
// should be called before calling a function

public
void store_all_registers_in_temporaries ()
{
  store_all_registers_in_temporaries_except_some_nodes (0,0,0);
}

/***********************************************************************************/

// save all registers (Xn and Fn) to temporaries
// should be called before calling a function

public
void store_all_registers_in_temporaries_except_for_this_pcode ()
{
  int dont_touch_i, dont_touch_f, dont_touch_a;
  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);
  store_all_registers_in_temporaries_except_some_nodes (dont_touch_i, dont_touch_f, dont_touch_a);
}

/***********************************************************************************/

// convert 'a' operand from memory into effective address

public
void flush_effective_address (ref NODE a)
{
  REG r;

  if (a.kind == EFFECTIVE_ADDRESS)
    return;

  r = allocate_register (a);   // allows reusing registers of node a

  c_load_register_from_memory (r, a.ea, data_signed => false, size => address_size);

  a.kind = EFFECTIVE_ADDRESS;
  a.ea.base = r;
  a.ea.index = ZERO;
  a.ea.scale = 1;
  a.ea.offset = 0;
  a.ea.reloc.kind = RELOC_NONE;
  a.ea.reloc.nr = 0;
}

/***********************************************************************************/

// test if a register is used in a node

bool reg_used_in_node (REG r, NODE n)
{
  int count[68];

  clear count;

  count_registers_used_in_node (n, ref count, +1);

  return count[(int)r] > 0;
}

/***********************************************************************************/

// test if a register is used in a node

bool freg_used_in_node (FREG r, NODE n)
{
  int count[68];

  clear count;

  count_registers_used_in_node (n, ref count, +1);

  return count[34+(int)r] > 0;
}

/***********************************************************************************/

// this allocates temporary storage

void free_xregister_except_tops (REG r, int dont_touch_i, int dont_touch_f, int dont_touch_a)
{
  int i;

  for (i=0; i<istack_count - dont_touch_i; i++)
    if (reg_used_in_node (r, istack[i]))
      (void)store_xnode_in_temp (ref istack[i]);

  for (i=0; i<fstack_count - dont_touch_f; i++)
    if (reg_used_in_node (r, fstack[i]))
      (void)store_xnode_in_temp (ref fstack[i]);

  for (i=0; i<astack_count - dont_touch_a; i++)
    if (reg_used_in_node (r, astack[i]))
      (void)store_xnode_in_temp (ref astack[i]);
}

/***********************************************************************************/

// this allocates temporary storage

void free_fregister_except_tops (FREG r, int dont_touch_f)
{
  int i;
  for (i=0; i<fstack_count - dont_touch_f; i++)
    if (freg_used_in_node (r, fstack[i]))
      (void)store_fnode_in_temp (ref fstack[i]);
}

/***********************************************************************************/

public
void free_register_except_for_this_pcode (REG r)
{
  int dont_touch_i, dont_touch_f, dont_touch_a;
  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);
  free_xregister_except_tops (r, dont_touch_i, dont_touch_f, dont_touch_a);
}

/***********************************************************************************/

public
void free_fregister_except_for_this_pcode (FREG r)
{
  int dont_touch_i, dont_touch_f, dont_touch_a;
  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);
  _unused dont_touch_i, dont_touch_a;
  free_fregister_except_tops (r, dont_touch_f);
}

/***********************************************************************************/

// for modif means no other node uses this register

public
void flush_uint1_in_register_for_modif (ref NODE n)
{
  REG r;

  if (n.kind == INT_REGISTER && register_usage_count (n.reg) == 1)
    return;

  r = allocate_register (n);   // we can reuse n's registers, if any

  if (n.kind == INT_CONSTANT)
  {
    move_register_immediate (target => r,
                             imm    => n.icte,
                             size   => 4);      // 4 or 8
  }
  else if (n.kind == INT_REGISTER)
  {
    c_mov_reg_reg (target    => r,       // ZERO allowed (SP not allowed)
                   source    => n.reg,   // ZERO allowed (SP not allowed)
                   data_size => 4);      // 4 or 8
  }
  else  // MEMORY
  {
    c_load_register_from_memory (r, n.ea, data_signed => false, size => 1); // size = 1, 2, 4, 8
  }

  n.kind = INT_REGISTER;
  n.reg = r;
}

/***********************************************************************************/

public
void flush_uint1_in_register (ref NODE n)
{
  if (n.kind == INT_REGISTER)
    return;
  flush_uint1_in_register_for_modif (ref n);
}

/***********************************************************************************/

// for modif means no other node uses this register

public
void flush_int4_in_register_for_modif (ref NODE n)
{
  REG r;

  if (n.kind == INT_REGISTER && register_usage_count (n.reg) == 1)
    return;

  r = allocate_register (n);   // we can reuse n's registers, if any

  if (n.kind == INT_CONSTANT)
  {
    move_register_immediate (target => r,
                             imm    => n.icte,
                             size   => 4);      // 4 or 8
  }
  else if (n.kind == INT_REGISTER)
  {
    c_mov_reg_reg (target    => r,       // ZERO allowed (SP not allowed)
                   source    => n.reg,   // ZERO allowed (SP not allowed)
                   data_size => 4);      // 4 or 8
  }
  else  // MEMORY
  {
    c_load_register_from_memory (r, n.ea, data_signed => true, size => 4); // size = 1, 2, 4, 8
  }

  n.kind = INT_REGISTER;
  n.reg = r;
}

/***********************************************************************************/

public
void flush_int4_in_register (ref NODE n)
{
  if (n.kind == INT_REGISTER)
    return;
  flush_int4_in_register_for_modif (ref n);
}

/***********************************************************************************/

// for modif means no other node uses this register

public
void flush_int8_in_register_for_modif (ref NODE n)
{
  REG r;

  if (n.kind == INT_REGISTER && register_usage_count (n.reg) == 1)
    return;

  r = allocate_register (n);   // we can reuse n's registers, if any

  if (n.kind == INT_CONSTANT)
  {
    move_register_immediate (target => r,
                             imm    => n.icte,
                             size   => 8);      // 4 or 8
  }
  else if (n.kind == INT_REGISTER)
  {
    c_mov_reg_reg (target    => r,       // ZERO allowed (SP not allowed)
                   source    => n.reg,   // ZERO allowed (SP not allowed)
                   data_size => 8);      // 4 or 8
  }
  else  // MEMORY
  {
    c_load_register_from_memory (r, n.ea, data_signed => true, size => 8); // size = 1, 2, 4, 8
  }

  n.kind = INT_REGISTER;
  n.reg = r;
}

/***********************************************************************************/

public
void flush_int8_in_register (ref NODE n)
{
  if (n.kind == INT_REGISTER)
    return;
  flush_int8_in_register_for_modif (ref n);
}

/***********************************************************************************/

// for modif means no other node uses this register

public
void flush_float4_in_register_for_modif (ref NODE n)
{
  FREG r;

  if (n.kind == FLOAT_REGISTER && fregister_usage_count (n.freg) == 1)
    return;

  r = allocate_fregister (n);   // we can reuse n's registers, if any

  if (n.kind == FLOAT_CONSTANT)
  {
    move_fregister_immediate (target => r,
                              imm    => n.fcte,
                              size   => 4);      // 4 or 8
  }
  else if (n.kind == FLOAT_REGISTER)
  {
    c_regf_to_regf (target    => r,        // ZERO allowed (SP not allowed)
                    source    => n.freg,   // ZERO allowed (SP not allowed)
                    data_size => 4);       // 4 or 8
  }
  else  // MEMORY
  {
    c_load_fregister_from_memory (r, n.ea, size => 4); // size = 1, 2, 4, 8
  }

  n.kind = FLOAT_REGISTER;
  n.freg = r;
}

/***********************************************************************************/

public
void flush_float4_in_register (ref NODE n)
{
  if (n.kind == FLOAT_REGISTER)
    return;
  flush_float4_in_register_for_modif (ref n);
}

/***********************************************************************************/

// for modif means no other node uses this register

public
void flush_float8_in_register_for_modif (ref NODE n)
{
  FREG r;

  if (n.kind == FLOAT_REGISTER && fregister_usage_count (n.freg) == 1)
    return;

  r = allocate_fregister (n);   // we can reuse n's registers, if any

  if (n.kind == FLOAT_CONSTANT)
  {
    move_fregister_immediate (target => r,
                              imm    => n.fcte,
                              size   => 8);      // 4 or 8
  }
  else if (n.kind == FLOAT_REGISTER)
  {
    c_regf_to_regf (target    => r,        // ZERO allowed (SP not allowed)
                    source    => n.freg,   // ZERO allowed (SP not allowed)
                    data_size => 8);       // 4 or 8
  }
  else  // MEMORY
  {
    c_load_fregister_from_memory (r, n.ea, size => 8); // size = 1, 2, 4, 8
  }

  n.kind = FLOAT_REGISTER;
  n.freg = r;
}

/***********************************************************************************/

public
void flush_float8_in_register (ref NODE n)
{
  if (n.kind == FLOAT_REGISTER)
    return;
  flush_float8_in_register_for_modif (ref n);
}

/***********************************************************************************/

void free_register_except_for_this_node (REG r, NODE n)
{
  int i;

#begin unsafe
  for (i=0; i<istack_count; i++)
    if (&istack[i] != &n && reg_used_in_node (r, istack[i]))
      (void)store_xnode_in_temp (ref istack[i]);

  for (i=0; i<fstack_count; i++)
    if (&fstack[i] != &n && reg_used_in_node (r, fstack[i]))
      (void)store_xnode_in_temp (ref fstack[i]);

  for (i=0; i<astack_count; i++)
    if (&astack[i] != &n && reg_used_in_node (r, astack[i]))
      (void)store_xnode_in_temp (ref astack[i]);
#end unsafe

}

/***********************************************************************************/

void free_fregister_except_for_this_node (FREG r, NODE n)
{
  int i;

#begin unsafe
  for (i=0; i<fstack_count; i++)
    if (&fstack[i] != &n && freg_used_in_node (r, fstack[i]))
      (void)store_fnode_in_temp (ref fstack[i]);
#end unsafe

}

/***********************************************************************************/

public
void load_bool_into_reg (ref NODE i, REG r)
{
  if (i.typ != 'b')
   fatal_compiler_error0 ("load_bool_into_reg()");

  free_register_except_for_this_node (r, i);

  switch (i.kind)
  {
    case INT_CONSTANT:
      move_register_immediate (target => r,
                               imm    => i.icte,
                               size   => 4);    // 4 or 8
      break;

    case INT_REGISTER:
      if (r != i.reg)
        c_mov_reg_reg (target    => r,      // ZERO allowed (SP not allowed)
                       source    => i.reg,  // ZERO allowed (SP not allowed)
                       data_size => 4);     // 4 or 8
      break;

    case MEMORY:
      c_load_register_from_memory (r => r, ea => i.ea, data_signed => false, size => 1); // size = 1, 2, 4, 8
      break;

    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = r;
}

/***********************************************************************************/

public
void load_int4_into_reg (ref NODE i, REG r)    // signed-extend to 8 bytes of register
{
  if (i.typ != 'i')
   fatal_compiler_error0 ("load_int4_into_reg()");

  free_register_except_for_this_node (r, i);

  switch (i.kind)
  {
    case INT_CONSTANT:
      move_register_immediate (target => r,
                               imm    => i.icte,
                               size   => 4);    // 4 or 8
      break;

    case INT_REGISTER:
      if (r != i.reg)
        c_mov_reg_reg (target    => r,      // ZERO allowed (SP not allowed)
                       source    => i.reg,  // ZERO allowed (SP not allowed)
                       data_size => 4);     // 4 or 8
      break;

    case MEMORY:
      c_load_register_from_memory (r => r, ea => i.ea, data_signed => true, size => 4); // size = 1, 2, 4, 8
      break;

    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = r;
}

/***********************************************************************************/

public
void load_uint4_into_reg (ref NODE i, REG r)    // zero-extend to 8 bytes of register
{
  if (i.typ != 'i')
   fatal_compiler_error0 ("load_uint4_into_reg()");

  free_register_except_for_this_node (r, i);

  switch (i.kind)
  {
    case INT_CONSTANT:
      move_register_immediate (target => r,
                               imm    => i.icte,
                               size   => 4);    // 4 or 8
      break;

    case INT_REGISTER:
      if (r != i.reg)
        c_mov_reg_reg (target    => r,      // ZERO allowed (SP not allowed)
                       source    => i.reg,  // ZERO allowed (SP not allowed)
                       data_size => 4);     // 4 or 8
      break;

    case MEMORY:
      c_load_register_from_memory (r => r, ea => i.ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
      break;

    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = r;
}

/***********************************************************************************/

public
void load_int8_into_reg (ref NODE i, REG r)
{
  if (i.typ != 'l')
   fatal_compiler_error0 ("load_int8_into_reg()");

  free_register_except_for_this_node (r, i);

  switch (i.kind)
  {
    case INT_CONSTANT:
      move_register_immediate (target => r,
                               imm    => i.icte,
                               size   => 8);    // 4 or 8
      break;

    case INT_REGISTER:
      if (r != i.reg)
        c_mov_reg_reg (target    => r,      // ZERO allowed (SP not allowed)
                       source    => i.reg,  // ZERO allowed (SP not allowed)
                       data_size => 8);     // 4 or 8
      break;

    case MEMORY:
      c_load_register_from_memory (r => r, ea => i.ea, data_signed => true, size => 8); // size = 1, 2, 4, 8
      break;

    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = r;
}

/***********************************************************************************/

public
void load_float4_into_reg (ref NODE f, FREG r)
{
  if (f.typ != 'f')
   fatal_compiler_error0 ("load_float4_into_reg()");

  free_fregister_except_for_this_node (r, f);

  switch (f.kind)
  {
    case FLOAT_CONSTANT:
      move_fregister_immediate (target => r,
                                imm    => f.fcte,
                                size   => 4);    // 4 or 8
      break;

    case FLOAT_REGISTER:
      if (r != f.freg)
        c_regf_to_regf (target    => r,
                        source    => f.freg,
                        data_size => 4);      // 4 or 8
      break;

    case MEMORY:
      c_load_fregister_from_memory (r => r, ea => f.ea, size => 4); // size = 4, 8
      break;

    default:
      abort;
  }

  f.kind = FLOAT_REGISTER;
  f.freg = r;
}

/***********************************************************************************/

public
void load_float8_into_reg (ref NODE f, FREG r)
{
  if (f.typ != 'd')
   fatal_compiler_error0 ("load_float8_into_reg()");

  free_fregister_except_for_this_node (r, f);

  switch (f.kind)
  {
    case FLOAT_CONSTANT:
      move_fregister_immediate (target => r,
                                imm    => f.fcte,
                                size   => 8);    // 4 or 8
      break;

    case FLOAT_REGISTER:
      if (r != f.freg)
        c_regf_to_regf (target    => r,
                        source    => f.freg,
                        data_size => 8);      // 4 or 8
      break;

    case MEMORY:
      c_load_fregister_from_memory (r => r, ea => f.ea, size => 8); // size = 4, 8
      break;

    default:
      abort;
  }

  f.kind = FLOAT_REGISTER;
  f.freg = r;
}

/***********************************************************************************/

public
void load_addr_into_reg (ref NODE a, REG r)
{
  if (a.typ != 'a')
   fatal_compiler_error0 ("load_addr_into_reg()");

  free_register_except_for_this_node (r, a);

  switch (a.kind)
  {
    case EFFECTIVE_ADDRESS:
      compute_effective_address_in_register (a.ea, r);
      break;

    case MEMORY:
      c_load_register_from_memory (r => r, ea => a.ea, data_signed => false, size => 8); // size = 1, 2, 4, 8
      break;

    default:
      abort;
  }

  a.typ = 'a';
  a.kind = EFFECTIVE_ADDRESS;
  a.ea.base   = r;
  a.ea.index  = ZERO;
  a.ea.scale  = 1;
  a.ea.offset = 0;
  a.ea.reloc.kind = RELOC_NONE;
  a.ea.reloc.nr   = 0;
}

/***********************************************************************************/

void free_param_slot (PARAMETER_STORAGE_LOCATION loc, NODE node, ref NODE[] xstack)
{
  int s;
  for (s=0; s<xstack'length; s++)
  {
    ref NODE slot = xstack[s];

#begin unsafe
    if (&slot == &node)   // skip
      continue;
#end unsafe

    if (loc.type == IN_X_REGISTER)  // check X registers
    {
      if (reg_used_in_node (r => (REG)loc.nr, slot))
        (void)store_xnode_in_temp (ref slot);
    }
    else if (loc.type == IN_F_REGISTER)  // check F registers
    {
      if (freg_used_in_node (r => (FREG)loc.nr, slot))
        (void)store_fnode_in_temp (ref slot);
    }
    else   // check SP memory locations
    {
      if (slot.kind == MEMORY && slot.ea.base == SP && slot.ea.offset == loc.nr)
      {
        // slot in use : load it in X16,
        c_load_register_from_memory (r           => X16,
                                     ea          => slot.ea,
                                     data_signed => false,
                                     size        => size_of_operand (slot)); // size = 1, 2, 4, 8
        slot.kind = INT_REGISTER;
        slot.reg  = X16;

        // then store it in temporary variable
        store_xnode_in_temp (ref slot);
      }
    }
  }
}

/***********************************************************************************/

// check all previous nodes : if any node uses this register or stack location, copy it to temporaries !

public void free_param_slot_for_xstack (PARAMETER_STORAGE_LOCATION loc, NODE node)
{
  free_param_slot (loc, node, ref istack[0 : istack_count]);
  free_param_slot (loc, node, ref fstack[0 : fstack_count]);
  free_param_slot (loc, node, ref astack[0 : astack_count]);
}

/***********************************************************************************/

public
void call_libc (string function_name)
{
  EA ea;

  clear ea;
  ea.base = ZERO;
  ea.index = ZERO;
  ea.scale = 1;
//  ea.offset = 0;
  ea.reloc.kind = RELOC_DLL;
  ea.reloc.nr   = insert_dll_name ("libc.so", function_name);

  c_load_register_from_memory (X8, ea, data_signed => false, size => 8);
  c_jsr_reg (target => X8);
}

/***********************************************************************************/
