
// fixup.h

//----------------------------------------------------

void init_fixup_structures ();

//----------------------------------------------------

// EXPORT

// declare entry point name at function entry declaration
// entry point
// return true if inserted, false if duplicate name
bool export_register (wstring name);

// set entry point IP once code for the function was generated
void export_set_code_ip (wstring name, uint code_ip);

typedef void OPERATE_EXPORT_FUNCTION1 (wstring name);
// in elf, generating export tables
void loop1_on_export_functions (OPERATE_EXPORT_FUNCTION1 op);

// in elf, set offset where to paste ip
void export_set_fix_offset_in_file (wstring name, uint fixup_offset_in_file);

// in elf, fix export table
typedef void OPERATE_EXPORT_FUNCTION2 (uint fixup_offset_in_file, uint code_ip);
void loop2_on_export_functions (OPERATE_EXPORT_FUNCTION2 op);

//----------------------------------------------------

// IMPORT

// imported shared objects and functions

// returns slot_nr
int register_imported_shared_object_and_func (string so, string func);

typedef void TREAT_SO_NAME1 (string so_name, out int str_table_index);
void import_loop_so_names1 (TREAT_SO_NAME1 treat);

typedef void TREAT_SO_NAME2 (int str_table_index);
void import_loop_so_names2 (TREAT_SO_NAME2 treat);

typedef void TREAT_IMPORT_FUNC1 (string func_name, int seqnr, int slot_nr);
void import_loop_import_funcs1 (TREAT_IMPORT_FUNC1 treat);

int nb_of_imported_funcs ();

//----------------------------------------------------

// FUNCTION CALLS

// called when generating code for a function, to declare it and fix its IP
void function_register (int label_nr,
                        uint code_ip);   // ip of function in code segment

// returns function's IP (to send it into export table)
uint function_get_ip (int label_nr);

// for a call
void func_store_backfill (int func_label_nr, int fill_position);

//----------------------------------------------------

enum RELOC_KIND {
  RELOC_NONE,    // stack or absolute
  RELOC_FUNC,    // code segment
  RELOC_POOL,    // data segment (constants)
  RELOC_GLOBAL,  // bss segment (global variable)
  RELOC_DLL,     // import table
  RELOC_SYSCALL, // pseudo syscall address
};

enum FILL_TYP 
{
  ADRP_PAGE_4K,            // 21 bit (4K page)
  ADD_OFFSET_4095,         // 12 bit
  LOAD_STORE_OFFSET_4095,  // 12-bit offset must be >> by data_size_shifts before being stored in instruction
                           // (extra_offset is always aligned at data_size)
};                                       

void register_reloc (RELOC_KIND kind,             // RELOC_FUNC, RELOC_POOL, RELOC_GLOBAL or RELOC_DLL
                     int8       nr,               // function nr, constant nr, or global address in bss
                     FILL_TYP   typ,              // ADRP_PAGE_4K, ADD_OFFSET_4095 or LOAD_STORE_OFFSET_4095
                     int        extra_offset,     // to be added to code/pool/global address
                     int        data_size_shifts, // used for LOAD_STORE_OFFSET_4095 only
                     int        fill_position);   // position in code blob

//----------------------------------------------------

// called when flushing pool constants in g_blob_data.
// position of relative 64-byte address in g_blob_data, 
// needs to be relocated statically (add memory address of g_blob_code or g_blob_data)
// and dynamically (add relocation record in elf file)
void store_code_data_relocation (int  fill_position, 
                                 bool code_blob);   // true = code blob, false = data blob

//----------------------------------------------------

struct ELF_REL
{
  int  fill_position;
  bool code_blob;    // true = code blob, false = data blob
}

void get_elf_relocation_table (out ELF_REL[]^ table, out int table_count);
void get_elf_relocation_table_count (out int table_count);

//----------------------------------------------------

// $ - move_dbg_lines ((int)backfill_addr, (int)offset_size_increase);

//----------------------------------------------------

// PCODE LEAVE

void leave_clear_all ();   // called at the start of each new function
void leave_register (int backfill_pos);   // save point in blob code where to add code

typedef void LEAVE_INSERT_CODE (int backfill_pos);
void leave_loop_on_all (LEAVE_INSERT_CODE func);   // loop on all, in reverse order (from last to first)

//----------------------------------------------------

// NEAR LABELS

// allocate table for all labels (we know in advance how many labels we need)
// to be called at start of each function.
void near_label_allocate_table (int nb_labels);

void near_label_declare (int label_nr, int code_position);

enum BRANCH_TYP {ARM_BRANCH,               // +/- 128 MB
                 ARM_CONDITIONAL_BRANCH,   // +/- 1 MB
                 ARM_COMPARE_AND_BRANCH};  // +/- 1 MB

void near_label_branch (int label_nr, BRANCH_TYP typ, int code_position);

// to be called after generating asm for each function
void near_labels_relocate_all ();

//----------------------------------------------------

// mark jump table
// 1) at end of function, convert labels into code blob relative vectors
// 2) at end of program, add memory address of code blog to all vectors
// 3) add entry in elf's reloca table
void register_jump_table (int8 pool_nr);

//----------------------------------------------------

// insert some bytes in code, 
// displaces function calls, dll, pool constants, globals, near labels and debug info.
// used for near labels, enter and leave pcodes
void insert_bytes_in_code (int pos, int size_increase, bool extend_instruction);

//----------------------------------------------------

// to be called at the end of compilation
void fix_function_calls ();

void backfill_globals_pools_references_in_code (uint start_of_code_mem,
                                                uint start_of_data_blob_mem,
                                                uint start_of_bss_mem,
                                                uint got_plt_base_address);

//----------------------------------------------------
