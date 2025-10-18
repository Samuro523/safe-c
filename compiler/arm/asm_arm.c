
// asm_arm.c

from std use tracing;
use ../pcodes, ../codout, ../error, ../fixup, ../goptions, ../dbginfo;
use ../blob, ../blob2, ../common, ../pool;
use arm64, aslab, astacks, initarm;

// ----------------------------------------------------------------------------------------------

int   g_extra_frame_bytes_pos;          // code for inserting stack space and probe stack for 4K page will be inserted at this position in blob
bool  g_is_thread_entry_point, g_is_callback, g_is_entry;
int   g_current_unit_key;
int   g_current_source_line;
int   g_after_i, g_after_f, g_after_a;

int   g_callee_saved_regs;
int   g_enter_pcode_inserted_bytes;

// ----------------------------------------------------------------------------------------------

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

  fixup.near_label_declare (label_nr      => p.label_nr,
                            code_position => blob_index (g_blob_code));

  // SMC Secure Monitor Call
  c_smc (0); // 0 to 65535

  return 0;
}

// ----------------------------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------------------------

void validate_xx_stacks_before_pcode ()
{
  validate_xx_stacks_for_pcode (g_current_pcode);
}

// ----------------------------------------------------------------------------------------------

void validate_xx_stacks_for_additional_pcode (PCODE c)
{
  if (g_after_i != istack_count || g_after_f != fstack_count || g_after_a != astack_count)
    fatal_compiler_error0 ("generate_asm_for_ipc (bad xx_stack indexes)");

  g_current_pcode = c;

  validate_xx_stacks_for_pcode (c);
}

// ----------------------------------------------------------------------------------------------

int nb_register_moves_to_copy_size (int size)
{
  int count = 0;
  int rest = size;

  count += (rest >> 3);
  rest &= 7;

  count += (rest >> 2);
  rest &= 3;

  count += (rest >> 1);
  rest &= 1;

  count += rest;

  return count;
}

// ----------------------------------------------------------------------------------------------

// to be called from bottom to top addresses, to avoid that one changes updates the others.

void insert_leave_code (int blob_pos)
{
  int  idx, bytes_to_insert;
  uint extra;

  extra = g_extra_frame_size + g_max_extra_param_call_size;

  idx = blob_index (g_blob_code);

  bytes_to_insert = 0;
  if (extra != 0)
    bytes_to_insert += 4;
  if (g_first_page_size <= 504)
    bytes_to_insert += 4;
  else
    bytes_to_insert += 8;

  if (g_callee_saved_regs != 0)    // we need to restore callee-registers
  {
    int offset = g_callee_saved_regs + (int)g_first_page_size;

    if (offset <= 504)
      bytes_to_insert += 4;
    else
      bytes_to_insert += 8;

    offset += 16;

    if (offset <= 504)
      bytes_to_insert += 4;
    else
      bytes_to_insert += 8;
  }

  fixup.insert_bytes_in_code (pos => blob_pos+g_enter_pcode_inserted_bytes, size_increase => bytes_to_insert, extend_instruction => true);

  blob_set_index (ref g_blob_code, index => blob_pos+g_enter_pcode_inserted_bytes);

  if (g_callee_saved_regs != 0)   // save 4 callee-registers X19-X22
  {
    int offset = g_callee_saved_regs + (int)g_first_page_size;

    if (offset <= 504)
    {
      assert c_load_pair (target1     => X19,          // ZERO allowed
                          target2     => X20,          // ZERO allowed
                          base        => FP,           // SP allowed (effective address)
                          offset      => offset,       // -512 to +504 to be added to base address
                          ldp_mode    => SIMPLE_LOAD, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                          data_signed => false,
                          data_size   => 8);          // 4 or 8
    }
    else
    {
      assert c_load_ofs12 (target      => X19,      // ZERO allowed
                           base        => FP,       // SP allowed (effective address)
                           offset      => offset,   // 12 bits (0 to 4095 * size) to be added to base address
                           data_signed => false,
                           data_size   => 8);       // 1, 2, 4 or 8

      assert c_load_ofs12 (target      => X20,      // ZERO allowed
                           base        => FP,       // SP allowed (effective address)
                           offset      => offset+8, // 12 bits (0 to 4095 * size) to be added to base address
                           data_signed => false,
                           data_size   => 8);       // 1, 2, 4 or 8
    }

    offset += 16;

    if (offset <= 504)
    {
      assert c_load_pair (target1     => X21,          // ZERO allowed
                          target2     => X22,          // ZERO allowed
                          base        => FP,           // SP allowed (effective address)
                          offset      => offset,      // -512 to +504 to be added to base address
                          ldp_mode    => SIMPLE_LOAD, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                          data_signed => false,
                          data_size   => 8);          // 4 or 8
    }
    else
    {
      assert c_load_ofs12 (target      => X21,      // ZERO allowed
                           base        => FP,       // SP allowed (effective address)
                           offset      => offset,   // 12 bits (0 to 4095 * size) to be added to base address
                           data_signed => false,
                           data_size   => 8);       // 1, 2, 4 or 8

      assert c_load_ofs12 (target      => X22,      // ZERO allowed
                           base        => FP,       // SP allowed (effective address)
                           offset      => offset+8, // 12 bits (0 to 4095 * size) to be added to base address
                           data_signed => false,
                           data_size   => 8);       // 1, 2, 4 or 8
    }
  }


  if (extra != 0)
  {
    // mov sp,fp
    c_add_reg_imm (target     => SP,    // SP allowed (ZERO not allowed)
                   source     => FP,    // SP allowed (ZERO not allowed)
                   imm12      => 0,     // 0 to 4095
                   shl_imm_12 => false, // true to shift imm12 << 12
                   size       => 8);    // 4 or 8
  }

  if (g_first_page_size > 504)
  {
    assert g_first_page_size <= 4096;

    // stp FP,X30,[SP]
    assert c_load_pair (target1   => FP,      // ZERO allowed
                        target2   => X30,     // ZERO allowed
                        base      => SP,      // SP allowed (effective address)
                        offset    => 0,    // -512 to +504 to be added to base address
                        ldp_mode  => SIMPLE_LOAD, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                        data_signed => false,
                        data_size => 8);      // 4 or 8

    // add sp,#g_first_page_size
    if (g_first_page_size == 4096)
    {
      c_add_reg_imm (target     => SP,
                     source     => SP,
                     imm12      => 1,      // 0 to 4095
                     shl_imm_12 => true,   // true to shift imm12 << 12
                     size       => 8);     // 4 or 8
    }
    else
    {
      c_add_reg_imm (target     => SP,
                     source     => SP,
                     imm12      => (int)g_first_page_size,      // 0 to 4095
                     shl_imm_12 => false,  // true to shift imm12 << 12
                     size       => 8);     // 4 or 8
    }
  }
  else
  {
    // stp FP,X30,[SP],#-g_first_page_size
    assert c_load_pair (target1   => FP,                  // ZERO allowed
                        target2   => X30,                 // ZERO allowed
                        base      => SP,                  // SP allowed (effective address)
                        offset    => (int)g_first_page_size,  // -512 to +504 to be added to base address
                        ldp_mode  => POST_ADD,            // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                        data_signed => false,
                        data_size => 8);                  // 4 or 8
  }

  blob_set_index (ref g_blob_code, index => idx + (int)bytes_to_insert);
}

// ----------------------------------------------------------------------------------------------
#begin unsafe

// make effective address that consists atmost of a base and a small offset,
// so no index, relocation or large offset requiring multiple instructions.

void make_simple_effective_address (ref NODE a)
{
  flush_effective_address (ref a);    // MEMORY -> EFFECTIVE_ADDRESS

  if (a.ea.reloc.kind != RELOC_NONE || a.ea.index != ZERO ||
      a.ea.offset < -256 || a.ea.offset > 4095)   // complex ea, not a single register or no registers
  {
    REG r = allocate_register (a);

    compute_effective_address_in_register (a.ea, r);

    a.kind = EFFECTIVE_ADDRESS;
    a.ea.base = r;
    a.ea.index = ZERO;
    a.ea.scale = 1;
    a.ea.offset = 0;
    a.ea.reloc.kind = RELOC_NONE;
    a.ea.reloc.nr = 0;
  }
}

// ----------------------------------------------------------------------------------------------

void make_effective_address_in_base (ref NODE a)
{
  REG r;

  flush_effective_address (ref a);    // MEMORY -> EFFECTIVE_ADDRESS

  if (a.kind == EFFECTIVE_ADDRESS &&
      a.ea.base < X15 &&
      a.ea.index == ZERO &&
      a.ea.offset == 0 &&
      a.ea.reloc.kind == RELOC_NONE)
    return;

  r = allocate_register (a);

  compute_effective_address_in_register (a.ea, r);

  a.kind = EFFECTIVE_ADDRESS;
  a.ea.base = r;
  a.ea.index = ZERO;
  a.ea.scale = 1;
  a.ea.offset = 0;
  a.ea.reloc.kind = RELOC_NONE;
  a.ea.reloc.nr = 0;
}

// ----------------------------------------------------------------------------------------------

public
void declare_function (uint function_nr)
{
  fixup.function_register (label_nr => (int)function_nr,
                           code_ip  => current_RIP());
}

// ----------------------------------------------------------------------------------------------

public
void call_function (uint function_nr)
{
  func_store_backfill (func_label_nr => (int)function_nr,
                       fill_position => blob_index (g_blob_code));
  c_jsr (offset => 0);    // intern direct call
}

// ----------------------------------------------------------------------------------------------

public
void declare_near_label (int near_label)
{
  fixup.near_label_declare (label_nr      => near_label,
                            code_position => blob_index (g_blob_code));
}

// ----------------------------------------------------------------------------------------------

public
void cond_branch (COMPARISON_FLAG cmp, bool signed, int near_label)
{
  fixup.near_label_branch (label_nr      => near_label,
                           typ           => ARM_CONDITIONAL_BRANCH,  // +/- 1 MB, or will be expanded later
                           code_position => blob_index (g_blob_code));

  c_cond_branch (cmp    => cmp,
                 signed => signed,
                 offset => 0);  // 21 bits (+/- 1<<20)  +/- 1MB
}

//======================================================================================

public
void branch_if_not_zero (REG reg, int size, int near_label)
{
  assert size == 4 || size == 8;

  fixup.near_label_branch (label_nr      => near_label,
                           typ           => ARM_COMPARE_AND_BRANCH,  // +/- 1 MB, or will be expanded later
                           code_position => blob_index (g_blob_code));

  // CBNZ (branch if not zero)
  c_bnz_reg (source => reg,   // register being tested
             size   => size,  // 4 or 8 bytes
             offset => 0);    // 21 bits (+/- 1<<20)  +/- 1MB
}

//======================================================================================

public
void branch_if_zero (REG reg, int size, int near_label)
{
  assert size == 4 || size == 8;

  fixup.near_label_branch (label_nr      => near_label,
                           typ           => ARM_COMPARE_AND_BRANCH,  // +/- 1 MB, or will be expanded later
                           code_position => blob_index (g_blob_code));

  // CBNZ (branch if not zero)
  c_bz_reg (source => reg,   // register being tested
            size   => size,  // 4 or 8 bytes
            offset => 0);    // 21 bits (+/- 1<<20)  +/- 1MB
}

//======================================================================================

public
void branch (int near_label)
{
  fixup.near_label_branch (label_nr      => near_label,     // +/- 128 MB
                           typ           => ARM_BRANCH,
                           code_position => blob_index (g_blob_code));

  c_jmp (offset => 0); // 28 bits (+/- 1<<27)  +/- 128 MB
}

//======================================================================================

public
void generate_asm_for_function (bool is_main, int nb_labels)
{
  byte*  mem;
  int    mem_size;
  int    mem_offset;

  out_obtain_pcode_mem (out mem, out mem_size);   // get block of pcodes to translate into arm code
  mem_offset = 0;

  // labels
  fixup.near_label_allocate_table (nb_labels);

  // enter/leave
  g_enter_pcode_inserted_bytes = 0;
  leave_clear_all ();   // clear "leave" table

  g_extra_frame_bytes_pos = 0;

  g_frame_size = 0;
  g_saved_on_stack_size = 0;
  g_first_page_size = 0;
  g_extra_frame_size = 0;
  g_min_extra_frame_size = 0;
  g_stack_alignment = 0;
  g_max_extra_param_call_size = 0;

  g_is_thread_entry_point = false;
  g_is_callback = false;
  g_is_entry = false;
  g_callee_saved_regs = 0;


  // tree keeping list of branches to error handlers under the function
  create_ll_tree ();


  while (mem_offset < mem_size)
  {
    g_current_pcode'byte = mem[mem_offset:g_current_pcode'size];
    mem_offset += (int)g_current_pcode'size;

    validate_xx_stacks_before_pcode ();

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
          trace ("  # decimal %d   (%f)\n", *(int*)&f, f);

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
          trace ("  # decimal %d  (%f)\n", *(long*)&d, d);

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
        a->ea.base   = ZERO;
        a->ea.index  = ZERO;
        a->ea.scale  = 1;
        a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;
        a->ea.reloc.nr   = 0;
      }
      break;


      case P_FUNC_LABEL:    // <func_label_4>    ; each function must start with a func_label
      {
        int4 nr;

        nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label of func #%d\n", nr);

        declare_function ((uint)nr);
      }
      break;

      // load address of code, constant, global, local   (  -->  <addr> )

      case P_LOAD_CODE:     // <func_label_4>    ; load addr of func_label on addr_stack
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
        a->ea.base    = ZERO;
        a->ea.index   = ZERO;
        a->ea.scale   = 1;
        a->ea.offset  = 0;
        a->ea.reloc.kind = RELOC_FUNC;
        a->ea.reloc.nr   = nr;
      }
      break;


      // load pseudo address of syscall   (  -->  <addr> )

      case P_LOAD_SYSCALL:     // <syscall_nr_4>          ; load addr of syscall_nr on addr_stack
      {
        int4 nr;
        NODE* a;

        nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" address of syscall #%d\n", nr);

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base    = ZERO;
        a->ea.index   = ZERO;
        a->ea.scale   = 1;
        a->ea.offset  = 0;
        a->ea.reloc.kind = RELOC_SYSCALL;
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
        a->ea.base    = ZERO;
        a->ea.index   = ZERO;
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
        a->ea.base   = ZERO;
        a->ea.index  = ZERO;
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
        a->ea.base   = ZERO;
        a->ea.index  = ZERO;
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

        offset += (int)g_first_page_size;     // we need to add first page frame size to any local address or parameter on stack

        if (g_tracing)
          trace (" address of local #%d\n", offset);

        a = &astack[astack_count++];
        a->typ  = 'a';
        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base   = FP;
        a->ea.index  = ZERO;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            i = &istack[istack_count++];
            i->typ  = 'b';
            i->kind = MEMORY;
            i->ea.base   = r;
            i->ea.index  = ZERO;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => true, size => 1);

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
          }
          break;

          case MEMORY:
          {
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            {
              EA  ea;

              clear ea;
              ea.base   = r;
              ea.index  = ZERO;
              ea.scale  = 1;
              ea.offset = 0;
              ea.reloc.kind = RELOC_NONE;
              ea.reloc.nr   = 0;

              c_load_register_from_memory (r, ea, data_signed => true, size => 1);
            }

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => true, size => 2);

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
          }
          break;

          case MEMORY:
          {
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            {
              EA  ea;

              clear ea;
              ea.base   = r;
              ea.index  = ZERO;
              ea.scale  = 1;
              ea.offset = 0;
              ea.reloc.kind = RELOC_NONE;
              ea.reloc.nr   = 0;

              c_load_register_from_memory (r, ea, data_signed => true, size => 2);
            }

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => 1);

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
          }
          break;

          case MEMORY:
          {
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            {
              EA  ea;

              clear ea;
              ea.base   = r;
              ea.index  = ZERO;
              ea.scale  = 1;
              ea.offset = 0;
              ea.reloc.kind = RELOC_NONE;
              ea.reloc.nr   = 0;

              c_load_register_from_memory (r, ea, data_signed => false, size => 1);
            }

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => 2);

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
          }
          break;

          case MEMORY:
          {
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            {
              EA  ea;

              clear ea;
              ea.base   = r;
              ea.index  = ZERO;
              ea.scale  = 1;
              ea.offset = 0;
              ea.reloc.kind = RELOC_NONE;
              ea.reloc.nr   = 0;

              c_load_register_from_memory (r, ea, data_signed => false, size => 2);
            }

            i = &istack[istack_count++];
            i->typ = 'i';
            i->kind = INT_REGISTER;
            i->reg = r;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            i = &istack[istack_count++];
            i->typ  = 'i';
            i->kind = MEMORY;
            i->ea.base   = r;
            i->ea.index  = ZERO;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            i = &istack[istack_count++];
            i->typ  = 'l';
            i->kind = MEMORY;
            i->ea.base   = r;
            i->ea.index  = ZERO;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            f = &fstack[fstack_count++];
            f->typ  = 'f';
            f->kind = MEMORY;
            f->ea.base   = r;
            f->ea.index  = ZERO;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            f = &fstack[fstack_count++];
            f->typ  = 'd';
            f->kind = MEMORY;
            f->ea.base   = r;
            f->ea.index  = ZERO;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            a->typ = 'a';
            a->kind = MEMORY;
            a->ea.base   = r;
            a->ea.index  = ZERO;
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
            REG r = allocate_register (crash_node => *a);

            c_load_register_from_memory (r, a->ea, data_signed => false, size => address_size);

            a->typ  = 'a';
            a->kind = MEMORY;
            a->ea.base   = r;
            a->ea.index  = ZERO;
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
          if (i->icte == 0)
          {
            c_store_register_in_memory (r => ZERO, ea => a->ea, size => 1);
          }
          else
          {
            REG r = allocate_register ();

            move_register_immediate (target => r,
                                     imm    => i->icte,
                                     size   => 4);      // 4 or 8

            c_store_register_in_memory (r    => r,
                                        ea   => a->ea,
                                        size => 1);    // size = 1, 2, 4, 8
          }
        }
        else   // INT_REGISTER or MEMORY
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r = allocate_register (crash_node => *i);

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 1);

            i->kind = INT_REGISTER;
            i->reg  = r;
          }

          c_store_register_in_memory (r => i->reg, ea => a->ea, size => 1);
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

        flush_effective_address (ref *a);  // convert 'a' operand from memory into effective address

        if (i->kind == INT_CONSTANT)
        {
          if (i->icte == 0)
          {
            c_store_register_in_memory (r => ZERO, ea => a->ea, size => 1);
          }
          else
          {
            REG r = allocate_register ();

            move_register_immediate (target => r,
                                     imm    => i->icte,
                                     size   => 4);      // 4 or 8

            c_store_register_in_memory (r    => r,
                                        ea   => a->ea,
                                        size => 1);    // size = 1, 2, 4, 8
          }
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r = allocate_register (crash_node => *i);  // allows reusing registers of node i

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 1);

            i->kind = INT_REGISTER;
            i->reg  = r;
          }

          c_store_register_in_memory (r => i->reg, ea => a->ea, size => 1);
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

        flush_effective_address (ref *a);  // convert 'a' operand from memory into effective address

        if (i->kind == INT_CONSTANT)
        {
          if (i->icte == 0)
          {
            c_store_register_in_memory (r => ZERO, ea => a->ea, size => 2);
          }
          else
          {
            REG r = allocate_register ();

            move_register_immediate (target => r,
                                     imm    => i->icte,
                                     size   => 4);      // 4 or 8

            c_store_register_in_memory (r    => r,
                                        ea   => a->ea,
                                        size => 2);    // size = 1, 2, 4, 8
          }
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r = allocate_register (crash_node => *i);  // allows reusing registers of node i

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 2);

            i->kind = INT_REGISTER;
            i->reg  = r;
          }

          c_store_register_in_memory (r => i->reg, ea => a->ea, size => 2);
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

        flush_effective_address (ref *a);  // convert 'a' operand from memory into effective address

        if (i->kind == INT_CONSTANT)
        {
          if (i->icte == 0)
          {
            c_store_register_in_memory (r => ZERO, ea => a->ea, size => 4);
          }
          else
          {
            REG r = allocate_register ();

            move_register_immediate (target => r,
                                     imm    => i->icte,
                                     size   => 4);      // 4 or 8

            c_store_register_in_memory (r    => r,
                                        ea   => a->ea,
                                        size => 4);    // size = 1, 2, 4, 8
          }
        }
        else
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r = allocate_register (crash_node => *i);  // allows reusing registers of node i

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 4);

            i->kind = INT_REGISTER;
            i->reg  = r;
          }

          c_store_register_in_memory (r => i->reg, ea => a->ea, size => 4);
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

        flush_effective_address (ref *a);  // convert 'a' operand from memory into effective address

        if (i->kind == INT_CONSTANT)
        {
          if (i->icte == 0)
          {
            c_store_register_in_memory (r => ZERO, ea => a->ea, size => 8);
          }
          else
          {
            REG r = allocate_register ();

            move_register_immediate (target => r,
                                     imm    => i->icte,
                                     size   => 8);      // 4 or 8

            c_store_register_in_memory (r    => r,
                                        ea   => a->ea,
                                        size => 8);    // size = 1, 2, 4, 8
          }
        }
        else  // memory or register
        {
          if (i->kind == MEMORY)  // load memory operand into register
          {
            REG r = allocate_register (crash_node => *i);  // allows reusing registers of node i

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 8);

            i->kind = INT_REGISTER;
            i->reg  = r;
          }

          c_store_register_in_memory (r => i->reg, ea => a->ea, size => 8);
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

        flush_effective_address (ref *a);  // convert 'a' operand from memory into effective address

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            float fcte = (float)f->fcte;
            int4  value;

            value'byte = fcte'byte;

            if (value == 0)
            {
              c_store_register_in_memory (r => ZERO, ea => a->ea, size => 4);
            }
            else
            {
              REG r = allocate_register ();

              move_register_immediate (target => r,
                                       imm    => value,
                                       size   => 4);      // 4 or 8

              c_store_register_in_memory (r    => r,
                                          ea   => a->ea,
                                          size => 4);    // size = 1, 2, 4, 8
            }
          }
          break;

          case FLOAT_REGISTER:
          {
            c_store_fregister_in_memory (r    => f->freg,
                                         ea   => a->ea,
                                         size => 4);
          }
          break;

          case MEMORY:  // load memory operand into register
          {
            REG r = allocate_register (crash_node => *f);  // allows reusing registers of node f
            c_load_register_from_memory (r, f->ea, data_signed => false, size => 4);
            c_store_register_in_memory (r => r, ea => a->ea, size => 4);
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

        flush_effective_address (ref *a);  // convert 'a' operand from memory into effective address

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
          {
            double fcte = (float)f->fcte;
            int8   value;

            value'byte = fcte'byte;

            if (value == 0)
            {
              c_store_register_in_memory (r => ZERO, ea => a->ea, size => 8);
            }
            else
            {
              REG r = allocate_register ();

              move_register_immediate (target => r,
                                       imm    => value,
                                       size   => 8);      // 4 or 8

              c_store_register_in_memory (r    => r,
                                          ea   => a->ea,
                                          size => 8);    // size = 1, 2, 4, 8
            }
          }
          break;

          case FLOAT_REGISTER:
          {
            c_store_fregister_in_memory (r    => f->freg,
                                         ea   => a->ea,
                                         size => 8);
          }
          break;

          case MEMORY:  // load memory operand into register
          {
            REG r = allocate_register (crash_node => *f);  // allows reusing registers of node f
            c_load_register_from_memory (r, f->ea, data_signed => false, size => 8);
            c_store_register_in_memory (r => r, ea => a->ea, size => 8);
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
        NODE* val = &astack[astack_count - 1];
        NODE* adr = &astack[astack_count - 2];

        if (g_tracing)
          trace ("\n");

        flush_effective_address (ref *adr);  // convert operand from memory into effective address
        flush_effective_address (ref *val);  // convert operand from memory into effective address

        if (is_null_ea (val->ea))
        {
          c_store_register_in_memory (r => ZERO, ea => adr->ea, size => address_size);
        }
        else if (val->ea.base < X15 && val->ea.index == ZERO &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_store_register_in_memory (r => val->ea.base, ea => adr->ea, size => address_size);
        }
        else if (val->ea.base == ZERO && val->ea.index < X15 && val->ea.scale == 1 &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_store_register_in_memory (r => val->ea.index, ea => adr->ea, size => address_size);
        }
        else
        {
          REG r = allocate_register (*val);  // allows reusing registers of node val
          compute_effective_address_in_register (ea => val->ea, r => r);
          c_store_register_in_memory (r    => r,
                                      ea   => adr->ea,
                                      size => address_size);  // 1, 2, 4, 8
        }

        astack_count -= 2;
      }
      break;



      // store block        (  <target_addr>   <source_addr>   <size_uint4>  -->   )
      case P_COPY_BLOCK:          // pop <size_uint4>, pop <source_addr>, pop <target_addr>
      case P_ORDERED_COPY_BLOCK:  // same as above for overlapping areas (copy high-to-low or low-to-high)
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
          int siz = (int)nsize->icte;    // size to copy

          if (siz > 0)
          {
            int chunk, ri;
            REG regs[4];

            make_simple_effective_address (ref *src);
            make_simple_effective_address (ref *dst);

            allocate_registers (out regs);  // 4 new regs

            // load in registers
            chunk = address_size;
            siz = (int)nsize->icte;
            ri = 0;
            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_load_register_from_memory (r => regs[ri], ea => src->ea, data_signed => false, size => chunk); // size = 1, 2, 4, 8
                src->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }

            // save from registers
            chunk = address_size;
            siz = (int)nsize->icte;
            ri = 0;
            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_store_register_in_memory (r => regs[ri], ea => dst->ea, size => chunk); // size = 1, 2, 4, 8
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
          // all registers must be flushed to temporaries before calling OS
          store_all_registers_in_temporaries_except_for_this_pcode ();

          load_int4_into_reg (ref *nsize, X2);
          load_addr_into_reg (ref *src,  X1);
          load_addr_into_reg (ref *dst,  X0);

          if (g_current_pcode == P_COPY_BLOCK)
            call_libc ("memcpy");
          else                // P_ORDERED_COPY_BLOCK
            call_libc ("memmove");
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
          if (siz == 1 || siz == 2 || siz == 4 || siz == 8)
          {
            REG r;

            make_simple_effective_address (ref *src);
            make_simple_effective_address (ref *dst);
            flush_int4_in_register_for_modif (ref *count);

            r = allocate_register ();     // r = data to copy 'count' times

            // algo: jmp L1
            //     L2:
            //       memcpy (target, source, size);
            //       target += size;
            //     L1:
            //       count4--;
            //       if (count4 >= 0) goto L2   (jns)

            // load value to copy in r
            c_load_register_from_memory (r, src->ea, data_signed => false, size => siz); // size = 1, 2, 4, 8

            branch (lab1);

            declare_near_label (near_label => lab2);

            // store in indexed destination
            {
              EA ea_dst;

              ea_dst = dst->ea;
              ea_dst.index = count->reg;
              ea_dst.scale = siz;

              c_store_register_in_memory (r, ea_dst, siz); // size = 1, 2, 4, 8)
            }

            declare_near_label (near_label => lab1);

            c_subs_reg_imm (target     => count->reg,   // ZERO allowed (SP not allowed)
                            source     => count->reg,   // SP allowed (ZERO not allowed)
                            imm12      => 1,            // 0 to 4095
                            shl_imm_12 => false,        // true to shift imm12 << 12
                            size       => 4);           // 4 or 8

            fixup.near_label_branch (label_nr      => lab2,
                                     typ           => ARM_CONDITIONAL_BRANCH,  // +/- 1 MB, or will be expanded later
                                     code_position => blob_index (g_blob_code));
            c_cond_branch_raw (mask              => 0b0101,  // sign bit clear
                               offset            => 0,       // 21 bits (+/- 1<<20)  +/- 1MB
                               often_same_choice => true);

            // note that destination address (dst) is kept for the next pcode !
          }
          else       // any size
          {
            // all registers must be flushed to temporaries before calling OS
            store_all_registers_in_temporaries_except_for_this_pcode ();

            // note: X19-X22 are available (callee-saved)

            load_int4_into_reg (ref *count, X22);   // number of iterations (decrementing)
            load_addr_into_reg (ref *src, X21);     // source address
            load_addr_into_reg (ref *dst, X19);     // target address (to keep)

            c_mov_reg_reg (target => X20, source => X19,   data_size => 8);   // initialize varying target address

            // algo: jmp L1
            //     L2:
            //       memcpy (target, source, size);
            //       target += size;
            //     L1:
            //       count4--;
            //       if (count4>=0) goto L2

            branch (lab1);

            declare_near_label (near_label => lab2);


            c_mov_reg_reg (target => X0,   source => X20,   data_size => 8);   // varying target
            c_mov_reg_reg (target => X1,   source => X21,   data_size => 8);   // source
            move_register_immediate (target => X2,  imm => siz,  size => 4);
            call_libc ("memcpy");

            // increase target address for next loop
            add_offset_using_x17 (target => X20,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => X20,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => siz,
                                  size   => 8);     // 4 or 8

            declare_near_label (near_label => lab1);

            c_subs_reg_imm (target     => X22,    // ZERO allowed (SP not allowed)
                            source     => X22,    // SP allowed (ZERO not allowed)
                            imm12      => 1,      // 0 to 4095
                            shl_imm_12 => false,  // true to shift imm12 << 12
                            size       => 4);     // 4 or 8

            fixup.near_label_branch (label_nr      => lab2,
                                     typ           => ARM_CONDITIONAL_BRANCH,  // +/- 1 MB, or will be expanded later
                                     code_position => blob_index (g_blob_code));
            c_cond_branch_raw (mask              => 0b0101,  // sign bit clear
                               offset            => 0,       // 21 bits (+/- 1<<20)  +/- 1MB
                               often_same_choice => true);

            // note that destination address (dst) is kept for the next pcode !
            {
              PCODE next_pcode;
              next_pcode'byte = mem[mem_offset:next_pcode'size];

              if (next_pcode != P_DROP_ADDR)   // we don't drop it, restore destination address for next pcode
              {
                c_mov_reg_reg (target => X0, source => X19,   data_size => 8);   // restore original target address
                dst->reg = X0;
              }
            }
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

        declare_near_label (near_label => near_label_nr);
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

        branch (near_label_nr);
      }
      break;


      //    (  <bool>  -->   )
      // pop bool1 from int_stack and test it.
      // note: P_TSTBOOL is ALWAYS followed immediately either by P_BTRUE or P_BFALSE

      case P_TSTBOOL:
      {
        PCODE c;
        int4  near_label_nr;
        NODE  *i;

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

        if (c != P_BTRUE && c != P_BFALSE)
          fatal_compiler_error0 ("asm(P_TSTBOOL) : null");

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("  %s  lab #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

        // store all registers before a jump
        store_all_registers_in_temporaries_except_for_this_pcode ();

        i = &istack[istack_count - 1];
        if (i->kind == INT_CONSTANT)
        {
          if ((i->icte != 0 && c == P_BTRUE) || (i->icte == 0 && c == P_BFALSE))
          {
            branch (near_label_nr);
          }
        }
        else
        {
          if (i->kind == MEMORY)
          {
            REG r = allocate_register (*i);
            c_load_register_from_memory (r, i->ea, data_signed => false, size => 1); // size = 1, 2, 4, 8
            i->kind = INT_REGISTER;
            i->reg = r;
          }

          if (c == P_BTRUE)
          {
            branch_if_not_zero (reg => i->reg, size => 4, near_label => near_label_nr);
          }
          else  // P_BFALSE
          {
            branch_if_zero (reg => i->reg, size => 4, near_label => near_label_nr);
          }
        }

        istack_count--;

        validate_xx_stacks_for_additional_pcode (c);
      }
      break;


      //   (  <bool>  <bool>  -->   )
      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

      case P_CMP_BOOL:   // <mask>  ; compare two bool   on int_stack - 1 byte unsigned
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

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

        if (i1->kind == MEMORY)
        {
          REG r = allocate_register (*i1);   // can reuse register from i1
          c_load_register_from_memory (r, i1->ea, data_signed => false, size => 1); // size = 1, 2, 4, 8
          i1->kind = INT_REGISTER;
          i1->reg = r;
        }

        if (i2->kind == MEMORY)
        {
          REG r = allocate_register (*i2);   // can reuse register from i2
          c_load_register_from_memory (r, i2->ea, data_signed => false, size => 1); // size = 1, 2, 4, 8
          i2->kind = INT_REGISTER;
          i2->reg = r;
        }

        assert i1->kind == INT_REGISTER;
        assert i2->kind == INT_REGISTER || i2->kind == INT_CONSTANT;

        if (i2->kind == INT_CONSTANT)
        {
          assert i2->icte >= 0 && i2->icte <= 1;

          // special optimized case
          if (((COMPARISON_FLAG)mask == CMP_EQUAL || (COMPARISON_FLAG)mask == CMP_NOT_EQUAL) &&
              (c == P_BTRUE || c == P_BFALSE))
          {
            int4 near_label_nr;
            REG  r = i1->reg;

            istack_count -= 2;
            validate_xx_stacks_for_additional_pcode (c);

            near_label_nr = *((int4 *)&mem[mem_offset]);
            mem_offset += 4;

            if (g_tracing)
              trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

            store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

            if ((i2->icte == 1) ^ ((COMPARISON_FLAG)mask == CMP_NOT_EQUAL))
              branch_if_not_zero (reg => r, size => 4, near_label => near_label_nr);
            else
              branch_if_zero (reg => r, size => 4, near_label => near_label_nr);

            break;
          }

          c_cmp_reg_imm (source     => i1->reg,        // SP allowed (ZERO not allowed)
                         imm12      => (int)i2->icte,  // 0 to 4095
                         shl_imm_12 => false,          // true to shift imm12 << 12
                         size       => 4);             // 4 or 8
        }
        else   // both are registers
        {
          c_cmp_reg_reg (source1             => i1->reg, // ZERO allowed (SP not allowed)
                         source2             => i2->reg, // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,     // LSL, LSR, ASR
                         source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                         size                => 4);      // 4 or 8
        }

        // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register ();

          // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
          // used for: b = (x < y);
          c_cset (cmp    => (COMPARISON_FLAG)mask,
                  signed => false,
                  target => r,
                  size   => 4);      // 4 or 8

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

          cond_branch ((COMPARISON_FLAG)mask, signed => false, near_label_nr);
        }
      }
      break;

      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.
      //   (  <int4_a>  <int4_b>  -->   )

      case P_CMP_S4:   // <mask>  ; compare two int4   on int_stack     - signed
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

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

        if (i1->kind == MEMORY)
        {
          REG r = allocate_register (*i1);   // can reuse register from i1
          c_load_register_from_memory (r, i1->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8
          i1->kind = INT_REGISTER;
          i1->reg = r;
        }

        if (i2->kind == MEMORY)
        {
          REG r = allocate_register (*i2);   // can reuse register from i2
          c_load_register_from_memory (r, i2->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8
          i2->kind = INT_REGISTER;
          i2->reg = r;
        }

        assert i1->kind == INT_REGISTER;
        assert i2->kind == INT_REGISTER || i2->kind == INT_CONSTANT;

        if (i1->kind == INT_REGISTER && i2->kind == INT_CONSTANT)
        {
          assert (int)i2->icte == i2->icte;    // range is int4

          // special optimized case : compare with zero
          if (i2->icte == 0 &&
              ((COMPARISON_FLAG)mask == CMP_EQUAL || (COMPARISON_FLAG)mask == CMP_NOT_EQUAL) &&
              (c == P_BTRUE || c == P_BFALSE))
          {
            int4 near_label_nr;
            REG  r = i1->reg;

            istack_count -= 2;
            validate_xx_stacks_for_additional_pcode (c);

            near_label_nr = *((int4 *)&mem[mem_offset]);
            mem_offset += 4;

            if (g_tracing)
              trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

            store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

            if ((COMPARISON_FLAG)mask == CMP_NOT_EQUAL)
              branch_if_not_zero (reg => r, size => 4, near_label => near_label_nr);
            else
              branch_if_zero (reg => r, size => 4, near_label => near_label_nr);

            break;
          }

          if (i2->icte >= 0 && i2->icte <= 4095)
          {
            c_cmp_reg_imm (source     => i1->reg,        // SP allowed (ZERO not allowed)
                           imm12      => (int)i2->icte,  // 0 to 4095
                           shl_imm_12 => false,          // true to shift imm12 << 12
                           size       => 4);             // 4 or 8
          }
          else if (i2->icte >= -4095 && i2->icte <= 0)
          {
            c_cmn_reg_imm (source     => i1->reg,         // SP allowed (ZERO not allowed)
                           imm12      => -(int)i2->icte,  // 0 to 4095
                           shl_imm_12 => false,           // true to shift imm12 << 12
                           size       => 4);              // 4 or 8
          }
          else if (i2->icte >= 0 && i2->icte <= 4095*4096 && (i2->icte & 4095) == 0)
          {
            c_cmp_reg_imm (source     => i1->reg,                // SP allowed (ZERO not allowed)
                           imm12      => ((int)i2->icte) >> 12,  // 0 to 4095
                           shl_imm_12 => true,                   // true to shift imm12 << 12
                           size       => 4);                     // 4 or 8
          }
          else if (-i2->icte >= 0 && -i2->icte <= 4095*4096 && (i2->icte & 4095) == 0)
          {
            c_cmn_reg_imm (source     => i1->reg,                // SP allowed (ZERO not allowed)
                           imm12      => (-(int)i2->icte) >> 12, // 0 to 4095
                           shl_imm_12 => true,                   // true to shift imm12 << 12
                           size       => 4);                     // 4 or 8
          }
          else
          {
            REG r = allocate_register ();
            int  s, ofs;

            ofs = (int)i2->icte;
            for (s=31; (ofs & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
              ;

            ofs >>= s;

            move_register_immediate (target => r,
                                     imm    => ofs,
                                     size   => 4);   // 4 or 8

            i2->kind = INT_REGISTER;
            i2->reg = r;

            c_cmp_reg_reg (source1             => i1->reg, // ZERO allowed (SP not allowed)
                           source2             => i2->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,     // LSL, LSR, ASR
                           source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                           size                => 4);      // 4 or 8
          }
        }
        else
        {
          assert i1->kind == INT_REGISTER && i2->kind == INT_REGISTER;

          c_cmp_reg_reg (source1             => i1->reg, // ZERO allowed (SP not allowed)
                         source2             => i2->reg, // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,     // LSL, LSR, ASR
                         source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                         size                => 4);      // 4 or 8
        }

        // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register ();

          // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
          // used for: b = (x < y);
          c_cset (cmp    => (COMPARISON_FLAG)mask,
                  signed => true,
                  target => r,
                  size   => 4);      // 4 or 8

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
        else
        {
          int4 near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

          cond_branch ((COMPARISON_FLAG)mask, signed => true, near_label_nr);
        }
      }
      break;


      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.
      //   (  <uint4_a>  <uint4_b>  -->   )

      case P_CMP_U4:   // <mask>  ; compare two uint4   on int_stack     - unsigned
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_U4) : null");

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

        if (i1->kind == MEMORY)
        {
          REG r = allocate_register (*i1);   // can reuse register from i1
          c_load_register_from_memory (r, i1->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
          i1->kind = INT_REGISTER;
          i1->reg = r;
        }

        if (i2->kind == MEMORY)
        {
          REG r = allocate_register (*i2);   // can reuse register from i2
          c_load_register_from_memory (r, i2->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
          i2->kind = INT_REGISTER;
          i2->reg = r;
        }

        assert i1->kind == INT_REGISTER;
        assert i2->kind == INT_REGISTER || i2->kind == INT_CONSTANT;

        if (i1->kind == INT_REGISTER && i2->kind == INT_CONSTANT)
        {
          // special optimized case : compare with zero
          if (i2->icte == 0 &&
              ((COMPARISON_FLAG)mask == CMP_EQUAL || (COMPARISON_FLAG)mask == CMP_NOT_EQUAL) &&
              (c == P_BTRUE || c == P_BFALSE))
          {
            int4 near_label_nr;
            REG  r = i1->reg;

            istack_count -= 2;
            validate_xx_stacks_for_additional_pcode (c);

            near_label_nr = *((int4 *)&mem[mem_offset]);
            mem_offset += 4;

            if (g_tracing)
              trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

            store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

            if (((COMPARISON_FLAG)mask == CMP_NOT_EQUAL))
              branch_if_not_zero (reg => r, size => 4, near_label => near_label_nr);
            else
              branch_if_zero (reg => r, size => 4, near_label => near_label_nr);

            break;
          }

          if (i2->icte >= 0 && i2->icte <= 4095)
          {
            c_cmp_reg_imm (source     => i1->reg,        // SP allowed (ZERO not allowed)
                           imm12      => (int)i2->icte,  // 0 to 4095
                           shl_imm_12 => false,          // true to shift imm12 << 12
                           size       => 4);             // 4 or 8
          }
          else if (i2->icte >= -4095 && i2->icte <= 0)
          {
            c_cmn_reg_imm (source     => i1->reg,         // SP allowed (ZERO not allowed)
                           imm12      => -(int)i2->icte,  // 0 to 4095
                           shl_imm_12 => false,           // true to shift imm12 << 12
                           size       => 4);              // 4 or 8
          }
          else if (i2->icte >= 0 && i2->icte <= 4095*4096 && (i2->icte & 4095) == 0)
          {
            c_cmp_reg_imm (source     => i1->reg,                // SP allowed (ZERO not allowed)
                           imm12      => ((int)i2->icte) >> 12,  // 0 to 4095
                           shl_imm_12 => true,                   // true to shift imm12 << 12
                           size       => 4);                     // 4 or 8
          }
          else if (-i2->icte >= 0 && -i2->icte <= 4095*4096 && (i2->icte & 4095) == 0)
          {
            c_cmn_reg_imm (source     => i1->reg,                // SP allowed (ZERO not allowed)
                           imm12      => (-(int)i2->icte) >> 12, // 0 to 4095
                           shl_imm_12 => true,                   // true to shift imm12 << 12
                           size       => 4);                     // 4 or 8
          }
          else
          {
            REG  r = allocate_register ();
            uint s, ofs;

            ofs = (uint)i2->icte;
            for (s=31; (ofs & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
              ;

            ofs >>= s;

            move_register_immediate (target => r,
                                     imm    => ofs,
                                     size   => 4);   // 4 or 8

            i2->kind = INT_REGISTER;
            i2->reg = r;

            c_cmp_reg_reg (source1             => i1->reg, // ZERO allowed (SP not allowed)
                           source2             => i2->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,     // LSL, LSR, ASR
                           source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                           size                => 4);      // 4 or 8
          }
        }
        else
        {
          assert i1->kind == INT_REGISTER && i2->kind == INT_REGISTER;

          c_cmp_reg_reg (source1             => i1->reg, // ZERO allowed (SP not allowed)
                         source2             => i2->reg, // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,     // LSL, LSR, ASR
                         source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                         size                => 4);      // 4 or 8
        }

        // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register ();

          // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
          // used for: b = (x < y);
          c_cset (cmp    => (COMPARISON_FLAG)mask,
                  signed => false,
                  target => r,
                  size   => 4);      // 4 or 8

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
        else
        {
          int4 near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

          cond_branch ((COMPARISON_FLAG)mask, signed => false, near_label_nr);
        }
      }
      break;


      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.
      //   (  <int8_a>  <int8_b>  -->   )

      case P_CMP_S8:   // <mask>  ; compare two int8   on int_stack     - signed
      {
        byte  mask;
        PCODE c;
        NODE* i1, i2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_S8) : null");

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

        if (i1->kind == MEMORY)
        {
          REG r = allocate_register (*i1);   // can reuse register from i1
          c_load_register_from_memory (r, i1->ea, data_signed => true, size => 8); // size = 1, 2, 4, 8
          i1->kind = INT_REGISTER;
          i1->reg = r;
        }

        if (i2->kind == MEMORY)
        {
          REG r = allocate_register (*i2);   // can reuse register from i2
          c_load_register_from_memory (r, i2->ea, data_signed => true, size => 8); // size = 1, 2, 4, 8
          i2->kind = INT_REGISTER;
          i2->reg = r;
        }

        assert i1->kind == INT_REGISTER;
        assert i2->kind == INT_REGISTER || i2->kind == INT_CONSTANT;

        if (i1->kind == INT_REGISTER && i2->kind == INT_CONSTANT)
        {
          // special optimized case : compare with zero
          if (i2->icte == 0 &&
              ((COMPARISON_FLAG)mask == CMP_EQUAL || (COMPARISON_FLAG)mask == CMP_NOT_EQUAL) &&
              (c == P_BTRUE || c == P_BFALSE))
          {
            int4 near_label_nr;
            REG  r = i1->reg;

            istack_count -= 2;
            validate_xx_stacks_for_additional_pcode (c);

            near_label_nr = *((int4 *)&mem[mem_offset]);
            mem_offset += 4;

            if (g_tracing)
              trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

            store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

            if (((COMPARISON_FLAG)mask == CMP_NOT_EQUAL))
              branch_if_not_zero (reg => r, size => 8, near_label => near_label_nr);
            else
              branch_if_zero (reg => r, size => 8, near_label => near_label_nr);

            break;
          }

          if (i2->icte >= 0 && i2->icte <= 4095)
          {
            c_cmp_reg_imm (source     => i1->reg,        // SP allowed (ZERO not allowed)
                           imm12      => (int)i2->icte,  // 0 to 4095
                           shl_imm_12 => false,          // true to shift imm12 << 12
                           size       => 8);             // 4 or 8
          }
          else if (i2->icte >= -4095 && i2->icte <= 0)
          {
            c_cmn_reg_imm (source     => i1->reg,         // SP allowed (ZERO not allowed)
                           imm12      => -(int)i2->icte,  // 0 to 4095
                           shl_imm_12 => false,           // true to shift imm12 << 12
                           size       => 8);              // 4 or 8
          }
          else if (i2->icte >= 0 && i2->icte <= 4095*4096 && (i2->icte & 4095) == 0)
          {
            c_cmp_reg_imm (source     => i1->reg,                // SP allowed (ZERO not allowed)
                           imm12      => ((int)i2->icte) >> 12,  // 0 to 4095
                           shl_imm_12 => true,                   // true to shift imm12 << 12
                           size       => 8);                     // 4 or 8
          }
          else if (-i2->icte >= 0 && -i2->icte <= 4095*4096 && (i2->icte & 4095) == 0)
          {
            c_cmn_reg_imm (source     => i1->reg,                // SP allowed (ZERO not allowed)
                           imm12      => (-(int)i2->icte) >> 12, // 0 to 4095
                           shl_imm_12 => true,                   // true to shift imm12 << 12
                           size       => 8);                     // 4 or 8
          }
          else
          {
            REG  r = allocate_register ();
            int8 s, ofs;

            ofs = i2->icte;
            for (s=63; (ofs & ((1L<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
              ;

            ofs >>= s;

            move_register_immediate (target => r,
                                     imm    => ofs,
                                     size   => 8);   // 4 or 8

            i2->kind = INT_REGISTER;
            i2->reg = r;

            c_cmp_reg_reg (source1             => i1->reg, // ZERO allowed (SP not allowed)
                           source2             => i2->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,     // LSL, LSR, ASR
                           source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                           size                => 8);      // 4 or 8
          }
        }
        else
        {
          assert i1->kind == INT_REGISTER && i2->kind == INT_REGISTER;

          c_cmp_reg_reg (source1             => i1->reg, // ZERO allowed (SP not allowed)
                         source2             => i2->reg, // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,     // LSL, LSR, ASR
                         source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                         size                => 8);      // 4 or 8
        }

        // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

        istack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register ();

          // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
          // used for: b = (x < y);
          c_cset (cmp    => (COMPARISON_FLAG)mask,
                  signed => true,
                  target => r,
                  size   => 4);      // 4 or 8

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
        else
        {
          int4 near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

          cond_branch ((COMPARISON_FLAG)mask, signed => true, near_label_nr);
        }
      }
      break;


      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.
      //   (  <float_a>  <float_b>  -->   )

      case P_CMP_FLT4:    // <mask>  ; compare two float  on float_stack
      {
        byte  mask;
        PCODE c;
        NODE* f1, f2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_FLT4) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        f2 = &fstack[fstack_count - 1];   // kind can be FLOAT_CONSTANT, FLOAT_REGISTER or MEMORY
        f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        if ((f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER) ||
            (f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_REGISTER && f2->reg < f1->reg) ||
            f1->kind == FLOAT_CONSTANT)
        {
          swap_nodes (ref *f1, ref *f2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        if (f1->kind == MEMORY)
        {
          FREG r = allocate_fregister (*f1);   // can reuse register from f1
          c_load_fregister_from_memory (r, f1->ea, size => 4); // size = 4, 8
          f1->kind = FLOAT_REGISTER;
          f1->freg = r;
        }

        if (f2->kind == MEMORY)
        {
          FREG r = allocate_fregister (*f2);   // can reuse register from i2
          c_load_fregister_from_memory (r, f2->ea, size => 4); // size = 4, 8
          f2->kind = FLOAT_REGISTER;
          f2->freg = r;
        }

        assert f1->kind == FLOAT_REGISTER;
        assert f2->kind == FLOAT_REGISTER || f2->kind == FLOAT_CONSTANT;

        if (f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_CONSTANT)
        {
          if (f2->fcte == 0.0)
          {
            c_fcmp_zero (source1 => f1->freg,
                         size    => 4);         // 4 or 8
          }
          else
          {
            FREG r = allocate_fregister ();

            // FMOV (scalar, immediate): Floating-point move immediate (scalar).
            // returns false if immediate value could not be encoded.
            if (!c_fmov (target => r,
                         cte    => (float4)f2->fcte,
                         size   => 4))  // 4 or 8
            {
              // fmov() failed : allocate pool constant
              POOL p = new_pool_constant (size => 4, align => 4);
              EA   ea;

              store_float (p => p, offset => 0, value => f2->fcte, size => 4);

              clear ea;
              ea.base   = ZERO;
              ea.index  = ZERO;
              ea.scale  = 1;
              ea.offset = 0;
              ea.reloc.kind = RELOC_POOL;
              ea.reloc.nr   = serial_nr_of_pool_cte (p);

              c_load_fregister_from_memory (r    => r,
                                            ea   => ea,
                                            size => 4); // size = 4, 8
            }

            f2->kind = FLOAT_REGISTER;
            f2->freg = r;

            c_fcmp (source1 => f1->freg, // ZERO allowed (SP not allowed)
                    source2 => f2->freg, // ZERO allowed (SP not allowed)
                    size    => 4);      // 4 or 8
          }
        }
        else
        {
          assert f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_REGISTER;

          c_fcmp (source1 => f1->freg,
                  source2 => f2->freg,
                  size    => 4);
        }

        // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

        fstack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register ();

          // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
          // used for: b = (x < y);
          c_cset (cmp    => (COMPARISON_FLAG)mask,
                  signed => true,
                  target => r,
                  size   => 4);      // 4 or 8

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
        else
        {
          int4  near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

          cond_branch ((COMPARISON_FLAG)mask, signed => true, near_label_nr);
        }
      }
      break;


      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.
      //   (  <double_a>  <double_b>  -->   )

      case P_CMP_FLT8:    // <mask>  ; compare two double on float_stack
      {
        byte  mask;
        PCODE c;
        NODE* f1, f2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_FLT8) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        f2 = &fstack[fstack_count - 1];   // kind can be FLOAT_CONSTANT, FLOAT_REGISTER or MEMORY
        f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        if ((f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER) ||
            (f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_REGISTER && f2->reg < f1->reg) ||
            f1->kind == FLOAT_CONSTANT)
        {
          swap_nodes (ref *f1, ref *f2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }

        if (f1->kind == MEMORY)
        {
          FREG r = allocate_fregister (*f1);   // can reuse register from f1
          c_load_fregister_from_memory (r, f1->ea, size => 8); // size = 4, 8
          f1->kind = FLOAT_REGISTER;
          f1->freg = r;
        }

        if (f2->kind == MEMORY)
        {
          FREG r = allocate_fregister (*f2);   // can reuse register from i2
          c_load_fregister_from_memory (r, f2->ea, size => 8); // size = 4, 8
          f2->kind = FLOAT_REGISTER;
          f2->freg = r;
        }

        assert f1->kind == FLOAT_REGISTER;
        assert f2->kind == FLOAT_REGISTER || f2->kind == FLOAT_CONSTANT;

        if (f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_CONSTANT)
        {
          if (f2->fcte == 0.0)
          {
            c_fcmp_zero (source1 => f1->freg,
                         size    => 8);         // 4 or 8
          }
          else
          {
            FREG r = allocate_fregister ();

            // FMOV (scalar, immediate): Floating-point move immediate (scalar).
            // returns false if immediate value could not be encoded.
            if (f2->fcte != (float)f2->fcte ||
                !c_fmov (target => r,
                         cte    => (float4)f2->fcte,
                         size   => 8))  // 4 or 8
            {
              // fmov() failed : allocate pool constant
              POOL p = new_pool_constant (size => 8, align => 8);
              EA   ea;

              store_float (p => p, offset => 0, value => f2->fcte, size => 8);

              clear ea;
              ea.base   = ZERO;
              ea.index  = ZERO;
              ea.scale  = 1;
              ea.offset = 0;
              ea.reloc.kind = RELOC_POOL;
              ea.reloc.nr   = serial_nr_of_pool_cte (p);

              c_load_fregister_from_memory (r    => r,
                                            ea   => ea,
                                            size => 8); // size = 4, 8
            }

            f2->kind = FLOAT_REGISTER;
            f2->freg = r;

            c_fcmp (source1 => f1->freg, // ZERO allowed (SP not allowed)
                    source2 => f2->freg, // ZERO allowed (SP not allowed)
                    size    => 8);      // 4 or 8
          }
        }
        else
        {
          assert f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_REGISTER;

          c_fcmp (source1 => f1->freg,
                  source2 => f2->freg,
                  size    => 8);
        }

        // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

        fstack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register ();

          // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
          // used for: b = (x < y);
          c_cset (cmp    => (COMPARISON_FLAG)mask,
                  signed => true,
                  target => r,
                  size   => 4);      // 4 or 8

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
        else
        {
          int4 near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

          cond_branch ((COMPARISON_FLAG)mask, signed => true, near_label_nr);
        }
      }
      break;


      // compare pair of pointers, function pointers, unsafe pointers, or null pointer.
      // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.
      //   (  <addr_a>  <addr_b>  -->   )

      case P_CMP_ADDR:     // <mask>  ; compare two addr   on addr_stack    - unsigned
      {
        byte  mask;
        PCODE c;
        NODE* a1, a2;

        mask = *((byte *)&mem[mem_offset++]);

        c'byte = mem[mem_offset:c'size];
        mem_offset += (int)c'size;

        if (c != P_BTRUE && c != P_BFALSE && c != P_SETBOOL)
          fatal_compiler_error0 ("asm(P_CMP_ADDR) : null");

        if (c == P_BFALSE)
          mask = (byte)(7 - mask);

        a2 = &astack[astack_count - 1];   // .kind can be EFFECTIVE_ADDRESS or MEMORY
        a1 = &astack[astack_count - 2];   // .kind can be EFFECTIVE_ADDRESS or MEMORY

        if (g_tracing)
          trace ("  mask=%u\n", mask);

        flush_effective_address (ref *a1);  // convert 'a' operand from memory into effective address
        flush_effective_address (ref *a2);  // convert 'a' operand from memory into effective address

        if (is_null_ea (a1->ea))   // put null, if any, in a2
        {
          swap_nodes (ref *a1, ref *a2);

          if (mask == 1 || mask == 3)
            mask += 3;
          else if (mask == 4 || mask == 6)
            mask -= 3;
        }


        make_effective_address_in_base (ref *a1);

        if (is_null_ea (a2->ea))   // compare a1 to null
        {
          // special optimized case
          if (((COMPARISON_FLAG)mask == CMP_EQUAL || (COMPARISON_FLAG)mask == CMP_NOT_EQUAL) &&
              (c == P_BTRUE || c == P_BFALSE))
          {
            int4 near_label_nr;
            REG  r = a1->ea.base;

            astack_count -= 2;
            validate_xx_stacks_for_additional_pcode (c);

            near_label_nr = *((int4 *)&mem[mem_offset]);
            mem_offset += 4;

            if (g_tracing)
              trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

            store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

            if (((COMPARISON_FLAG)mask == CMP_NOT_EQUAL))
              branch_if_not_zero (reg => r, size => address_size, near_label => near_label_nr);
            else
              branch_if_zero (reg => r, size => address_size, near_label => near_label_nr);

            break;
          }

          c_cmp_reg_imm (source     => a1->ea.base, // SP allowed (ZERO not allowed)
                         imm12      => 0,           // 0 to 4095
                         shl_imm_12 => false,       // true to shift imm12 << 12
                         size       => 8);          // 4 or 8
        }
        else
        {
          make_effective_address_in_base (ref *a2);

          c_cmp_reg_reg (source1             => a1->ea.base, // ZERO allowed (SP not allowed)
                         source2             => a2->ea.base, // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,         // LSL, LSR, ASR
                         source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                         size                => 8);          // 4 or 8
        }

        // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.

        astack_count -= 2;
        validate_xx_stacks_for_additional_pcode (c);

        if (c == P_SETBOOL)
        {
          REG  r;
          NODE* i;

          if (g_tracing)
            trace ("  SETBOOL\n");

          r = allocate_register ();

          // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
          // used for: b = (x < y);
          c_cset (cmp    => (COMPARISON_FLAG)mask,
                  signed => false,
                  target => r,
                  size   => 4);      // 4 or 8

          i = &istack[istack_count++];
          i->typ = 'b';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
        else
        {
          int4 near_label_nr;

          near_label_nr = *((int4 *)&mem[mem_offset]);
          mem_offset += 4;

          if (g_tracing)
            trace ("  %s  label #%d\n", c == P_BTRUE ? "P_BTRUE" : "P_BFALSE", near_label_nr);

          store_all_registers_in_temporaries ();   // flush all registers before a cond. branch !

          cond_branch ((COMPARISON_FLAG)mask, signed => false, near_label_nr);
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

        i = &istack[istack_count - 1];

        assert i->kind == INT_REGISTER || i->kind == MEMORY;

        load_bool_into_reg (ref *i, X0);

        branch_if_zero (reg => i->reg, size => 4, near_label => near_label_nr);

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

        i = &istack[istack_count - 1];

        assert i->kind == INT_REGISTER || i->kind == MEMORY;

        load_bool_into_reg (ref *i, X0);

        branch_if_not_zero (reg => i->reg, size => 4, near_label => near_label_nr);

        istack_count--;   // remove if no branch
      }
      break;


      //  (  <int4(X0)>   -->   <int4(X0)>  )   ;  comparison for switch statement

      case P_SWITCH_CMP_S4:  // <cmp_int4> <mask>  <label4>
      {                      // compare <int4> with <cmp_int4> and branch if true
        NODE  *i;
        byte  mask;
        int4  value, near_label_nr;

        value = *((int4 *)&mem[mem_offset]);
        mask = *((byte *)&mem[mem_offset+4]);
        near_label_nr = *((int4 *)&mem[mem_offset+5]);
        mem_offset += 9;

        if (g_tracing)
          trace (" %d  mask %u  label #%d\n", value, mask, near_label_nr);

        i = &istack[istack_count - 1];
        assert i->kind == INT_REGISTER && i->reg == X0;

        if (value >= 0 && value <= 4095)
        {
          c_cmp_reg_imm (source     => X0,             // SP allowed (ZERO not allowed)
                         imm12      => value,          // 0 to 4095
                         shl_imm_12 => false,          // true to shift imm12 << 12
                         size       => 4);             // 4 or 8
        }
        else if (value >= -4095 && value <= 0)
        {
          c_cmn_reg_imm (source     => X0,              // SP allowed (ZERO not allowed)
                         imm12      => -value,          // 0 to 4095
                         shl_imm_12 => false,           // true to shift imm12 << 12
                         size       => 4);              // 4 or 8
        }
        else if (value >= 0 && value <= 4095*4096 && (value & 4095) == 0)
        {
          c_cmp_reg_imm (source     => X0,                // SP allowed (ZERO not allowed)
                         imm12      => value >> 12,       // 0 to 4095
                         shl_imm_12 => true,              // true to shift imm12 << 12
                         size       => 4);                // 4 or 8
        }
        else if (-value >= 0 && -value <= 4095*4096 && (value & 4095) == 0)
        {
          c_cmn_reg_imm (source     => X0,                // SP allowed (ZERO not allowed)
                         imm12      => (-value) >> 12,    // 0 to 4095
                         shl_imm_12 => true,              // true to shift imm12 << 12
                         size       => 4);                // 4 or 8
        }
        else
        {
          REG r = allocate_register ();   // don't reuse i !
          int s, ofs;

          for (s=31; (value & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
            ;

          ofs = value >> s;

          move_register_immediate (target => r,
                                   imm    => ofs,
                                   size   => 4);   // 4 or 8

          c_cmp_reg_reg (source1             => X0,      // ZERO allowed (SP not allowed)
                         source2             => r,       // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,     // LSL, LSR, ASR
                         source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                         size                => 4);      // 4 or 8
        }

        cond_branch ((COMPARISON_FLAG)mask, signed => true, near_label_nr);
      }
      break;


      //  (  <uint4(X0)>   -->   <uint4(X0)>  )   ;  comparison for switch statement

      case P_SWITCH_CMP_U4:  // <cmp_uint4> <mask>  <label4>
      {                      // compare <int4> with <cmp_int4> and branch if true
        NODE  *i;
        byte  mask;
        int4  near_label_nr;
        uint4 value;

        value = *((uint4 *)&mem[mem_offset]);
        mask = *((byte *)&mem[mem_offset+4]);
        near_label_nr = *((int4 *)&mem[mem_offset+5]);
        mem_offset += 9;

        if (g_tracing)
          trace (" %u  mask %u  label #%d\n", value, mask, near_label_nr);

        i = &istack[istack_count - 1];
        assert i->kind == INT_REGISTER && i->reg == X0;

        if (value <= 4095)
        {
          c_cmp_reg_imm (source     => X0,             // SP allowed (ZERO not allowed)
                         imm12      => (int)value,          // 0 to 4095
                         shl_imm_12 => false,          // true to shift imm12 << 12
                         size       => 4);             // 4 or 8
        }
        else if (value <= 4095*4096 && (value & 4095) == 0)
        {
          c_cmp_reg_imm (source     => X0,                  // SP allowed (ZERO not allowed)
                         imm12      => ((int)value) >> 12,  // 0 to 4095
                         shl_imm_12 => true,                // true to shift imm12 << 12
                         size       => 4);                  // 4 or 8
        }
        else
        {
          REG  r = allocate_register ();   // don't reuse i !
          uint s, ofs;

          for (s=31; (value & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
            ;

          ofs = value >> s;

          move_register_immediate (target => r,
                                   imm    => ofs,
                                   size   => 4);   // 4 or 8

          c_cmp_reg_reg (source1             => X0,      // ZERO allowed (SP not allowed)
                         source2             => r,       // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,     // LSL, LSR, ASR
                         source2_shift_value => s,       // range 0..31 (or 0..63 for size==8)
                         size                => 4);      // 4 or 8
        }

        cond_branch ((COMPARISON_FLAG)mask, signed => false, near_label_nr);
      }
      break;


      //  (  <int8>   -->   <int8>  )        ;  comparison for switch statement

      case P_SWITCH_CMP_8:  // <cmp_int8> <mask>  <label4>
      {
        NODE  *i;
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

        i = &istack[istack_count - 1];
        assert i->kind == INT_REGISTER && i->reg == X0;

        if (value >= 0 && value <= 4095)
        {
          c_cmp_reg_imm (source     => X0,             // SP allowed (ZERO not allowed)
                         imm12      => (int)value,     // 0 to 4095
                         shl_imm_12 => false,          // true to shift imm12 << 12
                         size       => 8);             // 4 or 8
        }
        else if (value >= -4095 && value <= 0)
        {
          c_cmn_reg_imm (source     => X0,              // SP allowed (ZERO not allowed)
                         imm12      => -(int)value,     // 0 to 4095
                         shl_imm_12 => false,           // true to shift imm12 << 12
                         size       => 8);              // 4 or 8
        }
        else if (value >= 0 && value <= 4095*4096 && (value & 4095) == 0)
        {
          c_cmp_reg_imm (source     => X0,                 // SP allowed (ZERO not allowed)
                         imm12      => ((int)value) >> 12, // 0 to 4095
                         shl_imm_12 => true,               // true to shift imm12 << 12
                         size       => 8);                 // 4 or 8
        }
        else if (-value >= 0 && -value <= 4095*4096 && (value & 4095) == 0)
        {
          c_cmn_reg_imm (source     => X0,                   // SP allowed (ZERO not allowed)
                         imm12      => (-(int)value) >> 12,  // 0 to 4095
                         shl_imm_12 => true,                 // true to shift imm12 << 12
                         size       => 8);                   // 4 or 8
        }
        else
        {
          REG  r = allocate_register ();   // don't reuse i !
          int8 ofs;
          int  s;

          for (s=63; (value & ((1L<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
            ;

          ofs = value >> s;

          move_register_immediate (target => r,
                                   imm    => ofs,
                                   size   => 8);   // 4 or 8

          c_cmp_reg_reg (source1             => X0,      // ZERO allowed (SP not allowed)
                         source2             => r,       // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,     // LSL, LSR, ASR
                         source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                         size                => 8);      // 4 or 8
        }

        cond_branch ((COMPARISON_FLAG)mask, signed => true, near_label_nr);
      }
      break;


      //  (  <uint4(X0)>   -->  / )        ;  jump table for switch statement

      case P_JUMP_4:    // <pool_id8>   pool constant contains a list of <label_nr>
      {                 //              that will be converted into addresses later on.
        NODE* i;
        int8  nr;        // jump to <pool_cte>[<uint4*address_size>]
        EA    ea;

        nr = *((int8 *)&mem[mem_offset]);
        mem_offset += 8;

        if (g_tracing)
          trace (" table pool nr=%d\n", nr);

        i = &istack[istack_count - 1];
        assert i->kind == INT_REGISTER && i->reg == X0;

        // mark for later relocate labels within jump table
        fixup.register_jump_table (nr);

        clear ea;
        ea.base   = ZERO;
        ea.index  = X0;
        ea.scale  = address_size;
        ea.offset = 0;
        ea.reloc.kind = RELOC_POOL;
        ea.reloc.nr   = nr;

        c_load_register_from_memory (r           => X0,
                                     ea          => ea,
                                     data_signed => false,
                                     size        => address_size); // size = 1, 2, 4, 8
        c_jsr_reg (target => X0);

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

      case P_HINT_PARAM:   // <typ> <nr>  :  typ 0 = Xreg, 1 = Freg   gives hint to use register >= hint
                           // cancel at next hint or at any P_STORE_PARAM_xx
      {
        byte typ = mem[mem_offset];
        int4 ofs = *(int4 *)&mem[mem_offset+1];

        mem_offset += 5;

        if (g_tracing)
        {
          if (typ == 0)  trace (" reg X%d\n", ofs);
          if (typ == 1)  trace (" reg F%d\n", ofs);
          if (typ == 2)  trace (" [SP]%d\n", ofs);
        }

        if (typ == 0)
          set_hint_x ((REG)ofs);
        else if (typ == 1)
          set_hint_f ((FREG)ofs);
        else
          clear_hints ();
      }
      break;


      // ( bool --> bool )

      case P_STORE_PARAM_BOOL:  // <typ> <nr> <signed> :  typ 0 = Xreg, 1 = Freg, 2 = Onstack[SP]  /  nr: register nr or stack offset
      {
        ref NODE                       i   = istack[istack_count - 1];
        ref PARAMETER_STORAGE_LOCATION loc = istack_extra[istack_count - 1];
        byte                           typ = mem[mem_offset];
        int4                           ofs = *(int4 *)&mem[mem_offset+1];
        bool                           signed = (bool)mem[mem_offset+5];

        mem_offset += 6;

        if (g_tracing)
        {
          if (typ == 0)  trace (" reg X%d\n", ofs);
          if (typ == 1)  trace (" reg F%d\n", ofs);
          if (typ == 2)  trace (" [SP]%d\n", ofs);
        }

        clear_hints ();

        istack_extra[istack_count - 1] = {type => (PARAMETER_STORAGE_LOCATION_TYPE)typ, nr => ofs, is_signed => signed};

        // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
        free_param_slot_for_xstack (loc, i);

        // evaluate node into register, except constants
        if (i.kind != INT_CONSTANT)
        {
          REG r;

          if (typ == 0)   // final parameter location is register
            r = (REG)ofs;
          else            // final parameter location is stack location [SP], so store temporarily in register
            r = allocate_register (i);

          load_bool_into_reg (ref i, r);
        }
      }
      break;


      // ( int -->  int )

      case P_STORE_PARAM_INT4:  // <typ> <nr> <signed>
      {
        ref NODE                       i   = istack[istack_count - 1];
        ref PARAMETER_STORAGE_LOCATION loc = istack_extra[istack_count - 1];
        byte                           typ = mem[mem_offset];
        int4                           ofs = *(int4 *)&mem[mem_offset+1];
        bool                           signed = (bool)mem[mem_offset+5];

        mem_offset += 6;

        if (g_tracing)
        {
          if (typ == 0)  trace (" reg X%d\n", ofs);
          if (typ == 1)  trace (" reg F%d\n", ofs);
          if (typ == 2)  trace (" [SP]%d\n", ofs);
        }

        clear_hints ();

        // store kind & offset in parallel xstack table
        istack_extra[istack_count - 1] = {type => (PARAMETER_STORAGE_LOCATION_TYPE)typ, nr => ofs, is_signed => signed};

        // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
        free_param_slot_for_xstack (loc, i);

        // evaluate node into register, except constants
        if (i.kind != INT_CONSTANT)
        {
          REG r;

          if (typ == 0)   // final parameter location is register
            r = (REG)ofs;
          else            // final parameter location is stack location [SP], so store temporarily in register
            r = allocate_register (i);

          if (signed)
            load_int4_into_reg (ref i, r);
          else
            load_uint4_into_reg (ref i, r);
        }
      }
      break;


      // ( long -->  long )

      case P_STORE_PARAM_INT8:  // <typ> <nr> <signed>
      {
        ref NODE                       i   = istack[istack_count - 1];
        ref PARAMETER_STORAGE_LOCATION loc = istack_extra[istack_count - 1];
        byte                           typ = mem[mem_offset];
        int4                           ofs = *(int4 *)&mem[mem_offset+1];
        bool                           signed = (bool)mem[mem_offset+5];

        mem_offset += 6;

        if (g_tracing)
        {
          if (typ == 0)  trace (" reg X%d\n", ofs);
          if (typ == 1)  trace (" reg F%d\n", ofs);
          if (typ == 2)  trace (" [SP]%d\n", ofs);
        }

        clear_hints ();

        // store kind & offset in parallel xstack table
        istack_extra[istack_count - 1] = {type => (PARAMETER_STORAGE_LOCATION_TYPE)typ, nr => ofs, is_signed => signed};

        // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
        free_param_slot_for_xstack (loc, i);

        // evaluate node into register, except constants
        if (i.kind != INT_CONSTANT)
        {
          REG r;

          if (typ == 0)   // final parameter location is register
            r = (REG)ofs;
          else            // final parameter location is stack location [SP], so store temporarily in register
            r = allocate_register (i);

          load_int8_into_reg (ref i, r);
        }
      }
      break;


      //   (  float  -->  float )

      case P_STORE_PARAM_FLT4:  // <typ> <nr> <signed>
      {
        ref NODE                       f   = fstack[fstack_count - 1];
        ref PARAMETER_STORAGE_LOCATION loc = fstack_extra[fstack_count - 1];
        byte                           typ = mem[mem_offset];
        int4                           ofs = *(int4 *)&mem[mem_offset+1];
        bool                           signed = (bool)mem[mem_offset+5];

        mem_offset += 6;

        if (g_tracing)
        {
          if (typ == 0)  trace (" reg X%d\n", ofs);
          if (typ == 1)  trace (" reg F%d\n", ofs);
          if (typ == 2)  trace (" [SP]%d\n", ofs);
        }

        clear_hints ();

        fstack_extra[fstack_count - 1] = {type => (PARAMETER_STORAGE_LOCATION_TYPE)typ, nr => ofs, is_signed => signed};

        // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
        free_param_slot_for_xstack (loc, f);

        // evaluate node into register, except constants
        if (f.kind != FLOAT_CONSTANT)
        {
          FREG r;

          if (typ == 0)   // final parameter location is register
            r = (FREG)ofs;
          else            // final parameter location is stack location [SP], so store temporarily in register
            r = allocate_fregister (f);

          load_float4_into_reg (ref f, r);
        }
      }
      break;


      case P_STORE_PARAM_FLT8:  // <typ> <nr> <signed>
      {
        ref NODE                       f   = fstack[fstack_count - 1];
        ref PARAMETER_STORAGE_LOCATION loc = fstack_extra[fstack_count - 1];
        byte                           typ = mem[mem_offset];
        int4                           ofs = *(int4 *)&mem[mem_offset+1];
        bool                           signed = (bool)mem[mem_offset+5];

        mem_offset += 6;

        if (g_tracing)
        {
          if (typ == 0)  trace (" reg X%d\n", ofs);
          if (typ == 1)  trace (" reg F%d\n", ofs);
          if (typ == 2)  trace (" [SP]%d\n", ofs);
        }

        clear_hints ();

        fstack_extra[fstack_count - 1] = {type => (PARAMETER_STORAGE_LOCATION_TYPE)typ, nr => ofs, is_signed => signed};

        // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
        free_param_slot_for_xstack (loc, f);

        // evaluate node into register, except constants
        if (f.kind != FLOAT_CONSTANT)
        {
          FREG r;

          if (typ == 0)   // final parameter location is register
            r = (FREG)ofs;
          else            // final parameter location is stack location [SP], so store temporarily in register
            r = allocate_fregister (f);

          load_float8_into_reg (ref f, r);
        }
      }
      break;


      //   (  addr  -->  addr )

      case P_STORE_PARAM_ADDR:  // <typ> <nr> <signed>
      {
        ref NODE                       a   = astack[astack_count - 1];
        ref PARAMETER_STORAGE_LOCATION loc = astack_extra[astack_count - 1];
        byte                           typ = mem[mem_offset];
        int4                           ofs = *(int4 *)&mem[mem_offset+1];
        bool                           signed = (bool)mem[mem_offset+5];

        mem_offset += 6;

        if (g_tracing)
        {
          if (typ == 0)  trace (" reg X%d\n", ofs);
          if (typ == 1)  trace (" reg F%d\n", ofs);
          if (typ == 2)  trace (" [SP]%d\n", ofs);
        }

        clear_hints ();

        // store kind & offset in parallel xstack table
        astack_extra[astack_count - 1] = {type => (PARAMETER_STORAGE_LOCATION_TYPE)typ, nr => ofs, is_signed => signed};

        // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
        free_param_slot_for_xstack (loc, a);

        // evaluate node into register, except effective address with index == ZERO (only base, offset, relocation)

        // a can be of kind EFFECTIVE_ADDRESS or MEMORY

        if (a.kind == MEMORY)
        {
          REG r;

          if (typ == 0)   // final parameter location is register
            r = (REG)ofs;
          else            // final parameter location is stack location [SP], so store temporarily in register
            r = allocate_register (a);

          c_load_register_from_memory (r, a.ea, data_signed => false, size => address_size);

          a.kind = EFFECTIVE_ADDRESS;
          a.typ  = 'a';
          a.ea.base = r;
          a.ea.index = ZERO;
          a.ea.scale = 1;
          a.ea.offset = 0;
          a.ea.reloc = {RELOC_NONE, 0};
        }

        if (a.ea.index != ZERO)
        {
          REG r;

          if (typ == 0)   // final parameter location is register
            r = (REG)ofs;
          else            // final parameter location is stack location [SP], so store temporarily in register
            r = allocate_register (a);

          compute_effective_address_in_register (a.ea, r);

          a.kind = EFFECTIVE_ADDRESS;
          a.typ  = 'a';
          a.ea.base = r;
          a.ea.index = ZERO;
          a.ea.scale = 1;
          a.ea.offset = 0;
          a.ea.reloc = {RELOC_NONE, 0};
        }
      }
      break;


      //   (  -->   )

      case P_CALL_ARM:   // <#istack_slots> <#fstack_slots> <#astack_slots> <size_of_param_on_stack>
      {
        int[3] ifa_slots;         // nb of istack,fstack,astack slots used by parameters
        int    size_of_param_on_stack;
        NODE*  a_call;
        bool   extern_call;

        ifa_slots'byte = mem[mem_offset:12];
        mem_offset += (int)ifa_slots'size;

        size_of_param_on_stack'byte = mem[mem_offset:4];
        mem_offset += (int)size_of_param_on_stack'size;

        // this is a dirty trick : we access the function call address below all address parameters
        a_call = &astack[astack_count - ifa_slots[2] - 1];

        extern_call = (a_call->ea.reloc.kind != RELOC_FUNC);    // it's an indirect call to DLL or syscall

        if (g_tracing)
          trace ("\n");

        // compute total stack requirements and allocate this on extra frame, rounded up to 16
        // (usually zero if there are no stack parameters)
        size_of_param_on_stack = (size_of_param_on_stack + 15) & -16;
        if (g_max_extra_param_call_size < (uint)size_of_param_on_stack)
          g_max_extra_param_call_size = (uint)size_of_param_on_stack;

        // flush all registers to temporaries, except for this pcode and for all parameters
        store_all_registers_in_temporaries_except_some_nodes (ifa_slots[0],
                                                              ifa_slots[1],
                                                              ifa_slots[2] + 1);   // call address

        // parameter nodes must be finally evaluated and stored in their final target (register or [SP] location)

        // send istack parameters
        {
          int s;
          for (s=istack_count - 1; s>=istack_count - ifa_slots[0]; s--)  // from right to left disturbs less X0
          {
            ref PARAMETER_STORAGE_LOCATION loc = istack_extra[s];
            ref NODE                       i   = istack[s];
            int                            siz = size_of_operand (i);   // 1, 4, or 8

            // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
            free_param_slot_for_xstack (loc, i);

            // node can be : INT_CONSTANT, INT_REGISTER, or MEMORY (temporary, so base=FP with offset)
            // INITIAL NODE PARAMETERS MIGHT USE ALL REGISTERS (X0 to X14) OR BE IN TEMP LOCATIONS
            // X19-X22 are available for moving data around.

            if (loc.type == IN_X_REGISTER)  // must be stored in register
            {
              REG target_r = (REG)loc.nr;

              // node can be : INT_CONSTANT, INT_REGISTER, or MEMORY (temporary, so base=FP with offset)

              switch (i.kind)
              {
                case INT_CONSTANT:
                  if (extern_call)
                  {
                    move_register_immediate (target => target_r,
                                             imm    => i.icte,
                                             size   => 8);     // 4 or 8
                  }
                  else
                  {
                    move_register_immediate (target => target_r,
                                             imm    => i.icte,
                                             size   => siz == 1 ? 4 : siz);     // 4 or 8
                  }
                  break;

                case INT_REGISTER:
                  if (i.reg != target_r)
                  {
                    if (extern_call)
                    {
                      if (siz < 8 && loc.is_signed)
                      {
                        c_extend_signed (target      => target_r,   // 8 bytes
                                         source      => i.reg,
                                         source_size => siz);       // 1, 2 or 4 bytes
                      }
                      else
                      {
                        c_mov_reg_reg (target    => target_r,             // ZERO allowed (SP not allowed)
                                       source    => i.reg,                // ZERO allowed (SP not allowed)
                                       data_size => siz == 1 ? 4 : siz);  // 4 or 8
                      }
                    }
                    else
                    {
                      c_mov_reg_reg (target    => target_r,             // ZERO allowed (SP not allowed)
                                     source    => i.reg,                // ZERO allowed (SP not allowed)
                                     data_size => siz == 1 ? 4 : siz);  // 4 or 8
                    }
                  }
                  else
                  {
                    if (extern_call)
                    {
                      // we want all parameters to be extended to 8 bytes, to avoid any uninitialized parameter
                      if (siz < 8 && loc.is_signed)
                      {
                        c_extend_signed (target      => target_r,   // 8 bytes
                                         source      => i.reg,
                                         source_size => siz);       // 1, 2 or 4 bytes
                      }
                    }
                  }
                  break;

                case MEMORY:
                  c_load_register_from_memory (target_r, i.ea, data_signed => loc.is_signed, size => siz); // size = 1, 2, 4, 8
                  break;

                default:
                  abort;
              }

              i.kind = INT_REGISTER;
              i.reg  = target_r;
            }
            else     // must be stored on stack
            {
              EA sp_ea;

              // bool/int4/int8
              // can be INT_CONSTANT, INT_REGISTER, MEMORY
              // must be stored in [SP] location

              sp_ea = {base => SP, index => ZERO, scale => 1, offset => loc.nr, reloc => {RELOC_NONE, 0}};

              switch (i.kind)
              {
                case INT_CONSTANT:
                  move_memory_immediate (target  => sp_ea,
                                         imm     => i.icte,
                                         size    => 8);      // 1, 2, 4 or 8
                  break;

                case INT_REGISTER:
                  if (extern_call)
                  {
                    if (siz < 8 && loc.is_signed)
                    {
                      c_extend_signed (target      => i.reg,   // 8 bytes
                                       source      => i.reg,
                                       source_size => siz);    // 1, 2 or 4 bytes
                    }
                  }
                  c_store_register_in_memory (r => i.reg, ea => sp_ea, size => 8); // size = 1, 2, 4, 8
                  break;

                case MEMORY:
                  c_load_register_from_memory (X19, i.ea, data_signed => loc.is_signed, size => siz); // size = 1, 2, 4, 8
                  c_store_register_in_memory (r => X19, ea => sp_ea, size => 8); // size = 1, 2, 4, 8
                  break;

                default:
                  abort;
              }

              i.kind = MEMORY;
              i.ea = sp_ea;
            }
          }
        }

        // send fstack parameters
        {
          int s;
          for (s=fstack_count - 1; s>=fstack_count - ifa_slots[1]; s--)  // from right to left disturbs less F0
          {
            ref PARAMETER_STORAGE_LOCATION loc = fstack_extra[s];
            ref NODE                       f   = fstack[s];
            int                            siz = size_of_operand (f);   // 4, or 8


            // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
            free_param_slot_for_xstack (loc, f);

            // node can be : FLOAT_CONSTANT, FLOAT_REGISTER, or MEMORY (temporary, so base=FP with offset)
            // INITIAL NODE PARAMETERS MIGHT USE ALL REGISTERS (F0 to F7) OR BE IN TEMP LOCATIONS
            // THERE ARE NO TEMPORARY FLOAT REGISTERS AVAILABLE !  (F8-F31 are callee-saved)

            if (loc.type == IN_F_REGISTER)  // must be stored in register
            {
              FREG target_r = (FREG)loc.nr;

              // node can be : FLOAT_CONSTANT, FLOAT_REGISTER, or MEMORY (temporary, so base=FP with offset)

              switch (f.kind)
              {
                case FLOAT_CONSTANT:
                  move_fregister_immediate (target => target_r,
                                            imm    => f.fcte,
                                            size   => siz);       // 4 or 8
                  break;

                case FLOAT_REGISTER:
                  if (f.freg != target_r)
                  {
                    c_regf_to_regf (target    => target_r,
                                    source    => f.freg,
                                    data_size => siz); // 4 or 8
                  }
                  break;

                case MEMORY:
                  c_load_fregister_from_memory (target_r, f.ea, size => siz); // size = 4, 8
                  break;

                default:
                  abort;
              }

              f.kind = FLOAT_REGISTER;
              f.freg  = target_r;
            }
            else     // must be stored on stack
            {
              EA sp_ea;

              // bool/int4/int8
              // can be FLOAT_CONSTANT, FLOAT_REGISTER, MEMORY
              // must be stored in [SP] location

              sp_ea = {base => SP, index => ZERO, scale => 1, offset => loc.nr, reloc => {RELOC_NONE, 0}};

              switch (f.kind)
              {
                case FLOAT_CONSTANT:
                  move_memory_fimmediate (target => sp_ea,  value => f.fcte,  size => siz);
                  break;

                case FLOAT_REGISTER:
                  c_store_fregister_in_memory (f.freg, sp_ea, siz); // size = 4, 8
                  break;

                case MEMORY:  // load memory operand into register
                  c_load_register_from_memory (X19, f.ea, data_signed => false, size => siz); // size = 1, 2, 4, 8
                  c_store_register_in_memory (r => X19, ea => sp_ea, size => siz); // size = 1, 2, 4, 8
                  break;

                default:
                  abort;
              }

              f.kind = MEMORY;
              f.ea = sp_ea;
            }
          }
        }


        // send astack parameters
        {
          int s;
          // subtract 1 to skip function call address
          for (s=astack_count - 1; s>=astack_count - ifa_slots[2]; s--)  // from right to left disturbs less X0
          {
            ref PARAMETER_STORAGE_LOCATION loc = astack_extra[s];
            ref NODE                       a   = astack[s];


            // check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
            free_param_slot_for_xstack (loc, a);

            // node can be : EFFECTIVE_ADDRESS or MEMORY (temporary, so base=FP with offset)

            if (loc.type == IN_X_REGISTER)  // must be stored in register
            {
              REG target_r = (REG)loc.nr;
              load_addr_into_reg (ref a, target_r);
            }
            else     // must be stored on stack
            {
              EA sp_ea;

              // must be stored in [SP] location
              sp_ea = {base => SP, index => ZERO, scale => 1, offset => loc.nr, reloc => {RELOC_NONE, 0}};

              switch (a.kind)
              {
                case EFFECTIVE_ADDRESS:
                  load_addr_into_reg (ref a, X19);
                  c_store_register_in_memory (r    => X19,
                                              ea   => sp_ea,
                                              size => 8);    // size = 1, 2, 4, 8
                  break;

                case MEMORY:  // load memory operand (temporary location) to sp location
                  c_load_register_from_memory (X19, a.ea, data_signed => false, size => address_size); // size = 1, 2, 4, 8
                  c_store_register_in_memory (r => X19, ea => sp_ea, size => address_size); // size = 1, 2, 4, 8
                  break;

                default:
                  abort;
              }

              a.kind = MEMORY;
              a.ea = sp_ea;
            }
          }
        }

        {
          switch (a_call->kind)
          {
            case EFFECTIVE_ADDRESS:
              if (a_call->ea.base == ZERO && a_call->ea.index == ZERO && a_call->ea.offset == 0 && a_call->ea.reloc.kind == RELOC_FUNC)
              {
                call_function ((uint)a_call->ea.reloc.nr);
              }
              else if (a_call->ea.base != ZERO && a_call->ea.index == ZERO && a_call->ea.offset == 0 && a_call->ea.reloc.kind == RELOC_NONE)
              {
                c_jsr_reg (target => a_call->ea.base);   // intern indirect call
              }
              else if (a_call->ea.base == ZERO && a_call->ea.index == ZERO && a_call->ea.offset == 0 && a_call->ea.reloc.kind == RELOC_SYSCALL)
              {
                move_register_immediate (target => X8,      // syscall nr
                                         imm    => a_call->ea.reloc.nr,
                                         size   => 4);      // 4 or 8
                c_svc (0);
              }
              else
              {
                compute_effective_address_in_register (ea => a_call->ea, r => X8);
                c_jsr_reg (target => X8);
              }
              break;

            case MEMORY:
              c_load_register_from_memory (X8, a_call->ea, data_signed => false, size => 8);
              c_jsr_reg (target => X8);
              break;

            default:
              abort;
          }
        }
      }
      break;

      // when returning from the function call, the return value will be
      // in top xx_stack position (always X0 / F0)


      // code that indicates that the called function pushed a value on xx_stack.
      // no effect for asm except indicating that X0 / F0 is allocated.

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
        i->reg = X0;
      }
      break;


      case P_RETVALUE_4:     // function returned an int4/uint4 value on int_stack
      {
        NODE* i = &istack[istack_count++];
        if (g_tracing)
          trace ("\n");

        i->typ = 'i';
        i->kind = INT_REGISTER;
        i->reg = X0;
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
        i->reg = X0;
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
        f->freg = F0;
      }
      break;


      //   (   -->  <double> )

      case P_RETVALUE_FLT8:    // function returned a float8 value on float_stack
      {
        NODE* f = &fstack[fstack_count++];
        if (g_tracing)
          trace ("\n");

        f->typ = 'd';
        f->kind = FLOAT_REGISTER;
        f->freg = F0;
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
        a->ea.base   = X0;
        a->ea.index  = ZERO;
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

      case P_ENTER:    // <size4>  <free4>  <block4>  <flag_1X_is_thread_entry_point_2X_is_callback_4X_is_entry>
      {
        g_frame_size            = *((uint4 *)&mem[mem_offset]);      // size of frame to allocate (>=0)
        g_saved_on_stack_size   = *((uint4 *)&mem[mem_offset+4]);    // total size of parameters saved on stack (>=0)
        g_stack_alignment       = *((uint4 *)&mem[mem_offset+8]);    // allocate new space in blocks.
        g_callee_saved_regs     = *(( int4 *)&mem[mem_offset+12]);   // android only (negative local address) (0 if no saving)
        g_is_thread_entry_point = (*((int4 *)&mem[mem_offset+16]) & 1) != 0;
        g_is_callback           = (*((int4 *)&mem[mem_offset+16]) & 2) != 0;
        g_is_entry              = (*((int4 *)&mem[mem_offset+16]) & 4) != 0;

        mem_offset += (20+4);  // parameter_table is unused

        if (g_tracing)
          trace (" frame_size = %u, g_saved_on_stack_size = %u, alignment = %u, is_thread = %u, is_callback = %u, is_entry = %u\n",
                 g_frame_size, g_saved_on_stack_size, g_stack_alignment, (uint)g_is_thread_entry_point, (uint)g_is_callback, (uint)g_is_entry);

        if (g_frame_size <= 4096 - 16 - g_saved_on_stack_size)    // only 1 page allocated
        {
          g_first_page_size = g_saved_on_stack_size + g_frame_size + 16;   // includes 16 for (old_fp+ret_addr)
          g_extra_frame_size = 0;                                          // to subtract from SP
        }
        else if (g_frame_size < 4096 - g_saved_on_stack_size)
          abort;
        else
        {
          g_first_page_size = 4096 - g_saved_on_stack_size;        // includes 16 for (old_fp+ret_addr)
          g_extra_frame_size = g_frame_size - g_first_page_size;   // to subtract from SP
        }

        if (g_first_page_size > 512)
        {
          assert g_first_page_size <= 4096;

          // sub sp,#g_first_page_size
          if (g_first_page_size == 4096)
          {
            c_sub_reg_imm (target     => SP,
                           source     => SP,
                           imm12      => 1,      // 0 to 4095
                           shl_imm_12 => true,   // true to shift imm12 << 12
                           size       => 8);
          }
          else
          {
            c_sub_reg_imm (target     => SP,
                           source     => SP,
                           imm12      => (int)g_first_page_size,      // 0 to 4095
                           shl_imm_12 => false, // true to shift imm12 << 12
                           size       => 8);
          }

          // stp FP,X30,[SP]
          assert c_store_pair (source1   => FP,      // ZERO allowed
                               source2   => X30,     // ZERO allowed
                               base      => SP,      // SP allowed (effective address)
                               offset    => 0,       // -512 to +504 to be added to base address
                               ldp_mode  => SIMPLE_LOAD, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                               data_size => 8);      // 4 or 8
        }
        else
        {
          // stp FP,X30,[SP],#-g_first_page_size
          assert c_store_pair (source1   => FP,                  // ZERO allowed
                               source2   => X30,                 // ZERO allowed
                               base      => SP,                  // SP allowed (effective address)
                               offset    => -(int)g_first_page_size,  // -512 to +504 to be added to base address
                               ldp_mode  => PRE_ADD,             // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                               data_size => 8);                  // 4 or 8
        }

        // mov fp,sp
        c_add_reg_imm (target     => FP,    // SP allowed (ZERO not allowed)
                       source     => SP,    // SP allowed (ZERO not allowed)
                       imm12      => 0,     // 0 to 4095
                       shl_imm_12 => false, // true to shift imm12 << 12
                       size       => 8);    // 4 or 8

        if (g_callee_saved_regs != 0)   // save 4 callee-registers X19-X22
        {
          int offset = g_callee_saved_regs + (int)g_first_page_size;

          if (offset <= 504)
          {
            assert c_store_pair (source1   => X19,         // ZERO allowed
                                 source2   => X20,         // ZERO allowed
                                 base      => FP,          // SP allowed (effective address)
                                 offset    => offset,      // -512 to +504 to be added to base address
                                 ldp_mode  => SIMPLE_LOAD, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                                 data_size => 8);          // 4 or 8
          }
          else
          {
            assert c_store_ofs12 (source    => X19,      // ZERO allowed
                                  base      => FP,       // SP allowed (effective address)
                                  offset    => offset,   // 12 bits (0 to 4095 * size) to be added to base address
                                  data_size => 8);       // 1, 2, 4 or 8

            assert c_store_ofs12 (source    => X20,      // ZERO allowed
                                  base      => FP,       // SP allowed (effective address)
                                  offset    => offset+8, // 12 bits (0 to 4095 * size) to be added to base address
                                  data_size => 8);       // 1, 2, 4 or 8
          }


          offset += 16;

          if (offset <= 504)
          {
            assert c_store_pair (source1   => X21,         // ZERO allowed
                                 source2   => X22,         // ZERO allowed
                                 base      => FP,          // SP allowed (effective address)
                                 offset    => offset,      // -512 to +504 to be added to base address
                                 ldp_mode  => SIMPLE_LOAD, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                                 data_size => 8);          // 4 or 8
          }
          else
          {
            assert c_store_ofs12 (source    => X21,      // ZERO allowed
                                  base      => FP,       // SP allowed (effective address)
                                  offset    => offset,   // 12 bits (0 to 4095 * size) to be added to base address
                                  data_size => 8);       // 1, 2, 4 or 8

            assert c_store_ofs12 (source    => X22,      // ZERO allowed
                                  base      => FP,       // SP allowed (effective address)
                                  offset    => offset+8, // 12 bits (0 to 4095 * size) to be added to base address
                                  data_size => 8);       // 1, 2, 4 or 8
          }
        }

        g_min_extra_frame_size = g_extra_frame_size; // all locals below this are temporaries

        // "g_extra_frame_size" will be allocated below, page per page, with probing
        g_extra_frame_bytes_pos = blob_index (g_blob_code);   // code for inserting stack space and probe stack for 4K page will be inserted here !
      }
      break;

      case P_SAVE_XREG:   // <reg_nr>, <offset>, <actual size>
        {
          int reg_nr = *((int4 *)&mem[mem_offset]);
          int ofs    = *((int4 *)&mem[mem_offset+4]);
          int siz    = *((int4 *)&mem[mem_offset+8]);
          EA  ea;

          mem_offset += 12;

          ofs += (int)g_first_page_size;

          if (g_tracing)
            trace (" reg=%s, ofs=%d, siz=%d\n", (X0+reg_nr)'string, ofs, siz);

          clear ea;
          ea.base = FP;
          ea.index = ZERO;
          ea.scale = 1;
          ea.offset = ofs;
          ea.reloc.kind = RELOC_NONE;

          c_store_register_in_memory (r    => X0+reg_nr,
                                      ea   => ea,
                                      size => siz);  // size = 1, 2, 4, 8

          // maybe combine several pcodes to use stp (store pair of registers) for small ofs, if full registers are set
        }
        break;

      case P_SAVE_FREG:
        {
          int reg_nr = *((int4 *)&mem[mem_offset]);
          int ofs    = *((int4 *)&mem[mem_offset+4]);
          int siz    = *((int4 *)&mem[mem_offset+8]);
          EA  ea;

          mem_offset += 12;

          ofs += (int)g_first_page_size;

          if (g_tracing)
            trace (" reg=%s, ofs=%d, siz=%d\n", (F0+reg_nr)'string, ofs, siz);

          clear ea;
          ea.base = FP;
          ea.index = ZERO;
          ea.scale = 1;
          ea.offset = ofs;
          ea.reloc.kind = RELOC_NONE;

          c_store_fregister_in_memory (r    => F0+reg_nr,
                                       ea   => ea,
                                       size => siz);  // size = 4, 8

          // maybe combine several pcodes to use stp (store pair of registers) for small ofs, if full registers are set
        }
        break;

      case P_LEAVE:
        if (g_tracing)
          trace ("\n");
        leave_register (backfill_pos => blob_index (g_blob_code));    // store blob position for later filling
        break;


        //   (   -->   )

      case P_RETURN_VOID:    // indicates a function with no return value
        if (g_tracing)
          trace ("\n");

        if (is_main || g_is_thread_entry_point || g_is_callback || g_is_entry)
        {
          // clear X0
          move_register_immediate (target => X0,
                                   imm    => 0,
                                   size   => 4);   // 4 or 8
        }
        break;


      // flush result value and make sure it is in X0
      //   ( <uint1>   -->   )

      case P_RETURN_BOOL:   // return value is bool on int_stack
      {
        ref NODE i = istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        load_bool_into_reg (ref i, X0);

        if (g_is_callback || g_is_entry)   // returns value to OS or library
        {
          c_extend_unsigned (target      => X0,   // 8 bytes
                             source      => X0,
                             source_size => 1);   // 1 or 2 bytes
        }

        istack_count--;
      }
      break;


      // flush result value and make sure it is in X0
      //   ( <int4/uint4> -->  )

      case P_RETURN_4:      // return value is int4/uint4 on int_stack
      {
        ref NODE i = istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        load_int4_into_reg (ref i, X0);

        istack_count--;
      }
      break;


      // flush result value and make sure it is in X0
      //   (   <int8> -->  )

      case P_RETURN_8:      // return value is int8 value on int_stack
      {
        ref NODE i = istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        load_int8_into_reg (ref i, X0);

        istack_count--;
      }
      break;


      //   (   <float> -->  )

      case P_RETURN_FLT4:  // return value is float4 value on float_stack
      {
        ref NODE f = fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        load_float4_into_reg (ref f, F0);

        fstack_count--;
      }
      break;

      case P_RETURN_FLT8:  // return value is float8 value on float_stack
      {
        ref NODE f = fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        load_float8_into_reg (ref f, F0);

        fstack_count--;
      }
      break;


      case P_RETURN_ADDR:      // return value is addr value on addr_stack
      {
        ref NODE a = astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        load_addr_into_reg (ref a, X0);

        astack_count--;
      }
      break;


      //   (   -->   )

      case P_RET:
      {
        int size = *((int4 *)&mem[mem_offset]);  // <size4> ; size of parameters to pop from stack (ignore for arm)
        mem_offset += 4;

        _unused size;

        if (g_tracing)
          trace ("\n");

        c_ret (target => X30);
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
          flush_uint1_in_register (ref *i);

          // TST (shifted register)
          // sets condition flags
          c_tst_reg (source => i->reg,  // ZERO allowed (SP not allowed)
                     size   => 4);      // 4 or 8

          {
            REG r = allocate_register (*i);

            // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
            c_cset (cmp    => CMP_EQUAL,
                    signed => false,
                    target => r,
                    size   => 4);

            i->kind = INT_REGISTER;
            i->reg = r;
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

        // prefer smallest register nr as first, or constant as second
        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_uint1_in_register_for_modif (ref *i1);      // flush into byte-aligned register

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
            c_or_reg_reg (target              => i1->reg,  // ZERO allowed (SP not allowed)
                          source1             => i1->reg,  // ZERO allowed (SP not allowed)
                          source2             => i2->reg,  // ZERO allowed (SP not allowed)
                          source2_shift_type  => LSL,      // LSL, LSR, ASR
                          source2_shift_value => 0,        // range 0..31 (or 0..63 for size==8)
                          size                => 4);       // 4 or 8
            break;

          case MEMORY:
            flush_uint1_in_register (ref *i2);

            c_or_reg_reg (target              => i1->reg,  // ZERO allowed (SP not allowed)
                          source1             => i1->reg,  // ZERO allowed (SP not allowed)
                          source2             => i2->reg,  // ZERO allowed (SP not allowed)
                          source2_shift_type  => LSL,      // LSL, LSR, ASR
                          source2_shift_value => 0,        // range 0..31 (or 0..63 for size==8)
                          size                => 4);       // 4 or 8
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_AND_BOOL:   // ( bool  bool --> bool )
      {
        NODE* i2 = &istack[istack_count - 1];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)
        NODE* i1 = &istack[istack_count - 2];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)

        if (g_tracing)
          trace ("\n");

        // prefer smallest register nr as first, or constant as second
        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_uint1_in_register_for_modif (ref *i1);      // flush into byte-aligned register

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
            c_and_reg_reg (target              => i1->reg,  // ZERO allowed (SP not allowed)
                           source1             => i1->reg,  // ZERO allowed (SP not allowed)
                           source2             => i2->reg,  // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,      // LSL, LSR, ASR
                           source2_shift_value => 0,        // range 0..31 (or 0..63 for size==8)
                           size                => 4);       // 4 or 8
            break;

          case MEMORY:
            flush_uint1_in_register (ref *i2);

            c_and_reg_reg (target              => i1->reg,  // ZERO allowed (SP not allowed)
                           source1             => i1->reg,  // ZERO allowed (SP not allowed)
                           source2             => i2->reg,  // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,      // LSL, LSR, ASR
                           source2_shift_value => 0,        // range 0..31 (or 0..63 for size==8)
                           size                => 4);       // 4 or 8
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_XOR_BOOL:   // ( bool  bool --> bool )
      {
        NODE* i2 = &istack[istack_count - 1];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)
        NODE* i1 = &istack[istack_count - 2];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)

        if (g_tracing)
          trace ("\n");

        // prefer smallest register nr as first, or constant as second
        if ((i1->kind != INT_REGISTER && i2->kind == INT_REGISTER) ||
            (i1->kind == INT_REGISTER && i2->kind == INT_REGISTER && i2->reg < i1->reg) ||
            i1->kind == INT_CONSTANT)
        {
          swap_nodes (ref *i1, ref *i2);
        }

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_uint1_in_register_for_modif (ref *i1);      // flush into byte-aligned register

        switch (i2->kind)
        {
          case INT_CONSTANT:
            if ((int)i2->icte == 0)   // "xor 0" has no effect
            {
              // leave first operand
            }
            else
            {
              assert c_eor_reg_imm (target => i1->reg,     // SP allowed (ZERO not allowed)
                                    source => i1->reg,     // ZERO allowed (SP not allowed)
                                    imm    => 1,
                                    size   => 4);
            }
            break;

          case INT_REGISTER:
            c_eor_reg_reg (target              => i1->reg,  // ZERO allowed (SP not allowed)
                           source1             => i1->reg,  // ZERO allowed (SP not allowed)
                           source2             => i2->reg,  // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,      // LSL, LSR, ASR
                           source2_shift_value => 0,        // range 0..31 (or 0..63 for size==8)
                           size                => 4);       // 4 or 8
            break;

          case MEMORY:
            flush_uint1_in_register (ref *i2);

            c_eor_reg_reg (target              => i1->reg,  // ZERO allowed (SP not allowed)
                           source1             => i1->reg,  // ZERO allowed (SP not allowed)
                           source2             => i2->reg,  // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,      // LSL, LSR, ASR
                           source2_shift_value => 0,        // range 0..31 (or 0..63 for size==8)
                           size                => 4);       // 4 or 8
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
          flush_int4_in_register (ref *i);

          {
            REG r = allocate_register (*i);

            // NEG (shifted register)
            c_neg_reg_reg (target => r,       // ZERO allowed (SP not allowed)
                           source => i->reg,  // ZERO allowed (SP not allowed)
                           size   => 4);      // 4 or 8

            i->reg = r;
          }
        }
      }
      break;


      case P_NOT4:   // ( int4 --> int4 )
      {
        NODE* i = &istack[istack_count - 1];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)

        if (g_tracing)
          trace ("\n");

        if (i->kind == INT_CONSTANT)
        {
          i->icte = ~i->icte;   // toggles all bits
        }
        else
        {
          flush_int4_in_register (ref *i);

          {
            REG r = allocate_register (*i);

            c_not_reg_reg (target    => r,        // ZERO allowed (SP not allowed)
                           source    => i->reg,   // ZERO allowed (SP not allowed)
                           data_size => 4);       // 4 or 8

            i->reg = r;
          }
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
        flush_int4_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int icte = (int)i2->icte;
          if (icte == 0)  // "+ 0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            REG r = allocate_register (*i1);

            // generates 1 to 3 instructions
            add_offset_using_x17 (target => r,        // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => i1->reg,  // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => icte,
                                  size   => 4);       // 4 or 8
            i1->reg = r;
          }
        }
        else  // INT_REGISTER or MEMORY
        {
          flush_int4_in_register (ref *i2);

          {
            REG r = allocate_register (*i1);

            c_add_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                           source1             => i1->reg, // ZERO allowed (SP not allowed)
                           source2             => i2->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,     // LSL, LSR, ASR
                           source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                           size                => 4);      // 4 or 8
            i1->reg = r;
          }
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
        flush_int4_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int icte = (int)i2->icte;
          if (icte == 0)  // "- 0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            REG r = allocate_register (*i1);

            // generates 1 to 3 instructions
            add_offset_using_x17 (target => r,        // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => i1->reg,  // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => -icte,
                                  size   => 4);       // 4 or 8
            i1->reg = r;
          }
        }
        else  // INT_REGISTER or MEMORY
        {
          flush_int4_in_register (ref *i2);

          {
            REG r = allocate_register (*i1);

            c_sub_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                           source1             => i1->reg, // ZERO allowed (SP not allowed)
                           source2             => i2->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,     // LSL, LSR, ASR
                           source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                           size                => 4);      // 4 or 8
            i1->reg = r;
          }
        }

        istack_count--;
      }
      break;


      case P_SMUL4:   //  ( int4  int4   --> int4 )
      case P_UMUL4:   //  ( uint4  uint4   --> uint4 )
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
        flush_int4_in_register (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int icte = (int)i2->icte;

              if (icte == -1 && g_current_pcode == P_SMUL4)  // negate
              {
                REG r = allocate_register (*i1);

                c_neg_reg_reg (target => r,        // ZERO allowed (SP not allowed)
                               source => i1->reg,  // ZERO allowed (SP not allowed)
                               size   => 4);       // 4 or 8

                i1->reg = r;
              }
              else if (icte == 0)  // "* 0" yields zero
              {
                i1->kind = INT_CONSTANT;
                i1->icte = 0;
              }
              else if (icte == 1)  // "* 1" has no effect
              {
                // leave first register operand unchanged
              }
              else if (icte >= 2 && icte <= 2_000_000_000 && (icte & (icte-1)) == 0)  // power of 2
              {
                REG r = allocate_register (*i1);

                c_add_reg_reg (target              => r,                       // ZERO allowed (SP not allowed)
                               source1             => ZERO,                    // ZERO allowed (SP not allowed)
                               source2             => i1->reg,                 // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,                     // LSL, LSR, ASR
                               source2_shift_value => (uint)lshifts_of(icte),  // range 0..31 (or 0..63 for size==8)
                               size                => 4);                      // 4 or 8
                i1->reg = r;
              }
              else if (icte >= 3 && icte <= 2_000_000_000 && ((icte-1) & (icte-2)) == 0)  // power of 2 + 1
              {
                REG r = allocate_register (*i1);

                c_add_reg_reg (target              => r,                         // ZERO allowed (SP not allowed)
                               source1             => i1->reg,                   // ZERO allowed (SP not allowed)
                               source2             => i1->reg,                   // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,                       // LSL, LSR, ASR
                               source2_shift_value => (uint)lshifts_of(icte-1),  // range 0..31 (or 0..63 for size==8)
                               size                => 4);                        // 4 or 8
                i1->reg = r;
              }
              else      // general case
              {
                REG r = allocate_register (*i1);

                move_register_immediate (target => X19,  imm => icte,  size => 4);

                c_mult (target    => r,        // ZERO allowed
                        mul1      => i1->reg,  // ZERO allowed
                        mul2      => X19,      // ZERO allowed
                        data_size => 4);       // 4 or 8

                i1->reg = r;
              }
            }
            break;

          case INT_REGISTER:
            {
              REG r = allocate_register (*i1);
              c_mult (target    => r,        // ZERO allowed
                      mul1      => i1->reg,  // ZERO allowed
                      mul2      => i2->reg,  // ZERO allowed
                      data_size => 4);       // 4 or 8
              i1->reg = r;
            }
            break;

          case MEMORY:
            flush_int4_in_register (ref *i2);
            {
              REG r = allocate_register (*i1);
              c_mult (target    => r,        // ZERO allowed
                      mul1      => i1->reg,  // ZERO allowed
                      mul2      => i2->reg,  // ZERO allowed
                      data_size => 4);       // 4 or 8
              i1->reg = r;
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_SDIV4:   //  ( int4   int4   -->  int4 )
      case P_UDIV4:   //  ( uint4  uint4  -->  uint4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        flush_int4_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int icte = (int)i2->icte;

          if (icte == -1 && g_current_pcode == P_SDIV4)  // div -1
          {
            REG r = allocate_register (*i1);

            c_neg_reg_reg (target => r,        // ZERO allowed (SP not allowed)
                           source => i1->reg,  // ZERO allowed (SP not allowed)
                           size   => 4);       // 4 or 8

            i1->reg = r;
          }
          else if (icte == 1)  // div 1
          {
            // has no effect
          }
          else
          {
            REG r;

            flush_int4_in_register (ref *i2);

            r = allocate_register (*i1);

            c_div (target    => r,        // ZERO allowed (SP not allowed)
                   source1   => i1->reg,  // ZERO allowed (SP not allowed)
                   source2   => i2->reg,
                   signed    => (g_current_pcode == P_SDIV4),
                   data_size => 4);       // 4 or 8

            i1->reg = r;
          }
        }
        else
        {
          REG r;

          flush_int4_in_register (ref *i2);

          r = allocate_register (*i1);

          c_div (target    => r,        // ZERO allowed (SP not allowed)
                 source1   => i1->reg,  // ZERO allowed (SP not allowed)
                 source2   => i2->reg,
                 signed    => (g_current_pcode == P_SDIV4),
                 data_size => 4);       // 4 or 8

          i1->reg = r;
        }

        istack_count--;
      }
      break;



      case P_SMOD4:   //  ( int4   int4   -->  int4 )
      case P_UMOD4:   //  ( uint4  uint4  -->  uint4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT && (i2->icte == +1 || i2->icte == -1))
        {
          // ex: a % 1 = 0
          // ex: a % -1 = 0
          i1->kind = INT_CONSTANT;
          i1->icte = 0;
        }
        else
        {
          REG r;

          flush_int4_in_register (ref *i2);

          r = allocate_register ();

          // a % b  = a - (a / b * b)

          c_div (target    => r,        // ZERO allowed (SP not allowed)
                 source1   => i1->reg,  // ZERO allowed (SP not allowed)
                 source2   => i2->reg,
                 signed    => (g_current_pcode == P_SMOD4),
                 data_size => 4);       // 4 or 8

          // t = term - mul1 x mul2
          c_mult_sub (target    => i1->reg,   // ZERO allowed
                      term      => i1->reg,   // ZERO allowed
                      mul1      => r,         // ZERO allowed
                      mul2      => i2->reg,   // ZERO allowed
                      data_size => 4);        // 4 or 8
        }

        istack_count--;
      }
      break;


      case P_BITAND4:   //  ( int4  int4   --> int4 )
      case P_BITOR4:    //  ( int4  int4   --> int4 )
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
        flush_int4_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int  icte = (int)i2->icte;
          bool done = false;

          switch (g_current_pcode)
          {
            case P_BITAND4:
              if (icte == -1)  // "& 0xFFFFFFFF" has no effect
              {
                // leave first register operand unchanged
                done = true;
              }
              else if (icte == 0)    // "& 0" yields 0
              {
                i1->kind = INT_CONSTANT;
                i1->icte = 0;
                done = true;
              }
              break;

            case P_BITOR4:
              if (icte == -1)  // "or 0xFFFFFFFF" yields -1
              {
                i1->kind = INT_CONSTANT;
                i1->icte = -1;
                done = true;
              }
              else if (icte == 0)    // "| 0" has no effect
              {
                // leave first register operand unchanged
                done = true;
              }
              break;

            case P_BITXOR4:
              if (icte == -1)    // "xor FFFFFFFF"
              {
                REG r = allocate_register (*i1);

                c_not_reg_reg (target    => r,        // ZERO allowed (SP not allowed)
                               source    => i1->reg,  // ZERO allowed (SP not allowed)
                               data_size => 4);       // 4 or 8
                i1->reg = r;
                done = true;
              }
              else if (icte == 0)    // "xor 0" has no effect
              {
                // leave first register operand unchanged
                done = true;
              }
              break;

            default:
              abort;
          }

          if (!done)
          {
            REG  r = allocate_register (*i1);
            bool b;

            switch (g_current_pcode)
            {
              case P_BITAND4:
                b = c_and_reg_imm (target => r,               // SP allowed (ZERO not allowed)
                                   source => i1->reg,         // ZERO allowed (SP not allowed)
                                   imm    => (int)i2->icte,
                                   size   => 4);              // 4 or 8
                break;

              case P_BITOR4:
                b = c_or_reg_imm (target => r,               // SP allowed (ZERO not allowed)
                                  source => i1->reg,         // ZERO allowed (SP not allowed)
                                  imm    => (int)i2->icte,
                                  size   => 4);              // 4 or 8
                break;

              case P_BITXOR4:
                b = c_eor_reg_imm (target => r,               // SP allowed (ZERO not allowed)
                                   source => i1->reg,         // ZERO allowed (SP not allowed)
                                   imm    => (int)i2->icte,
                                   size   => 4);              // 4 or 8
                break;

              default:
                abort;
            }

            if (!b)
            {
              int imm, s;

              imm = (int)i2->icte;

              for (s=31; (imm & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
                ;

              imm >>= s;

              move_register_immediate (target => X19,
                                       imm    => imm,
                                       size   => 4);   // 4 or 8

              switch (g_current_pcode)
              {
                case P_BITAND4:
                  c_and_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                                 source1             => i1->reg, // ZERO allowed (SP not allowed)
                                 source2             => X19,     // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,     // LSL, LSR, ASR
                                 source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                                 size                => 4);      // 4 or 8
                  break;

                case P_BITOR4:
                  c_or_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                                source1             => i1->reg, // ZERO allowed (SP not allowed)
                                source2             => X19,     // ZERO allowed (SP not allowed)
                                source2_shift_type  => LSL,     // LSL, LSR, ASR
                                source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                                size                => 4);      // 4 or 8
                  break;

                case P_BITXOR4:
                  c_eor_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                                 source1             => i1->reg, // ZERO allowed (SP not allowed)
                                 source2             => X19,     // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,     // LSL, LSR, ASR
                                 source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                                 size                => 4);      // 4 or 8
                  break;

                default:
                  abort;
              }
            }

            i1->reg = r;
          }
        }
        else
        {
          REG r;

          flush_int4_in_register (ref *i2);

          r = allocate_register (*i1);

          switch (g_current_pcode)
          {
            case P_BITAND4:
              c_and_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                             source1             => i1->reg, // ZERO allowed (SP not allowed)
                             source2             => i2->reg, // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,     // LSL, LSR, ASR
                             source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                             size                => 4);      // 4 or 8
              break;

            case P_BITOR4:
              c_or_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                            source1             => i1->reg, // ZERO allowed (SP not allowed)
                            source2             => i2->reg, // ZERO allowed (SP not allowed)
                            source2_shift_type  => LSL,     // LSL, LSR, ASR
                            source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                            size                => 4);      // 4 or 8
              break;

            case P_BITXOR4:
              c_eor_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                             source1             => i1->reg, // ZERO allowed (SP not allowed)
                             source2             => i2->reg, // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,     // LSL, LSR, ASR
                             source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                             size                => 4);      // 4 or 8
              break;

            default:
              abort;
          }

          i1->reg = r;
        }

        istack_count--;
      }
      break;


      case P_ASL4:   //  ( int4  int4   --> int4 )
      case P_SHL4:   //  ( uint4  uint4  --> uint4 )
      case P_ASR4:   //  ( int4  int4   --> int4 )
      case P_SHR4:   //  ( uint4  uint4  --> uint4 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int4_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          if (i2->icte != 0)  // 0 has no effect
          {
            REG r = allocate_register (*i1);

            switch (g_current_pcode)
            {
              case P_ASL4:  // signed shift left
              case P_SHL4:  // unsigned shift left
                if (((int)i2->icte & 31) != 0)
                {
                  c_lsl_imm (target    => r,                  // ZERO allowed
                             source    => i1->reg,            // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i2->icte & 31, // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;


              case P_ASR4:  // signed shift right
                if (((int)i2->icte & 31) != 0)
                {
                  c_asr_imm (target    => r,                  // ZERO allowed
                             source    => i1->reg,            // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i2->icte & 31, // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              case P_SHR4:  // unsigned shift right
                if (((int)i2->icte & 31) != 0)
                {
                  c_lsr_imm (target    => r,                  // ZERO allowed
                             source    => i1->reg,            // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i2->icte & 31, // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              default:
                abort;
            }

            i1->reg = r;
          }
        }
        else
        {
          REG r;

          flush_int4_in_register (ref *i2);

          r = allocate_register (*i1);

          switch (g_current_pcode)
          {
            case P_ASL4:  // signed shift left    <<
            case P_SHL4:  // unsigned shift left  <<
              c_lsl_reg (target    => r,        // ZERO allowed
                         source    => i1->reg,  // ZERO allowed (base effective address)(64 bit address)
                         shifts    => i2->reg,  // 0 to 63  (or 0 to 31 for size 4)
                         data_size => 4);       // 4 or 8
              break;

            case P_ASR4:  // signed shift right  >>
              c_asr_reg (target    => r,        // ZERO allowed
                         source    => i1->reg,  // ZERO allowed (base effective address)(64 bit address)
                         shifts    => i2->reg,  // 0 to 63  (or 0 to 31 for size 4)
                         data_size => 4);       // 4 or 8
              break;

            case P_SHR4:  // unsigned shift right >>
              c_lsr_reg (target    => r,        // ZERO allowed
                         source    => i1->reg,  // ZERO allowed (base effective address)(64 bit address)
                         shifts    => i2->reg,  // 0 to 63  (or 0 to 31 for size 4)
                         data_size => 4);       // 4 or 8
              break;

            default:
              abort;
          }

          i1->reg = r;
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

        if (i->kind == INT_CONSTANT)
        {
          i->icte = - i->icte;
        }
        else
        {
          flush_int8_in_register (ref *i);

          {
            REG r = allocate_register (*i);

            // NEG (shifted register)
            c_neg_reg_reg (target => r,       // ZERO allowed (SP not allowed)
                           source => i->reg,  // ZERO allowed (SP not allowed)
                           size   => 8);      // 4 or 8

            i->reg = r;
          }
        }
      }
      break;


      case P_NOT8:   // ( int8 --> int8 )
      {
        NODE* i = &istack[istack_count - 1];       // kind can be (INT_CONSTANT, INT_REGISTER, MEMORY)

        if (g_tracing)
          trace ("\n");

        if (i->kind == INT_CONSTANT)
        {
          i->icte = ~i->icte;   // toggles all bits
        }
        else
        {
          flush_int8_in_register (ref *i);

          {
            REG r = allocate_register (*i);

            c_not_reg_reg (target    => r,        // ZERO allowed (SP not allowed)
                           source    => i->reg,   // ZERO allowed (SP not allowed)
                           data_size => 8);       // 4 or 8

            i->reg = r;
          }
        }
      }
      break;


      case P_ADD8:   //  ( int8  int8   --> int8 )
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
        flush_int8_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int8 icte = i2->icte;
          if (icte == 0)  // "+ 0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            REG r = allocate_register (crash_node => *i1);

            // generates 1 to 3 instructions
            add_offset_using_x17 (target => r,        // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => i1->reg,  // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => icte,
                                  size   => 8);       // 4 or 8
            i1->reg = r;
          }
        }
        else  // INT_REGISTER or MEMORY
        {
          flush_int8_in_register (ref *i2);

          {
            REG r = allocate_register (crash_node => *i1);

            c_add_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                           source1             => i1->reg, // ZERO allowed (SP not allowed)
                           source2             => i2->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,     // LSL, LSR, ASR
                           source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                           size                => 8);      // 4 or 8
            i1->reg = r;
          }
        }

        istack_count--;
      }
      break;


      case P_SUB8:   //  ( int8  int8   --> int8 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int8 icte = i2->icte;
          if (icte == 0)  // "- 0" has no effect
          {
            // leave first register operand unchanged
          }
          else
          {
            REG r = allocate_register (crash_node => *i1);

            // generates 1 to 3 instructions
            add_offset_using_x17 (target => r,        // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => i1->reg,  // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => -icte,
                                  size   => 8);       // 4 or 8
            i1->reg = r;
          }
        }
        else  // INT_REGISTER or MEMORY
        {
          flush_int8_in_register (ref *i2);

          {
            REG r = allocate_register (crash_node => *i1);

            c_sub_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                           source1             => i1->reg, // ZERO allowed (SP not allowed)
                           source2             => i2->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,     // LSL, LSR, ASR
                           source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                           size                => 8);      // 4 or 8
            i1->reg = r;
          }
        }

        istack_count--;
      }
      break;


      case P_SMUL8:   //  ( int8  int8   --> int8 )
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
        flush_int8_in_register (ref *i1);

        switch (i2->kind)
        {
          case INT_CONSTANT:
            {
              int8 icte = i2->icte;

              if (icte == -1)  // negate
              {
                REG r = allocate_register (*i1);

                c_neg_reg_reg (target => r,        // ZERO allowed (SP not allowed)
                               source => i1->reg,  // ZERO allowed (SP not allowed)
                               size   => 8);       // 4 or 8

                i1->reg = r;
              }
              else if (icte == 0)  // "* 0" yields zero
              {
                i1->kind = INT_CONSTANT;
                i1->icte = 0;
              }
              else if (icte == 1)  // "* 1" has no effect
              {
                // leave first register operand unchanged
              }
              else if (icte >= 2 && (icte & (icte-1)) == 0)  // power of 2
              {
                REG r = allocate_register (*i1);

                c_add_reg_reg (target              => r,                       // ZERO allowed (SP not allowed)
                               source1             => ZERO,                    // ZERO allowed (SP not allowed)
                               source2             => i1->reg,                 // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,                     // LSL, LSR, ASR
                               source2_shift_value => (uint)lshifts_of(icte),  // range 0..31 (or 0..63 for size==8)
                               size                => 8);                      // 4 or 8
                i1->reg = r;
              }
              else if (icte >= 3 && ((icte-1) & (icte-2)) == 0)  // power of 2 + 1
              {
                REG r = allocate_register (*i1);

                c_add_reg_reg (target              => r,                         // ZERO allowed (SP not allowed)
                               source1             => i1->reg,                   // ZERO allowed (SP not allowed)
                               source2             => i1->reg,                   // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,                       // LSL, LSR, ASR
                               source2_shift_value => (uint)lshifts_of(icte-1),  // range 0..31 (or 0..63 for size==8)
                               size                => 8);                        // 4 or 8
                i1->reg = r;
              }
              else      // general case
              {
                REG r = allocate_register (*i1);

                move_register_immediate (target => X19,  imm => icte,  size => 8);

                c_mult (target    => r,        // ZERO allowed
                        mul1      => i1->reg,  // ZERO allowed
                        mul2      => X19,      // ZERO allowed
                        data_size => 8);       // 4 or 8

                i1->reg = r;
              }
            }
            break;

          case INT_REGISTER:
            {
              REG r = allocate_register (*i1);
              c_mult (target    => r,        // ZERO allowed
                      mul1      => i1->reg,  // ZERO allowed
                      mul2      => i2->reg,  // ZERO allowed
                      data_size => 8);       // 4 or 8
              i1->reg = r;
            }
            break;

          case MEMORY:
            flush_int8_in_register (ref *i2);
            {
              REG r = allocate_register (*i1);
              c_mult (target    => r,        // ZERO allowed
                      mul1      => i1->reg,  // ZERO allowed
                      mul2      => i2->reg,  // ZERO allowed
                      data_size => 8);       // 4 or 8
              i1->reg = r;
            }
            break;

          default:
            abort;
        }

        istack_count--;
      }
      break;


      case P_SDIV8:   //  ( int8   int8   -->  int8 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.

        flush_int8_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int8 icte = i2->icte;

          if (icte == -1)  // div -1
          {
            REG r = allocate_register (*i1);

            c_neg_reg_reg (target => r,        // ZERO allowed (SP not allowed)
                           source => i1->reg,  // ZERO allowed (SP not allowed)
                           size   => 8);       // 4 or 8

            i1->reg = r;
          }
          else if (icte == 1)  // div 1
          {
            // has no effect
          }
          else
          {
            REG r;

            flush_int8_in_register (ref *i2);

            r = allocate_register (*i1);

            c_div (target    => r,        // ZERO allowed (SP not allowed)
                   source1   => i1->reg,  // ZERO allowed (SP not allowed)
                   source2   => i2->reg,
                   signed    => true,
                   data_size => 8);       // 4 or 8

            i1->reg = r;
          }
        }
        else
        {
          REG r;

          flush_int8_in_register (ref *i2);

          r = allocate_register (*i1);

          c_div (target    => r,        // ZERO allowed (SP not allowed)
                 source1   => i1->reg,  // ZERO allowed (SP not allowed)
                 source2   => i2->reg,
                 signed    => true,
                 data_size => 8);       // 4 or 8

          i1->reg = r;
        }

        istack_count--;
      }
      break;



      case P_SMOD8:   //  ( int8   int8   -->  int8 )
      {
        NODE* i2 = &istack[istack_count - 1];
        NODE* i1 = &istack[istack_count - 2];

        if (g_tracing)
          trace ("\n");

        // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
        flush_int8_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT && (i2->icte == +1 || i2->icte == -1))
        {
          // ex: a % 1 = 0
          // ex: a % -1 = 0
          i1->kind = INT_CONSTANT;
          i1->icte = 0;
        }
        else
        {
          REG r;

          flush_int8_in_register (ref *i2);

          r = allocate_register ();

          // a % b  = a - (a / b * b)

          c_div (target    => r,        // ZERO allowed (SP not allowed)
                 source1   => i1->reg,  // ZERO allowed (SP not allowed)
                 source2   => i2->reg,
                 signed    => true,
                 data_size => 8);       // 4 or 8

          // t = term - mul1 x mul2
          c_mult_sub (target    => i1->reg,   // ZERO allowed
                      term      => i1->reg,   // ZERO allowed
                      mul1      => r,         // ZERO allowed
                      mul2      => i2->reg,   // ZERO allowed
                      data_size => 8);        // 4 or 8
        }

        istack_count--;
      }
      break;


      case P_BITAND8:   //  ( int8  int8   --> int8 )
      case P_BITOR8:    //  ( int8  int8   --> int8 )
      case P_BITXOR8:   //  ( int8  int8   --> int8 )
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
        flush_int8_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          int8 icte = i2->icte;
          bool done = false;

          switch (g_current_pcode)
          {
            case P_BITAND8:
              if (icte == -1)  // "& 0xFFFFFFFF" has no effect
              {
                // leave first register operand unchanged
                done = true;
              }
              else if (icte == 0)    // "& 0" yields 0
              {
                i1->kind = INT_CONSTANT;
                i1->icte = 0;
                done = true;
              }
              break;

            case P_BITOR8:
              if (icte == -1)  // "or 0xFFFFFFFF" yields -1
              {
                i1->kind = INT_CONSTANT;
                i1->icte = -1;
                done = true;
              }
              else if (icte == 0)    // "| 0" has no effect
              {
                // leave first register operand unchanged
                done = true;
              }
              break;

            case P_BITXOR8:
              if (icte == -1)    // "xor FFFFFFFF"
              {
                REG r = allocate_register (*i1);

                c_not_reg_reg (target    => r,        // ZERO allowed (SP not allowed)
                               source    => i1->reg,  // ZERO allowed (SP not allowed)
                               data_size => 8);       // 4 or 8
                i1->reg = r;
                done = true;
              }
              else if (icte == 0)    // "xor 0" has no effect
              {
                // leave first register operand unchanged
                done = true;
              }
              break;

            default:
              abort;
          }

          if (!done)
          {
            REG  r = allocate_register (*i1);
            bool b;

            switch (g_current_pcode)
            {
              case P_BITAND8:
                b = c_and_reg_imm (target => r,               // SP allowed (ZERO not allowed)
                                   source => i1->reg,         // ZERO allowed (SP not allowed)
                                   imm    => i2->icte,
                                   size   => 8);              // 4 or 8
                break;

              case P_BITOR8:
                b = c_or_reg_imm (target => r,               // SP allowed (ZERO not allowed)
                                  source => i1->reg,         // ZERO allowed (SP not allowed)
                                  imm    => i2->icte,
                                  size   => 8);              // 4 or 8
                break;

              case P_BITXOR8:
                b = c_eor_reg_imm (target => r,               // SP allowed (ZERO not allowed)
                                   source => i1->reg,         // ZERO allowed (SP not allowed)
                                   imm    => i2->icte,
                                   size   => 8);              // 4 or 8
                break;

              default:
                abort;
            }

            if (!b)
            {
              int8 imm;
              int  s;

              imm = i2->icte;

              for (s=63; (imm & ((1L<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
                ;

              imm >>= s;

              move_register_immediate (target => X19,
                                       imm    => imm,
                                       size   => 8);   // 4 or 8

              switch (g_current_pcode)
              {
                case P_BITAND8:
                  c_and_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                                 source1             => i1->reg, // ZERO allowed (SP not allowed)
                                 source2             => X19,     // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,     // LSL, LSR, ASR
                                 source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                                 size                => 8);      // 4 or 8
                  break;

                case P_BITOR8:
                  c_or_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                                source1             => i1->reg, // ZERO allowed (SP not allowed)
                                source2             => X19,     // ZERO allowed (SP not allowed)
                                source2_shift_type  => LSL,     // LSL, LSR, ASR
                                source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                                size                => 8);      // 4 or 8
                  break;

                case P_BITXOR8:
                  c_eor_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                                 source1             => i1->reg, // ZERO allowed (SP not allowed)
                                 source2             => X19,     // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,     // LSL, LSR, ASR
                                 source2_shift_value => (uint)s, // range 0..31 (or 0..63 for size==8)
                                 size                => 8);      // 4 or 8
                  break;

                default:
                  abort;
              }
            }

            i1->reg = r;
          }
        }
        else
        {
          REG r;

          flush_int8_in_register (ref *i2);

          r = allocate_register (*i1);

          switch (g_current_pcode)
          {
            case P_BITAND8:
              c_and_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                             source1             => i1->reg, // ZERO allowed (SP not allowed)
                             source2             => i2->reg, // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,     // LSL, LSR, ASR
                             source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                             size                => 8);      // 4 or 8
              break;

            case P_BITOR8:
              c_or_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                            source1             => i1->reg, // ZERO allowed (SP not allowed)
                            source2             => i2->reg, // ZERO allowed (SP not allowed)
                            source2_shift_type  => LSL,     // LSL, LSR, ASR
                            source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                            size                => 8);      // 4 or 8
              break;

            case P_BITXOR8:
              c_eor_reg_reg (target              => r,       // ZERO allowed (SP not allowed)
                             source1             => i1->reg, // ZERO allowed (SP not allowed)
                             source2             => i2->reg, // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,     // LSL, LSR, ASR
                             source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                             size                => 8);      // 4 or 8
              break;

            default:
              abort;
          }

          i1->reg = r;
        }

        istack_count--;
      }
      break;


      //  ( int8  int8   --> int8 )

      case P_ASL8:       // <near_label_nr>      ;  <<
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
        flush_int8_in_register (ref *i1);

        if (i2->kind == INT_CONSTANT)
        {
          if (i2->icte != 0)  // 0 has no effect
          {
            REG r;

            // note: we always evaluate operand1 into a register because any tombstone to a memory operand is released after this pcode.
            flush_int8_in_register (ref *i1);

            r = allocate_register (*i1);

            switch (g_current_pcode)
            {
              case P_ASL8:  // signed shift left
                if (((int)i2->icte & 63) != 0)
                {
                  c_lsl_imm (target    => r,                  // ZERO allowed
                             source    => i1->reg,            // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i2->icte & 63, // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 8);                 // 4 or 8
                }
                break;


              case P_ASR8:  // signed shift right
                if (((int)i2->icte & 63) != 0)
                {
                  c_asr_imm (target    => r,                  // ZERO allowed
                             source    => i1->reg,            // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i2->icte & 63, // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 8);                 // 4 or 8
                }
                break;

              default:
                abort;
            }

            i1->reg = r;
          }
        }
        else
        {
          REG r;

          flush_int8_in_register (ref *i2);

          r = allocate_register (*i1);

          switch (g_current_pcode)
          {
            case P_ASL8:  // signed shift left
              c_lsl_reg (target    => r,        // ZERO allowed
                         source    => i1->reg,  // ZERO allowed (base effective address)(64 bit address)
                         shifts    => i2->reg,  // 0 to 63  (or 0 to 31 for size 4)
                         data_size => 8);       // 4 or 8
              break;


            case P_ASR8:  // signed shift right
              c_asr_reg (target    => r,        // ZERO allowed
                         source    => i1->reg,  // ZERO allowed (base effective address)(64 bit address)
                         shifts    => i2->reg,  // 0 to 63  (or 0 to 31 for size 4)
                         data_size => 8);       // 4 or 8
              break;

            default:
              abort;
          }

          i1->reg = r;
        }

        istack_count--;
      }
      break;


  // float
  // -----

      case P_NEG_FLT4:      //  ( float4  -->  float4 )
      case P_NEG_FLT8:      //  ( float8  -->  float8 )
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace ("\n");

        if (f->kind == FLOAT_CONSTANT)
        {
          f->fcte = - f->fcte;
        }
        else
        {
          int size;

          if (g_current_pcode == P_NEG_FLT4)
          {
            flush_float4_in_register (ref *f);
            size = 4;
          }
          else
          {
            flush_float8_in_register (ref *f);
            size = 8;
          }

          {
            FREG r = allocate_fregister (*f);

            c_fneg (target => r,
                    source => f->freg,
                    size   => size);      // 4 or 8

            f->freg = r;
          }
        }
      }
      break;


      case P_ADD_FLT4:   //  ( float4  float4  -->  float4 )
      case P_ADD_FLT8:   //  ( float8  float8  -->  float8 )
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER) ||
            (f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_REGISTER && f2->freg < f1->freg) ||
            f1->kind == FLOAT_CONSTANT)
        {
          swap_nodes (ref *f1, ref *f2);
        }

        if (f2->kind == FLOAT_CONSTANT && f2->fcte == 0.0)
          ;
        else
        {
          FREG r;
          int  size;

          if (g_current_pcode == P_ADD_FLT4)
          {
            flush_float4_in_register (ref *f1);
            flush_float4_in_register (ref *f2);
            size = 4;
          }
          else
          {
            flush_float8_in_register (ref *f1);
            flush_float8_in_register (ref *f2);
            size = 8;
          }

          r = allocate_fregister (*f1);

          c_fadd (target  => r,
                  source1 => f1->freg,
                  source2 => f2->freg,
                  size    => size);         // 4 or 8
        }

        fstack_count--;
      }
      break;




      case P_SUB_FLT4:   //  ( float4  float4  -->  float4 )
      case P_SUB_FLT8:   //  ( float8  float8  -->  float8 )
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");


        if (f2->kind == FLOAT_CONSTANT && f2->fcte == 0.0)
          ;
        else
        {
          FREG r;
          int  size;

          if (g_current_pcode == P_SUB_FLT4)
          {
            flush_float4_in_register (ref *f1);
            flush_float4_in_register (ref *f2);
            size = 4;
          }
          else
          {
            flush_float8_in_register (ref *f1);
            flush_float8_in_register (ref *f2);
            size = 8;
          }

          r = allocate_fregister (*f1);

          c_fsub (target  => r,
                  source1 => f1->freg,
                  source2 => f2->freg,
                  size    => size);         // 4 or 8
        }

        fstack_count--;
      }
      break;



      case P_MUL_FLT4:      //  ( float4  float4  -->  float4 )
      case P_MUL_FLT8:      //  ( float8  float8  -->  float8 )
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");

        if ((f1->kind != FLOAT_REGISTER && f2->kind == FLOAT_REGISTER) ||
            (f1->kind == FLOAT_REGISTER && f2->kind == FLOAT_REGISTER && f2->freg < f1->freg) ||
            f1->kind == FLOAT_CONSTANT)
        {
          swap_nodes (ref *f1, ref *f2);
        }

        if (f2->kind == FLOAT_CONSTANT && f2->fcte == 0.0)
        {
          f1->kind = FLOAT_CONSTANT;
          f1->fcte = 0.0;
        }
        else if (f2->kind == FLOAT_CONSTANT && f2->fcte == 1.0)
          ;
        else
        {
          FREG r;
          int  size;

          if (g_current_pcode == P_MUL_FLT4)
          {
            flush_float4_in_register (ref *f1);
            flush_float4_in_register (ref *f2);
            size = 4;
          }
          else
          {
            flush_float8_in_register (ref *f1);
            flush_float8_in_register (ref *f2);
            size = 8;
          }

          r = allocate_fregister (*f1);

          c_fmul (target  => r,
                  source1 => f1->freg,
                  source2 => f2->freg,
                  size    => size);         // 4 or 8
        }

        fstack_count--;
      }
      break;


      case P_DIV_FLT4:      //  ( float4  float4  -->  float4 )
      case P_DIV_FLT8:      //  ( float8  float8  -->  float8 )
      {
        NODE* f2 = &fstack[fstack_count - 1];
        NODE* f1 = &fstack[fstack_count - 2];

        if (g_tracing)
          trace ("\n");


        if (f2->kind == FLOAT_CONSTANT && f2->fcte == 1.0)
          ;
        else
        {
          FREG r;
          int  size;

          if (g_current_pcode == P_DIV_FLT4)
          {
            flush_float4_in_register (ref *f1);
            flush_float4_in_register (ref *f2);
            size = 4;
          }
          else
          {
            flush_float8_in_register (ref *f1);
            flush_float8_in_register (ref *f2);
            size = 8;
          }

          r = allocate_fregister (*f1);

          c_fdiv (target  => r,
                  source1 => f1->freg,
                  source2 => f2->freg,
                  size    => size);         // 4 or 8
        }

        fstack_count--;
      }
      break;


  // -------------------------
  // 6. pre/post dec/increment
  // -------------------------

      //  (  <addr>  -->  /  )

      case P_INC1:    // pop addr_stack, increment int1 at address
      case P_INC2:    // pop addr_stack, increment int2 at address
      case P_INC4:    // pop addr_stack, increment int4 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG r = allocate_register ();
          int size;

          if (g_current_pcode == P_INC1)
            size = 1;
          else if (g_current_pcode == P_INC2)
            size = 2;
          else
            size = 4;

          c_load_register_from_memory (r           => r,
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => size); // size = 1, 2, 4, 8

          c_add_reg_imm (target     => r,    // SP allowed (ZERO not allowed)
                         source     => r,    // SP allowed (ZERO not allowed)
                         imm12      => 1,     // 0 to 4095
                         shl_imm_12 => false, // true to shift imm12 << 12
                         size       => 4);    // 4 or 8

          c_store_register_in_memory (r     => r,
                                      ea    => a->ea,
                                      size  => size);    // size = 1, 2, 4, 8
        }

        astack_count--;
      }
      break;


      case P_INC8:    // pop addr_stack, increment int8 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG r = allocate_register ();

          c_load_register_from_memory (r           => r,
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => 8);   // size = 1, 2, 4, 8

          c_add_reg_imm (target     => r,    // SP allowed (ZERO not allowed)
                         source     => r,    // SP allowed (ZERO not allowed)
                         imm12      => 1,     // 0 to 4095
                         shl_imm_12 => false, // true to shift imm12 << 12
                         size       => 8);    // 4 or 8

          c_store_register_in_memory (r     => r,
                                      ea    => a->ea,
                                      size  => 8);    // size = 1, 2, 4, 8
        }

        astack_count--;
      }
      break;


      case P_DEC1:    // pop addr_stack, decrement int1 at address
      case P_DEC2:    // pop addr_stack, decrement int2 at address
      case P_DEC4:    // pop addr_stack, decrement int4 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG r = allocate_register ();
          int size;

          if (g_current_pcode == P_DEC1)
            size = 1;
          else if (g_current_pcode == P_DEC2)
            size = 2;
          else
            size = 4;

          c_load_register_from_memory (r           => r,
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => size); // size = 1, 2, 4, 8

          c_sub_reg_imm (target     => r,    // SP allowed (ZERO not allowed)
                         source     => r,    // SP allowed (ZERO not allowed)
                         imm12      => 1,     // 0 to 4095
                         shl_imm_12 => false, // true to shift imm12 << 12
                         size       => 4);    // 4 or 8

          c_store_register_in_memory (r     => r,
                                      ea    => a->ea,
                                      size  => size);    // size = 1, 2, 4, 8
        }

        astack_count--;
      }
      break;


      case P_DEC8:    // pop addr_stack, decrement int8 at address
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG r = allocate_register ();

          c_load_register_from_memory (r           => r,
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => 8);   // size = 1, 2, 4, 8

          c_sub_reg_imm (target     => r,    // SP allowed (ZERO not allowed)
                         source     => r,    // SP allowed (ZERO not allowed)
                         imm12      => 1,     // 0 to 4095
                         shl_imm_12 => false, // true to shift imm12 << 12
                         size       => 8);    // 4 or 8

          c_store_register_in_memory (r     => r,
                                      ea    => a->ea,
                                      size  => 8);    // size = 1, 2, 4, 8
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
      case P_OR_BOOL_TO:     // |=    (  addr  bool  -->   )
      case P_XOR_BOOL_TO:    // ^=    (  addr  bool  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => 1);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            switch (g_current_pcode)
            {
              case P_AND_BOOL_TO:
                if (i->icte == 0)
                {
                  c_mov_reg_reg (target    => reg[0],  // ZERO allowed (SP not allowed)
                                 source    => ZERO,    // ZERO allowed (SP not allowed)
                                 data_size => 4);      // 4 or 8
                }
                break;

              case P_OR_BOOL_TO:
                if (i->icte != 0)
                {
                  assert c_or_reg_imm (target => reg[0],  // SP allowed (ZERO not allowed)
                                       source => reg[0],  // ZERO allowed (SP not allowed)
                                       imm    => 1,
                                       size   => 4);      // 4 or 8
                }
                break;

              case P_XOR_BOOL_TO:
                if (i->icte != 0)
                {
                  assert c_eor_reg_imm (target => reg[0],  // SP allowed (ZERO not allowed)
                                        source => reg[0],  // ZERO allowed (SP not allowed)
                                        imm    => 1,
                                        size   => 4);      // 4 or 8
                }
                break;

              default:
                abort;
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => false, size => 1);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            switch (g_current_pcode)
            {
              case P_AND_BOOL_TO:
                c_and_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_OR_BOOL_TO:
                c_or_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                              source1             => reg[0],  // ZERO allowed (SP not allowed)
                              source2             => i->reg,  // ZERO allowed (SP not allowed)
                              source2_shift_type  => LSL,     // LSL, LSR, ASR
                              source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                              size                => 4);      // 4 or 8
                break;

              case P_XOR_BOOL_TO:
                c_eor_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              default:
                abort;
            }
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 1);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;



      // int4   (signed value on int_stack)
      // ----

      case P_ADD1_TO:    // +=     (  addr  int4  -->   )
      case P_SUB1_TO:    // -=     (  addr  int4  -->   )
      case P_AND1_TO:    // |=     (  addr  int4  -->   )
      case P_OR1_TO:     // |=     (  addr  int4  -->   )
      case P_XOR1_TO:    // ^=     (  addr  int4  -->   )
      case P_ASL1_TO:    // <<=  (signed)     (  addr  int4   -->   )
      case P_SHL1_TO:    // <<=  (unsigned)   (  addr  uint4  -->   )
      case P_ASR1_TO:    // >>=  (signed)     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => true,
                                       size        => 1);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            switch (g_current_pcode)
            {
              case P_ADD1_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => i->icte,
                                      size   => 4);        // 4 or 8
                break;

              case P_SUB1_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => -i->icte,
                                      size   => 4);        // 4 or 8
                break;

              case P_AND1_TO:
                if (!c_and_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_and_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 4);     // 4 or 8
                }
                break;

              case P_OR1_TO:
                if (!c_or_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                   source => reg[0],         // ZERO allowed (SP not allowed)
                                   imm    => i->icte,
                                   size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_or_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                source1             => reg[0], // ZERO allowed (SP not allowed)
                                source2             => X19,    // ZERO allowed (SP not allowed)
                                source2_shift_type  => LSL,    // LSL, LSR, ASR
                                source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                size                => 4);     // 4 or 8
                }
                break;

              case P_XOR1_TO:
                if (!c_eor_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_eor_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 4);     // 4 or 8
                }
                break;

              case P_ASL1_TO:  // signed shift left
              case P_SHL1_TO:  // unsigned shift left
                if (((int)i->icte & 31) != 0)
                {
                  c_lsl_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              case P_ASR1_TO: // signed shift right
                if (((int)i->icte & 31) != 0)
                {
                  c_asr_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              default:
                abort;
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => true, size => 1);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            switch (g_current_pcode)
            {
              case P_ADD1_TO:
                c_add_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_SUB1_TO:
                c_sub_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_AND1_TO:
                c_and_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_OR1_TO:
                c_or_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                              source1             => reg[0],  // ZERO allowed (SP not allowed)
                              source2             => i->reg,  // ZERO allowed (SP not allowed)
                              source2_shift_type  => LSL,     // LSL, LSR, ASR
                              source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                              size                => 4);      // 4 or 8
                break;

              case P_XOR1_TO:
                c_eor_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_ASL1_TO:
              case P_SHL1_TO:
                c_lsl_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 4);      // 4 or 8
                break;

              case P_ASR1_TO:
                c_asr_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 4);      // 4 or 8
                break;

              default:
                abort;
            }
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 1);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_ADD2_TO:    // +=     (  addr  int4  -->   )
      case P_SUB2_TO:    // -=     (  addr  int4  -->   )
      case P_AND2_TO:    // &=     (  addr  int4  -->   )
      case P_OR2_TO:     // |=     (  addr  int4  -->   )
      case P_XOR2_TO:    // ^=     (  addr  int4  -->   )
      case P_ASL2_TO:    // <<=  (signed)     (  addr  int4   -->   )
      case P_SHL2_TO:    // <<=  (unsigned)   (  addr  uint4  -->   )
      case P_ASR2_TO:    // >>=  (signed)     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => true,
                                       size        => 2);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            switch (g_current_pcode)
            {
              case P_ADD2_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => i->icte,
                                      size   => 4);        // 4 or 8
                break;

              case P_SUB2_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => -i->icte,
                                      size   => 4);        // 4 or 8
                break;

              case P_AND2_TO:
                if (!c_and_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_and_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 4);     // 4 or 8
                }
                break;

              case P_OR2_TO:
                if (!c_or_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                   source => reg[0],         // ZERO allowed (SP not allowed)
                                   imm    => i->icte,
                                   size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_or_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                source1             => reg[0], // ZERO allowed (SP not allowed)
                                source2             => X19,    // ZERO allowed (SP not allowed)
                                source2_shift_type  => LSL,    // LSL, LSR, ASR
                                source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                size                => 4);     // 4 or 8
                }
                break;

              case P_XOR2_TO:
                if (!c_eor_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_eor_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 4);     // 4 or 8
                }
                break;

              case P_ASL2_TO:  // signed shift left
              case P_SHL2_TO:  // unsigned shift left
                if (((int)i->icte & 31) != 0)
                {
                  c_lsl_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              case P_ASR2_TO: // signed shift right
                if (((int)i->icte & 31) != 0)
                {
                  c_asr_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              default:
                abort;
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => true, size => 2);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            switch (g_current_pcode)
            {
              case P_ADD2_TO:
                c_add_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_SUB2_TO:
                c_sub_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_AND2_TO:
                c_and_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_OR2_TO:
                c_or_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                              source1             => reg[0],  // ZERO allowed (SP not allowed)
                              source2             => i->reg,  // ZERO allowed (SP not allowed)
                              source2_shift_type  => LSL,     // LSL, LSR, ASR
                              source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                              size                => 4);      // 4 or 8
                break;

              case P_XOR2_TO:
                c_eor_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_ASL2_TO:
              case P_SHL2_TO:
                c_lsl_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 4);      // 4 or 8
                break;

              case P_ASR2_TO:
                c_asr_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 4);      // 4 or 8
                break;

              default:
                abort;
            }
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 2);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_ADD4_TO:    // +=     (  addr  int4  -->   )
      case P_SUB4_TO:    // -=     (  addr  int4  -->   )
      case P_AND4_TO:    // &=     (  addr  int4  -->   )
      case P_OR4_TO:     // |=     (  addr  int4  -->   )
      case P_XOR4_TO:    // ^=     (  addr  int4  -->   )
      case P_ASL4_TO:    // <<=  (signed)     (  addr  int4   -->   )
      case P_SHL4_TO:    // <<=  (unsigned)   (  addr  uint4  -->   )
      case P_ASR4_TO:    // >>=  (signed)     (  addr  int4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => true,
                                       size        => 4);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            switch (g_current_pcode)
            {
              case P_ADD4_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => i->icte,
                                      size   => 4);        // 4 or 8
                break;

              case P_SUB4_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => -i->icte,
                                      size   => 4);        // 4 or 8
                break;

              case P_AND4_TO:
                if (!c_and_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_and_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 4);     // 4 or 8
                }
                break;

              case P_OR4_TO:
                if (!c_or_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                   source => reg[0],         // ZERO allowed (SP not allowed)
                                   imm    => i->icte,
                                   size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_or_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                source1             => reg[0], // ZERO allowed (SP not allowed)
                                source2             => X19,    // ZERO allowed (SP not allowed)
                                source2_shift_type  => LSL,    // LSL, LSR, ASR
                                source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                size                => 4);     // 4 or 8
                }
                break;

              case P_XOR4_TO:
                if (!c_eor_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 4))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 4);   // 4 or 8
                  c_eor_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 4);     // 4 or 8
                }
                break;

              case P_ASL4_TO:  // signed shift left
              case P_SHL4_TO:  // unsigned shift left
                if (((int)i->icte & 31) != 0)
                {
                  c_lsl_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              case P_ASR4_TO: // signed shift right
                if (((int)i->icte & 31) != 0)
                {
                  c_asr_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 4);                 // 4 or 8
                }
                break;

              default:
                abort;
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => true, size => 4);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            switch (g_current_pcode)
            {
              case P_ADD4_TO:
                c_add_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_SUB4_TO:
                c_sub_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_AND4_TO:
                c_and_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_OR4_TO:
                c_or_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                              source1             => reg[0],  // ZERO allowed (SP not allowed)
                              source2             => i->reg,  // ZERO allowed (SP not allowed)
                              source2_shift_type  => LSL,     // LSL, LSR, ASR
                              source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                              size                => 4);      // 4 or 8
                break;

              case P_XOR4_TO:
                c_eor_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 4);      // 4 or 8
                break;

              case P_ASL4_TO:
              case P_SHL4_TO:
                c_lsl_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 4);      // 4 or 8
                break;

              case P_ASR4_TO:
                c_asr_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 4);      // 4 or 8
                break;

              default:
                abort;
            }
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 4);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;


  // unsigned shift

      case P_SHR1_TO:  // >>=  (unsigned)     (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => 1);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            if (((int)i->icte & 31) != 0)
            {
              c_lsr_imm (target    => reg[0],             // ZERO allowed
                         source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                         shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                         data_size => 4);                 // 4 or 8
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => false, size => 1);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            c_lsr_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                       source     => reg[0],  // ZERO allowed (SP not allowed)
                       shifts     => i->reg,  // ZERO allowed (SP not allowed)
                       data_size  => 4);      // 4 or 8
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 1);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_SHR2_TO:  // >>=  (unsigned)     (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => 2);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            if (((int)i->icte & 31) != 0)
            {
              c_lsr_imm (target    => reg[0],             // ZERO allowed
                         source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                         shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                         data_size => 4);                 // 4 or 8
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => false, size => 2);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            c_lsr_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                       source     => reg[0],  // ZERO allowed (SP not allowed)
                       shifts     => i->reg,  // ZERO allowed (SP not allowed)
                       data_size  => 4);      // 4 or 8
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 2);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;


      case P_SHR4_TO:  // >>=  (unsigned)     (  addr  uint4  -->   )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => 4);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            if (((int)i->icte & 31) != 0)
            {
              c_lsr_imm (target    => reg[0],             // ZERO allowed
                         source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                         shifts    => (int)i->icte & 31,  // 1 to 63  (or 1 to 31 for size 4)
                         data_size => 4);                 // 4 or 8
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => false, size => 4);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            c_lsr_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                       source     => reg[0],  // ZERO allowed (SP not allowed)
                       shifts     => i->reg,  // ZERO allowed (SP not allowed)
                       data_size  => 4);      // 4 or 8
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 4);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;


      // int8
      // ----

      case P_ADD8_TO:    // +=     (  addr  int8  -->   )
      case P_SUB8_TO:    // -=     (  addr  int8  -->   )
      case P_AND8_TO:    // &=     (  addr  int8  -->   )
      case P_OR8_TO:     // |=     (  addr  int8  -->   )
      case P_XOR8_TO:    // ^=     (  addr  int8  -->   )
      case P_ASL8_TO:    // <<=  (signed)
      case P_ASR8_TO:    // >>=  (signed)
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];

        if (g_current_pcode == P_ASL8_TO ||
            g_current_pcode == P_ASR8_TO)
          mem_offset += 4;   // skip near label (used for intel 32 bit only)

        if (g_tracing)
          trace ("\n");

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG[2] reg;

          allocate_registers (out reg);

          // initial load
          c_load_register_from_memory (r           => reg[0],
                                       ea          => a->ea,
                                       data_signed => true,
                                       size        => 8);     // size = 1, 2, 4, 8
          // operate on reg[0] and i

          if (i->kind == INT_CONSTANT)
          {
            switch (g_current_pcode)
            {
              case P_ADD8_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => i->icte,
                                      size   => 8);        // 4 or 8
                break;

              case P_SUB8_TO:
                add_offset_using_x17 (target => reg[0],    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => reg[0],    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => -i->icte,
                                      size   => 8);        // 4 or 8
                break;

              case P_AND8_TO:
                if (!c_and_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 8))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 8);   // 4 or 8
                  c_and_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 8);     // 4 or 8
                }
                break;

              case P_OR8_TO:
                if (!c_or_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                   source => reg[0],         // ZERO allowed (SP not allowed)
                                   imm    => i->icte,
                                   size   => 8))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 8);   // 4 or 8
                  c_or_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                source1             => reg[0], // ZERO allowed (SP not allowed)
                                source2             => X19,    // ZERO allowed (SP not allowed)
                                source2_shift_type  => LSL,    // LSL, LSR, ASR
                                source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                size                => 8);     // 4 or 8
                }
                break;

              case P_XOR8_TO:
                if (!c_eor_reg_imm (target => reg[0],         // SP allowed (ZERO not allowed)
                                    source => reg[0],         // ZERO allowed (SP not allowed)
                                    imm    => i->icte,
                                    size   => 8))             // 4 or 8
                {
                  move_register_immediate (target => X19,  imm => i->icte, size => 8);   // 4 or 8
                  c_eor_reg_reg (target              => reg[0], // ZERO allowed (SP not allowed)
                                 source1             => reg[0], // ZERO allowed (SP not allowed)
                                 source2             => X19,    // ZERO allowed (SP not allowed)
                                 source2_shift_type  => LSL,    // LSL, LSR, ASR
                                 source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                                 size                => 8);     // 4 or 8
                }
                break;

              case P_ASL8_TO:  // signed shift left
                if (((int)i->icte & 63) != 0)
                {
                  c_lsl_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 63,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 8);                 // 4 or 8
                }
                break;

              case P_ASR8_TO: // signed shift right
                if (((int)i->icte & 63) != 0)
                {
                  c_asr_imm (target    => reg[0],             // ZERO allowed
                             source    => reg[0],             // ZERO allowed (base effective address)(64 bit address)
                             shifts    => (int)i->icte & 63,  // 1 to 63  (or 1 to 31 for size 4)
                             data_size => 8);                 // 4 or 8
                }
                break;

              default:
                abort;
            }
          }
          else    // i is memory or register
          {
            if (i->kind == MEMORY)  // load memory operand into register
            {
              c_load_register_from_memory (reg[1], i->ea, data_signed => true, size => 8);

              i->kind = INT_REGISTER;
              i->reg  = reg[1];
            }

            switch (g_current_pcode)
            {
              case P_ADD8_TO:
                c_add_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 8);      // 4 or 8
                break;

              case P_SUB8_TO:
                c_sub_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 8);      // 4 or 8
                break;

              case P_AND8_TO:
                c_and_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 8);      // 4 or 8
                break;

              case P_OR8_TO:
                c_or_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                              source1             => reg[0],  // ZERO allowed (SP not allowed)
                              source2             => i->reg,  // ZERO allowed (SP not allowed)
                              source2_shift_type  => LSL,     // LSL, LSR, ASR
                              source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                              size                => 8);      // 4 or 8
                break;

              case P_XOR8_TO:
                c_eor_reg_reg (target              => reg[0],  // ZERO allowed (SP not allowed)
                               source1             => reg[0],  // ZERO allowed (SP not allowed)
                               source2             => i->reg,  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,     // LSL, LSR, ASR
                               source2_shift_value => 0,       // range 0..31 (or 0..63 for size==8)
                               size                => 8);      // 4 or 8
                break;

              case P_ASL8_TO:    // <<=  (signed)
                c_lsl_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 8);      // 4 or 8
                break;

              case P_ASR8_TO:
                c_asr_reg (target     => reg[0],  // ZERO allowed (SP not allowed)
                           source     => reg[0],  // ZERO allowed (SP not allowed)
                           shifts     => i->reg,  // ZERO allowed (SP not allowed)
                           data_size  => 8);      // 4 or 8
                break;

              default:
                abort;
            }
          }

          // store back
          c_store_register_in_memory (r    => reg[0],
                                      ea   => a->ea,
                                      size => 8);      // size = 1, 2, 4, 8
        }

        astack_count--;
        istack_count--;
      }
      break;



      // ( addr  int4  -->   )

      case P_UNSAFE_ADD_TO:      //  <size4>   ;  address[addr] += signed int4 * size4;
      case P_UNSAFE_SUB_TO:      //  <size4>   ;  address[addr] -= signed int4 * size4;
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        if (size != 0)
        {
          if (i->kind == INT_CONSTANT)
          {
            if (i->icte != 0)
            {
              REG r = allocate_register();

              // initial load
              c_load_register_from_memory (r           => r,
                                           ea          => a->ea,
                                           data_signed => false,
                                           size        => address_size);     // size = 1, 2, 4, 8

              if (g_current_pcode == P_UNSAFE_ADD_TO)
              {
                add_offset_using_x17 (target => r,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => r,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => (int)(i->icte * size),
                                      size   => address_size);        // 4 or 8
              }
              else
              {
                add_offset_using_x17 (target => r,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                      source => r,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                      offset => -(int)(i->icte * size),
                                      size   => address_size);        // 4 or 8
              }

              // store back
              c_store_register_in_memory (r    => r,
                                          ea   => a->ea,
                                          size => address_size);    // size = 1, 2, 4, 8
            }
          }
          else
          {
            REG[2] reg;

            flush_int4_in_register (ref *i);    // allocates a register

            allocate_registers (out reg);    // allocates 2 registers

            // initial load
            c_load_register_from_memory (r           => reg[0],            // initial address value
                                         ea          => a->ea,
                                         data_signed => false,
                                         size        => address_size);     // size = 1, 2, 4, 8

            if (size == 1 || size == 2 || size == 4 || size == 8 || size == 16)
            {
              if (g_current_pcode == P_UNSAFE_ADD_TO)
              {
                c_add_ext_reg (target           => reg[0],                  // SP allowed (ZERO not allowed)
                               source1          => reg[0],                  // SP allowed (ZERO not allowed)
                               source2          => i->reg,                  // ZERO allowed (SP not allowed)
                               source2_size     => 4,                       // 1, 2, 4 or 8
                               source2_signed   => true,
                               source2_shl_imm  => (uint)lshifts_of (size), // 0 .. 4
                               size             => address_size);           // target size (4 or 8)
              }
              else
              {
                c_sub_ext_reg (target           => reg[0],                  // SP allowed (ZERO not allowed)
                               source1          => reg[0],                  // SP allowed (ZERO not allowed)
                               source2          => i->reg,                  // ZERO allowed (SP not allowed)
                               source2_size     => 4,                       // 1, 2, 4 or 8
                               source2_signed   => true,
                               source2_shl_imm  => (uint)lshifts_of (size), // 0 .. 4
                               size             => address_size);           // target size (4 or 8)
              }
            }
            else
            {
              if (size == 1)  // "* 1" has no effect
              {
                // leave i operand unchanged
              }
              else if (size >= 2 && size <= 2_000_000_000 && (size & (size-1)) == 0)  // power of 2
              {
                c_add_reg_reg (target              => reg[1],                  // ZERO allowed (SP not allowed)
                               source1             => ZERO,                    // ZERO allowed (SP not allowed)
                               source2             => i->reg,                  // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,                     // LSL, LSR, ASR
                               source2_shift_value => (uint)lshifts_of(size),  // range 0..31 (or 0..63 for size==8)
                               size                => 4);                      // 4 or 8
                i->reg = reg[1];
              }
              else if (size >= 3 && size <= 2_000_000_000 && ((size-1) & (size-2)) == 0)  // power of 2 + 1
              {
                c_add_reg_reg (target              => reg[1],                    // ZERO allowed (SP not allowed)
                               source1             => i->reg,                    // ZERO allowed (SP not allowed)
                               source2             => i->reg,                    // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,                       // LSL, LSR, ASR
                               source2_shift_value => (uint)lshifts_of(size-1),  // range 0..31 (or 0..63 for size==8)
                               size                => 4);                        // 4 or 8
                i->reg = reg[1];
              }
              else      // general case
              {
                move_register_immediate (target => X19,  imm => size,  size => 4);

                c_mult (target    => reg[1],   // ZERO allowed
                        mul1      => i->reg,   // ZERO allowed
                        mul2      => X19,      // ZERO allowed
                        data_size => 4);       // 4 or 8

                i->reg = reg[1];
              }

              if (g_current_pcode == P_UNSAFE_ADD_TO)
              {
                c_add_ext_reg (target           => reg[0],           // SP allowed (ZERO not allowed)
                               source1          => reg[0],           // SP allowed (ZERO not allowed)
                               source2          => i->reg,           // ZERO allowed (SP not allowed)
                               source2_size     => 4,                // 1, 2, 4 or 8
                               source2_signed   => true,
                               source2_shl_imm  => 0,                // 0 .. 4
                               size             => address_size);    // target size (4 or 8)
              }
              else
              {
                c_sub_ext_reg (target           => reg[0],           // SP allowed (ZERO not allowed)
                               source1          => reg[0],           // SP allowed (ZERO not allowed)
                               source2          => i->reg,           // ZERO allowed (SP not allowed)
                               source2_size     => 4,                // 1, 2, 4 or 8
                               source2_signed   => true,
                               source2_shl_imm  => 0,                // 0 .. 4
                               size             => address_size);    // target size (4 or 8)
              }
            }

            // store back
            c_store_register_in_memory (r    => reg[0],
                                        ea   => a->ea,
                                        size => address_size);    // size = 1, 2, 4, 8
          }
        }

        astack_count--;
        istack_count--;
      }
      break;


      // ( addr  -->   )

      case P_INC_ADDR:  // <size4>   ; pop addr_stack, increment addr at address by size
      case P_DEC_ADDR:  // <size4>   ; pop addr_stack, decrement addr at address by size
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        if (size != 0)
        {
          REG r = allocate_register();

          // initial load
          c_load_register_from_memory (r           => r,
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => address_size);     // size = 1, 2, 4, 8

          if (g_current_pcode == P_INC_ADDR)
          {
            add_offset_using_x17 (target => r,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => size,
                                  size   => address_size);        // 4 or 8
          }
          else
          {
            add_offset_using_x17 (target => r,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => -size,
                                  size   => address_size);        // 4 or 8
          }

          // store back
          c_store_register_in_memory (r    => r,
                                      ea   => a->ea,
                                      size => address_size);    // size = 1, 2, 4, 8
        }

        astack_count--;
      }
      break;


      // ( addr  -->  addr1 )

      case P_INC_VALUE_ADDR:  // <size4>    ;  example: ++p   (inc [addr] by size, then take addr1 at [addr])
      case P_DEC_VALUE_ADDR:  // <size4>    ;  example: --p
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG r = allocate_register();

          // initial load
          c_load_register_from_memory (r           => r,
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => address_size);     // size = 1, 2, 4, 8

          if (g_current_pcode == P_INC_VALUE_ADDR)
          {
            add_offset_using_x17 (target => r,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => size,
                                  size   => address_size);        // 4 or 8
          }
          else
          {
            add_offset_using_x17 (target => r,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => -size,
                                  size   => address_size);        // 4 or 8
          }

          // store back
          c_store_register_in_memory (r    => r,
                                      ea   => a->ea,
                                      size => address_size);    // size = 1, 2, 4, 8

          a->typ = 'a';
          a->kind = EFFECTIVE_ADDRESS;
          a->ea.base   = r;
          a->ea.index  = ZERO;
          a->ea.scale  = 1;
          a->ea.offset = 0;
          a->ea.reloc  = {RELOC_NONE, 0};
        }
      }
      break;


      // ( addr  -->  addr1 )

      case P_VALUE_ADDR_INC:   // <size4>    ;  example: p++   (take addr1 at [addr], then inc [addr] by size)
      case P_VALUE_ADDR_DEC:   // <size4>    ;  example: p--
      {
        NODE* a = &astack[astack_count - 1];
        int4 size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size=%d\n", size);

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        {
          REG r[2];

          allocate_registers (out r);

          // initial load
          c_load_register_from_memory (r           => r[0],
                                       ea          => a->ea,
                                       data_signed => false,
                                       size        => address_size);     // size = 1, 2, 4, 8


          if (g_current_pcode == P_VALUE_ADDR_INC)
          {
            add_offset_using_x17 (target => r[1],  // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r[0],  // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => size,
                                  size   => address_size);        // 4 or 8
          }
          else
          {
            add_offset_using_x17 (target => r[1],  // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                  source => r[0],  // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                  offset => -size,
                                  size   => address_size);        // 4 or 8
          }

          // store back
          c_store_register_in_memory (r    => r[1],
                                      ea   => a->ea,
                                      size => address_size);    // size = 1, 2, 4, 8

          a->typ = 'a';
          a->kind = EFFECTIVE_ADDRESS;
          a->ea.base   = r[0];
          a->ea.index  = ZERO;
          a->ea.scale  = 1;
          a->ea.offset = 0;
          a->ea.reloc  = {RELOC_NONE, 0};
        }
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
            c_extend_unsigned (target      => i->reg,       // 8 bytes
                               source      => i->reg,
                               source_size => 1);           // 1 or 2 bytes
            break;

          case MEMORY:
          {
            REG r = allocate_register (*i);    // allows reusing registers of node i

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 1); // size = 1, 2, 4, 8

            i->kind = INT_REGISTER;
            i->reg = r;
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
            break;

          case MEMORY:
            // will use lower byte of int in memory
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
            c_extend_signed (target      => i->reg,       // 8 bytes
                             source      => i->reg,
                             source_size => 4);           // 1, 2 or 4 bytes
            break;

          case MEMORY:
          {
            REG r = allocate_register (*i);    // allows reusing registers of node i

            c_load_register_from_memory (r, i->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8

            i->kind = INT_REGISTER;
            i->reg = r;
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
            c_mov_reg_reg (target    => i->reg,       // 8 bytes
                           source    => i->reg,
                           data_size => 4);           // 1, 2 or 4 bytes
            break;

          case MEMORY:
          {
            REG r = allocate_register (*i);    // allows reusing registers of node i

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8

            i->kind = INT_REGISTER;
            i->reg = r;
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
            i->icte = (int)i->icte;
            break;

          case INT_REGISTER:
            // will clear high 4 bytes of int8
            c_mov_reg_reg (target    => i->reg,     // ZERO allowed (SP not allowed)
                           source    => i->reg,     // ZERO allowed (SP not allowed)
                           data_size => 4);         // 4 or 8
            break;

          case MEMORY:
            // will use lower 4 bytes of int8 in memory
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
              NODE* f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_CONSTANT;
              f->fcte = (float)i->icte;
            }
            break;

          case INT_REGISTER:
          {
            FREG fr = allocate_fregister ();

            c_conv_int_to_float (target        => fr,
                                 target_size   => 4,         // 4 or 8
                                 source        => i->reg,    // can be ZERO
                                 source_signed => true,
                                 source_size   => 4);        // 4 or 8

            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register (*i);    // allows reusing registers of node i
            FREG fr = allocate_fregister ();

            c_load_register_from_memory (r, i->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8

            c_conv_int_to_float (target        => fr,
                                 target_size   => 4,       // 4 or 8
                                 source        => r,       // can be ZERO
                                 source_signed => true,
                                 source_size   => 4);      // 4 or 8
            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
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
            FREG fr = allocate_fregister ();

            c_conv_int_to_float (target        => fr,
                                 target_size   => 8,         // 4 or 8
                                 source        => i->reg,    // can be ZERO
                                 source_signed => true,
                                 source_size   => 4);        // 4 or 8

            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register (*i);    // allows reusing registers of node i
            FREG fr = allocate_fregister ();

            c_load_register_from_memory (r, i->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8

            c_conv_int_to_float (target        => fr,
                                 target_size   => 8,       // 4 or 8
                                 source        => r,       // can be ZERO
                                 source_signed => true,
                                 source_size   => 4);      // 4 or 8
            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
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
              NODE* f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_CONSTANT;
              f->fcte = (float)i->icte;
            }
            break;

          case INT_REGISTER:
          {
            FREG fr = allocate_fregister ();

            c_conv_int_to_float (target        => fr,
                                 target_size   => 4,         // 4 or 8
                                 source        => i->reg,    // can be ZERO
                                 source_signed => false,
                                 source_size   => 4);        // 4 or 8

            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register (*i);    // allows reusing registers of node i
            FREG fr = allocate_fregister ();

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8

            c_conv_int_to_float (target        => fr,
                                 target_size   => 4,       // 4 or 8
                                 source        => r,       // can be ZERO
                                 source_signed => false,
                                 source_size   => 4);      // 4 or 8
            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
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
              f->fcte = (float8)i->icte;
            }
            break;

          case INT_REGISTER:
          {
            FREG fr = allocate_fregister ();

            c_conv_int_to_float (target        => fr,
                                 target_size   => 8,         // 4 or 8
                                 source        => i->reg,    // can be ZERO
                                 source_signed => false,
                                 source_size   => 4);        // 4 or 8

            {
              NODE* f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register (*i);    // allows reusing registers of node i
            FREG fr = allocate_fregister ();

            c_load_register_from_memory (r, i->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8

            c_conv_int_to_float (target        => fr,
                                 target_size   => 8,       // 4 or 8
                                 source        => r,       // can be ZERO
                                 source_signed => false,
                                 source_size   => 4);      // 4 or 8
            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
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
            f->fcte = (float)i->icte;
            break;
          }

          case INT_REGISTER:
          {
            FREG fr = allocate_fregister ();

            c_conv_int_to_float (target        => fr,
                                 target_size   => 4,         // 4 or 8
                                 source        => i->reg,    // can be ZERO
                                 source_signed => true,
                                 source_size   => 8);        // 4 or 8

            {
              NODE* f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register (*i);    // allows reusing registers of node i
            FREG fr = allocate_fregister ();

            c_load_register_from_memory (r, i->ea, data_signed => true, size => 8); // size = 1, 2, 4, 8

            c_conv_int_to_float (target        => fr,
                                 target_size   => 4,       // 4 or 8
                                 source        => r,       // can be ZERO
                                 source_signed => true,
                                 source_size   => 8);      // 4 or 8
            {
              NODE* f = &fstack[fstack_count++];
              f->typ = 'f';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
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
            f->fcte = (float8)i->icte;
            break;
          }

          case INT_REGISTER:
          {
            FREG fr = allocate_fregister ();

            c_conv_int_to_float (target        => fr,
                                 target_size   => 8,         // 4 or 8
                                 source        => i->reg,    // can be ZERO
                                 source_signed => true,
                                 source_size   => 8);        // 4 or 8

            {
              NODE* f = &fstack[fstack_count++];
              f->typ = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register (*i);    // allows reusing registers of node i
            FREG fr = allocate_fregister ();

            c_load_register_from_memory (r, i->ea, data_signed => true, size => 8); // size = 1, 2, 4, 8

            c_conv_int_to_float (target        => fr,
                                 target_size   => 8,       // 4 or 8
                                 source        => r,       // can be ZERO
                                 source_signed => true,
                                 source_size   => 8);      // 4 or 8
            {
              NODE* f = &fstack[fstack_count++];
              f->typ  = 'd';
              f->kind = FLOAT_REGISTER;
              f->freg = fr;
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
            i->typ  = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (uint)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            REG r = allocate_register ();

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => false,
                                 target_size   => 4,        // 4 or 8
                                 source        => f->freg,
                                 source_size   => 4);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register ();
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 4); // size = 4, 8

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => false,
                                 target_size   => 4,        // 4 or 8
                                 source        => fr,
                                 source_size   => 4);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
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
            i->typ  = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (int)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            REG r = allocate_register ();

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 4,        // 4 or 8
                                 source        => f->freg,
                                 source_size   => 4);      // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register ();
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 4); // size = 4, 8

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 4,        // 4 or 8
                                 source        => fr,
                                 source_size   => 4);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
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
            i->typ  = 'l';
            i->kind = INT_CONSTANT;
            i->icte = (int8)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            REG r = allocate_register ();

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 8,        // 4 or 8
                                 source        => f->freg,
                                 source_size   => 4);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'l';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register ();
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 4); // size = 4, 8

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 8,        // 4 or 8
                                 source        => fr,
                                 source_size   => 4);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'l';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
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
            i->typ  = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (uint)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            REG r = allocate_register ();

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => false,
                                 target_size   => 4,        // 4 or 8
                                 source        => f->freg,
                                 source_size   => 8);      // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register ();
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 8); // size = 4, 8

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => false,
                                 target_size   => 4,        // 4 or 8
                                 source        => fr,
                                 source_size   => 8);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
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
            i->typ  = 'i';
            i->kind = INT_CONSTANT;
            i->icte = (int)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            REG r = allocate_register ();

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 4,        // 4 or 8
                                 source        => f->freg,
                                 source_size   => 8);      // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register ();
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 8); // size = 4, 8

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 4,        // 4 or 8
                                 source        => fr,
                                 source_size   => 8);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'i';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
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
            i->typ  = 'l';
            i->kind = INT_CONSTANT;
            i->icte = (int)f->fcte;
            break;
          }

          case FLOAT_REGISTER:
          {
            REG r = allocate_register ();

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 8,        // 4 or 8
                                 source        => f->freg,
                                 source_size   => 8);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'l';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
          }
          break;

          case MEMORY:
          {
            REG  r  = allocate_register ();
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 8); // size = 4, 8

            c_conv_float_to_int (target        => r,        // can be ZERO
                                 target_signed => true,
                                 target_size   => 8,        // 4 or 8
                                 source        => fr,
                                 source_size   => 8);       // 4 or 8
            {
              NODE* i = &istack[istack_count++];
              i->typ  = 'l';
              i->kind = INT_REGISTER;
              i->reg  = r;
            }
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

        f->typ = 'd';

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
            break;

          case FLOAT_REGISTER:
            c_fconv_precision (target      => f->freg,
                               target_size => 8,        // 4 or 8
                               source      => f->freg,
                               source_size => 4);       // 4 or 8
            break;

          case MEMORY:
          {
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 4); // size = 4, 8

            c_fconv_precision (target      => fr,
                               target_size => 8,     // 4 or 8
                               source      => fr,
                               source_size => 4);    // 4 or 8

            f->kind = FLOAT_REGISTER;
            f->freg = fr;
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

        f->typ = 'f';

        switch (f->kind)
        {
          case FLOAT_CONSTANT:
            f->fcte = (float)f->fcte;
            break;

          case FLOAT_REGISTER:
            c_fconv_precision (target      => f->freg,
                               target_size => 4,        // 4 or 8
                               source      => f->freg,
                               source_size => 8);       // 4 or 8
            break;

          case MEMORY:
          {
            FREG fr = allocate_fregister ();

            c_load_fregister_from_memory (fr, f->ea, size => 8); // size = 4, 8

            c_fconv_precision (target      => fr,
                               target_size => 4,     // 4 or 8
                               source      => fr,
                               source_size => 8);    // 4 or 8

            f->kind = FLOAT_REGISTER;
            f->freg = fr;
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

        assert op <= 3;

        switch (i->kind)
        {
          case INT_CONSTANT:
          {
            REG r = allocate_register ();

            move_register_immediate (target => r,
                                     imm    => i->icte,
                                     size   => 4);     // 4 or 8
            i->kind = INT_REGISTER;
            i->reg  = r;
          }
          break;

          case INT_REGISTER:
            break;

          case MEMORY:
          {
            REG r = allocate_register (*i);

            c_load_register_from_memory (r           => r,
                                         ea          => i->ea,
                                         data_signed => op >= 2,
                                         size        => (op == 0 || op == 2) ? 1 : 2); // size = 1, 2, 4, 8
            i->kind = INT_REGISTER;
            i->reg  = r;
          }
          break;

          default:
            abort;
        }

        {
          REG r = allocate_register (*i);

          if (op < 2)   // 0, 1
          {
            c_extend_unsigned (target      => r,       // 8 bytes
                               source      => i->reg,
                               source_size => (op == 0) ? 1 : 2); // 1 or 2 bytes
          }
          else  // 2, 3
          {
            c_extend_signed (target       => r,       // 8 bytes
                             source       => i->reg,
                             source_size  => (op == 2) ? 1 : 2); // 1, 2 or 4 bytes
          }

          i->reg = r;
        }
      }
      break;


  // --------
  // 9. names
  // --------

  // ----------------------------------------------------------------------

      // ( addr  -->  addr  uint4  )

      case P_GET_CONSTR0:  // <offset4>  ; load constraint from memory at [addr + offset]
      {
        NODE* a = &astack[astack_count - 1];
        int4  offset;
        REG   r;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset %d\n", offset);

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        r = allocate_register ();

        a->ea.offset += offset;
        c_load_register_from_memory (r, a->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
        a->ea.offset -= offset;

        {
          NODE* i;
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
      }
      break;


      // ( addr  addr2  -->  addr  addr2  uint4  )

      case P_GET_CONSTR1:  // <offset4> ; load constraint from memory at [addr + offset]
      {
        NODE* a = &astack[astack_count - 2];
        int4  offset;
        REG   r;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset %d\n", offset);

        // make effective address that consists only of a base and a small offset,
        // so no index, relocation or large offset requiring multiple instructions.
        make_simple_effective_address (ref *a);

        r = allocate_register ();

        a->ea.offset += offset;
        c_load_register_from_memory (r, a->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
        a->ea.offset -= offset;

        {
          NODE* i;
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = r;
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
        bool  swapped_nodes;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        swapped_nodes = false;
        if (index->kind == INT_CONSTANT)
        {
          swap_nodes (ref *index, ref *length);
          swapped_nodes = true;
        }

        flush_int4_in_register (ref *index);

        if (length->kind == INT_CONSTANT)
        {
          assert (int)length->icte == length->icte;    // range is int4

          if (length->icte <= 4095)
          {
            c_cmp_reg_imm (source     => index->reg,         // SP allowed (ZERO not allowed)
                           imm12      => (int)length->icte,  // 0 to 4095
                           shl_imm_12 => false,              // true to shift imm12 << 12
                           size       => 4);                 // 4 or 8
          }
          else if (length->icte <= 4095*4096 && (length->icte & 4095) == 0)
          {
            c_cmp_reg_imm (source     => index->reg,                 // SP allowed (ZERO not allowed)
                           imm12      => ((int)length->icte) >> 12,  // 0 to 4095
                           shl_imm_12 => true,                       // true to shift imm12 << 12
                           size       => 4);                         // 4 or 8
          }
          else
          {
            REG r = allocate_register ();
            int s, ofs;

            ofs = (int)length->icte;
            for (s=31; (ofs & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
              ;

            ofs >>= s;

            move_register_immediate (target => r,
                                     imm    => ofs,
                                     size   => 4);   // 4 or 8

            length->kind = INT_REGISTER;
            length->reg = r;

            c_cmp_reg_reg (source1             => index->reg,  // ZERO allowed (SP not allowed)
                           source2             => length->reg, // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,         // LSL, LSR, ASR
                           source2_shift_value => (uint)s,     // range 0..31 (or 0..63 for size==8)
                           size                => 4);          // 4 or 8
          }
        }
        else
        {
          if (length->kind == MEMORY)
          {
            REG r = allocate_register (*length);
            c_load_register_from_memory (r, length->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
            length->kind = INT_REGISTER;
            length->reg = r;
          }

          c_cmp_reg_reg (source1             => index->reg,  // ZERO allowed (SP not allowed)
                         source2             => length->reg, // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,         // LSL, LSR, ASR
                         source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                         size                => 4);          // 4 or 8
        }

        if (swapped_nodes)
          swap_nodes (ref *index, ref *length);   // restore order

        cond_branch (cmp        => swapped_nodes ? CMP_SMALLER_OR_EQUAL : CMP_LARGER_OR_EQUAL,
                     signed     => false,
                     near_label => store_ll (g_current_source_line, near_label_nr));

        istack_count--;
      }
      break;


      //    (  addr  index4  -->  addr2  )

      case P_ADD_INDEX:       // <size4>
      {
        NODE* addr  = &astack[astack_count - 1];
        NODE* index = &istack[istack_count - 1];
        int4  size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size %d\n", size);

        if (index->kind == INT_CONSTANT)
        {
          flush_effective_address (ref *addr);      // MEMORY -> EFFECTIVE ADDRESS

          addr->ea.offset += (int)index->icte * size;
        }
        else if (size == 0)
        {
          // nothing to do
        }
        else
        {
          flush_effective_address (ref *addr);      // MEMORY -> EFFECTIVE ADDRESS

          // index can be INT_REGISTER or MEMORY

          if (index->kind == INT_REGISTER)
          {
            if (addr->ea.index == ZERO)
            {
              // set as index register
              addr->ea.index = index->reg;
              addr->ea.scale = size;
            }
            else if (addr->ea.scale == size && register_usage_count(addr->ea.index) == 1)
            {
              // add to index register
              c_add_reg_reg (target              => addr->ea.index, // ZERO allowed (SP not allowed)
                             source1             => addr->ea.index, // ZERO allowed (SP not allowed)
                             source2             => index->reg,     // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,            // LSL, LSR, ASR
                             source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                             size                => 4);             // 4 or 8
            }
            else
            {
              REG r = allocate_register (*addr);

              compute_effective_address_in_register (addr->ea, r);

              addr->ea.base = r;
              addr->ea.index = index->reg;
              addr->ea.scale = size;
              addr->ea.offset = 0;
              addr->ea.reloc.kind = RELOC_NONE;
              addr->ea.reloc.nr   = 0;
            }
          }
          else  // index is MEMORY operand
          {
            if (addr->ea.base == ZERO && size == 1)
            {
              REG r = allocate_register ();
              addr->ea.base = r;
              c_load_register_from_memory (r, index->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
            }
            else if (addr->ea.index == ZERO)
            {
              REG r = allocate_register ();
              addr->ea.index = r;   // set as index register
              addr->ea.scale = size;
              c_load_register_from_memory (r, index->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
            }
            else if (addr->ea.base != ZERO && addr->ea.base != FP && addr->ea.base != SP &&
                     register_usage_count(addr->ea.base) == 1 && size == 1)
            {
              REG r = allocate_register ();
              c_load_register_from_memory (r, index->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8

              // add to base register
              c_add_reg_reg (target              => addr->ea.base,  // ZERO allowed (SP not allowed)
                             source1             => addr->ea.base,  // ZERO allowed (SP not allowed)
                             source2             => r,              // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,            // LSL, LSR, ASR
                             source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                             size                => address_size);  // 4 or 8
            }
            else if (addr->ea.index != ZERO && register_usage_count(addr->ea.index) == 1 && addr->ea.scale == size)
            {
              REG r = allocate_register ();
              c_load_register_from_memory (r, index->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8

              // add to index register
              c_add_reg_reg (target              => addr->ea.index, // ZERO allowed (SP not allowed)
                             source1             => addr->ea.index, // ZERO allowed (SP not allowed)
                             source2             => r,              // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,            // LSL, LSR, ASR
                             source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                             size                => 4);             // 4 or 8
            }
            else
            {
              REG r = allocate_register (*addr);

              compute_effective_address_in_register (addr->ea, r);

              addr->ea.base = r;
              addr->ea.index = ZERO;
              addr->ea.scale = 1;
              addr->ea.offset = 0;
              addr->ea.reloc.kind = RELOC_NONE;
              addr->ea.reloc.nr   = 0;

              flush_int4_in_register (ref *index);
              addr->ea.index = index->reg;
              addr->ea.scale = size;
            }
          }
        }

        istack_count--;
      }
      break;


      //    (  addr  index4  -->  addr2  )

      //  note: the index4 is signed, so it must be sign-extended to int8 !

      case P_ADD_PTR_OFFSET:  // <size4>   (same for unsafe pointers, note that index4 is a signed offset !)
      case P_SUB_PTR_OFFSET:  // <size4>
      {
        NODE* addr  = &astack[astack_count - 1];
        NODE* index = &istack[istack_count - 1];
        int4  size;

        size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" size %d\n", size);

        if (index->kind == INT_CONSTANT)
        {
          flush_effective_address (ref *addr);      // MEMORY -> EFFECTIVE ADDRESS

          if (g_current_pcode == P_ADD_PTR_OFFSET)
            addr->ea.offset += (int)index->icte * size;
          else
            addr->ea.offset -= (int)index->icte * size;
        }
        else if (size == 0)
        {
          // nothing to do
        }
        else
        {
          flush_effective_address (ref *addr);      // MEMORY -> EFFECTIVE ADDRESS

          // index can be INT_REGISTER or MEMORY

          if (index->kind == INT_REGISTER)
          {
            if (addr->ea.index == ZERO)      // set as index register
            {
              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                addr->ea.index = index->reg;
                addr->ea.scale = size;
              }
              else
              {
                REG r;

                if (register_usage_count(index->reg) == 1)
                  r = index->reg;
                else
                  r = allocate_register ();

                c_neg_reg_reg (target => r,           // ZERO allowed (SP not allowed)
                               source => index->reg,  // ZERO allowed (SP not allowed)
                               size   => 4);          // 4 or 8

                addr->ea.index = r;
                addr->ea.scale = size;
              }
            }
            else if (addr->ea.scale == size && register_usage_count(addr->ea.index) == 1)
            {
              // add to index register
              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                c_add_reg_reg (target              => addr->ea.index, // ZERO allowed (SP not allowed)
                               source1             => addr->ea.index, // ZERO allowed (SP not allowed)
                               source2             => index->reg,     // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,            // LSL, LSR, ASR
                               source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                               size                => 4);             // 4 or 8
              }
              else
              {
                c_sub_reg_reg (target              => addr->ea.index, // ZERO allowed (SP not allowed)
                               source1             => addr->ea.index, // ZERO allowed (SP not allowed)
                               source2             => index->reg,     // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,            // LSL, LSR, ASR
                               source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                               size                => 4);             // 4 or 8
              }
            }
            else  // general case : add index to addr
            {
              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                REG r = allocate_register (*addr);

                compute_effective_address_in_register (addr->ea, r);

                addr->ea.base = r;
                addr->ea.index = index->reg;
                addr->ea.scale = size;
                addr->ea.offset = 0;
                addr->ea.reloc.kind = RELOC_NONE;
                addr->ea.reloc.nr   = 0;
              }
              else
              {
                REG r[2];

                allocate_registers (out r, *addr);

                compute_effective_address_in_register (addr->ea, r[0]);

                c_neg_reg_reg (target => r[1],         // ZERO allowed (SP not allowed)
                               source => index->reg,   // ZERO allowed (SP not allowed)
                               size   => 4);           // 4 or 8

                addr->ea.base = r[0];
                addr->ea.index = r[1];
                addr->ea.scale = size;
                addr->ea.offset = 0;
                addr->ea.reloc.kind = RELOC_NONE;
                addr->ea.reloc.nr   = 0;
              }
            }
          }
          else  // index is MEMORY operand
          {
            if (addr->ea.base == ZERO && size == 1)
            {
              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                REG r = allocate_register ();
                c_load_register_from_memory (r, index->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8
                addr->ea.base = r;
              }
              else
              {
                REG r = allocate_register ();
                c_load_register_from_memory (r, index->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8

                c_neg_reg_reg (target => r,    // ZERO allowed (SP not allowed)
                               source => r,    // ZERO allowed (SP not allowed)
                               size   => 8);   // 4 or 8

                addr->ea.base = r;
              }
            }
            else if (addr->ea.index == ZERO)
            {
              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                REG r = allocate_register ();
                c_load_register_from_memory (r, index->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8
                addr->ea.index = r;   // set as index register
                addr->ea.scale = size;
              }
              else
              {
                REG r = allocate_register ();
                c_load_register_from_memory (r, index->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8

                c_neg_reg_reg (target => r,    // ZERO allowed (SP not allowed)
                               source => r,    // ZERO allowed (SP not allowed)
                               size   => 4);   // 4 or 8

                addr->ea.index = r;   // set as index register
                addr->ea.scale = size;
              }
            }
            else if (addr->ea.base != ZERO && addr->ea.base != FP && addr->ea.base != SP &&
                     register_usage_count(addr->ea.base) == 1 && size == 1)
            {
              REG r = allocate_register ();
              c_load_register_from_memory (r, index->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8

              // add to base register
              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                c_add_reg_reg (target              => addr->ea.base,  // ZERO allowed (SP not allowed)
                               source1             => addr->ea.base,  // ZERO allowed (SP not allowed)
                               source2             => r,              // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,            // LSL, LSR, ASR
                               source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                               size                => address_size);  // 4 or 8
              }
              else
              {
                c_sub_reg_reg (target              => addr->ea.base,  // ZERO allowed (SP not allowed)
                               source1             => addr->ea.base,  // ZERO allowed (SP not allowed)
                               source2             => r,              // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,            // LSL, LSR, ASR
                               source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                               size                => address_size);  // 4 or 8
              }
            }
            else if (addr->ea.index != ZERO && register_usage_count(addr->ea.index) == 1 && addr->ea.scale == size)
            {
              REG r = allocate_register ();
              c_load_register_from_memory (r, index->ea, data_signed => true, size => 4); // size = 1, 2, 4, 8

              // add to index register
              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                c_add_reg_reg (target              => addr->ea.index, // ZERO allowed (SP not allowed)
                               source1             => addr->ea.index, // ZERO allowed (SP not allowed)
                               source2             => r,              // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,            // LSL, LSR, ASR
                               source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                               size                => 4);             // 4 or 8
              }
              else
              {
                c_sub_reg_reg (target              => addr->ea.index, // ZERO allowed (SP not allowed)
                               source1             => addr->ea.index, // ZERO allowed (SP not allowed)
                               source2             => r,              // ZERO allowed (SP not allowed)
                               source2_shift_type  => LSL,            // LSL, LSR, ASR
                               source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                               size                => 4);             // 4 or 8
              }
            }
            else
            {
              REG r = allocate_register (*addr);

              compute_effective_address_in_register (addr->ea, r);

              addr->ea.base = r;
              addr->ea.index = ZERO;
              addr->ea.scale = 1;
              addr->ea.offset = 0;
              addr->ea.reloc.kind = RELOC_NONE;
              addr->ea.reloc.nr   = 0;

              flush_int4_in_register (ref *index);

              if (g_current_pcode == P_ADD_PTR_OFFSET)
              {
                addr->ea.index = index->reg;
                addr->ea.scale = size;
              }
              else
              {
                c_neg_reg_reg (target => index->reg,    // ZERO allowed (SP not allowed)
                               source => index->reg,    // ZERO allowed (SP not allowed)
                               size   => 4);   // 4 or 8

                addr->ea.index = index->reg;
                addr->ea.scale = size;
              }
            }
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

        if (len >= 0 && len <= 4095)
        {
          c_cmp_reg_imm (source     => ofs->reg,   // SP allowed (ZERO not allowed)
                         imm12      => len,        // 0 to 4095
                         shl_imm_12 => false,      // true to shift imm12 << 12
                         size       => 4);         // 4 or 8
        }
        else if (len >= 0 && len <= 4095*4096 && (len & 4095) == 0)
        {
          c_cmp_reg_imm (source     => ofs->reg,   // SP allowed (ZERO not allowed)
                         imm12      => len >> 12,  // 0 to 4095
                         shl_imm_12 => true,       // true to shift imm12 << 12
                         size       => 4);         // 4 or 8
        }
        else
        {
          REG r = allocate_register ();
          int s, lg;

          lg = len;
          for (s=31; (lg & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
            ;

          lg >>= s;

          move_register_immediate (target => r,
                                   imm    => lg,
                                   size   => 4);   // 4 or 8

          c_cmp_reg_reg (source1             => ofs->reg,    // ZERO allowed (SP not allowed)
                         source2             => r,           // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,         // LSL, LSR, ASR
                         source2_shift_value => (uint)s,     // range 0..31 (or 0..63 for size==8)
                         size                => 4);          // 4 or 8
        }

        cond_branch (cmp         => CMP_LARGER,
                     signed      => false,
                     near_label  => store_ll (g_current_source_line, near_label_nr));
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

        // length = length - len
        if (len >= 0 && len <= 4095)
        {
          c_subs_reg_imm (target     => length->reg,
                          source     => length->reg, // SP allowed (ZERO not allowed)
                          imm12      => len,         // 0 to 4095
                          shl_imm_12 => false,       // true to shift imm12 << 12
                          size       => 4);          // 4 or 8
        }
        else if (len >= 0 && len <= 4095*4096 && (len & 4095) == 0)
        {
          c_subs_reg_imm (target     => length->reg,
                          source     => length->reg, // SP allowed (ZERO not allowed)
                          imm12      => len >> 12,   // 0 to 4095
                          shl_imm_12 => true,        // true to shift imm12 << 12
                          size       => 4);          // 4 or 8
        }
        else
        {
          REG r = allocate_register ();
          int s, lg;

          lg = len;
          for (s=31; (lg & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
            ;

          lg >>= s;

          move_register_immediate (target => r,
                                   imm    => lg,
                                   size   => 4);   // 4 or 8

          c_subs_reg_reg (target              => length->reg,
                          source1             => length->reg, // ZERO allowed (SP not allowed)
                          source2             => r,           // ZERO allowed (SP not allowed)
                          source2_shift_type  => LSL,         // LSL, LSR, ASR
                          source2_shift_value => (uint)s,     // range 0..31 (or 0..63 for size==8)
                          size                => 4);          // 4 or 8
        }

        cond_branch (cmp        => CMP_SMALLER,
                     signed     => false,
                     near_label => store_ll (g_current_source_line, near_label_nr));


        switch (ofs->kind)
        {
          case INT_CONSTANT:      //  if length < ofs  --> error    (uint4 comparison)

            if (ofs->icte >= 0 && ofs->icte <= 4095)
            {
              c_cmp_reg_imm (source     => length->reg,      // SP allowed (ZERO not allowed)
                             imm12      => (int)ofs->icte,   // 0 to 4095
                             shl_imm_12 => false,            // true to shift imm12 << 12
                             size       => 4);               // 4 or 8
            }
            else if (ofs->icte >= 0 && ofs->icte <= 4095*4096 && (ofs->icte & 4095) == 0)
            {
              c_cmp_reg_imm (source     => length->reg,             // SP allowed (ZERO not allowed)
                             imm12      => ((int)ofs->icte) >> 12,  // 0 to 4095
                             shl_imm_12 => true,                    // true to shift imm12 << 12
                             size       => 4);                      // 4 or 8
            }
            else
            {
              REG r = allocate_register ();
              int s, cte;

              cte = (int)ofs->icte;
              for (s=31; (cte & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
                ;

              cte >>= s;

              move_register_immediate (target => r,
                                       imm    => cte,
                                       size   => 4);   // 4 or 8

              c_cmp_reg_reg (source1             => length->reg, // ZERO allowed (SP not allowed)
                             source2             => r,           // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,         // LSL, LSR, ASR
                             source2_shift_value => (uint)s,     // range 0..31 (or 0..63 for size==8)
                             size                => 4);          // 4 or 8
            }
            break;

          case INT_REGISTER:
            c_cmp_reg_reg (source1             => length->reg, // ZERO allowed (SP not allowed)
                           source2             => ofs->reg,    // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,         // LSL, LSR, ASR
                           source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                           size                => 4);          // 4 or 8
            break;

          case MEMORY:
            {
              REG r = allocate_register (*ofs);
              c_load_register_from_memory (r, ofs->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
              ofs->kind = INT_REGISTER;
              ofs->reg = r;
            }

            c_cmp_reg_reg (source1             => length->reg, // ZERO allowed (SP not allowed)
                           source2             => ofs->reg,    // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,         // LSL, LSR, ASR
                           source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                           size                => 4);          // 4 or 8
            break;

          default:
            abort;
        }

        cond_branch (cmp        => CMP_SMALLER,
                     signed     => false,
                     near_label => store_ll (g_current_source_line, near_label_nr));

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
        int4  len_addr, near_label_nr;
        EA    ea;

        // len is evaluated and stored in a local variable before the pcode.
        //
        //  if len > length       --> error    (uint4 comparison)
        //  if ofs > length - len --> error    (uint4 comparison)

        len_addr      = *((int4 *)&mem[mem_offset]);
        near_label_nr = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        len_addr += (int)g_first_page_size;    // we need to add first page frame size to any local address or parameter on stack

        if (g_tracing)
          trace (" len_addr %d  label #%d\n", len_addr, near_label_nr);

        flush_int4_in_register_for_modif (ref *length);

        // compute length - len

        {
          REG r;

          clear ea;
          ea.base  = FP;
          ea.index = ZERO;
          ea.scale = 1;
          ea.offset = len_addr;
          ea.reloc.kind = RELOC_NONE;
          ea.reloc.nr   = 0;

          r = allocate_register ();

          c_load_register_from_memory (r, ea, data_signed => false, size => 4); // size = 1, 2, 4, 8

          c_subs_reg_reg (target              => length->reg,
                          source1             => length->reg, // ZERO allowed (SP not allowed)
                          source2             => r,           // ZERO allowed (SP not allowed)
                          source2_shift_type  => LSL,         // LSL, LSR, ASR
                          source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                          size                => 4);          // 4 or 8
        }

        cond_branch (cmp        => CMP_SMALLER,
                     signed     => false,
                     near_label => store_ll (g_current_source_line, near_label_nr));


        switch (ofs->kind)
        {
          case INT_CONSTANT:      //  if length < ofs  --> error    (uint4 comparison)

            if (ofs->icte >= 0 && ofs->icte <= 4095)
            {
              c_cmp_reg_imm (source     => length->reg,      // SP allowed (ZERO not allowed)
                             imm12      => (int)ofs->icte,   // 0 to 4095
                             shl_imm_12 => false,            // true to shift imm12 << 12
                             size       => 4);               // 4 or 8
            }
            else if (ofs->icte >= 0 && ofs->icte <= 4095*4096 && (ofs->icte & 4095) == 0)
            {
              c_cmp_reg_imm (source     => length->reg,             // SP allowed (ZERO not allowed)
                             imm12      => ((int)ofs->icte) >> 12,  // 0 to 4095
                             shl_imm_12 => true,                    // true to shift imm12 << 12
                             size       => 4);                      // 4 or 8
            }
            else
            {
              REG r = allocate_register ();
              int s, cte;

              cte = (int)ofs->icte;
              for (s=31; (cte & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
                ;

              cte >>= s;

              move_register_immediate (target => r,
                                       imm    => cte,
                                       size   => 4);   // 4 or 8

              c_cmp_reg_reg (source1             => length->reg, // ZERO allowed (SP not allowed)
                             source2             => r,           // ZERO allowed (SP not allowed)
                             source2_shift_type  => LSL,         // LSL, LSR, ASR
                             source2_shift_value => (uint)s,     // range 0..31 (or 0..63 for size==8)
                             size                => 4);          // 4 or 8
            }
            break;

          case INT_REGISTER:
            c_cmp_reg_reg (source1             => length->reg, // ZERO allowed (SP not allowed)
                           source2             => ofs->reg,    // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,         // LSL, LSR, ASR
                           source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                           size                => 4);          // 4 or 8
            break;

          case MEMORY:
            {
              REG r = allocate_register (*ofs);
              c_load_register_from_memory (r, ofs->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
              ofs->kind = INT_REGISTER;
              ofs->reg = r;
            }

            c_cmp_reg_reg (source1             => length->reg, // ZERO allowed (SP not allowed)
                           source2             => ofs->reg,    // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,         // LSL, LSR, ASR
                           source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                           size                => 4);          // 4 or 8
            break;

          default:
            abort;
        }

        cond_branch (cmp        => CMP_SMALLER,
                     signed     => false,
                     near_label => store_ll (g_current_source_line, near_label_nr));

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
        int4  cte0, near_label_nr;

        cte0          = *((int4 *)&mem[mem_offset]);
        near_label_nr = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" cte = #%d  near_label_nr = %d\n", cte0, near_label_nr);

        if (value->kind == INT_CONSTANT)
          fatal_compiler_error0 ("as86(P_CHECK_SAME)");

        if (value->kind == MEMORY)
        {
          REG r = allocate_register (*value);
          c_load_register_from_memory (r, value->ea, data_signed => false, size => 4); // size = 1, 2, 4, 8
          value->kind = INT_REGISTER;
          value->reg = r;
        }

        // special optimized case : compare with zero
        if (cte0 == 0)
        {
          branch_if_not_zero (reg => value->reg, size => 4, near_label => near_label_nr);
          istack_count--;
          break;
        }

        if (cte0 >= 0 && cte0 <= 4095)
        {
          c_cmp_reg_imm (source     => value->reg,  // SP allowed (ZERO not allowed)
                         imm12      => cte0,        // 0 to 4095
                         shl_imm_12 => false,       // true to shift imm12 << 12
                         size       => 4);          // 4 or 8
        }
        else if (cte0 >= 0 && cte0 <= 4095*4096 && (cte0 & 4095) == 0)
        {
          c_cmp_reg_imm (source     => value->reg,  // SP allowed (ZERO not allowed)
                         imm12      => cte0 >> 12,  // 0 to 4095
                         shl_imm_12 => true,        // true to shift imm12 << 12
                         size       => 4);          // 4 or 8
        }
        else
        {
          REG r = allocate_register ();
          int s, cte;

          cte = cte0;
          for (s=31; (cte & ((1<<s)-1)) != 0; s--)   // not multiple of 1<<s   (s==0 will succeed)
            ;

          cte >>= s;

          move_register_immediate (target => r,
                                   imm    => cte,
                                   size   => 4);   // 4 or 8

          c_cmp_reg_reg (source1             => value->reg,  // ZERO allowed (SP not allowed)
                         source2             => r,           // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,         // LSL, LSR, ASR
                         source2_shift_value => (uint)s,     // range 0..31 (or 0..63 for size==8)
                         size                => 4);          // 4 or 8
        }

        cond_branch (cmp        => CMP_NOT_EQUAL,
                     signed     => false,
                     near_label => store_ll (g_current_source_line, near_label_nr));

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

        flush_int4_in_register (ref *value1);
        flush_int4_in_register (ref *value2);

        c_cmp_reg_reg (source1             => value1->reg, // ZERO allowed (SP not allowed)
                       source2             => value2->reg, // ZERO allowed (SP not allowed)
                       source2_shift_type  => LSL,         // LSL, LSR, ASR
                       source2_shift_value => 0,           // range 0..31 (or 0..63 for size==8)
                       size                => 4);          // 4 or 8

        cond_branch (cmp         => CMP_NOT_EQUAL,
                     signed      => false,
                     near_label  => store_ll (g_current_source_line, near_label_nr));

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

        if (n->kind == INT_CONSTANT)
        {
          n->icte *= elem_size;
        }
        else
        {
          flush_int4_in_register_for_modif (ref *n);

          if (elem_size == 0)  // "* 0" yields zero
          {
            n->kind = INT_CONSTANT;
            n->icte = 0;
          }
          else if (elem_size == 1)  // "* 1" has no effect
          {
            // leave operand unchanged
          }
          else if (elem_size >= 2 && elem_size <= 2_000_000_000 && (elem_size & (elem_size-1)) == 0)  // power of 2
          {
            REG r = allocate_register (*n);

            c_add_reg_reg (target              => r,                       // ZERO allowed (SP not allowed)
                           source1             => ZERO,                    // ZERO allowed (SP not allowed)
                           source2             => n->reg,                  // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,                     // LSL, LSR, ASR
                           source2_shift_value => (uint)lshifts_of(elem_size),  // range 0..31 (or 0..63 for size==8)
                           size                => 4);                      // 4 or 8
            n->reg = r;
          }
          else if (elem_size >= 3 && elem_size <= 2_000_000_000 && ((elem_size-1) & (elem_size-2)) == 0)  // power of 2 + 1
          {
            REG r = allocate_register (*n);

            c_add_reg_reg (target              => r,                         // ZERO allowed (SP not allowed)
                           source1             => n->reg,                    // ZERO allowed (SP not allowed)
                           source2             => n->reg,                    // ZERO allowed (SP not allowed)
                           source2_shift_type  => LSL,                       // LSL, LSR, ASR
                           source2_shift_value => (uint)lshifts_of(elem_size-1),  // range 0..31 (or 0..63 for size==8)
                           size                => 4);                        // 4 or 8
            n->reg = r;
          }
          else      // general case
          {
            REG r = allocate_register (*n);

            move_register_immediate (target => X19,  imm => elem_size,  size => 4);

            c_mult (target    => r,        // ZERO allowed
                    mul1      => n->reg,   // ZERO allowed
                    mul2      => X19,      // ZERO allowed
                    data_size => 4);       // 4 or 8

            n->reg = r;
          }
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

        if (a1->ea.base > X15 || a1->ea.index != ZERO || a1->ea.offset != 0 || a1->ea.reloc.kind != RELOC_NONE)
        {
          REG r = allocate_register (*a1);   // allows reusing registers of node a1

          compute_effective_address_in_register (ea => a1->ea, r => r);

          a1->ea.base = r;
          a1->ea.index = ZERO;
          a1->ea.scale = 1;
          a1->ea.offset = 0;
          a1->ea.reloc.kind = RELOC_NONE;
          a1->ea.reloc.nr = 0;
        }

        if (a2->ea.base > X15 || a2->ea.index != ZERO || a2->ea.offset != 0 || a2->ea.reloc.kind != RELOC_NONE)
        {
          REG r = allocate_register (*a2);   // allows reusing registers of node a2

          compute_effective_address_in_register (ea => a2->ea, r => r);

          a2->ea.base = r;
          a2->ea.index = ZERO;
          a2->ea.scale = 1;
          a2->ea.offset = 0;
          a2->ea.reloc.kind = RELOC_NONE;
          a2->ea.reloc.nr = 0;
        }

        {
          REG r = allocate_register (*a1);   // allows reusing registers of node a2

          c_sub_reg_reg (target              => r,           // ZERO allowed (SP not allowed)
                         source1             => a1->ea.base, // ZERO allowed (SP not allowed)
                         source2             => a2->ea.base, // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,  // LSL, LSR, ASR
                         source2_shift_value => 0, // range 0..31 (or 0..63 for size==8)
                         size                => address_size);   // 4 or 8

          astack_count -= 2;

          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = r;
        }
      }
      break;


      //  (  <pointer_addr>  -->  <heap_object_addr>  )

      case P_DEREF:   // <type4>  <local_addr4>   <near_label4>   <near_label4>
      {
        NODE* a = &astack[astack_count - 1];
        int4  typ, loc_addr, near_label_nr, near_label_nr2;
        REG   r, rr[2];

        typ            = *((int4 *)&mem[mem_offset]);
        loc_addr       = *((int4 *)&mem[mem_offset+4]);
        near_label_nr  = *((int4 *)&mem[mem_offset+8]);
        near_label_nr2 = *((int4 *)&mem[mem_offset+12]);
        mem_offset += 16;

        loc_addr += (int)g_first_page_size;    // we need to add first page frame size to any local address or parameter on stack

        if (g_tracing)
          trace (" typ=%d  loc_addr=%d  label=#%d\n", typ, loc_addr, near_label_nr);

        make_effective_address_in_base (ref *a);

        r = a->ea.base;     // r = tombstone address

        allocate_registers (out rr);    // allocate 2 new registers


        // lock and increment tombstone counter at r

#if 0    // ARM 8.1

        // rr[0] = 1;
        move_register_immediate (target => rr[0],
                                 imm    => 1,
                                 size   => 4);   // 4 or 8

        // tombstone.count++;
        // causes exception if ptr is null ?
        c_addto (target => ZERO,   // ZERO allowed - register receives loaded initial value
                 addr   => r,      // SP allowed   - address of memory to change
                 source => rr[0],  // ZERO allowed - register value to add
                 size   => 4);     // 1, 2, 4 or 8

        _unused near_label_nr2;

#else    // ARM 8.0

        declare_near_label (near_label => near_label_nr2);
        
        // rr[0] = tombstone.count;
        c_load_excl (target    => rr[0],  // ZERO allowed (data_size bytes)
                     base      => r,      // SP allowed (base effective address)(64 bit address)
                     data_size => 4);     // 1, 2, 4 or 8

        // rr[0]++;
        c_add_reg_imm (target     => rr[0],  // SP allowed (ZERO not allowed)
                       source     => rr[0],  // SP allowed (ZERO not allowed)
                       imm12      => 1,      // 0 to 4095
                       shl_imm_12 => false,  // true to shift imm12 << 12
                       size       => 4);     // 4 or 8

        // tombstone.count = rr[0];
        c_store_excl (target    => rr[1], // 32-bit register will contain status result : 0 = ok, 1 = failed
                      base      => r,     // SP allowed (base effective address)(64 bit address)
                      source    => rr[0], // ZERO allowed (data_size bytes)
                      data_size => 4);    // 1, 2, 4 or 8

        branch_if_not_zero (reg => rr[1], size => 4, near_label => near_label_nr2);
#endif

        // check that <type> matches tombstone type at r[4] -> error if check fails

        assert c_load_ofs8 (target      => rr[1], // ZERO allowed
                            base        => r,     // SP allowed (effective address)
                            offset      => 4,     // 9 bits (-256 to 255) to be added to base address
                            data_signed => false, // data to load is signed
                            data_size   => 4);    // size of data to load : 1, 2, 4, 8

        if (typ <= 4095)
        {
          c_cmp_reg_imm (source     => rr[1],   // SP allowed (ZERO not allowed)
                         imm12      => typ,     // 0 to 4095
                         shl_imm_12 => false,   // true to shift imm12 << 12
                         size       => 4);      // 4 or 8
        }
        else
        {
          move_register_immediate (target => rr[0],
                                   imm    => typ,
                                   size   => 4);   // 4 or 8

          c_cmp_reg_reg (source1             => rr[1],  // ZERO allowed (SP not allowed)
                         source2             => rr[0],  // ZERO allowed (SP not allowed)
                         source2_shift_type  => LSL,    // LSL, LSR, ASR
                         source2_shift_value => 0,      // range 0..31 (or 0..63 for size==8)
                         size                => 4);     // 4 or 8
        }

        cond_branch (cmp        => CMP_NOT_EQUAL,
                     signed     => false,
                     near_label => store_ll (g_current_source_line, near_label_nr));


        {
          // saves r (address of tombstone thus a pointer value) in temp <local_addr4>
          EA ea;

          clear ea;
          ea.base = FP;
          ea.index = ZERO;
          ea.scale = 1;
          ea.offset = loc_addr;
          ea.reloc.kind = RELOC_NONE;
          ea.reloc.nr = 0;

          c_store_register_in_memory (r, ea, size => address_size);  // size = 1, 2, 4, 8
        }

        // return address of heap object within the tombstone, thus r[8]
        a->typ = 'a';
        a->kind = MEMORY;
        a->ea.base = r;
        a->ea.index = ZERO;
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
        int  loc_addr, near_label_nr2;
        REG  rr[3];
        EA   ea;

        loc_addr       = *((int4 *)&mem[mem_offset]);
        near_label_nr2 = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        loc_addr += (int)g_first_page_size;    // we need to add first page frame size to any local address or parameter on stack

        if (g_tracing)
          trace (" loc_addr=%d\n", loc_addr);


        allocate_registers (out rr);    // allocate 3 registers

        clear ea;
        ea.base = FP;
        ea.index = ZERO;
        ea.scale = 1;
        ea.offset = loc_addr;
        ea.reloc.kind = RELOC_NONE;
        ea.reloc.nr = 0;

        // load address of tombstone structure in rr[0]
        c_load_register_from_memory (rr[0], ea, data_signed => false, size => address_size); // size = 1, 2, 4, 8


#if 0    // ARM 8.1

        // rr[1] = -1;
        move_register_immediate (target => rr[1],
                                 imm    => -1,
                                 size   => 4);   // 4 or 8

        // tombstone.count += rr[1];
        c_addto (target => ZERO,   // ZERO allowed - register receives loaded initial value
                 addr   => rr[0],  // SP allowed   - address of memory to change
                 source => rr[1],  // ZERO allowed - register value to add
                 size   => 4);     // 1, 2, 4 or 8

#else    // ARM 8.0

        declare_near_label (near_label => near_label_nr2);
        
        // rr[1] = tombstone.count;
        c_load_excl (target    => rr[1],  // ZERO allowed (data_size bytes)
                     base      => rr[0],  // SP allowed (base effective address)(64 bit address)
                     data_size => 4);     // 1, 2, 4 or 8

        // rr[1]--;
        c_sub_reg_imm (target     => rr[1],  // SP allowed (ZERO not allowed)
                       source     => rr[1],  // SP allowed (ZERO not allowed)
                       imm12      => 1,      // 0 to 4095
                       shl_imm_12 => false,  // true to shift imm12 << 12
                       size       => 4);     // 4 or 8

        // tombstone.count = rr[1];
        c_store_excl (target    => rr[2],  // 32-bit register will contain status result : 0 = ok, 1 = failed
                      base      => rr[0],  // SP allowed (base effective address)(64 bit address)
                      source    => rr[1],  // ZERO allowed (data_size bytes)
                      data_size => 4);     // 1, 2, 4 or 8

        branch_if_not_zero (reg => rr[2], size => 4, near_label => near_label_nr2);
#endif
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
        flush_uint1_in_register (ref *i);
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
        flush_int8_in_register (ref *i);
      }
      break;


      //   ( float4 --> float4 )

      case P_FORCE_FLT4:    // for m-code : force float4 value in a register
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_float4_in_register (ref *f);
      }
      break;


      //   ( float8 --> float8 )

      case P_FORCE_FLT8:    // for m-code : force float8 value in a register
      {
        NODE* f = &fstack[fstack_count - 1];
        if (g_tracing)
          trace (" \n");
        flush_float8_in_register (ref *f);
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
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (X0);
        load_bool_into_reg (ref *i, X0);
      }
      break;


      //   ( int4 --> int4 )

      case P_SYNC_4:     // for m-code : force int4 value in register X0
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (X0);
        load_int4_into_reg (ref *i, X0);
      }
      break;


      //   ( int8 --> int8 )

      case P_SYNC_8:     // for m-code : force int8 value in register X0
      {
        NODE* i = &istack[istack_count - 1];

        if (g_tracing)
          trace (" \n");
        free_register_except_for_this_pcode (X0);
        load_int8_into_reg (ref *i, X0);
      }
      break;


      //   ( float4 --> float4 )

      case P_SYNC_FLT4:    // for m-code : force float4 value on top float stack
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace (" \n");

        free_fregister_except_for_this_pcode (F0);
        load_float4_into_reg (ref *f, F0);
      }
      break;


      //   ( float8 --> float8 )

      case P_SYNC_FLT8:    // for m-code : force float8 value on top float stack
      {
        NODE* f = &fstack[fstack_count - 1];

        if (g_tracing)
          trace (" \n");

        free_fregister_except_for_this_pcode (F0);
        load_float8_into_reg (ref *f, F0);
      }
      break;


      //  ( addr --> addr )

      case P_SYNC_ADDR:     // for m-code : force addr value in register X0
      {
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace (" \n");

        free_register_except_for_this_pcode (X0);
        load_addr_into_reg (ref *a, X0);
      }
      break;


      //   ( addr int4 --> addr int4 )

      case P_SYNC_ADDR_INT4:  // for m-code : force addr value in register X1, and int4 in register X0
      {
        NODE* i = &istack[istack_count - 1];
        NODE* a = &astack[astack_count - 1];

        if (g_tracing)
          trace (" \n");

        free_register_except_for_this_pcode (X0);
        load_int4_into_reg (ref *i, X0);

        free_register_except_for_this_pcode (X1);
        load_addr_into_reg (ref *a, X1);
      }
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

      case P_SHADOW_4:   // for m-code : after a branch, we assert that an int4 is in lowest CPU register (X0).
      {
        NODE* i = &istack[istack_count++];

        if (g_tracing)
          trace (" \n");

        i->typ = 'i';
        i->kind = INT_REGISTER;
        i->reg = X0;
      }
      break;


      //   (  --> int8 )

      case P_SHADOW_8:    // for m-code : after a branch, we assert that an int8 is in lowest CPU register (X0).
      {
        NODE* i = &istack[istack_count++];

        if (g_tracing)
          trace (" \n");

        i->typ = 'l';
        i->kind = INT_REGISTER;
        i->reg = X0;
      }
      break;


  // --------------
  // 10. Aggregates
  // --------------

  // load address of aggregate on addr_stack, then store all fields :
  //
  // pop xx_stack, store value at addr_target + offset4

      case P_STORE_FIELD_BOOL: // <offset4>    (used for bool)         ( addr_target  bool   -->  addr_target )
      case P_STORE_FIELD_1:    // <offset4>    (used for int1, uint1)  ( addr_target  uint1  -->  addr_target )
      case P_STORE_FIELD_2:    // <offset4>                            ( addr_target  uint2  -->  addr_target )
      case P_STORE_FIELD_4:    // <offset4>                            ( addr_target  int    -->  addr_target )
      case P_STORE_FIELD_8:    // <offset4>                            ( addr_target  value8 -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4  offset, size;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        switch (g_current_pcode)
        {
          case P_STORE_FIELD_BOOL:   size = 1;   break;
          case P_STORE_FIELD_1:      size = 1;   break;
          case P_STORE_FIELD_2:      size = 2;   break;
          case P_STORE_FIELD_4:      size = 4;   break;
          case P_STORE_FIELD_8:      size = 8;   break;
          default:  abort;
        }

        make_simple_effective_address (ref *a);
        a->ea.offset += offset;

        if (i->kind == INT_CONSTANT)
        {
          move_memory_immediate (target => a->ea, imm => i->icte, size => size);  // 1, 2, 4 or 8
        }
        else
        {
          if (size <= 4)
            flush_int4_in_register (ref *i);
          else
            flush_int8_in_register (ref *i);

          c_store_register_in_memory (r => i->reg, ea => a->ea, size => size); // size = 1, 2, 4, 8
        }

        a->ea.offset -= offset;
        istack_count--;
      }
      break;


      case P_STORE_FIELD_FLT4:  // <offset4>          ( addr_target   f4 -->  addr_target )
      case P_STORE_FIELD_FLT8:  // <offset4>          ( addr_target   b8 -->  addr_target )
      {
        NODE* a = &astack[astack_count - 1];
        NODE* f = &fstack[fstack_count - 1];
        int4 offset, size;

        offset = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" offset = %d\n", offset);

        switch (g_current_pcode)
        {
          case P_STORE_FIELD_FLT4:  size = 4;   break;
          case P_STORE_FIELD_FLT8:  size = 8;   break;
          default:  abort;
        }

        make_simple_effective_address (ref *a);
        a->ea.offset += offset;

        if (f->kind == FLOAT_CONSTANT)
        {
          move_memory_fimmediate (target => a->ea, value => f->fcte, size => size);  // 4 or 8
        }
        else
        {
          if (size <= 4)
            flush_float4_in_register (ref *f);
          else
            flush_float8_in_register (ref *f);

          c_store_fregister_in_memory (r => f->freg, ea => a->ea, size => size); // size = 4 or 8
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

        make_simple_effective_address (ref *adr);
        make_simple_effective_address (ref *val);

        adr->ea.offset += offset;

        if (is_null_ea (val->ea))
        {
          c_store_register_in_memory (r => ZERO, ea => adr->ea, size => address_size);
        }
        else if (val->ea.base < X15 && val->ea.index == ZERO &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_store_register_in_memory (r => val->ea.base, ea => adr->ea, size => address_size);
        }
        else if (val->ea.base == ZERO && val->ea.index < X15 && val->ea.scale == 1 &&
                 val->ea.offset == 0 && val->ea.reloc.kind == RELOC_NONE)
        {
          c_store_register_in_memory (r => val->ea.index, ea => adr->ea, size => address_size);
        }
        else
        {
          REG r = allocate_register (*val);  // allows reusing registers of node val
          compute_effective_address_in_register (ea => val->ea, r => r);
          c_store_register_in_memory (r    => r,
                                      ea   => adr->ea,
                                      size => address_size);  // 1, 2, 4, 8
        }

        adr->ea.offset -= offset;
        astack_count--;
      }
      break;


      case P_STORE_FIELD_BLOCK:   // <offset4>  <size4>        ( addr_target   addr_src   -->  addr_target )
      {
        NODE* src = &astack[astack_count - 1];
        NODE* dst = &astack[astack_count - 2];
        int4 field_offset, block_size;

        field_offset = *((int4 *)&mem[mem_offset]);
        block_size   = *((int4 *)&mem[mem_offset+4]);
        mem_offset += 8;

        if (g_tracing)
          trace (" field_offset = %d, block_size = %d\n", field_offset, block_size);

        if (block_size > 0)
        {
          if (nb_register_moves_to_copy_size (block_size) <= 4)      // max 4 moves
          {
            REG regs[4];
            int chunk, ri, siz;

            make_simple_effective_address (ref *src); // make effective address that consists atmost of a base and a small offset
            make_simple_effective_address (ref *dst); // make effective address that consists atmost of a base and a small offset

            dst->ea.offset += field_offset;

            allocate_registers (out regs);  // 4 new regs

            // load in registers
            chunk = address_size;
            siz = block_size;
            ri = 0;

            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_load_register_from_memory (r => regs[ri], ea => src->ea, data_signed => false, size => chunk); // size = 1, 2, 4, 8
                src->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }

            // save from registers
            chunk = address_size;
            siz = block_size;
            ri = 0;
            while (siz > 0)
            {
              if (siz >= chunk)
              {
                c_store_register_in_memory (r => regs[ri], ea => dst->ea, size => chunk); // size = 1, 2, 4, 8
                dst->ea.offset += chunk;
                ri++;
                siz -= chunk;
              }
              else
              {
                chunk >>= 1;
              }
            }

            src->ea.offset -= block_size;
            dst->ea.offset -= block_size;

            dst->ea.offset -= field_offset;
          }
          else
          {
            PCODE next_pcode;
            next_pcode'byte = mem[mem_offset:next_pcode'size];

            // all registers must be flushed to temporaries before calling OS
            store_all_registers_in_temporaries_except_for_this_pcode ();

            move_register_immediate (target => X2, imm => block_size, size => 4);
            load_addr_into_reg (ref *src,  X1);

            if (next_pcode != P_DROP_ADDR)   // we need to keep target address for next pcode
            {
              load_addr_into_reg (ref *dst,  X19);

              add_offset_using_x17 (target => X0,     // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                    source => X19,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                    offset => field_offset,
                                    size   => 8);    // 4 or 8
            }
            else  // no need to save target address for next pcode
            {
              load_addr_into_reg (ref *dst,  X0);

              add_offset_using_x17 (target => X0,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                                    source => X0,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                                    offset => field_offset,
                                    size   => 8);    // 4 or 8
            }

            call_libc ("memmove");

            // note that destination address (dst) is kept for the next pcode !
            if (next_pcode != P_DROP_ADDR)   // we don't drop it, restore destination address for next pcode
              c_mov_reg_reg (target => X0, source => X19, data_size => 8);   // restore target address
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

        // all registers must be flushed to temporaries before calling OS
        store_all_registers_in_temporaries_except_for_this_pcode ();

        if (i->kind == INT_CONSTANT)
          move_register_immediate (target => X0, imm => i->icte, size => 4);
        else
          load_int4_into_reg (ref *i, X0);

        if (fill_zeroes && i->kind != INT_CONSTANT)
          c_mov_reg_reg (target => X20, source => X0, data_size => 8);   // save size in X20 for later

        call_libc ("malloc");

        istack_count--;

        {
          NODE* a;
          a = &astack[astack_count++];
          a->typ = 'a';
          a->kind = EFFECTIVE_ADDRESS;
          a->ea.base = X0;
          a->ea.index = ZERO;
          a->ea.scale = 1;
          a->ea.offset = 0;
          a->ea.reloc.kind = RELOC_NONE;
          a->ea.reloc.nr = 0;
        }

        if (fill_zeroes)
        {
          // memset (addr, 0x00, size)
          c_mov_reg_reg (target => X19, source => X0, data_size => 8);   // save address
          move_register_immediate (target => X1, imm => 0, size => 8);   // value to fill

          if (i->kind == INT_CONSTANT)
            move_register_immediate (target => X2, imm => i->icte, size => 4);
          else
            c_mov_reg_reg (target => X2, source => X20, data_size => 8);   // size

          call_libc ("memset");
          c_mov_reg_reg (target => X0, source => X19, data_size => 8);   // restore address
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

        // all registers must be flushed to temporaries before calling OS
        store_all_registers_in_temporaries_except_for_this_pcode ();

        load_addr_into_reg (ref *a, r => X0);
        call_libc ("free");

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

        load_addr_into_reg (ref *a, X0);
        move_register_immediate (target => X1, imm => typ, size => 4);   // 1, 2, 4 or 8

        // X0 = address returned by malloc()
        // X1 = typ
        call_function (func_allocate_tombstone);

        a->kind = EFFECTIVE_ADDRESS;
        a->ea.base = X0;  a->ea.index = ZERO;  a->ea.scale = 1;  a->ea.offset = 0;
        a->ea.reloc.kind = RELOC_NONE;  a->ea.reloc.nr = 0;
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

        load_addr_into_reg (ref *a, r => X0);
        move_register_immediate (target => X1, imm => typ, size => 4);

        // X0 = address of tombstone
        // X1 = type (4 bytes)
        call_function (func_free_tombstone);

        astack_count--;
      }
      break;


  // --------------
  // 12. statements
  // --------------

      //  ( --> )

      case P_INIT_THREADS:
      {
        int4 stack_size;
        EA   ea;

        stack_size = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace ("\n");

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 40;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X0);
        call_libc ("pthread_attr_init");

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 40;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X0);
        move_register_immediate (X1, imm => stack_size, size => 8);    // stack size
        call_libc ("pthread_attr_setstacksize");

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 40;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X0);
        move_register_immediate (X1, imm => 1, size => 8);    // PTHREAD_CREATE_DETACHED
        call_libc ("pthread_attr_setdetachstate");
      }
      break;


      //  (  code_addr  -->   int4  )   stack frames of threads need to be 16-byte aligned.

      case P_RUN_VOID_PARAM:     // <near_label>
      {
        NODE* a = &astack[astack_count - 1];
        int4  near_label_nr;
        EA    ea;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" label #%d\n", near_label_nr);

        store_all_registers_in_temporaries_except_for_this_pcode ();

        load_addr_into_reg (ref *a, X2);                      // start address
        move_register_immediate (X3, imm => 0, size => 4);    // argument

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 32;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X0);

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 40;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X1);

        call_libc ("pthread_create");

        // TST (shifted register)
        // sets condition flags
        c_tst_reg (source => X0,      // ZERO allowed (SP not allowed)
                   size   => 4);      // 4 or 8

        // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
        // used for: b = (x < y);
        c_cset (cmp    => CMP_NOT_EQUAL,   // 1 if RC != 0, 0 otherwise
                signed => false,
                target => X0,
                size   => 8);      // 4 or 8

        c_neg_reg_reg (target => X0,  // ZERO allowed (SP not allowed)
                       source => X0,  // ZERO allowed (SP not allowed)
                       size   => 8);  // 4 or 8

        astack_count--;

        {
          NODE* i;
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = X0;
        }
      }
      break;


      //  (  code_addr  param_int4  -->   int4  )

      case P_RUN_INT4_PARAM:     // <near_label>
      {
        NODE* a = &astack[astack_count - 1];
        NODE* i = &istack[istack_count - 1];
        int4  near_label_nr;
        EA    ea;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" near_label_nr = #%d\n", near_label_nr);

        store_all_registers_in_temporaries_except_for_this_pcode ();

        load_addr_into_reg (ref *a, X2);  // start address
        load_int4_into_reg (ref *i, X3);  // argument

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 32;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X0);

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 40;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X1);

        call_libc ("pthread_create");

        // TST (shifted register)
        // sets condition flags
        c_tst_reg (source => X0,      // ZERO allowed (SP not allowed)
                   size   => 4);      // 4 or 8

        // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
        // used for: b = (x < y);
        c_cset (cmp    => CMP_NOT_EQUAL,   // 1 if RC != 0, 0 otherwise
                signed => false,
                target => X0,
                size   => 8);      // 4 or 8

        c_neg_reg_reg (target => X0,  // ZERO allowed (SP not allowed)
                       source => X0,  // ZERO allowed (SP not allowed)
                       size   => 8);  // 4 or 8

        astack_count--;
        istack_count--;

        {
          i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = X0;
        }
      }
      break;


      //  (  code_addr  param_addr -->   int4  )

      case P_RUN_ADDR_PARAM:     // <near_label>
      {
        NODE* param = &astack[astack_count - 1];
        NODE* start = &astack[astack_count - 2];
        int4  near_label_nr;
        EA    ea;

        near_label_nr = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" near_label_nr = #%d\n", near_label_nr);

        store_all_registers_in_temporaries_except_for_this_pcode ();

        load_addr_into_reg (ref *start, X2);  // start address
        load_addr_into_reg (ref *param, X3);  // argument

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 32;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X0);

        clear ea;
        ea.base = ZERO;  ea.index = ZERO;  ea.scale = 1;  ea.offset = 40;   ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
        compute_effective_address_in_register (ea, X1);

        call_libc ("pthread_create");

        // TST (shifted register)
        // sets condition flags
        c_tst_reg (source => X0,      // ZERO allowed (SP not allowed)
                   size   => 4);      // 4 or 8

        // CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
        // used for: b = (x < y);
        c_cset (cmp    => CMP_NOT_EQUAL,   // 1 if RC != 0, 0 otherwise
                signed => false,
                target => X0,
                size   => 8);      // 4 or 8

        c_neg_reg_reg (target => X0,  // ZERO allowed (SP not allowed)
                       source => X0,  // ZERO allowed (SP not allowed)
                       size   => 8);  // 4 or 8

        astack_count -= 2;

        {
          NODE* i = &istack[istack_count++];
          i->typ = 'i';
          i->kind = INT_REGISTER;
          i->reg = X0;
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
          int siz, chunk;

          if (a->kind != EFFECTIVE_ADDRESS || a->ea.index != ZERO || a->ea.reloc.kind != RELOC_NONE)
            load_addr_into_reg (ref *a, allocate_register (*a));

          chunk = 8;

          siz = (int)i->icte;    // size to clear

          while (siz > 0)
          {
            if (siz >= chunk)
            {
              c_store_register_in_memory (r => ZERO, ea => a->ea, size => chunk); // size = 1, 2, 4, 8
              a->ea.offset += chunk;
              siz -= chunk;
            }
            else
            {
              chunk >>= 1;
            }
          }
        }
        else
        {
          // all registers must be flushed to temporaries before calling OS
          store_all_registers_in_temporaries_except_for_this_pcode ();

          load_int4_into_reg (ref *i, X2);
          load_addr_into_reg (ref *a, X0);

          c_mov_reg_reg (target    => X1,    // ZERO allowed (SP not allowed)
                         source    => ZERO,  // ZERO allowed (SP not allowed)
                         data_size => 4);    // 4 or 8

          call_libc ("memset");
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

        // SMC Secure Monitor Call
        c_smc (0); // 0 to 65535
      }
      break;


      //  (  / --->   /   )
      case P_ASSERT:   //  <label_nr>   ; effect : define a label below this function for a failed assertion.
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

      case P_SLEEP_CTE:   // <uint4>  ; sleep <uint4> milliseconds
      {
        int4 cte = *((int4 *)&mem[mem_offset]);
        mem_offset += 4;

        if (g_tracing)
          trace (" %d msecs\n", cte);

        // all registers must be flushed to temporaries before calling OS
        store_all_registers_in_temporaries_except_for_this_pcode ();

        move_register_immediate (target => X0, imm => cte / 1000, size => 4);   // 4 or 8 (SECS)
        move_register_immediate (target => X1, imm => cte % 1000, size => 4);   // 4 or 8 (MILLISECS)
        call_function (func_usleep);
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

        // all registers must be flushed to temporaries before calling OS
        store_all_registers_in_temporaries_except_for_this_pcode ();

        load_int4_into_reg (ref *i, r => X0);   // signed-extend to 8 bytes of register

        c_cmp_reg_imm (source     => i->reg,    // SP allowed (ZERO not allowed)
                       imm12      => 0,         // 0 to 4095
                       shl_imm_12 => false,     // true to shift imm12 << 12
                       size       => 4);        // 4 or 8

        cond_branch (CMP_SMALLER, signed => true, near_label => near_label_nr);

        move_register_immediate (target => X1, imm => 0, size => 4);   // 4 or 8 (zero MILLISECS)
        call_function (func_usleep);

        declare_near_label (near_label_nr);

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

        // all registers must be flushed to temporaries before calling OS
        store_all_registers_in_temporaries_except_for_this_pcode ();

        flush_float4_in_register_for_modif (ref *f);

        {
          FREG freg = allocate_fregister();

          // uses X15
          move_fregister_immediate (target => freg, imm => 1000.0, size => 4);

          c_fmul (target  => f->freg,
                  source1 => f->freg,
                  source2 => freg,
                  size    => 4);

          c_conv_float_to_int (target        => X2,      // can be ZERO
                               target_signed => true,
                               target_size   => 4,       // 4 or 8
                               source        => f->freg,
                               source_size   => 4);      // 4 or 8
        }

        c_cmp_reg_imm (source     => X2,        // SP allowed (ZERO not allowed)
                       imm12      => 0,         // 0 to 4095
                       shl_imm_12 => false,     // true to shift imm12 << 12
                       size       => 4);        // 4 or 8

        cond_branch (CMP_SMALLER, signed => true, near_label => near_label_nr);

        // w2 contains delay in milliseconds

        // X2 / 1000 -> X0
        // to divide by 1000, 64-bit multiply by 2199023256 and shift right by 41

        move_register_immediate (target => X3, imm => 2199023256, size => 4);

        // X0 = w2 * w3
        c_umul_32_32_64_add_64 (xtarget => X0,      // ZERO allowed
                                xsum    => ZERO,    // ZERO allowed
                                wmul1   => X2,      // ZERO allowed
                                wmul2   => X3);     // ZERO allowed

        // unsigned shift !
        c_lsr_imm (target    => X0,  // ZERO allowed
                   source    => X0,  // SP allowed (base effective address)(64 bit address)
                   shifts    => 41,  // 1 to 63  (or 1 to 31 for size 4)
                   data_size => 8);  // 4 or 8

        // w0 contains delay / 1000

        // X2 - X0 * 1000 -> X1

        move_register_immediate (target => X3, imm => 1000, size => 4);

        // X1 = w0 * w3
        c_mult (target    => X1,   // ZERO allowed
                mul1      => X0,   // ZERO allowed
                mul2      => X3,   // ZERO allowed
                data_size => 4);   // 4 or 8


        // w1 contains delay / 1000 * 1000

        // X1 = X2 - X1
        c_sub_reg_reg (target              => X1,   // ZERO allowed (SP not allowed)
                       source1             => X2,   // ZERO allowed (SP not allowed)
                       source2             => X1,   // ZERO allowed (SP not allowed)
                       source2_shift_type  => LSL,  // LSL, LSR, ASR
                       source2_shift_value => 0,    // range 0..31 (or 0..63 for size==8)
                       size                => 4);   // 4 or 8

        // w0 = SECS
        // w1 = MILLISECS  (0 .. 999)

        call_function (func_usleep);

        declare_near_label (near_label_nr);

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
          trace (" unit %d line %d\n", key, line);

        if (g_current_unit_key != key)
        {
          dbg_new_unit (key);
          error_set_code_generator_unit_key (key);

          if (g_current_unit_key == 0)  // very first unit
            dbg_store_line (line => line, ip => 0);  // first line of main program is valid during initialization

          g_current_unit_key = key;
          g_current_source_line = 0;
        }

        if (g_current_source_line != line)
        {
          dbg_store_line (line => line, ip => (int)current_RIP());
          error_set_code_generator_source_line (line);
          g_current_source_line = line;
        }
      }
      break;

      default:
//        printf ("unsupported pcode : %s\n", g_current_pcode'string);
        fatal_compiler_error0 ("generate_asm_for_function (unsupported pcode)");
        break;
    }

    if (g_after_i != istack_count || g_after_f != fstack_count || g_after_a != astack_count)
      fatal_compiler_error0 ("generate_asm_for_function (bad xx_stack indexes)");
  }

  if (istack_count != 0 || fstack_count != 0 || astack_count != 0)
    fatal_compiler_error0 ("generate_asm_for_function (stacks_counters_not_zero)");


  // add extra code for reserving stack space and probing each 4K page

  // when the compiler detects that a function is allocating more
  // than 4K of local data, it generates code that touches (writes) the allocated
  // data sequentially, from high to low, 4K at a time. Whenever the guard
  // page is touched, a new page is committed 4K below it and the newly
  // committed page becomes the guard page; the compiler stack probe
  // routine prevents a stack fault from occurring this way.

  if (g_extra_frame_bytes_pos != 0 && g_extra_frame_size + g_max_extra_param_call_size > 0)
  {
    uint extra;
    int  nb_pages, rest_offset, bytes_to_insert, i;
    int  idx;

    extra = g_extra_frame_size + g_max_extra_param_call_size;

    assert (extra & 15) == 0;  // check it's M16

    nb_pages    = ((int)extra >> 12);
    rest_offset = (int)extra & 4095;

    bytes_to_insert = nb_pages * 8;   // 2 instructions per 4K page
    if (rest_offset >= 257)   // 257 .. 4095
      bytes_to_insert += 8;
    else if (rest_offset > 0) // 1 .. 256
      bytes_to_insert += 4;

    idx = blob_index (g_blob_code);

    fixup.insert_bytes_in_code (pos => g_extra_frame_bytes_pos, size_increase => bytes_to_insert, extend_instruction => false);

    g_enter_pcode_inserted_bytes = (int)bytes_to_insert;

    blob_set_index (ref g_blob_code, index => g_extra_frame_bytes_pos);

    for (i=0; i<nb_pages; i++)
    {
      c_sub_reg_imm (target     => SP,
                     source     => SP,
                     imm12      => 1,    // 0 to 4095
                     shl_imm_12 => true, // true to shift imm12 << 12
                     size       => 8);

      assert c_store_ofs8 (source    => ZERO,   // ZERO allowed
                           base      => SP,     // SP allowed (effective address)
                           offset    => 0,      // 9 bits (-256 to 255) to be added to base address
                           data_size => 8);     // size of data to load : 1, 2, 4, 8
    }

    if (rest_offset >= 257)   // 257 .. 4095
    {
      c_sub_reg_imm (target     => SP,
                     source     => SP,
                     imm12      => (int)rest_offset,  // 0 to 4095
                     shl_imm_12 => false,             // true to shift imm12 << 12
                     size       => 8);

      assert c_store_ofs8 (source    => ZERO,   // ZERO allowed
                           base      => SP,     // SP allowed (effective address)
                           offset    => 0,      // 9 bits (-256 to 255) to be added to base address
                           data_size => 8);     // size of data to load : 1, 2, 4, 8
    }
    else if (rest_offset > 0)   // 1 .. 256
    {
      c_store_add (source    => ZERO,      // ZERO allowed
                   base      => SP,
                   offset    => -(int)rest_offset,  // 9 bits (-256 to 255) to be added to base address
                   pre_add   => true,               // false : post_add base by offset, true : pre_add base by offset
                   data_size => 8);                 // 1, 2, 4 or 8
    }

    blob_set_index (ref g_blob_code, index => idx + (int)bytes_to_insert);
  }

  // insert code for P_LEAVE
  fixup.leave_loop_on_all (insert_leave_code);


  // generate list of interrupt labels for handling errors
  traverse_ll_tree (ll_operate);
  close_ll_tree ();

  dbg_store_line (line => 0, ip => (int)current_RIP());      // add closing node to complete interval


  // shrink/extend near_label unconditional and conditional jumps
  // possibly expand code by converting label offset fields

  fixup.near_labels_relocate_all ();

#end unsafe
}

// ----------------------------------------------------------------------------------------------
