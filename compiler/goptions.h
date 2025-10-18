
// goptions.h : global compiler options

/**********************************************************************************/

void load_global_options (string ini_filename);

/**********************************************************************************/

enum TARGET {UNDEFINED_TARGET, INTEL, ANDROID};
TARGET g_target;

int address_size;      // 4 or 8 bytes
bool pointer_checks_enabled = true;
bool array_checks_enabled = true;
bool assertion_checks_enabled = true;

bool g_tracing;
bool big_endian = false;   // false=Intel, true=Motorola(not supported)
bool option_check_stack_M16_alignment = false;  // true for test, false for prod

/**********************************************************************************/
