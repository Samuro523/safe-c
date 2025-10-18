
// asm_arm.h

use ../pcodes, ../common;
use arm64;

void generate_asm_for_function (bool is_main, int nb_labels);


uint4 g_frame_size;           // size of frame to allocate
uint4 g_saved_on_stack_size;  // total size of parameters saved on stack
uint4 g_first_page_size;      // includes 16 for (old_fp+ret_addr)
uint4 g_extra_frame_size;     // to subtract from SP
uint4 g_min_extra_frame_size; // all locals below this are temporaries
uint4 g_max_extra_param_call_size;  // max size of extra parameters stored on stack, M16

/* so how does this work ?

   see allocate_local_chunk()

   first, move up frame_offset at M16

   if saved_on_stack_size + frame_offset <= 4096 - 16
     we need to allocate only 1 page of size (saved_on_stack_size + frame_offset + 16), that includes 16 for (old_fp+ret_addr)

   if saved_on_stack_size + frame_offset > 4096 - 16 && saved_on_stack_size + frame_offset < 4096
     that's not possible

   if saved_on_stack_size + frame_offset >= 4096
     allocate a first_page = (4096 - frame.saved_on_stack_size), that includes 16 for (old_fp+ret_addr)
     allocate extra space = frame_offset - first_page
*/


uint4 g_stack_alignment;      // always 16

PCODE g_current_pcode;

void declare_near_label (int near_label);
void cond_branch (COMPARISON_FLAG cmp, bool signed, int near_label);
void branch_if_not_zero (REG reg, int size, int near_label);
void branch_if_zero (REG reg, int size, int near_label);
void branch (int near_label);

void declare_function (uint function_nr);
void call_function (uint function_nr);
