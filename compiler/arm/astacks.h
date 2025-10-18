
// astacks.h

use arm64, ../fixup;

/***********************************************************************************/

/*
enum RELOC_KIND {          // declared in fixup.h
  RELOC_NONE,    // stack or absolute
  RELOC_FUNC,    // code segment
  RELOC_POOL,    // data segment (constants)
  RELOC_GLOBAL,  // bss segment (global variable)
  RELOC_DLL,     // import table
  RELOC_SYSCALL, // pseudo syscall address
};
*/

struct RELOC_INFO
{
  RELOC_KIND  kind;
  int8        nr;   // func-label, dll-nr, syscall-nr, pool-id or global-addr.
}

struct EA  // effective address
{
  REG        base;     // X0 to X14, FP, SP, ZERO (UNSIGNED 8 bytes)
                       // (FP for parameter, local variable or temporary, ZERO for absolute pointer, SP for parameter)
  REG        index;    // X0 to X14, or ZERO  (SIGNED 4 bytes)
  int        scale;    // >= 1 (> 1 allowed only if index != ZERO)
  int        offset;   // limited to 32-bit (SIGNED 4 bytes) (2GB range)
  RELOC_INFO reloc;
}

/***********************************************************************************/

enum NODE_KIND  { INT_CONSTANT, FLOAT_CONSTANT, 
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
  REG    reg;   // X0 to X14

  // FLOAT_REGISTER
  FREG   freg;  // F0 to F7

  // EFFECTIVE_ADDRESS / MEMORY (can be temporary)
  EA     ea;    // ('effective address' denotes the address, 'memory' denotes the location of the operand)
}

/***********************************************************************************/

const int MAX_NODES = 1000;

NODE istack[MAX_NODES];    // integer (INT_CONSTANT, INT_REGISTER, MEMORY) // b=bool, i=int, l=long
int  istack_count;

NODE fstack[MAX_NODES];    // float (FLOAT_CONSTANT, FLOAT_REGISTER, MEMORY) // f=float, d=double
int  fstack_count;

NODE astack[MAX_NODES];    // address (EFFECTIVE_ADDRESS, MEMORY) // a=address
int  astack_count;

const NODE NULL_CRASH_NODE = {'i', INT_CONSTANT, 0, 0.0, ZERO, F31, {ZERO, ZERO, 1, 0, {RELOC_NONE, 0}}};

/***********************************************************************************/

enum PARAMETER_STORAGE_LOCATION_TYPE (byte) {IN_X_REGISTER, IN_F_REGISTER, ON_STACK};

struct PARAMETER_STORAGE_LOCATION
{
  PARAMETER_STORAGE_LOCATION_TYPE type;
  int                             nr;    // register number or stack offset
  bool                            is_signed;
}

// parameter storage location
PARAMETER_STORAGE_LOCATION istack_extra[MAX_NODES];
PARAMETER_STORAGE_LOCATION fstack_extra[MAX_NODES];
PARAMETER_STORAGE_LOCATION astack_extra[MAX_NODES];

/***********************************************************************************/

void move_register_immediate (REG  target,
                              long imm,
                              int  size);   // 4 or 8

/***********************************************************************************/

// uses X15
void move_fregister_immediate (FREG   target,
                               float8 imm,
                               int    size);  // 4 or 8

/***********************************************************************************/

// uses X15-X17
void move_memory_immediate (EA     target,
                            int8   imm,
                            int    size);   // 1, 2, 4 or 8

/***********************************************************************************/

// uses X15-X17
void move_memory_fimmediate (EA     target,
                             float8 value,
                             int    size);   // 4 or 8

/***********************************************************************************/

// source cannot be X17
void add_offset_using_x17 (REG  target,    // ZERO NOT ALLOWED !  SP NOT ALLOWED !
                           REG  source,    // ZERO NOT ALLOWED !  X17 NOT ALLOWED !
                           int8 offset,
                           int  size);     // 4 or 8

/***********************************************************************************/

bool is_null_ea (EA ea);

/***********************************************************************************/

// >>>>>>>>>>>>> X16 & X17 used as very temporary registers <<<<<<<<<<<<<<<
// X16 for index
// X17 for large offset

void c_store_register_in_memory   (REG  r, EA ea,                   int size); // size = 1, 2, 4, 8
void c_load_register_from_memory  (REG  r, EA ea, bool data_signed, int size); // size = 1, 2, 4, 8
void c_store_fregister_in_memory  (FREG r, EA ea,                   int size); // size = 4, 8
void c_load_fregister_from_memory (FREG r, EA ea,                   int size); // size = 4, 8

void compute_effective_address_in_register (EA ea, REG r);

/************************************************************************/

void swap_nodes (ref NODE n1, ref NODE n2);
int lshifts_of (int8 value);
int size_of_operand (NODE n);  // 1, 4, or 8

/***********************************************************************************/

// save all registers (Xn and Fn) to temporaries except the number of node entries given as parameters.
void store_all_registers_in_temporaries_except_some_nodes (int dont_touch_i, int dont_touch_f, int dont_touch_a);

/***********************************************************************************/

// save all registers (Xn and Fn) to temporaries 
// should be called before calling a function
void store_all_registers_in_temporaries_except_for_this_pcode ();

/***********************************************************************************/

// save all registers (Xn and Fn) to temporaries
void store_all_registers_in_temporaries ();

/***********************************************************************************/

// count how many times a register is used.
// note that a register can be used several times, if a node was cloned.
int register_usage_count (REG r);
int fregister_usage_count (FREG r);

// allocate registers except those used in operands of current pcode.
// free some registers of other nodes if needed.
void allocate_registers (out REG  reg[],
                             NODE crash_node = NULL_CRASH_NODE); // allow reuse of registers of this node as it will be erased
void allocate_fregisters (out FREG reg[],
                              NODE crash_node = NULL_CRASH_NODE); // allow reuse of registers of this node as it will be erased
                              
void set_hint_x (REG r);
void set_hint_f (FREG r);
void clear_hints ();
                              
// allocate 1 register except those used in operands of current pcode.
REG allocate_register (NODE crash_node = NULL_CRASH_NODE);
FREG allocate_fregister (NODE crash_node = NULL_CRASH_NODE);

/***********************************************************************************/

void flush_effective_address (ref NODE a);  // convert 'a' operand from memory into effective address

/***********************************************************************************/

void free_register_except_for_this_pcode (REG r);
void free_fregister_except_for_this_pcode (FREG r);

/***********************************************************************************/

// for modif means no other node uses this register

void flush_uint1_in_register           (ref NODE n);
void flush_uint1_in_register_for_modif (ref NODE n);
void flush_int4_in_register            (ref NODE n);
void flush_int4_in_register_for_modif  (ref NODE n);
void flush_int8_in_register            (ref NODE n);
void flush_int8_in_register_for_modif  (ref NODE n);
void flush_float4_in_register            (ref NODE n);
void flush_float4_in_register_for_modif  (ref NODE n);
void flush_float8_in_register            (ref NODE n);
void flush_float8_in_register_for_modif  (ref NODE n);


void load_bool_into_reg   (ref NODE i, REG r);
void load_int4_into_reg   (ref NODE i, REG r);   // signed-extend to 8 bytes of register
void load_uint4_into_reg  (ref NODE i, REG r);   // zero-extend to 8 bytes of register
void load_int8_into_reg   (ref NODE i, REG r);
void load_float4_into_reg (ref NODE f, FREG r);
void load_float8_into_reg (ref NODE f, FREG r);
void load_addr_into_reg   (ref NODE a, REG r);

/***********************************************************************************/

// check all previous nodes : if any node uses this register or stack location, copy it to temporaries !
void free_param_slot_for_xstack (PARAMETER_STORAGE_LOCATION loc, NODE node);

/***********************************************************************************/

void call_libc (string function_name);

/***********************************************************************************/
