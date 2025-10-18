
// pe.h

/*********************************************************************************/

void pe_init_image ();

/*********************************************************************************/

// code segment

const uint LOAD_ADDRESS =  0x00400000;
const uint IP_START     =  0x00401000;   // load address + HEADER

uint4 current_RIP ();

/*********************************************************************************/

// called when generating code for new function
void exe_reset_dll_move_chain ();

// called just after writing an indirect DLL address that needs relocation
void exe_dll_reference_written (string dll, string func, bool absolute);

/*********************************************************************************/

void exe_add_func_label (int func_label_nr);

/*****
    call func_label;  (c_call_relative() -> 0xE8 + i4)
                       store -(IP+5),  relocation adds function's address to value in zone;
    load func_label;  (for indirect calls) (c_mov_reg_imm (REG,imm,4) with imm>0 : n+4 bytes)
                       store 0; relocation adds function's address to value in zone;
*****/

void exe_func_backfill_addr4_written (int func_label_nr, bool absolute);

/*********************************************************************************/

void exe_global_backfill_addr4_written (int relative_global_addr, bool absolute);

/*********************************************************************************/

// to be called for each jump table
void exe_mark_jump_table (int8 pool_nr);

/*********************************************************************************/

// to be called after p-code for a function was generated,
// before generating asm.
void exe_allocate_near_label_table (int nb_labels);

void exe_declare_near_label (int label_nr);

// we just wrote a 1-byte branch
void exe_near_branch_written (int label_nr, bool conditional);

// to be called after generating asm for each function
void relocate_all_near_labels ();

/*********************************************************************************/

void exe_insert_code_sequence (uint4 backfill_addr, uint4 offset_size_increase);
void exe_update_code_sequence (uint4 backfill_addr, byte[] seq);

void exe_finish_code ();

uint4 exe_store_pool_constant (byte[] cte, uint4 align);

void exe_finish_data (string res_filename, int8 global_offset);

/*********************************************************************************/

// finish exe

void exe_terminate_image (uint4 bss_size,    // must be multiple of PAGE !
                          uint4 stack_size,  // must be multiple of PAGE !
                          bool  console_app);

/*********************************************************************************/

void parse_resource_file (string current_dir, string res_filename);

/*********************************************************************************/

void print_crc ();

/*********************************************************************************/
