
// astacks.c

from std use arithm;
use ../pool, ../goptions, ../error, ../codout;
use a86, as86;

// -------------------------------------------------------------------------------------

int g_temporary_free_offset;

const int MAX_FLOAT_REGISTERS = 8;

// -------------------------------------------------------------------------------------

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

// an EA with base==RSP indicates a temporary storage location
// with offset >= 0, so a few bytes after the stack pointer.

int addr_after_temporary (NODE n)
{
  if (n.kind == MEMORY && n.ea.base == RSP)
    return n.ea.offset + size_of_operand (n);
  return 0;
}

/***********************************************************************************/

// clear the single temporary zone that can be allocated in pcode

public void clear_pcode_temporary_zone ()
{
  g_temporary_free_offset = 0;
}

// -------------------------------------------------------------------------------------

// We compute a free address for a temporary after all other temporaries.
// Note that this uses the current astacks situation,
// so all already allocated temporary variables must be in the 3 astacks.

int addr_after_all_temporaries ()
{
  int offset, i, ofs;

  offset = 0;

  for (i=0; i<istack_count; i++)
  {
    ofs = addr_after_temporary (istack[i]);
    if (ofs > offset)
      offset = ofs;
  }

  for (i=0; i<fstack_count; i++)
  {
    ofs = addr_after_temporary (fstack[i]);
    if (ofs > offset)
      offset = ofs;
  }

  for (i=0; i<astack_count; i++)
  {
    ofs = addr_after_temporary (astack[i]);
    if (ofs > offset)
      offset = ofs;
  }

  if (g_temporary_free_offset > offset)
    offset = g_temporary_free_offset;

  return offset;
}

/***********************************************************************************/

// allocates an offset for a small temporary zone on the stack just after ESP.
// Note that this uses the current astacks situation,
// so all already allocated temporary variables must already be in the 3 astacks.
// Consequently, for each pcode, a temporary zone should be allocated only once and in one block !

int4 allocate_temporary (int size)  // 1, 4, 8, ..
{
  int offset = addr_after_all_temporaries ();

  // align offset

  if (size > 8)       // 9 ..
    offset = (offset + 15) & (-16); // align at 16
  else if (size > 4)  // 5 .. 8
    offset = (offset + 7) & (-8);  // align at 8
  else if (size > 2)  // 3 .. 4
    offset = (offset + 3) & (-4);  // align at 4
  else if (size > 1)  // 2
    offset = (offset + 1) & (-2);  // align at 2


  // allocate temporary at RSP[offset]

  while (offset + size > (int)g_extra_bytes)
  {
    g_extra_bytes += g_stack_alignment;
    g_frame_size  += g_stack_alignment;
  }

  return offset;
}

/***********************************************************************************/

// allocate a single temporary zone for this pcode
// this zone is valid until the pcode terminates.

public int4 allocate_temporary_zone (int size)  // 1, 4, 8, ..
{
  int4 ofs;

  if (g_temporary_free_offset != 0)
    fatal_compiler_error0 ("only one zone allowed");

  ofs = allocate_temporary (size);

  g_temporary_free_offset = ofs + size;

  return ofs;
}

/***********************************************************************************/

// counts how many times each register is used in a node.
// note that a register can be used several times if a node was cloned.

void count_registers_used_in_node (NODE n, ref int count[32], int inc)
{
  switch (n.kind)
  {
    case INT_REGISTER:
      count[(int)n.reg] += inc;
      count[(int)n.reg_high] += inc;
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

void count_register_usage (out int count[32])
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
  int count[32];

  count_register_usage (out count);

  return count[(uint)r];
}

/***********************************************************************************/

// computes a list of 'nb_registers' registers of size 'size' not used in any stack's nodes.
// size : 1, 2, 4 or 8 (8 not allowed in 32-bit)
// allocates RAX, RBX, RCX, RDX, ESI, EDI, (R8..R15 if 64 bit) (32 bit: size==1 does not return RSI, RDI)
// returns 0 if all registers allocated, -1 if we're out of free registers.

int get_free_registers (    int  size,
                            bool with_crash_node, // see next parameter
                            NODE crash_node,     // allow reuse of registers of this node as it will be erased
                            int  nb_registers,    // IN : number of registers you need
                        out REG  regs[])          // OUT : list of registers
{
  int count[32];
  int nb_reg;

  clear regs;
  
  if (size != 1 && size != 2 && size != 4 && size != 8)
    fatal_compiler_error0 ("get_free_registers(1)");

  if (nb_registers < 1)
    fatal_compiler_error0 ("get_free_registers(2)");

  if (size == 8 && address_size == 4)
    fatal_compiler_error0 ("get_free_registers(3)");

  count_register_usage (out count);

  if (with_crash_node)
    count_registers_used_in_node (crash_node, ref count, -1);  // subtract crash_node

  {
    const REG REGS[6+8+1] = {RAX, // preferred register (return value, shorter opcodes)
                             RBX,
                             RSI, RDI,  // 32-bit: don't use ESI, EDI for byte-size
                             RCX, RDX,  // keep free for shifts, divisions
                             R8,  R9,  R10, R11, R12, R13, R14, R15,  // for 64-bit only
                             NONE};
    int i;

    i = 0;
    nb_reg = 0;
    while (nb_reg < nb_registers)
    {
      REG r = REGS[i++];

      if (address_size == 4 && r == R8)
        return -1;   // 32-bit mode : could not reserve enough registers

      if (r == NONE)
        return -1;   // 64-bit mode : could not reserve enough registers

      if (count[(int)r] > 0)   // register is used
        continue;

      if (size == 1 && address_size == 4 && (r == RSI || r == RDI))  // 32-bit: for size 1, not ESI/EDI
        continue;

      regs[nb_reg++] = r;  // store in output
    }
  }

  return 0;   // all registers could be allocated
}

/***********************************************************************************/

void set_node_to_new_temporary_memory (ref NODE n, int size)
{
  int ofs = allocate_temporary (size);   // this must be done BEFORE changing the node !!

  n.kind          = MEMORY;
  n.ea.base       = RSP;
  n.ea.index      = NONE;
  n.ea.scale      = 1;
  n.ea.offset     = ofs;
  n.ea.reloc.kind = RELOC_NONE;
  n.ea.reloc.nr   = 0;
}

/***********************************************************************************/

// for istack or astack.
// there are only mov's and lea's so this doesn't change any flags.
// this allocates temporary variables.
// returns true if at least one register was freed.

public bool store_node_in_temp (ref NODE n)
{
  if (n.kind == INT_REGISTER)
  {
    REG low, high;
    int size;

    low  = n.reg;
    high = n.reg_high;

    size = size_of_operand (n);   // 1, 4 or 8

    set_node_to_new_temporary_memory (ref n, size);

    if (address_size == 4)
    {
      c_mov_mem_reg (n.ea, low, min(size,4));
      if (size == 8)
      {
        n.ea.offset += 4;
        c_mov_mem_reg (n.ea, high, 4);
        n.ea.offset -= 4;
      }
    }
    else
    {
      c_mov_mem_reg (n.ea, low, size);  // 1, 4 or 8
    }

    return true;  // the node does not use any registers anymore (except none, rbp, rsp)
  }
  else if (n.kind == EFFECTIVE_ADDRESS)
  {
    REG r;
    int ofs;

    if (n.ea.base != NONE && n.ea.base != RBP && n.ea.base != RSP)
    {
      r = n.ea.base;
    }
    else
    {
      r = n.ea.index;
    }

    if (r != NONE && r != RBP && r != RSP)  // some register is used for indexing (it's not a simple ebp or rsp or absolute)
    {
      if ((n.ea.base == r && n.ea.index == NONE && n.ea.offset == 0 && n.ea.reloc.kind == RELOC_NONE) ||
          (n.ea.base == NONE && n.ea.index == r && n.ea.scale == 1 && n.ea.offset == 0 && n.ea.reloc.kind == RELOC_NONE))
      {
        set_node_to_new_temporary_memory (ref n, address_size);
        c_mov_mem_reg (n.ea, r, address_size);
      }
      else
      {
        bool save_r;
        EA   save_ea;

        // we will use register r as tempoary for moving the address to memory

        clear save_ea;

        save_r = (register_usage_count (r) > 1);
        if (save_r)    // r is used in several nodes
        {
          ofs = allocate_temporary (address_size * 2);

          save_ea.base       = RSP;
          save_ea.index      = NONE;
          save_ea.scale      = 1;
          save_ea.offset     = ofs + address_size;
          save_ea.reloc.kind = RELOC_NONE;
          save_ea.reloc.nr   = 0;

          c_mov_mem_reg (save_ea, r, address_size);    // save r temporarily in ofs + address_size so we can use it freely
        }
        else
        {
          ofs = allocate_temporary (address_size);     // new RSP temporary for the node
        }

        c_lea_reg_mem (r, n.ea, size => address_size);

        n.kind          = MEMORY;
        n.ea.base       = RSP;
        n.ea.index      = NONE;
        n.ea.scale      = 1;
        n.ea.offset     = ofs;
        n.ea.reloc.kind = RELOC_NONE;
        n.ea.reloc.nr   = 0;

        c_mov_mem_reg (n.ea, r, address_size);    // store effective address r at ofs

        if (save_r)
        {
          c_mov_reg_mem (r, save_ea, address_size);   // restore r
        }
      }

      return true;  // the node does not use any registers anymore (except none, rbp, rsp)
    }
  }
  else if (n.kind == MEMORY)    // load operand and store it in temp
  {
    REG r;

    if (n.ea.base != NONE && n.ea.base != RBP && n.ea.base != RSP)
    {
      r = n.ea.base;
    }
    else
    {
      r = n.ea.index;
    }

    if (r != NONE && r != RBP && r != RSP)  // some register is used for indexing (it's not a simple ebp or rsp or absolute)
    {
      int  size, ofs, padding;
      NODE n2;
      bool save_r;
      EA   save_ea, temp_ea;

      size = size_of_operand (n);   // 1, 4 or 8

      padding = (address_size == 4 && size == 1 && (r == RSI || r == RDI))
              ? 1 : 0;  // 1 means register r not compatible with size 1

     clear save_ea;

      save_r = (register_usage_count(r) > 1);   // register r is used multiple times !

      if (save_r || size > address_size)
      {
        ofs = allocate_temporary (address_size + size + padding);

        save_ea.base       = RSP;
        save_ea.index      = NONE;
        save_ea.scale      = 1;
        save_ea.offset     = ofs;
        save_ea.reloc.kind = RELOC_NONE;
        save_ea.reloc.nr   = 0;

        c_mov_mem_reg (save_ea, r, address_size);   // temporary save r at ofs

        // temp_ea is a temporary variable of size "size + padding"
        clear temp_ea;
        temp_ea.base       = RSP;
        temp_ea.index      = NONE;
        temp_ea.scale      = 1;
        temp_ea.offset     = ofs + address_size;
        temp_ea.reloc.kind = RELOC_NONE;
        temp_ea.reloc.nr   = 0;
      }
      else
      {
        ofs = allocate_temporary (size + padding);

        // temp_ea is a temporary variable of size "size + padding"
        clear temp_ea;
        temp_ea.base       = RSP;
        temp_ea.index      = NONE;
        temp_ea.scale      = 1;
        temp_ea.offset     = ofs;
        temp_ea.reloc.kind = RELOC_NONE;
        temp_ea.reloc.nr   = 0;
      }

      // preparing result node n2
      clear n2;
      n2.typ  = n.typ;
      n2.kind = MEMORY;
      n2.ea = temp_ea;

      if (size <= address_size)   // fits into one register
      {
        if (padding == 1)   // 32-mode only : register SI or DI not compatible with size 1
        {
          // load-extend byte value into full register r
          c_movzx_reg_mem (target => r,    size_target => address_size,
                           source => n.ea, size_source => size);
          c_mov_mem_reg (n2.ea, r, size => 2);   // we padded the temporary earlier from 1 to 2
        }
        else
        {
          c_mov_reg_mem (r, n.ea, size);
          c_mov_mem_reg (n2.ea, r, size);
        }
      }
      else  // 32 bit mode : address_size = 4, size = 8 (r was saved)
      {
        c_mov_reg_mem (r, n.ea, 4);
        c_mov_mem_reg (n2.ea, r, 4);

        c_mov_reg_mem (r, save_ea, address_size);   // reload again r because n->ea uses it as base or index
        n.ea.offset += 4;
        n2.ea.offset += 4;
        c_mov_reg_mem (r, n.ea, 4);
        c_mov_mem_reg (n2.ea, r, 4);

        n.ea.offset -= 4;   // restore offsets
        n2.ea.offset -= 4;
      }

      n = n2;

      if (save_r)
      {
        c_mov_reg_mem (r, save_ea, address_size);   // restore r
      }

      return true;  // the node does not use any registers anymore (except none, rbp, rsp)
    }
  }

  return false;  // the node DOES NOT use any registers (except none, rbp, rsp)
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

// pcodes have max 3 nodes : 2 address + 1 integer
// that could take, assuming EA, EA, EA -> 6 registers, so the maximum of all registers in 32-bit code.

// try allocate registers except those used in operands of current pcode.
// free some registers of other nodes if needed.
// this allocates temporary zones so we must not previously allocate and use temporary zones in this pcode as they will be erased !!
// returns 0 if success, -1 if registers couldn't be allocated

public 
int try_allocate_registers (    int  size,        // 1, 4 or 8 (32-bit: size==1 does not return RSI, RDI)
                                bool with_crash_node,
                                NODE crash_node, // allow reuse of registers of this node as it will be erased
                                int  nb_registers,
                            out REG  reg[])
{
  int dont_touch_i, dont_touch_f, dont_touch_a;

  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);

  {
    int i;

    i = 0;

    for (;;)
    {
      if (get_free_registers (size, with_crash_node, crash_node, nb_registers, out reg) == 0)   // 32-bit: size==1 does not return RSI, RDI.
        return 0;

      // there are no free registers :
      // we have to get rid of a register of any node outside the current pcode's arguments.

      if (i >= istack_count - dont_touch_i &&
          i >= fstack_count - dont_touch_f &&
          i >= astack_count - dont_touch_a)
      {
// it seems this never (rarely) happens in practice
        return -1;
      }

      if (i < istack_count - dont_touch_i)
      {
        if (store_node_in_temp (ref istack[i]))
          continue;
      }

      if (i < fstack_count - dont_touch_f)
      {
        if (store_node_in_temp (ref fstack[i]))
          continue;
      }

      if (i < astack_count - dont_touch_a)
      {
        if (store_node_in_temp (ref astack[i]))
          continue;
      }

      i++;
    }
  }
}

/***********************************************************************************/

// allocate registers except those used in operands of current pcode.
// free some registers of other nodes if needed.
// this allocates temporary zones so we must not previously allocate and use temporary zones in this pcode as they will be erased !!
// should never fail with 6 registers (2 do not support size == 1)

public 
void allocate_registers (    int  size,        // 1, 4 or 8 (32-bit: size==1 does not return RSI, RDI)
                             bool with_crash_node,
                             NODE crash_node, // allow reuse of registers of this node as it will be erased
                             int  nb_registers,
                         out REG  reg[])
{
  if (try_allocate_registers (size, with_crash_node, crash_node, nb_registers, out reg) < 0)
    fatal_compiler_error0 ("allocate_registers(1)");    // cannot allocate registers
}

/***********************************************************************************/

public
REG allocate_register (int  size,        // 1, 4 or 8 (32-bit: size==1 does not return RSI, RDI)
                       bool with_crash_node,
                       NODE crash_node)  // allow reuse of registers of this node as it will be erased
{
  REG[1] reg;
  allocate_registers (size, with_crash_node, crash_node, 1, out reg);
  return reg[0];
}

/***********************************************************************************/

// save all registers to temporaries except the number of node entries given as parameters.
// there are only mov's and lea's so this doesn't change any flags.
// this allocates temporary variables relative to RSP.

public 
void store_all_registers_in_temporaries_except_some_nodes (int dont_touch_i, int dont_touch_f, int dont_touch_a)
{
  int  i;

  i = 0;

  for (i=0; i<istack_count - dont_touch_i; i++)
    (void)store_node_in_temp (ref istack[i]);

  for (i=0; i<fstack_count - dont_touch_f; i++)
    (void)store_node_in_temp (ref fstack[i]);

  for (i=0; i<astack_count - dont_touch_a; i++)
    (void)store_node_in_temp (ref astack[i]);
}

/***********************************************************************************/

// register changed by kernel call

bool register_used_by_kernel (REG r)
{
  return (/*r >= RAX && */ r <= RDX) || (r >= R8 && r <= R11);
}

/***********************************************************************************/

// test if node uses any registers changed by a kernel call

bool node_uses_kernel_registers (NODE n)
{
  switch (n.kind)
  {
    case INT_REGISTER:
      return register_used_by_kernel (n.reg)
          || register_used_by_kernel (n.reg_high);

    case EFFECTIVE_ADDRESS:
    case MEMORY:
      return register_used_by_kernel (n.ea.base)
          || register_used_by_kernel (n.ea.index);

    default:
      return false;
  }
}

/***********************************************************************************/

void store_all_registers_used_by_kernel_call_in_temporaries_except_some_nodes (int dont_touch_i, int dont_touch_f, int dont_touch_a)
{
  int  i;

  i = 0;

  for (i=0; i<istack_count - dont_touch_i; i++)
    if (node_uses_kernel_registers (istack[i]))
      (void)store_node_in_temp (ref istack[i]);

  for (i=0; i<fstack_count - dont_touch_f; i++)
    if (node_uses_kernel_registers (fstack[i]))
      (void)store_node_in_temp (ref fstack[i]);

  for (i=0; i<astack_count - dont_touch_a; i++)
    if (node_uses_kernel_registers (astack[i]))
      (void)store_node_in_temp (ref astack[i]);
}

/***********************************************************************************/

public 
void store_all_registers_in_temporaries ()
{
  store_all_registers_in_temporaries_except_some_nodes (0,0,0);
}

/***********************************************************************************/

// test if a register is used in a node

bool reg_used_in_node (REG r, NODE n)
{
  int count[32];

  clear count;

  count_registers_used_in_node (n, ref count, +1);

  return count[(int)r] > 0;
}

/***********************************************************************************/

// this allocates temporary variables relative to RSP.

void free_register_except_tops (REG r, int dont_touch_i, int dont_touch_f, int dont_touch_a)
{
  int i;

  for (i=0; i<istack_count - dont_touch_i; i++)
    if (reg_used_in_node (r, istack[i]))
      (void)store_node_in_temp (ref istack[i]);

  for (i=0; i<fstack_count - dont_touch_f; i++)
    if (reg_used_in_node (r, fstack[i]))
      (void)store_node_in_temp (ref fstack[i]);

  for (i=0; i<astack_count - dont_touch_a; i++)
    if (reg_used_in_node (r, astack[i]))
      (void)store_node_in_temp (ref astack[i]);
}

/***********************************************************************************/

public 
void free_register_except_for_this_pcode (REG r)
{
  int dont_touch_i, dont_touch_f, dont_touch_a;
  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);
  free_register_except_tops (r, dont_touch_i, dont_touch_f, dont_touch_a);
}

/***********************************************************************************/

// should be called before calling a function

public 
void store_all_registers_in_temporaries_except_for_this_pcode ()
{
  int dont_touch_i, dont_touch_f, dont_touch_a;
  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);
  store_all_registers_in_temporaries_except_some_nodes (dont_touch_i, dont_touch_f, dont_touch_a);
}

/***********************************************************************************/

// should be called before calling a Windows API

public 
void store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ()
{
  int dont_touch_i, dont_touch_f, dont_touch_a;
  compute_dont_touch_indexes (out dont_touch_i, out dont_touch_f, out dont_touch_a);
  store_all_registers_used_by_kernel_call_in_temporaries_except_some_nodes (dont_touch_i, dont_touch_f, dont_touch_a);
}

/***********************************************************************************/

void free_register_except_for_this_node (REG r, NODE n)
{
  int i;

#begin unsafe
  for (i=0; i<istack_count; i++)
    if (&istack[i] != &n && reg_used_in_node (r, istack[i]))
      (void)store_node_in_temp (ref istack[i]);

  for (i=0; i<fstack_count; i++)
    if (&fstack[i] != &n && reg_used_in_node (r, fstack[i]))
      (void)store_node_in_temp (ref fstack[i]);

  for (i=0; i<astack_count; i++)
    if (&astack[i] != &n && reg_used_in_node (r, astack[i]))
      (void)store_node_in_temp (ref astack[i]);
#end unsafe

}

/***********************************************************************************/

public 
void free_register (REG r)
{
  free_register_except_tops (r, 0, 0, 0);
}

/***********************************************************************************/

// save all floating-point registers to temporaries

public 
void store_all_float_registers_in_temporaries ()
{
  int  i;

  for (i=fstack_count-1; i>=0; i--)
  {
    ref NODE f = fstack[i];
    if (f.kind == FLOAT_REGISTER)
    {
      int size, ofs;

      size = size_of_operand (f);   // 4 or 8

      ofs = allocate_temporary (size);

      f.kind          = MEMORY;
      f.ea.base       = RSP;
      f.ea.index      = NONE;
      f.ea.scale      = 1;
      f.ea.offset     = ofs;
      f.ea.reloc.kind = RELOC_NONE;
      f.ea.reloc.nr   = 0;

      c_fstp (f.ea, size);
    }
  }
}

/***********************************************************************************/

// this might cause a storing of all other float registers in temporaries !

public 
int allocate_float_register ()
{
  int i, nr;

  for (i=fstack_count-1; i>=0; i--)
  {
    ref NODE f = fstack[i];

    if (f.kind == FLOAT_REGISTER)
    {
      nr = f.freg;
      if (nr == MAX_FLOAT_REGISTERS-1)   // largest register already allocated
      {
        store_all_float_registers_in_temporaries ();
        return 0;
      }

      return nr+1;
    }
  }

  return 0;
}

/***********************************************************************************/

public 
int nb_float_registers_left ()
{
  int  i, nr;

  for (i=fstack_count-1; i>=0; i--)
  {
    ref NODE f = fstack[i];

    if (f.kind == FLOAT_REGISTER)
    {
      nr = f.freg;
      return MAX_FLOAT_REGISTERS-1 - nr;
    }
  }

  return MAX_FLOAT_REGISTERS;
}

/***********************************************************************************/

public 
void swap_nodes (ref NODE n1, ref NODE n2)
{
  NODE n;
  n  = n1;
  n1 = n2;
  n2 = n;
}

/***********************************************************************************/

bool reg_can_handle_int1 (REG r)
{
  if (address_size == 8)
    return true;
  return r != RSI && r != RDI;
}

/***********************************************************************************/

// for modif means no other node uses this register

public 
void flush_int1_in_register_for_modif (ref NODE n)
{
  REG r;

  if (n.kind == INT_REGISTER && register_usage_count (n.reg) == 1 && reg_can_handle_int1 (n.reg))
    return;

  r = allocate_register (size => 1, true, n);   // we can reuse n's registers if any

  if (n.kind == INT_CONSTANT)
  {
    c_mov_reg_imm (r, n.icte, size => 1);
  }
  else if (n.kind == INT_REGISTER)
  {
    c_mov_reg_reg (r, n.reg, size => 4);   // from non-byte register into byte register
  }
  else  // MEMORY
  {
    c_mov_reg_mem (r, n.ea, size => 1);
  }

  n.kind = INT_REGISTER;
  n.reg = r;
  n.reg_high = NONE;
}

/***********************************************************************************/

public 
void flush_int1_in_register (ref NODE n)
{
  if (n.kind == INT_REGISTER && reg_can_handle_int1 (n.reg))
    return;
  flush_int1_in_register_for_modif (ref n);
}

/***********************************************************************************/

public 
void flush_int4_in_register_for_modif (ref NODE n)
{
  REG r;

  if (n.kind == INT_REGISTER && register_usage_count (n.reg) == 1)
    return;

  r = allocate_register (size => 4, true, n);   // we can reuse n's registers if any

  if (n.kind == INT_CONSTANT)
  {
    c_mov_reg_imm (r, n.icte, size => 4);
  }
  else if (n.kind == INT_REGISTER)
  {
    c_mov_reg_reg (r, n.reg, size => 4);
  }
  else  // MEMORY
  {
    c_mov_reg_mem (r, n.ea, size => 4);
  }

  n.kind = INT_REGISTER;
  n.reg = r;
  n.reg_high = NONE;
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

public 
void flush_int8_in_registers_for_modif (ref NODE n)
{
  if (n.kind == INT_REGISTER &&
      register_usage_count (n.reg) == 1 &&
      (n.reg_high == NONE || register_usage_count (n.reg_high) == 1))
    return;

  if (address_size == 4)
  {
    REG r[2];

    allocate_registers (size => 4, false, n, 2, out r);  // 2 new regs

    if (n.kind == INT_CONSTANT)
    {
      c_mov_reg_imm (r[0], (int)n.icte, size => 4);
      c_mov_reg_imm (r[1], (int)(n.icte >> 32), size => 4);
    }
    else if (n.kind == INT_REGISTER)
    {
      c_mov_reg_reg (r[0], n.reg, size => 4);
      c_mov_reg_reg (r[1], n.reg_high, size => 4);
    }
    else   // MEMORY
    {
      c_mov_reg_mem (r[0], n.ea, size => 4);
      n.ea.offset += 4;
      c_mov_reg_mem (r[1], n.ea, size => 4);
      n.ea.offset -= 4;
    }

    n.kind = INT_REGISTER;
    n.reg = r[0];
    n.reg_high = r[1];
  }
  else  // 64-bit
  {
    REG r;

    r = allocate_register (8, true, n);   // can reuse n

    if (n.kind == INT_CONSTANT)
    {
      c_mov_reg_imm (r, n.icte, size => 8);
    }
    else if (n.kind == INT_REGISTER)
    {
      c_mov_reg_reg (r, n.reg, size => 8);
    }
    else   // MEMORY
    {
      c_mov_reg_mem (r, n.ea, size => 8);
    }

    n.kind = INT_REGISTER;
    n.reg = r;
    n.reg_high = NONE;
  }
}

/***********************************************************************************/

public 
void flush_int8_in_registers (ref NODE n)
{
  if (n.kind == INT_REGISTER)
    return;
  flush_int8_in_registers_for_modif (ref n);
}

/***********************************************************************************/

// convert 'a' operand from memory into effective address

public 
void flush_effective_address (ref NODE a)
{
  REG r;

  if (a.kind == EFFECTIVE_ADDRESS)
    return;

  r = allocate_register (address_size, true, a);   // allows reusing registers of node a

  c_mov_reg_mem (r, a.ea, address_size);

  a.kind = EFFECTIVE_ADDRESS;
  a.ea.base = r;
  a.ea.index = NONE;
  a.ea.scale = 1;
  a.ea.offset = 0;
  a.ea.reloc.kind = RELOC_NONE;
  a.ea.reloc.nr = 0;
}

/***********************************************************************************/

// node f must always be the top-most node
// this might cause a storing of all other float registers in temporaries !

public 
void flush_float_in_register (ref NODE f)
{
  int freg;
  int size;

  if (f.kind == FLOAT_REGISTER)
    return;

  size = f.typ == 'f' ? 4 : 8;

  freg = allocate_float_register ();

  if (f.kind == FLOAT_CONSTANT)
  {
    if (f.fcte == 0.0)
      c_fld_zero ();
    else if (f.fcte == 1.0)
      c_fld_one ();
    else
    {
      EA   ea;
      POOL p = new_pool_constant ((uint)size, (uint)size);

      store_float (p, 0, f.fcte, (uint)size);

      clear ea;
      ea.base   = NONE;
      ea.index  = NONE;
      ea.scale  = 1;
      ea.offset = 0;
      ea.reloc.kind = RELOC_POOL;
      ea.reloc.nr   = serial_nr_of_pool_cte (p);

      c_fld (ea, size);
    }
  }
  else  // MEMORY
  {
    c_fld (f.ea, size);
  }

  f.kind = FLOAT_REGISTER;
  f.freg = freg;
}

/***********************************************************************************/

public 
void push_int4 (NODE i)
{
  switch (i.kind)
  {
    case INT_CONSTANT:
      c_push_imm ((int4)i.icte);
      break;

    case INT_REGISTER:
      c_push_reg (i.reg, address_size);
      break;

    case MEMORY:
      // potential problem in 64 bit here, we push 8 bytes from a 4-byte zone, but it's faster.
      // ok for global variables, local variables on stack, pool constants.
      // can cause trouble if the operating system provides a buffer that is at segment boundary, but that's unlikely.
      c_push_mem (i.ea, address_size);
      break;
      
    default:
      abort;
  }
}

/***********************************************************************************/

public 
void push_int8 (NODE i)
{
  switch (i.kind)
  {
    case INT_CONSTANT:
      if (address_size == 4)
      {
        c_push_imm ((int4)(i.icte >> 32));  // high
        c_push_imm ((int4)i.icte);          // low
      }
      else   // 64-bit
      {
        if (i.icte >= -(int8)2147483648 && i.icte <= 2147483647)   // a small constant
          c_push_imm ((int4)i.icte);   // store 4 bytes sign-extended to 8 bytes
         else
         {
           REG r = allocate_register (address_size, false, i);   // don't reuse registers from node, it has none anyway
           c_mov_reg_imm (r, i.icte, 8);     // 64-bit imm allowed.
           c_push_reg (r, 8);
         }
      }
      break;

    case INT_REGISTER:
      if (address_size == 4)
      {
        c_push_reg (i.reg_high, address_size);    // push high
        c_push_reg (i.reg,      address_size);    // push low
      }
      else
      {
        c_push_reg (i.reg, address_size);  // push 8-byte value
      }
      break;

    case MEMORY:
      if (address_size == 4)
      {
        EA ea = i.ea;
        ea.offset += 4;
        c_push_mem (ea, 4);    // push high
        ea.offset -= 4;
        c_push_mem (ea, 4);    // push low
      }
      else   // 64-bit
      {
        c_push_mem (i.ea, 8);
      }
      break;
      
    default:
      abort;
  }
}

/***********************************************************************************/

public 
void push_addr (NODE a)
{
  switch (a.kind)
  {
    case EFFECTIVE_ADDRESS:
      if (a.ea.base == NONE && a.ea.index == NONE && a.ea.reloc.kind == RELOC_NONE)
      {
        c_push_imm (a.ea.offset);   // null or some small offset
      }
      else if (a.ea.base == NONE && a.ea.index == NONE)
      {
        c_push_imm_reloc (a.ea.offset, a.ea.reloc); // absolute addr, func addr, dll addr, pool cte, ..
      }
      else if (a.ea.base != NONE && a.ea.index == NONE && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        c_push_reg (a.ea.base, address_size);
      }
      else if (a.ea.base == NONE && a.ea.index != NONE && a.ea.scale == 1 && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        c_push_reg (a.ea.index, address_size);
      }
      else   // general case
      {
        REG r;
        r = allocate_register (address_size, true, a);   // we can reuse a's registers
        c_lea_reg_mem (r, a.ea, size => address_size);
        c_push_reg (r, address_size);
      }
      break;

    case MEMORY:
      c_push_mem (a.ea, address_size);
      break;
      
    default:
      abort;
  }
}

/***********************************************************************************/

public 
void push_float (NODE f)
{
  int size;

  size = f.typ == 'f' ? 4 : 8;

  switch (f.kind)
  {
    case FLOAT_CONSTANT:
      if (size == 4)
      {
        float f4 = (float)f.fcte;
        int   i;
        i'byte = f4'byte;
        c_push_imm (i);
      }
      else   // double
      {
        if (address_size == 4)
        {
          int i;
          
          i'byte = f.fcte'byte[4:4];
          c_push_imm (i);  // high
          
          i'byte = f.fcte'byte[0:4];
          c_push_imm (i);    // low
        }
        else   // 64-bit
        {
          REG  r;
          long i8;
          r = allocate_register (address_size, false, f);
          i8'byte = f.fcte'byte;
          c_mov_reg_imm (r, i8, 8);     // 64-bit imm allowed.
          c_push_reg (r, 8);
        }
      }
      break;

    case FLOAT_REGISTER:
      c_push_fltp (size);
      break;

    case MEMORY:
      if (size == 4)
      {
        // potential problem in 64 bit here, if we push 8 bytes from a 4-byte zone, but it's faster.
        // ok for global variables, local variables on stack, pool constants.
        // can cause trouble if the operating system provides a buffer that is at segment boundary, but that's unlikely.
        c_push_mem (f.ea, address_size);
      }
      else  // push double
      {
        if (address_size == 4)
        {
          EA ea = f.ea;
          ea.offset += 4;
          c_push_mem (ea, 4);    // push high
          ea.offset -= 4;
          c_push_mem (ea, 4);    // push low
        }
        else   // 64-bit
        {
          c_push_mem (f.ea, 8);
        }
      }
      break;
      
    default:
      abort;
  }
}

// -----------------------------------------------------------------------

// make sure bool is in register AL.
// (this is used both for function return values and operands of operator ?:)

public 
void sync_bool ()
{
  ref NODE i = istack[istack_count - 1];
  
  switch (i.kind)
  {
    case INT_CONSTANT:
      c_mov_reg_imm (RAX, (int4)i.icte, 1);
      break;

    case INT_REGISTER:
      if (i.reg != RAX)
        c_mov_reg_reg (RAX, i.reg, 1);
      break;

    case MEMORY:
      c_mov_reg_mem (RAX, i.ea, 1);
      break;
  
    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = RAX;
  i.reg_high = NONE;
}

// -----------------------------------------------------------------------

public 
void load_int4_into_reg (ref NODE i, REG r)
{
  if (i.typ != 'i')
   fatal_compiler_error0 ("load_int4_into_reg(1)");

  free_register_except_for_this_node (r, i);

  switch (i.kind)
  {
    case INT_CONSTANT:
      c_mov_reg_imm (r, (int4)i.icte, 4);
      break;

    case INT_REGISTER:
      if (r != i.reg)
        c_mov_reg_reg (r, i.reg, 4);
      break;

    case MEMORY:
      c_mov_reg_mem (r, i.ea, size => 4);
      break;

    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = r;
  i.reg_high = NONE;
}

// -----------------------------------------------------------------------

public 
void load_int8_into_reg_for_64bit (ref NODE i, REG r)
{
  if (i.typ != 'l')
   fatal_compiler_error0 ("load_int8_into_reg_for_64bit(1)");

  if (address_size == 4)
    fatal_compiler_error0 ("load_int8_into_reg_for_64bit(2)");

  free_register_except_for_this_node (r, i);

  switch (i.kind)
  {
    case INT_CONSTANT:
      c_mov_reg_imm (r, i.icte, size => 8);   // 64-bit large imm value
      break;

    case INT_REGISTER:
      if (r != i.reg)
        c_mov_reg_reg (r, i.reg, 8);
      break;

    case MEMORY:
      c_mov_reg_mem (r, i.ea, size => 8);
      break;

    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = r;
  i.reg_high = NONE;
}

// -----------------------------------------------------------------------

public 
void sync_int4 ()    // in EAX
{
  ref NODE i = istack[istack_count - 1];
  load_int4_into_reg (ref i, RAX);
}

// -----------------------------------------------------------------------

public 
void sync_int8 ()     // in EDX:EAX for 32-bit, or RAX for 64-bit
{
  ref NODE i = istack[istack_count - 1];
  switch (i.kind)
  {
    case INT_CONSTANT:
      if (address_size == 4)
      {
        c_mov_reg_imm (RAX, (int4)i.icte, 4);
        c_mov_reg_imm (RDX, (int4)(i.icte >> 32), 4);
      }
      else   // 64-bit
      {
        c_mov_reg_imm (RAX, i.icte, 8);
      }
      break;

    case INT_REGISTER:
      if (address_size == 4)
      {
        if (i.reg == RAX && i.reg_high == RDX)
        {
          // nothing to do
        }
        else if (i.reg == RDX && i.reg_high == RAX)  // both RAX and RDX are used but in wrong order
        {
          c_xchg_reg_reg (RAX, RDX, 4);
        }
        else if (i.reg != RAX && i.reg_high != RAX)  // RAX is free
        {
          if (i.reg != RAX)
            c_mov_reg_reg (RAX, i.reg, 4);

          if (i.reg_high != RDX)
            c_mov_reg_reg (RDX, i.reg_high, 4);
        }
        else if (i.reg != RDX && i.reg_high != RDX)  // RDX is free
        {
          if (i.reg_high != RDX)
            c_mov_reg_reg (RDX, i.reg_high, 4);

          if (i.reg != RAX)
            c_mov_reg_reg (RAX, i.reg, 4);
        }
        else
        {
          fatal_compiler_error0 ("asm(P_RETURN_8)");  // we should never have the same register twice
        }
      }
      else   // 64-bit
      {
        if (i.reg != RAX)
          c_mov_reg_reg (RAX, i.reg, 8);
      }
      break;

    case MEMORY:
      if (address_size == 4)
      {
        if (i.ea.base != RAX && i.ea.index != RAX)
        {
          c_mov_reg_mem (RAX, i.ea, 4);
          i.ea.offset += 4;
          c_mov_reg_mem (RDX, i.ea, 4);
        }
        else if (i.ea.base != RDX && i.ea.index != RDX)
        {
          i.ea.offset += 4;
          c_mov_reg_mem (RDX, i.ea, 4);
          i.ea.offset -= 4;
          c_mov_reg_mem (RAX, i.ea, 4);
        }
        else  // both RAX and RDX are used in effective address
        {
          c_lea_reg_mem (RAX, i.ea, size => address_size);
          i.ea.base = RAX;
          i.ea.index = NONE;
          i.ea.scale = 1;
          i.ea.offset = 0;
          i.ea.reloc.kind = RELOC_NONE;
          i.ea.reloc.nr = 0;

          i.ea.offset += 4;
          c_mov_reg_mem (RDX, i.ea, 4);
          i.ea.offset -= 4;
          c_mov_reg_mem (RAX, i.ea, 4);
        }
      }
      else   // 64-bit
      {
        c_mov_reg_mem (RAX, i.ea, 8);
      }
      break;
  
    default:
      abort;
  }

  i.kind = INT_REGISTER;
  i.reg = RAX;
  i.reg_high = (address_size == 4) ? RDX : NONE;
}

// -----------------------------------------------------------------------

public 
void load_addr_into_reg (ref NODE a, REG r)
{
  if (a.typ != 'a')
   fatal_compiler_error0 ("load_addr_into_reg(1)");

  free_register_except_for_this_node (r, a);

  switch (a.kind)
  {
    case EFFECTIVE_ADDRESS:
      if (a.ea.base == r && a.ea.index == NONE && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        // nothing to do, is already in register
      }
      else if (a.ea.base == NONE && a.ea.index == r && a.ea.scale == 1 && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        // nothing to do, is already in register
      }
      else if (a.ea.base == NONE && a.ea.index == NONE && a.ea.reloc.kind == RELOC_NONE)  // null or some offset
      {
        if (a.ea.offset >= 0)
          c_mov_reg_imm (r, a.ea.offset, 4);   // save 1 byte (we can load into 4 bytes, the high 4 bytes are set to 0 automatically)
        else
          c_mov_reg_imm (r, a.ea.offset, 8);
      }
      else if (a.ea.base == NONE && a.ea.index == NONE)  // RELOC_FUNC, RELOC_DLL, RELOC_POOL, RELOC_GLOBAL
      {
        c_mov_reg_imm_reloc (r, a.ea.offset, address_size, a.ea.reloc);  // addr always inside 2GB code+data image
      }
      else if (a.ea.base != NONE && a.ea.index == NONE && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        if (a.ea.base != r)
          c_mov_reg_reg (r, a.ea.base, address_size);
      }
      else if (a.ea.base == NONE && a.ea.index != NONE && a.ea.scale == 1 && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        if (a.ea.index != r)
          c_mov_reg_reg (r, a.ea.index, address_size);
      }
      else   // general case
      {
        c_lea_reg_mem (r, a.ea, size => address_size);
      }
      break;

    case MEMORY:
      c_mov_reg_mem (r, a.ea, address_size);
      break;
  
    default:
      abort;
  }

  a.kind = EFFECTIVE_ADDRESS;
  a.ea.base = r;
  a.ea.index = NONE;
  a.ea.scale = 1;
  a.ea.offset = 0;
  a.ea.reloc.kind = RELOC_NONE;
  a.ea.reloc.nr = 0;
}

// -----------------------------------------------------------------------

public 
void sync_addr ()     // in RAX
{
  ref NODE a = astack[astack_count - 1];
  load_addr_into_reg (ref a, RAX);
}

// ------------------------------------

public 
void sync_addr_int4 ()     // in RAX=int4, RSI=addr
{
  ref NODE i = istack[istack_count - 1];
  ref NODE a = astack[astack_count - 1];
  
  if (reg_used_in_node (RSI, i))
    free_register (RSI);

  switch (a.kind)
  {
    case EFFECTIVE_ADDRESS:
      if (a.ea.base == NONE && a.ea.index == NONE && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        c_mov_reg_imm (RSI, 0, address_size);
      }
      else if (a.ea.base == NONE && a.ea.index == NONE)
      {
        c_mov_reg_imm_reloc (RSI, a.ea.offset, address_size, a.ea.reloc);
      }
      else if (a.ea.base != NONE && a.ea.index == NONE && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        if (a.ea.base != RSI)
          c_mov_reg_reg (RSI, a.ea.base, address_size);
      }
      else if (a.ea.base == NONE && a.ea.index != NONE && a.ea.scale == 1 &&
               a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE)
      {
        if (a.ea.index != RSI)
          c_mov_reg_reg (RSI, a.ea.index, address_size);
      }
      else   // general case
      {
        c_lea_reg_mem (RSI, a.ea, size => address_size);
      }
      break;

    case MEMORY:
      c_mov_reg_mem (RSI, a.ea, address_size);
      break;

    default:
      abort;
  }

  switch (i.kind)
  {
    case INT_CONSTANT:
      c_mov_reg_imm (RAX, (int4)i.icte, 4);
      break;

    case INT_REGISTER:
      if (i.reg != RAX)
        c_mov_reg_reg (RAX, i.reg, 4);
      break;

    case MEMORY:
      c_mov_reg_mem (RAX, i.ea, 4);
      break;

    default:
      abort;
  }

  a.kind = EFFECTIVE_ADDRESS;
  a.ea.base = RSI;
  a.ea.index = NONE;
  a.ea.scale = 1;
  a.ea.offset = 0;
  a.ea.reloc.kind = RELOC_NONE;
  a.ea.reloc.nr = 0;

  i.kind = INT_REGISTER;
  i.reg = RAX;
  i.reg_high = NONE;
}

/***********************************************************************************/

// if register in index with scale 1, and base is free, move it to base.

public 
void normalize_ea (ref NODE a)
{
  if (a.kind == EFFECTIVE_ADDRESS || a.kind == MEMORY)
  {
    if (a.ea.base == NONE && a.ea.index != NONE && a.ea.scale == 1)
    {
      a.ea.base = a.ea.index;
      a.ea.index = NONE;
    }
  }
}

/***********************************************************************************/

public 
bool ea_is_just_ea_with_register_base (NODE a)
{
  return a.kind == EFFECTIVE_ADDRESS && a.ea.base != NONE && a.ea.index == NONE && a.ea.offset == 0 && a.ea.reloc.kind == RELOC_NONE;
}

/***********************************************************************************/

public 
bool ea_is_simple_constant_offset (NODE a)
{
  return a.kind == EFFECTIVE_ADDRESS && a.ea.base == NONE && a.ea.index == NONE && a.ea.reloc.kind == RELOC_NONE;
}

/***********************************************************************************/

public 
void load_ea_into_just_register_base (ref NODE a)
{
  if (a.kind == MEMORY)
  {
    flush_effective_address (ref a);
  }
  else if (a.kind == EFFECTIVE_ADDRESS)
  {
    if (!ea_is_just_ea_with_register_base (a))
    {
      REG r = allocate_register (size => address_size, true, a);   // we can reuse a2's register

      c_lea_reg_mem (r, a.ea, size => address_size);

      a.ea.base   = r;
      a.ea.index  = NONE;
      a.ea.scale  = 1;
      a.ea.offset = 0;
      a.ea.reloc.kind = RELOC_NONE;
      a.ea.reloc.nr   = 0;
    }
  }
  else
    fatal_compiler_error0 ("load_ea_into_just_register_base(1)");
}

/***********************************************************************************/
