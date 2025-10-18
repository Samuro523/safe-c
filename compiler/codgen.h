
// codgen.h : code generator

use front/entities, common;

//======================================================================================

// for INTEL and ANDROID:
//   0: ADDR : heap ID (windows only)
//   8: int4 : lockf (0=unlocked, 1=lock) ; protects head & tail of tombstone structure
//  16: ADDR : head of tombstone structure
//  24: ADDR : tail of tombstone structure
// for ANDROID only:
//  32: ADDR : dummy thread id (garbage)
//  40: pthread_attr_t;   // 60 bytes
// 100:
const int INTEL_BEGIN_GLOBAL_OFFSET = 32;
const int ANDROID_BEGIN_GLOBAL_OFFSET = 100;
int8 g_global_offset;

//======================================================================================

PENTITY    g_func_main;
bool       g_function_main_has_parameter_array_of_string;

PENTITY[]^ g_entry_function_list;

//======================================================================================

typedef void GENERATE_ASM_FOR_PCODES (bool is_main, 
                                      int  nb_labels);

void generate_code                          (PENTITY func,
                                             GENERATE_ASM_FOR_PCODES generate_asm_for_pcodes);

void generate_code_to_init_global_variables (int                     func_label_to_init_constants,
                                             GENERATE_ASM_FOR_PCODES generate_asm_for_pcodes,
                                             int                     stack_size);   // for ANDROID

//======================================================================================

void add_function_to_pending_functions (PENTITY e);

//======================================================================================

void add_global_variable_to_list (PENTITY e);

//======================================================================================

// frame info collects data for a whole function, from enter to leave, declarations and statements

struct FRAME_INFO
{
  
  int8 currently_pushed_offset;   // bytes currently pushed on stack (must be zero when function ends)
                                  // for intel 64-bit stack alignment on 16-byte.
                                  
  int8 param_offset;  // size of all parameters reserved on stack for this function + 2 * address_size
                      // used by parameters, return address, frame pointer (>=0)
                      
  int8 frame_offset;  // local variables address relative to EBP (<=0)
  
  int8 minimum_frame_offset;  // smallest frame_offset ever reached (for local variables & temporaries)
  

  int  saved_on_stack_size;  // total size of parameters saved on stack (for ANDROID) (>=0)
}

int4 allocate_temp_variable (int4 size, ref FRAME_INFO frame);
int8 allocate_global_variable (int4 size);

//======================================================================================

int get_new_near_label_nr ();
int get_new_func_label_nr ();

//======================================================================================

// returns true if code after region is unreachable

bool generate_code_for_region (    wstring    func_id,
                                   PENTITY    e_parent,
                                   PENTITY    e_list,
                               ref FRAME_INFO frame,
                                   int        outer_label_break,
                                   int        outer_label_continue,
                               ref bool       any_jumps_to_label_break,
                               ref bool       any_jumps_to_label_continue);

//======================================================================================

// for k_typ below

const int K_FOR      =     0x01;
const int K_WHILE    =     0x02;
const int K_SWITCH   =     0x04;

//======================================================================================

// returns true if code was generated, false otherwise

bool release_objects_in_outer_scope (int     k_typ,
                                     PENTITY e_parent,
                                     bool    b_generate_code);

//======================================================================================

void generate_p_location (LOCATION l);

//======================================================================================

void generate_leave_ret (FRAME_INFO frame);

//======================================================================================
