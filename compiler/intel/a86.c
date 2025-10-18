
// a86.c : generate intel/amd 80x86 machine code (32- or 64-bit)

from std use arithm, strings;
use ../error, ../pool, ../goptions, ../dllnames, ../common, ../exeout;
use a86tr, pe;

/************************************************************************/

bool ESP_correction_on;
int4 ESP_correction;  // effective addresses using ESP must be corrected with this value
                      // which is updated by push/pop/call instructions
                      // and checked for zero at the end.

/************************************************************************/

// see AMD-3-Set.pdf p 368, 369.

void evaluate_ea (    EA   ea,
                      byte reg,            // 0 to 15 (register or opcode code)
                      bool is_reg,         // 1 = reg is register, 0 = opcode
                      int  operand_size,   // 1, 2, 4 or 8.
                  out byte prex,          // lower nybble of REX byte (needed if > 0) (wrxb)
                  out bool rex_prefix_needed,
                  out byte pmem,
                  out bool psib_follows,
                  out byte psib,
                  out int  poffset_size,   // usually 4 (can also be 0 or 1 when RELOC_NONE)
                  out int  poffset_corr,   // offset correction
                  out bool absolute)       // true = absolute, false = RIP-relative 4-byte offset
{
  bool rex_w, rex_r, rex_b, rex_x;
  int  mem_rm, mem_mod;
  int  sib_base, sib_index, sib_scale;
  int  offset_size, offset_corr;

  if (ea.base == RIP && ea.index != NONE)
    fatal_compiler_error0 ("evaluate_ea(1)");

  if (ea.index == RSP || ea.index == RIP)
    fatal_compiler_error0 ("evaluate_ea(2)");

  if (ea.scale != 1 && ea.scale != 2 && ea.scale != 4 && ea.scale != 8)
    fatal_compiler_error0 ("evaluate_ea(3)");

  if (ea.scale != 1 && ea.index == NONE)
    fatal_compiler_error0 ("evaluate_ea(4)");

  if (reg > 15)
    fatal_compiler_error0 ("evaluate_ea(5)");

  if (operand_size != 1 && operand_size != 2 &&
      operand_size != 4 && operand_size != 8)
    fatal_compiler_error0 ("evaluate_ea(6)");

  if (address_size == 4)   // we generate 32-bit code
  {
    if (ea.base != NONE && ea.base >= R8)
      fatal_compiler_error0 ("evaluate_ea(7)");

    if (ea.index != NONE && ea.index >= R8)
      fatal_compiler_error0 ("evaluate_ea(8)");

    if (reg >= 8)
      fatal_compiler_error0 ("evaluate_ea(9)");

    if (operand_size == 8)
      fatal_compiler_error0 ("evaluate_ea(10)");
  }

  rex_w       = (operand_size == 8);   // 32- or 64-bit
  rex_r       = (reg >= 8);
  mem_mod     = 0;           // 0, 1 or 2  (3 is for reg,reg mode in the function below)
  mem_rm      = 0;
  sib_base    = 0;
  rex_b       = false;
  sib_index   = 0;
  rex_x       = false;
  sib_scale   = 0;
  offset_size = 0;
  offset_corr = (ea.base == RSP && ESP_correction_on) ? ESP_correction : 0;
  absolute   = true;

  if (ea.index == NONE)
  {
    if (ea.base == NONE)
    {
      if (address_size == 8)  // we generate 64-bit code
      {
        if (ea.reloc.kind == RELOC_NONE)
        {
          mem_rm      = 4;      // SIB byte follows
          sib_base    = 5;      // no base, simple offset32
          sib_index   = 4;      // no index
          offset_size = 4;      // mem_mod 0 + sib_base 5 means 32-bit offset
        }
        else   // 4-byte offset can be relative to RIP
        {
          mem_rm      = 5;   // RIP-relative (will take only 2 opcode bytes instead of 3)
          offset_size = 4;
          absolute = false;
        }
      }
      else   // we generate 32-bit code
      {
        mem_rm      = 5;
        offset_size = 4;
      }
    }
    else if (ea.base == RIP)  // can only be 64-bit
    {
      mem_rm      = 5;
      offset_size = 4;
    }
    else    // only base, no index
    {
      mem_rm = ((int)ea.base) & 7;
      rex_b  = (ea.base >= R8);

      if (ea.offset + offset_corr == 0 && ea.reloc.kind == RELOC_NONE)
      {
        if (mem_rm == 5)    // base is 5 (RBP) or 13 (R13)
        {
          mem_mod     = 1;   // force to offset8
          offset_size = 1;
        }
      }
      else if (ea.offset + offset_corr >= -128 && ea.offset + offset_corr <= 127 && ea.reloc.kind == RELOC_NONE)
      {
        mem_mod     = 1;
        offset_size = 1;   // offset8
      }
      else
      {
        mem_mod     = 2;
        offset_size = 4;   // offset32
      }

      if (mem_rm == 4)   // base is 4 (RSP) or 12 (R12)
      {
        sib_base  = 4;   // RSP or R12
        sib_index = 4;   // no index
      }
    }
  }
  else  // non-null index
  {
    rex_x = (ea.index >= R8);

    mem_rm = 4;  // SIB byte follows

    if (ea.base == NONE)   // no base, only index
    {
      sib_base    = 5;       // no base, simple offset32
      offset_size = 4;
    }
    else   // base + index
    {
      sib_base = ((int)ea.base) & 7;
      rex_b = (ea.base >= R8);

      if (ea.offset + offset_corr == 0 && ea.reloc.kind == RELOC_NONE)
      {
        if (sib_base == 5)  // base is 5 (RBP) or 13 (R13)
        {
          mem_mod     = 1;   // force to offset8
          offset_size = 1;
        }
      }
      else if (ea.offset + offset_corr >= -128 && ea.offset + offset_corr <= 127 && ea.reloc.kind == RELOC_NONE)
      {
        mem_mod     = 1;
        offset_size = 1;  // offset8
      }
      else
      {
        mem_mod     = 2;
        offset_size = 4;  // offset32
      }
    }

    sib_index = ((int)ea.index) & 7;

    if (ea.scale == 1)
      sib_scale = 0;
    else if (ea.scale == 2)
      sib_scale = 1;
    else if (ea.scale == 4)
      sib_scale = 2;
    else
      sib_scale = 3;
  }

  prex = (byte)(((byte)rex_w << 3) + ((byte)rex_r << 2) + ((byte)rex_x << 1) + (byte)rex_b);

  rex_prefix_needed = (prex > 0)
                    || (address_size == 8 && operand_size == 1 && is_reg && reg >= (byte)RSP);
  pmem = (byte)(((byte)mem_mod << 6) + ((reg & 7) << 3) + (byte)mem_rm);
  psib_follows = (mem_rm == 4);
  psib         = (byte)(sib_base + (sib_index << 3) + (sib_scale << 6));
  poffset_size = offset_size;
  poffset_corr = offset_corr;
}

/************************************************************************/

void evaluate_reg (    REG  modrm_reg,
                       bool modrm_reg_is_reg,
                       int  modrm_reg_size, // possibly smaller than operand_size for source of movsx, movzx.
                       byte reg,            // 0 to 15 (register or opcode code)
                       bool is_reg,         // 1 = reg is register, 0 = opcode
                       int  operand_size,   // 1, 2, 4 (or 8 for 64 bit only)
                   out byte prex,           // lower nybble of REX byte (needed if > 0)
                   out bool rex_prefix_needed,
                   out byte pmem)
{
  bool rex_w, rex_r, rex_b;

  if (operand_size != 1 && operand_size != 2 &&
      operand_size != 4 && operand_size != 8)
    fatal_compiler_error0 ("evaluate_reg(1)");

  if (modrm_reg_size != 1 && modrm_reg_size != 2 &&
      modrm_reg_size != 4 && modrm_reg_size != 8)
    fatal_compiler_error0 ("evaluate_reg(1b)");

  if (address_size == 4)   // we generate 32-bit code
  {
    if (modrm_reg >= R8)
      fatal_compiler_error0 ("evaluate_reg(2)");

    if (operand_size == 8)
      fatal_compiler_error0 ("evaluate_reg(3)");
  }

  rex_w = (operand_size == 8);   // 32- or 64-bit
  rex_r = (reg >= 8);
  rex_b = (modrm_reg >= R8);

  prex = (byte)(((byte)rex_w << 3) + ((byte)rex_r << 2) + (byte)rex_b);

  rex_prefix_needed = (prex > 0);

  if (address_size == 8)
  {
    bool x1 = operand_size == 1 && is_reg && reg >= (byte)RSP;
    bool x2 = modrm_reg_size == 1 && modrm_reg_is_reg && modrm_reg >= RSP;
    if (x1 | x2)
      rex_prefix_needed = true;
  }

  pmem = (byte)(0xC0 + ((reg & 7) << 3) + ((byte)modrm_reg & 7));
}

/************************************************************************/

void prefix_operand_size (int size)
{
  if (size != 1 && size != 2 && size != 4 && size != 8)
    fatal_compiler_error0 ("prefix_operand_size(1)");
  if (address_size < 8 && size == 8)
    fatal_compiler_error0 ("prefix_operand_size(2)");

  if (size == 2)
  {
    exe_write_byte (0x66);  // indicates 16-bit size
  }
}

/************************************************************************/

void valid_reg (REG r, int size)
{
  if (address_size == 4)
  {
    if (size != 1 && size != 2 && size != 4)
      fatal_compiler_error0 ("valid_reg(0)");

    if (r > RDI)
      fatal_compiler_error0 ("valid_reg(1)");

    if (size == 1 && (r == RSI || r == RDI || r == RBP || r == RSP))
      fatal_compiler_error0 ("valid_reg(2)");
  }
  else
  {
    if (size != 1 && size != 2 && size != 4 && size != 8)
      fatal_compiler_error0 ("valid_reg(3)");

    if (r > R15)
      fatal_compiler_error0 ("valid_reg(4)");
  }
}

/************************************************************************/

// indicates byte size

uint W (int size)
{
  return (size == 1) ? 0 : 1;
}

/************************************************************************/

int data_size (int data)
{
  if (data >= -128 && data <= 127)
    return 1;

  if (data >= -32768 && data <= 32767)
    return 2;

  return 4;
}

/************************************************************************/

void write_offset (int size, int8 value)
{
  switch (size)
  {
    case 0:
      break;

    case 1:
      exe_write_byte ((uint)value);
      break;

    case 2:
      exe_write_int2 ((int)value);
      break;

    case 4:
      exe_write_int4 ((int)value);
      break;

    case 8:
      exe_write_int8 (value);
      break;

    default:
      fatal_compiler_error0 ("write_offset(1)");
      break;
  }
}

/************************************************************************/

void write_offset_reloc (int size, int8 value, bool absolute, RELOC_INFO preloc)
{
  if (size != 4)
  {
    if (preloc.kind != RELOC_NONE)   // relocation only allowed for size 4
      fatal_compiler_error0 ("write_offset_reloc(0)");

    if (preloc.nr != 0)
      fatal_compiler_error0 ("write_offset_reloc(1)");

    if (!absolute)
      fatal_compiler_error0 ("write_offset_reloc(2)");
  }

  if (address_size == 4)
  {
    if (!absolute)
      fatal_compiler_error0 ("write_offset_reloc(3)");
  }

  switch (size)
  {
    case 0:
      break;

    case 1:
      exe_write_byte ((uint)value);
      break;

    case 2:
      exe_write_int2 ((int)value);
      break;

    case 4:
      exe_write_int4 ((int)value);
      switch (preloc.kind)
      {
        case RELOC_FUNC:
          exe_func_backfill_addr4_written ((int4)preloc.nr, absolute);
          break;

        case RELOC_DLL:
          {
            string^ dll, func;
            get_dll_name ((uint4)preloc.nr, out dll, out func);
            exe_dll_reference_written (dll^, func^, absolute);
          }
          break;

        case RELOC_POOL:
          pool_add_backfill_addr4 (preloc.nr, current_RIP()-4, absolute, (uint4)value);
          break;

        case RELOC_GLOBAL:
          if (preloc.nr != 0)
            fatal_compiler_error0 ("write_offset_reloc(global)");
          exe_global_backfill_addr4_written ((int)value, absolute);
          break;

        case RELOC_NONE:
          if (!absolute)
            fatal_compiler_error0 ("write_offset_reloc(5)");
          if (preloc.nr != 0)
            fatal_compiler_error0 ("write_offset_reloc(none)");
          break;

        default:
          fatal_compiler_error0 ("write_offset_reloc(bad kind)");
          break;
      }
      break;

    case 8:
      exe_write_int8 (value);
      break;

    default:
      fatal_compiler_error0 ("write_offset_reloc(9)");
      break;
  }
}

/************************************************************************/

void reg_to_reg_1 (byte opcode, REG target, byte source, bool source_is_reg, int size)
{
  byte rex, mem;
  bool rex_prefix_needed;

  prefix_operand_size (size);

  evaluate_reg (target, modrm_reg_is_reg => true, size,
                (byte)source, is_reg=> source_is_reg, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode + W(size));
  exe_write_byte (mem);
}

/************************************************************************/

// with 2 opcodes

void reg_to_reg_2 (byte opcode, byte opcode2, REG reg1, int size_reg1, REG reg2, int size)
{
  byte rex, mem;
  bool rex_prefix_needed;

  prefix_operand_size (size);

  evaluate_reg (reg1, modrm_reg_is_reg => true, size_reg1,
                (byte)reg2, is_reg => true, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode);
  exe_write_byte (opcode2 + W(size));
  exe_write_byte (mem);
}

/************************************************************************/

void reg_to_reg_normal (byte opcode, REG target, REG source, int size)
{
  valid_reg (target, size);
  valid_reg (source, size);
  reg_to_reg_1 ((byte)(opcode*8), target, (byte)source, source_is_reg => true, size);
}

/************************************************************************/

void mem_to_reg_1 (byte opcode, REG target, EA source, int size, int imm_size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  valid_reg (target, size);
  prefix_operand_size (size);

  evaluate_ea (source, (byte)target, is_reg => true, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  if (!absolute)  // 64bit RIP relative offset
    offset_corr = -imm_size;  // rip is relative to next opcode

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/************************************************************************/

// with 2 opcodes

void mem_to_reg_2 (byte opcode, byte opcode2, REG target, EA source, int size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  valid_reg (target, size);
  prefix_operand_size (size);

  evaluate_ea (source, (byte)target, is_reg => true, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode);
  exe_write_byte (opcode2 + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/************************************************************************/

void mem_to_reg_normal (byte opcode, REG target, EA source, int size)
{
  valid_reg (target, size);
  mem_to_reg_1 ((byte)(opcode*8 + 0x02), target, source, size, 0);
}

/************************************************************************/

void reg_to_mem_1 (byte opcode, EA target, byte source, bool source_is_reg, int size, int imm_size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  prefix_operand_size (size);

  evaluate_ea (target, source, is_reg => source_is_reg, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  if (!absolute)  // 64bit RIP relative offset
    offset_corr = -imm_size;  // rip is relative to next opcode

  write_offset_reloc (offset_size, target.offset + offset_corr, absolute, target.reloc);
}

/************************************************************************/

void reg_to_mem_normal (byte opcode, EA target, REG source, int size)
{
  valid_reg (source, size);
  reg_to_mem_1 ((byte)(opcode*8 + 0x00), target, (byte)source, source_is_reg => true, size, 0);
}

/************************************************************************/

void reg_to_mem_2 (byte opcode, byte opcode2, EA target, REG source, int size, int imm_size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  valid_reg (source, size);
  prefix_operand_size (size);

  evaluate_ea (target, (byte)source, is_reg => true, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode);
  exe_write_byte (opcode2 + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  if (!absolute)  // 64bit RIP relative offset
    offset_corr = -imm_size;  // rip is relative to next opcode

  write_offset_reloc (offset_size, target.offset + offset_corr, absolute, target.reloc);
}

/************************************************************************/

// for add, sub, adc, subc, and, or, xor, cmp.
// !! a 64-bit imm value is not possible !!

void imm_to_reg_normal (byte opcode, REG target, int imm, int size)
{
  byte rex, mem, extend;
  int  imm_size;
  bool rex_prefix_needed;

  valid_reg (target, size);

  imm_size = data_size(imm);

  prefix_operand_size (size);

  evaluate_reg (target, modrm_reg_is_reg => true, size,
                opcode, is_reg => false, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  if (target == RAX && imm_size == min(4,size))
  {
    exe_write_byte (opcode*8 + 4 + W(size));
    write_offset (imm_size, imm);
  }
  else
  {
    if (imm_size == 1 && size > 1)
    {
      extend = 1;
    }
    else
    {
      imm_size = min(4,size);   // 1, 2, 4
      extend = 0;
    }

    exe_write_byte (128 + extend*2 + W(size));
    exe_write_byte (mem);
    write_offset (imm_size, imm);
  }
}

/************************************************************************/

// for add, sub, adc, subc, and, or, xor, cmp.
// !! a 64-bit imm value is not possible !!

void imm_to_mem_normal (byte opcode, EA target, int imm, int size)
{
  byte rex, mem, sib, extend;
  bool rex_prefix_needed, sib_follows;
  int  offset_size;
  int  imm_size, offset_corr;
  bool absolute;

  prefix_operand_size (size);

  evaluate_ea (target, opcode, is_reg => false, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  imm_size = data_size(imm);
  if (imm_size == 1 && size > 1)
  {
    imm_size = 1;
    extend = 1;
  }
  else
  {
    imm_size = min(4,size);
    extend = 0;
  }

  exe_write_byte (128 + extend*2 + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  if (!absolute)  // 64bit RIP relative offset
    offset_corr = -imm_size;  // rip is relative to next opcode

  write_offset_reloc (offset_size, target.offset + offset_corr, absolute, target.reloc);
  write_offset (imm_size, imm);
}

/************************************************************************/

// opcode + nybble.

void mem_operand_1 (byte opcode, byte opcode2, bool opcode2_is_reg, EA source, int size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  prefix_operand_size (size);

  evaluate_ea (source, (byte)opcode2, is_reg => opcode2_is_reg, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/************************************************************************/

// opcode + opcode2 (used for c_setcond_mem)

void mem_operand_3 (byte opcode, byte opcode2, EA source, int size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  prefix_operand_size (size);

  evaluate_ea (source, 0, is_reg => false, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode);
  exe_write_byte (opcode2);

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/************************************************************************/

// opcode + nybble

void reg_operand_1 (byte opcode, byte opcode2, REG source, int size)
{
  byte rex, mem;
  bool rex_prefix_needed;

  prefix_operand_size (size);    // optional 16-bit

  evaluate_reg (source, modrm_reg_is_reg => true, size,
                opcode2, is_reg => false, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode + W(size));
  exe_write_byte (mem);
}

/************************************************************************/

// opcode + reg

void reg_operand_2 (byte opcode, REG target, int size)
{
  byte rex, mem;
  bool rex_prefix_needed;

  prefix_operand_size (size);

  evaluate_reg (target, modrm_reg_is_reg => true, size,
                0, is_reg => false, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode + (mem & 7));
}

/************************************************************************/

// used for call_reg

void reg_operand_3 (byte opcode, byte opcode2, REG target, int size)
{
  byte rex, mem;
  bool rex_prefix_needed;

  evaluate_reg (target, modrm_reg_is_reg => true, size,
                opcode2, is_reg => false, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (opcode);
  exe_write_byte (mem);
}

/************************************************************************/

// 32 bit : size 1 :  AL,  BL,  CL,  DL
//          size 2 :  AX,  BX,  CX,  DX,  BP,  SP,  SI,  DI
//          size 4 : EAX, EBX, ECX, EDL, EBP, ESP, ESI, EDI

// 64 bit : size 1, 2, 4, 8 (all registers)

public
void c_add_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("add", target, source, size);
  reg_to_reg_normal (0, target, source, size);
}

public
void c_add_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("add", target, source, size);
  mem_to_reg_normal (0, target, source, size);
}

public
void c_add_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("add", target, source, size);
  reg_to_mem_normal (0, target, source, size);
}

public
void c_add_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("add", target, imm, size);
  imm_to_reg_normal (0, target, imm, size);
}

public
void c_add_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("add", target, imm, size);
  imm_to_mem_normal (0, target, imm, size);
}

/************************************************************************/

public
void c_adc_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("adc", target, source, size);
  reg_to_reg_normal (2, target, source, size);
}

public
void c_adc_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("adc", target, source, size);
  mem_to_reg_normal (2, target, source, size);
}

public
void c_adc_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("adc", target, source, size);
  reg_to_mem_normal (2, target, source, size);
}

public
void c_adc_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("adc", target, imm, size);
  imm_to_reg_normal (2, target, imm, size);
}

public
void c_adc_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("adc", target, imm, size);
  imm_to_mem_normal (2, target, imm, size);
}

/************************************************************************/

public
void c_sub_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("sub", target, source, size);
  reg_to_reg_normal (5, target, source, size);
}

public
void c_sub_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("sub", target, source, size);
  mem_to_reg_normal (5, target, source, size);
}

public
void c_sub_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("sub", target, source, size);
  reg_to_mem_normal (5, target, source, size);
}

public
void c_sub_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("sub", target, imm, size);
  imm_to_reg_normal (5, target, imm, size);
}

public
void c_sub_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("sub", target, imm, size);
  imm_to_mem_normal (5, target, imm, size);
}

/************************************************************************/

public
void c_sbb_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("sbb", target, source, size);
  reg_to_reg_normal (3, target, source, size);
}

public
void c_sbb_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("sbb", target, source, size);
  mem_to_reg_normal (3, target, source, size);
}

public
void c_sbb_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("sbb", target, source, size);
  reg_to_mem_normal (3, target, source, size);
}

public
void c_sbb_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("sbb", target, imm, size);
  imm_to_reg_normal (3, target, imm, size);
}

public
void c_sbb_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("sbb", target, imm, size);
  imm_to_mem_normal (3, target, imm, size);
}

/************************************************************************/

public
void c_and_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("and", target, source, size);
  reg_to_reg_normal (4, target, source, size);
}

public
void c_and_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("and", target, source, size);
  mem_to_reg_normal (4, target, source, size);
}

public
void c_and_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("and", target, source, size);
  reg_to_mem_normal (4, target, source, size);
}

public
void c_and_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("and", target, imm, size);
  imm_to_reg_normal (4, target, imm, size);
}

public
void c_and_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("and", target, imm, size);
  imm_to_mem_normal (4, target, imm, size);
}

/************************************************************************/

public
void c_or_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("or", target, source, size);
  reg_to_reg_normal (1, target, source, size);
}

public
void c_or_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("or", target, source, size);
  mem_to_reg_normal (1, target, source, size);
}

public
void c_or_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("or", target, source, size);
  reg_to_mem_normal (1, target, source, size);
}

public
void c_or_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("or", target, imm, size);
  imm_to_reg_normal (1, target, imm, size);
}

public
void c_or_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("or", target, imm, size);
  imm_to_mem_normal (1, target, imm, size);
}

/************************************************************************/

public
void c_xor_reg_reg (REG target, REG source, int size)
{
  int siz0;

  if (size == 8 && target == source)
    siz0 = 4;  // 4 bytes is enough, the upper 4 bytes will be cleared
  else
    siz0 = size;

  trace_opcode_reg_reg ("xor", target, source, siz0);
  reg_to_reg_normal (6, target, source, siz0);
}

public
void c_xor_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("xor", target, source, size);
  mem_to_reg_normal (6, target, source, size);
}

public
void c_xor_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("xor", target, source, size);
  reg_to_mem_normal (6, target, source, size);
}

public
void c_xor_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("xor", target, imm, size);
  imm_to_reg_normal (6, target, imm, size);
}

public
void c_xor_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("xor", target, imm, size);
  imm_to_mem_normal (6, target, imm, size);
}

/************************************************************************/

public
void c_cmp_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("cmp", target, source, size);
  reg_to_reg_normal (7, target, source, size);
}

public
void c_cmp_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("cmp", target, source, size);
  mem_to_reg_normal (7, target, source, size);
}

public
void c_cmp_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("cmp", target, source, size);
  reg_to_mem_normal (7, target, source, size);
}

public
void c_cmp_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("cmp", target, imm, size);
  imm_to_reg_normal (7, target, imm, size);
}

public
void c_cmp_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("cmp", target, imm, size);
  imm_to_mem_normal (7, target, imm, size);
}

/************************************************************************/

// generates 5 bytes of code

public
void c_call_relative (int func_label_nr, int nb_arguments)
{
  trace_opcode_call_relative ("call", func_label_nr);

  exe_write_byte (0xE8);   // opcode
  write_offset (4, 0);
  exe_func_backfill_addr4_written (func_label_nr, absolute => false);

  ESP_correction -= nb_arguments * address_size;
}

/************************************************************************/

// size MUST be address size

// 32-bit : EAX .. RDI
// 64-bit : RAX .. R15

public
void c_call_reg (REG source, int size, int nb_arguments)
{
  trace_opcode_reg ("call", source, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_call_reg(1)");

  valid_reg (source, size);

  reg_operand_3 (0xFF, 2, source, 4);   // 4 because no REX.w prefix must be generated

  ESP_correction -= nb_arguments * address_size;
}

/************************************************************************/

// memory always reads address size

public
void c_call_indirect (EA source, int size, int nb_arguments)
{
  trace_opcode_mem ("call", source, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_call_indirect(1)");

  mem_operand_1 (0xFF-1, 2, opcode2_is_reg => false, source, 4);  // opcode + nybble
                                                                   // 4 because no REX prefix must be generated

  ESP_correction -= nb_arguments * address_size;
}

/************************************************************************/

// generates 2 bytes of code (maybe later expanded to 5 bytes)
// ! offset is relative to RIP of following instruction !

public
void c_jump_relative (int near_label_nr)
{
  trace_opcode_jmp_relative ("jmp", near_label_nr);

//  if (relative_offset >= -128 && relative_offset <= 127)
//  {
    exe_write_byte (0xEB);
    exe_write_byte (0 /*relative_offset*/);
    exe_near_branch_written (near_label_nr, conditional => false);
//  }
//  else
//  {
//    exe_write_byte (0xE9);
//    write_offset (4, relative_offset);
//  }
}

/************************************************************************/

public
void c_jump_indirect (EA source, int size)
{
  trace_opcode_mem ("jmp", source, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_jump_indirect(1)");

  mem_operand_1 (0xFF-1, 4, opcode2_is_reg => false, source, 4);  // size is 4 because no "operand size" prefix needed in 64bit mode
}

/************************************************************************/

// generates 2 bytes of code (maybe later expanded to 6 bytes)
// depending if relative_offset fits into -128 .. +127 or not.
// ! offset is relative to RIP of following instruction !

public
void c_jcond_raw (byte opcode2,    // 0=jo, 1=jno, 8=js, 9=jns, 10=jp, 11=jnp
                  int  near_label_nr)
{
  trace_opcode_jcond ("jcc", opcode2, near_label_nr);

  if (opcode2 > 15)
    fatal_compiler_error0 ("c_jcond_raw(1)");

//  if (relative_offset >= -128 && relative_offset <= 127)
//  {
    exe_write_byte (0x70 + opcode2);
    write_offset (1, 0/*relative_offset*/);
    exe_near_branch_written (near_label_nr, conditional =>  true);
//  }
//  else
//  {
//    exe_write_byte (0x0F);
//    exe_write_byte (0x80 + opcode2);
//    write_offset (4, relative_offset);
//  }
}

/************************************************************************/

package CMP
  const byte table_unsigned[7] = {0, 0x2, 0x4, 0x6, 0x7, 0x5, 0x3};
  const byte table_signed[7]   = {0, 0xC, 0x4, 0xE, 0xF, 0x5, 0xD};
end CMP;

/************************************************************************/

// generates 2 bytes of code (maybe later expanded to 6 bytes)
// depending if relative_offset fits into -128 .. +127 or not.
// ! offset is relative to RIP of following instruction !

public
void c_jcond (COMPARISON_FLAG condition,
              bool            signed_operands,
              int             near_label_nr)
{
  byte opcode2;

  if ((int)condition < 1 || (int)condition > 6)
    fatal_compiler_error0 ("c_jcond(1)");

  opcode2 = (byte)(signed_operands ? table_signed[(int)condition] : table_unsigned[(int)condition]);

  c_jcond_raw (opcode2, near_label_nr);
}

/************************************************************************/

// REG
// 32-bit : al, bl, cl, dl
// 64-bit : al, bl, cl, dl, bpl, spl, sil, dil, r8b to r15b

public
void c_setcond_reg (COMPARISON_FLAG condition,
                    bool signed_operands,
                    REG  target,
                    int  size)       // always 1
{
  byte opcode2;

  trace_opcode_setcc_reg ("scc", condition, signed_operands, target, size);

  if (size != 1)
    fatal_compiler_error0 ("c_setcond_reg(1)");

  valid_reg (target, 1);

  if ((uint)condition < 1 || (uint)condition > 6)
    fatal_compiler_error0 ("c_setcond_reg(2)");

  opcode2 = (byte)(signed_operands ? table_signed[(uint)condition] : table_unsigned[(uint)condition]);

  reg_to_reg_2 (0x0F, (byte)(0x90+opcode2),
                target, 1,
                (REG)0, 1);   // 2 opcodes + /0 in nybble.
}

/************************************************************************/

public
void c_setcond_mem (COMPARISON_FLAG condition,
                    bool signed_operands,
                    EA   target,
                    int  size)       // always 1
{
  byte opcode2;

  trace_opcode_setcc_mem ("scc", condition, signed_operands, target, size);

  if (size != 1)
    fatal_compiler_error0 ("c_setcond_mem(1)");

  if ((uint)condition < 1 || (uint)condition > 6)
    fatal_compiler_error0 ("c_setcond_mem(2)");

  opcode2 = (byte)(signed_operands ? table_signed[(uint)condition] : table_unsigned[(uint)condition]);

  mem_operand_3 (0x0F, (byte)(0x90+opcode2), target, 1);
}

/************************************************************************/

public
void c_leave ()
{
  trace_opcode ("leave");

  exe_write_byte (0xC9);
}

/************************************************************************/

public
void c_ret (int rsp_offset)   // rsp_offset in range 0 .. 32767
{
  trace_opcode_imm ("ret", rsp_offset);

  if (rsp_offset == 0)
    exe_write_byte (0xC3);
  else
  {
    exe_write_byte (0xC2);
    write_offset (2, rsp_offset);
  }
}

/************************************************************************/

// generate interrupt (usually 0 for division by zero, or 5 for constraint error)

public
void c_int (byte nr)
{
  trace_opcode_imm ("int", nr);

  exe_write_byte (0xCD);
  exe_write_byte (nr);
}

/************************************************************************/

// size indicates REG : 4 or 8 bytes

public
void c_lea_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("lea", target, source, size);

  valid_reg (target, address_size);
  mem_operand_1 (0x8D-1, (byte)target, opcode2_is_reg => true, source, size);
}

/************************************************************************/

// 32 bit : EAX, EBX, ECX, EDX, EBP, ESP, ESI, EDI
// 64 bit : all registers

// ! only allows size == address_size
// ! always pushes address-size bytes on stack

public
void c_push_reg (REG source, int size)
{
  trace_opcode_reg ("push", source, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_push_reg(1)");

  valid_reg (source, size);

  reg_operand_2 (0x50, source, 4); // size is 4 because we want no REX prefix for "operand size" in 64bit mode

  ESP_correction += address_size;
}

/************************************************************************/

// ! only allows size == address_size
// ! always pushes address-size bytes on stack

public
void c_push_mem (EA source, int size)
{
  trace_opcode_mem ("push", source, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_push_mem(1)");

  mem_operand_1 (0xFF-1, 6, opcode2_is_reg => false, source, 4); // size is 4 because no "operand size" prefix needed in 64bit mode

  ESP_correction += address_size;
}

/************************************************************************/

// ! imm is limited to 32-bit values
// ! always pushes address-size bytes on stack

public
void c_push_imm (int4 imm)
{
  trace_opcode_imm ("push", imm);

  if (imm >= -128 && imm <= 127)
  {
    exe_write_byte (0x6A);
    exe_write_byte ((byte)imm);
  }
  else
  {
    exe_write_byte (0x68);
    exe_write_int4 (imm);
  }

  ESP_correction += address_size;
}

/************************************************************************/

public
void c_push_imm_reloc (int4 imm, RELOC_INFO preloc)   // relocatable constant
{
  trace_opcode_imm_reloc ("push", imm, preloc, address_size);

  exe_write_byte (0x68);
  write_offset_reloc (4, imm, true, preloc);

  ESP_correction += address_size;
}

/************************************************************************/

public
void c_push_flags ()
{
  trace_opcode ("push flags");

  exe_write_byte (0x9C);

  ESP_correction += address_size;
}

/************************************************************************/

public
void c_pop_flags ()
{
  trace_opcode ("pop flags");

  exe_write_byte (0x9D);

  ESP_correction -= address_size;
}

/************************************************************************/

// ! only allows size == address_size
// ! always pops address-size bytes from stack

public
void c_pop_reg (REG target, int size)
{
  trace_opcode_reg ("pop", target, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_pop_reg(1)");

  valid_reg (target, size);

  reg_operand_2 (0x58, target, 4); // size is 4 because no "operand size" prefix needed in 64bit mode

  ESP_correction -= address_size;
}

/************************************************************************/

// ! only allows size == address_size
// ! always pops address-size bytes on stack

public
void c_pop_mem (EA source, int size)
{
  trace_opcode_mem ("pop", source, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_pop_mem(1)");

  mem_operand_1 (0x8F-1, 0, opcode2_is_reg => false, source, 4); // size is 4 because no "operand size" prefix needed in 64bit mode

  ESP_correction -= address_size;
}

/************************************************************************/

public
void c_neg_reg (REG target, int size)
{
  trace_opcode_reg ("neg", target, size);
  valid_reg (target, size);
  reg_operand_1 (0xF6, 3, target, size);
}

/************************************************************************/

public
void c_neg_mem (EA target, int size)
{
  trace_opcode_mem ("neg", target, size);
  mem_operand_1 (0xF6, 3, opcode2_is_reg => false, target, size);
}

/************************************************************************/

public
void c_not_reg (REG target, int size)
{
  trace_opcode_reg ("not", target, size);
  valid_reg (target, size);
  reg_operand_1 (0xF6, 2, target, size);
}

/************************************************************************/

public
void c_not_mem (EA target, int size)
{
  trace_opcode_mem ("not", target, size);
  mem_operand_1 (0xF6, 2, opcode2_is_reg => false, target, size);
}

/************************************************************************/

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

public
void c_inc_reg (REG target, int size)
{
  trace_opcode_reg ("inc", target, size);
  valid_reg (target, size);

  if (address_size == 4)   // 32-bit mode
  {
    if (size == 1)
    {
      reg_operand_1 (0xFE, 0, target, size);
    }
    else
    {
      prefix_operand_size (size);           // optional 16-bit prefix
      exe_write_byte ((byte)(0x40 + (int)target));  // single-byte inc opcode (does not exist in 64bit mode)
    }
  }
  else   // 64-bit mode
  {
    reg_operand_1 (0xFE, 0, target, size);
  }
}

/************************************************************************/

public
void c_dec_reg (REG target, int size)
{
  trace_opcode_reg ("dec", target, size);
  valid_reg (target, size);

  if (address_size == 4)   // 32-bit mode
  {
    if (size == 1)
    {
      reg_operand_1 (0xFE, 1, target, size);
    }
    else  // size 2, 4, 8
    {
      prefix_operand_size (size);           // optional 16-bit prefix
      exe_write_byte ((byte)(0x48 + (int)target));  // single-byte inc opcode (does not exist in 64bit mode)
    }
  }
  else   // 64-bit mode
  {
    reg_operand_1 (0xFE, 1, target, size);
  }
}

/************************************************************************/

public
void c_inc_mem (EA target, int size)
{
  trace_opcode_mem ("inc", target, size);
  mem_operand_1 (0xFE, 0, opcode2_is_reg => false, target, size);
}

/************************************************************************/

public
void c_dec_mem (EA target, int size)
{
  trace_opcode_mem ("dec", target, size);
  mem_operand_1 (0xFE, 1, opcode2_is_reg => false, target, size);
}

/************************************************************************/

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

public
void c_xchg_reg_reg (REG source1, REG source2, int size)
{
  valid_reg (source1, size);
  valid_reg (source2, size);

  if (source1 == source2)
    fatal_compiler_error0 ("c_xchg_reg_reg(1)");  // not useful

  if (source1 == RAX && source2 != RAX)
    c_xchg_reg_reg (source2, source1, size);
  else if (source2 == RAX && size >= 2)
  {
    trace_opcode_reg_reg ("xchg", source1, source2, size);
    reg_operand_2 (0x90, source1, size);
  }
  else
  {
    trace_opcode_reg_reg ("xchg", source1, source2, size);
    reg_to_reg_1 (0x86, source1, (byte)source2, source_is_reg => true, size);
  }
}

/************************************************************************/

public
void c_xchg_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("xchg", target, source, size);
  valid_reg (target, size);
  mem_to_reg_1 (0x86, target, source, size, 0);
}

/************************************************************************/

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

public
void c_test_reg_reg (REG source1, REG source2, int size)
{
  trace_opcode_reg_reg ("test", source1, source2, size);

  valid_reg (source1, size);
  valid_reg (source2, size);

  reg_to_reg_1 (0x84, source1, (byte)source2, source_is_reg => true, size);
}

/************************************************************************/

public
void c_test_mem_reg (EA source1, REG source2, int size)
{
  trace_opcode_mem_reg ("test", source1, source2, size);
  valid_reg (source2, size);
  reg_to_mem_1 (0x84, source1, (byte)source2, source_is_reg => true, size, 0);
}

/************************************************************************/

// imm is limited to 32-bit

public
void c_test_reg_imm (REG source, int4 imm, int size)
{
  byte rex, mem;

  trace_opcode_reg_imm ("test", source, imm, size);

  valid_reg (source, size);

  prefix_operand_size (size);   // 16-bit prefix

  if (source == RAX)
  {
    if (size == 8)
      exe_write_byte (0x48);  // REX byte for 64-bit operand
    exe_write_byte (0xA8 + W(size));
  }
  else
  {
    bool rex_prefix_needed;

    evaluate_reg (source, modrm_reg_is_reg => true, size,
                  0, is_reg => false, size,
                  out rex, out rex_prefix_needed, out mem);

    if (rex_prefix_needed)
      exe_write_byte (0x40 + rex);

    exe_write_byte (0xF6 + W(size));
    exe_write_byte (mem);
  }

  write_offset (min(4,size), imm);   // 1, 2 or 4 byte immediate value
}

/************************************************************************/

public
void c_test_mem_imm (EA source, int4 imm, int size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr, imm_size;
  bool absolute;

  trace_opcode_mem_imm ("test", source, imm, size);

  prefix_operand_size (size);   // 16-bit prefix

  evaluate_ea (source, 0, is_reg => false, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0xF6 + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  imm_size = min(4,size);

  if (!absolute)  // 64bit RIP relative offset
    offset_corr = -imm_size;  // rip is relative to next opcode

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
  write_offset (imm_size, imm);
}

/************************************************************************/

// REG

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

public
void c_mov_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("mov", target, source, size);
  valid_reg (target, size);
  valid_reg (source, size);
  reg_to_reg_1 (0x88, target, (byte)source, source_is_reg => true, size);
}

/************************************************************************/

public
void c_mov_reg_mem (REG target, EA source, int size)
{
  valid_reg (target, size);

  trace_opcode_reg_mem ("mov", target, source, size);

  if (target == RAX && address_size == 4 &&     // it's not interesting for 64-bit because 8 byte offsets are longer than EA coding
      source.base == NONE && source.index == NONE)  // global variable at offset into AL/AX/EAX/RAX
  {
    prefix_operand_size (size);   // 16-bit prefix
    exe_write_byte (0xA0 + W(size));                // load from global absolute address into register
    write_offset_reloc (4, source.offset, true, source.reloc);
  }
  else
  {
    mem_to_reg_1 (0x8A, target, source, size, 0);
  }
}

/************************************************************************/

public
void c_mov_mem_reg (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("mov", target, source, size);

  valid_reg (source, size);

  if (source == RAX && address_size == 4 &&
      target.base == NONE && target.index == NONE)  // EAX into global variable at offset
  {
    prefix_operand_size (size);   // 16-bit prefix
    exe_write_byte (0xA2 + W(size));
    write_offset_reloc (4, target.offset, true, target.reloc);
  }
  else
  {
    reg_to_mem_1 (0x88, target, (byte)source, source_is_reg => true, size, 0);
  }
}

/************************************************************************/

// 64-bit imm allowed.

public
void c_mov_reg_imm (REG target, int8 imm, int size)
{
  valid_reg (target, size);

  if (imm == 0)  // optimize using xor instruction.
  {
    c_xor_reg_reg (target, target, size);
  }
  else if (size == 8 && imm >= -(int8)2147483648 && imm <= (int8)2147483647)  // sign-extend 32imm into reg64
  {
    byte rex, mem;
    bool rex_prefix_needed;

    trace_opcode_reg_imm ("mov", target, imm, size);

    prefix_operand_size (size);

    evaluate_reg (target, modrm_reg_is_reg => true, size,
                  0, is_reg => false, size,
                  out rex, out rex_prefix_needed, out mem);

    if (rex_prefix_needed)
      exe_write_byte (0x40 + rex);

    exe_write_byte (0xC6 + W(size));    // MOV reg/mem64, imm32
    exe_write_byte (mem);

    write_offset (4, imm);
  }
  else if (size == 8 && imm >= 0 && imm <= (int8)4294967295)  // zero-extend 32imm into reg64
  {
    byte rex, mem;
    bool rex_prefix_needed;

    trace_opcode_reg_imm ("mov", target, imm, size);

    prefix_operand_size (4);

    evaluate_reg (target, modrm_reg_is_reg => true, 4,
                  0, is_reg => false, 4,
                  out rex, out rex_prefix_needed, out mem);

    if (rex_prefix_needed)
      exe_write_byte (0x40 + rex);

    exe_write_byte (0xC6 + W(4));
    exe_write_byte (mem);

    write_offset (4, imm);
  }
  else  // sizes 1, 2, 4, 8
  {
    byte rex, mem;
    bool rex_prefix_needed;

    trace_opcode_reg_imm ("mov", target, imm, size);

    prefix_operand_size (size);

    evaluate_reg (target, modrm_reg_is_reg => true, size,
                  (byte)W(size), is_reg => false, size,
                  out rex, out rex_prefix_needed, out mem);

    if (rex_prefix_needed)
      exe_write_byte (0x40 + rex);

    exe_write_byte (0xB0 + (mem & 0x3F));

    write_offset (size, imm);
  }
}

/************************************************************************/

// size always address_size, imm limited to 32-bit as it's an address inside 32 bit code+data+bss.

public
void c_mov_reg_imm_reloc (REG target, int4 imm, int size, RELOC_INFO preloc)
{
  byte rex, mem;
  bool rex_prefix_needed;
  const int siz4 = 4;

  trace_opcode_reg_imm_reloc ("mov", target, imm, preloc, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_mov_reg_imm_reloc(1)");

  valid_reg (target, size);

  prefix_operand_size (size);

  evaluate_reg (target, modrm_reg_is_reg => true, siz4,
                (byte)W(siz4), is_reg => false, siz4,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0xB0 + (mem & 0x3F));

  write_offset_reloc (siz4, imm, true, preloc);
}

/************************************************************************/

public
void c_mov_mem_imm (EA target, int4 imm, int size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr, imm_size;
  bool absolute;

  trace_opcode_mem_imm ("mov", target, imm, size);
  prefix_operand_size (size);   // 16-bit prefix

  evaluate_ea (target, 0, is_reg => false, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0xC6 + W(size));

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  imm_size = min(4,size);

  if (!absolute)  // 64bit RIP relative offset
    offset_corr = -imm_size;  // rip is relative to next opcode

  write_offset_reloc (offset_size, target.offset + offset_corr, absolute, target.reloc);
  write_offset (imm_size, imm);
}

/************************************************************************/

// size always address_size, imm limited to 32-bit as it's an address inside 32 bit code+data+bss.

public
void c_mov_mem_imm_reloc (EA target, int4 imm, int size, RELOC_INFO preloc)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr, imm_size;
  bool absolute;

  trace_opcode_mem_imm_reloc ("mov", target, imm, preloc, size);

  if (size != address_size)
    fatal_compiler_error0 ("c_mov_mem_imm_reloc(1)");

  prefix_operand_size (size);   // 16-bit prefix

  evaluate_ea (target, 0, is_reg => false, size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0xC6 + W(size));  // MOV mem32,imm32  or   MOV mem64,imm32

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  imm_size = min(4,size);

  if (!absolute)  // 64bit RIP relative offset
    offset_corr = -imm_size;  // rip is relative to next opcode

  write_offset_reloc (offset_size, target.offset + offset_corr, absolute, target.reloc);
  write_offset_reloc (imm_size, imm, absolute => true, preloc);
}

/************************************************************************/

// move with sign-extension
// assert: size_target > size-source

public
void c_movsx_reg_reg (REG target, int size_target,
                      REG source, int size_source)
{
  trace_opcode_reg_reg_sizes ("movsx", target, source, size_target, size_source);

  if (size_target <= size_source)
    fatal_compiler_error0 ("c_movsx_reg_reg(1)");

  valid_reg (target, size_target);
  valid_reg (source, size_source);

  if (source == RAX && target == RAX && size_target - size_source < 7)
  {
    if (size_source == 1)
    {
      exe_write_byte (0x66);
      exe_write_byte (0x98);   // CBW (AX -> AL)
    }

    if (size_source <= 2 && size_target >= 4)
      exe_write_byte (0x98);   // CWDE (EAX <- AX)

    if (size_target == 8)
    {
      exe_write_byte (0x48);   // REX.W
      exe_write_byte (0x98);   // CWQE (RAX <- EAX)
    }
  }
  else
  {
    if (size_source == 1)
    {
      reg_to_reg_2 (0x0F, 0xBE-1,
                    source, size_source,
                    target, size_target);
    }
    else if (size_source == 2)
    {
      reg_to_reg_2 (0x0F, 0xBF-1,
                    source, size_source,
                    target, size_target);
    }
    else   // 4 to 8
    {
      reg_to_reg_1 (0x62, source, (byte)target, source_is_reg => true, size_target);
    }
  }
}

/************************************************************************/

// move with sign-extension
// assert: size_target > size-source

public
void c_movsx_reg_mem (REG target, int size_target,
                      EA source, int size_source)
{
  trace_opcode_reg_mem_sizes ("movsx", target, source, size_target, size_source);

  if (size_target <= size_source)
    fatal_compiler_error0 ("c_movsx_reg_mem(1)");

  valid_reg (target, size_target);

  if (size_source == 1)
    mem_to_reg_2 (0x0F, 0xBE-1, target, source, size_target);
  else if (size_source == 2)
    mem_to_reg_2 (0x0F, 0xBF-1, target, source, size_target);
  else  // 4 -> 8 bytes
    mem_to_reg_1 (0x62, target, source, size_target, 0);
}

/************************************************************************/

// move with zero-extension
// assert: size_target > size-source

public
void c_movzx_reg_reg (REG target, int size_target,
                      REG source, int size_source)
{
  trace_opcode_reg_reg_sizes ("movzx", target, source, size_target, size_source);

  if (size_target <= size_source)
    fatal_compiler_error0 ("c_movzx_reg_reg(1)");

  valid_reg (target, size_target);
  valid_reg (source, size_source);

  if (size_source == 1)
  {
    reg_to_reg_2 (0x0F, 0xB6-1,
                  source, size_source,
                  target, size_target);
  }
  else if (size_source == 2)
  {
    reg_to_reg_2 (0x0F, 0xB7-1,
                  source, size_source,
                  target, size_target);
  }
  else  // 4 -> 8 bytes
  {
    if (source != target)
      c_mov_reg_reg (target, source, 4);   // implicit extension to 8 bytes
  }
}

/************************************************************************/

// move with zero-extension
// assert: size_target > size-source

public
void c_movzx_reg_mem (REG target, int size_target,
                      EA source, int size_source)
{
  trace_opcode_reg_mem_sizes ("movzx", target, source, size_target, size_source);

  if (size_target <= size_source)
    fatal_compiler_error0 ("c_movzx_reg_mem(1)");

  valid_reg (target, size_target);

  if (size_source == 1)
    mem_to_reg_2 (0x0F, 0xB6-1, target, source, size_target);
  else if (size_source == 2)
    mem_to_reg_2 (0x0F, 0xB7-1, target, source, size_target);
  else   // 4 -> 8 bytes
    c_mov_reg_mem (target, source, 4);   // implicit extension to 8 bytes
}

/************************************************************************/

// size == 1 is not allowed.
// size == 8 allowed for 64 bit

public
void c_imul_reg_reg (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("imul", target, source, size);

  if (size == 1)
    fatal_compiler_error0 ("c_imul_reg_reg(1)");

  valid_reg (target, size);
  valid_reg (source, size);

  reg_to_reg_2 (0x0F, 0xAE,
                source, size,
                target, size);
}

/************************************************************************/

// size == 1 is not allowed.
// size == 8 allowed for 64 bit

public
void c_imul_reg_mem (REG target, EA source, int size)
{
  trace_opcode_reg_mem ("imul", target, source, size);

  if (size == 1)
    fatal_compiler_error0 ("c_imul_reg_mem(1)");

  valid_reg (target, size);

  mem_to_reg_2 (0x0F, 0xAE, target, source, size);
}

/************************************************************************/

// size == 1 is not allowed.
// size == 8 allowed for 64 bit

// imm is limited to 32-bit.

public
void c_imul_reg_imm (REG target, int imm, int size)
{
  if (size == 1)
    fatal_compiler_error0 ("c_imul_reg_imm(1)");

  valid_reg (target, size);

  if (imm == 0)
  {
    c_xor_reg_reg (target, target, size);
  }
  else if (imm == 1)
  {
  }
  else if (imm == -1)
  {
    c_neg_reg (target, size);
  }
  else if (imm >= 2 && (imm & (imm-1)) == 0)   // power of 2 (2^1, .. 2^31)
  {
    int shifts = 0, power = 1;
    while (power < imm)
    {
      shifts++;
      power <<= 1;
    }

    c_shl_reg_imm (target, shifts, size);  // 1..31 (or 1..63 for 64-bit).
  }
  else if (imm >= -128 && imm <= 127)
  {
    if (imm == 3 || imm == 5 || imm == 9)
    {
      EA ea;

      clear ea;
      ea.scale = 8;
      while (ea.scale > imm)
        ea.scale >>= 1;

      ea.base = target;
      ea.index = target;
      ea.offset = 0;
      ea.reloc.kind = RELOC_NONE;
      ea.reloc.nr = 0;

      c_lea_reg_mem (target, ea, size => max(size,4));   // usually use 4,  when 8-byte operand use 8
    }
    else
    {
      trace_opcode_reg_imm ("imul", target, imm, size);

      reg_to_reg_1 (0x6A, target, (byte)target, source_is_reg => true, size);
      exe_write_int1 (imm);
    }
  }
  else
  {
    trace_opcode_reg_imm ("imul", target, imm, size);

    reg_to_reg_1 (0x68, target, (byte)target, source_is_reg => true, size);
    write_offset (min(4,size), imm);
  }
}

/************************************************************************/

// size == 1 is not allowed.
// imm is limited to 32-bit.

public
void c_imul_reg_reg_imm (REG target, REG source, int imm, int size)  // imm is limited to 32-bit.
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_reg_reg_imm ("imul", target, source, imm, size);

  if (size == 1)
    fatal_compiler_error0 ("c_imul_reg_reg_imm(1)");

  valid_reg (target, size);
  valid_reg (source, size);

  prefix_operand_size (size);

  evaluate_reg (source, modrm_reg_is_reg => true, size,
                (byte)target, is_reg => true, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  if (imm >= -128 && imm <= 127)
  {
    exe_write_byte (0x6A + W(size));
    exe_write_byte (mem);
    exe_write_int1 (imm);
  }
  else
  {
    exe_write_byte (0x68 + W(size));
    exe_write_byte (mem);
    write_offset (min(4,size), imm);
  }
}

/************************************************************************/

// size == 1 is not allowed.
// imm is limited to 32-bit.

public
void c_imul_reg_mem_imm (REG target, EA source, int imm, int size)
{
  int imm_size;
  
  trace_opcode_reg_mem_imm ("imul", target, source, imm, size);

  if (size == 1)
    fatal_compiler_error0 ("c_imul_reg_mem_imm(1)");

  valid_reg (target, size);

  if (imm >= -128 && imm <= 127)
  {
    mem_to_reg_1 (0x6A, target, source, size, 1);
    exe_write_int1 (imm);
  }
  else
  {
    imm_size = min(4,size);
    mem_to_reg_1 (0x68, target, source, size, imm_size);
    write_offset (imm_size, imm);
  }
}

/************************************************************************/

// AX      =  AL * source  (1 byte)
// DX:AX   =  AX * source  (2 byte)
// EDX:EAX = EAX * source  (4 byte)
// RDX:RAX = RAX * source  (8 byte) (64 bit only)

public
void c_umul_rax_reg (REG source, int size)
{
  trace_opcode_ext_reg_reg ("umul", RDX, RAX, source, size);
  valid_reg (source, size);
  reg_to_reg_1 (0xF6, source, 4, source_is_reg => false, size);
}

public
void c_umul_rax_mem (EA source, int size)
{
  trace_opcode_ext_reg_mem ("umul", RDX, RAX, source, size);
  mem_operand_1 (0xF6, 4, opcode2_is_reg => false, source, size);
}

/************************************************************************/

void signed_extend_for_idiv (int size)
{
  if (size == 4)
  {
    trace_opcode ("cdq (EAX -> EDX:EAX)");
    exe_write_byte (0x99);   // CDQ (EAX -> EDX:EAX)
  }
  else if (size == 8)
  {
    trace_opcode ("cqo (RAX -> RDX:RAX)");
    exe_write_byte (0x48);   // REX.W
    exe_write_byte (0x99);   // CQO (RAX -> RDX:RAX)
  }
  else
  {
    fatal_compiler_error0 ("signed_extend_for_idiv(1)");
  }
}

/************************************************************************/

// EAX  =   EAX  /  source  (4 byte)
// RAX  =   RAX  /  source  (8 byte) (64 bit only)

// destroys RDX !!

public
void c_idiv_rax_reg (REG source, int size)
{
  signed_extend_for_idiv (size);

  trace_opcode_reg_reg ("idiv", RAX, source, size);
  valid_reg (source, size);
  reg_to_reg_1 (0xF6, source, 7, source_is_reg => false, size);    // source & 7 are reversed !!
}

public
void c_idiv_rax_mem (EA source, int size)
{
  signed_extend_for_idiv (size);

  trace_opcode_reg_mem ("idiv", RAX, source, size);
  mem_operand_1 (0xF6, 7, opcode2_is_reg => false, source, size);
}

/************************************************************************/

// EDX  =   EAX  %  source  (4 byte)
// RDX  =   RAX  %  source  (8 byte) (64 bit only)

public
void c_imod_rax_reg (REG source, int size)
{
  signed_extend_for_idiv (size);

  trace_opcode_reg_reg ("idiv", RAX, source, size);
  valid_reg (source, size);
  reg_to_reg_1 (0xF6, source, 7, source_is_reg => false, size);   // operands are reversed
}

public
void c_imod_rax_mem (EA source, int size)
{
  signed_extend_for_idiv (size);

  trace_opcode_reg_mem ("idiv", RAX, source, size);
  mem_operand_1 (0xF6, 7, opcode2_is_reg => false, source, size);
}

/************************************************************************/

void zero_extend_for_udiv (int size)
{
  if (size == 4)
  {
    c_xor_reg_reg (RDX, RDX, 4);   // codes in 2 bytes
  }
  else if (size == 8)
  {
    c_xor_reg_reg (RDX, RDX, 8);
  }
  else
  {
    fatal_compiler_error0 ("zero_extend_for_udiv(1)");
  }
}

/************************************************************************/

// EAX  =   EAX  /  source  (4 byte)
// RAX  =   RAX  /  source  (8 byte) (64 bit only)

// destroys RDX !!

public
void c_udiv_rax_reg (REG source, int size)
{
  zero_extend_for_udiv (size);

  trace_opcode_reg_reg ("udiv", RAX, source, size);
  valid_reg (source, size);
  reg_to_reg_1 (0xF6, source, 6, source_is_reg => false, size);   // operands are reversed
}


public
void c_udiv_rax_mem (EA source, int size)
{
  zero_extend_for_udiv (size);

  trace_opcode_reg_mem ("udiv", RAX, source, size);
  mem_operand_1 (0xF6, 6, opcode2_is_reg => false, source, size);
}

/************************************************************************/

// EDX  =   EAX  %  source  (4 byte)
// RDX  =   RAX  %  source  (8 byte) (64 bit only)

public
void c_umod_rax_reg (REG source, int size)
{
  zero_extend_for_udiv (size);

  trace_opcode_reg_reg ("udiv", RAX, source, size);
  valid_reg (source, size);
  reg_to_reg_1 (0xF6, source, 6, source_is_reg => false, size);  // operands are reversed
}


public
void c_umod_rax_mem (EA source, int size)
{
  zero_extend_for_udiv (size);

  trace_opcode_reg_mem ("udiv", RAX, source, size);
  mem_operand_1 (0xF6, 6, opcode2_is_reg => false, source, size);
}

/************************************************************************/

// multiplies by powers of 2
// imm between 0..31 (or 0..63 for 64-bit).

public
void c_shl_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("shl", target, imm, size);

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shl_reg_imm(1)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shl_reg_imm(2)");

  valid_reg (target, size);

  if (imm == 0)  // nothing
    ;
  else if (imm == 1)  // single shift
    reg_to_reg_1 (0xD0, target, 4, source_is_reg => false, size);
  else
  {
    reg_to_reg_1 (0xC0, target, 4, source_is_reg => false, size);
    exe_write_byte ((byte)imm);
  }
}

/************************************************************************/

// cl between 0..31 (or 0..63 for 64-bit).

public
void c_shl_reg_CL (REG target, int size)
{
  trace_opcode_reg_reg ("shl", target, RCX, size);
  valid_reg (target, size);
  reg_to_reg_1 (0xD2, target, 4, source_is_reg => false, size);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).

public
void c_shl_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("shl", target, imm, size);

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shl_mem_imm(1)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shl_mem_imm(2)");

  if (imm == 0)  // nothing
    ;
  else if (imm == 1)  // single shift
    reg_to_mem_1 (0xD0, target, 4, source_is_reg => false, size, 0);
  else
  {
    reg_to_mem_1 (0xC0, target, 4, source_is_reg => false, size, 1);
    exe_write_byte ((byte)imm);
  }
}

/************************************************************************/

// cl between 0..31 (or 0..63 for 64-bit).

public
void c_shl_mem_CL (EA target, int size)
{
  trace_opcode_mem_reg ("shl", target, RCX, size);
  reg_to_mem_1 (0xD2, target, 4, source_is_reg => false, size, 0);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).
// signed divide by powers of 2 (preserves the sign bit)
// sar is not the same as idiv for negative values

public
void c_sar_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("sar", target, imm, size);

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_sar_reg_imm(1)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_sar_reg_imm(2)");

  valid_reg (target, size);

  if (imm == 0)  // nothing
    ;
  else if (imm == 1)  // single shift
    reg_to_reg_1 (0xD0, target, 7, source_is_reg => false, size);
  else
  {
    reg_to_reg_1 (0xC0, target, 7, source_is_reg => false, size);
    exe_write_byte ((byte)imm);
  }
}

/************************************************************************/

// CL between 0..31 (or 0..63 for 64-bit).

public
void c_sar_reg_CL (REG target, int size)
{
  trace_opcode_reg_reg ("sar", target, RCX, size);
  valid_reg (target, size);
  reg_to_reg_1 (0xD2, target, 7, source_is_reg => false, size);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).
// sar is not the same as idiv for negative values

public
void c_sar_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("sar", target, imm, size);

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_sar_mem_imm(1)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_sar_mem_imm(2)");

  if (imm == 0)  // nothing
    ;
  else if (imm == 1)  // single shift
    reg_to_mem_1 (0xD0, target, 7, source_is_reg => false, size, 0);
  else
  {
    reg_to_mem_1 (0xC0, target, 7, source_is_reg => false, size, 1);
    exe_write_byte ((byte)imm);
  }
}

/************************************************************************/

// CL between 0..31 (or 0..63 for 64-bit).

public
void c_sar_mem_CL (EA target, int size)
{
  trace_opcode_mem_reg ("sar", target, RCX, size);
  reg_to_mem_1 (0xD2, target, 7, source_is_reg => false, size, 0);
}

/************************************************************************/

// unsigned divide by powers of 2
// imm between 0..31 (or 0..63 for 64-bit).

public
void c_shr_reg_imm (REG target, int imm, int size)
{
  trace_opcode_reg_imm ("shr", target, imm, size);

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shr_reg_imm(1)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shr_reg_imm(2)");

  valid_reg (target, size);

  if (imm == 0)  // nothing
    ;
  else if (imm == 1)  // single shift
    reg_to_reg_1 (0xD0, target, 5, source_is_reg => false, size);
  else
  {
    reg_to_reg_1 (0xC0, target, 5, source_is_reg => false, size);
    exe_write_byte ((byte)imm);
  }
}

/************************************************************************/

// cl between 0..31 (or 0..63 for 64-bit).

public
void c_shr_reg_CL (REG target, int size)
{
  trace_opcode_reg_reg ("shr", target, RCX, size);
  valid_reg (target, size);
  reg_to_reg_1 (0xD2, target, 5, source_is_reg => false, size);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).

public
void c_shr_mem_imm (EA target, int imm, int size)
{
  trace_opcode_mem_imm ("shr", target, imm, size);

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shr_mem_imm(1)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shr_mem_imm(2)");

  if (imm == 0)  // nothing
    ;
  else if (imm == 1)  // single shift
    reg_to_mem_1 (0xD0, target, 5, source_is_reg => false, size, 0);
  else
  {
    reg_to_mem_1 (0xC0, target, 5, source_is_reg => false, size, 1);
    exe_write_byte ((byte)imm);
  }
}

/************************************************************************/

// cl between 0..31 (or 0..63 for 64-bit).

public
void c_shr_mem_CL (EA target, int size)
{
  trace_opcode_mem_reg ("shr", target, RCX, size);
  reg_to_mem_1 (0xD2, target, 5, source_is_reg => false, size, 0);
}

/************************************************************************/

// CL between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shld_reg_reg_CL (REG target, REG source, int size)
{
  trace_opcode_reg_reg ("shld", target, source, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shld_reg_reg_CL(1)");

  valid_reg (target, size);
  valid_reg (source, size);

  reg_to_reg_2 (0x0F, 0xA4,
                target, size,
                source, size);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shld_reg_reg_imm (REG target, REG source, int imm, int size)
{
  trace_opcode_reg_reg_imm ("shld", target, source, imm, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shld_reg_reg_imm(1)");

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shld_reg_reg_imm(2)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shld_reg_reg_imm(3)");

  valid_reg (target, size);
  valid_reg (source, size);

  reg_to_reg_2 (0x0F, 0xA3,
                target, size,
                source, size);

  exe_write_byte ((byte)imm);
}

/************************************************************************/

// CL between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shld_mem_reg_CL (EA target, REG source, int size)
{
  trace_opcode_mem_reg ("shld", target, source, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shld_mem_reg_CL(1)");

  valid_reg (source, size);

  reg_to_mem_2 (0x0F, 0xA4, target, source, size, 0);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shld_mem_reg_imm (EA target, REG source, int imm, int size)
{
  trace_opcode_mem_reg_imm ("shld", target, source, imm, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shld_mem_reg_imm(1)");

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shld_mem_reg_imm(2)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shld_mem_reg_imm(3)");

  valid_reg (source, size);

  reg_to_mem_2 (0x0F, 0xA3, target, source, size, 1);
  exe_write_byte ((byte)imm);
}

/************************************************************************/

// CL between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shrd_reg_reg_CL (REG target, REG source, int size)
{
  trace_opcode_reg_reg_reg ("shrd", target, source, RCX, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shrd_reg_reg_CL(1)");

  valid_reg (target, size);
  valid_reg (source, size);

  reg_to_reg_2 (0x0F, 0xAC,
                target, size,
                source, size);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shrd_reg_reg_imm (REG target, REG source, int imm, int size)
{
  trace_opcode_reg_reg_imm ("shrd", target, source, imm, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shrd_reg_reg_imm(1)");

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shrd_reg_reg_imm(2)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shrd_reg_reg_imm(3)");

  valid_reg (target, size);
  valid_reg (source, size);

  reg_to_reg_2 (0x0F, 0xAB,
                target, size,
                source, size);

  exe_write_byte ((byte)imm);
}

/************************************************************************/

// CL between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shrd_mem_reg_CL (EA target, REG source, int size)
{
  trace_opcode_mem_reg_reg ("shrd", target, source, RCX, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shrd_mem_reg_CL(1)");

  valid_reg (source, size);

  reg_to_mem_2 (0x0F, 0xAC, target, source, size, 0);
}

/************************************************************************/

// imm between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !

public
void c_shrd_mem_reg_imm (EA target, REG source, int imm, int size)
{
  trace_opcode_mem_reg_imm ("shrd", target, source, imm, size);

  if (size == 1)
    fatal_compiler_error0 ("c_shrd_mem_reg_imm(1)");

  if (imm < 0 || imm > 63)
    fatal_compiler_error0 ("c_shrd_mem_reg_imm(2)");
  if (address_size == 4 && imm > 31)
    fatal_compiler_error0 ("c_shrd_mem_reg_imm(3)");

  valid_reg (source, size);

  reg_to_mem_2 (0x0F, 0xAB, target, source, size, 1);
  exe_write_byte ((byte)imm);
}

/************************************************************************/

public
void c_rcl_reg (REG target, int size)
{
  trace_opcode_reg ("rcl", target, size);
  valid_reg (target, size);
  reg_to_reg_1 (0xD0, target, 2, source_is_reg => false, size);
}

/************************************************************************/

public
void c_rcl_mem (EA target, int size)
{
  trace_opcode_mem ("rcl", target, size);
  reg_to_mem_1 (0xD0, target, 2, source_is_reg => false, size, 0);
}

/************************************************************************/

// inverse Carry Flag

public
void c_cmc ()
{
  trace_opcode ("cmc");
  exe_write_byte (0xF5);
}

/************************************************************************/

public
void c_lock_prefix ()
{
  trace_opcode ("lock");
  exe_write_byte (0xF0);
}

/************************************************************************/

public
void c_pause ()
{
  trace_opcode ("pause");
  exe_write_byte (0xF3);
  exe_write_byte (0x90);
}

/************************************************************************/

/* UNUSED
// load random number in register
// carry flag set if successful

// Support for the RDRAND instruction is optional. On processors that support the instruction, CPUID
// Fn0000_0001_ECX[RDRAND] = 1.
// For more information on using the CPUID instruction, see the instruction reference page for the
// CPUID instruction on page 165. For a description of all feature flags related to instruction subset
// support, see Appendix D, “Instruction Subsets and CPUID Feature Flags,” on page 591.

void c_rdrand (REG target, int size)
{

  trace_opcode_reg ("neg", target, size);
  valid_reg (target, size);
  exe_write_byte (0x0F);
  reg_operand_1 (0xC7-1, 6, target, size);
}

// $  0:  0f c7 f0                rdrand eax
*/

/************************************************************************/

// load SF, ZF, AF, PF, and CF flags of the EFLAGS into AX
// for 32bit only

public
void c_lahf ()   // flags -> AH (32-bit only)
{
  trace_opcode ("lahf");
  if (address_size == 8)
    fatal_compiler_error0 ("c_lahf(1)");
  exe_write_byte (0x9F);
}

/************************************************************************/

// load AX into SF, ZF, AF, PF, and CF flags
// for 32bit only

public
void c_sahf ()   // AH -> flags (32-bit only)
{
  trace_opcode ("sahf");
  if (address_size == 8)
    fatal_compiler_error0 ("c_sahf(1)");
  exe_write_byte (0x9E);
}

/************************************************************************/

// for 32bit only

public
void c_mov_al_ah ()
{
  trace_opcode ("mov al,ah");
  if (address_size == 8)
    fatal_compiler_error0 ("c_mov_al_ah(1)");
  reg_to_reg_1 (0x88, RAX, (byte)RSP, source_is_reg => false, 1);
}

/************************************************************************/

// for 32bit only

public
void c_and_ah_al ()
{
  trace_opcode ("and ah,al");
  if (address_size == 8)
    fatal_compiler_error0 ("c_and_ah_al(1)");
  reg_to_reg_1 ((byte)(4*8), RSP, (byte)RAX, source_is_reg => false, 1);
}

/************************************************************************/

// FLD    Load floating-point value
// size is 4(float) or 8(double).

public
void c_fld (EA source, int size)
{
  trace_opcode_mem ("fld", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fld(1)");

  if (size == 4)
    mem_operand_1 (0xD9-1, 0, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDD-1, 0, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

public
void c_fld_zero ()
{
  trace_opcode ("fld #0.0");

  exe_write_byte (0xD9);
  exe_write_byte (0xEE);
}

/************************************************************************/

public
void c_fld_one ()
{
  trace_opcode ("fld #1.0");

  exe_write_byte (0xD9);
  exe_write_byte (0xE8);
}

/************************************************************************/

// FILD Load integer
// size is 2(int2), 4(int4) or 8(int8).

public
void c_fld_imem (EA source, int size)
{
  trace_opcode_mem ("fld int", source, size);

  if (size != 2 && size != 4 && size != 8)
    fatal_compiler_error0 ("c_fld_imem(1)");

  if (size == 2)
    mem_operand_1 (0xDF-1, 0, opcode2_is_reg => false, source, 4);
  else if (size == 4)
    mem_operand_1 (0xDB-1, 0, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDF-1, 5, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FSTP   Store floating-point value and pop
// size is 4(float) or 8(double).

public
void c_fstp (EA target, int size)
{
  trace_opcode_mem ("fstp", target, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fstp(1)");

  if (size == 4)
    mem_operand_1 (0xD9-1, 3, opcode2_is_reg => false, target, 4);
  else
    mem_operand_1 (0xDD-1, 3, opcode2_is_reg => false, target, 4);
}

/************************************************************************/

// FISTP Store integer (with rounding from status word)
// size is 2(int2), 4(int4) or 8(int8).

public
void c_fistp (EA target, int size)
{
  trace_opcode_mem ("fistp round_int", target, size);

  if (size != 2 && size != 4 && size != 8)
    fatal_compiler_error0 ("c_fistp(1)");

  if (size == 2)
    mem_operand_1 (0xDF-1, 3, opcode2_is_reg => false, target, 4);
  else if (size == 4)
    mem_operand_1 (0xDB-1, 3, opcode2_is_reg => false, target, 4);
  else
    mem_operand_1 (0xDF-1, 7, opcode2_is_reg => false, target, 4);
}

/************************************************************************/

// FISTTP Store integer (with truncation)
// size is 2(int2), 4(int4) or 8(int8).
// SSE3 !!

public
void c_fisttp (EA target, int size)
{
  trace_opcode_mem ("fisttp trunc_int", target, size);

  if (size != 2 && size != 4 && size != 8)
    fatal_compiler_error0 ("c_fisttp(1)");

  if (size == 2)
    mem_operand_1 (0xDF-1, 1, opcode2_is_reg => false, target, 4);
  else if (size == 4)
    mem_operand_1 (0xDB-1, 1, opcode2_is_reg => false, target, 4);
  else
    mem_operand_1 (0xDD-1, 1, opcode2_is_reg => false, target, 4);
}

/************************************************************************/

/*
void c_finit ()
{
  exe_write_byte (0x9B);
  exe_write_byte (0xDB);
  exe_write_byte (0xE3);
}
*/

/************************************************************************/

/*
void c_fnclex ()
{
  exe_write_byte (0xDB);
  exe_write_byte (0xE2);
}
*/

/************************************************************************/

public
void c_fldcw (EA source)   // 16-bit source operand (value 0x037F + (3 << 10))
{
  trace_opcode_mem ("fldcw", source, 2);
  mem_operand_1 (0xD9-1, 5, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

public
void c_fstcw (EA target)   // 16-bit operand
{
  trace_opcode_mem ("fstcw", target, 2);
  mem_operand_1 (0xD9-1, 7, opcode2_is_reg => false, target, 4);
}

/************************************************************************/

// push top float register on stack and pop
// size is 4(float) or 8(double).

public
void c_push_fltp (int size)
{
  EA ea;

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_push_flt(1)");

  if (address_size == 4)
    c_sub_reg_imm (RSP, size, address_size);
  else
    c_sub_reg_imm (RSP, 8, address_size);  // 48 83 EC 08

  if (address_size == 4)
    ESP_correction += size;
  else
    ESP_correction += 8;

  clear ea;
  ea.base = RSP;
  ea.index = NONE;
  ea.scale = 1;
  if (ESP_correction_on)
    ea.offset = -ESP_correction;    // cancel effect of ESP correction
  else
    ea.offset = 0;
  ea.reloc.kind = RELOC_NONE;
  ea.reloc.nr = 0;

  c_fstp (ea, size);
}

/************************************************************************/

// FADD Add float
// size is 4(float) or 8(double).

public
void c_fadd_fmem (EA source, int size)
{
  trace_opcode_mem ("fadd", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fadd_fmem(1)");

  if (size == 4)
    mem_operand_1 (0xD8-1, 0, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDC-1, 0, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FADD Add int to float
// size is 2(int2) or 4(int4).

public
void c_fadd_imem (EA source, int size)
{
  trace_opcode_mem ("fadd int", source, size);

  if (size != 2 && size != 4)
    fatal_compiler_error0 ("c_fadd_imem(1)");

  if (size == 4)
    mem_operand_1 (0xDA-1, 0, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDE-1, 0, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FADD Add float to float + pop

public
void c_faddp ()
{
  trace_opcode ("faddp");
  exe_write_byte (0xDE);
  exe_write_byte (0xC1);
}

/************************************************************************/

// FSUB Sub float
// size is 4(float) or 8(double).

public
void c_fsub_fmem (EA source, int size)
{
  trace_opcode_mem ("fsub", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fsub_fmem(1)");

  if (size == 4)
    mem_operand_1 (0xD8-1, 4, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDC-1, 4, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FSUB Sub int from float
// size is 2(int2) or 4(int4).

public
void c_fsub_imem (EA source, int size)
{
  trace_opcode_mem ("fsub int", source, size);

  if (size != 2 && size != 4)
    fatal_compiler_error0 ("c_fsub_imem(1)");

  if (size == 4)
    mem_operand_1 (0xDA-1, 4, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDE-1, 4, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FSUB Sub float from float + pop

public
void c_fsubp ()
{
  trace_opcode ("fsubp");
  exe_write_byte (0xDE);
  exe_write_byte (0xE9);
}

/************************************************************************/

// FSUBR Sub float
// size is 4(float) or 8(double).

public
void c_fsubr_fmem (EA source, int size)
{
  trace_opcode_mem ("fsubr", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fsubr_fmem(1)");

  if (size == 4)
    mem_operand_1 (0xD8-1, 5, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDC-1, 5, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FSUBR float - int -> float
// size is 2(int2) or 4(int4).

public
void c_fsubr_imem (EA source, int size)
{
  trace_opcode_mem ("fsubr int", source, size);

  if (size != 2 && size != 4)
    fatal_compiler_error0 ("c_fsubr_imem(1)");

  if (size == 4)
    mem_operand_1 (0xDA-1, 5, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDE-1, 5, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FSUB Sub float from float + pop

public
void c_fsubrp ()
{
  trace_opcode ("fsubrp");
  exe_write_byte (0xDE);
  exe_write_byte (0xE1);
}

/************************************************************************/

// FMUL float
// size is 4(float) or 8(double).

public
void c_fmul_fmem (EA source, int size)
{
  trace_opcode_mem ("fmul", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fmul_fmem(1)");

  if (size == 4)
    mem_operand_1 (0xD8-1, 1, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDC-1, 1, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FMUL Mul int from float
// size is 2(int2) or 4(int4).

public
void c_fmul_imem (EA source, int size)
{
  trace_opcode_mem ("fmul int", source, size);

  if (size != 2 && size != 4)
    fatal_compiler_error0 ("c_fmul_imem(1)");

  if (size == 4)
    mem_operand_1 (0xDA-1, 1, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDE-1, 1, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FMUL Mul float by float + pop

public
void c_fmulp ()
{
  trace_opcode ("fmulp");
  exe_write_byte (0xDE);
  exe_write_byte (0xC9);
}

/************************************************************************/

// FDIV float
// size is 4(float) or 8(double).

public
void c_fdiv_fmem (EA source, int size)
{
  trace_opcode_mem ("fdiv", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fdiv_fmem(1)");

  if (size == 4)
    mem_operand_1 (0xD8-1, 6, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDC-1, 6, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FDIV Div by int
// size is 2(int2) or 4(int4).

public
void c_fdiv_imem (EA source, int size)
{
  trace_opcode_mem ("fdiv int", source, size);

  if (size != 2 && size != 4)
    fatal_compiler_error0 ("c_fdiv_imem(1)");

  if (size == 4)
    mem_operand_1 (0xDA-1, 6, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDE-1, 6, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FDiv Div float by float + pop

public
void c_fdivp ()
{
  trace_opcode ("fdivp");
  exe_write_byte (0xDE);
  exe_write_byte (0xF9);
}

/************************************************************************/

// FDIVR float
// size is 4(float) or 8(double).

public
void c_fdivr_fmem (EA source, int size)
{
  trace_opcode_mem ("fdivr", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fdivr_fmem(1)");

  if (size == 4)
    mem_operand_1 (0xD8-1, 7, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDC-1, 7, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FDIVR Div int by float -> float
// size is 2(int2) or 4(int4).

public
void c_fdivr_imem (EA source, int size)
{
  trace_opcode_mem ("fdivr int", source, size);

  if (size != 2 && size != 4)
    fatal_compiler_error0 ("c_fdivr_imem(1)");

  if (size == 4)
    mem_operand_1 (0xDA-1, 7, opcode2_is_reg => false, source, 4);
  else
    mem_operand_1 (0xDE-1, 7, opcode2_is_reg => false, source, 4);
}

/************************************************************************/

// FDivR Div float by float + pop

public
void c_fdivrp ()
{
  trace_opcode ("fdivpr");
  exe_write_byte (0xDE);
  exe_write_byte (0xF1);
}

/************************************************************************/

public
void c_fneg ()
{
  trace_opcode ("fneg");
  exe_write_byte (0xD9);
  exe_write_byte (0xE0);
}

/************************************************************************/

// destroys RAX !
// !! not supported in 64-bit mode !!
// size is 4(float) or 8(double).
// attention: this instruction compares the memory operand with ST0, not the reverse !!!

public
void c_fcomp_fmem (EA source, int size)
{
  trace_opcode_mem ("fcomp", source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_fcomp_fmem(1)");

  if (address_size == 4)
  {
    if (size == 4)
      mem_operand_1 (0xD8-1, 3, opcode2_is_reg => false, source, 4);
    else
      mem_operand_1 (0xDC-1, 3, opcode2_is_reg => false, source, 4);

    exe_write_byte (0xDF);  // FNSTSW AX  (C3=bit 14(=), C2=bit 10, C1=bit 9, C0=bit 8(<))
    exe_write_byte (0xE0);

    c_sahf (); // load AH into flags SF, ZF, AF, PF, and CF (not always available in 64 bit)
  }
  else
  {
    fatal_compiler_error0 ("c_fcomp_fmem(2)");  // not supported (SAHF does not exist)
  }
}

/************************************************************************/

// destroys RAX in 32-bit mode
// attention: this instruction compares ST0 with ST1, not the reverse !!!

public
void c_fcomp ()
{
  if (address_size == 4)
  {
    trace_opcode ("fucompp");
    exe_write_byte (0xDA);  // FUCOMPP (compare 2 floats, set status word, pop stack twice)
    exe_write_byte (0xE9);

    trace_opcode ("fnstw ax");
    exe_write_byte (0xDF);  // FNSTSW AX  (C3=bit 14(=), C2=bit 10, C1=bit 9, C0=bit 8(<))
    exe_write_byte (0xE0);

    c_sahf ();              // load AH into flags SF, ZF, AF, PF, and CF (not always available in 64 bit)
  }
  else
  {
    trace_opcode ("fucomi");
    exe_write_byte (0xDB);  // FUCOMI  (no pop) - changes directly flags
    exe_write_byte (0xE9);
    trace_opcode ("fucompp");
    exe_write_byte (0xDA);  // FUCOMPP (pops twice)
    exe_write_byte (0xE9);
  }
}

/************************************************************************/

// this instruction compares ST0 with itself,
// sets the condition codes, and pops the stack.

public
void c_fpop ()
{
  trace_opcode ("fcomp");
  exe_write_byte (0xD8);  // FCOMP st0,st0
  exe_write_byte (0xD8);
}

/************************************************************************/

public
void c_code (byte b)
{
  {
    char str[16];
    sprintf (out str, "byte %u", b);
    trace_opcode (str);
  }
  exe_write_byte (b);
}

/************************************************************************/

public
void c_verify_ESP_correction ()
{
  if (!ESP_correction_on)
    fatal_compiler_error0 ("c_verify_ESP_correction(1)");
  if (ESP_correction != 0)
    fatal_compiler_error0 ("c_verify_ESP_correction(2)");
}

/************************************************************************/

public
void c_ESP_correction_ON (bool on)
{
  ESP_correction_on = on;
}

/************************************************************************/

public
bool is_ESP_correction_ON ()
{
  return ESP_correction_on;
}

/************************************************************************/

public
void reset_ESP_correction ()
{
  ESP_correction = 0;
}

/************************************************************************/

public
void add_ESP_correction (int offset)
{
  ESP_correction += offset;
}

/************************************************************************/

public
int ESP_correction_value ()
{
  return ESP_correction;
}

/************************************************************************/
// XMM opcodes
/************************************************************************/

public
void c_mov_xm_xm  (XM target, XM source, int size)  // dest = X0 to X15   size = 4 or 8
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_xm_xm ("mov", target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_mov_xm_xm(1)");

  exe_write_byte (size == 4 ? 0xF3 : 0xF2);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_reg (    modrm_reg         => (REG)(int)source, 
                    modrm_reg_is_reg  => true, 
                    modrm_reg_size    => 4,
                    reg               => (byte)target,
                    is_reg            => true, 
                    operand_size      => 4,
                out prex              => rex,
                out rex_prefix_needed => rex_prefix_needed, 
                out pmem              => mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x10);

  exe_write_byte (mem);
}


#if 0

https://defuse.ca/online-x86-assembler.htm

0:  f3 0f 10 c1             movss  xmm0,xmm1
4:  f3 0f 10 c2             movss  xmm0,xmm2
8:  f3 0f 10 c3             movss  xmm0,xmm3
c:  f3 41 0f 10 c0          movss  xmm0,xmm8
11: f3 41 0f 10 c1          movss  xmm0,xmm9
16: f3 41 0f 10 c2          movss  xmm0,xmm10
1b: f3 0f 10 c0             movss  xmm0,xmm0
1f: f3 0f 10 c8             movss  xmm1,xmm0
23: f3 0f 10 d0             movss  xmm2,xmm0
27: f3 44 0f 10 c0          movss  xmm8,xmm0
2c: f3 44 0f 10 c8          movss  xmm9,xmm0
31: f3 44 0f 10 d0          movss  xmm10,xmm0
#endif

/************************************************************************/

public
void c_mov_xm_mem (XM target, EA source,  int size)  // dest = X0 to X15   size = 4 or 8
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  trace_opcode_xm_mem ("mov", target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_mov_xm_mem(1)");

  exe_write_byte (size == 4 ? 0xF3 : 0xF2);

  valid_reg ((REG)(int)target, size);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_ea (source, (byte)target, is_reg => true, 4, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x10);

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/************************************************************************/

public
void c_mov_mem_xm (EA target, XM source,  int size)   // size = 4 or 8
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  trace_opcode_mem_xm ("mov", target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_mov_mem_xm(1)");

  exe_write_byte (size == 4 ? 0xF3 : 0xF2);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_ea (target, (byte)source, is_reg => true, 4, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x11);  // 11 here

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, target.offset + offset_corr, absolute, target.reloc);
}

/************************************************************************/

public
void c_mov_xm_reg (XM target, REG source,  int size)  // XM = 0 to 15   size = 4 or 8
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_xm_reg ("mov", target, source, size);

// MOVD xmm, reg32/mem32   66 (W0) 0F 6E /r   Move a 32-bit value from reg32/mem32 to xmm.
// MOVD xmm, reg64/mem64   66 (W1) 0F 6E /r   Move a 64-bit value from reg64/mem64 to xmm.

/*
0:  66 0f 6e c0             movd   xmm0,eax
4:  66 0f 6e c8             movd   xmm1,eax
8:  66 0f 6e d0             movd   xmm2,eax
c:  66 48 0f 6e c0          movq   xmm0,rax
11: 66 48 0f 6e c8          movq   xmm1,rax
16: 66 48 0f 6e d0          movq   xmm2,rax
1b: 66 0f 6e c0             movd   xmm0,eax
1f: 66 0f 6e c3             movd   xmm0,ebx
23: 66 0f 6e c1             movd   xmm0,ecx
27: 66 48 0f 6e c0          movq   xmm0,rax
2c: 66 48 0f 6e c3          movq   xmm0,rbx
31: 66 48 0f 6e c1          movq   xmm0,rcx
*/

  trace_opcode_xm_reg ("mov", target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_mov_xm_reg(1)");

  exe_write_byte (0x66);

  evaluate_reg (source, modrm_reg_is_reg => true, size,
                (byte)target, is_reg => true, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x6E);

  exe_write_byte (mem);
}

/************************************************************************/

public
void c_mov_reg_xm (REG target, XM source,  int size)  // XM = 0 to 15   size = 4 or 8
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_reg_xm ("mov", target, source, size);

// MOVD reg32/mem32, xmm   66 (W0) 0F 7E /r   Move a 32-bit value from xmm to reg32/mem32
// MOVD reg64/mem64, xmm   66 (W1) 0F 7E /r   Move a 64-bit value from xmm to reg64/mem64.

/*
0:  66 0f 7e c0             movd   eax,xmm0
4:  66 0f 7e c8             movd   eax,xmm1
8:  66 0f 7e d0             movd   eax,xmm2
c:  66 48 0f 7e c0          movq   rax,xmm0
11: 66 48 0f 7e c8          movq   rax,xmm1
16: 66 48 0f 7e d0          movq   rax,xmm2
1b: 66 0f 7e c0             movd   eax,xmm0
1f: 66 0f 7e c3             movd   ebx,xmm0
23: 66 0f 7e c1             movd   ecx,xmm0
27: 66 48 0f 7e c0          movq   rax,xmm0
2c: 66 48 0f 7e c3          movq   rbx,xmm0
31: 66 48 0f 7e c1          movq   rcx,xmm0
*/


  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_mov_xm_reg(1)");

  exe_write_byte (0x66);

  evaluate_reg (target, modrm_reg_is_reg => true, size,
                (byte)source, is_reg => true, size,
                out rex, out rex_prefix_needed, out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x7E);

  exe_write_byte (mem);
}

/************************************************************************/

// convert between single and double

public
void c_cnv_xm_xm (XM target, XM source, int target_size, int source_size)
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_xm_size_xm_size ("cnv", target, target_size, source, source_size);
  
  if (target_size != 4 && target_size != 8)
    fatal_compiler_error0 ("c_cnv_xm_xm(1)");
  if (source_size != 4 && source_size != 8)
    fatal_compiler_error0 ("c_cnv_xm_xm(2)");
  if (source_size == target_size)
    fatal_compiler_error0 ("c_cnv_xm_xm(3)");
  
  exe_write_byte (source_size == 4 ? 0xF3 : 0xF2);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_reg (    (REG)(int)source, 
                    modrm_reg_is_reg => true, 
                    4,
                    (byte)target, 
                    is_reg => true, 4,
                out rex, 
                out rex_prefix_needed, 
                out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x5A);

  exe_write_byte (mem);
}

/************************************************************************/

// load float from integer

public
void c_load_xm_size_ireg_size (XM target, REG source, int target_size, int source_size)   // int4/8 to float/double
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_xm_size_ireg_size ("load", target, target_size, source, source_size);
  
  if (target_size != 4 && target_size != 8)
    fatal_compiler_error0 ("c_load_xm_size_ireg_size(1)");
  if (source_size != 4 && source_size != 8)
    fatal_compiler_error0 ("c_load_xm_size_ireg_size(2)");
  
  exe_write_byte (target_size == 4 ? 0xF3 : 0xF2);

  evaluate_reg (    (REG)(int)source, 
                    modrm_reg_is_reg =>true, 
                    4,
                (byte)target, 
                    is_reg => true, 
                    source_size,
                out rex, 
                out rex_prefix_needed, 
                out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x2A);

  exe_write_byte (mem);
}


/*
CVTSI2SS xmm, reg/mem32 load 'int'  reg/mem, to 'float' in XMM (SSE)
CVTSI2SS xmm, reg/mem64 load 'long' reg/mem, to 'float' in XMM (SSE)  (64 bit mode only)
CVTSI2SS xmm1, reg32/mem32
  F3 (W0) 0F 2A /r 
  Converts a doubleword integer in reg32 or mem32 to a single-precision floating-point value in xmm1.
CVTSI2SS xmm1, reg64/mem64 
  F3 (W1) 0F 2A /r 
  Converts a quadword integer in reg64 or mem64 to a single-precision floating-point value in xmm1.

CVTSI2SD xmm, reg/mem32 load 'int'  reg/mem, to 'double' in XMM (SSE2)
CVTSI2SD xmm, reg/mem64 load 'long' reg/mem, to 'double' in XMM (SSE2)  (64 bit mode only)
CVTSI2SD xmm1, reg32/mem32 
  F2 (W0) 0F 2A /r 
  Converts a doubleword integer in reg32 or mem32 to a double-precision floating-point value in xmm1.
CVTSI2SD xmm1, reg64/mem64 
  F2 (W1) 0F 2A /r 
  Converts a quadword integer in reg64 or mem64 to a double-precision floating-point value in xmm1.
*/

/************************************************************************/

// load float from integer

public
void c_load_xm_size_imem_size (XM target,  EA source, int target_size, int source_size)   // int4/8 to float/double
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  trace_opcode_xm_size_imem_size ("load", target, source, target_size, source_size);
  
  if (target_size != 4 && target_size != 8)
    fatal_compiler_error0 ("c_load_xm_size_imem_size(1)");
  if (source_size != 4 && source_size != 8)
    fatal_compiler_error0 ("c_load_xm_size_imem_size(2)");

  exe_write_byte (target_size == 4 ? 0xF3 : 0xF2);

  evaluate_ea (source, (byte)target, is_reg => true, source_size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x2A);

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/*
CVTSI2SS xmm, reg/mem32 load 'int'  reg/mem, to 'float' in XMM (SSE)
CVTSI2SS xmm, reg/mem64 load 'long' reg/mem, to 'float' in XMM (SSE)  (64 bit mode only)
CVTSI2SS xmm1, reg32/mem32
  F3 (W0) 0F 2A /r 
  Converts a doubleword integer in reg32 or mem32 to a single-precision floating-point value in xmm1.
CVTSI2SS xmm1, reg64/mem64 
  F3 (W1) 0F 2A /r 
  Converts a quadword integer in reg64 or mem64 to a single-precision floating-point value in xmm1.

CVTSI2SD xmm, reg/mem32 load 'int'  reg/mem, to 'double' in XMM (SSE2)
CVTSI2SD xmm, reg/mem64 load 'long' reg/mem, to 'double' in XMM (SSE2)  (64 bit mode only)
CVTSI2SD xmm1, reg32/mem32 
  F2 (W0) 0F 2A /r 
  Converts a doubleword integer in reg32 or mem32 to a double-precision floating-point value in xmm1.
CVTSI2SD xmm1, reg64/mem64 
  F2 (W1) 0F 2A /r 
  Converts a quadword integer in reg64 or mem64 to a double-precision floating-point value in xmm1.
*/

/************************************************************************/

// trunc+store float as integer

public
void c_store_ireg_size_xm_size (REG target, XM source, int target_size, int source_size)   // float/double to int4/8
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_ireg_size_xm_size ("store", target, target_size, source, source_size);
  
  if (target_size != 4 && target_size != 8)
    fatal_compiler_error0 ("c_store_ireg_size_xm_size(1)");
  if (source_size != 4 && source_size != 8)
    fatal_compiler_error0 ("c_store_ireg_size_xm_size(2)");
  
  exe_write_byte (source_size == 4 ? 0xF3 : 0xF2);

  evaluate_reg (    (REG)(int)source, 
                    modrm_reg_is_reg => true, 
                    4,
                    (byte)target, 
                    is_reg => true, 
                    target_size,
                out rex, 
                out rex_prefix_needed, 
                out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x2C);

  exe_write_byte (mem);
}

/*
CVTTSS2SI reg32,xmm/mem32  convert 'float' to 'int' in reg with truncation (SSE2)
CVTTSS2SI reg64,xmm/mem32  convert 'float' to 'long' in reg with truncation (SSE2)  (64 bit mode only)
CVTTSS2SI reg32, xmm1/mem32 
  F3 (W0) 0F 2C /r 
  Converts a single-precision floating-point value in xmm1 or mem32 to a 32-bit integer value in reg32. Truncates inexact result.
CVTTSS2SI reg64, xmm1/mem64 
  F3 (W1) 0F 2C /r 
  Converts a single-precision floating-point value in xmm1 or mem64 to a 64-bit integer value in reg64. Truncates inexact result.

CVTTSD2SI reg32,xmm/mem64  convert 'double' to 'int' in reg with truncation (SSE2)
CVTTSD2SI reg64,xmm/mem64  convert 'double' to 'long' in reg with truncation (SSE2)  (64 bit mode only)
CVTTSD2SI reg32, xmm1/mem64 
  F2 (W0) 0F 2C /r 
  Converts a packed double-precision floating-point value in xmm1 or mem64 to a doubleword integer in reg32. Truncates inexact result.
CVTTSD2SI reg64, xmm1/mem64 
  F2 (W1) 0F 2C /r 
  Converts a packed double-precision floating-point value in xmm1 or mem64 to a quadword integer in reg64.Truncates inexact result.
*/

/************************************************************************/

// trunc+store float as integer

public
void c_store_ireg_size_memf_size (REG target, EA source, int target_size, int source_size)   // float/double to int4/8
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  trace_opcode_ireg_size_memf_size ("store", target, source, target_size, source_size);
  
  if (target_size != 4 && target_size != 8)
    fatal_compiler_error0 ("c_store_imem_size_memf_size(1)");
  if (source_size != 4 && source_size != 8)
    fatal_compiler_error0 ("c_store_imem_size_memf_size(2)");

  exe_write_byte (source_size == 4 ? 0xF3 : 0xF2);

  evaluate_ea (source, (byte)target, is_reg => true, target_size, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x2C);

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}


/*
CVTTSS2SI reg32,xmm/mem32  convert 'float' to 'int' in reg with truncation (SSE2)
CVTTSS2SI reg64,xmm/mem32  convert 'float' to 'long' in reg with truncation (SSE2)  (64 bit mode only)
CVTTSS2SI reg32, xmm1/mem32 
  F3 (W0) 0F 2C /r 
  Converts a single-precision floating-point value in xmm1 or mem32 to a 32-bit integer value in reg32. Truncates inexact result.
CVTTSS2SI reg64, xmm1/mem64 
  F3 (W1) 0F 2C /r 
  Converts a single-precision floating-point value in xmm1 or mem64 to a 64-bit integer value in reg64. Truncates inexact result.

CVTTSD2SI reg32,xmm/mem64  convert 'double' to 'int' in reg with truncation (SSE2)
CVTTSD2SI reg64,xmm/mem64  convert 'double' to 'long' in reg with truncation (SSE2)  (64 bit mode only)
CVTTSD2SI reg32, xmm1/mem64 
  F2 (W0) 0F 2C /r 
  Converts a packed double-precision floating-point value in xmm1 or mem64 to a doubleword integer in reg32. Truncates inexact result.
CVTTSD2SI reg64, xmm1/mem64 
  F2 (W1) 0F 2C /r 
  Converts a packed double-precision floating-point value in xmm1 or mem64 to a quadword integer in reg64.Truncates inexact result.
*/

/************************************************************************/

void normal_xmm_xmm (string opcode, byte code, XM target, XM source, int size)
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_xm_xm (opcode, target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("normal_xmm_xmm");

  exe_write_byte (size == 4 ? 0xF3 : 0xF2);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_reg (    (REG)(int)source, 
                    modrm_reg_is_reg => true, 
                    4,
                    (byte)target, 
                    is_reg => true, 
                    4,
                out rex, 
                out rex_prefix_needed, 
                out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (code);

  exe_write_byte (mem);
}


void normal_xmm_mem (string opcode, byte code, XM target, EA source, int size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  trace_opcode_xm_mem (opcode, target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("normal_xmm_mem(1)");

  exe_write_byte (size == 4 ? 0xF3 : 0xF2);

  valid_reg ((REG)(int)target, size);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_ea (    source, 
                   (byte)target, 
                   is_reg => true, 
                   4, 
               out rex, 
               out rex_prefix_needed, 
               out mem, 
               out sib_follows, 
               out sib, 
               out offset_size, 
               out offset_corr, 
               out absolute);
               
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (code);

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/************************************************************************/

public
void c_add_xm_xm  (XM target, XM source, int size)   // XM = 0 to 15   size = 4 or 8
{
  normal_xmm_xmm ("add", 0x58, target, source, size);
}

public
void c_add_xm_mem (XM target, EA source,  int size)
{
  normal_xmm_mem ("add", 0x58, target, source, size);
}

public
void c_sub_xm_xm  (XM target, XM source, int size)   // XM = 0 to 15   size = 4 or 8
{
  normal_xmm_xmm ("sub", 0x5C, target, source, size);
}

public
void c_sub_xm_mem (XM target, EA source,  int size)
{
  normal_xmm_mem ("sub", 0x5C, target, source, size);
}

public
void c_mul_xm_xm (XM target, XM source, int size)   // XM = 0 to 15   size = 4 or 8
{
  normal_xmm_xmm ("mul", 0x59, target, source, size);
}

public
void c_mul_xm_mem (XM target, EA source,  int size)
{
  normal_xmm_mem ("mul", 0x59, target, source, size);
}

// only 16 / 22 cycles
public
void c_div_xm_xm (XM target, XM source, int size)   // XM = 0 to 15   size = 4 or 8
{
  normal_xmm_xmm ("div", 0x5E, target, source, size);
}

public
void c_div_xm_mem (XM target, EA source,  int size)
{
  normal_xmm_mem ("div", 0x5E, target, source, size);
}

public
void c_sqr_xm_xm (XM target, XM source, int size)   // XM = 0 to 15   size = 4 or 8
{
  normal_xmm_xmm ("sqr", 0x51, target, source, size);
}

public
void c_sqr_xm_mem (XM target, EA source,  int size)
{
  normal_xmm_mem ("sqr", 0x51, target, source, size);
}

/************************************************************************/

public
void c_cmp_xm_xm (XM target, XM source, int size)   // XM = 0 to 15   size = 4 or 8
{
  byte rex, mem;
  bool rex_prefix_needed;

  trace_opcode_xm_xm ("cmp", target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_cmp_xm_xm(1)");

  if (size == 8)
    exe_write_byte (0x66);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_reg (    (REG)(int)source, 
                    modrm_reg_is_reg => true, 
                    4,
                    (byte)target, 
                    is_reg => true, 
                    4,
                out rex, 
                out rex_prefix_needed, 
                out mem);

  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x2E);

  exe_write_byte (mem);
}

/************************************************************************/

public
void c_cmp_xm_mem (XM target, EA source,  int size)
{
  byte rex, mem, sib;
  bool rex_prefix_needed, sib_follows;
  int  offset_size, offset_corr;
  bool absolute;

  trace_opcode_xm_mem ("cmp", target, source, size);

  if (size != 4 && size != 8)
    fatal_compiler_error0 ("c_cmp_xm_mem(1)");

  if (size == 8)
    exe_write_byte (0x66);

  valid_reg ((REG)(int)target, size);

  // size is always 4 bytes (we need no REX byte to indicate 8 bytes, this is already signaled by first byte opcode)
  evaluate_ea (source, (byte)target, is_reg => true, 4, out rex, out rex_prefix_needed, out mem, out sib_follows, out sib, out offset_size, out offset_corr, out absolute);
  if (rex_prefix_needed)
    exe_write_byte (0x40 + rex);

  exe_write_byte (0x0F);
  exe_write_byte (0x2E);

  exe_write_byte (mem);
  if (sib_follows)
    exe_write_byte (sib);

  write_offset_reloc (offset_size, source.offset + offset_corr, absolute, source.reloc);
}

/************************************************************************/
