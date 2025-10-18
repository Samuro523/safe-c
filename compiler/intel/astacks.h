
// astacks.h

/***********************************************************************************/

use a86;

/***********************************************************************************/

enum NODE_KIND
  { INT_CONSTANT, FLOAT_CONSTANT,
    INT_REGISTER, FLOAT_REGISTER,
    EFFECTIVE_ADDRESS, MEMORY };

struct NODE
{
  char      typ;   // b=bool, i=int, l=long, f=float, d=double, a=address
  NODE_KIND kind;

  // INT_CONSTANT
  int8   icte;

  // FLOAT_CONSTANT
  double fcte;

  // INT_REGISTER
  REG    reg;
  REG    reg_high;  // high dword for istack when using type long in 32-bit

  // FLOAT_REGISTER
  int    freg;   // 0 to 7

  // EFFECTIVE_ADDRESS / MEMORY (can be temporary)
  EA     ea;    // ('effective address' denotes the address, 'memory' denotes the location of the operand)
                // base==ESP indicates a temporary variable.
}

/***********************************************************************************/

const int MAX_NODES = 1000;

NODE istack[MAX_NODES];    // integer (INT_CONSTANT, INT_REGISTER, MEMORY) // b=bool, i=int, l=long
int  istack_count;

NODE fstack[MAX_NODES];    // float (FLOAT_CONSTANT, FLOAT_REGISTER, MEMORY) // f=float, d=double
int  fstack_count;

NODE astack[MAX_NODES];    // address (EFFECTIVE_ADDRESS, MEMORY) // a=address
int  astack_count;

/***********************************************************************************/

void swap_nodes (ref NODE n1, ref NODE n2);

/***********************************************************************************/


/***********************************************************************************/

// clear the single temporary zone that can be alloced in pcode

void clear_pcode_temporary_zone ();

/***********************************************************************************/

// allocate a single temporary zone for this pcode.
// this zone is on the stack just after ESP.
// this zone is valid until the pcode terminates.

int4 allocate_temporary_zone (int size); // 1, 4, 8, ..

/***********************************************************************************/



/***********************************************************************************/

// save all registers to temporaries except the number of node entries given as parameters.
// there are only mov's and lea's so this doesn't change any flags.
// this allocates temporary variables relative to RSP.

void store_all_registers_in_temporaries_except_some_nodes (int dont_touch_i, int dont_touch_f, int dont_touch_a);

/***********************************************************************************/

void store_all_registers_in_temporaries ();

/***********************************************************************************/

// should be called before calling a function

void store_all_registers_in_temporaries_except_for_this_pcode ();

/***********************************************************************************/

// should be called before calling a Windows API

void store_all_registers_used_by_kernel_call_in_temporaries_except_for_this_pcode ();

/***********************************************************************************/
  
// save all floating-point registers to temporaries
// this allocates temporary variables relative to RSP.

void store_all_float_registers_in_temporaries ();

/***********************************************************************************/



/***********************************************************************************/

// for istack or astack.
// there are only mov's and lea's so this doesn't change any flags.
// this allocates temporary variables.
// returns true if at least one register was freed.

bool store_node_in_temp (ref NODE n);

/***********************************************************************************/

// count how many times a register is used.
// note that a register can be used several times, if a node was cloned.
int register_usage_count (REG r);

// this allocates temporary zones so we must not previously allocate and use temporary zones in this pcode as they will be erased !!
REG allocate_register (int  size,         // 1, 4 or 8 (32-bit: size==1 does not return RSI, RDI)
                       bool with_crash_node,
                       NODE crash_node); // allow reuse of registers of this node as it will be erased

// try allocate registers except those used in operands of current pcode.
// free some registers of other nodes if needed.
// this allocates temporary zones so we must not previously allocate and use temporary zones in this pcode as they will be erased !!
// returns 0 if success, -1 if registers couldn't be allocated
int try_allocate_registers (    int   size,        // 1, 4 or 8 (32-bit: size==1 does not return RSI, RDI)
                                bool  with_crash_node,
                                NODE  crash_node, // allow reuse of registers of this node as it will be erased
                                int   nb_registers,
                            out REG[] reg);

// allocate registers except those used in operands of current pcode.
// free some registers of other nodes if needed.
// should never fail with 6 registers (2 do not support size == 1)
void allocate_registers (    int   size,        // 1, 4 or 8 (32-bit: size==1 does not return RSI, RDI)
                             bool  with_crash_node,
                             NODE  crash_node, // allow reuse of registers of this node as it will be erased
                             int   nb_registers,
                         out REG[] reg);


void free_register (REG r);
void free_register_except_for_this_pcode (REG r);

// this might cause a storing of all other float registers in temporaries !
int allocate_float_register ();

int nb_float_registers_left ();


// for modif means no other node uses this register

void flush_int1_in_register            (ref NODE n);
void flush_int1_in_register_for_modif  (ref NODE n);
void flush_int4_in_register            (ref NODE n);
void flush_int4_in_register_for_modif  (ref NODE n);
void flush_int8_in_registers           (ref NODE n);
void flush_int8_in_registers_for_modif (ref NODE n);

// convert 'a' operand from memory into effective address
void flush_effective_address (ref NODE a);

// node f must always be the top-most node
// this might cause a storing of all other float registers in temporaries !
void flush_float_in_register (ref NODE f);



void push_int4 (NODE i);
void push_int8 (NODE i);
void push_addr (NODE a);
void push_float (NODE f);

void load_int4_into_reg           (ref NODE i, REG r);
void load_int8_into_reg_for_64bit (ref NODE i, REG r);
void load_addr_into_reg           (ref NODE a, REG r);


// (this is used both for function return values and operands of operator ?:)
void sync_bool ();      // in AL
void sync_int4 ();      // in EAX
void sync_int8 ();      // in EDX:EAX for 32-bit, or RAX for 64-bit
void sync_addr ();      // in RAX
void sync_addr_int4 (); // in RAX=int4, RSI=addr

/***********************************************************************************/

// if register in index with scale 1, and base is free, move it to base.
void normalize_ea (ref NODE a);

bool ea_is_just_ea_with_register_base (NODE a);

bool ea_is_simple_constant_offset (NODE a);

void load_ea_into_just_register_base (ref NODE a);

/***********************************************************************************/
