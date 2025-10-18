
// as86.c

/***************************************************************************/

from std use tracing;
use ../pcodes, ../error, ../goptions, ../dbginfo, ../common, ../dllnames, ../codout;
use ../pool;
use a86, pe, aslab, astacks, init86;

/***********************************************************************************/

uint4 g_extra_frame_bytes_rip;
bool  g_is_thread_entry_point, g_is_callback;
int   g_save_registers_for_callback_at_ofs;
int   g_current_unit_key;
int   g_current_source_line;
int   g_after_i, g_after_f, g_after_a;
int   g_callee_saved_regs;  // android only

/***********************************************************************************/

// operate tree that stores labels for jumping under the function's code and raise int5 exceptions

int ll_operate (bool^ user,
                LINE_LABEL_DATA p)
{
  _unused user;
  if (g_current_source_line != p.line)
  {
    dbg_store_line (p.line, (int)current_RIP());
    g_current_source_line = p.line;
  }

  exe_declare_near_label (p.label_nr);
  c_int (5);

  return 0;
}

/***********************************************************************************/

void validate_xx_stacks_for_pcode (PCODE c)
{
  int  di, df, da;
  char typ;
  int  j;

  di = 0;
  df = 0;
  da = 0;

  {
    ref string input = pcode_table[(int)c].input;

    for (j=0; j<input'length; j++)
    {
      typ = input[j];

      if (typ == 'b' || typ == 'i' || typ == 'l')
        di++;
      else if (typ == 'f' || typ == 'd')
        df++;
      else if (typ == 'a')
        da++;
      else
        abort;
    }

    if (di > istack_count || df > fstack_count || da > astack_count)
      fatal_compiler_error0 ("validate_xx_stacks_for_pcode(1)");
  }

  {
    ref string output = pcode_table[(int)c].output;

    for (j=0; j<output'length; j++)
    {
      typ = output[j];

      if (typ == 'b' || typ == 'i' || typ == 'l')
        di--;
      else if (typ == 'f' || typ == 'd')
        df--;
      else if (typ == 'a')
        da--;
      else
        abort;
    }
  }

  g_after_i = istack_count - di;
  g_after_f = fstack_count - df;
  g_after_a = astack_count - da;
}

/***********************************************************************************/

void validate_xx_stacks_before_pcode ()
{
  validate_xx_stacks_for_pcode (g_current_pcode);
}

/***********************************************************************************/

void validate_xx_stacks_for_additional_pcode (PCODE c)
{
  if (g_after_i != istack_count || g_after_f != fstack_count || g_after_a != astack_count)
    fatal_compiler_error0 ("generate_asm_for_ipc (bad xx_stack indexes)");

  g_current_pcode = c;

  validate_xx_stacks_for_pcode (c);
}

/***********************************************************************************/

// used for 12 different float to integer conversions

void save_8087_control_word (int ofs)
{
  EA ea;
  clear ea;
  ea.base = RSP;  ea.index = NONE;   ea.scale = 1;  ea.offset = ofs;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_fstcw (ea);   // store control word in memory (2 bytes) (note: each thread has a separate value)
}

/***********************************************************************************/

void restore_8087_control_word (int ofs)
{
  EA ea;
  clear ea;
  ea.base = RSP;  ea.index = NONE;   ea.scale = 1;  ea.offset = ofs;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_fldcw (ea);   // change control word (note: each thread has a separate value)
}

/***********************************************************************************/

void change_8087_mode_to_trunc ()
{
  EA ea;
  clear ea;
  ea.base = NONE;  ea.index = NONE;   ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_POOL; ea.reloc.nr = pool_nr_87_control_word_trunc;
  c_fldcw (ea);   // change control word (note: each thread has a separate value)
}

/***********************************************************************************/

public
void call_kernel (string func, int nb_arguments)
{
  EA ea;

  clear ea;
  ea.base   = NONE;
  ea.index  = NONE;
  ea.scale  = 1;
  ea.offset = 0;
  ea.reloc.kind = RELOC_DLL;
  ea.reloc.nr = insert_dll_name ("KERNEL32.DLL", func);

  if (address_size == 4)
    c_call_indirect (ea, 4, nb_arguments);
  else
  {
    if ((ESP_correction_value() & 15) != 0)    // ESP not aligned at M16
      fatal_compiler_error0 ("call_kernel(1)");

    if (option_check_stack_M16_alignment)
      c_call_relative (label_check_stack_M16_alignment, 0);

    c_call_indirect (ea, 8, 0);   // call without fixing ESP, because Windows does not fix stack in 64 bit mode !
  }
}

/***********************************************************************************/

// to be called before pushing or setting parameters for Windows call

public
void align_stack ()
{
  if (address_size == 8)
  {
    g_alignment_needed = ((ESP_correction_value() & 15) != 0);  // ESP not aligned at M16
    if (g_alignment_needed)
    {
      c_push_reg (RAX, address_size);   // 1 byte opcode
//      add_ESP_correction (address_size);
    }
  }
}

/***********************************************************************************/

public
void alloc_shadow_space ()
{
  if (address_size == 8)
  {
    if ((ESP_correction_value() & 15) != 0)    // ESP not aligned at M16
      fatal_compiler_error0 ("alloc_shadow_space(1)");

    g_shadow_space = 32;
    c_sub_reg_imm (RSP, g_shadow_space, 8);   // allocate shadow space
    add_ESP_correction (g_shadow_space);
  }
}

/***********************************************************************************/

public
void free_shadow_space ()
{
  if (address_size == 8)
  {
    c_add_reg_imm (RSP, g_shadow_space, 8);   // free shadow space
    add_ESP_correction (-g_shadow_space);
  }
}

/***********************************************************************************/

// to be called after freeing shadow space

public
void dealign_stack ()
{
  if (address_size == 8)
  {
    if (g_alignment_needed)
    {
      free_register (RCX);
      c_pop_reg (RCX, address_size);
//      add_ESP_correction (-address_size);
    }
  }
}

/***********************************************************************************/

// does both above together

public
void free_shadow_space_and_dealign_stack ()
{
  if (address_size == 8)
  {
    int sp = g_shadow_space;
    if (g_alignment_needed)
      sp += 8;
    c_add_reg_imm (RSP, sp, 8);   // free shadow space + stack dealignment
    add_ESP_correction (-sp);
  }
}

/***********************************************************************************/

int lshifts_of (int value)
{
  int n = value;
  int r = 0;

  while (n > 1)
  {
    r++;
    n >>= 1;
  }
  return r;
}

/***********************************************************************************/

int nb_register_moves_to_copy_size (int size)
{
  int count = 0;
  int rest = size;

  if (address_size == 8)
  {
    count += (rest >> 3);
    rest &= 7;
  }

  count += (rest >> 2);
  rest &= 3;

  count += (rest >> 1);
  rest &= 1;

  count += rest;

  return count;
}

/***********************************************************************************/

#begin unsafe

public
void generate_asm_for_function (bool is_main, int nb_labels)
{
  byte*  mem;
  int    mem_size;
  int    mem_offset;

  pe.exe_reset_dll_move_chain ();
  pe.exe_allocate_near_label_table (nb_labels);

  out_obtain_pcode_mem (out mem, out mem_size);   // get block of pcodes to translate into x86
  mem_offset = 0;

  g_extra_frame_bytes_rip = 0;
  g_frame_size = 0;
  g_extra_bytes = 0;
  g_stack_alignment = 0;
  g_is_thread_entry_point = false;
  g_is_callback = false;

  c_ESP_correction_ON (false);


  create_ll_tree ();

  while (mem_offset < mem_size)
  {
    g_current_pcode'byte = mem[mem_offset:2];
    mem_offset += 2;

    validate_xx_stacks_before_pcode ();
    clear_pcode_temporary_zone ();

    if (g_tracing)
      trace ("  %s", pcode_table[(int)g_current_pcode].name);

    switch (g_current_pcode)
    {
      // --------------
      // 1. memory move
      // --------------

      // push constant value on int_stack         (   -->  <int> )

      case P_CTE_BOOL:    //  <int1>   ; push constant uint1      on int_stack as 1 byte
      {
        int   i;
        NODE* p;

        i = *((int1 *)&mem[mem_offset]);
        mem_offset += 1;

        if (g_tracing)
          trace ("  #%d\n", i);

        p = &istack[istack_count++];
        p->typ  = 'b';
        p->kind = INT_CONSTANT;
        p->icte = i;
      }
      break;

      case P_CTE_4:       //  <int4>   ; push constant int4/uint4 on int_stack as 4 bytes
      {
        int4 i;
        NODE* p;

        i = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("  #%d\n", i);

        p = &istack[istack_count++];
        p->typ  = 'i';
        p->kind = INT_CONSTANT;
        p->icte = i;
      }
      break;

      case P_CTE_8:       //  <int8>   ; push constant int8       on int_stack as 8 bytes
      {
        int8 l;
        NODE* p;

        l = *((int8 *)&mem[mem_offset]);
        mem_offset += 8;

        if (g_tracing)
          trace ("  #%d\n", l);

        p = &istack[istack_count++];
        p->typ  = 'l';
        p->kind = INT_CONSTANT;
        p->icte = l;
      }
      break;


      // push constant value on float_stack       (   -->  <float> )

      case P_CTE_FLT4:    //  <float4> ; push constant float4     on float_stack as 4 bytes
      {
        float f;
        NODE  *p;

        f = *((float *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
//          trace ("  #%f\n", f);
          trace ("  # decimal %d\n", *(int*)&f);

        p = &fstack[fstack_count++];
        p->typ  = 'f';
        p->kind = FLOAT_CONSTANT;
        p->fcte = f;
      }
      break;

      case P_CTE_FLT8:    //  <float8> ; push constant float8     on float_stack as 8 bytes
      {
        double d;
        NODE   *p;

        d = *((double *)&mem[mem_offset]);
        mem_offset += 8;

        if (g_tracing)
//          trace ("  #%f\n", d);
          trace ("  # decimal %d\n", *(long*)&d);

        p = &fstack[fstack_count++];
        p->typ  = 'd';
        p->kind = FLOAT_CONSTANT;
        p->fcte = d;
      }
      break;


      // push null value on addr_stack             (   -->  <addr> )

      case P_CTE_NULL:    //           ; push constant 0          on addr_stack
      {
        NODE* a;

        if (g_tracing)
          trace ("  null\n");

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = NONE;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;


      case P_FUNC_LABEL:    // <func_label_4>          ; each function must start with a func_label
      {
        int4 nr;

        nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label of func #%d\n", nr);

        exe_add_func_label (nr);
      }
      break;


      // load address of code, constant, global, local   (  -->  <addr> )

      case P_LOAD_CODE:     // <func_label_4>          ; load addr of func_label on addr_stack
      {
        int4 nr;
        NODE* a;

        nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" address of func #%d\n", nr);

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base    = NONE;
        a->ea.index   = NONE;
        a->ea.scale   = 1;
        a->ea.offset  = 0;
        a->ea.reloc.kind = RELOC_FUNC;
        a->ea.reloc.nr   = nr;
      }
      break;

      case P_LOAD_DLL:      // <dll_func_id4>          ; load addr of dll_func_nr on addr_stack
      {
        int4 nr;
        NODE* a;

        nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" address of dll #%d\n", nr);

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base    = NONE;
        a->ea.index   = NONE;
        a->ea.scale   = 1;
        a->ea.offset  = 0;
        a->ea.reloc.kind = RELOC_DLL;
        a->ea.reloc.nr   = nr;
      }
      break;


      case P_LOAD_CONST:    // <pool_id8>              ; load addr on addr_stack
      {
        NODE* a;
        int8 nr;

        nr = *((int8 *)&mem[mem_offset]);
        mem_offset += 8;

        if (g_tracing)
          trace (" address of pool #%d\n", nr);

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = NONE;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_POOL;
        a->ea.reloc.nr   = nr;
      }
      break;


      case P_LOAD_GLOBAL:    // <global_offset_8>       ; load addr on addr_stack
      {
        NODE* a;
        int8 offset;

        offset = *((int8 *)&mem[mem_offset]);
        mem_offset += 8;

        if (g_tracing)
          trace (" address of global #%d\n", offset);

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = NONE;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = (int4)offset;
        a->ea.reloc.kind = RELOC_GLOBAL;
        a->ea.reloc.nr   = 0;
      }
      break;


      case P_LOAD_LOCAL:    // <local_offset_4>        ; load addr on addr_stack
      {
        NODE* a;
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" address of local #%d\n", offset);

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = RBP;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = offset;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;



      // load item from memory address   (  <addr>  -->  <int> )

      case P_VALUE_BOOL:    // pop addr_stack, load bool1,      push on int_stack as 1 byte
      {
        NODE* a, i;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count-1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            i = &istack[istack_count++];
            i->typ = 'b';
            i->kind = MEMORY;
            i->ea = a->ea;
          }
          break;

          case MEMORY:
          {
            REG r = allocate_register (address_size, with_crash_node => true, crash_node => *a);

            c_mov_reg_mem (r, a->ea, address_size);

            i = &istack[istack_count++];
            i->typ  = 'b';
            i->kind = MEMORY;
            i->ea.base   = r;
            i->ea.index  = NONE;
            i->ea.scale  = 1;
            i->ea.offset = 0;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      // load item from memory address   (  <addr>  -->  <int> )

      case P_VALUE_I1:    // pop addr_stack, load int1,       push on int_stack as 4 bytes sign-extended
      {
        NODE* a, i;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            REG r = allocate_register (size => 1, with_crash_node => true, *a);  // prefer 1-byte register in case of following store

            c_movsx_reg_mem (r, 4, a->ea, size_source => 1);

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
            i->reg_high = NONE;
          }
          break;

          case MEMORY:
          {
            REG r;
            EA  ea;

            r = allocate_register (size => 1, with_crash_node => true, *a);     // prefer 1-byte register in case of following store

            c_mov_reg_mem (r, a->ea, address_size);

            clear ea;
            ea.base   = r;
            ea.index  = NONE;
            ea.scale  = 1;
            ea.offset = 0;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            c_movsx_reg_mem (r, 4, ea, size_source => 1);

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
            i->reg_high = NONE;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      // load item from memory address   (  <addr>  -->  <int> )

      case P_VALUE_I2:    // pop addr_stack, load int2,       push on int_stack as 4 bytes sign-extended
      {
        NODE* a, i;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            REG r;

            r = allocate_register (4, true, *a);

            c_movsx_reg_mem (r, 4, a->ea, size_source => 2);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }
          break;

          case MEMORY:
          {
            REG r;
            EA  ea;

            r = allocate_register (size => address_size, with_crash_node => true, *a);

            c_mov_reg_mem (r, a->ea, address_size);

            clear ea;
            ea.base   = r;
            ea.index  = NONE;
            ea.scale  = 1;
            ea.offset = 0;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            c_movsx_reg_mem (r, 4, ea, size_source => 2);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      case P_VALUE_U1:    // pop addr_stack, load uint1,      push on int_stack as 4 bytes zero-extended
      {
        NODE* a, i;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            REG r;

            r = allocate_register (size => 1, with_crash_node => true, *a);   // prefer 1-byte register in case of following store

            c_movzx_reg_mem (r, 4, a->ea, size_source => 1);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }
          break;

          case MEMORY:
          {
            REG r;
            EA  ea;

            r = allocate_register (size => 1, with_crash_node => true, *a);   // prefer 1-byte register in case of following store

            c_mov_reg_mem (r, a->ea, address_size);

            clear ea;
            ea.base   = r;
            ea.index  = NONE;
            ea.scale  = 1;
            ea.offset = 0;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            c_movzx_reg_mem (r, 4, ea, size_source => 1);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      case P_VALUE_U2:    // pop addr_stack, load uint2,      push on int_stack as 4 bytes zero-extended
      {
        NODE* a, i;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            REG r;

            r = allocate_register (4, with_crash_node => true, *a);

            c_movzx_reg_mem (r, 4, a->ea, size_source => 2);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }
          break;

          case MEMORY:
          {
            REG r;
            EA  ea;

            r = allocate_register (size => address_size, with_crash_node => true, *a);

            c_mov_reg_mem (r, a->ea, address_size);

            clear ea;
            ea.base   = r;
            ea.index  = NONE;
            ea.scale  = 1;
            ea.offset = 0;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            c_movzx_reg_mem (r, 4, ea, size_source => 2);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      case P_VALUE_4:    // pop addr_stack, load int4/uint4, push on int_stack as 4 bytes
      {
        NODE* a, i;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea = a->ea;
          }
          break;

          case MEMORY:    // memory location contains address
          {
            REG r;

            r = allocate_register (size => address_size, with_crash_node => true, *a);

            c_mov_reg_mem (r, a->ea, address_size);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = MEMORY;
            i->ea.base   = r;
            i->ea.index  = NONE;
            i->ea.scale  = 1;
            i->ea.offset = 0;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      case P_VALUE_8:    // pop addr_stack, load int8,       push on int_stack as 8 bytes
      {
        NODE* a, i;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            i = &istack[istack_count++];
            i->typ  = 'l';
            i->kind = MEMORY;
            i->ea = a->ea;
          }
          break;

          case MEMORY:
          {
            REG r;

            r = allocate_register (size => address_size, with_crash_node => true, *a);   // we can reuse a's registers

            c_mov_reg_mem (r, a->ea, address_size);

            i = &istack[istack_count++];
            i->typ  = 'l';
            i->kind = MEMORY;
            i->ea.base   = r;
            i->ea.index  = NONE;
            i->ea.scale  = 1;
            i->ea.offset = 0;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      // load item from memory address   (  <addr>  -->  <float> )

      case P_VALUE_FLT4:   // pop addr_stack, load float,  push on float_stack as 4 bytes
      {
        NODE* a, f;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            f = &fstack[fstack_count++];
            f->typ = 'f';
            f->kind = MEMORY;
            f->ea = a->ea;
          }
          break;

          case MEMORY:    // memory location contains address
          {
            REG r;

            r = allocate_register (size => address_size, with_crash_node => true, *a);

            c_mov_reg_mem (r, a->ea, address_size);

            f = &fstack[fstack_count++];
            f->typ  = 'f';
            f->kind = MEMORY;
            f->ea.base   = r;
            f->ea.index  = NONE;
            f->ea.scale  = 1;
            f->ea.offset = 0;
            f->ea.reloc.kind = RELOC_NONE;
            f->ea.reloc.nr   = 0;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      // load item from memory address   (  <addr>  -->  <float> )

      case P_VALUE_FLT8:   // pop addr_stack, load double, push on float_stack as 8 bytes
      {
        NODE* a, f;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            f = &fstack[fstack_count++];
            f->typ = 'd';
            f->kind = MEMORY;
            f->ea = a->ea;
          }
          break;

          case MEMORY:    // memory location contains address
          {
            REG r;

            r = allocate_register (size => address_size, with_crash_node => true, *a);

            c_mov_reg_mem (r, a->ea, address_size);

            f = &fstack[fstack_count++];
            f->typ  = 'd';
            f->kind = MEMORY;
            f->ea.base   = r;
            f->ea.index  = NONE;
            f->ea.scale  = 1;
            f->ea.offset = 0;
            f->ea.reloc.kind = RELOC_NONE;
            f->ea.reloc.nr   = 0;
          }
          break;

          default:
            abort;
        }

        astack_count--;
      }
      break;


      // load item from memory address   (  <addr>  -->  <addr>  )

      case P_VALUE_ADDR:    // pop addr_stack, load address, push on addr_stack
      {
        NODE* a;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 1];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            a->kind = MEMORY;
          }
          break;

          case MEMORY:
          {
            REG r;

            r = allocate_register (address_size, with_crash_node => true, *a);   // allows reusing registers of node a

            c_mov_reg_mem (r, a->ea, address_size);

            a->typ = 'a';
            a->kind = MEMORY;
            a->ea.base   = r;
            a->ea.index  = NONE;
            a->ea.scale  = 1;
            a->ea.offset = 0;
            a->ea.reloc.kind = RELOC_NONE;
            a->ea.reloc.nr   = 0;
          }
          break;

          default:
            abort;
        }
      }
      break;


      // load item from memory address   (  <addr>  <top_addr>  -->  <addr>  <top_addr> )

      case P_VALUE_ADDR1:    // take addr, load address, put back <addr>
      {
        NODE* a;

        if (g_tracing)
          trace ("\n");

        a = &astack[astack_count - 2];
        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
          {
            a->kind = MEMORY;
          }
          break;

          case MEMORY:
          {
            REG r;

            r = allocate_register (address_size, with_crash_node => true, *a);   // allows reusing registers of node a

            c_mov_reg_mem (r, a->ea, address_size);

            a->typ  = 'a';
            a->kind = MEMORY;
            a->ea.base   = r;
            a->ea.index  = NONE;
            a->ea.scale  = 1;
            a->ea.offset = 0;
            a->ea.reloc.kind = RELOC_NONE;
            a->ea.reloc.nr   = 0;
          }
          break;

          default:
            abort;
        }
      }
      break;


      // store value to memory address     (  <addr> -->  /  )
      //                                   (  <int>       /  )

      case P_STORE_BOOL:    // pop int_stack, pop addr_stack, store 1 byte (used for bool)
      {
        NODE* a, i;

        a = &astack[astack_count - 1];
        i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);  // convert 'a' operand from memory into effective address

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 1, with_crash_node => true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 1);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          flush_int1_in_register (ref *i);     // stores i into byte-aligned register

          c_mov_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;

      case P_STORE_1:    // pop int_stack,   pop addr_stack, store 1 byte (used for int1, uint1)
      {
        NODE* a, i;

        a = &astack[astack_count - 1];
        i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 1, with_crash_node => true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 1);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          flush_int1_in_register (ref *i);     // stores i into byte-aligned register

          c_mov_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;

      case P_STORE_2:    // pop int_stack,   pop addr_stack, store 2 bytes (used for int2, uint2)
      {
        NODE* a, i;

        a = &astack[astack_count - 1];
        i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 2);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 2, with_crash_node => true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 2);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          c_mov_mem_reg (a->ea, i->reg, /* size= */ 2);
        }

        astack_count--;
        istack_count--;
      }
      break;

      case P_STORE_4:    // pop int_stack,   pop addr_stack, store 4 bytes (used for int4, uint4)
      {
        NODE* a, i;

        a = &astack[astack_count - 1];
        i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 4);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 4, true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 4);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          c_mov_mem_reg (a->ea, i->reg, /* size= */ 4);
        }

        astack_count--;
        istack_count--;
      }
      break;

      case P_STORE_8:    // pop int_stack,   pop addr_stack, store 8 bytes (using for int8)
      {
        NODE* a, i;

        a = &astack[astack_count - 1];
        i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (address_size == 4 || i->icte < -(int8)2147483648 || i->icte > +2147483647)
          {
            c_mov_mem_imm (a->ea, (int4)i->icte, size => 4);    // imm limited to 32-bit.

            a->ea.offset += 4;
            c_mov_mem_imm (a->ea, (int4)(i->icte >> 32), size => 4);    // imm limited to 32-bit.
          }
          else  // 64-bit with 32-bit signed-extended imm.
          {
            c_mov_mem_imm (a->ea, (int4)i->icte, size => 8);    // sign-extended 32-bit imm.
          }
        }
        else  // memory or register
        {
          if (address_size == 4)
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              REG r;

              r = allocate_register (size => 4, false, *i);   // don't reuse any registers (false)

              c_mov_reg_mem (r, i->ea, /* size= */ 4);
              c_mov_mem_reg (a->ea, r, /* size= */ 4);

              i->ea.offset += 4;
              a->ea.offset += 4;

              c_mov_reg_mem (r, i->ea, /* size= */ 4);
              c_mov_mem_reg (a->ea, r, /* size= */ 4);
            }
            else  // INT_REGISTER
            {
              c_mov_mem_reg (a->ea, i->reg, /* size= */ 4);
              a->ea.offset += 4;
              c_mov_mem_reg (a->ea, i->reg_high, /* size= */ 4);
            }
          }
          else    // 64-bit
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              REG r;

              r = allocate_register (size => 8, true, *i);   // allows reusing registers of node i

              c_mov_reg_mem (r, i->ea, /* size= */ 8);

              i->kind = INT_REGISTER;
              i->reg  = r;
              i->reg_high = NONE;
            }

            c_mov_mem_reg (a->ea, i->reg, /* size= */ 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      // store value to memory address     (  <addr> -->  /  )
      //                                   (  <float>     /  )

      case P_STORE_FLT4:  // pop float_stack, pop addr_stack, store 4 bytes (used for float4)
      {
        NODE* a, f;

        a = &astack[astack_count - 1];
        f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            float fcte = (float)f->fcte;
            c_mov_mem_imm (a->ea, *((int *)&fcte), size => 4);    // imm limited to 32-bit.
          }
          break;

          case FLOAT_REGISTER:
          {
            c_fstp (a->ea, 4);
          }
          break;

          case MEMORY:  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 4, true, *f);   // allows reusing registers of node f

            c_mov_reg_mem (r, f->ea, /* size= */ 4);
            c_mov_mem_reg (a->ea, r, /* size= */ 4);
          }
          break;

          default:
            abort;
        }

        astack_count--;
        fstack_count--;
      }
      break;


      case P_STORE_FLT8:  // pop float_stack, pop addr_stack, store 8 bytes (used for float8)
      {
        NODE* a, f;

        a = &astack[astack_count - 1];
        f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            flush_float_in_register (ref *f);    // store constant in pool and load it in register
            c_fstp (a->ea, 8);
          }
          break;

          case FLOAT_REGISTER:
          {
            c_fstp (a->ea, 8);
          }
          break;

          case MEMORY:  // load memory operand into register
          {
            flush_float_in_register (ref *f);
            c_fstp (a->ea, 8);
          }
          break;

          default:
            abort;
        }

        astack_count--;
        fstack_count--;
      }
      break;


      // store value to memory address     (  <target_addr> <source_addr> -->  /  )

      case P_STORE_ADDR:    // pop source_addr, pop target_addr, store 4 or 8 addr bytes
      {
        NODE* adr, val;

        val = &astack[astack_count - 1];
        adr = &astack[astack_count - 2];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *adr);    // convert 'a' operand from memory into effective address

        if (val->kind == MEMORY)  // load memory operand into register
        {
          REG r;

          r = allocate_register (size => address_size, true, *val);   // allows reusing registers of node val

          c_mov_reg_mem (r, val->ea, /* size= */ address_size);

          val->kind = EFFECTIVE_ADDRESS;
          val->ea.base   = r;
          val->ea.index  = NONE;
          val->ea.scale  = 1;
          val->ea.offset = 0;
          val->ea.reloc.kind = RELOC_NONE;
          val->ea.reloc.nr   = 0;
        }


        if (val->ea.base == NONE && val->ea.index == NONE && val->ea.reloc.kind == RELOC_NONE)
        {
          c_mov_mem_imm (adr->ea, val->ea.offset, size => address_size);    // imm limited to 32-bit.
        }
        else if (val->ea.base == NONE && val->ea.index == NONE)
        {
          c_mov_mem_imm_reloc (adr->ea, val->ea.offset, address_size, val->ea.reloc);
        }
        else if (val->ea.base != NONE && val->ea.index == NONE &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_mov_mem_reg (adr->ea, val->ea.base, /* size= */ address_size);
        }
        else if (val->ea.base == NONE && val->ea.index != NONE && val->ea.scale == 1 &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_mov_mem_reg (adr->ea, val->ea.index, /* size= */ address_size);
        }
        else   // general case
        {
          REG r;
          r = allocate_register (address_size, true, *val);   // we can reuse val's registers
          c_lea_reg_mem (r, val->ea, size => address_size);
          c_mov_mem_reg (adr->ea, r, /* size= */ address_size);
        }

        astack_count -= 2;
      }
      break;


      // store block        (  <target_addr>   <source_addr>   <size_uint4>  -->   )

      case P_COPY_BLOCK:          // pop <size_uint4>, pop <source_addr>, pop <target_addr>
      {
        NODE* nsize, src, dst;

        nsize = &istack[istack_count - 1];
        src = &astack[astack_count - 1];
        dst = &astack[astack_count - 2];

        if (g_tracing)
          trace ("\n");

        if (nsize->kind == INT_CONSTANT &&
            nb_register_moves_to_copy_size ((int)nsize->icte) <= 4)   // max 4 moves
        {
          REG[1] r_dummy;
          int    siz, chunk;

          flush_effective_address (ref *src);    // convert 'a' operand from memory into effective address
          flush_effective_address (ref *dst);    // convert 'a' operand from memory into effective address

          siz = (int)nsize->icte;    // size to copy

          if ((siz & 1) == 1 && try_allocate_registers (size => 1, false, *nsize, 1, out r_dummy) < 0)
          {
            // could not allocate byte-aligned register : free one, then max 3 registers are in use and 1 byte register is free

            REG r = allocate_register (size => address_size, true, *dst);

            _unused r_dummy;

            c_lea_reg_mem (r, dst->ea, size => address_size);

            dst->ea.base   = r;
            dst->ea.index  = NONE;
            dst->ea.scale  = 1;
            dst->ea.offset = 0;
            dst->ea.reloc.kind = RELOC_NONE;
            dst->ea.reloc.nr   = 0;
          }

          // now allocation of any size should work !
          if (address_size == 4)
          {
            REG r = allocate_register ((siz & 1) == 1 ? 1 : address_size, false, *nsize);  // we need a byte-aligned register

            chunk = address_size;

            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_reg_mem (r, src->ea, /* size= */ chunk);
                c_mov_mem_reg (dst->ea, r, /* size= */ chunk);
                src->ea.offset += chunk;
                dst->ea.offset += chunk;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }
          }
          else   // 64 bit
          {
            int ri;
            REG regs[4];

            allocate_registers (size => address_size, false, *nsize, 4, out regs);  // 4 new regs

            chunk = address_size;
            siz = (int)nsize->icte;
            ri = 0;
            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_reg_mem (regs[ri], src->ea, /* size= */ chunk);
                src->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }

            chunk = address_size;
            siz = (int)nsize->icte;
            ri = 0;
            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_mem_reg (dst->ea, regs[ri], /* size= */ chunk);
                dst->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }
          }
        }
        else
        {
          // all registers must be flushed to temporaries before calling Windows API
          store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
          store_all_float_registers_in_temporaries ();

          if (address_size == 4)
          {
            push_int4 (*nsize);
            push_addr (*src);
            push_addr (*dst);
            call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
          }
          else
          {
            align_stack ();

            load_int4_into_reg (ref *nsize, R8);
            load_addr_into_reg (ref *src,  RDX);
            load_addr_into_reg (ref *dst,  RCX);

            alloc_shadow_space ();
            call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
            free_shadow_space_and_dealign_stack ();
          }
        }

        istack_count--;
        astack_count -= 2;
      }
      break;


      // store block        (  <target_addr>   <source_addr>   <size_uint4>  -->   )

      case P_ORDERED_COPY_BLOCK:  // same as above for overlapping areas (copy high-to-low or low-to-high)
      {
        NODE* nsize, src, dst;

        nsize = &istack[istack_count - 1];
        src   = &astack[astack_count - 1];
        dst   = &astack[astack_count - 2];

        if (g_tracing)
          trace ("\n");

        if (nsize->kind == INT_CONSTANT && nsize->icte == 0)
        {
          // do nothing
        }
        else if (nsize->kind == INT_CONSTANT &&
                 nb_register_moves_to_copy_size ((int)nsize->icte) <= (address_size == 4 ? 1 : 4))   // max 1 or 4 moves
        {
          REG[1] r_dummy;

          flush_effective_address (ref *src);    // convert 'a' operand from memory into effective address
          flush_effective_address (ref *dst);    // convert 'a' operand from memory into effective address

          if (nsize->icte == 1 && try_allocate_registers (size => 1, false, *nsize, 1, out r_dummy) < 0)
          {
            // could not allocate byte-aligned register : free one, then max 3 registers are in use and 1 byte register is free
            REG r = allocate_register (size => address_size, true, *dst);

            _unused r_dummy;

            c_lea_reg_mem (r, dst->ea, size => address_size);

            dst->ea.base   = r;
            dst->ea.index  = NONE;
            dst->ea.scale  = 1;
            dst->ea.offset = 0;
            dst->ea.reloc.kind = RELOC_NONE;
            dst->ea.reloc.nr   = 0;
          }

          // now allocation of byte-aligned register should work

          if (address_size == 4)
          {
            REG r = allocate_register ((int)nsize->icte, false, *nsize);

            c_mov_reg_mem (r, src->ea, /* size= */ (int)nsize->icte);
            c_mov_mem_reg (dst->ea, r, /* size= */ (int)nsize->icte);
          }
          else   // 64 bit
          {
            int siz, ri, chunk;
            REG regs[4];

            allocate_registers (size => address_size, false, *nsize, 4, out regs);  // 4 new regs

            chunk = address_size;
            siz = (int)nsize->icte;
            ri = 0;
            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_reg_mem (regs[ri], src->ea, /* size= */ chunk);
                src->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }

            chunk = address_size;
            siz = (int)nsize->icte;
            ri = 0;
            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_mem_reg (dst->ea, regs[ri], /* size= */ chunk);
                dst->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }
          }
        }
        else
        {
          // all registers must be flushed to temporaries before calling Windows API
          store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
          store_all_float_registers_in_temporaries ();

          if (address_size == 4)
          {
            push_int4 (*nsize);
            push_addr (*src);
            push_addr (*dst);
            call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
          }
          else
          {
            align_stack ();

            load_int4_into_reg (ref *nsize, R8);
            load_addr_into_reg (ref *src,   RDX);
            load_addr_into_reg (ref *dst,   RCX);

            alloc_shadow_space ();
            call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
            free_shadow_space_and_dealign_stack ();
          }
        }

        istack_count--;
        astack_count -= 2;
      }
      break;


      // multi copy        (  <target_addr>   <count4>   <source_addr>   -->   <target_addr>  )
      // copy an array element multiple times on a target address

      case P_MULTI_COPY:         // <size4>  <near_label_4> <near_label_4>
                                 // pop <source_addr>, pop <repeat4>, pop <target_addr>
      {                          // while (count4) {memcpy (target, source, size); target+=size; count4--;}
        NODE* count, src, dst;
        int4  siz, lab1, lab2;

        siz  = *((int4 *)&mem[mem_offset]);
        lab1 = *((int4 *)&mem[mem_offset+4]);
        lab2 = *((int4 *)&mem[mem_offset+8]);
        mem_offset += 12;

        src = &astack[astack_count - 1];
        dst = &astack[astack_count - 2];
        count = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if (siz == 0 || (count->kind == INT_CONSTANT && count->icte == 0))
        {
          // do nothing
        }
        else
        {
          REG r;

          if (siz == 1 || siz == 2 || siz == 4 || siz == address_size)
          {
            flush_effective_address (ref *src);  // convert 'a' operand from memory into effective address
            flush_effective_address (ref *dst);  // convert 'a' operand from memory into effective address
            flush_int4_in_register_for_modif (ref *count);

            if (dst->ea.index != NONE)   // index is not free : make it free
            {
              r = allocate_register (size => address_size, true, *dst);

              c_lea_reg_mem (r, dst->ea, size => address_size);

              dst->ea.base   = r;
              dst->ea.index  = NONE;
              dst->ea.scale  = 1;
              dst->ea.offset = 0;
              dst->ea.reloc.kind = RELOC_NONE;
              dst->ea.reloc.nr   = 0;
            }

            // allocation of byte-aligned register should work
            r = allocate_register (siz, false, *src);       // r = data to copy 'count' times

            // algo: jmp L1
            //     L2:
            //       memcpy (target, source, size);
            //       target += size;
            //     L1:
            //       count4--;
            //       if (count4 >= 0) goto L2   (jns)

            c_mov_reg_mem (r, src->ea, /* size= */ siz);       // load source in register
            c_jump_relative (lab1);

            exe_declare_near_label (lab2);
            {
              EA ea_dst;

              ea_dst = dst->ea;
              ea_dst.index = count->reg;
              ea_dst.scale = siz;

              c_mov_mem_reg (ea_dst, r, /* size= */ siz);
            }

            exe_declare_near_label (lab1);
            c_dec_reg (count->reg, 4);
            c_jcond_raw (9, lab2);   // jns

            // note that destination address (dst) is kept for the next pcode !
          }
          else       // any size
          {
            store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
            store_all_float_registers_in_temporaries ();

            // DLL call below will save/restore ESI, EDI, EBX registers, so we can use them

            load_addr_into_reg (ref *src, RSI);
            load_addr_into_reg (ref *dst, RDI);
            load_int4_into_reg (ref *count, RBX);

            if (*(PCODE*)&mem[mem_offset] != P_DROP_ADDR)
            {
              // store dst in temporary to keep it for next pcode, so we can freely use the RDI register
              store_node_in_temp (ref *dst);
            }

            // algo: jmp L1
            //     L2:
            //       memcpy (target, source, size);
            //       target += size;
            //     L1:
            //       count4--;
            //       if (count4>=0) goto L2

            c_jump_relative (lab1);
            exe_declare_near_label (lab2);

            if (address_size == 4)
            {
              c_push_imm (siz);    // size
              c_push_reg (RSI, 4);  // src
              c_push_reg (RDI, 4);  // dest

              call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
            }
            else
            {
              align_stack ();

              c_mov_reg_reg (RCX, RDI, 8);  // dest in rcx
              c_mov_reg_reg (RDX, RSI, 8);  // src in rdx
              c_mov_reg_imm (R8, siz, 4);   // size in R8

              alloc_shadow_space ();
              call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
              free_shadow_space_and_dealign_stack ();
            }

            c_add_reg_imm (RDI, siz, address_size);    // increase target address for next loop

            exe_declare_near_label (lab1);

            c_dec_reg (RBX, 4);
            c_jcond_raw (9, lab2);   // jns

            // note that destination address (dst) is kept for the next pcode !
          }
        }

        istack_count--;
        astack_count--;
      }
      break;


  // ------------
  // 2. branching
  // ------------

      //    (  -->  )       (near labels numbering starts again at 0 at each next function)

      case P_NEAR_LABEL:       //  <near_label_4>   ; (each goto/btrue/bfalse/jump/switch target must have a near label)
      {
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("  define label #%d\n", near_label_nr);

        exe_declare_near_label (near_label_nr);
      }
      break;


      //    (  -->  )

      case P_GOTO:          // <near_label_4>
      {
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("  label #%d\n", near_label_nr);

        c_jump_relative (near_label_nr);
      }
      break;


      //    (  <bool>  -->   )
      // pop bool1 from int_stack and test it.
      // note: P_TSTBOOL is ALWAYS followed immediately either by P_BTRUE or P_BFALSE
      // intel:  test al,al

      case P_TSTBOOL:
      {
        PCODE c;
        int4  near_label_nr;
        NODE  *i;

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE)
          fatal_compiler_error0 ("asm(P_TSTBOOL) : null");

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("  %s  lab #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

        store_all_registers_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        i = &istack[istack_count - 1];
        if (i->kind == INT_CONSTANT)
        {
          if ((i->icte != 0 && c == P_BTRUE) || (i->icte == 0 && c == P_BFALSE))
            c_jump_relative (near_label_nr);
        }
        else
        {
          switch (i->kind)
          {
            case INT_REGISTER:
              flush_int1_in_register (ref *i);     // stores i into byte-aligned register
              c_test_reg_reg (i->reg, i->reg, 1);
              break;

            case MEMORY:
              c_test_mem_imm (i->ea, 255, 1);
              break;

            default:
              abort;
          }

          c_jcond ((c == P_BTRUE ? CMP_NOT_EQUAL : CMP_EQUAL),
                   /*signed=*/ false,
                   near_label_nr);
        }

        istack_count--;

        validate_xx_stacks_for_additional_pcode (c);
      }
      break;


      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

      //   (  <bool>  <bool>  -->   )
      case P_CMP_BOOL:   // <mask>  ; compare two bool   on int_stack - 1 byte unsigned
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_BOOL) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        i2 = &istack[istack_count - 1];
        i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        if (i1->kind == MEMORY && i2->kind == INT_CONSTANT)
        {
          c_cmp_mem_imm (i1->ea, (int4)i2->icte, size => 1);
        }
        else
        {
          flush_int1_in_register (ref *i1);     // stores i1 into byte-aligned register

          switch (i2->kind)
          {
            case INT_CONSTANT:
              c_cmp_reg_imm (i1->reg, (int4)i2->icte, size => 1);    // imm limited to 32-bit
              break;

            case INT_REGISTER:
              c_cmp_reg_reg (i1->reg, i2->reg, size => 1);
              break;

            case MEMORY:
              c_cmp_reg_mem (i1->reg, i2->ea, size => 1);
              break;

            default:
              abort;
          }
        }

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register (size => 1, false, *i1);
          c_setcond_reg ((COMPARISON_FLAG)mask, /*signed=*/ false, r, size => 1);

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !
          store_all_float_registers_in_temporaries ();

          c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ false,  near_label_nr);
        }
      }
      break;


      //   (  <int4_a>  <int4_b>  -->   )

      case P_CMP_S4:   // <mask>  ; compare two int4   on int_stack     - signed
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_S4) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        i2 = &istack[istack_count - 1];   // kind can be INT_CONSTANT, INT_REGISTER or MEMORY
        i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        if (i1->kind == MEMORY && i2->kind == INT_CONSTANT)
        {
          c_cmp_mem_imm (i1->ea, (int4)i2->icte, size => 4);    // imm limited to 32-bit
        }
        else
        {
          flush_int4_in_register (ref *i1);

          switch (i2->kind)
          {
            case INT_CONSTANT:
              c_cmp_reg_imm (i1->reg, (int4)i2->icte, size => 4);    // imm limited to 32-bit
              break;

            case INT_REGISTER:
              c_cmp_reg_reg (i1->reg, i2->reg, size => 4);
              break;

            case MEMORY:
              c_cmp_reg_mem (i1->reg, i2->ea, size => 4);
              break;

            default:
              abort;
          }
        }

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register (size => 1, false, *i1);
          c_setcond_reg ((COMPARISON_FLAG)mask, /*signed=*/ true,  r, size => 1);

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !
          store_all_float_registers_in_temporaries ();

          c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ true,  near_label_nr);
        }
      }
      break;


      //   (  <uint4_a>  <uint4_b>  -->   )

      case P_CMP_U4:   // <mask>  ; compare two uint4   on int_stack     - unsigned
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_U4) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);


        i2 = &istack[istack_count - 1];
        i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        if (i1->kind == MEMORY && i2->kind == INT_CONSTANT)
        {
          c_cmp_mem_imm (i1->ea, (int4)i2->icte, size => 4);    // imm limited to 32-bit
        }
        else
        {
          flush_int4_in_register (ref *i1);

          switch (i2->kind)
          {
            case INT_CONSTANT:
              c_cmp_reg_imm (i1->reg, (int4)i2->icte, size => 4);    // imm limited to 32-bit
              break;

            case INT_REGISTER:
              c_cmp_reg_reg (i1->reg, i2->reg, size => 4);
              break;

            case MEMORY:
              c_cmp_reg_mem (i1->reg, i2->ea, size => 4);
              break;

            default:
              abort;
          }
        }

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register (size => 1, false, *i1);
          c_setcond_reg ((COMPARISON_FLAG)mask, /*signed=*/ false,  r, size => 1);

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !
          store_all_float_registers_in_temporaries ();

          c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ false,  near_label_nr);
        }
      }
      break;


      //   (  <int8_a>  <int8_b>  -->   )

      case P_CMP_S8:   // <mask>  ; compare two int8   on int_stack     - signed
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_S8) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        i2 = &istack[istack_count - 1];
        i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        if (address_size == 4)
        {
          REG regs[2];

          /*
            P_CMP_S8  mask=3
          >     cmp_4 low1,low2
                PUSHFD         opcode 9C    ; save ZF  (check that this pushes 4 bytes)
                POP    esi
                mov    EDI,high1
          >     sbb_4  EDI,high2
                PUSHFD         opcode 9C    ; save final flags
                or     esi,-1-64              ; mask to set all flags set except Z flags
                and     dword ptr[esp],esi  ; and to mask Z flag (but this destroys OV flag)

          >     cmp_4 low1,low2
                mov    EDI,high1
          >     sbb_4  EDI,high2           ; OV flag computed again
                POPFD                      ; opcode 9D to load final flags with Z flag adapted
          */

          flush_int8_in_registers_for_modif (ref *i1);


          allocate_registers (4, with_crash_node => false, *i1, 2, out regs);   // allocate 2 registers of 4 bytes


          // compare low word

          switch (i2->kind)
          {
            case INT_CONSTANT:
              c_cmp_reg_imm (i1->reg, (int4)i2->icte, size => 4);
              break;

            case INT_REGISTER:
              c_cmp_reg_reg (i1->reg, i2->reg, size => 4);
              break;

            case MEMORY:
              c_cmp_reg_mem (i1->reg, i2->ea, size => 4);
              break;

            default:
              abort;
          }

          c_push_flags ();
          c_pop_reg (regs[0], 4);

          c_mov_reg_reg (regs[1], i1->reg_high, 4);

          // subtract high word with borrow

          switch (i2->kind)
          {
            case INT_CONSTANT:
              c_sbb_reg_imm (regs[1], (int4)(i2->icte >> 32), size => 4);
              break;

            case INT_REGISTER:
              c_sbb_reg_reg (regs[1], i2->reg_high, size => 4);
              break;

            case MEMORY:
              i2->ea.offset += 4;
              c_sbb_reg_mem (regs[1], i2->ea, size => 4);
              i2->ea.offset -= 4;
              break;

            default:
              abort;
          }

          c_push_flags ();

          c_or_reg_imm (regs[0], -1-64, 4);

          c_ESP_correction_ON (false);
          {
            EA ea;
            clear ea;
            ea.base = RSP;  ea.index = NONE;   ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
            c_and_mem_reg (ea, regs[0], 4);
          }
          c_ESP_correction_ON (true);

          // compare low word

          switch (i2->kind)
          {
            case INT_CONSTANT:
              c_cmp_reg_imm (i1->reg, (int4)i2->icte, size => 4);
              break;

            case INT_REGISTER:
              c_cmp_reg_reg (i1->reg, i2->reg, size => 4);
              break;

            case MEMORY:
              c_cmp_reg_mem (i1->reg, i2->ea, size => 4);
              break;

            default:
              abort;
          }

          c_mov_reg_reg (regs[1], i1->reg_high, 4);

          // subtract high word with borrow

          switch (i2->kind)
          {
            case INT_CONSTANT:
              c_sbb_reg_imm (regs[1], (int4)(i2->icte >> 32), size => 4);
              break;

            case INT_REGISTER:
              c_sbb_reg_reg (regs[1], i2->reg_high, size => 4);
              break;

            case MEMORY:
              i2->ea.offset += 4;
              c_sbb_reg_mem (regs[1], i2->ea, size => 4);
              i2->ea.offset -= 4;
              break;

            default:
              abort;
          }

          c_pop_flags ();
        }
        else    // 64-bit
        {
          if (i1->kind == MEMORY && i2->kind == INT_CONSTANT &&
              i2->icte >= -(int8)2147483648 && i2->icte <= +2147483647)  // small int8
          {
            c_cmp_mem_imm (i1->ea, (int4)i2->icte, size => 8);    // imm limited to 32-bit
          }
          else
          {
            flush_int8_in_registers_for_modif (ref *i1);

            switch (i2->kind)
            {
              case INT_CONSTANT:
              {
                if (i2->icte >= -(int8)2147483648 && i2->icte <= +2147483647)  // small int8
                  c_cmp_reg_imm (i1->reg, (int4)i2->icte, size => 8);
                else
                {
                  REG  r;
                  r = allocate_register (size => 8, false, *i2);
                  c_mov_reg_imm (r, i2->icte, size => 8);
                  c_cmp_reg_reg (i1->reg, r, size => 8);
                }
              }
              break;

              case INT_REGISTER:
                c_cmp_reg_reg (i1->reg, i2->reg, size => 8);
                break;

              case MEMORY:
                c_cmp_reg_mem (i1->reg, i2->ea, size => 8);
                break;

              default:
                abort;
            }
          }
        }

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register (size => 1, false, *i1);
          c_setcond_reg ((COMPARISON_FLAG)mask, /*signed=*/ true,  r, size => 1);

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !
          store_all_float_registers_in_temporaries ();

          c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ true,  near_label_nr);
        }
      }
      break;


      //   (  <float_a>  <float_b>  -->   )

      case P_CMP_FLT4:    // <mask>  ; compare two float  on float_stack
      {
        byte  mask;
        PCODE c;
        NODE* f1, f2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_FLT4) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        if (address_size == 4)
          free_register_except_for_this_pcode (RAX);   // fcomp destroys AX in 32-bit

        f2 = &fstack[fstack_count - 1];
        f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        // reverse comparison because ST0 is compared with ST1, not the opposite
        if (mask == 1 || mask == 3)
          mask += 3;
        else if (mask == 4 || mask == 6)
          mask -= 3;

        if (nb_float_registers_left() <
               (int)(f1->kind != FLOAT_REGISTER)    // we need 1 slot for f1
               + (int)(f2->kind == FLOAT_CONSTANT   // and one slot for f2 constant or, 64 bit only f2 memory
                           || (f2->kind == MEMORY && address_size == 8)))
        {
          store_all_float_registers_in_temporaries ();
        }

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            flush_float_in_register (ref *f2);
            c_fcomp ();
            break;

          case FLOAT_REGISTER:
            c_fcomp ();
            break;

          case MEMORY:
            if (address_size == 4)
            {
              c_fcomp_fmem (f2->ea, size => 4);   // this instruction is not supported in 64-bit mode !

              // exchange operands because the c_fcom_fmem instruction compares mem with ST0, not the opposite
              if (mask == 1 || mask == 3)
                mask += 3;
              else if (mask == 4 || mask == 6)
                mask -= 3;
            }
            else
            {
              flush_float_in_register (ref *f2);
              c_fcomp ();
            }
            break;

          default:
            abort;
        }

        fstack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register (size => 1, false, *f1);
          c_setcond_reg ((COMPARISON_FLAG)mask, /*signed=*/ false,  r, size => 1);

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !
          store_all_float_registers_in_temporaries ();

          c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ false,  near_label_nr);
        }
      }
      break;


      case P_CMP_FLT8:    // <mask>  ; compare two double on float_stack
      {
        byte  mask;
        PCODE c;
        NODE* f1, f2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_FLT8) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        if (address_size == 4)
          free_register_except_for_this_pcode (RAX);   // fcomp destroys AX in 32-bit

        f2 = &fstack[fstack_count - 1];
        f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        // reverse comparison because ST0 is compared with ST1, not the opposite
        if (mask == 1 || mask == 3)
          mask += 3;
        else if (mask == 4 || mask == 6)
          mask -= 3;

        if (nb_float_registers_left() <
               (int)(f1->kind != FLOAT_REGISTER)
               + (int)(f2->kind == FLOAT_CONSTANT || (f2->kind == MEMORY && address_size == 8)))
        {
          store_all_float_registers_in_temporaries ();
        }

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            flush_float_in_register (ref *f2);
            c_fcomp ();
            break;

          case FLOAT_REGISTER:
            c_fcomp ();
            break;

          case MEMORY:
            if (address_size == 4)
            {
              c_fcomp_fmem (f2->ea, size => 8);

              // exchange operands because the c_fcom_fmem instruction compares mem with ST0, not the opposite
              if (mask == 1 || mask == 3)
                mask += 3;
              else if (mask == 4 || mask == 6)
                mask -= 3;
            }
            else
            {
              flush_float_in_register (ref *f2);
              c_fcomp ();
            }
            break;

          default:
            abort;
        }

        fstack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register (size => 1, false, *f1);
          c_setcond_reg ((COMPARISON_FLAG)mask, /*signed=*/ false,  r, size => 1);

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !
          store_all_float_registers_in_temporaries ();

          c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ false,  near_label_nr);
        }
      }
      break;


      //   (  <addr_a>  <addr_b>  -->   )
      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

      case P_CMP_ADDR:     // <mask>  ; compare two addr   on addr_stack    - unsigned
      {
        byte  mask;
        PCODE c;
        NODE* a1, a2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:2];
        mem_offset += 2;
        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_ADDR) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        a2 = &astack[astack_count - 1];   // .kind can be EFFECTIVE_ADDRESS or MEMORY
        a1 = &astack[astack_count - 2];   // .kind can be EFFECTIVE_ADDRESS or MEMORY

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        // if register in index with scale 1, and base is free, move it to base.
        normalize_ea (ref *a1);
        normalize_ea (ref *a2);

        if ((!ea_is_just_ea_with_register_base (*a1) && ea_is_just_ea_with_register_base (*a2)) ||
            (ea_is_just_ea_with_register_base (*a1) && ea_is_just_ea_with_register_base (*a2) && a2->ea.base < a1->ea.base) ||
            ea_is_simple_constant_offset (*a1))
        {
          swap_nodes (ref *a1, ref *a2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        if (a1->kind == MEMORY && ea_is_simple_constant_offset (*a2))
        {
          c_cmp_mem_imm (a1->ea, a2->ea.offset, size => address_size);
        }
        else
        {
          load_ea_into_just_register_base (ref *a1);

          if (ea_is_simple_constant_offset (*a2))
          {
            c_cmp_reg_imm (a1->ea.base, a2->ea.offset, size => address_size);
          }
          else if (a2->kind == MEMORY)
          {
            c_cmp_reg_mem (a1->ea.base, a2->ea, size => address_size);
          }
          else
          {
            load_ea_into_just_register_base (ref *a2);
            c_cmp_reg_reg (a1->ea.base, a2->ea.base, size => address_size);
          }
        }

        astack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register (size => 1, false, *a1);
          c_setcond_reg ((COMPARISON_FLAG)mask, /*signed=*/ false,  r, size => 1);

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !
          store_all_float_registers_in_temporaries ();

          c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ false,  near_label_nr);
        }
      }
      break;


      //   ( bool  -->   bool  )    (if branch)
      //   ( bool  -->         )    (if not branch)

      case P_CAND:       // <near_label4>  ; branch if false
      {
        NODE  *i;
        int4  near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        store_all_registers_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        i = &istack[istack_count - 1];

        if (i->kind == INT_CONSTANT)
        {
          c_mov_reg_imm (RAX, i->icte, /*size= */ 1);
        }
        else if (i->kind == INT_REGISTER)
        {
          if (i->reg != RAX)
            c_mov_reg_reg (RAX, i->reg, /*size= */ 4);   // from non-byte register into byte register
        }
        else  // MEMORY
        {
          c_mov_reg_mem (RAX, i->ea, /*size= */ 1);
        }

        c_test_reg_reg (RAX, RAX, size => 1);

        c_jcond (CMP_EQUAL,   /*signed=*/ false,  near_label_nr);

        istack_count--;   // remove if no branch
      }
      break;


      //   ( bool  -->   bool  )    (if branch)
      //   ( bool  -->         )    (if not branch)

      case P_COR:      // <near_label4>  ; branch if true
      {
        NODE  *i;
        int4  near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        store_all_registers_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        i = &istack[istack_count - 1];

        if (i->kind == INT_CONSTANT)
        {
          c_mov_reg_imm (RAX, i->icte, /*size= */ 1);
        }
        else if (i->kind == INT_REGISTER)
        {
          if (i->reg != RAX)
            c_mov_reg_reg (RAX, i->reg, /*size= */ 4);   // from non-byte register into byte register
        }
        else  // MEMORY
        {
          c_mov_reg_mem (RAX, i->ea, /*size= */ 1);
        }

        c_test_reg_reg (RAX, RAX, size => 1);

        c_jcond (CMP_NOT_EQUAL,   /*signed=*/ false,  near_label_nr);

        istack_count--;   // remove if no branch
      }
      break;


      //  (  <int4(EAX)>   -->   <int4(EAX)>  )   ;  comparison for switch statement

      case P_SWITCH_CMP_S4:  // <cmp_int4> <mask>  <label4>
      {                      // ; compare <int4> with <cmp_int4> and branch if true
        byte  mask;
        int4  value, near_label_nr;

        value = *((int4 *)&mem[mem_offset]);
        mask = *((byte *)&mem[mem_offset+4]);
        near_label_nr = *((int4 *)&mem[mem_offset+5]);
        mem_offset += 9;

        if (g_tracing)
          trace (" %d  mask %u  label #%d\n", value, mask, near_label_nr);

        c_cmp_reg_imm (RAX, value, size => 4);    // imm limited to 32-bit
        c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ true,  near_label_nr);
      }
      break;


      //  (  <uint4(EAX)>   -->   <uint4(EAX)>  )   ;  comparison for switch statement

      case P_SWITCH_CMP_U4:  // <cmp_uint4> <mask>  <label4>
      {                      // ; compare <int4> with <cmp_int4> and branch if true
        byte  mask;
        int4  value, near_label_nr;

        value = *((int4 *)&mem[mem_offset]);
        mask = *((byte *)&mem[mem_offset+4]);
        near_label_nr = *((int4 *)&mem[mem_offset+5]);
        mem_offset += 9;

        if (g_tracing)
          trace (" %d  mask %u  label #%d\n", value, mask, near_label_nr);

        c_cmp_reg_imm (RAX, value, size => 4);    // imm limited to 32-bit
        c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ false,  near_label_nr);
      }
      break;


      //  (  <int8>   -->   <int8>  )        ;  comparison for switch statement

      case P_SWITCH_CMP_8:  // <cmp_int8> <mask>  <label4>
      {
        byte  mask;
        int8  value;
        int4  near_label_nr;

        value = *((int8 *)&mem[mem_offset]);
        mask = *((byte *)&mem[mem_offset+8]);
        near_label_nr = *((int4 *)&mem[mem_offset+9]);
        mem_offset += 13;

        if (g_tracing)
          trace (" %d  mask %u  label #%d\n", value, mask, near_label_nr);

        // possible masks : CMP_LARGER_OR_EQUAL, CMP_SMALLER, CMP_LARGER, CMP_NOT_EQUAL.

        if (address_size == 4)
        {
          REG  regs[2];
          NODE dummy;

          /*
            P_CMP_S8  mask=3
          >     cmp_4 low1,low2
                PUSHFD         opcode 9C    ; save ZF  (check that this pushes 4 bytes)
                POP    esi
                mov    EDI,high1
          >     sbb_4  EDI,high2
                PUSHFD         opcode 9C    ; save final flags
                or     esi,-1-64              ; mask to set all flags set except Z flags
                and     dword ptr[esp],esi  ; and to mask Z flag (but this destroys OV flag)

          >     cmp_4 low1,low2
                mov    EDI,high1
          >     sbb_4  EDI,high2           ; OV flag computed again
                POPFD                       ; opcode 9D to load final flags with Z flag adapted
          */

          clear dummy;
          allocate_registers (4, false, dummy, 2, out regs);   // allocate 2 registers of 4 bytes

          // compare low word
          c_cmp_reg_imm (RAX, (int4)value, size => 4);

          c_push_flags ();
          c_pop_reg (regs[0], 4);

          // subtract high word
          c_mov_reg_reg (regs[1], RDX, 4);
          c_sbb_reg_imm (regs[1], (int4)(value >> 32), size => 4);

          c_push_flags ();

          c_or_reg_imm (regs[0], -1-64, 4);

          c_ESP_correction_ON (false);
          {
            EA ea;
            clear ea;
            ea.base = RSP;  ea.index = NONE;   ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
            c_and_mem_reg (ea, regs[0], 4);
          }
          c_ESP_correction_ON (true);

          // compare low word
          c_cmp_reg_imm (RAX, (int4)value, size => 4);

          // subtract high word
          c_mov_reg_reg (regs[1], RDX, 4);
          c_sbb_reg_imm (regs[1], (int4)(value >> 32), size => 4);

          c_pop_flags ();
        }
        else    // 64-bit
        {
          if (value >= -(int8)2147483648 && value <= +2147483647)
            c_cmp_reg_imm (RAX, (int4)value, size => 8);
          else
          {
            c_mov_reg_imm (RCX, value, size => 8);
            c_cmp_reg_reg (RAX, RCX, size => 8);
          }
        }

        c_jcond ((COMPARISON_FLAG)mask,   /*signed=*/ true,  near_label_nr);
      }
      break;


      //  (  <uint4(EAX)>   -->  / )        ;  jump table for switch statement

      case P_JUMP_4:    // <pool_id8>   pool constant contains a list of <label_nr>
      {                 //              that will be converted into addresses later on.
        int8 nr;        // jump to <pool_cte>[<uint4*address_size>]
        EA   ea;

        nr = *((int8 *)&mem[mem_offset]);
        mem_offset += 8;

        if (g_tracing)
          trace (" table pool nr=%d\n", nr);

        // relocate labels within jump table
        pe.exe_mark_jump_table (nr);

        clear ea;
        ea.base   = NONE;
        ea.index  = RAX;
        ea.scale  = address_size;
        ea.offset = 0;
        ea.reloc.kind = RELOC_POOL;
        ea.reloc.nr   = nr;

        c_jump_indirect (ea, address_size);

        istack_count--;
      }
      break;


      //  (  <bool>   -->   )
      //  (  <int4/uint4>   -->   )
      //  (  <int8>   -->   )

      case P_DROP_BOOL:
      case P_DROP_4:
      case P_DROP_8:
        if (g_tracing)
          trace ("\n");

        istack_count--;
        break;


      //  (  <float>   -->   )
      //  (  <double>   -->   )

      case P_DROP_FLT_4:
      case P_DROP_FLT_8:
        if (g_tracing)
          trace ("\n");

        c_fpop ();
        fstack_count--;
        break;


      //  (  <addr>   -->   )

      case P_DROP_ADDR:
        if (g_tracing)
          trace ("\n");

        astack_count--;
        break;


      //  (  <addr>   -->  <addr> <addr>  )

      case P_DUP_ADDR:
      {
        if (g_tracing)
          trace ("\n");

        astack[astack_count] = astack[astack_count-1];
        astack_count++;
      }
      break;



  // ----------------
  // 3. function call
  // ----------------


      case P_SYSCALL_EXTRA_STACK_SPACE:   // <int4> : for 64-bit operating system calls
                           // nb bytes to reserve on stack for shadow space and alignment.
      {
        int4 extra = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" %d\n", extra);

        if (extra != 0)
        {
          if (extra == 8)
          {
            c_push_reg (RAX, address_size);   // 1 byte opcode
          }
          else if (extra == 16)
          {
            c_push_reg (RAX, address_size);   // 1 byte opcode
            c_push_reg (RAX, address_size);   // 1 byte opcode
          }
          else
          {
            c_sub_reg_imm (RSP, extra, 8);   // 4 bytes opcode
            add_ESP_correction (extra);   // ESP temp parameter have a longer offset now
          }
        }
      }
      break;

      //   (  bool  -->   )

      case P_PUSH_BOOL:   // pop int_stack,   push 4 or 8 bytes on function stack (depending on version)
      {
        NODE* i;

        i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (i->kind)
        {
          case INT_CONSTANT:
            c_push_imm ((int4)i->icte);
            break;

          case INT_REGISTER:
            c_push_reg (i->reg, address_size);
            break;

          case MEMORY:
            if ((i->ea.base == RBP || i->ea.base == RSP || i->ea.base == NONE) &&
                i->ea.index == NONE &&
                ((i->ea.offset & (address_size-1)) == 0) &&
                (i->ea.reloc.kind == RELOC_NONE || i->ea.reloc.kind == RELOC_GLOBAL))
            {
              // ea is aligned at address boundary
              c_push_mem (i->ea, address_size);  // push address size bytes, despite that it's a bool
            }
            else     // pushing memory operand in some unknown memory area might cross some boundary and cause a page fault,
            {        // so load byte variable into register first.
              REG r;
              r = allocate_register (address_size, true, *i);
              c_movzx_reg_mem (r, 4, i->ea, size_source => 1);  // will zero-extend 4 bytes into 8 bytes for 64-bit
              c_push_reg (r, address_size);
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //   (  int  -->   )

      case P_PUSH_4:      // pop int_stack,   push 4 or 8 bytes on function stack (depending on version)
      {
        NODE* i;
        i = &istack[istack_count - 1];
        if (g_tracing)
          trace ("\n");

        push_int4 (*i);
        istack_count--;
      }
      break;

      case P_PUSH_8:      // pop int_stack, push 8 bytes on function stack
      {
        NODE* i;
        i = &istack[istack_count - 1];
        if (g_tracing)
          trace ("\n");

        push_int8 (*i);
        istack_count--;
      }
      break;


      //   (  float  -->   )

      case P_PUSH_FLT4:   // pop float_stack, push 4 or 8 bytes on function stack (depending on version)
      {
        NODE* f;
        f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace ("\n");

        push_float (*f);
        fstack_count--;
      }
      break;


      case P_PUSH_FLT8:   // pop float_stack, push      8 bytes on function stack (depending on version)
      {
        NODE* f;
        f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace ("\n");

        push_float (*f);
        fstack_count--;
      }
      break;


      //   (  addr  -->   )

      case P_PUSH_ADDR:    // pop addr_stack,  push 4 or 8 bytes on function stack (depending on version)
      {
        NODE* a;
        a = &astack[astack_count - 1];
        if (g_tracing)
          trace ("\n");

        push_addr (*a);
        astack_count--;
      }
      break;


      //   (  addr  -->   )

      case P_CALL_INTEL:   // <alignment_and_extra_shadow_stack_space>  <nb_86_entries_pushed>  <is_os_call>
      {
        NODE* a;
        int4 alignment_and_extra_shadow_stack_space, nb_86_entries_pushed, pushed_params;
        bool is_os_call;                // calling [callback] (operating system function)
        char parameter_table[4];        // data type of first 4 parameters for os call

        alignment_and_extra_shadow_stack_space = *((int4 *)&mem[mem_offset]); // nb bytes we reserved on stack for alignment and extra shadow space
        nb_86_entries_pushed = *((int4 *)&mem[mem_offset+4]);
        is_os_call           = (bool)mem[mem_offset+8];
        parameter_table'byte = mem[mem_offset+9:4];
        mem_offset += 9+4;

        if (g_tracing)
          trace ("  extra_shadow=%d, entries_pushed=%d, os_call=%d, parameter_table=%.4s\n", alignment_and_extra_shadow_stack_space, nb_86_entries_pushed, (int)is_os_call, parameter_table);

        // all registers must be flushed to temporaries before calling a function
        store_all_registers_in_temporaries_except_some_nodes (0, 0, +1);   // don't store latest address entry
        store_all_float_registers_in_temporaries ();

        a = &astack[astack_count - 1];


        if (address_size == 8)
        {
          if (is_os_call)   // we call Windows : load first 4 parameters in registers
          {
            const REG REGS[4] = {RCX, RDX, R8, R9};
            EA  ea;
            int par;

            c_ESP_correction_ON (false);   // use RSP without temporary correction

            clear ea;
            ea.base = RSP;  ea.index = NONE;   ea.scale = 1;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;

            for (par=0; par<nb_86_entries_pushed && par<4; par++)
            {
              ea.offset = par * 8;

              switch (parameter_table[par])
              {
                case 'b':  // boolean
                  c_movzx_reg_mem (REGS[par], /*size_target=*/4, ea, /*size_source=*/1);
                  break;

                case 'i':  // int4/uint4
                  c_mov_reg_mem (REGS[par], ea, 4);
                  break;

                case 'f':  // float4
                  c_mov_xm_mem ((XM)par, ea,  4);
                  break;

                case 'd':  // float8
                  c_mov_xm_mem ((XM)par, ea,  8);
                  break;

                case 'l':  // int8
                case 'a':  // address
                  c_mov_reg_mem (REGS[par], ea, 8);
                  break;

                default:
                  fatal_compiler_error0 ("p_call bad typ");
                  break;
              }
            }

            c_ESP_correction_ON (true);
          }

          // check that stack is aligned to M16
          if (option_check_stack_M16_alignment)
            c_call_relative (label_check_stack_M16_alignment, 0);

          if ((ESP_correction_value() % 16) != 0)
            fatal_compiler_error0 ("p_call bad align");     // this should never occur
        }


        if (address_size == 4)
        {
          pushed_params = nb_86_entries_pushed;
        }
        else
        {
          if (is_os_call)   // operating system call
            pushed_params = 0;   // no ESP fix
          else
            pushed_params = nb_86_entries_pushed;
        }

        switch (a->kind)
        {
          case EFFECTIVE_ADDRESS:
            if (a->ea.base == NONE && a->ea.index == NONE && a->ea.offset == 0 && a->ea.reloc.kind == RELOC_FUNC)
            {
              c_call_relative ((int4)a->ea.reloc.nr, pushed_params);
            }
            else if (a->ea.base != NONE && a->ea.index == NONE && a->ea.offset == 0 && a->ea.reloc.kind == RELOC_NONE)
            {
              c_call_reg (a->ea.base, address_size, pushed_params);
            }
            else
            {
              fatal_compiler_error0 ("asm(P_CALL)(1)");     // this should never occur
            }
            break;

          case MEMORY:
            c_call_indirect (a->ea, address_size, pushed_params);
            break;

          default:
            abort;
        }

        astack_count--;

        if (address_size == 8)   // 64-bit : cleanup stack after call
        {
          int fix = alignment_and_extra_shadow_stack_space;

          if (is_os_call)   // operating system call : must also remove parameters
            fix += nb_86_entries_pushed * 8;

          if (fix > 0)
          {
            if (fix == 8)
            {
              c_pop_reg (RCX, address_size);
              add_ESP_correction (address_size);  // cancel ESP offset change
            }
            else if (fix == 16)
            {
              c_pop_reg (RCX, address_size);
              add_ESP_correction (address_size);  // cancel ESP offset change
              c_pop_reg (RCX, address_size);
              add_ESP_correction (address_size);  // cancel ESP offset change
            }
            else
            {
              c_add_reg_imm (RSP, fix, 8);
            }
          }

          add_ESP_correction (- fix);   // ESP temp parameter have a shorter offset now
        }
      }
      break;



      // when returning from the function call, the return value will be
      // in top xx_stack position (always EDX:EAX / ST(0))

      // code that indicates that the called function pushed a value on xx_stack.
      // no effect for asm except indicating that eax / st(0) is allocated.


      //   (   -->   )

      case P_RETVALUE_VOID:  // function returned nothing
        if (g_tracing)
          trace ("\n");
        break;


      //   (   -->  <uint1> )
      //   (   -->  <int4/uint4> )

      case P_RETVALUE_BOOL:  // function returned a 1-byte bool on int_stack
      {
        NODE* i = &istack[istack_count++];
        if (g_tracing)
          trace ("\n");

        i->typ = 'b';
        i->kind = INT_REGISTER;
        i->reg = RAX;
        i->reg_high = NONE;
      }
      break;

      case P_RETVALUE_4:     // function returned an int4/uint4 value on int_stack
      {
        NODE* i = &istack[istack_count++];
        if (g_tracing)
          trace ("\n");

        i->typ = 'i';
        i->kind = INT_REGISTER;
        i->reg = RAX;
        i->reg_high = NONE;
      }
      break;

      //   (   -->  <int8> )

      case P_RETVALUE_8:     // function returned an int8 value  on int_stack
      {
        NODE* i = &istack[istack_count++];
        if (g_tracing)
          trace ("\n");

        i->typ = 'l';
        i->kind = INT_REGISTER;

        if (address_size == 4)
        {
          i->reg = RAX;
          i->reg_high = RDX;
        }
        else
        {
          i->reg = RAX;
          i->reg_high = NONE;
        }
      }
      break;


      //   (   -->  <float> )

      case P_RETVALUE_FLT4:  // function returned a float4 value on float_stack
      {
        NODE* f = &fstack[fstack_count++];
        if (g_tracing)
          trace ("\n");

        f->typ = 'f';
        f->kind = FLOAT_REGISTER;
        f->freg = 0;
      }
      break;

      case P_RETVALUE_FLT8:  // function returned a float8 value on float_stack
      {
        NODE* f = &fstack[fstack_count++];
        if (g_tracing)
          trace ("\n");

        f->typ = 'd';
        f->kind = FLOAT_REGISTER;
        f->freg = 0;
      }
      break;


      //   (   -->  <addr> )

      case P_RETVALUE_ADDR:     // function returned an addr value on addr_stack
      {
        NODE* a = &astack[astack_count++];
        if (g_tracing)
          trace ("\n");

        a->typ = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = RAX;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;


  // -------------------------
  // 4. function prolog/epilog
  // -------------------------

      //   (   -->   )

      case P_ENTER:    // <size4>  <free4>  <block4>  <callee_addr> <flag_1X_is_thread_entry_point_2X_is_callback_4X entry>
      {
        char parameter_table[4];

        g_frame_size      = *((uint4 *)&mem[mem_offset]);      // size of frame to allocate,
        g_extra_bytes     = *((uint4 *)&mem[mem_offset+4]);    // free space for spilling registers,
        g_stack_alignment = *((uint4 *)&mem[mem_offset+8]);    // allocate new space in blocks.
        g_callee_saved_regs = *((int4 *)&mem[mem_offset+12]);    // android only
        g_is_thread_entry_point = (*((int4 *)&mem[mem_offset+16]) & 1) != 0;
        g_is_callback           = (*((int4 *)&mem[mem_offset+16]) & 2) != 0;

        parameter_table'byte = mem[mem_offset+20:4];
        mem_offset += (20+4);

        if (g_tracing)
          trace (" frame_size = %u, extra_bytes = %u, alignment = %u, is_thread = %u, is_callback = %u, parameter_table=%.4s\n",
                 g_frame_size, g_extra_bytes, g_stack_alignment, (uint)g_is_thread_entry_point, (uint)g_is_callback, parameter_table);

            // intel:   push  ebp
            //          mov   ebp,esp
            //          sub   esp,<size4>
            //  + probe stack on each 4K page using (test ebp[-ofs],eax)
            //     if size larger than 4K page

        c_push_reg    (RBP,      address_size);  // push ebp
        c_mov_reg_reg (RBP, RSP, address_size);  // mov  ebp,esp

        g_extra_frame_bytes_rip = current_RIP();   // code for inserting stack space and probe stack for 4K page will be inserted here !

        if (g_is_thread_entry_point || g_is_callback)
        {
          c_and_reg_imm (RSP, -16, address_size);  // align stack pointer at M16
        }
        else
        {
          if (address_size == 8)
          {
            // check that stack is aligned to M16
            if (option_check_stack_M16_alignment)
              c_call_relative (label_check_stack_M16_alignment, 0);

            if ((g_frame_size % g_stack_alignment) != 0)
              fatal_compiler_error0 ("P_ENTER bad align");
          }
        }

        if (g_is_thread_entry_point || g_is_callback)
        {
          if (address_size == 4)
          {
            uint reg_space = (uint)(3*address_size);  // save 3 registers (RSI, RDI, RBX)
            EA   ea;

            while (g_extra_bytes < reg_space)
            {
              g_extra_bytes += g_stack_alignment;
              g_frame_size += g_stack_alignment;
            }

            g_extra_bytes -= reg_space;

            g_save_registers_for_callback_at_ofs = -(int)g_frame_size + (int)g_extra_bytes;

            clear ea;
            ea.base = RBP;  ea.index = NONE;   ea.scale = 1;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;

            ea.offset = g_save_registers_for_callback_at_ofs;
            c_mov_mem_reg (ea, RSI, address_size);

            ea.offset += address_size;
            c_mov_mem_reg (ea, RDI, address_size);

            ea.offset += address_size;
            c_mov_mem_reg (ea, RBX, address_size);
          }
          else  // 64 bit
          {
            // store parameters (int or float) into caller's shadow space
            {
              const REG REGS[4] = {RCX, RDX, R8, R9};
              int par;
              EA  ea;

              clear ea;
              ea.base = RBP;  ea.index = NONE;   ea.scale = 1;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;

              for (par=0; par<4; par++)
              {
                ea.offset = 16 + par * 8;

                switch (parameter_table[par])
                {
                  case 'b':
                    c_mov_mem_reg (ea, REGS[par], 1);
                    break;

                  case 'i':
                    c_mov_mem_reg (ea, REGS[par], 4);
                    break;

                  case 'f':
                    c_mov_mem_xm (ea, (XM)par, 4);
                    break;

                  case 'd':
                    c_mov_mem_xm (ea, (XM)par, 8);
                    break;

                  case 'l':
                  case 'a':
                    c_mov_mem_reg (ea, REGS[par], 8);
                    break;

                  default:
                    break;
                }
              }
            }


            // save registers RBX, RSI, RDI, R12, R13, R14, R15  (7 registers to save/restore for callback)

            {
              uint reg_space = 7*(uint)address_size;
              EA   ea;

              while (g_extra_bytes < reg_space)
              {
                g_extra_bytes += g_stack_alignment;
                g_frame_size += g_stack_alignment;
              }

              g_extra_bytes -= reg_space;

              g_save_registers_for_callback_at_ofs = -(int)g_frame_size + (int)g_extra_bytes;

              clear ea;
              ea.base = RBP;  ea.index = NONE;   ea.scale = 1;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;

              ea.offset = g_save_registers_for_callback_at_ofs;
              c_mov_mem_reg (ea, RSI, address_size);

              ea.offset += address_size;
              c_mov_mem_reg (ea, RDI, address_size);

              ea.offset += address_size;
              c_mov_mem_reg (ea, RBX, address_size);

              ea.offset += address_size;
              c_mov_mem_reg (ea, R12, address_size);

              ea.offset += address_size;
              c_mov_mem_reg (ea, R13, address_size);

              ea.offset += address_size;
              c_mov_mem_reg (ea, R14, address_size);

              ea.offset += address_size;
              c_mov_mem_reg (ea, R15, address_size);
            }
          }
        }

        c_ESP_correction_ON (true);
        reset_ESP_correction ();
      }
      break;


      case P_LEAVE:      // intel:  leave      (equivalent to:   mov ESP, EBP  +  pop EBP)
        if (g_tracing)
          trace ("\n");

        if (g_is_thread_entry_point || g_is_callback)
        {
          if (address_size == 4)
          {
            EA ea;

            clear ea;
            ea.base = RBP;  ea.index = NONE;   ea.scale = 1;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;

            ea.offset = g_save_registers_for_callback_at_ofs;
            c_mov_reg_mem (RSI, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (RDI, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (RBX, ea, address_size);
          }
          else  // 64-bit
          {
            EA ea;

            clear ea;
            ea.base = RBP;  ea.index = NONE;   ea.scale = 1;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;

            ea.offset = g_save_registers_for_callback_at_ofs;
            c_mov_reg_mem (RSI, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (RDI, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (RBX, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (R12, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (R13, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (R14, ea, address_size);

            ea.offset += address_size;
            c_mov_reg_mem (R15, ea, address_size);
          }
        }

        c_leave ();

        break;


        //   (   -->   )

      case P_RETURN_VOID:    // indicates a function with no return value
        if (g_tracing)
          trace ("\n");

        if (is_main || g_is_thread_entry_point || g_is_callback)
          c_mov_reg_imm (RAX, 0, 4);  // asm: main() returning void should clean EAX
        break;



      // flush result value and make sure it is in AL
      //   ( <uint1>   -->   )

      case P_RETURN_BOOL:   // return value is bool on int_stack
        if (g_tracing)
          trace ("\n");

        sync_bool ();

        if (address_size == 8 && g_is_callback)   // returns value to Windows API
        {
          c_movzx_reg_reg (RAX, 4, RAX, 1);   // AL -> RAX  (to be sure)
        }

        istack_count--;
        break;


      // flush result value and make sure it is in RAX
      //   ( <int4/uint4> -->  )

      case P_RETURN_4:      // return value is int4/uint4 on int_stack
        if (g_tracing)
          trace ("\n");

        sync_int4 ();
        istack_count--;
        break;


      // flush result value and make sure it is in RDX:RAX or RAX(64-bit)
      //   (   <int8> -->  )

      case P_RETURN_8:      // return value is int8 value on int_stack
        if (g_tracing)
          trace ("\n");

        sync_int8 ();
        istack_count--;
        break;


      //   (   <float> -->  )

      case P_RETURN_FLT4:  // return value is float4 value on float_stack
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace ("\n");

        flush_float_in_register (ref *f);
        fstack_count--;
      }
      break;

      case P_RETURN_FLT8:  // return value is float8 value on float_stack
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace ("\n");

        flush_float_in_register (ref *f);
        fstack_count--;
      }
      break;


      case P_RETURN_ADDR:      // return value is addr value on addr_stack
        if (g_tracing)
          trace ("\n");

        sync_addr ();
        astack_count--;
        break;


       //   (   -->   )

      case P_RET:
      {
        int size;

        size = *((int4 *)&mem[mem_offset]);     // <size4>      ; size of parameters to pop from stack
        mem_offset += 4;

        if (g_is_thread_entry_point || g_is_callback)
        {
          if (address_size == 8)   // 64-bit callback function : no stack correction
            size = 0;
        }

        if (g_tracing)
          trace (" %d\n", size);

        c_ret (size);
      }
      break;



  // -------------
  // 5. arithmetic
  // -------------

      // bool   (1-byte values on int_stack)
      // ----

      case P_NOT_BOOL:   // ( bool --> bool )
      {
        NODE* i = &istack[istack_count - 1];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)

        if (g_tracing)
          trace ("\n");

        if (i->kind == INT_CONSTANT)
        {
          i->icte = (int)!(bool)i->icte;
        }
        else
        {
          if (i->kind == MEMORY)
          {
            c_test_mem_imm (i->ea, 255, size => 1);       // "and" with 255, to set flags
          }
          else  // register
          {
            c_test_reg_reg (i->reg, i->reg, size => 1);   // "and" with itself, to set flags
          }

          {
            REG r = allocate_register (size => 1, true, *i);

            c_setcond_reg (CMP_EQUAL, /*signed=*/false, r, size => 1);  // compare with zero

            i->kind = INT_REGISTER;
            i->reg = r;
            i->reg_high = NONE;
          }
        }
      }
      break;


      case P_OR_BOOL:   // ( bool  bool --> bool )
      {
        NODE* i2 = &istack[istack_count - 1];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)
        NODE* i1 = &istack[istack_count - 2];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int1_in_register_for_modif (ref *i1);      // flush into byte-aligned register

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if ((int)i2->icte != 0)   // "or true" yields true
            {
              i1->kind = INT_CONSTANT;
              i1->icte = 1;
            }
            else     // "or false" has no effect
            {
              // leave first register operand unchanged
            }
            break;

          case INT_REGISTER:
            c_or_reg_reg (i1->reg, i2->reg, size => 1);
            break;

          case MEMORY:
            c_or_reg_mem (i1->reg, i2->ea, size => 1);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_AND_BOOL:   // ( bool  bool --> bool )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int1_in_register_for_modif (ref *i1);      // flush into byte-aligned register

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if ((int)i2->icte != 0)  // "and true" has no effect
            {
              // leave first operand
            }
            else     // "and false" yields false
            {
              i1->kind = INT_CONSTANT;
              i1->icte = 0;
            }
            break;

          case INT_REGISTER:
            c_and_reg_reg (i1->reg, i2->reg, size => 1);
            break;

          case MEMORY:
            c_and_reg_mem (i1->reg, i2->ea, size => 1);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_XOR_BOOL:   // ( bool  bool --> bool )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int1_in_register_for_modif (ref *i1);      // flush into byte-aligned register

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if ((int)i2->icte == 0)   // "xor 0" has no effect
            {
              // leave first operand
            }
            else
            {
              c_xor_reg_imm (i1->reg, 1, 1);    // imm limited to 32-bit
            }
            break;

          case INT_REGISTER:
            c_xor_reg_reg (i1->reg, i2->reg, 1);
            break;

          case MEMORY:
            c_xor_reg_mem (i1->reg, i2->ea, 1);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;



      // int4   (signed value on int_stack)
      // ----

      case P_NEG4:   // ( int4         --> int4 )
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if (i->kind == INT_CONSTANT)
        {
          i->icte = - i->icte;
        }
        else
        {
          flush_int4_in_register_for_modif (ref *i);
          c_neg_reg (i->reg, size => 4);
        }
      }
      break;


      case P_NOT4:   // ( int4         --> int4 )
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if (i->kind == INT_CONSTANT)
        {
          i->icte = ~i->icte;
        }
        else
        {
          flush_int4_in_register_for_modif (ref *i);
          c_not_reg (i->reg, size => 4);
        }
      }
      break;



      case P_ADD4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int icte = (int)i2->icte;
              if (icte == 0)  // "+ 0" has no effect
              {
                // leave first register operand unchanged
              }
              else if (icte == 1)
              {
                c_inc_reg (i1->reg, size => 4);
              }
              else if (icte == -1)
              {
                c_dec_reg (i1->reg, size => 4);
              }
              else
              {
                c_add_reg_imm (i1->reg, icte, size => 4);    // imm limited to 32-bit
              }
            }
            break;

          case INT_REGISTER:
            c_add_reg_reg (i1->reg, i2->reg, size => 4);
            break;

          case MEMORY:
            c_add_reg_mem (i1->reg, i2->ea, size => 4);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_SUB4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int icte = (int)i2->icte;
              if (icte == 0)  // "- 0" has no effect
              {
                // leave first register operand unchanged
              }
              else if (icte == 1)
              {
                c_dec_reg (i1->reg, size => 4);
              }
              else if (icte == -1)
              {
                c_inc_reg (i1->reg, size => 4);
              }
              else
              {
                c_sub_reg_imm (i1->reg, icte, size => 4);    // imm limited to 32-bit
              }
            }
            break;

          case INT_REGISTER:
            c_sub_reg_reg (i1->reg, i2->reg, size => 4);
            break;

          case MEMORY:
            c_sub_reg_mem (i1->reg, i2->ea, size => 4);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_SMUL4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int icte = (int)i2->icte;
              if (icte == -1)  // negate
              {
                c_neg_reg (i1->reg, size => 4);
              }
              else if (icte == 1)  // "* 1" has no effect
              {
                // leave first register operand unchanged
              }
              else if (icte == 0)  // "* 0" yields zero
              {
                i1->kind = INT_CONSTANT;
                i1->icte = 0;
              }
              else if (icte == 2)
              {
                c_add_reg_reg (i1->reg, i1->reg, size => 4);
              }
              else if (icte == 3 || icte == 5 || icte == 9)
              {
                // 32 bit : generates 3 bytes of code :   8d 1c db      lea    ebx,[ebx+ebx*8]
                // 64 bit : generates 4 bytes of code :   67 8d 1c db   lea    ebx,[ebx+ebx*8]
                EA ea;
                clear ea;
                ea.base = i1->reg;   ea.index = i1->reg;  ea.scale = icte - 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
                c_lea_reg_mem (i1->reg, ea, 4);
              }
              else if (icte >= 4 && icte <= 2000000000 && (icte & (icte-1)) == 0)  // large power of 2
              {
                c_shl_reg_imm (i1->reg, lshifts_of(icte), 4);   // generates 3 bytes of code
              }
              else      // general case
              {
                c_imul_reg_imm (i1->reg, icte, size => 4);    // imm limited to 32-bit   // generates 3 bytes of code
              }
            }
            break;

          case INT_REGISTER:
            c_imul_reg_reg (i1->reg, i2->reg, size => 4);
            break;

          case MEMORY:
            c_imul_reg_mem (i1->reg, i2->ea, size => 4);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_SDIV4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if (i2->kind == INT_CONSTANT)
        {
          int icte = (int)i2->icte;

          if (icte == -1)  // div -1
          {
            flush_int4_in_register_for_modif (ref *i1);
            c_neg_reg (i1->reg, size => 4);
          }
          else if (icte == 1)  // div 1
          {
            flush_int4_in_register_for_modif (ref *i1);
            // has no effect
          }
          else
          {
            load_int4_into_reg (ref *i1, RAX);
            free_register (RDX);

            i1->reg_high = RDX;   // to avoid that RDX be allocated by flush
            flush_int4_in_register (ref *i2);
            i1->reg_high = NONE;  // restore i1

            c_idiv_rax_reg (i2->reg, 4);  // EAX = EAX / source  (4 byte)  with EDX receiving remainder
          }
        }
        else
        {
          load_int4_into_reg (ref *i1, RAX);
          free_register (RDX);

          if (i2->kind == INT_REGISTER)
            c_idiv_rax_reg (i2->reg, 4);  // EAX = EAX / source  (4 byte)  with EDX receiving remainder
          else
            c_idiv_rax_mem (i2->ea, 4);  // EAX = EAX / source  (4 byte)  with EDX receiving remainder
        }

        istack_count--;
      }
      break;


      case P_SMOD4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        load_int4_into_reg (ref *i1, RAX);
        free_register (RDX);

        if (i2->kind == INT_CONSTANT)
        {
          i1->reg_high = RDX;   // to avoid that RDX be allocated by flush
          flush_int4_in_register (ref *i2);
          i1->reg_high = NONE;  // restore i1

          c_imod_rax_reg (i2->reg, 4);
        }
        else
        {
          if (i2->kind == INT_REGISTER)
            c_imod_rax_reg (i2->reg, 4);
          else
            c_imod_rax_mem (i2->ea, 4);
        }

        i1->kind = INT_REGISTER;
        i1->reg = RDX;
        i1->reg_high = NONE;

        istack_count--;
      }
      break;


      case P_BITAND4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int icte = (int)i2->icte;
              if (icte == -1)  // "& 0xFFFFFFFF" has no effect
              {
                // leave first register operand unchanged
              }
              else if (icte == 0)    // "& 0" yields 0
              {
                i1->kind = INT_CONSTANT;
                i1->icte = 0;
              }
              else
              {
                c_and_reg_imm (i1->reg, icte, size => 4);    // imm limited to 32-bit
              }
            }
            break;

          case INT_REGISTER:
            c_and_reg_reg (i1->reg, i2->reg, size => 4);
            break;

          case MEMORY:
            c_and_reg_mem (i1->reg, i2->ea, size => 4);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_BITOR4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int icte = (int)i2->icte;
              if (icte == -1)  // "or 0xFFFFFFFF" yields -1
              {
                i1->kind = INT_CONSTANT;
                i1->icte = -1;
              }
              else if (icte == 0)    // "| 0" has no effect
              {
                // leave first register operand unchanged
              }
              else
              {
                c_or_reg_imm (i1->reg, icte, size => 4);    // imm limited to 32-bit
              }
            }
            break;

          case INT_REGISTER:
            c_or_reg_reg (i1->reg, i2->reg, size => 4);
            break;

          case MEMORY:
            c_or_reg_mem (i1->reg, i2->ea, size => 4);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_BITXOR4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int icte = (int)i2->icte;
              if (icte == -1)    // "xor FFFFFFFF"
              {
                c_not_reg (i1->reg, size => 4);
              }
              else if (icte == 0)    // "xor 0" has no effect
              {
                // leave first register operand unchanged
              }
              else
              {
                c_xor_reg_imm (i1->reg, icte, size => 4);    // imm limited to 32-bit
              }
            }
            break;

          case INT_REGISTER:
            c_xor_reg_reg (i1->reg, i2->reg, size => 4);
            break;

          case MEMORY:
            c_xor_reg_mem (i1->reg, i2->ea, size => 4);
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_ASL4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (i2->kind == INT_CONSTANT)
        {
          flush_int4_in_register_for_modif (ref *i1);

          if (i2->icte != 0)  // 0 has no effect
            c_shl_reg_imm (i1->reg, (int4)i2->icte & 31, size => 4);    // imm limited to 32-bit
        }
        else
        {
          load_int4_into_reg (ref *i2, RCX);
          flush_int4_in_register_for_modif (ref *i1);
          c_shl_reg_CL (i1->reg, size => 4);
        }

        istack_count--;
      }
      break;


      case P_ASR4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (i2->kind == INT_CONSTANT)
        {
          flush_int4_in_register_for_modif (ref *i1);

          if (i2->icte != 0)  // 0 has no effect
            c_sar_reg_imm (i1->reg, (int4)i2->icte & 31, size => 4);    // imm limited to 32-bit
        }
        else
        {
          load_int4_into_reg (ref *i2, RCX);
          flush_int4_in_register_for_modif (ref *i1);
          c_sar_reg_CL (i1->reg, size => 4);
        }

        istack_count--;
      }
      break;


      case P_UMUL4:    //  ( uint4  uint4   --> uint4 )
      {
        NODE* i2 = &istack[istack_count - 1];   // INT_CONSTANT, INT_REGISTER or MEMORY
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        if (i1->kind == INT_CONSTANT)
          swap_nodes (ref *i1, ref *i2);

        if (i2->kind == INT_CONSTANT)
        {
          int icte = (int)i2->icte;
          if (icte == 0)
          {
            i1->kind = INT_CONSTANT;
            i1->icte = 0;
          }
          else if (icte == 1)
          {
            flush_int4_in_register (ref *i1);   // needs to be in a register for tombstone
          }
          else if (icte == 2)
          {
            flush_int4_in_register_for_modif (ref *i1);
            c_add_reg_reg (i1->reg, i1->reg, 4);
          }
          else if (icte == 3 || icte == 5 || icte == 9)
          {
            REG r;
            EA ea;

            flush_int4_in_register_for_modif (ref *i1);
            r = i1->reg;

            clear ea;
            ea.base = r;   ea.index = r;  ea.scale = icte - 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
            c_lea_reg_mem (r, ea, 4);
          }
          else if (icte >= 4 && icte <= 2000000000 && (icte & (icte-1)) == 0)  // large power of 2
          {
            flush_int4_in_register_for_modif (ref *i1);
            c_shl_reg_imm (i1->reg, lshifts_of(icte), 4);
          }
          else      // general case
          {
            if (i1->kind == INT_REGISTER && i1->reg == RAX)     // RAX * cte
            {
              free_register (RDX);
              c_mov_reg_imm (RDX, icte, 4);
              c_umul_rax_reg (RDX, 4);
            }
            else if (i1->kind == INT_REGISTER && i1->reg == RDX)     // RDX * cte
            {
              free_register (RAX);
              c_mov_reg_imm (RAX, icte, 4);
              c_umul_rax_reg (RDX, 4);
              i1->reg = RAX;
            }
            else if (i1->kind == MEMORY && (i1->ea.base == RAX || i1->ea.index == RAX))    // [eax+???] * cte
            {
              load_int4_into_reg (ref *i1, RAX);
              free_register (RDX);

              c_mov_reg_imm (RDX, icte, 4);
              c_umul_rax_reg (RDX, 4);

              i1->kind = INT_REGISTER;
              i1->reg = RAX;
              i1->reg_high = NONE;
            }
            else
            {
              free_register (RAX);
              free_register_except_for_this_pcode (RDX);

              c_mov_reg_imm (RAX, icte, 4);

              if (i1->kind == INT_REGISTER)
                c_umul_rax_reg (i1->reg, 4);
              else
                c_umul_rax_mem (i1->ea, 4);

              i1->kind = INT_REGISTER;
              i1->reg = RAX;
              i1->reg_high = NONE;
            }
          }
        }
        else       // there are no constants : it's INT_REGISTER or MEMORY
        {
          // move RAX in first operand if possible
          if (i2->kind == INT_REGISTER && i2->reg == RAX)
            swap_nodes (ref *i1, ref *i2);

          load_int4_into_reg (ref *i1, RAX);
          free_register_except_for_this_pcode (RDX);

          if (i2->kind == INT_REGISTER)
            c_umul_rax_reg (i2->reg, 4);
          else
            c_umul_rax_mem (i2->ea, 4);

          i1->kind = INT_REGISTER;
          i1->reg = RAX;
          i1->reg_high = NONE;
        }

        istack_count--;
      }
      break;


      case P_UDIV4:   //  ( uint4  uint4  ---> uint4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        free_register (RDX);
        load_int4_into_reg (ref *i1, RAX);

        if (i2->kind == INT_CONSTANT)
        {
          if (i2->icte != 1)  // div 1 has no effect
          {
            i1->reg_high = RDX;   // to avoid that RDX be allocated by flush
            flush_int4_in_register (ref *i2);
            i1->reg_high = NONE;  // restore i1

            c_udiv_rax_reg (i2->reg, 4);
          }
        }
        else
        {
          if (i2->kind == INT_REGISTER)
            c_udiv_rax_reg (i2->reg, 4);
          else
            c_udiv_rax_mem (i2->ea, 4);
        }

        istack_count--;
      }
      break;


      case P_UMOD4:   //  ( int4  int4   --> int4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        free_register (RDX);
        load_int4_into_reg (ref *i1, RAX);

        if (i2->kind == INT_CONSTANT)
        {
          i1->reg_high = RDX;   // to avoid that RDX be allocated by flush
          flush_int4_in_register (ref *i2);
          i1->reg_high = NONE;  // restore i1

          c_umod_rax_reg (i2->reg, 4);
        }
        else
        {
          if (i2->kind == INT_REGISTER)
            c_umod_rax_reg (i2->reg, 4);
          else
            c_umod_rax_mem (i2->ea, 4);
        }

        i1->kind = INT_REGISTER;
        i1->reg = RDX;
        i1->reg_high = NONE;

        istack_count--;
      }
      break;


      case P_SHL4:   //  ( uint4  uint4  --> uint4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (i2->kind == INT_CONSTANT)
        {
          flush_int4_in_register_for_modif (ref *i1);

          if (i2->icte != 0)  // 0 has no effect
            c_shl_reg_imm (i1->reg, (int4)i2->icte & 31, size => 4);    // imm limited to 32-bit
        }
        else
        {
          load_int4_into_reg (ref *i2, RCX);
          flush_int4_in_register_for_modif (ref *i1);
          c_shl_reg_CL (i1->reg, size => 4);
        }

        istack_count--;
      }
      break;


      case P_SHR4:   //  ( uint4  uint4  --> uint4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (i2->kind == INT_CONSTANT)
        {
          flush_int4_in_register_for_modif (ref *i1);

          if (i2->icte != 0)  // 0 has no effect
            c_shr_reg_imm (i1->reg, (int4)i2->icte & 31, size => 4);    // imm limited to 32-bit
        }
        else
        {
          load_int4_into_reg (ref *i2, RCX);
          flush_int4_in_register_for_modif (ref *i1);
          c_shr_reg_CL (i1->reg, size => 4);
        }

        istack_count--;
      }
      break;


      // int8
      //-----

      case P_NEG8:     //  ( int8  --> int8 )
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (i->kind == INT_CONSTANT)
        {
          i->icte = - i->icte;
        }
        else
        {
          flush_int8_in_registers_for_modif (ref *i);

          if (address_size == 4)
          {
            c_neg_reg     (i->reg,         size => 4);
            c_adc_reg_imm (i->reg_high, 0, size => 4);
            c_neg_reg     (i->reg_high,    size => 4);
          }
          else   // 64-bit
          {
            c_neg_reg (i->reg, size => 8);
          }
        }
      }
      break;


      case P_NOT8:     //  ( int8  --> int8 )
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (i->kind == INT_CONSTANT)
        {
          i->icte = ~i->icte;
        }
        else
        {
          flush_int8_in_registers_for_modif (ref *i);

          if (address_size == 4)
          {
            // inverse all bits
            c_not_reg (i->reg,      size => 4);
            c_not_reg (i->reg_high, size => 4);
          }
          else   // 64-bit
          {
            c_not_reg (i->reg, size => 8);
          }
        }
      }
      break;


      case P_ADD8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_registers_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if (i2->icte == 0)  // "+ 0" has no effect
            {
              // leave first register operand unchanged
            }
            else
            {
              if (address_size == 4)
              {
                c_add_reg_imm (i1->reg,      (int4)i2->icte,         size => 4);    // imm limited to 32-bit
                c_adc_reg_imm (i1->reg_high, (int4)(i2->icte >> 32), size => 4);    // imm limited to 32-bit
              }
              else
              {
                if (i2->icte >= -(int8)2147483648 && i2->icte <= 2147483647)   // a small int8 constant
                {
                  if (i2->icte == 1)
                  {
                    c_inc_reg (i1->reg, size => 8);
                  }
                  else if (i2->icte == -1)
                  {
                    c_dec_reg (i1->reg, size => 8);
                  }
                  else
                  {
                    c_add_reg_imm (i1->reg, (int4)i2->icte, size => 8);
                  }
                }
                else
                {
                  REG r;
                  r = allocate_register (size => 8, false, *i1);
                  c_mov_reg_imm (r, i2->icte, size => 8);       // 64-bit large imm value
                  c_add_reg_reg (i1->reg, r, size => 8);
                }
              }
            }
            break;

          case INT_REGISTER:
            if (address_size == 4)
            {
              c_add_reg_reg (i1->reg,      i2->reg,      size => 4);
              c_adc_reg_reg (i1->reg_high, i2->reg_high, size => 4);
            }
            else
            {
              c_add_reg_reg (i1->reg, i2->reg, size => 8);
            }
            break;

          case MEMORY:
            if (address_size == 4)
            {
              c_add_reg_mem (i1->reg,      i2->ea, size => 4);
              i2->ea.offset += 4;
              c_adc_reg_mem (i1->reg_high, i2->ea, size => 4);
              i2->ea.offset -= 4;
            }
            else
            {
              c_add_reg_mem (i1->reg, i2->ea, size => 8);
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_SUB8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_registers_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if (i2->icte == 0)  // "- 0" has no effect
            {
              // leave first register operand unchanged
            }
            else
            {
              if (address_size == 4)
              {
                c_sub_reg_imm (i1->reg,      (int4)i2->icte,         size => 4);    // imm limited to 32-bit
                c_sbb_reg_imm (i1->reg_high, (int4)(i2->icte >> 32), size => 4);    // imm limited to 32-bit
              }
              else
              {
                if (i2->icte >= -(int8)2147483648 && i2->icte <= 2147483647)   // a small int8 constant
                {
                  if (i2->icte == 1)
                  {
                    c_dec_reg (i1->reg, size => 8);
                  }
                  else if (i2->icte == -1)
                  {
                    c_inc_reg (i1->reg, size => 8);
                  }
                  else
                  {
                    c_sub_reg_imm (i1->reg, (int4)i2->icte, size => 8);
                  }
                }
                else
                {
                  REG r;
                  r = allocate_register (size => 8, false, *i1);
                  c_mov_reg_imm (r, i2->icte, size => 8);       // 64-bit large imm value
                  c_sub_reg_reg (i1->reg, r, size => 8);
                }
              }
            }
            break;

          case INT_REGISTER:
            if (address_size == 4)
            {
              c_sub_reg_reg (i1->reg,      i2->reg,      size => 4);
              c_sbb_reg_reg (i1->reg_high, i2->reg_high, size => 4);
            }
            else
            {
              c_sub_reg_reg (i1->reg, i2->reg, size => 8);
            }
            break;

          case MEMORY:
            if (address_size == 4)
            {
              c_sub_reg_mem (i1->reg,      i2->ea, size => 4);
              i2->ea.offset += 4;
              c_sbb_reg_mem (i1->reg_high, i2->ea, size => 4);
              i2->ea.offset -= 4;
            }
            else
            {
              c_sub_reg_mem (i1->reg, i2->ea, size => 8);
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_SMUL8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (address_size == 4)
        {
          store_all_registers_in_temporaries_except_for_this_pcode ();

          push_int8 (*i2);
          push_int8 (*i1);

          c_call_relative (label_multiply_int8, 4);
          used_multiply_int8 = true;

          i1->kind = INT_REGISTER;
          i1->reg = RAX;
          i1->reg_high = RDX;
        }
        else  // 64-bit
        {
          if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
              (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
              i1->kind == INT_CONSTANT)
          {
            swap_nodes (ref *i1, ref *i2);
          }

          // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
          flush_int8_in_registers_for_modif (ref *i1);

          switch (i2->kind)
          {
            case INT_CONSTANT:
              if (i2->icte == 1)  // "* 1" has no effect
              {
                // leave first register operand unchanged
              }
              else if (i2->icte == 0)  // "* 0" yields zero
              {
                c_mov_reg_imm (i1->reg, 0, size => 4);
              }
              else
              {
                if (i2->icte >= -(int8)2147483648 && i2->icte <= 2147483647)   // a small int8 constant
                {
                  c_imul_reg_imm (i1->reg, (int4)i2->icte, size => 8);
                }
                else
                {
                  REG r;
                  r = allocate_register (size => 8, false, *i1);
                  c_mov_reg_imm (r, i2->icte, size => 8);       // 64-bit large imm value
                  c_imul_reg_reg (i1->reg, r, size => 8);
                }
              }
              break;

            case INT_REGISTER:
              c_imul_reg_reg (i1->reg, i2->reg, size => 8);
              break;

            case MEMORY:
              c_imul_reg_mem (i1->reg, i2->ea, size => 8);
              break;

            default:
              abort;
          }
        }

        istack_count--;
      }
      break;


      case P_SDIV8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1, i2;

        i1 = &istack[istack_count - 2];
        i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (address_size == 4)
        {
          store_all_registers_in_temporaries_except_for_this_pcode ();

          push_int8 (*i2);
          push_int8 (*i1);

          c_call_relative (label_divide_int8, 4);
          used_divide_int8 = true;

          i1->kind = INT_REGISTER;
          i1->reg = RAX;
          i1->reg_high = RDX;
        }
        else
        {
          free_register (RDX);
          load_int8_into_reg_for_64bit (ref *i1, RAX);

          if (i2->kind == INT_CONSTANT)
          {
            if (i2->icte != 1)  // div 1 has no effect
            {
              i1->reg_high = RDX;   // to avoid that RDX be allocated by flush
              flush_int8_in_registers (ref *i2);
              i1->reg_high = NONE;  // restore i1

              c_idiv_rax_reg (i2->reg, 8);  // RAX = RAX / source  (8 byte) (64 bit only)  with RDX receiving remainder
            }
          }
          else
          {
            if (i2->kind == INT_REGISTER)
              c_idiv_rax_reg (i2->reg, 8);  // RAX = RAX / source  (8 byte) (64 bit only)  with RDX receiving remainder
            else
              c_idiv_rax_mem (i2->ea, 8);  // RAX = RAX / source  (8 byte) (64 bit only)  with RDX receiving remainder
          }
        }

        istack_count--;
      }
      break;


      case P_SMOD8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1, i2;

        i1 = &istack[istack_count - 2];
        i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        if (address_size == 4)
        {
          store_all_registers_in_temporaries_except_for_this_pcode ();

          push_int8 (*i2);
          push_int8 (*i1);

          c_call_relative (label_modulo_int8, 4);
          used_modulo_int8 = true;

          i1->kind = INT_REGISTER;
          i1->reg = RAX;
          i1->reg_high = RDX;
        }
        else
        {
          free_register (RDX);
          load_int8_into_reg_for_64bit (ref *i1, RAX);

          if (i2->kind == INT_CONSTANT)
          {
            i1->reg_high = RDX;   // to avoid that RDX be allocated by flush
            flush_int8_in_registers (ref *i2);
            i1->reg_high = NONE;  // restore i1

            c_imod_rax_reg (i2->reg, 8);  // RAX = RAX / source  (8 byte) (64 bit only)  with RDX receiving remainder
          }
          else
          {
            if (i2->kind == INT_REGISTER)
              c_imod_rax_reg (i2->reg, 8);  // RAX = RAX / source  (8 byte) (64 bit only)  with RDX receiving remainder
            else
              c_imod_rax_mem (i2->ea, 8);  // RAX = RAX / source  (8 byte) (64 bit only)  with RDX receiving remainder
          }

          i1->kind = INT_REGISTER;
          i1->reg = RDX;
          i1->reg_high = NONE;
        }

        istack_count--;
      }
      break;


      case P_BITAND8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_registers_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if (i2->icte == -1)  // "& 0xFFFFFFFFFFFFFFFF" has no effect
            {
              // leave first register operand unchanged
            }
            else if (i2->icte == 0)    // "& 0" yields 0
            {
              i1->kind = INT_CONSTANT;
              i1->icte = 0;
            }
            else
            {
              if (address_size == 4)
              {
                c_and_reg_imm (i1->reg,      (int4)i2->icte,         size => 4);    // imm limited to 32-bit
                c_and_reg_imm (i1->reg_high, (int4)(i2->icte >> 32), size => 4);    // imm limited to 32-bit
              }
              else
              {
                if (i2->icte >= -(int8)2147483648 && i2->icte <= 2147483647)   // a small int8 constant
                {
                  c_and_reg_imm (i1->reg, (int4)i2->icte, size => 8);
                }
                else
                {
                  REG r;
                  r = allocate_register (size => 8, false, *i1);
                  c_mov_reg_imm (r, i2->icte, size => 8);       // 64-bit large imm value
                  c_and_reg_reg (i1->reg, r, size => 8);
                }
              }
            }
            break;

          case INT_REGISTER:
            if (address_size == 4)
            {
              c_and_reg_reg (i1->reg,      i2->reg,      size => 4);
              c_and_reg_reg (i1->reg_high, i2->reg_high, size => 4);
            }
            else
            {
              c_and_reg_reg (i1->reg, i2->reg, size => 8);
            }
            break;

          case MEMORY:
            if (address_size == 4)
            {
              c_and_reg_mem (i1->reg,      i2->ea, size => 4);
              i2->ea.offset += 4;
              c_and_reg_mem (i1->reg_high, i2->ea, size => 4);
              i2->ea.offset -= 4;
            }
            else
            {
              c_and_reg_mem (i1->reg, i2->ea, size => 8);
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_BITOR8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_registers_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if (i2->icte == -1)  // "OR 0xFFFFFFFFFFFFFFFF" yields -1
            {
              i1->kind = INT_CONSTANT;
              i1->icte = -1;
            }
            else if (i2->icte == 0)  // "OR 0" has no effect
            {
              // leave first register operand unchanged
            }
            else
            {
              if (address_size == 4)
              {
                c_or_reg_imm (i1->reg,      (int4)i2->icte,         size => 4);    // imm limited to 32-bit
                c_or_reg_imm (i1->reg_high, (int4)(i2->icte >> 32), size => 4);    // imm limited to 32-bit
              }
              else
              {
                if (i2->icte >= -(int8)2147483648 && i2->icte <= 2147483647)   // a small int8 constant
                {
                  c_or_reg_imm (i1->reg, (int4)i2->icte, size => 8);
                }
                else
                {
                  REG r;
                  r = allocate_register (size => 8, false, *i1);
                  c_mov_reg_imm (r, i2->icte, size => 8);       // 64-bit large imm value
                  c_or_reg_reg (i1->reg, r, size => 8);
                }
              }
            }
            break;

          case INT_REGISTER:
            if (address_size == 4)
            {
              c_or_reg_reg (i1->reg,      i2->reg,      size => 4);
              c_or_reg_reg (i1->reg_high, i2->reg_high, size => 4);
            }
            else
            {
              c_or_reg_reg (i1->reg, i2->reg, size => 8);
            }
            break;

          case MEMORY:
            if (address_size == 4)
            {
              c_or_reg_mem (i1->reg,      i2->ea, size => 4);
              i2->ea.offset += 4;
              c_or_reg_mem (i1->reg_high, i2->ea, size => 4);
              i2->ea.offset -= 4;
            }
            else
            {
              c_or_reg_mem (i1->reg, i2->ea, size => 8);
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_BITXOR8:   //  ( int8  int8   --> int8 )
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_registers_for_modif (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if (i2->icte == 0)  // "xor 0" has no effect
            {
              // leave first register operand unchanged
            }
            else
            {
              if (address_size == 4)
              {
                c_xor_reg_imm (i1->reg,      (int4)i2->icte,         size => 4);    // imm limited to 32-bit
                c_xor_reg_imm (i1->reg_high, (int4)(i2->icte >> 32), size => 4);    // imm limited to 32-bit
              }
              else
              {
                if (i2->icte >= -(int8)2147483648 && i2->icte <= 2147483647)   // a small int8 constant
                {
                  if (i2->icte == -1)  // "XOR 0xFFFFFFFFFFFFFFFF"
                  {
                    c_not_reg (i1->reg, size => 8);  // reverse all bits
                  }
                  else
                  {
                    c_xor_reg_imm (i1->reg, (int4)i2->icte, size => 8);
                  }
                }
                else
                {
                  REG r;
                  r = allocate_register (size => 8, false, *i1);
                  c_mov_reg_imm (r, i2->icte, size => 8);       // 64-bit large imm value
                  c_xor_reg_reg (i1->reg, r, size => 8);
                }
              }
            }
            break;

          case INT_REGISTER:
            if (address_size == 4)
            {
              c_xor_reg_reg (i1->reg,      i2->reg,      size => 4);
              c_xor_reg_reg (i1->reg_high, i2->reg_high, size => 4);
            }
            else
            {
              c_xor_reg_reg (i1->reg, i2->reg, size => 8);
            }
            break;

          case MEMORY:
            if (address_size == 4)
            {
              c_xor_reg_mem (i1->reg,      i2->ea, size => 4);
              i2->ea.offset += 4;
              c_xor_reg_mem (i1->reg_high, i2->ea, size => 4);
              i2->ea.offset -= 4;
            }
            else
            {
              c_xor_reg_mem (i1->reg, i2->ea, size => 8);
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //  ( int8  int8   --> int8 )

      case P_ASL8:       // <near_label_nr>      ;  <<
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_registers_for_modif (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          if (i2->icte == 0)  // "0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            if (address_size == 4)
            {
              if (i2->icte >= 32)
              {
                c_mov_reg_reg (i1->reg_high, i1->reg, size => 4);
                c_mov_reg_imm (i1->reg,            0, size => 4);

                if (i2->icte > 32)
                  c_shl_reg_imm (i1->reg_high, ((int)i2->icte - 32) & 31, size => 4);
              }
              else
              {
                c_shld_reg_reg_imm (i1->reg_high, i1->reg, (int)i2->icte & 31, size => 4);
                c_shl_reg_imm      (i1->reg,               (int)i2->icte & 31, size => 4);
              }
            }
            else   // 64-bit
            {
              c_shl_reg_imm (i1->reg, (int4)i2->icte & 63, size => 8);
            }
          }
        }
        else   // i2 is register or memory
        {
          i2->typ = 'i';  // long to int
          load_int4_into_reg (ref *i2, RCX);

          flush_int8_in_registers_for_modif (ref *i1);   // in case freeing register rcx pushed i1 in a temporary

          if (address_size == 4)
          {
            c_cmp_reg_imm (RCX, 32, size => 1);
            c_jcond (CMP_SMALLER,  /*signed=*/ false,  near_label_nr);

            c_mov_reg_reg (i1->reg_high, i1->reg, size => 4);
            c_mov_reg_imm (i1->reg,            0, size => 4);

            exe_declare_near_label (near_label_nr);

            c_shld_reg_reg_CL (i1->reg_high, i1->reg, size => 4);
            c_shl_reg_CL (i1->reg, size => 4);
          }
          else   // 64-bit
          {
            c_shl_reg_CL (i1->reg, size => 8);
          }
        }

        istack_count--;
      }
      break;


      //  ( int8  int8   --> int8 )

      case P_ASR8:       // <near_label_nr>      ;  >>
      {
        NODE* i1 = &istack[istack_count - 2];
        NODE* i2 = &istack[istack_count - 1];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_registers_for_modif (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          if (i2->icte == 0)  // "0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            if (address_size == 4)
            {
              if (i2->icte >= 32)
              {
                c_mov_reg_reg (i1->reg, i1->reg_high, size => 4);
                c_sar_reg_imm (i1->reg_high,      31, size => 4);  // copy sign bit to all bits

                if (i2->icte > 32)
                  c_sar_reg_imm (i1->reg, ((int)i2->icte - 32) & 31, size => 4);
              }
              else
              {
                c_shrd_reg_reg_imm (i1->reg, i1->reg_high, (int)i2->icte & 31, size => 4);
                c_sar_reg_imm      (i1->reg_high, (int)i2->icte & 31, size => 4);
              }
            }
            else   // 64-bit
            {
              c_sar_reg_imm (i1->reg, (int4)i2->icte & 63, size => 8);
            }
          }
        }
        else
        {
          i2->typ = 'i';  // long to int
          load_int4_into_reg (ref *i2, RCX);

          flush_int8_in_registers_for_modif (ref *i1);   // in case freeing register rcx pushed i1 in a temporary

          if (address_size == 4)
          {
            c_cmp_reg_imm (RCX, 32, size => 1);
            c_jcond (CMP_SMALLER,  /*signed=*/ false,  near_label_nr);

            c_mov_reg_reg (i1->reg, i1->reg_high, size => 4);
            c_sar_reg_imm (i1->reg_high,      31, size => 4);  // copy sign bit to all bits

            exe_declare_near_label (near_label_nr);

            c_shrd_reg_reg_CL (i1->reg, i1->reg_high, size => 4);
            c_sar_reg_CL (i1->reg_high, size => 4);
          }
          else   // 64-bit
          {
            c_sar_reg_CL (i1->reg, size => 8);
          }
        }

        istack_count--;
      }
      break;


  // float
  // -----

      //  ( float4  -->  float4 )

      case P_NEG_FLT4:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        if (f->kind == FLOAT_CONSTANT)
        {
          f->fcte = -f->fcte;
        }
        else
        {
          // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
          flush_float_in_register (ref *f);
          c_fneg ();
        }
      }
      break;


      //  ( float4  float4  -->  float4 )

      case P_ADD_FLT4:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 0.0)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              c_faddp ();
            }
            break;

          case FLOAT_REGISTER:
            c_faddp ();
            break;

          case MEMORY:
            c_fadd_fmem (f2->ea, size => 4);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  ( float4  float4  -->  float4 )

      case P_SUB_FLT4:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];
        bool reverse = false;

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
          reverse = true;
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 0.0 && !reverse)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              if (reverse)
                c_fsubrp ();
              else
                c_fsubp ();
            }
            break;

          case FLOAT_REGISTER:
            if (reverse)
              c_fsubrp ();
            else
              c_fsubp ();
            break;

          case MEMORY:
            if (reverse)
              c_fsubr_fmem (f2->ea, size => 4);
            else
              c_fsub_fmem (f2->ea, size => 4);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  ( float4  float4  -->  float4 )

      case P_MUL_FLT4:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 1.0)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              c_fmulp ();
            }
            break;

          case FLOAT_REGISTER:
            c_fmulp ();
            break;

          case MEMORY:
            c_fmul_fmem (f2->ea, size => 4);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  ( float4  float4  -->  float4 )

      case P_DIV_FLT4:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];
        bool reverse = false;

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
          reverse = true;
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 1.0 && !reverse)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              if (reverse)
                c_fdivrp ();
              else
                c_fdivp ();
            }
            break;

          case FLOAT_REGISTER:
            if (reverse)
              c_fdivrp ();
            else
              c_fdivp ();
            break;

          case MEMORY:
            if (reverse)
              c_fdivr_fmem (f2->ea, size => 4);
            else
              c_fdiv_fmem (f2->ea, size => 4);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;



  // double
  // ------

      //  ( float8          -->  float8 )

      case P_NEG_FLT8:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        if (f->kind == FLOAT_CONSTANT)
        {
          f->fcte = -f->fcte;
        }
        else
        {
          flush_float_in_register (ref *f);
          c_fneg ();
        }
      }
      break;


      //  ( float8  float8  -->  float8 )

      case P_ADD_FLT8:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 0.0)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              c_faddp ();
            }
            break;

          case FLOAT_REGISTER:
            c_faddp ();
            break;

          case MEMORY:
            c_fadd_fmem (f2->ea, size => 8);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      case P_SUB_FLT8:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];
        bool reverse = false;

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
          reverse = true;
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 0.0 && !reverse)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              if (reverse)
                c_fsubrp ();
              else
                c_fsubp ();
            }
            break;

          case FLOAT_REGISTER:
            if (reverse)
              c_fsubrp ();
            else
              c_fsubp ();
            break;

          case MEMORY:
            if (reverse)
              c_fsubr_fmem (f2->ea, size => 8);
            else
              c_fsub_fmem (f2->ea, size => 8);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      case P_MUL_FLT8:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 1.0)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              c_fmulp ();
            }
            break;

          case FLOAT_REGISTER:
            c_fmulp ();
            break;

          case MEMORY:
            c_fmul_fmem (f2->ea, size => 8);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      case P_DIV_FLT8:
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];
        bool reverse = false;

        if (g_tracing)
          trace ("\n");

        // make sure f1 is a register if possible
        if (f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER)
        {
          swap_nodes (ref *f1, ref *f2);
          reverse = true;
        }

        if (nb_float_registers_left() < (int)(f1->kind != FLOAT_REGISTER) + (int)(f2->kind == FLOAT_CONSTANT))
          store_all_float_registers_in_temporaries ();

        flush_float_in_register (ref *f1);

        switch (f2->kind)
        {
          case FLOAT_CONSTANT:
            if (f2->fcte == 1.0 && !reverse)
            {
              // leave first register operand unchanged
            }
            else
            {
              flush_float_in_register (ref *f2);
              if (reverse)
                c_fdivrp ();
              else
                c_fdivp ();
            }
            break;

          case FLOAT_REGISTER:
            if (reverse)
              c_fdivrp ();
            else
              c_fdivp ();
            break;

          case MEMORY:
            if (reverse)
              c_fdivr_fmem (f2->ea, size => 8);
            else
              c_fdiv_fmem (f2->ea, size => 8);
            break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;

  // -------------------------
  // 6. pre/post dec/increment
  // -------------------------

      //  (  <addr>  -->  /  )

      case P_INC1:    // pop addr_stack, increment int1 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);
        c_inc_mem (a->ea, size => 1);

        astack_count--;
      }
      break;


      case P_INC2:    // pop addr_stack, increment int2 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);
        c_inc_mem (a->ea, size => 2);

        astack_count--;
      }
      break;


      case P_INC4:    // pop addr_stack, increment int4 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);
        c_inc_mem (a->ea, size => 4);

        astack_count--;
      }
      break;


      case P_INC8:    // pop addr_stack, increment int8 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (address_size == 4)
        {
          c_add_mem_imm (a->ea, 1, size => 4);
          a->ea.offset += 4;
          c_adc_mem_imm (a->ea, 0, size => 4);
          a->ea.offset -= 4;
        }
        else
        {
          c_inc_mem (a->ea, size => 8);
        }

        astack_count--;
      }
      break;


      case P_DEC1:    // pop addr_stack, decrement int1 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);
        c_dec_mem (a->ea, size => 1);

        astack_count--;
      }
      break;


      case P_DEC2:    // pop addr_stack, decrement int2 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);
        c_dec_mem (a->ea, size => 2);

        astack_count--;
      }
      break;


      case P_DEC4:    // pop addr_stack, decrement int4 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);
        c_dec_mem (a->ea, size => 4);

        astack_count--;
      }
      break;


      case P_DEC8:    // pop addr_stack, decrement int8 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (address_size == 4)
        {
          c_sub_mem_imm (a->ea, 1, size => 4);
          a->ea.offset += 4;
          c_sbb_mem_imm (a->ea, 0, size => 4);
          a->ea.offset -= 4;
        }
        else
        {
          c_dec_mem (a->ea, size => 8);
        }

        astack_count--;
      }
      break;


  // --------------------------------
  // 7. combined arithmetic and store
  // --------------------------------

      // bool   (bool on int_stack)
      // ----

      case P_AND_BOOL_TO:    // &=    (  addr  bool  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_and_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_and_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_OR_BOOL_TO:    // |=    (  addr  bool  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_or_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_or_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_XOR_BOOL_TO:    // ^=    (  addr  bool  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_xor_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_xor_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      // int4   (signed value on int_stack)
      // ----

      case P_ADD1_TO:    // +=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_add_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_add_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_SUB1_TO:    // -=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_sub_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_sub_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_AND1_TO:    // &=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_and_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_and_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_OR1_TO:    // |=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_or_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_or_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_XOR1_TO:    // ^=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_xor_mem_imm (a->ea, (int)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          flush_int1_in_register (ref *i);      // flush into byte-aligned register
          c_xor_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_ASL1_TO:    // <<=  (signed)     (  addr  int4   -->   )
      case P_SHL1_TO:    // <<=  (unsigned)   (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_shl_mem_imm (a->ea, (int)i->icte & 31, size => 1);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_shl_mem_CL (a->ea, size => 1);
        }

        istack_count--;
        astack_count--;
      }
      break;

      case P_ASR1_TO:  // >>=  (signed)     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_sar_mem_imm (a->ea, (int)i->icte & 31, size => 1);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_sar_mem_CL (a->ea, size => 1);
        }

        istack_count--;
        astack_count--;
      }
      break;


      case P_ADD2_TO:    // +=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_add_mem_imm (a->ea, (int)i->icte, size => 2);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_add_mem_reg (a->ea, i->reg, /* size= */ 2);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_SUB2_TO:    // -=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_sub_mem_imm (a->ea, (int)i->icte, size => 2);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_sub_mem_reg (a->ea, i->reg, /* size= */ 2);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_AND2_TO:    // &=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_and_mem_imm (a->ea, (int)i->icte, size => 2);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_and_mem_reg (a->ea, i->reg, /* size= */ 2);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_OR2_TO:    // |=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_or_mem_imm (a->ea, (int)i->icte, size => 2);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_or_mem_reg (a->ea, i->reg, /* size= */ 2);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_XOR2_TO:    // ^=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_xor_mem_imm (a->ea, (int)i->icte, size => 2);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_xor_mem_reg (a->ea, i->reg, /* size= */ 2);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_ASL2_TO:    // <<=  (signed)     (  addr  int4   -->   )
      case P_SHL2_TO:    // <<=  (unsigned)   (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_shl_mem_imm (a->ea, (int)i->icte & 31, size => 2);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_shl_mem_CL (a->ea, size => 2);
        }

        istack_count--;
        astack_count--;
      }
      break;


      case P_ASR2_TO:  // >>=  (signed)     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_sar_mem_imm (a->ea, (int)i->icte & 31, size => 2);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_sar_mem_CL (a->ea, size => 2);
        }

        istack_count--;
        astack_count--;
      }
      break;


      case P_ADD4_TO:    // +=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_add_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_add_mem_reg (a->ea, i->reg, /* size= */ 4);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_SUB4_TO:    // -=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_sub_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_sub_mem_reg (a->ea, i->reg, /* size= */ 4);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_AND4_TO:    // &=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_and_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_and_mem_reg (a->ea, i->reg, /* size= */ 4);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_OR4_TO:    // |=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_or_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_or_mem_reg (a->ea, i->reg, /* size= */ 4);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_XOR4_TO:    // ^=     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_xor_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
        }
        else
        {
          flush_int4_in_register (ref *i);   // prefer loading 4 bytes, opcode is shorter
          c_xor_mem_reg (a->ea, i->reg, /* size= */ 4);
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_ASL4_TO:    // <<=  (signed)     (  addr  int4   -->   )
      case P_SHL4_TO:    // <<=  (unsigned)   (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_shl_mem_imm (a->ea, (int)i->icte & 31, size => 4);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_shl_mem_CL (a->ea, size => 4);
        }

        istack_count--;
        astack_count--;
      }
      break;


      case P_ASR4_TO:  // >>=  (signed)     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_sar_mem_imm (a->ea, (int)i->icte & 31, size => 4);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_sar_mem_CL (a->ea, size => 4);
        }

        istack_count--;
        astack_count--;
      }
      break;


  // uint4 (unsigned)
  // -----

      case P_SHR1_TO:  // >>=  (unsigned)     (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_shr_mem_imm (a->ea, (int)i->icte & 31, size => 1);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_shr_mem_CL (a->ea, size => 1);
        }

        istack_count--;
        astack_count--;
      }
      break;


      case P_SHR2_TO:  // >>=  (unsigned)     (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_shr_mem_imm (a->ea, (int)i->icte & 31, size => 2);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_shr_mem_CL (a->ea, size => 2);
        }

        istack_count--;
        astack_count--;
      }
      break;


      case P_SHR4_TO:  // >>=  (unsigned)     (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          c_shr_mem_imm (a->ea, (int)i->icte & 31, size => 4);
        }
        else
        {
          load_int4_into_reg (ref *i, RCX);
          c_shr_mem_CL (a->ea, size => 4);
        }

        istack_count--;
        astack_count--;
      }
      break;


      // int8
      // ----

      case P_ADD8_TO:    // +=     (  addr  int8  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (address_size == 4 || i->icte < -(int8)2147483648 || i->icte > +2147483647)  // 32-bit or large cte
          {
            c_add_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
            a->ea.offset += 4;
            c_adc_mem_imm (a->ea, (int)(i->icte >> 32), size => 4);    // imm limited to 32-bit.
          }
          else
          {
            c_add_mem_imm (a->ea, (int)i->icte, size => 8);    // imm limited to 32-bit.
          }
        }
        else
        {
          flush_int8_in_registers (ref *i);

          if (address_size == 4)
          {
            c_add_mem_reg (a->ea, i->reg, /* size= */ 4);
            a->ea.offset += 4;
            c_adc_mem_reg (a->ea, i->reg_high, /* size= */ 4);
          }
          else   // 64-bit
          {
            c_add_mem_reg (a->ea, i->reg, /* size= */ 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_SUB8_TO:    // -=     (  addr  int8  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (address_size == 4 || i->icte < -(int8)2147483648 || i->icte > +2147483647)  // 32-bit or large cte
          {
            c_sub_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
            a->ea.offset += 4;
            c_sbb_mem_imm (a->ea, (int)(i->icte >> 32), size => 4);    // imm limited to 32-bit.
          }
          else
          {
            c_sub_mem_imm (a->ea, (int)i->icte, size => 8);    // imm limited to 32-bit.
          }
        }
        else
        {
          flush_int8_in_registers (ref *i);

          if (address_size == 4)
          {
            c_sub_mem_reg (a->ea, i->reg, /* size= */ 4);
            a->ea.offset += 4;
            c_sbb_mem_reg (a->ea, i->reg_high, /* size= */ 4);
          }
          else   // 64-bit
          {
            c_sub_mem_reg (a->ea, i->reg, /* size= */ 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_AND8_TO:    // &=     (  addr  int8  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (address_size == 4 || i->icte < -(int8)2147483648 || i->icte > +2147483647)  // 32-bit or large cte
          {
            c_and_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
            a->ea.offset += 4;
            c_and_mem_imm (a->ea, (int)(i->icte >> 32), size => 4);    // imm limited to 32-bit.
          }
          else
          {
            c_and_mem_imm (a->ea, (int)i->icte, size => 8);    // imm limited to 32-bit.
          }
        }
        else
        {
          flush_int8_in_registers (ref *i);

          if (address_size == 4)
          {
            c_and_mem_reg (a->ea, i->reg, /* size= */ 4);
            a->ea.offset += 4;
            c_and_mem_reg (a->ea, i->reg_high, /* size= */ 4);
          }
          else   // 64-bit
          {
            c_and_mem_reg (a->ea, i->reg, /* size= */ 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;

      case P_OR8_TO:    // |=     (  addr  int8  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (address_size == 4 || i->icte < -(int8)2147483648 || i->icte > +2147483647)  // 32-bit or large cte
          {
            c_or_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
            a->ea.offset += 4;
            c_or_mem_imm (a->ea, (int)(i->icte >> 32), size => 4);    // imm limited to 32-bit.
          }
          else
          {
            c_or_mem_imm (a->ea, (int)i->icte, size => 8);    // imm limited to 32-bit.
          }
        }
        else
        {
          flush_int8_in_registers (ref *i);

          if (address_size == 4)
          {
            c_or_mem_reg (a->ea, i->reg, /* size= */ 4);
            a->ea.offset += 4;
            c_or_mem_reg (a->ea, i->reg_high, /* size= */ 4);
          }
          else   // 64-bit
          {
            c_or_mem_reg (a->ea, i->reg, /* size= */ 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;

      case P_XOR8_TO:    // ^=     (  addr  int8  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (address_size == 4 || i->icte < -(int8)2147483648 || i->icte > +2147483647)  // 32-bit or large cte
          {
            c_xor_mem_imm (a->ea, (int)i->icte, size => 4);    // imm limited to 32-bit.
            a->ea.offset += 4;
            c_xor_mem_imm (a->ea, (int)(i->icte >> 32), size => 4);    // imm limited to 32-bit.
          }
          else
          {
            c_xor_mem_imm (a->ea, (int)i->icte, size => 8);    // imm limited to 32-bit.
          }
        }
        else
        {
          flush_int8_in_registers (ref *i);

          if (address_size == 4)
          {
            c_xor_mem_reg (a->ea, i->reg, /* size= */ 4);
            a->ea.offset += 4;
            c_xor_mem_reg (a->ea, i->reg_high, /* size= */ 4);
          }
          else   // 64-bit
          {
            c_xor_mem_reg (a->ea, i->reg, /* size= */ 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_ASL8_TO:  // <<=  (signed)
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (i->icte == 0)  // "0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            if (address_size == 4)
            {
              REG r = allocate_register (size => 4, false, *i);

              if (i->icte >= 32)
              {
                c_mov_reg_mem (r, a->ea, size => 4);    // get low

                if (i->icte > 32)
                  c_shl_reg_imm (r, ((int)i->icte - 32) & 31, size => 4);

                a->ea.offset += 4;
                c_mov_mem_reg (a->ea, r, size => 4);    // store high
                a->ea.offset -= 4;

                c_mov_mem_imm (a->ea, 0, size => 4);   // clear low
              }
              else
              {
                c_mov_reg_mem (r, a->ea, size => 4);    // get low

                a->ea.offset += 4;
                c_shld_mem_reg_imm (a->ea, r, (int)i->icte & 31, size => 4);
                a->ea.offset -= 4;

                c_shl_mem_imm (a->ea, (int)i->icte & 31, size => 4);
              }
            }
            else   // 64-bit
            {
              c_shl_mem_imm (a->ea, (int4)i->icte & 63, size => 8);
            }
          }
        }
        else
        {
          i->typ = 'i';  // long to int
          load_int4_into_reg (ref *i, RCX);

          if (address_size == 4)
          {
            REG r = allocate_register (size => 4, false, *i);

            c_cmp_reg_imm (RCX, 32, size => 1);
            c_jcond (CMP_SMALLER,  /*signed=*/ false,  near_label_nr);

            c_mov_reg_mem (r, a->ea, size => 4);    // get low
            a->ea.offset += 4;
            c_mov_mem_reg (a->ea, r, size => 4);    // store high
            a->ea.offset -= 4;
            c_mov_mem_imm (a->ea, 0, size => 4);   // clear low

            exe_declare_near_label (near_label_nr);

            c_mov_reg_mem (r, a->ea, size => 4);    // get low
            a->ea.offset += 4;
            c_shld_mem_reg_CL (a->ea, r, size => 4);
            a->ea.offset -= 4;
            c_shl_mem_CL (a->ea, size => 4);
          }
          else   // 64-bit
          {
            c_shl_mem_CL (a->ea, size => 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_ASR8_TO:  // >>=  (signed)
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        flush_effective_address (ref *a);

        if (i->kind == INT_CONSTANT)
        {
          if (i->icte == 0)  // "0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            if (address_size == 4)
            {
              REG r = allocate_register (size => 4, false, *i);

              if (i->icte >= 32)
              {
                a->ea.offset += 4;
                c_mov_reg_mem (r, a->ea, size => 4);    // get high
                a->ea.offset -= 4;

                if (i->icte > 32)
                  c_sar_reg_imm (r, ((int)i->icte - 32) & 31, size => 4);

                c_mov_mem_reg (a->ea, r, size => 4);    // store low

                a->ea.offset += 4;
                c_sar_mem_imm (a->ea, 31, size => 4);   // for high : copy sign bit to all bits
                a->ea.offset -= 4;
              }
              else
              {
                a->ea.offset += 4;
                c_mov_reg_mem (r, a->ea, size => 4);    // get high
                a->ea.offset -= 4;

                c_shrd_mem_reg_imm (a->ea, r, (int)i->icte & 31, size => 4);

                a->ea.offset += 4;
                c_sar_mem_imm (a->ea, (int)i->icte & 31, size => 4);
                a->ea.offset -= 4;
              }
            }
            else   // 64-bit
            {
              c_sar_mem_imm (a->ea, (int4)i->icte & 63, size => 8);
            }
          }
        }
        else
        {
          i->typ = 'i';  // long to int
          load_int4_into_reg (ref *i, RCX);

          if (address_size == 4)
          {
            REG r = allocate_register (size => 4, false, *i);

            c_cmp_reg_imm (RCX, 32, size => 1);
            c_jcond (CMP_SMALLER,  /*signed=*/ false,  near_label_nr);

            a->ea.offset += 4;
            c_mov_reg_mem (r, a->ea, size => 4);    // get high
            a->ea.offset -= 4;
            c_mov_mem_reg (a->ea, r, size => 4);    // store low

            a->ea.offset += 4;
            c_sar_mem_imm (a->ea, 31, size => 4);   // for high : copy sign bit to all bits
            a->ea.offset -= 4;

            exe_declare_near_label (near_label_nr);

            a->ea.offset += 4;
            c_mov_reg_mem (r, a->ea, size => 4);    // get high
            a->ea.offset -= 4;

            c_shrd_mem_reg_CL (a->ea, r, size => 4);

            a->ea.offset += 4;
            c_sar_mem_CL (a->ea, size => 4);
            a->ea.offset -= 4;
          }
          else   // 64-bit
          {
            c_sar_mem_CL (a->ea, size => 8);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      // ( addr  int4  -->   )

      case P_UNSAFE_ADD_TO:      //  <size4>   ;  address[addr] += signed int4 * size4;
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        if (size != 0)
        {
          if (i->kind == INT_CONSTANT)
          {
            if (i->icte != 0)
              c_add_mem_imm (a->ea, (int)i->icte * size, size => address_size);
          }
          else
          {
            if (i->kind == INT_REGISTER)
            {
              if (size == 1)
              {
                // nothing to do
              }
              else if (size == 2)
              {
                flush_int4_in_register_for_modif (ref *i);
                c_add_reg_reg (i->reg, i->reg, 4);
              }
              else if (size == 3 || size == 5 || size == 9)
              {
                REG r;
                EA ea;

                flush_int4_in_register_for_modif (ref *i);
                r = i->reg;

                clear ea;
                ea.base = r;   ea.index = r;  ea.scale = size - 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
                c_lea_reg_mem (r, ea, 4);
              }
              else if (size >= 4 && size <= 2000000000 && (size & (size-1)) == 0)  // large power of 2
              {
                flush_int4_in_register_for_modif (ref *i);
                c_shl_reg_imm (i->reg, lshifts_of(size), 4);
              }
              else      // general case
              {
                flush_int4_in_register_for_modif (ref *i);
                c_imul_reg_reg_imm (i->reg, i->reg, size, /* size= */ 4);
              }
            }
            else    // MEMORY operand
            {
              REG r = allocate_register (size => 4, true, *i);   // allows reusing registers of node i

              if (size == 1)
              {
                c_mov_reg_mem (r, i->ea, /* size= */ 4);
              }
              else if (size == 2)
              {
                c_mov_reg_mem (r, i->ea, /* size= */ 4);
                c_add_reg_reg (r, r, 4);
              }
              else if (size == 3 || size == 5 || size == 9)
              {
                EA ea;

                c_mov_reg_mem (r, i->ea, /* size= */ 4);

                clear ea;
                ea.base = r;   ea.index = r;  ea.scale = size - 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
                c_lea_reg_mem (r, ea, 4);
              }
              else if (size >= 4 && size <= 2000000000 && (size & (size-1)) == 0)  // large power of 2
              {
                c_mov_reg_mem (r, i->ea, /* size= */ 4);
                c_shl_reg_imm (r, lshifts_of(size), 4);
              }
              else      // general case
              {
                c_imul_reg_mem_imm (r, i->ea, size, /* size= */ 4);
              }

              i->kind = INT_REGISTER;
              i->reg = r;
              i->reg_high = NONE;
            }

             if (address_size == 8)
               c_movsx_reg_reg (i->reg, 8, i->reg, 4);

            c_add_mem_reg (a->ea, i->reg, size => address_size);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      // ( addr  int4  -->   )

      case P_UNSAFE_SUB_TO:   //  <size4>   ;  address[addr] -= signed int4 * size4;
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        if (size != 0)
        {
          if (i->kind == INT_CONSTANT)
          {
            if (i->icte != 0)
              c_sub_mem_imm (a->ea, (int)i->icte * size, size => address_size);
          }
          else
          {
            if (i->kind == INT_REGISTER)
            {
              if (size == 1)
              {
                // nothing to do
              }
              else if (size == 2)
              {
                flush_int4_in_register_for_modif (ref *i);
                c_add_reg_reg (i->reg, i->reg, 4);
              }
              else if (size == 3 || size == 5 || size == 9)
              {
                REG r;
                EA ea;

                flush_int4_in_register_for_modif (ref *i);
                r = i->reg;

                clear ea;
                ea.base = r;   ea.index = r;  ea.scale = size - 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
                c_lea_reg_mem (r, ea, 4);
              }
              else if (size >= 4 && size <= 2000000000 && (size & (size-1)) == 0)  // large power of 2
              {
                flush_int4_in_register_for_modif (ref *i);
                c_shl_reg_imm (i->reg, lshifts_of(size), 4);
              }
              else      // general case
              {
                flush_int4_in_register_for_modif (ref *i);
                c_imul_reg_reg_imm (i->reg, i->reg, size, /* size= */ 4);
              }
            }
            else    // MEMORY operand
            {
              REG r = allocate_register (size => 4, true, *i);   // allows reusing registers of node i

              if (size == 1)
              {
                c_mov_reg_mem (r, i->ea, /* size= */ 4);
              }
              else if (size == 2)
              {
                c_mov_reg_mem (r, i->ea, /* size= */ 4);
                c_add_reg_reg (r, r, 4);
              }
              else if (size == 3 || size == 5 || size == 9)
              {
                EA ea;

                c_mov_reg_mem (r, i->ea, /* size= */ 4);

                clear ea;
                ea.base = r;   ea.index = r;  ea.scale = size - 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
                c_lea_reg_mem (r, ea, 4);
              }
              else if (size >= 4 && size <= 2000000000 && (size & (size-1)) == 0)  // large power of 2
              {
                c_mov_reg_mem (r, i->ea, /* size= */ 4);
                c_shl_reg_imm (r, lshifts_of(size), 4);
              }
              else      // general case
              {
                c_imul_reg_mem_imm (r, i->ea, size, /* size= */ 4);
              }

              i->kind = INT_REGISTER;
              i->reg = r;
              i->reg_high = NONE;
            }

             if (address_size == 8)
               c_movsx_reg_reg (i->reg, 8, i->reg, 4);

            c_sub_mem_reg (a->ea, i->reg, size => address_size);
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      // ( addr  -->   )

      case P_INC_ADDR:  // <size4>   ; pop addr_stack, increment addr at address by size
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        if (size == 0)
        {
          // do nothing
        }
        else if (size == 1)
        {
          c_inc_mem (a->ea, size => address_size);
        }
        else
        {
          c_add_mem_imm (a->ea, size, size => address_size);
        }

        astack_count--;
      }
      break;


      // ( addr  -->   )

      case P_DEC_ADDR:      // <size4>   ; pop addr_stack, decrement addr at address by size
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        if (size == 0)
        {
          // do nothing
        }
        else if (size == 1)
        {
          c_dec_mem (a->ea, size => address_size);
        }
        else
        {
          c_sub_mem_imm (a->ea, size, size => address_size);
        }

        astack_count--;
      }
      break;


      // ( addr  -->  addr1 )

      case P_INC_VALUE_ADDR:  // <size4>    ;  example: ++p   (inc [addr] by size, then take addr1 at [addr])
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;
        REG  r;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        if (size == 0)
        {
          // do nothing
        }
        else if (size == 1)
        {
          c_inc_mem (a->ea, size => address_size);
        }
        else
        {
          c_add_mem_imm (a->ea, size, size => address_size);
        }

//$ maybe just become MEMORY ?
        r = allocate_register (address_size, true, *a);   // allows reusing registers of node a
        c_mov_reg_mem (r, a->ea, address_size);

        a->typ = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = r;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;


      // ( addr  -->  addr1 )

      case P_DEC_VALUE_ADDR:  // <size4>    ;  example: --p
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;
        REG  r;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        if (size == 0)
        {
          // do nothing
        }
        else if (size == 1)
        {
          c_dec_mem (a->ea, size => address_size);
        }
        else
        {
          c_sub_mem_imm (a->ea, size, size => address_size);
        }

//$ maybe just become MEMORY ?
        r = allocate_register (address_size, true, *a);   // allows reusing registers of node a
        c_mov_reg_mem (r, a->ea, address_size);

        a->typ = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = r;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;


      // ( addr  -->  addr1 )

      case P_VALUE_ADDR_INC:  // <size4>    ;  example: p++   (take addr1 at [addr], then inc [addr] by size)
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;
        REG  r;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        r = allocate_register (address_size, false, *a);   // new register !

        c_mov_reg_mem (r, a->ea, address_size);

        if (size == 0)
        {
          // do nothing
        }
        else if (size == 1)
        {
          c_inc_mem (a->ea, size => address_size);
        }
        else
        {
          c_add_mem_imm (a->ea, size, size => address_size);
        }

        a->typ = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = r;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;


      // ( addr  -->  addr1 )

      case P_VALUE_ADDR_DEC:   // <size4>    ;  example: p--
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;
        REG  r;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        flush_effective_address (ref *a);

        r = allocate_register (address_size, false, *a);   // new register !

        c_mov_reg_mem (r, a->ea, address_size);

        if (size == 0)
        {
          // do nothing
        }
        else if (size == 1)
        {
          c_dec_mem (a->ea, size => address_size);
        }
        else
        {
          c_sub_mem_imm (a->ea, size, size => address_size);
        }

        a->typ = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = r;
        a->ea.index  = NONE;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;


  // -------------
  // 8. conversion
  // -------------

      case P_CONV_BOOL_INT:   //  (  bool1  -->   int4       )
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        i->typ = 'i';

        switch (i->kind)
        {
          case INT_CONSTANT:
            break;

          case INT_REGISTER:
            c_movzx_reg_reg (i->reg, 4, i->reg, 1);
            break;

          case MEMORY:
          {
            REG r;

            r = allocate_register (size => 4, true, *i);   // allows reusing registers of node i

            c_movzx_reg_mem (r, 4, i->ea, 1);

            i->kind = INT_REGISTER;
            i->reg = r;
            i->reg_high = NONE;
          }
          break;

          default:
            abort;
        }
      }
      break;


      case P_CONV_INT_BOOL:   //  (  int4   -->   bool1      )
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        i->typ = 'b';

        switch (i->kind)
        {
          case INT_CONSTANT:
            i->icte = (int)(i->icte != 0);
            break;

          case INT_REGISTER:
            flush_int1_in_register_for_modif (ref *i);    // flush into byte-aligned register
            break;

          case MEMORY:
            break;

          default:
            abort;
        }
      }
      break;


      //  (  int4   -->   int8       )   sign-extend

      case P_CONV_INT_LONG:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        i->typ = 'l';

        switch (i->kind)
        {
          case INT_CONSTANT:
            break;

          case INT_REGISTER:
          {
            if (address_size == 4)
            {
              REG r;
              r = allocate_register (size => 4, false, *i);
              i->reg_high = r;

              c_mov_reg_reg (r, i->reg, size => 4);
              c_sar_reg_imm (r, 31, size => 4);
            }
            else
            {
              c_movsx_reg_reg (i->reg, size_target => 8, i->reg, size_source => 4);
            }
          }
          break;

          case MEMORY:
          {
            if (address_size == 4)
            {
              REG r[2];

              allocate_registers (size => 4, false, *i, 2, out r);  // 2 new regs

              c_mov_reg_mem (r[0], i->ea, size => 4);
              c_mov_reg_reg (r[1], r[0], size => 4);
              c_sar_reg_imm (r[1], 31, size => 4);

              i->kind = INT_REGISTER;
              i->reg = r[0];
              i->reg_high = r[1];
            }
            else  // 64-bit
            {
              REG r;

              r = allocate_register (size => 8, false, *i);

              c_movsx_reg_mem (r, size_target => 8, i->ea, size_source => 4);

              i->kind = INT_REGISTER;
              i->reg = r;
              i->reg_high = NONE;
            }
          }
          break;

          default:
            abort;
        }
      }
      break;


      //  (  uint4  -->   int8       )   unsigned-extend

      case P_CONV_UINT_LONG:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        i->typ = 'l';

        switch (i->kind)
        {
          case INT_CONSTANT:
            break;

          case INT_REGISTER:
          {
            if (address_size == 4)
            {
              REG r;

              r = allocate_register (size => 4, false, *i);
              i->reg_high = r;

              c_mov_reg_imm (r, 0, size => 4);
            }
            else
            {
              c_mov_reg_reg (i->reg, i->reg, size => 4);    // will be zero-extended automatically to 8 bytes
            }
          }
          break;

          case MEMORY:
          {
            if (address_size == 4)
            {
              REG r[2];

              allocate_registers (size => 4, false, *i, 2, out r);  // 2 new regs

              c_mov_reg_mem (r[0], i->ea, size => 4);
              c_mov_reg_imm (r[1], 0, size => 4);

              i->kind = INT_REGISTER;
              i->reg = r[0];
              i->reg_high = r[1];
            }
            else  // 64-bit
            {
              REG r = allocate_register (size => 8, false, *i);

              c_mov_reg_mem (r, i->ea, size => 4);    // will be zero-extended automatically to 8 bytes

              i->kind = INT_REGISTER;
              i->reg = r;
              i->reg_high = NONE;
            }
          }
          break;

          default:
            abort;
        }
      }
      break;


      //  (  int8   -->   int4/uint4 )  truncate

      case P_CONV_LONG_INT:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        i->typ = 'i';

        switch (i->kind)
        {
          case INT_CONSTANT:
            break;

          case INT_REGISTER:
          {
            if (address_size == 4)
            {
              i->reg_high = NONE;
            }
            else
            {
              // make sure upper part is zero because that's assumed in reverse conversion
              c_mov_reg_reg (i->reg, i->reg, size => 4);    // will be zero-extended automatically to 8 bytes
            }
          }
          break;

          case MEMORY:
          {
            // use variable at same address
          }
          break;

          default:
            abort;
        }
      }
      break;


      //  (  int4   -->   float4  )

      case P_CONV_INT_FLT4:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (i->kind)
        {
          case INT_CONSTANT:
            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_CONSTANT;
              f->fcte = (double)i->icte;
            }
            break;

          case INT_REGISTER:
          {
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            ofs = allocate_temporary_zone (size => 4);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            c_mov_mem_reg (ea, i->reg, size => 4);
            c_fld_imem (ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          case MEMORY:
          {
            int4 freg;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            c_fld_imem (i->ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //  (  int4   -->   float8  )

      case P_CONV_INT_FLT8:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (i->kind)
        {
          case INT_CONSTANT:
            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_CONSTANT;
              f->fcte = (double)i->icte;
            }
            break;

          case INT_REGISTER:
          {
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            ofs = allocate_temporary_zone (size => 4);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            c_mov_mem_reg (ea, i->reg, size => 4);
            c_fld_imem (ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          case MEMORY:
          {
            int4 freg;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            c_fld_imem (i->ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //  (  uint4  -->   float4  )

      case P_CONV_UINT_FLT4:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (i->kind)
        {
          case INT_CONSTANT:
            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_CONSTANT;
              f->fcte = (double)(uint)i->icte;
            }
            break;

          case INT_REGISTER:
          {
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            ofs = allocate_temporary_zone (size => 8);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            if (address_size == 4)
            {
              c_mov_mem_reg (ea, i->reg, size => 4);
              ea.offset += 4;
              c_mov_mem_imm (ea, 0, size => 4);
              ea.offset -= 4;
            }
            else
            {
              c_mov_mem_reg (ea, i->reg, size => 8);   // assume the register is already zero-extended
            }

            c_fld_imem (ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          case MEMORY:
          {
            REG  r;
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            r = allocate_register (size => address_size, false, *i);

            ofs = allocate_temporary_zone (size => 8);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            if (address_size == 4)
            {
              c_mov_reg_mem (r, i->ea, size => 4);

              c_mov_mem_reg (ea, r, size => 4);
              ea.offset += 4;
              c_mov_mem_imm (ea, 0, size => 4);
              ea.offset -= 4;
            }
            else
            {
              c_mov_reg_mem (r, i->ea, size => 4);   // will zero-extend register to 8 bytes
              c_mov_mem_reg (ea, r, size => 8);
            }

            c_fld_imem (ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //  (  uint4  -->   float8  )

      case P_CONV_UINT_FLT8:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (i->kind)
        {
          case INT_CONSTANT:
          {
            NODE* f;
            f = &fstack[fstack_count++];
            f->typ = 'd';
            f->kind = FLOAT_CONSTANT;
            f->fcte = (double)(uint)i->icte;
            break;
          }

          case INT_REGISTER:
          {
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            ofs = allocate_temporary_zone (size => 8);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            if (address_size == 4)
            {
              c_mov_mem_reg (ea, i->reg, size => 4);
              ea.offset += 4;
              c_mov_mem_imm (ea, 0, size => 4);
              ea.offset -= 4;
            }
            else
            {
              c_mov_mem_reg (ea, i->reg, size => 8);   // assume the register is already zero-extended
            }

            c_fld_imem (ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          case MEMORY:
          {
            REG  r;
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            r    = allocate_register (size => 4, false, *i);

            ofs = allocate_temporary_zone (size => 8);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            if (address_size == 4)
            {
              c_mov_reg_mem (r, i->ea, size => 4);

              c_mov_mem_reg (ea, r, size => 4);
              ea.offset += 4;
              c_mov_mem_imm (ea, 0, size => 4);
              ea.offset -= 4;
            }
            else
            {
              c_mov_reg_mem (r, i->ea, size => 4);   // will zero-extend register to 8 bytes
              c_mov_mem_reg (ea, r, size => 8);
            }

            c_fld_imem (ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //  (  int8   -->   float4  )

      case P_CONV_LONG_FLT4:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (i->kind)
        {
          case INT_CONSTANT:
          {
            NODE* f;
            f = &fstack[fstack_count++];
            f->typ = 'f';
            f->kind = FLOAT_CONSTANT;
            f->fcte = (double)i->icte;
            break;
          }

          case INT_REGISTER:
          {
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            ofs = allocate_temporary_zone (size => 8);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            if (address_size == 4)
            {
              c_mov_mem_reg (ea, i->reg, size => 4);

              ea.offset += 4;
              c_mov_mem_reg (ea, i->reg_high, size => 4);
              ea.offset -= 4;
            }
            else
            {
              c_mov_mem_reg (ea, i->reg, size => 8);
            }

            c_fld_imem (ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          case MEMORY:
          {
            int4 freg;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            c_fld_imem (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_CONV_LONG_FLT8:
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (i->kind)
        {
          case INT_CONSTANT:
          {
            NODE* f;
            f = &fstack[fstack_count++];
            f->typ = 'd';
            f->kind = FLOAT_CONSTANT;
            f->fcte = (double)i->icte;
            break;
          }

          case INT_REGISTER:
          {
            int4 ofs, freg;
            EA   ea;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            ofs = allocate_temporary_zone (size => 8);   // must occur after allocating float register !

            clear ea;
            ea.base       = RSP;
            ea.index      = NONE;
            ea.scale      = 1;
            ea.offset     = ofs;
            ea.reloc.kind = RELOC_NONE;
            ea.reloc.nr   = 0;

            if (address_size == 4)
            {
              c_mov_mem_reg (ea, i->reg, size => 4);

              ea.offset += 4;
              c_mov_mem_reg (ea, i->reg_high, size => 4);
              ea.offset -= 4;
            }
            else
            {
              c_mov_mem_reg (ea, i->reg, size => 8);
            }

            c_fld_imem (ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          case MEMORY:
          {
            int4 freg;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            c_fld_imem (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).

            {
              NODE* f;
              f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
          }
          break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //  (  float4 -->   uint4 )

      case P_CONV_FLT4_UINT:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            NODE* i;
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (int8)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            int4 ofs;
            NODE* i;

            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          case MEMORY:
          {
            int4  freg, ofs;
            NODE* i;

            freg = allocate_float_register ();   // this can store all float registers to temps !
            _unused freg;
            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            c_fld (f->ea, size => 4);

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  (  float4 -->   int4  )

      case P_CONV_FLT4_INT:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            NODE* i;
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (int8)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            int4 ofs;
            NODE* i;

            ofs = allocate_temporary_zone (size => 4   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            save_8087_control_word (ofs+4);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+4);
          }
          break;

          case MEMORY:
          {
            int4 freg, ofs;
            NODE* i;

            freg = allocate_float_register ();   // this can store all float registers to temps !
            _unused freg;
            ofs = allocate_temporary_zone (size => 4   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            c_fld (f->ea, size => 4);

            save_8087_control_word (ofs+4);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+4);
          }
          break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  (  float4 -->   int8  )

      case P_CONV_FLT4_LONG:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            NODE* i;
            i = &istack[istack_count++];
            i->typ = 'l';
            i->kind = INT_CONSTANT;
            i->icte = (int8)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            int4 ofs;
            NODE* i;

            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'l';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          case MEMORY:
          {
            int4 freg, ofs;
            NODE* i;

            freg = allocate_float_register ();   // this can store all float registers to temps !
            _unused freg;
            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'l';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            c_fld (f->ea, size => 4);

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  (  float8 -->   uint4 )

      case P_CONV_FLT8_UINT:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            NODE* i;
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (int8)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            int4 ofs;
            NODE* i;

            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          case MEMORY:
          {
            int4 freg, ofs;
            NODE* i;

            freg = allocate_float_register ();   // this can store all float registers to temps !
            _unused freg;
            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            c_fld (f->ea, size => 8);

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  (  float8 -->   int4  )

      case P_CONV_FLT8_INT:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            NODE* i;
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (int8)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            int4 ofs;
            NODE* i;

            ofs = allocate_temporary_zone (size => 4   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            save_8087_control_word (ofs+4);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+4);
          }
          break;

          case MEMORY:
          {
            int4 freg, ofs;
            NODE* i;

            freg = allocate_float_register ();   // this can store all float registers to temps !
            _unused freg;
            ofs = allocate_temporary_zone (size => 4   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            c_fld (f->ea, size => 8);

            save_8087_control_word (ofs+4);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+4);
          }
          break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  (  float8 -->   int8       )

      case P_CONV_FLT8_LONG:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            NODE* i;
            i = &istack[istack_count++];
            i->typ = 'l';
            i->kind = INT_CONSTANT;
            i->icte = (int8)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            int4 ofs;
            NODE* i;

            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'l';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          case MEMORY:
          {
            int4 freg, ofs;
            NODE* i;

            freg = allocate_float_register ();   // this can store all float registers to temps !
            _unused freg;
            ofs = allocate_temporary_zone (size => 8   // must occur after allocating float register !
                                              + 2); // for control word
            i = &istack[istack_count++];
            i->typ = 'l';
            i->kind = MEMORY;
            i->ea.base       = RSP;
            i->ea.index      = NONE;
            i->ea.scale      = 1;
            i->ea.offset     = ofs;
            i->ea.reloc.kind = RELOC_NONE;
            i->ea.reloc.nr   = 0;

            c_fld (f->ea, size => 8);

            save_8087_control_word (ofs+8);
            change_8087_mode_to_trunc ();
            c_fistp (i->ea, size => 8);  // size is 2(int2), 4(int4) or 8(int8).
            restore_8087_control_word (ofs+8);
          }
          break;

          default:
            abort;
        }

        fstack_count--;
      }
      break;


      //  (  float4 -->   float8 )

      case P_CONV_FLT4_FLT8:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
            f->typ = 'd';
            break;

          case FLOAT_REGISTER:
            f->typ = 'd';
            break;

          case MEMORY:
          {
            int4 freg;

            freg = allocate_float_register ();   // this can store all float registers to temps !

            c_fld (f->ea, size => 4);

            f->typ = 'd';
            f->kind = FLOAT_REGISTER;
            f->freg = freg;
          }
          break;

          default:
            abort;
        }
      }
      break;


      //  (  float8 -->   float4     )

      case P_CONV_FLT8_FLT4:
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
            f->typ = 'f';
            break;

          case FLOAT_REGISTER:
          {
            PCODE next_pcode = (mem_offset < mem_size) ? *(PCODE*)&mem[mem_offset] : (PCODE)0;
            if (next_pcode == P_STORE_FLT4 || next_pcode == P_STORE_FIELD_FLT4)
            {
              f->typ = 'f';     // next pcode will convert to float4 anyway, so don't cast
            }
            else   // purpose is to reduce precision
            {
              int4 ofs = allocate_temporary_zone (size => 4);

              f->typ = 'f';

              f->kind = MEMORY;
              f->ea.base       = RSP;
              f->ea.index      = NONE;
              f->ea.scale      = 1;
              f->ea.offset     = ofs;
              f->ea.reloc.kind = RELOC_NONE;
              f->ea.reloc.nr   = 0;

              c_fstp (f->ea, size => 4);
            }
          }
          break;

          case MEMORY:
          {
            PCODE next_pcode = (mem_offset < mem_size) ? *(PCODE*)&mem[mem_offset] : (PCODE)0;
            if (next_pcode == P_STORE_FLT4 || next_pcode == P_STORE_FIELD_FLT4)
            {
              // next pcode will convert to float4 anyway, so don't cast

              int4 freg = allocate_float_register ();   // this can store all float registers to temps !

              c_fld (f->ea, size => 8);

              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = freg;
            }
            else
            {
              int4 freg = allocate_float_register ();   // this can store all float registers to temps !
              int4 ofs  = allocate_temporary_zone (size => 4);  // must occur after allocating float register !

              _unused freg;

              c_fld (f->ea, size => 8);

              f->typ = 'f';
              f->kind = MEMORY;
              f->ea.base       = RSP;
              f->ea.index      = NONE;
              f->ea.scale      = 1;
              f->ea.offset     = ofs;
              f->ea.reloc.kind = RELOC_NONE;
              f->ea.reloc.nr   = 0;

              c_fstp (f->ea, size => 4);
            }
          }
          break;

          default:
            abort;
        }
      }
      break;


      //  (  int4 -->  int4  )  extend uint1/uint2/int1/int2 to int4/uint4

      case P_EXTEND:
      {
        byte op = *((byte *)&mem[mem_offset++]);
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace (" op %u\n", op);

        switch (op)
        {
          case 0:   // uint1
            flush_int1_in_register_for_modif (ref *i);
            c_movzx_reg_reg (i->reg, 4, i->reg, 1);
            break;
          case 1:   // uint2
            flush_int4_in_register_for_modif (ref *i);
            c_movzx_reg_reg (i->reg, 4, i->reg, 2);
            break;
          case 2:   // int1
            flush_int1_in_register_for_modif (ref *i);
            c_movsx_reg_reg (i->reg, 4, i->reg, 1);
            break;
          case 3:   // int2
            flush_int4_in_register_for_modif (ref *i);
            c_movsx_reg_reg (i->reg, 4, i->reg, 2);
            break;
          default:
            fatal_compiler_error0 ("P_EXTEND");
            break;
        }
      }
      break;


  // --------
  // 9. names
  // --------

  // ----------------------------------------------------------------------

      // ( addr  -->  addr  uint4  )

      case P_GET_CONSTR0:  // <offset4>  ; load constraint from memory at [addr_stack[top] + offset]
      {
        NODE* a = &astack[astack_count - 1];
        int4 offset;
        REG  r;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset %d\n", offset);

        flush_effective_address (ref *a);

        r = allocate_register (4, false, *a);   // new register !

        a->ea.offset += offset;
        c_mov_reg_mem (r, a->ea, 4);
        a->ea.offset -= offset;

        {
          NODE* i;
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
      }
      break;


      // ( addr  addr2  -->  addr  addr2  uint4  )

      case P_GET_CONSTR1:  // <offset4> ; load constraint from memory at [addr_stack[top-1] + offset]
      {
        NODE* a = &astack[astack_count - 2];
        int4 offset;
        REG  r;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset %d\n", offset);

        flush_effective_address (ref *a);

        r = allocate_register (4, false, *a);   // new register !

        a->ea.offset += offset;
        c_mov_reg_mem (r, a->ea, 4);
        a->ea.offset -= offset;

        {
          NODE* i;
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = r;
          i->reg_high = NONE;
        }
      }
      break;


  // ----------------------------------------------------------------------

  // There are two alternatives for indexes :

  // a) constant index, constant length  --> compile-time check, use P_ADD_OFFSET to increase address.
  // b) otherwise, use P_CHECK_INDEX + P_ADD_INDEX :

      // if index4 >= length -> error       (uint4 comparison)
      //
      //   intel:
      //     cmp x,length
      //     jae error

      //    (  index4  length  -->  index4  )

      case P_CHECK_INDEX:    //  <near_label_nr4>
      {
        NODE* length = &istack[istack_count - 1];
        NODE* index  = &istack[istack_count - 2];
        int4  near_label_nr;
        bool  save_constant_index;
        NODE  saved_constant_index;
        bool  swapped_nodes;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        save_constant_index = false;
        clear saved_constant_index;
        if (index->kind == INT_CONSTANT)
        {
          save_constant_index = true;
          saved_constant_index = *index;
        }

        swapped_nodes = false;
        if (index->kind == INT_CONSTANT && index->icte != 0)  // don't swap for cte 0, it's 1 byte longer
        {
          swap_nodes (ref *index, ref *length);
          swapped_nodes = true;
        }

        flush_int4_in_register (ref *index);

        switch (length->kind)
        {
          case INT_CONSTANT:
            c_cmp_reg_imm (index->reg, (int)length->icte, 4);
            break;

          case INT_REGISTER:
            c_cmp_reg_reg (index->reg, length->reg, 4);
            break;

          case MEMORY:
            c_cmp_reg_mem (index->reg, length->ea, 4);
            break;

          default:
            abort;
        }

        if (swapped_nodes)
          swap_nodes (ref *index, ref *length);   // restore order

        if (save_constant_index)
          *index = saved_constant_index;

        c_jcond (swapped_nodes ? CMP_SMALLER_OR_EQUAL : CMP_LARGER_OR_EQUAL,
                 signed_operands => false,
                 store_ll (g_current_source_line, near_label_nr));

        istack_count--;
      }
      break;


      //    (  addr  index4  -->  addr2  )

      case P_ADD_INDEX:       // <size4>
      {
        NODE* addr  = &astack[astack_count - 1];
        NODE* index = &istack[istack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size %d\n", size);

        if (index->kind == INT_CONSTANT)
        {
          flush_effective_address (ref *addr);

          //   addr2 = addr + (index4 * size)
          addr->ea.offset += (int)index->icte * size;
        }
        else if (size == 0)
        {
          // nothing to do
        }
        else if (size == 1)   // just add to address
        {
          flush_effective_address (ref *addr);

          if (index->kind == INT_REGISTER)   // (we suppose upper part was zeroed)
          {
            if (addr->ea.base == NONE)
            {
              addr->ea.base = index->reg;   // set as base register
            }
            else if (addr->ea.index == NONE)
            {
              addr->ea.index = index->reg;   // set as index register
              addr->ea.scale = 1;
            }
            else if (addr->ea.base != RBP && addr->ea.base != RSP &&
                     register_usage_count(addr->ea.base) == 1)
            {
              c_add_reg_reg (addr->ea.base, index->reg, address_size);   // add to base register
            }
            else if (addr->ea.scale == 1 &&
                     addr->ea.index != RBP && addr->ea.index != RSP &&
                     register_usage_count(addr->ea.index) == 1)
            {
              c_add_reg_reg (addr->ea.index, index->reg, address_size);   // add to index register
            }
            else
            {
              REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr

              c_lea_reg_mem (r, addr->ea, size => address_size);

              addr->ea.base = r;
              addr->ea.index = index->reg;     // set as index register with scale 1
              addr->ea.scale = 1;
              addr->ea.offset = 0;
              addr->ea.reloc.kind = RELOC_NONE;
              addr->ea.reloc.nr   = 0;
            }
          }
          else  // index is MEMORY operand (int4)
          {
            if (address_size == 4)
            {
              if (addr->ea.base != NONE && addr->ea.base != RBP && addr->ea.base != RSP &&
                       register_usage_count(addr->ea.base) == 1)
              {
                c_add_reg_mem (addr->ea.base, index->ea, 4);
              }
              else if (addr->ea.scale == 1 &&
                       addr->ea.index != NONE && addr->ea.index != RBP && addr->ea.index != RSP &&
                       register_usage_count(addr->ea.index) == 1)
              {
                c_add_reg_mem (addr->ea.index, index->ea, 4);
              }
              else if (addr->ea.base == NONE)
              {
                REG r = allocate_register (size => 4, false, *addr);
                addr->ea.base = r;
                c_mov_reg_mem (r, index->ea, 4);
              }
              else if (addr->ea.index == NONE)
              {
                REG r = allocate_register (size => 4, false, *addr);
                addr->ea.index = r;   // set as index register
                addr->ea.scale = 1;
                c_mov_reg_mem (r, index->ea, 4);
              }
              else
              {
                REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr
                c_lea_reg_mem (r, addr->ea, size => address_size);
                addr->ea.base = r;
                addr->ea.index = NONE;
                addr->ea.scale = 1;
                addr->ea.offset = 0;
                addr->ea.reloc.kind = RELOC_NONE;
                addr->ea.reloc.nr   = 0;

                flush_int4_in_register (ref *index);
                addr->ea.index = index->reg;
              }
            }
            else   // 64-bit
            {
              if (addr->ea.base == NONE)
              {
                REG r = allocate_register (size => address_size, false, *addr);
                addr->ea.base = r;
                c_mov_reg_mem (r, index->ea, 4);    // will be zero-extended
              }
              else if (addr->ea.index == NONE)
              {
                REG r = allocate_register (size => address_size, false, *addr);
                addr->ea.index = r;   // set as index register
                addr->ea.scale = 1;
                c_mov_reg_mem (r, index->ea, 4);    // will be zero-extended
              }
              else if (addr->ea.base != NONE && addr->ea.base != RBP && addr->ea.base != RSP &&
                       register_usage_count(addr->ea.base) == 1)
              {
                REG r = allocate_register (size => address_size, false, *addr);
                c_mov_reg_mem (r, index->ea, 4);    // will be zero-extended
                c_add_reg_reg (addr->ea.base, r, address_size);
              }
              else if (addr->ea.scale == 1 &&
                       addr->ea.index != NONE && addr->ea.index != RBP && addr->ea.index != RSP &&
                       register_usage_count(addr->ea.index) == 1)
              {
                REG r = allocate_register (size => address_size, false, *addr);
                c_mov_reg_mem (r, index->ea, 4);    // will be zero-extended
                c_add_reg_reg (addr->ea.index, r, address_size);
              }
              else
              {
                REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr

                c_lea_reg_mem (r, addr->ea, size => address_size);
                addr->ea.base = r;
                addr->ea.index = NONE;
                addr->ea.scale = 1;
                addr->ea.offset = 0;
                addr->ea.reloc.kind = RELOC_NONE;
                addr->ea.reloc.nr   = 0;

                flush_int4_in_register (ref *index);    // will be zero-extended
                addr->ea.index = index->reg;
              }
            }
          }
        }
        else if (size == 2 || size == 4 || size == 8)   // exploit ea's scaling
        {
          flush_effective_address (ref *addr);

          if (addr->ea.index != NONE)   // index already taken : free it
          {
            REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr
            c_lea_reg_mem (r, addr->ea, size => address_size);
            addr->ea.base = r;
            addr->ea.index = NONE;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;
          }

          flush_int4_in_register (ref *index);

          addr->ea.index = index->reg;
          addr->ea.scale = size;
        }
        else if (size >= 16 && size <= 2000000000 && (size & (size-1)) == 0)  // large power of 2
        {
          flush_effective_address (ref *addr);

          flush_int4_in_register_for_modif (ref *index);

          c_shl_reg_imm (index->reg, lshifts_of(size), 4);

          if (addr->ea.base == NONE)
          {
            addr->ea.base = index->reg;
          }
          else if (addr->ea.index == NONE)
          {
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
          }
          else
          {
            REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr

            c_lea_reg_mem (r, addr->ea, size => address_size);

            addr->ea.base = r;
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;
          }
        }
        else      // general case
        {
          flush_effective_address (ref *addr);

          if (index->kind == INT_REGISTER)
          {
            flush_int4_in_register_for_modif (ref *index);
            c_imul_reg_imm (index->reg, size, 4);    // zero-extended
          }
          else   // index->kind == MEMORY
          {
            REG r = allocate_register (size => 4, true, *index);   // allows reusing registers of node index

            c_imul_reg_mem_imm (r, index->ea, size, 4);    // zero-extended

            index->kind = INT_REGISTER;
            index->reg = r;
          }

          if (addr->ea.base == NONE)
          {
            addr->ea.base = index->reg;
          }
          else if (addr->ea.index == NONE)
          {
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
          }
          else
          {
            REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr

            c_lea_reg_mem (r, addr->ea, size => address_size);

            addr->ea.base = r;
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;
          }
        }

        istack_count--;
      }
      break;


      case P_ADD_PTR_OFFSET:  // <size4>   (same for unsafe pointers, note that index4 is a signed offset !)
      {
        NODE* addr  = &astack[astack_count - 1];
        NODE* index = &istack[istack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size %d\n", size);

        if (index->kind == INT_CONSTANT)
        {
          flush_effective_address (ref *addr);
          addr->ea.offset += (int)index->icte * size;     //   addr2 = addr + (index4 * size)
        }
        else if (size == 0)
        {
          // nothing to do
        }
        else if (size == 1)   // just add to address
        {
          flush_effective_address (ref *addr);

          if (address_size == 8)    // we must sign-extend the operand on 8 bytes into a register
          {
            REG r = allocate_register (8, true, *index);   // allows reusing registers of node index

            if (index->kind == INT_REGISTER)
              c_movsx_reg_reg (r, 8, index->reg, 4);
            else
            {
              c_movsx_reg_mem (r, 8, index->ea, 4);
              index->kind = INT_REGISTER;
            }

            index->reg = r;
          }

          if (index->kind == INT_REGISTER)
          {
            if (addr->ea.base == NONE)
            {
              addr->ea.base = index->reg;
            }
            else if (addr->ea.index == NONE)
            {
              addr->ea.index = index->reg;
              addr->ea.scale = 1;
            }
            else if (addr->ea.base != RBP && addr->ea.base != RSP &&
                     register_usage_count(addr->ea.base) == 1)
            {
              c_add_reg_reg (addr->ea.base, index->reg, address_size);
            }
            else if (addr->ea.scale == 1 &&
                     addr->ea.index != RBP && addr->ea.index != RSP &&
                     register_usage_count(addr->ea.index) == 1)
            {
              c_add_reg_reg (addr->ea.index, index->reg, address_size);
            }
            else
            {
              REG r;
              r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr
              c_lea_reg_mem (r, addr->ea, size => address_size);
              addr->ea.base = r;
              addr->ea.index = index->reg;
              addr->ea.scale = 1;
              addr->ea.offset = 0;
              addr->ea.reloc.kind = RELOC_NONE;
              addr->ea.reloc.nr   = 0;
            }
          }
          else  // MEMORY operand  (this never occurs for 64-bit !)
          {
            if (addr->ea.base != NONE && addr->ea.base != RBP && addr->ea.base != RSP &&
                register_usage_count(addr->ea.base) == 1)
            {
              c_add_reg_mem (addr->ea.base, index->ea, 4);
            }
            else if (addr->ea.scale == 1 &&
                     addr->ea.index != NONE && addr->ea.index != RBP && addr->ea.index != RSP &&
                     register_usage_count(addr->ea.index) == 1)
            {
              c_add_reg_mem (addr->ea.index, index->ea, 4);
            }
            else if (addr->ea.base == NONE)
            {
              REG r = allocate_register (size => 4, false, *addr);
              addr->ea.base = r;

              c_mov_reg_mem (r, index->ea, 4);
            }
            else if (addr->ea.index == NONE)
            {
              REG r = allocate_register (size => 4, false, *addr);
              addr->ea.index = r;   // set as index register
              addr->ea.scale = 1;

              c_mov_reg_mem (r, index->ea, 4);
            }
            else
            {
              REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr

              c_lea_reg_mem (r, addr->ea, size => address_size);

              addr->ea.base = r;
              addr->ea.index = NONE;
              addr->ea.scale = 1;
              addr->ea.offset = 0;
              addr->ea.reloc.kind = RELOC_NONE;
              addr->ea.reloc.nr   = 0;

              flush_int4_in_register (ref *index);
              addr->ea.index = index->reg;
            }
          }
        }
        else if (size == 2 || size == 4 || size == 8)   // exploit ea's scaling
        {
          flush_effective_address (ref *addr);

          if (addr->ea.index != NONE)
          {
            REG r;
            r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr
            c_lea_reg_mem (r, addr->ea, size => address_size);
            addr->ea.base = r;
            addr->ea.index = NONE;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;
          }

          if (address_size == 8)    // we must sign-extend the operand on 8 bytes into a register
          {
            REG r = allocate_register (8, true, *index);   // allows reusing registers of node index

            if (index->kind == INT_REGISTER)
              c_movsx_reg_reg (r, 8, index->reg, 4);
            else
            {
              c_movsx_reg_mem (r, 8, index->ea, 4);
              index->kind = INT_REGISTER;
            }

            index->reg = r;
          }
          else
          {
            flush_int4_in_register (ref *index);
          }

          addr->ea.index = index->reg;
          addr->ea.scale = size;
        }
        else if (size >= 16 && size <= 2000000000 && (size & (size-1)) == 0)  // large power of 2
        {
          flush_effective_address (ref *addr);

          flush_int4_in_register_for_modif (ref *index);

          c_shl_reg_imm (index->reg, lshifts_of(size), 4);

          if (address_size == 8)    // we must sign-extend the operand on 8 bytes into a register
            c_movsx_reg_reg (index->reg, 8, index->reg, 4);

          if (addr->ea.base == NONE)
          {
            addr->ea.base = index->reg;
          }
          else if (addr->ea.index == NONE)
          {
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
          }
          else
          {
            REG r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr

            c_lea_reg_mem (r, addr->ea, size => address_size);

            addr->ea.base = r;
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;
          }
        }
        else      // general case
        {
          flush_effective_address (ref *addr);

          if (index->kind == INT_REGISTER)
          {
            flush_int4_in_register_for_modif (ref *index);
            c_imul_reg_imm (index->reg, size, 4);    // zero-extended

            if (address_size == 8)    // we must sign-extend the operand on 8 bytes
              c_movsx_reg_reg (index->reg, 8, index->reg, 4);
          }
          else   // index->kind == MEMORY
          {
            REG r;

            r = allocate_register (size => 4, true, *index);   // allows reusing registers of node index

            c_imul_reg_mem_imm (r, index->ea, size, 4);    // zero-extended

            index->kind = INT_REGISTER;
            index->reg = r;

            if (address_size == 8)    // we must sign-extend the operand on 8 bytes
              c_movsx_reg_reg (index->reg, 8, index->reg, 4);
          }

          if (addr->ea.base == NONE)
          {
            addr->ea.base = index->reg;
          }
          else if (addr->ea.index == NONE)
          {
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
          }
          else
          {
            REG r;
            r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr

            c_lea_reg_mem (r, addr->ea, size => address_size);

            addr->ea.base = r;
            addr->ea.index = index->reg;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;
          }
        }

        istack_count--;
      }
      break;


      case P_SUB_PTR_OFFSET:    //  <size4>     (  addr  index4  -->  addr2  )
      {
        NODE* addr  = &astack[astack_count - 1];
        NODE* index = &istack[istack_count - 1];
        int4 size;

        //   note: this is used for subtracting an offset from an unsafe pointer.
        //   note for 64-bit addresses : the index4 is signed, so it must be sign-extended to int8 !

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size %d\n", size);

        if (index->kind == INT_CONSTANT)
        {
          flush_effective_address (ref *addr);

          //   addr2 = addr - (index4 * size)
          addr->ea.offset -= (int)index->icte * size;
        }
        else if (size == 0)
        {
          // nothing to do
        }
        else if (size == 1)   // just subtract from address
        {
          flush_effective_address (ref *addr);

          if (address_size == 8)    // we must sign-extend the operand on 8 bytes into a register
          {
            REG r = allocate_register (8, true, *index);   // allows reusing registers of node index

            if (index->kind == INT_REGISTER)
              c_movsx_reg_reg (r, 8, index->reg, 4);
            else
            {
              c_movsx_reg_mem (r, 8, index->ea, 4);
              index->kind = INT_REGISTER;
            }

            index->reg = r;
          }

          {
            REG r;
            r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr
            c_lea_reg_mem (r, addr->ea, size => address_size);
            addr->ea.base = r;
            addr->ea.index = NONE;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;

            if (index->kind == INT_REGISTER)
              c_sub_reg_reg (r, index->reg, address_size);
            else
              c_sub_reg_mem (r, index->ea, 4);  // never occurs for 64-bit
          }
        }
        else if (size >= 2 && size <= 2000000000 && (size & (size-1)) == 0)  // power of 2
        {
          flush_effective_address (ref *addr);

          if (address_size == 8)    // we must sign-extend the operand on 8 bytes into a register
          {
            REG r = allocate_register (8, true, *index);   // allows reusing registers of node index

            if (index->kind == INT_REGISTER)
              c_movsx_reg_reg (r, 8, index->reg, 4);
            else
            {
              c_movsx_reg_mem (r, 8, index->ea, 4);
              index->kind = INT_REGISTER;
            }

            index->reg = r;
          }
          else
          {
            flush_int4_in_register_for_modif (ref *index);
          }

          {
            REG r;
            r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr
            c_lea_reg_mem (r, addr->ea, size => address_size);
            addr->ea.base = r;
            addr->ea.index = NONE;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;

            c_shl_reg_imm (index->reg, lshifts_of(size), address_size);
            c_sub_reg_reg (r, index->reg, address_size);
          }
        }
        else      // general case
        {
          flush_effective_address (ref *addr);

          // compute all in base reg
          {
            REG r;
            r = allocate_register (size => address_size, true, *addr);   // allows reusing registers of node addr
            c_lea_reg_mem (r, addr->ea, size => address_size);
            addr->ea.base = r;
            addr->ea.index = NONE;
            addr->ea.scale = 1;
            addr->ea.offset = 0;
            addr->ea.reloc.kind = RELOC_NONE;
            addr->ea.reloc.nr   = 0;
          }

          if (index->kind == INT_REGISTER)
          {
            flush_int4_in_register_for_modif (ref *index);

            c_imul_reg_imm (index->reg, size, 4);

            if (address_size == 8)    // we must sign-extend the operand on 8 bytes
              c_movsx_reg_reg (index->reg, 8, index->reg, 4);

            c_sub_reg_reg (addr->ea.base, index->reg, address_size);
          }
          else
          {
            REG r;
            r = allocate_register (size => address_size, true, *index);   // allows reusing registers of node index

            c_imul_reg_mem_imm (r, index->ea, size, 4);

            if (address_size == 8)    // we must sign-extend the operand on 8 bytes
              c_movsx_reg_reg (r, 8, r, 4);

            c_sub_reg_reg (addr->ea.base, r, address_size);
          }
        }

        istack_count--;
      }
      break;


  // ----------------------------------------------------------------------

  // There are several alternatives for slices :
  //
  //  a) cte ofs, cte len, cte length --> compile-time check, use P_ADD_OFFSET to increase address.
  //  b)          cte len, cte length --> use P_CHECK_SLICE_0 + P_ADD_INDEX.
  //  c)          cte len             --> use P_CHECK_SLICE_1 + P_ADD_INDEX.
  //  d) otherwise use general case   --> use P_CHECK_SLICE_2 + P_ADD_INDEX.

  // case b : pcode to be used in case of constant len and constant length.
  // --------

      // (  ofs  -->  ofs )

      //  if ofs > length - len --> error    (uint4 comparison)

      case P_CHECK_SLICE_0:   //  <(length-len)_uint4>  <near_label4>
      {
        NODE* ofs = &istack[istack_count - 1];
        int4 len, near_label_nr;

        len           = *((int4 *)&mem[mem_offset]);
        near_label_nr = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" len %d  label #%d\n", len, near_label_nr);

        flush_int4_in_register (ref *ofs);

        c_cmp_reg_imm (ofs->reg, len, 4);

        c_jcond (CMP_LARGER,
                 /*signed=*/ false,
                 store_ll (g_current_source_line, near_label_nr));
      }
      break;


  // case c : pcode to be used in case of constant len and runtime length.
  // --------

      // (  ofs  length  -->  ofs )

      case P_CHECK_SLICE_1:    //  <cte_len4>  <near_label4>
      {
        NODE* ofs    = &istack[istack_count - 2];
        NODE* length = &istack[istack_count - 1];
        int4 len, near_label_nr;

        //  if len > length       --> error    (uint4 comparison)
        //  if ofs > length - len --> error    (uint4 comparison)

        len           = *((int4 *)&mem[mem_offset]);
        near_label_nr = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" len %d  label #%d\n", len, near_label_nr);

        flush_int4_in_register_for_modif (ref *length);

        c_sub_reg_imm (length->reg, len, 4);
        c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));

        switch (ofs->kind)
        {
          case INT_CONSTANT:
            if (ofs->icte != 0)
            {
              c_cmp_reg_imm (length->reg, (int)ofs->icte, 4);
              c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));
            }
            break;

          case INT_REGISTER:
            c_cmp_reg_reg (length->reg, ofs->reg, 4);
            c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));
            break;

          case MEMORY:
            c_cmp_reg_mem (length->reg, ofs->ea, 4);
            c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


  // case d : pcode to be used in case of runtime len (len must be stored in a local temporary variable).
  // --------

      //  (  ofs  length  -->  ofs  )
      case P_CHECK_SLICE_2:   //  <len_local_addr4>  <near_label4>
      {
        NODE* ofs    = &istack[istack_count - 2];
        NODE* length = &istack[istack_count - 1];
        int4 len_addr, near_label_nr;
        EA   ea;

        // len is evaluated and stored in a local variable before the pcode.
        //
        //  if len > length       --> error    (uint4 comparison)
        //  if ofs > length - len --> error    (uint4 comparison)

        len_addr      = *((int4 *)&mem[mem_offset]);
        near_label_nr = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" len_addr %d  label #%d\n", len_addr, near_label_nr);

        flush_int4_in_register_for_modif (ref *length);

        clear ea;
        ea.base  = RBP;
        ea.index = NONE;
        ea.scale = 1;
        ea.offset = len_addr;
        ea.reloc.kind = RELOC_NONE;
        ea.reloc.nr   = 0;

        c_sub_reg_mem (length->reg, ea, 4);
        c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));

        switch (ofs->kind)
        {
          case INT_CONSTANT:
            if (ofs->icte != 0)
            {
              c_cmp_reg_imm (length->reg, (int)ofs->icte, 4);
              c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));
            }
            break;

          case INT_REGISTER:
            c_cmp_reg_reg (length->reg, ofs->reg, 4);
            c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));
            break;

          case MEMORY:
            c_cmp_reg_mem (length->reg, ofs->ea, 4);
            c_jcond (CMP_SMALLER, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      //  (   addr --> addr2 )

      case P_ADD_OFFSET:   // <offset4>
      {
        NODE* addr = &astack[astack_count - 1];
        int4  offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset #%d\n", offset);

        flush_effective_address (ref *addr);

        addr->ea.offset += offset;
      }
      break;


      //   (   addr  top_addr --> addr2  top_addr )

      case P_ADD_OFFSET1:   // <offset4>
      {
        NODE* addr = &astack[astack_count - 2];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset #%d\n", offset);

        flush_effective_address (ref *addr);
        addr->ea.offset += offset;
      }
      break;


      // (  value4  -->    )     if value != cte -> error

      case P_CHECK_SAME_1:   //  <cte>   <near_label4>
      {
        NODE* value = &istack[istack_count - 1];
        int4 cte, near_label_nr;

        cte           = *((int4 *)&mem[mem_offset]);
        near_label_nr = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" cte = #%d  near_label_nr = %d\n", cte, near_label_nr);

        switch (value->kind)
        {
          case INT_CONSTANT:
            fatal_compiler_error0 ("as86(P_CHECK_SAME)");
            break;

          case INT_REGISTER:
            c_cmp_reg_imm (value->reg, cte, 4);
            break;

          case MEMORY:
            c_cmp_mem_imm (value->ea, cte, 4);
            break;

          default:
            abort;
        }

        c_jcond (CMP_NOT_EQUAL,
                 /*signed=*/ false,
                 store_ll (g_current_source_line, near_label_nr));

        istack_count--;
      }
      break;


      //  (  value4  value4  -->  value4  )         if value != cte -> error

      case P_CHECK_SAME_2:   // <near_label4>
      {
        NODE* value1 = &istack[istack_count - 2];
        NODE* value2 = &istack[istack_count - 1];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" near_label_nr = %d\n", near_label_nr);

        if (value2->kind == INT_REGISTER)
          swap_nodes (ref *value1, ref *value2);

        flush_int4_in_register (ref *value1);

        switch (value2->kind)
        {
          case INT_CONSTANT:
            c_cmp_reg_imm (value1->reg, (int)value2->icte, 4);
            break;

          case INT_REGISTER:
            c_cmp_reg_reg (value1->reg, value2->reg, 4);
            break;

          case MEMORY:
            c_cmp_reg_mem (value1->reg, value2->ea, 4);
            break;

          default:
            abort;
        }

        c_jcond (CMP_NOT_EQUAL,
                 /*signed=*/ false,
                 store_ll (g_current_source_line, near_label_nr));

        istack_count--;
      }
      break;


      //  (   length  -->  size    )                   size = length * <size_element>

      case P_ARRAY_SIZE:    //  <size_element>
      {
        NODE* n = &istack[istack_count - 1];
        int4  elem_size;

        elem_size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" elem_size = %d\n", elem_size);

        switch (n->kind)
        {
          case INT_CONSTANT:
            n->icte *= elem_size;
            break;

          case INT_REGISTER:
          {
            if (register_usage_count (n->reg) == 1)
              c_imul_reg_imm (n->reg, elem_size, 4);
            else
            {
              REG r;
              r = allocate_register (size => 4, true, *n);   // allows reusing registers of node index
              c_imul_reg_reg_imm (r, n->reg, elem_size, 4);
              n->reg = r;
            }
          }
          break;

          case MEMORY:
          {
            REG r = allocate_register (size => 4, true, *n);   // allows reusing registers of node index
            c_imul_reg_mem_imm (r, n->ea, elem_size, 4);
            n->kind = INT_REGISTER;
            n->reg = r;
          }
          break;

          default:
            abort;
        }
      }
      break;


      //    (  addr1  addr2  -->  value4  )

      case P_SUB_PTRS:
      {
        NODE* a1 = &astack[astack_count - 2];
        NODE* a2 = &astack[astack_count - 1];
        NODE* i;

        //   compute value4 = addr1 - addr2
        //   note: this is used for subtracting two unsafe pointers

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *a1);
        flush_effective_address (ref *a2);

        if (a1->ea.base == NONE || a1->ea.base == RSP || a1->ea.base == RBP ||
            a1->ea.index != NONE || a1->ea.offset != 0 || a1->ea.reloc.kind != RELOC_NONE)
        {
          REG r = allocate_register (size => address_size, true, *a1);   // allows reusing registers of node a1

          c_lea_reg_mem (r, a1->ea, size => address_size);

          a1->ea.base = r;
          a1->ea.index = NONE;
          a1->ea.scale = 1;
          a1->ea.offset = 0;
          a1->ea.reloc.kind = RELOC_NONE;
          a1->ea.reloc.nr = 0;
        }

        if (a2->ea.base == NONE || a2->ea.base == RSP || a2->ea.base == RBP ||
            a2->ea.index != NONE || a2->ea.offset != 0 || a2->ea.reloc.kind != RELOC_NONE)
        {
          REG r = allocate_register (size => address_size, true, *a2);   // allows reusing registers of node a2

          c_lea_reg_mem (r, a2->ea, size => address_size);

          a2->ea.base = r;
          a2->ea.index = NONE;
          a2->ea.scale = 1;
          a2->ea.offset = 0;
          a2->ea.reloc.kind = RELOC_NONE;
          a2->ea.reloc.nr = 0;
        }

        c_sub_reg_reg (a1->ea.base, a2->ea.base, address_size);

        astack_count -= 2;

        i = &istack[istack_count++];
        i->typ = 'i';
        i->kind = INT_REGISTER;
        i->reg = a1->ea.base;
        i->reg_high = NONE;
      }
      break;


      //  (  <pointer_addr>  -->  <heap_object_addr>  )

      case P_DEREF:   // <type4>  <local_addr4>   <near_label4>  <near_label4>
      {
        NODE* a = &astack[astack_count - 1];
        int4 typ, loc_addr, near_label_nr, near_label_nr2;
        REG  r;
        EA   ea;

        typ            = *((int4 *)&mem[mem_offset]);
        loc_addr       = *((int4 *)&mem[mem_offset+4]);
        near_label_nr  = *((int4 *)&mem[mem_offset+8]);
        near_label_nr2 = *((int4 *)&mem[mem_offset+12]);
        mem_offset += 16;

        _unused near_label_nr2;

        if (g_tracing)
          trace (" typ=%d  loc_addr=%d  label=#%d\n", typ, loc_addr, near_label_nr);

        flush_effective_address (ref *a);

        if (a->ea.base == NONE && a->ea.index == NONE && a->ea.reloc.kind == RELOC_NONE)
        {
          r = allocate_register (address_size, true, *a);
          c_mov_reg_imm (r, a->ea.offset, address_size);             // probably null
        }
        else if (a->ea.base != NONE && a->ea.index == NONE && a->ea.offset == 0 && a->ea.reloc.kind == RELOC_NONE)
        {
          r = a->ea.base;   // r is read-only
        }
        else if (a->ea.base == NONE && a->ea.index != NONE && a->ea.scale == 1 &&
                 a->ea.offset == 0 && a->ea.reloc.kind == RELOC_NONE)
        {
          r = a->ea.index;   // r is read-only
        }
        else   // general case
        {
          r = allocate_register (address_size, true, *a);   // we can reuse a's registers
          c_lea_reg_mem (r, a->ea, size => address_size);
        }

        // lock and increment tombstone counter
        clear ea;
        ea.base = r;  ea.index = NONE;  ea.scale = 1;  ea.offset = 0;   ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
        c_lock_prefix ();
        c_inc_mem (ea, 4);      // causes exception if ptr is null

        // check that <type> matches tombstone type -> error if check fails
        ea.offset += 4;
        c_cmp_mem_imm (ea, typ, 4);
        ea.offset -= 4;
        c_jcond (CMP_NOT_EQUAL, /*signed=*/ false, store_ll (g_current_source_line, near_label_nr));

        // saves address of tombstone (thus pointer value) in <local_addr4>
        ea.base = RBP;  ea.index = NONE;  ea.scale = 1;  ea.offset = loc_addr;   ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
        c_mov_mem_reg (ea, r, address_size);

        // return address of heap object within the tombstone
        a->kind = MEMORY;
        a->ea.base = r;
        a->ea.index = NONE;
        a->ea.scale = 1;
        a->ea.offset = 8;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr = 0;
      }
      break;


      // lock and decrement tombstone counter at <local_addr>
      // lock dec (*local_addr)->counter

      case P_UNDEREF:   //  <local_addr4>  <near_label4>
      {
        int  loc_addr, near_label_nr;
        REG  r;
        EA   ea;
        NODE dummy;

        loc_addr      = *((int4 *)&mem[mem_offset]);
        near_label_nr = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" loc_addr=%d\n", loc_addr);

        _unused near_label_nr;

        clear dummy;
        r = allocate_register (address_size, false, dummy);
        clear ea;
        ea.base = RBP;  ea.index = NONE;  ea.scale = 1;  ea.offset = loc_addr;   ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
        c_mov_reg_mem (r, ea, address_size);

        ea.base = r;  ea.index = NONE;  ea.scale = 1;  ea.offset = 0;   ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
        c_lock_prefix ();
        c_dec_mem (ea, 4);
      }
      break;


  // for machine-code : force value in register.
  // this might be used before a P_UNDEREF code to make sure
  // the value is read from a heap object before it gets released.

      //   ( b --> b )

      case P_FORCE_BOOL:  // for m-code : force b in a register
      {
        NODE* i = &istack[istack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_int1_in_register (ref *i);
      }
      break;


      //   ( int4 --> int4 )

      case P_FORCE_4:     // for m-code : force int4 value in a register
      {
        NODE* i = &istack[istack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_int4_in_register (ref *i);
      }
      break;


      //   ( int8 --> int8 )

      case P_FORCE_8:     // for m-code : force int8 value in a register
      {
        NODE* i = &istack[istack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_int8_in_registers (ref *i);
      }
      break;


      //   ( float4 --> float4 )

      case P_FORCE_FLT4:    // for m-code : force float4 value in a register
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_float_in_register (ref *f);
      }
      break;


      //   ( float8 --> float8 )

      case P_FORCE_FLT8:    // for m-code : force float8 value in a register
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_float_in_register (ref *f);
      }
      break;


      //   ( addr --> addr )

      case P_FORCE_ADDR:   // for m-code : force addr value in a register
      {
        NODE* a = &astack[astack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_effective_address (ref *a);
      }
      break;


  // sync pcodes are used for operator "?:" and for switch statement.

      //   ( b --> b )

      case P_SYNC_BOOL:   // for m-code : force b in register AL
        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (RAX);
        sync_bool ();
        break;

      //   ( int4 --> int4 )

      case P_SYNC_4:     // for m-code : force int4 value in register EAX
        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (RAX);
        sync_int4 ();
        break;


      //   ( int8 --> int8 )

      case P_SYNC_8:     // for m-code : force int8 value in registers EDX:EAX (or RAX for 64-bit)
        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (RAX);
        if (address_size == 4)
          free_register_except_for_this_pcode (RDX);
        sync_int8 ();
        break;


      //   ( float4 --> float4 )

      case P_SYNC_FLT4:    // for m-code : force float4 value on top float stack
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_float_in_register (ref *f);
      }
      break;


      //   ( float8 --> float8 )

      case P_SYNC_FLT8:    // for m-code : force float8 value on top float stack
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_float_in_register (ref *f);
      }
      break;


      //  ( addr --> addr )

      case P_SYNC_ADDR:     // for m-code : force addr value in register EAX
        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (RAX);
        sync_addr ();
        break;

      //   ( addr int4 --> addr int4 )

      case P_SYNC_ADDR_INT4:  // for m-code : force addr value in register ESI and int4 in register EAX
        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (RAX);
        if (address_size == 4)
          free_register_except_for_this_pcode (RSI);
        sync_addr_int4 ();
        break;


      // (for m-code only : remove specified nb of entries from xx_stacks) (used in ?: statement before goto)

      case P_SYNC_STACKS:  // <int_stack> <float_stack> <addr_stack>
      {
        int i, f, a;

        i = *((int4 *)&mem[mem_offset]);
        f = *((int4 *)&mem[mem_offset+4]);
        a = *((int4 *)&mem[mem_offset+8]);
        mem_offset += 12;

        if (i > istack_count || f > fstack_count || a > astack_count)
          fatal_compiler_error0 ("P_SYNC_STACKS");

        if (g_tracing)
          trace (" \n");

        istack_count -= i;
        fstack_count -= f;
        astack_count -= a;

        g_after_i -= i;
        g_after_f -= f;
        g_after_a -= a;
      }
      break;


      //   (  --> int4 )

      case P_SHADOW_4:   // for m-code : after a branch, we assert that an int4 is in lowest CPU register (EAX).
      {
        NODE* i = &istack[istack_count++];

        if (g_tracing)
          trace (" \n");

        i->typ = 'i';
        i->kind = INT_REGISTER;
        i->reg = RAX;
        i->reg_high = NONE;
      }
      break;


      //   (  --> int8 )

      case P_SHADOW_8:    // for m-code : after a branch, we assert that an int8 is in lowest CPU register (RAX or EDX:EAX).
      {
        NODE* i = &istack[istack_count++];

        if (g_tracing)
          trace (" \n");

        i->typ = 'l';
        i->kind = INT_REGISTER;
        i->reg = RAX;
        i->reg_high = address_size == 4 ? RDX : NONE;
      }
      break;


  // --------------
  // 10. Aggregates
  // --------------

  // load address of aggregate on addr_stack, then store all fields :
  //
  // pop xx_stack, store value at addr_target + offset4



      case P_STORE_FIELD_BOOL:   // <offset4>    (used for bool)       ( addr_target   bool  -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *a);
        a->ea.offset += offset;

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 1, true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 1);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          flush_int1_in_register_for_modif (ref *i);      // flush into byte-aligned register
          c_mov_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        a->ea.offset -= offset;
        istack_count--;
      }
      break;


      case P_STORE_FIELD_1:    // <offset4>    (used for int1, uint1)      ( addr_target   b1  -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *a);
        a->ea.offset += offset;

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 1);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 1, true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 1);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          flush_int1_in_register_for_modif (ref *i);      // flush into byte-aligned register
          c_mov_mem_reg (a->ea, i->reg, /* size= */ 1);
        }

        a->ea.offset -= offset;
        istack_count--;
      }
      break;


      case P_STORE_FIELD_2:    // <offset4>          ( addr_target   b2 -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *a);
        a->ea.offset += offset;

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 2);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 2, true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 2);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          c_mov_mem_reg (a->ea, i->reg, /* size= */ 2);
        }

        a->ea.offset -= offset;
        istack_count--;
      }
      break;

      case P_STORE_FIELD_4:    // <offset4>          ( addr_target   b4 -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *a);
        a->ea.offset += offset;

        if (i->kind == INT_CONSTANT)
        {
          c_mov_mem_imm (a->ea, (int4)i->icte, size => 4);    // imm limited to 32-bit.
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 4, true, *i);   // allows reusing registers of node i

            c_mov_reg_mem (r, i->ea, /* size= */ 4);

            i->kind = INT_REGISTER;
            i->reg  = r;
            i->reg_high = NONE;
          }

          c_mov_mem_reg (a->ea, i->reg, /* size= */ 4);
        }

        a->ea.offset -= offset;
        istack_count--;
      }
      break;


      case P_STORE_FIELD_8:    // <offset4>          ( addr_target   b8 -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *a);
        a->ea.offset += offset;

        if (i->kind == INT_CONSTANT)
        {
          if (address_size == 4 || i->icte < -(int8)2147483648 || i->icte > +2147483647)
          {
            c_mov_mem_imm (a->ea, (int4)i->icte, size => 4);    // imm limited to 32-bit.
            a->ea.offset += 4;
            c_mov_mem_imm (a->ea, (int4)(i->icte >> 32), size => 4);    // imm limited to 32-bit.
            a->ea.offset -= 4;
          }
          else  // 64-bit with 32-bit signed-extended imm.
          {
            c_mov_mem_imm (a->ea, (int4)i->icte, size => 8);    // sign-extended 32-bit imm.
          }
        }
        else
        {
          if (address_size == 4)
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              REG r;

              r = allocate_register (size => 4, false, *i);   // don't reuse any registers (false)

              c_mov_reg_mem (r, i->ea, /* size= */ 4);
              c_mov_mem_reg (a->ea, r, /* size= */ 4);
              i->ea.offset += 4;
              a->ea.offset += 4;

              c_mov_reg_mem (r, i->ea, /* size= */ 4);
              c_mov_mem_reg (a->ea, r, /* size= */ 4);
              i->ea.offset -= 4;
              a->ea.offset -= 4;
            }
            else  // INT_REGISTER
            {
              c_mov_mem_reg (a->ea, i->reg, /* size= */ 4);
              a->ea.offset += 4;
              c_mov_mem_reg (a->ea, i->reg_high, /* size= */ 4);
              a->ea.offset -= 4;
            }
          }
          else    // 64-bit
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              REG r;

              r = allocate_register (size => 8, true, *i);   // allows reusing registers of node i

              c_mov_reg_mem (r, i->ea, /* size= */ 8);

              i->kind = INT_REGISTER;
              i->reg  = r;
              i->reg_high = NONE;
            }

            c_mov_mem_reg (a->ea, i->reg, /* size= */ 8);
          }
        }

        a->ea.offset -= offset;
        istack_count--;
      }
      break;


      case P_STORE_FIELD_FLT4:  // <offset4>          ( addr_target   f4 -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* f = &fstack[fstack_count - 1];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *a);
        a->ea.offset += offset;

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            float fcte = (float)f->fcte;
            c_mov_mem_imm (a->ea, *((int *)&fcte), size => 4);    // imm limited to 32-bit.
          }
          break;

          case FLOAT_REGISTER:
          {
            c_fstp (a->ea, 4);
          }
          break;

          case MEMORY:  // load memory operand into register
          {
            REG r;

            r = allocate_register (size => 4, true, *f);   // allows reusing registers of node f

            c_mov_reg_mem (r, f->ea, /* size= */ 4);
            c_mov_mem_reg (a->ea, r, /* size= */ 4);
          }
          break;

          default:
            abort;
        }

        a->ea.offset -= offset;
        fstack_count--;
      }
      break;


      case P_STORE_FIELD_FLT8:  // <offset4>          ( addr_target   b8 -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* f = &fstack[fstack_count - 1];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *a);
        a->ea.offset += offset;

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            flush_float_in_register (ref *f);
            c_fstp (a->ea, 8);
          }
          break;

          case FLOAT_REGISTER:
          {
            c_fstp (a->ea, 8);
          }
          break;

          case MEMORY:  // load memory operand into register
          {
            flush_float_in_register (ref *f);
            c_fstp (a->ea, 8);
          }
          break;

          default:
            abort;
        }

        a->ea.offset -= offset;
        fstack_count--;
      }
      break;


      // store value to memory address             ( addr_target   addr_src   -->  addr_target )

      case P_STORE_FIELD_ADDR:    // <offset4>
      {
        NODE* val = &astack[astack_count - 1];
        NODE* adr = &astack[astack_count - 2];
        int4 offset;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        flush_effective_address (ref *adr);
        adr->ea.offset += offset;

        if (val->kind == MEMORY)  // load memory operand into register
        {
          REG r;

          r = allocate_register (size => address_size, true, *val);   // allows reusing registers of node val

          c_mov_reg_mem (r, val->ea, /* size= */ address_size);

          val->kind = EFFECTIVE_ADDRESS;
          val->ea.base   = r;
          val->ea.index  = NONE;
          val->ea.scale  = 1;
          val->ea.offset = 0;
          val->ea.reloc.kind = RELOC_NONE;
          val->ea.reloc.nr   = 0;
        }

        if (val->ea.base == NONE && val->ea.index == NONE && val->ea.reloc.kind == RELOC_NONE)
        {
          c_mov_mem_imm (adr->ea, val->ea.offset, size => address_size);    // imm limited to 32-bit.
        }
        else if (val->ea.base == NONE && val->ea.index == NONE)
        {
          c_mov_mem_imm_reloc (adr->ea, val->ea.offset, address_size, val->ea.reloc);
        }
        else if (val->ea.base != NONE && val->ea.index == NONE &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_mov_mem_reg (adr->ea, val->ea.base, /* size= */ address_size);
        }
        else if (val->ea.base == NONE && val->ea.index != NONE && val->ea.scale == 1 &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_mov_mem_reg (adr->ea, val->ea.index, /* size= */ address_size);
        }
        else   // general case
        {
          REG r;
          r = allocate_register (address_size, true, *val);   // we can reuse val's registers
          c_lea_reg_mem (r, val->ea, size => address_size);
          c_mov_mem_reg (adr->ea, r, /* size= */ address_size);
        }

        adr->ea.offset -= offset;
        astack_count--;
      }
      break;


      case P_STORE_FIELD_BLOCK:   // <offset4>  <size4>        ( addr_target   addr_src   -->  addr_target )
      {
        NODE* src = &astack[astack_count - 1];
        NODE* dst = &astack[astack_count - 2];
        int4 field_offset, size;

        field_offset = *((int4 *)&mem[mem_offset]);
        size         = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" field_offset = %d, size = %d\n", field_offset, size);

        if (nb_register_moves_to_copy_size (size) <= 4)      // max 4 moves
        {
          REG[1] r_dummy;

          flush_effective_address (ref *src);
          flush_effective_address (ref *dst);

          if ((size & 1) == 1 && try_allocate_registers (size => 1, false, *dst, 1, out r_dummy) < 0)
          {
            // could not allocate byte-aligned register : free one, then max 3 registers are in use and 1 byte register is free

            REG r = allocate_register (size => address_size, true, *dst);

            _unused r_dummy;

            c_lea_reg_mem (r, dst->ea, size => address_size);

            dst->ea.base   = r;
            dst->ea.index  = NONE;
            dst->ea.scale  = 1;
            dst->ea.offset = 0;
            dst->ea.reloc.kind = RELOC_NONE;
            dst->ea.reloc.nr   = 0;
          }

          // now allocation of any size should work !


          dst->ea.offset += field_offset;


          if (address_size == 4)
          {
            REG r = allocate_register ((size & 1) == 1 ? 1 : address_size, false, *dst);  // we need a byte-aligned register

            int siz = size;    // size to copy
            int chunk = address_size;

            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_reg_mem (r, src->ea, /* size= */ chunk);
                c_mov_mem_reg (dst->ea, r, /* size= */ chunk);
                src->ea.offset += chunk;
                dst->ea.offset += chunk;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }
          }
          else   // 64 bit
          {
            REG regs[4];
            int chunk, siz, ri;

            allocate_registers (size => address_size, false, *dst, 4, out regs);  // 4 new regs

            chunk = address_size;
            siz = size;
            ri = 0;

            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_reg_mem (regs[ri], src->ea, /* size= */ chunk);
                src->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }

            chunk = address_size;
            siz = size;
            ri = 0;

            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_mem_reg (dst->ea, regs[ri], /* size= */ chunk);
                dst->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }
          }

          src->ea.offset -= size;
          dst->ea.offset -= size;

          dst->ea.offset -= field_offset;
        }
        else   // larger block to copy
        {
          store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
          store_all_float_registers_in_temporaries ();

          if (address_size == 4)
          {
            c_push_imm (size);
            push_addr (*src);

            if (*(PCODE*)&mem[mem_offset] != P_DROP_ADDR)    // we need to keep dst for next pcode
            {
              load_addr_into_reg (ref *dst, RDI);      // dst in RDI (will be kept for next pcode)

              {
                EA ea;
                clear ea;
                ea.base = RDI;  ea.index = NONE; ea.scale = 1;  ea.offset = field_offset; ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
                c_lea_reg_mem (RCX, ea, address_size);      // RCX = RDI + field_offset;
              }
            }
            else   // next pcode does not need dst
            {
              if (dst->kind == EFFECTIVE_ADDRESS)
              {
                dst->ea.offset += field_offset;
                load_addr_into_reg (ref *dst, RCX);      // dst + offset in RCX
              }
              else   // MEMORY
              {
                load_addr_into_reg (ref *dst, RCX);      // dst in RCX
                c_add_reg_imm (RCX, field_offset, address_size);       // offset of field
              }
            }

            c_push_reg (RCX, address_size);

            call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
          }
          else   // 64-bit
          {
            align_stack ();

            c_mov_reg_imm (R8, size, 4);      // size
            load_addr_into_reg (ref *src, RDX);    // source

            if (*(PCODE*)&mem[mem_offset] != P_DROP_ADDR)    // we need to keep dst for next pcode
            {
              load_addr_into_reg (ref *dst, RDI);      // dst in RDI (will be kept for next pcode)

              {
                EA ea;
                clear ea;
                ea.base = RDI;  ea.index = NONE; ea.scale = 1;  ea.offset = field_offset; ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
                c_lea_reg_mem (RCX, ea, address_size);      // RCX = RDI + field_offset;
              }
            }
            else   // next pcode does not need dst
            {
              if (dst->kind == EFFECTIVE_ADDRESS)
              {
                dst->ea.offset += field_offset;
                load_addr_into_reg (ref *dst, RCX);      // dst + offset in RCX
              }
              else   // MEMORY
              {
                load_addr_into_reg (ref *dst, RCX);      // dst in RCX
                c_add_reg_imm (RCX, field_offset, address_size);    // offset of field
              }
            }

            alloc_shadow_space ();
            call_kernel ("RtlMoveMemory", /*nb_arguments=*/ 3);
            free_shadow_space_and_dealign_stack ();
          }
        }

        astack_count--;    // discard source address, keep target address
      }
      break;


  // --------
  // 11. heap
  // --------

      //  (  size_uint4 --> heap_addr )

      case P_MALLOC:    //  <bool_fill_zeroes>   ;  allocate heap block ;  causes error if out of memory
      {
        NODE* i = &istack[istack_count - 1];
        bool fill_zeroes = *((bool *)&mem[mem_offset++]);

        if (g_tracing)
          trace ("\n");

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        if (address_size == 4)
        {
          push_int4 (*i);

          // push flags
          if (fill_zeroes)
            c_push_imm (4 + 8);    // 4=generate exceptions if out of heap, 8=fill with zeroes
          else
            c_push_imm (4);

          // push process heap handle that was stored at global address zero.
          {
            EA ea;
            clear ea;
            ea.base = NONE;  ea.index = NONE;   ea.scale = 1;  ea.offset = 0;
            ea.reloc.kind = RELOC_GLOBAL;   ea.reloc.nr = 0;
            c_push_mem (ea, address_size);
          }

          call_kernel ("HeapAlloc", /*nb_arguments=*/ 3);
        }
        else
        {
          int flags;

          align_stack ();

          // arg 3 : size
          load_int4_into_reg (ref *i, R8);

          // arg 2 : push flags
          if (fill_zeroes)
            flags = (4 + 8);    // 4=generate exceptions if out of heap, 8=fill with zeroes
          else
            flags = (4);
          c_mov_reg_imm (RDX, flags, 4);

          // arg 1 : process heap handle that was stored at global address zero.
          {
            EA ea;
            clear ea;
            ea.base = NONE;  ea.index = NONE;   ea.scale = 1;  ea.offset = 0;
            ea.reloc.kind = RELOC_GLOBAL;   ea.reloc.nr = 0;
            c_mov_reg_mem (RCX, ea, 8);
          }

          alloc_shadow_space ();
          call_kernel ("HeapAlloc", /*nb_arguments=*/ 3);
          free_shadow_space_and_dealign_stack ();
        }

        istack_count--;

        {
          NODE* a;
          a = &astack[astack_count++];
          a->typ = 'a';
          a->kind = EFFECTIVE_ADDRESS;
          a->ea.base = RAX;
          a->ea.index = NONE;
          a->ea.scale = 1;
          a->ea.offset = 0;
          a->ea.reloc.kind = RELOC_NONE;
          a->ea.reloc.nr = 0;
        }
      }
      break;


      //  (  heap_addr  -->  /   )

      case P_FREE:    //  ; free heap block  ; causes error if bad ptr
      {
        NODE* a = &astack[astack_count - 1];
        bool never_null =  *((bool *)&mem[mem_offset++]);

        if (g_tracing)
          trace (" never_null = %u\n", never_null);

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        if (never_null)
        {
          if (address_size == 4)
          {
            // push addr
            push_addr (*a);

            // push flags
            c_push_imm (0);

            // push process heap handle that was stored at global address zero.
            {
              EA ea;
              clear ea;
              ea.base = NONE;  ea.index = NONE;   ea.scale = 1;  ea.offset = 0;
              ea.reloc.kind = RELOC_GLOBAL;   ea.reloc.nr = 0;
              c_push_mem (ea, address_size);
            }

            call_kernel ("HeapFree", /*nb_arguments=*/ 3);
          }
          else
          {
            align_stack ();

            // arg 3 : addr
            load_addr_into_reg (ref *a, R8);

            // arg 2 : push flags
            c_mov_reg_imm (RDX, 0, 4);

            // arg 1 : process heap handle that was stored at global address zero.
            {
              EA ea;
              clear ea;
              ea.base = NONE;  ea.index = NONE;   ea.scale = 1;  ea.offset = 0;
              ea.reloc.kind = RELOC_GLOBAL;   ea.reloc.nr = 0;
              c_mov_reg_mem (RCX, ea, 8);
            }

            alloc_shadow_space ();
            call_kernel ("HeapFree", /*nb_arguments=*/ 3);
            free_shadow_space_and_dealign_stack ();
          }
        }
        else
        {
          align_stack ();

          if (address_size == 4)
            push_addr (*a);
          else
            load_addr_into_reg (ref *a, RAX);        // pass in RAX for 64 bit

          if (address_size == 4)
            c_call_relative (label_free_possible_null, /*nb_arguments=*/ 1);
          else
            c_call_relative (label_free_possible_null, /*nb_arguments=*/ 0);

          dealign_stack ();
        }

        astack_count--;
      }
      break;


      // (  heap_addr -->  pointer_addr )
      // requires a previous call to P_MALLOC
      // allocate tombstone block,
      // fill its <type4> & <heap_addr>, return its <pointer_addr>

      case P_ALLOC_TOMB:  // <type4> ;  allocate tombstone,  causes error if out of memory
      {
        NODE* a = &astack[astack_count - 1];
        int4 typ;

        typ = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" typ = %d\n", typ);

        store_all_registers_in_temporaries_except_for_this_pcode ();

        align_stack ();

        if (address_size == 4)
        {
          push_addr (*a);
        }
        else
        {
          load_addr_into_reg (ref *a, RAX);   // RAX because previous P_MALLOC call left its result there
        }

        if (address_size == 4)
          c_call_relative (label_allocate_tombstone, /*nb_arguments=*/ 1);
        else
          c_call_relative (label_allocate_tombstone, /*nb_arguments=*/ 0);

        dealign_stack ();

        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base = RAX;  a->ea.index = NONE;  a->ea.scale = 1;  a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;  a->ea.reloc.nr = 0;

        a->ea.offset += 4;
        c_mov_mem_imm (a->ea, typ, 4);  // assign type to entry (makes it valid)
        a->ea.offset -= 4;
      }
      break;


      //  ( pointer_addr -->  )

      case P_FREE_TOMB:   // <type4> ; free tombstone + heap object ; causes error if bad ptr
      {                   //         ; null address value has no effect.
        NODE* a = &astack[astack_count - 1];
        int4 typ;

        typ = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("\n");

        store_all_registers_in_temporaries_except_for_this_pcode ();

        align_stack ();

        if (address_size == 4)
        {
          push_addr (*a);      // push tomb ptr
          c_push_imm (typ);   // push typ
        }
        else
        {
          load_addr_into_reg (ref *a, RAX);
          c_mov_reg_imm (RBX, typ, 4);
        }

        if (address_size == 4)
          c_call_relative (label_free_tombstone, /*nb_arguments=*/ 2);
        else
          c_call_relative (label_free_tombstone, /*nb_arguments=*/ 0);

        dealign_stack ();

        astack_count--;
      }
      break;


  // --------------
  // 12. statements
  // --------------

      //  ( --> )

      case P_INIT_THREADS:
        mem_offset += 4;  // skip stack size
        break;


      //  (  code_addr  -->   int4  )   stack frames of threads need to be 16-byte aligned.

      case P_RUN_VOID_PARAM:     // <near_label>
      {
        NODE* a = &astack[astack_count - 1];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        if (address_size == 4)
        {
          c_push_imm (0);   // LPDWORD lpThreadId
          c_push_imm (0);   // DWORD dwCreationFlags
          c_push_imm (0);   // LPVOID lpParameter
          push_addr (*a);    // LPTHREAD_START_ROUTINE lpStartAddress
          c_push_imm (0);   // SIZE_T dwStackSize
          c_push_imm (0);   // LPSECURITY_ATTRIBUTES lpThreadAttributes

          call_kernel ("CreateThread", /*nb_arguments=*/ 6);
        }
        else
        {
          align_stack ();

          c_push_imm (0);   // arg 6 : LPDWORD lpThreadId
          c_push_imm (0);   // arg 5 : DWORD dwCreationFlags

          load_addr_into_reg (ref *a, R8);  // arg 3 : LPTHREAD_START_ROUTINE lpStartAddress

          c_mov_reg_imm (R9, 0, 4);    // arg 4 : LPVOID lpParameter
          c_mov_reg_imm (RDX, 0, 4);   // arg 2 : SIZE_T dwStackSize
          c_mov_reg_imm (RCX, 0, 4);   // arg 1 : LPSECURITY_ATTRIBUTES lpThreadAttributes

          alloc_shadow_space ();
          call_kernel ("CreateThread", /*nb_arguments=*/ 6);
          g_shadow_space += 8*2;   // cancel the 2 push above
          free_shadow_space_and_dealign_stack ();
        }

        c_test_reg_reg (RAX, RAX, address_size);    // CreateThread() returns 4- or 8-bit handle
        c_jcond (CMP_EQUAL, false, near_label_nr);  // returns 0 if fails ---> convert into -1

        // close thread handle

        if (address_size == 4)
        {
          c_push_reg (RAX, address_size);   // arg 1
          call_kernel ("CloseHandle", /*nb_arguments=*/ 1);
        }
        else
        {
          align_stack ();

          c_mov_reg_reg (RCX, RAX, address_size);   // arg 1

          alloc_shadow_space ();
          call_kernel ("CloseHandle", /*nb_arguments=*/ 1);
          free_shadow_space_and_dealign_stack ();
        }

        c_mov_reg_imm (RAX, -1, 4);  // this will be converted into 0 by "not" below

        exe_declare_near_label (near_label_nr);
        c_not_reg (RAX, 4);          // not eax (converts 0 into -1, or -1 into 0)

        astack_count--;

        {
          NODE* i;
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = RAX;
          i->reg_high = NONE;
        }
      }
      break;


      //  (  code_addr  param_int4  -->   int4  )

      case P_RUN_INT4_PARAM:     // <near_label>
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" near_label_nr = #%d\n", near_label_nr);

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        if (address_size == 4)
        {
          c_push_imm (0);   // LPDWORD lpThreadId
          c_push_imm (0);   // DWORD dwCreationFlags
          push_int4 (*i);    // LPVOID lpParameter
          push_addr (*a);    // LPTHREAD_START_ROUTINE lpStartAddress
          c_push_imm (0);   // SIZE_T dwStackSize
          c_push_imm (0);   // LPSECURITY_ATTRIBUTES lpThreadAttributes

          call_kernel ("CreateThread", /*nb_arguments=*/ 6);
        }
        else
        {
          align_stack ();

          c_push_imm (0);   // arg 6 : LPDWORD lpThreadId
          c_push_imm (0);   // arg 5 : DWORD dwCreationFlags

          load_int4_into_reg (ref *i, R9);  // arg 4 : LPVOID lpParameter
          load_addr_into_reg (ref *a, R8);  // arg 3 : LPTHREAD_START_ROUTINE lpStartAddress
          c_mov_reg_imm (RDX, 0, 4);   // arg 2 : SIZE_T dwStackSize
          c_mov_reg_imm (RCX, 0, 4);   // arg 1 : LPSECURITY_ATTRIBUTES lpThreadAttributes

          alloc_shadow_space ();
          call_kernel ("CreateThread", /*nb_arguments=*/ 6);
          g_shadow_space += 8*2;   // cancel the 2 push above
          free_shadow_space_and_dealign_stack ();
        }

        astack_count--;
        istack_count--;

        c_test_reg_reg (RAX, RAX, address_size);   // CreateThread() returns 4- or 8-bit handle
        c_jcond (CMP_EQUAL, false, near_label_nr);   // returns 0 if fails ---> convert into -1

        // close thread handle

        if (address_size == 4)
        {
          c_push_reg (RAX, address_size);   // arg 1
          call_kernel ("CloseHandle", /*nb_arguments=*/ 1);
        }
        else
        {
          align_stack ();

          c_mov_reg_reg (RCX, RAX, address_size);   // arg 1

          alloc_shadow_space ();
          call_kernel ("CloseHandle", /*nb_arguments=*/ 1);
          free_shadow_space_and_dealign_stack ();
        }

        c_mov_reg_imm (RAX, -1, 4);  // this will be converted into 0 by "not" below

        exe_declare_near_label (near_label_nr);
        c_not_reg (RAX, 4);          // not eax (converts 0 into -1, or -1 into 0)

        i = &istack[istack_count++];
        i->typ = 'i';
        i->kind = INT_REGISTER;
        i->reg = RAX;
        i->reg_high = NONE;
      }
      break;


      //  (  code_addr  param_addr -->   int4  )

      case P_RUN_ADDR_PARAM:     // <near_label>
      {
        NODE* param = &astack[astack_count - 1];
        NODE* start = &astack[astack_count - 2];
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" near_label_nr = #%d\n", near_label_nr);

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        if (address_size == 4)
        {
          c_push_imm (0);   // LPDWORD lpThreadId
          c_push_imm (0);   // DWORD dwCreationFlags
          push_addr (*param); // LPVOID lpParameter
          push_addr (*start); // LPTHREAD_START_ROUTINE lpStartAddress
          c_push_imm (0);   // SIZE_T dwStackSize
          c_push_imm (0);   // LPSECURITY_ATTRIBUTES lpThreadAttributes

          call_kernel ("CreateThread", /*nb_arguments=*/ 6);
        }
        else
        {
          align_stack ();

          c_push_imm (0);   // LPDWORD lpThreadId
          c_push_imm (0);   // DWORD dwCreationFlags

          load_addr_into_reg (ref *param, R9); // arg 4 : LPVOID lpParameter
          load_addr_into_reg (ref *start, R8); // arg 3 : LPTHREAD_START_ROUTINE lpStartAddress
          c_mov_reg_imm (RDX, 0, 4);      // arg 2 : SIZE_T dwStackSize
          c_mov_reg_imm (RCX, 0, 4);      // arg 1 : LPSECURITY_ATTRIBUTES lpThreadAttributes

          alloc_shadow_space ();
          call_kernel ("CreateThread", /*nb_arguments=*/ 6);
          g_shadow_space += 8*2;
          free_shadow_space_and_dealign_stack ();
        }

        astack_count -= 2;

        c_test_reg_reg (RAX, RAX, address_size);     // CreateThread() returns 4- or 8-bit handle
        c_jcond (CMP_EQUAL, false, near_label_nr);   // returns 0 if fails ---> convert into -1

        // close thread handle

        if (address_size == 4)
        {
          c_push_reg (RAX, address_size);
          call_kernel ("CloseHandle", /*nb_arguments=*/ 1);
        }
        else
        {
          align_stack ();

          c_mov_reg_reg (RCX, RAX, address_size);    // arg 1

          alloc_shadow_space ();
          call_kernel ("CloseHandle", /*nb_arguments=*/ 1);
          free_shadow_space_and_dealign_stack ();
        }

        c_mov_reg_imm (RAX, -1, 4);  // this will be converted into 0 by "not" below

        exe_declare_near_label (near_label_nr);
        c_not_reg (RAX, 4);          // not eax (converts 0 into -1, or -1 into 0)

        {
          NODE* i;
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = RAX;
          i->reg_high = NONE;
        }
      }
      break;


      //  (  addr   size4    --->   /   )

      case P_CLEAR:
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        if (i->kind == INT_CONSTANT &&
            nb_register_moves_to_copy_size ((int)i->icte) <= 4)   // max 4 moves
        {
          REG r;
          int siz, chunk;

          flush_effective_address (ref *a);

          siz = (int)i->icte;    // size to clear

          if (siz == 1 || siz == 2 || siz == 4 || siz == address_size)
          {
            c_mov_mem_imm (a->ea, 0, siz);
          }
          else
          {
            if ((siz & 1) == 1)
              r = allocate_register (size => 1, false, *i);     // we need a byte-aligned register
            else
              r = allocate_register (size => address_size, false, *i);

            c_mov_reg_imm (r, 0, 4);

            chunk = address_size;

            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_mov_mem_reg (a->ea, r, /* size= */ chunk);
                a->ea.offset += chunk;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }
          }
        }
        else
        {
          // all registers must be flushed to temporaries before calling Windows API
          store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
          store_all_float_registers_in_temporaries ();

          if (address_size == 4)
          {
            push_int4 (*i);
            push_addr (*a);
            call_kernel ("RtlZeroMemory", /*nb_arguments=*/ 2);
          }
          else
          {
            align_stack ();

            load_addr_into_reg (ref *a, RCX);   // arg 1
            load_int4_into_reg (ref *i, RDX);   // arg 2

            alloc_shadow_space ();
            call_kernel ("RtlZeroMemory", /*nb_arguments=*/ 2);
            free_shadow_space_and_dealign_stack ();
          }
        }

        istack_count--;
        astack_count--;
      }
      break;


      //  (  / --->   /   )
      case P_ABORT:
      {
        if (g_tracing)
          trace ("\n");

        c_int (5);
      }
      break;


      //  (  / --->   /   )
      case P_ASSERT:   //  <label_nr>   ; define a label below this function for a failed assertion.
      {
        int4 near_label_nr;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" near_label_nr = #%d\n", near_label_nr);

        store_ll_forced (g_current_source_line, near_label_nr);
      }
      break;


      //  (    --->     )

      case P_SLEEP_CTE:   // <uint4>   ; sleep <uint4> milliseconds
      {
        int4 cte = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" %d msecs\n", cte);

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        if (address_size == 4)
        {
          c_push_imm (cte);
          call_kernel ("Sleep", /*nb_arguments=*/ 1);
        }
        else
        {
          align_stack ();

          c_mov_reg_imm (RCX, cte, 4);   // arg 1 : uint4 will be zero-extended

          alloc_shadow_space ();
          call_kernel ("Sleep", /*nb_arguments=*/ 1);
          free_shadow_space_and_dealign_stack ();
        }
      }
      break;


      //  (  int4  --->   /   )

      case P_SLEEP_INT4:   // <label_nr>    ; sleep <int4> seconds (negative value mean no sleep)
      {
        NODE* i = &istack[istack_count - 1];
        int4 near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("\n");

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();
        store_all_float_registers_in_temporaries ();

        flush_int4_in_register_for_modif (ref *i);

        c_test_reg_reg (i->reg, i->reg, 4);
        c_jcond_raw (/*js=*/ 8, near_label_nr);

        // multiply by 1000
        c_imul_reg_imm (i->reg, 1000, size => 4);

        if (address_size == 4)
        {
          push_int4 (*i);
          call_kernel ("Sleep", /*nb_arguments=*/ 1);
        }
        else
        {
          align_stack ();

          load_int4_into_reg (ref *i, RCX);  // int4 (garanteed positive) will be zero-extended

          alloc_shadow_space ();
          call_kernel ("Sleep", /*nb_arguments=*/ 1);
          free_shadow_space_and_dealign_stack ();
        }

        exe_declare_near_label (near_label_nr);

        istack_count--;
      }
      break;


      //  (  float4  --->   /   )

      case P_SLEEP_FLT4:   // <label_nr>    ; sleep <float4> seconds (negative value mean no sleep)
      {
        NODE* f = &fstack[fstack_count - 1];
        int4 near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("\n");

        store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();


        flush_float_in_register (ref *f);

        // multiply by 1000
        {
          POOL p = new_pool_constant (4, 4);
          EA   ea;

          store_float (p, 0, 1000.0, 4);

          clear ea;
          ea.base   = NONE;
          ea.index  = NONE;
          ea.scale  = 1;
          ea.offset = 0;
          ea.reloc.kind = RELOC_POOL;
          ea.reloc.nr   = serial_nr_of_pool_cte (p);

          c_fld (ea, 4);
          c_fmulp ();
        }

        // convert float to int4
        {
          NODE* i;
          int4 ofs;

          ofs = allocate_temporary_zone (size => 4   // must occur after allocating float register !
                                            + 2); // for control word

          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = MEMORY;
          i->ea.base       = RSP;
          i->ea.index      = NONE;
          i->ea.scale      = 1;
          i->ea.offset     = ofs;
          i->ea.reloc.kind = RELOC_NONE;
          i->ea.reloc.nr   = 0;

          save_8087_control_word (ofs+4);
          change_8087_mode_to_trunc ();
          c_fistp (i->ea, size => 4);  // size is 2(int2), 4(int4) or 8(int8).
          restore_8087_control_word (ofs+4);

          flush_int4_in_register (ref *i);

          c_test_reg_reg (i->reg, i->reg, 4);
          c_jcond_raw (/*js=*/ 8, near_label_nr);

          if (address_size == 4)
          {
            push_int4 (*i);
            call_kernel ("Sleep", /*nb_arguments=*/ 1);
          }
          else
          {
            align_stack ();

            load_int4_into_reg (ref *i, RCX);  // int4 (garanteed positive) will be zero-extended

            alloc_shadow_space ();
            call_kernel ("Sleep", /*nb_arguments=*/ 1);
            free_shadow_space_and_dealign_stack ();
          }
        }

        exe_declare_near_label (near_label_nr);

        istack_count--;
        fstack_count--;
      }
      break;


      //  (  /  --->   /   )

      case P_CODE:   // <byte>  ; machine code
      {
        byte b;
        b = mem[mem_offset++];

        if (g_tracing)
          trace (" byte %u\n", b);

        c_code (b);
      }
      break;


      //  (  /  --->   /   )

      case P_LOCATION:   // <unit4>  <line4>    ; unit unique key and source line
      {
        int4 key, line;

        key  = *((int4 *)&mem[mem_offset]);
        line = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" line %d\n", line);

        if (g_current_unit_key != key)
        {
          dbg_new_unit (key);
          error_set_code_generator_unit_key (key);

          if (g_current_unit_key == 0)  // very first unit
            dbg_store_line (line, (int)IP_START);  // first line of main program is valid during initialization

          g_current_unit_key = key;
          g_current_source_line = 0;
        }

        if (g_current_source_line != line)
        {
          dbg_store_line (line, (int)current_RIP());
          error_set_code_generator_source_line (line);
          g_current_source_line = line;
        }
      }
      break;

      default:
        fatal_compiler_error0 ("generate_asm_for_function (unsupported pcode)");
        break;
    }

    if (g_after_i != istack_count || g_after_f != fstack_count || g_after_a != astack_count)
      fatal_compiler_error0 ("generate_asm_for_function (bad xx_stack indexes)");
  }

  if (istack_count != 0 || fstack_count != 0 || astack_count != 0)
    fatal_compiler_error0 ("generate_asm_for_function (stacks_counters_not_zero)");

  c_verify_ESP_correction ();  // check that ESP correction is zero.
  c_ESP_correction_ON (false);


  // add extra code for reserving stack space and probing each 4K page

  // when the compiler detects that a function is allocating more
  // than 4K of local data, it generates code that ”touches” the allocated
  // data sequentially, from high to low, 4K at a time. Whenever the guard
  // page is touched, a new page is committed 4K below it and the newly
  // committed page becomes the guard page; the compiler stack probe
  // routine prevents a stack fault from occurring this way.

  if (g_extra_frame_bytes_rip != 0 && g_frame_size > 0)
  {
    int4 gap, extra, ad, backfill;

    if (address_size == 4)
      gap = 4096 - 8;
    else
      gap = 4096 - 64;   // 32 byte shadow space + 8 byte alignment, rounded up to 64 bytes

    extra = 6 * ((int)g_frame_size / gap)            // test [ebp]ofs,eax
          + (((int)address_size == 8) ? 1 : 0) + 6;  // sub  rsp,N

    exe_insert_code_sequence (g_extra_frame_bytes_rip, (uint)extra);

    backfill = (int)g_extra_frame_bytes_rip;

    for (ad=gap; ad<=(int)g_frame_size; ad+=gap)
    {
      byte seq[6];

      clear seq;
      seq[0] = 0x85;     // test ofs[esp],eax ; probe 4 bytes
      seq[1] = 0x85;
      seq[2] = (byte)(-ad);
      seq[3] = (byte)((-ad) >> 8);
      seq[4] = (byte)((-ad) >> 16);
      seq[5] = (byte)((-ad) >> 24);

      exe_update_code_sequence ((uint)backfill, seq);
      backfill += seq'length;
    }

    {
      byte seq[7];
      int  ofs = 0;

      clear seq;

      if (address_size == 8)
        seq[ofs++] = 0x48;

      seq[ofs++] = 0x81;    // sub rsp,N
      seq[ofs++] = 0xEC;
      seq[ofs++] = (byte)(g_frame_size);
      seq[ofs++] = (byte)(g_frame_size >> 8);
      seq[ofs++] = (byte)(g_frame_size >> 16);
      seq[ofs++] = (byte)(g_frame_size >> 24);

      exe_update_code_sequence ((uint)backfill, seq[0:ofs]);
      backfill += ofs;
    }
  }


  // generate list of interrupt labels for catching all check errors

  traverse_ll_tree (ll_operate);
  dbg_store_line (0, (int)current_RIP());      // add closing node to complete interval


  close_ll_tree ();


  // possibly expand code by converting label offset fields

  pe.relocate_all_near_labels ();
}

#end unsafe

/***********************************************************************************/
