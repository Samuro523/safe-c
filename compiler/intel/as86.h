
// as86.h

use ../pcodes;

//======================================================================================

void generate_asm_for_function (bool is_main, int nb_labels);

//======================================================================================

int8 pool_nr_87_control_word_trunc;

void align_stack ();   // to be called before pushing or setting parameters for Windows call
void alloc_shadow_space ();

void call_kernel (string func, int nb_arguments);

void free_shadow_space ();
void dealign_stack ();  // to be called after freeing shadow space
void free_shadow_space_and_dealign_stack ();   // does both above together


uint4 g_frame_size;
uint4 g_extra_bytes;
uint4 g_stack_alignment;

PCODE g_current_pcode;

bool g_alignment_needed;

int g_shadow_space;

//======================================================================================
