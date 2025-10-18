
// init86.h

//======================================================================================

void generate_bootstrap_86_code (int  func_label_to_main,
                                 int  func_label_to_init_constants,
                                 bool function_main_has_parameter_array_of_string);

//======================================================================================

void extra_86_code ();

// these help routines are called by as86.c
// the first 1023 label numbers are reserved for bootstrap code
const int label_multiply_int8             = 103;
const int label_divide_int8               = 104;
const int label_modulo_int8               = 105;

bool used_multiply_int8, used_divide_int8, used_modulo_int8;


const int label_getlock_tombstone         = 106;
const int label_allocate_tombstone        = 107;
const int label_free_tombstone            = 108;
const int label_free_possible_null        = 109;
const int label_check_stack_M16_alignment = 110;

//======================================================================================
