
// fixup.c

from std use strings;
use symbtab, error, blob, blob2, common, pool, dbginfo;
use arm/arm64;

//---------------------------------------------------------------------------------------------------

string^ new_string (string name)
{
  return new string ' (name[0 : strlen(name)]);
}

//---------------------------------------------------------------------------------------------------

wstring^ new_wstring (wstring name)
{
  return new wstring ' (name[0 : wstrlen(name)]);
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

// EXPORT

package THE_EXPORTS

  struct EXPORTS
  {
    wstring^  name;
    uint      fixup_offset_in_file;
    uint      code_ip;
  }

  package Exports = new SymbolTable (ELEMENT => EXPORTS);
  Exports.SYMBOL_TABLE g_exports;

  OPERATE_EXPORT_FUNCTION1 g_export_op1;
  OPERATE_EXPORT_FUNCTION2 g_export_op2;

end THE_EXPORTS;

//---------------------------------------------------------------------------------------------------

int compare_exports (EXPORTS a, EXPORTS b)
{
  return wstrcmp (a.name^, b.name^);
}

public bool export_register (wstring name)
{
  return Exports.insert (ref g_exports,
                             EXPORTS ' {name                 => new_wstring (name),
                                        fixup_offset_in_file => 0,
                                        code_ip              => 0});
}

public void export_set_code_ip (wstring name, uint code_ip)
{
  EXPORTS key, data;

  clear key;
  key.name = new_wstring (name);
  assert Exports.retrieve (ref g_exports, key, out data);
  free key.name;

  data.code_ip = code_ip;
  assert Exports.update (ref g_exports, data);
}

void operate_export_function1 (EXPORTS exports)
{
  g_export_op1 (exports.name^);
}

public void loop1_on_export_functions (OPERATE_EXPORT_FUNCTION1 op)
{
  g_export_op1 = op;
  Exports.traverse (ref g_exports, operate_export_function1);
}

public void export_set_fix_offset_in_file (wstring name, uint fixup_offset_in_file)
{
  EXPORTS key, data;

  clear key;
  key.name = new_wstring (name);
  assert Exports.retrieve (ref g_exports, key, out data);
  free key.name;

  data.fixup_offset_in_file = fixup_offset_in_file;
  assert Exports.update (ref g_exports, data);
}

void operate_export_function2 (EXPORTS exports)
{
  g_export_op2 (fixup_offset_in_file => exports.fixup_offset_in_file,
                code_ip              => exports.code_ip);
}

public void loop2_on_export_functions (OPERATE_EXPORT_FUNCTION2 op)
{
  g_export_op2 = op;
  Exports.traverse (ref g_exports, operate_export_function2);
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

// FUNCTION CALLS

package THE_FUNCTIONS

  struct FUNCTIONS
  {
    int   label_nr;
    uint  code_ip;
  }

  package Functions = new SymbolTable (ELEMENT => FUNCTIONS);
  Functions.SYMBOL_TABLE g_functions;

end THE_FUNCTIONS;

//---------------------------------------------------------------------------------------------------

int compare_functions (FUNCTIONS a, FUNCTIONS b)
{
  if (a.label_nr < b.label_nr)
    return -1;
  else if (a.label_nr > b.label_nr)
    return +1;
  else
    return 0;
}

//---------------------------------------------------------------------------------------------------

// called when generating code for a function, to declare it and fix its IP
public
void function_register (int  label_nr,
                        uint code_ip)   // ip of function in code segment
{
  assert Functions.insert (ref g_functions,
                               FUNCTIONS ' {label_nr => label_nr,
                                            code_ip  => code_ip});
}

//---------------------------------------------------------------------------------------------------

public
uint function_get_ip (int label_nr)
{
  FUNCTIONS func;

  clear func;
  func.label_nr = label_nr;

  assert Functions.retrieve (ref g_functions, func, out func);

  return func.code_ip;
}

//---------------------------------------------------------------------------------------------------

package FUNC_FILL_DATA

  // a list of addresses to backfill, in decreasing backfill order

  struct FIX_FUNC_INFO
  {
    int            fill_position;    // pos of asm instruction to fill
    int            func_label_nr;
    FIX_FUNC_INFO^ next;
  }

  FIX_FUNC_INFO^ g_func_reloc_list;   // in reverse backfill address order

end FUNC_FILL_DATA;

//---------------------------------------------------------------------------------------------------

public void func_store_backfill (int func_label_nr, int fill_position)
{
  FIX_FUNC_INFO^ n;

  n = new FIX_FUNC_INFO;
  n^.fill_position = fill_position;
  n^.func_label_nr = func_label_nr;
  n^.next = g_func_reloc_list;

  g_func_reloc_list = n;
}

//---------------------------------------------------------------------------------------------------

void move_all_func_backfills (int pos, int displacement)
{
  FIX_FUNC_INFO^ n = g_func_reloc_list;

  while (n != null && n^.fill_position >= pos)
  {
    n^.fill_position += displacement;
    n = n^.next;
  }
}

//---------------------------------------------------------------------------------------------------

void fill_all_func_backfills ()
{
  FIX_FUNC_INFO^ n = g_func_reloc_list;

  while (n != null)
  {
    int func_ip = (int)function_get_ip (label_nr => n^.func_label_nr);

    int offset = func_ip - n^.fill_position;

    if (offset >= -(1<<27) && offset < (1<<27))  // +/- 128 MB is ok
      ;
    else
      fatal_compiler_error0 ("function larger than 128 MB (2)");

    blob_set_index (ref g_blob_code, index => n^.fill_position);
    c_jsr (offset => offset);

    n = n^.next;
  }
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
//--------------------------------------

// IMPORT

// shared objects to import

package P_IMPORTS

  struct SO_INFO
  {
    string^ so_name;
    int     str_table_index;
  }

  package shared_objs = new SymbolTable (ELEMENT => SO_INFO);
  shared_objs.SYMBOL_TABLE g_shared_objs;

  TREAT_SO_NAME1  g_import_treat_so1;
  TREAT_SO_NAME2  g_import_treat_so2;

  //--------------------------------------

  // functions to import

  struct IMPORT_INFO
  {
    string^ import_func_name;
    int     slot_nr;                 // index in ".got.plt" table entry slot
  }

  package import_funcs = new SymbolTable (ELEMENT => IMPORT_INFO);
  import_funcs.SYMBOL_TABLE g_import_funcs;

  int g_nb_imported_funcs;

  TREAT_IMPORT_FUNC1  g_import_treat_import_func1;
  int g_import_treat_import_seq;

end P_IMPORTS;

//------------------

int compare_shared_objs (SO_INFO a, SO_INFO b)
{
  return strcmp (a.so_name^, b.so_name^);
}

int compare_import_funcs (IMPORT_INFO a, IMPORT_INFO b)
{
  return strcmp (a.import_func_name^, b.import_func_name^);
}

//---------------------------------------------------------------------------------------------------

// returns slot nr in got plt table

public int register_imported_shared_object_and_func (string so, string func)
{
  int ret;

  shared_objs.insert (ref g_shared_objs,
                          SO_INFO ' {so_name => new_string(so),
                                     str_table_index => 0});

  if (import_funcs.insert (ref g_import_funcs,
                               IMPORT_INFO ' {import_func_name => new_string(func),
                                              slot_nr          => g_nb_imported_funcs}))
  {
    ret = g_nb_imported_funcs;
    g_nb_imported_funcs++;
  }
  else   // insert failed (already exists)
  {
    string^  p = new_string(func);
    IMPORT_INFO import_func;

    clear import_func;
    import_func.import_func_name = p;

    assert import_funcs.retrieve (ref g_import_funcs, import_func, out import_func);

    free p;

    ret = import_func.slot_nr;
  }

  return ret;
}

//------------------

public int nb_of_imported_funcs ()
{
  return g_nb_imported_funcs;
}

//------------------

void treat_so_name1 (SO_INFO s)
{
  SO_INFO so2;

  so2 = s;
  g_import_treat_so1 (s.so_name^, out so2.str_table_index);

  assert shared_objs.update (ref st   => g_shared_objs,
                                 data => so2);
}

public
void import_loop_so_names1 (TREAT_SO_NAME1 treat)
{
  g_import_treat_so1 = treat;

  shared_objs.traverse (ref st      => g_shared_objs,
                            operate => treat_so_name1);
}

void treat_so_name2 (SO_INFO s)
{
  g_import_treat_so2 (s.str_table_index);
}

public
void import_loop_so_names2 (TREAT_SO_NAME2 treat)
{
  g_import_treat_so2 = treat;

  shared_objs.traverse (ref st      => g_shared_objs,
                            operate => treat_so_name2);
}

//------------------

void treat_import_func1 (IMPORT_INFO info)
{
  g_import_treat_import_func1 (info.import_func_name^,
                               g_import_treat_import_seq++,
                               info.slot_nr);
}

public void import_loop_import_funcs1 (TREAT_IMPORT_FUNC1 treat)
{
  g_import_treat_import_func1 = treat;
  g_import_treat_import_seq = 0;

  import_funcs.traverse (ref st      => g_import_funcs,
                             operate => treat_import_func1);
}

//------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

// PCODE LEAVE

package LEAVE_DATA
  int[]^ g_leave_table;
  int g_leave_count;
end LEAVE_DATA;

public
void leave_clear_all ()   // called at the start of each new function
{
  g_leave_count = 0;
  // don't both free table, it will be reused for next function
}

public
void leave_register (int backfill_pos)   // save point in blob code where to add code
{
  if (g_leave_table^'length == g_leave_count)   // table is full
  {
    int[]^ old = g_leave_table;
    g_leave_table = new int[g_leave_count * 2];   // allocate a new one with double size
    g_leave_table^[0:g_leave_count] = old^[0:g_leave_count];
    free old;
  }

  g_leave_table^[g_leave_count++] = backfill_pos;
}

public
void leave_loop_on_all (LEAVE_INSERT_CODE func)   // loop on all, in reverse order (from last to first)
{
  int i;
  ref int[] table = g_leave_table^;
  for (i=g_leave_count-1; i>=0; i--)
    func (table[i]);
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

package FILL_CODE_POOL_GLOBAL

  // a list of function, pool and global addresses to backfill in code, in decreasing fill position

  struct CODE_POOL_GLOBAL_FILL_INFO
  {
    RELOC_KIND             kind;             // RELOC_FUNC, RELOC_POOL, RELOC_GLOBAL or RELOC_DLL
    int8                   nr;               // function, constant nr, global address, slot nr
    FILL_TYP               typ;              // ADRP_PAGE_4K, ADD_OFFSET_4095 or LOAD_STORE_OFFSET_4095
    int                    extra_offset;     // to be added to global/pool address
    int                    data_size_shifts; // used for LOAD_STORE_OFFSET_4095 only
    int                    fill_position;    // position in code blob
    CODE_POOL_GLOBAL_FILL_INFO^ next;
  }

  CODE_POOL_GLOBAL_FILL_INFO^ g_code_pool_global_fill_list;   // in reverse fill position

end FILL_CODE_POOL_GLOBAL;


public
void register_reloc (RELOC_KIND kind,             // RELOC_FUNC, RELOC_POOL, RELOC_GLOBAL or RELOC_DLL
                     int8       nr,               // function nr, constant nr, global address, slot nr
                     FILL_TYP   typ,              // ADRP_PAGE_4K, ADD_OFFSET_4095 or LOAD_STORE_OFFSET_4095
                     int        extra_offset,     // to be added to code/pool/global address
                     int        data_size_shifts, // used for LOAD_STORE_OFFSET_4095 only
                     int        fill_position)    // position in code blob
{
  g_code_pool_global_fill_list =
     new CODE_POOL_GLOBAL_FILL_INFO '
         {
           kind             => kind,
           nr               => nr,
           typ              => typ,
           extra_offset     => extra_offset,
           data_size_shifts => data_size_shifts,
           fill_position    => fill_position,
           next             => g_code_pool_global_fill_list,
         };
}

//---------------------------------------------------------------------------------------------------

void move_all_pool_and_global_backfills (int pos, int displacement)
{
  CODE_POOL_GLOBAL_FILL_INFO^ n = g_code_pool_global_fill_list;

  while (n != null && n^.fill_position >= pos)
  {
    n^.fill_position += displacement;
    n = n^.next;
  }
}

//---------------------------------------------------------------------------------------------------

public
void backfill_globals_pools_references_in_code (uint start_of_code_mem,
                                                uint start_of_data_blob_mem,
                                                uint start_of_bss_mem,
                                                uint got_plt_base_address)
{
  CODE_POOL_GLOBAL_FILL_INFO^ n = g_code_pool_global_fill_list;

  while (n != null)
  {
    ref CODE_POOL_GLOBAL_FILL_INFO r = n^;
    int backfill_addr = (int)start_of_code_mem + r.fill_position;
    int glob_addr;
    uint instruction;

    if (r.kind == RELOC_FUNC)
    {
      glob_addr = (int)start_of_code_mem + (int)function_get_ip (label_nr => (int)r.nr);
    }
    else if (r.kind == RELOC_POOL)
    {
      glob_addr = (int)start_of_data_blob_mem + pool.position_of_pool_cte_in_data_blob (r.nr) + r.extra_offset;
    }
    else if (r.kind == RELOC_GLOBAL)
    {
      glob_addr = (int)start_of_bss_mem + (int)r.nr + r.extra_offset;
    }
    else if (r.kind == RELOC_DLL)
    {
      glob_addr = (int)got_plt_base_address + 8 * (int)r.nr;   // vector address in got_plt
    }
    else
    {
      abort;
    }

#begin unsafe
    instruction'byte = blob_ptr(g_blob_code)[r.fill_position:4];
#end unsafe

    switch (r.typ)
    {
      case ADRP_PAGE_4K:
        {
          int high_ofs = (glob_addr >> 12) - (backfill_addr >> 12);

          instruction |= (uint)(((high_ofs >> 2) & 0b1111111111111111111) << 5)
                       + (uint)((high_ofs & 0b11) << 29);
        }
        break;

      case ADD_OFFSET_4095:
        {
          int low_ofs = glob_addr & 4095;

          instruction |= ((uint)low_ofs << 10);
        }
        break;

      case LOAD_STORE_OFFSET_4095:
        {
          int low_ofs = glob_addr & 4095;
          int ofs = low_ofs >> r.data_size_shifts;

          assert (ofs << r.data_size_shifts) == low_ofs;   // make sure it's aligned

          instruction |= ((uint)ofs << 10);
        }
        break;

      default:
        abort;
    }

#begin unsafe
    blob_ptr(g_blob_code)[r.fill_position:4] = instruction'byte;
#end unsafe

    n = n^.next;
  }
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------

// NEAR LABELS

package LABELS

  int[]^ g_pnear_labels;          // contains 0 or IP of label
  int    g_nb_near_labels;

  //------------------------------------------------------------------------

  // a list of near addresses to backfill, in decreasing fill position

  struct RELOC_NEAR_INFO
  {
    int              fill_position;    // IP of address to fill
    int              label_nr;         // branch target label nr
    BRANCH_TYP       typ;
    bool             was_expanded;
    RELOC_NEAR_INFO^ next;
  }

  RELOC_NEAR_INFO^ g_reloc_near_list;   // in reverse fill position

end LABELS;

//------------------------------------------------------------------------

// to be called after p-code for a function was generated, before generating asm.
// we know in advance how many labels we need !

public void near_label_allocate_table (int nb_labels)
{
  free (g_pnear_labels);

  g_pnear_labels = new int [ nb_labels ];
  g_nb_near_labels = nb_labels;
}

//------------------------------------------------------------------------

public void near_label_declare (int label_nr, int code_position)
{
  if (label_nr < 0 || label_nr >= g_nb_near_labels)
    fatal_compiler_error0 ("near_label_declare(1)");

  g_pnear_labels^[label_nr] = code_position;
}

//------------------------------------------------------------------------

public void near_label_branch (int label_nr, BRANCH_TYP typ, int code_position)
{
  RELOC_NEAR_INFO^ n;

  n = new RELOC_NEAR_INFO;
  n^.fill_position = code_position;
  n^.label_nr      = label_nr;
  n^.typ           = typ;
  n^.was_expanded  = false;
  n^.next          = g_reloc_near_list;

  g_reloc_near_list = n;
}

//------------------------------------------------------------------------

void move_near_labels (int pos, int displacement)
{
  int              i;
  RELOC_NEAR_INFO^ p;

  // move label positions
  for (i=0; i<g_nb_near_labels; i++)
  {
    if (g_pnear_labels^[i] >= pos)
      g_pnear_labels^[i] += displacement;
  }

  // move branch instruction positions
  p = g_reloc_near_list;
  while (p != null && p^.fill_position >= pos)
  {
    p^.fill_position += displacement;
    p = p^.next;
  }
}

//------------------------------------------------------------------------

public void insert_bytes_in_code (int pos, int size_increase, bool extend_instruction)
{
  int dpos = pos;

  if (extend_instruction)
    dpos++;   // do not displace labels or relocation at pos instruction itself, since we expand the instruction

  move_all_func_backfills            (dpos, size_increase);
  move_all_pool_and_global_backfills (dpos, size_increase);
  move_near_labels                   (dpos, size_increase);
  move_dbg_lines                     (dpos, size_increase);

  blob_insert_bytes (ref g_blob_code, pos => pos, nb_bytes => size_increase);
}

//------------------------------------------------------------------------

// to be called after generating asm for each function

void expand_branch_offsets ()
{
  bool changes_done;

  for (;;)
  {
    RELOC_NEAR_INFO^ n;
    int              offset, size_increase;

    changes_done = false;

    for (n=g_reloc_near_list; n!=null; n=n^.next)
    {
      ref RELOC_NEAR_INFO r = n^;

      if (r.was_expanded)
        continue;

      offset = g_pnear_labels^[r.label_nr] - r.fill_position;

      if (r.typ == ARM_BRANCH)
      {
        if (offset >= -(1<<27) && offset < (1<<27))  // +/- 128 MB is ok
          continue;

        fatal_compiler_error0 ("function larger than 128 MB (1)");

        size_increase = 4;
      }
      else if (r.typ == ARM_CONDITIONAL_BRANCH ||
               r.typ == ARM_COMPARE_AND_BRANCH)
      {
        if (offset >= -(1<<20) && offset < (1<<20))  // +/- 1 MB is ok
          continue;

        size_increase = 4;
      }
      else
        abort;

      insert_bytes_in_code (pos => r.fill_position, size_increase => size_increase, extend_instruction => true);

      r.was_expanded = true;

      changes_done = true;
    }

    if (!changes_done)
      break;
  }
}

//------------------------------------------------------------------------

// to be called after generating asm for each function

void backfill_all_near_branch_offsets ()
{
#begin unsafe
  RELOC_NEAR_INFO^ n;

  for (n=g_reloc_near_list; n!=null; n=n^.next)
  {
    ref RELOC_NEAR_INFO r = n^;

    blob_set_index (ref g_blob_code, index => r.fill_position);

    if (r.was_expanded)     // expanded
    {
      if (r.typ == ARM_BRANCH)
      {
        abort;
      }
      else if (r.typ == ARM_CONDITIONAL_BRANCH)
      {
        // +4 because our instruction was moved 4 bytes further down by the extension
        byte mask = (byte)(blob_ptr(g_blob_code)[r.fill_position+4] & 15);  // save 4 bits

        // cond jump over next instruction (branch) with inverse condition
        c_cond_branch (cmp    => CMP_EQUAL,
                       signed => false,
                       offset => 8);

        // restore 4 bits toggled by 1 to inverse condition
        blob_ptr(g_blob_code)[r.fill_position] |= (byte)(mask ^ 1);

        c_jmp (offset => g_pnear_labels^[r.label_nr] - (r.fill_position + 4));
      }
      else if (r.typ == ARM_COMPARE_AND_BRANCH)
      {
        // +4 because our instruction was moved 4 bytes further down by the extension
        byte[4] opcode = blob_ptr(g_blob_code)[r.fill_position+4:4];

        c_bz_reg (source => X0,   // register being tested
                  size   => 4,    // 4 or 8 bytes
                  offset => 8);
        
        // restore size(4/8) and inverse opcode(CBZ/CNBZ)
        blob_ptr(g_blob_code)[r.fill_position+3] = (byte)(opcode[3] ^ 1);
        
        // set register
        blob_ptr(g_blob_code)[r.fill_position] |= (byte)(opcode[0] & 31);

        c_jmp (offset => g_pnear_labels^[r.label_nr] - (r.fill_position + 4));
      }
      else
        abort;
    }
    else   // not expanded
    {
      if (r.typ == ARM_BRANCH)
      {
        c_jmp (offset => g_pnear_labels^[r.label_nr] - r.fill_position);
      }
      else if (r.typ == ARM_CONDITIONAL_BRANCH)
      {
        byte mask = (byte)(blob_ptr(g_blob_code)[r.fill_position] & 15);  // save 4 bits

        c_cond_branch (cmp    => CMP_EQUAL,
                       signed => false,
                       offset => g_pnear_labels^[r.label_nr] - r.fill_position);

        blob_ptr(g_blob_code)[r.fill_position] |= mask;  // restore 4 bits
      }
      else if (r.typ == ARM_COMPARE_AND_BRANCH)
      {
        byte[4] opcode = blob_ptr(g_blob_code)[r.fill_position:4];

        c_bz_reg (source => X0,   // register being tested
                  size   => 4,    // 4 or 8 bytes
                  offset => g_pnear_labels^[r.label_nr] - r.fill_position);
        
        // restore size(4/8) and opcode(CBZ/CNBZ)
        blob_ptr(g_blob_code)[r.fill_position+3] = opcode[3];
        
        // set register
        blob_ptr(g_blob_code)[r.fill_position] |= (byte)(opcode[0] & 31);
      }
      else
        abort;
    }
  }
#end unsafe
}

//------------------------------------------------------------------------

// to be called after generating asm for each function

void free_near_list ()
{
  RELOC_NEAR_INFO^ n, prev;

  n = g_reloc_near_list;

  while (n != null)
  {
    prev = n;
    n = n^.next;
    free prev;
  }

  g_reloc_near_list = null;
}

//---------------------------------------------------------------------------------------------------
/************************************************************************/

package JUMP_TABLE

  /* a list of jump table constants to fix (convert each label_nr -> code_address) */

  struct RELOC_JUMPTABLE_INFO
  {
    int8                  pool_nr;
    RELOC_JUMPTABLE_INFO^ next;
  }

  RELOC_JUMPTABLE_INFO^ g_reloc_jumptable_list;

end JUMP_TABLE;

/************************************************************************/

// mark jump table
// 1) at end of function, convert labels into code blob relative vectors
// 2) at end of program, add memory address of code blog to all vectors
// 3) add entry in elf's reloca table

public
void register_jump_table (int8 pool_nr)
{
  g_reloc_jumptable_list = new RELOC_JUMPTABLE_INFO '
                               {pool_nr  => pool_nr,
                                next     => g_reloc_jumptable_list};
}

//----------------------------------------------------

// called at end of function
// convert labels into code blob relative vectors

void reloc_jumptable_data ()
{
  RELOC_JUMPTABLE_INFO^ n;

  n = g_reloc_jumptable_list;

  while (n != null)
  {
    pool.relocate_jumptable_pool_constant (n^.pool_nr, g_pnear_labels^);
    n = n^.next;
  }
}

//----------------------------------------------------

void free_jumptable ()
{
 RELOC_JUMPTABLE_INFO^ n, prev;

  n = g_reloc_jumptable_list;

  while (n != null)
  {
    prev = n;
    n = n^.next;
    free prev;
  }

  g_reloc_jumptable_list = null;
}

//----------------------------------------------------

// to be called after generating asm for each function

public void near_labels_relocate_all ()
{
  expand_branch_offsets ();
  backfill_all_near_branch_offsets ();
  free_near_list ();

  reloc_jumptable_data ();
  free_jumptable ();

  blob_set_index (ref g_blob_code, blob_size (g_blob_code));
}

//---------------------------------------------------------------------------------------------------

package ELF_RELOC
  int        g_elf_reloc_count;
  ELF_REL[]^ g_elf_rel;
end ELF_RELOC;

//---------------------------------------------------------------------------------------------------

// called when flushing pool constants in g_blob_data.
// position of relative 64-byte address in g_blob_data,
// needs to be relocated statically (add memory address of g_blob_code or g_blob_data)
// and dynamically (add relocation record in elf file)
// they are sorted by fill position, so loader can process them fast

public
void store_code_data_relocation (int  fill_position,  // in g_blob_data
                                 bool code_blob)      // true = code blob, false = data blob
{
  if (g_elf_reloc_count == g_elf_rel^'length)   // table is full, double its length
  {
    ELF_REL[]^ old = g_elf_rel;
    g_elf_rel = new ELF_REL [g_elf_reloc_count << 1];
    g_elf_rel^[0:g_elf_reloc_count] = old^;
    free old;
  }

  // count/store relocations for later relocation table
  g_elf_rel^[g_elf_reloc_count++] = {fill_position, code_blob};
}

//----------------------------------------------------

public void get_elf_relocation_table (out ELF_REL[]^ table, out int table_count)
{
  table = g_elf_rel;
  table_count = g_elf_reloc_count;
}

//----------------------------------------------------

public void get_elf_relocation_table_count (out int table_count)
{
  table_count = g_elf_reloc_count;
}

//----------------------------------------------------

// to be called at the end of compilation

public void fix_function_calls ()
{
  fill_all_func_backfills ();
  blob_set_index (ref g_blob_code, blob_size (g_blob_code));
}

//---------------------------------------------------------------------------------------------------

public void init_fixup_structures ()
{
  Exports  .create (out g_exports,   compare_exports);
  Functions.create (out g_functions, compare_functions);
  g_leave_table = new int[1];

  shared_objs   .create (out g_shared_objs,    compare_shared_objs);
  import_funcs  .create (out g_import_funcs,   compare_import_funcs);
  
  g_elf_rel = new ELF_REL[16];
}

//---------------------------------------------------------------------------------------------------
