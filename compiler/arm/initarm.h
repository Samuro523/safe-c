
// initarm.h

//======================================================================================

void extra_arm_code ();

// these help routines are called by am_arm.c
// the first 1023 function numbers are reserved for bootstrap code

const uint func_getlock_tombstone         = 106;
const uint func_allocate_tombstone        = 107;
const uint func_free_tombstone            = 108;
const uint func_usleep                    = 109;

//======================================================================================
