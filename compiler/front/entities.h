
// entities.h : central structure tree.

use ../pool, ../common;
use tokens;

typedef ENTITY;
typedef ENTITY^ PENTITY;

typedef REGION;
typedef REGION^ PREGION;

typedef EXPRESSION;
typedef EXPRESSION^ PEXPRESSION;



// mode

enum MODE {MODE_IN, MODE_OUT, MODE_REF};


// constraint

enum KIND_CONSTRAINT
{
  DOES_NOT_APPLY,        // (must be first in enum list) neither open-array nor open-struct type
  UNCONSTRAINED,         // open-array or open-struct without constraint
  CONSTANT_CONSTRAINT,   // constant constraint (value is given)
  RUNTIME_CONSTRAINT,    // runtime constraint
};

struct CONSTRAINT
{
  KIND_CONSTRAINT  kind;
  uint4            value;  // if kind == CONSTANT_CONSTRAINT : length or discriminant constraint
}


// context - used for array, struct and pointer types as context of aggregates or allocators.

struct CONTEXT
{
  PENTITY     base_type_or_null;   // null if no context available
  CONSTRAINT  constraint;
  bool        address_of;          // true if we want to take the expression's address
}



// expression

enum KIND_OPERATOR
{

  // 3 params
  OP_CONDITIONAL_TEST,  // "? :"
                        // arg 1 = bool
                        // arg 2 & 3 = pair of
                        //   { integer,
                        //     float,
                        //     enum,
                        //     pointer (or null_pointer),
                        //     function pointer(or null_pointer),
                        //     unsafe pointer(or null_pointer),
                        //     non-open struct,
                        //     union,
                        //     open array (see constant/runtime constraint),
                        //     open struct (see constant/runtime constraint),
                        //     opaque,
                        //     generic }
                        // result = same as arg2

  // 2 params
  OP_SHORT_CIRCUIT_AND, // bool && bool -> bool
  OP_SHORT_CIRCUIT_OR,  // bool || bool -> bool

  OP_AND,               // bool & bool -> bool
  OP_OR,                // bool | bool -> bool
  OP_XOR,               // bool ^ bool -> bool

  OP_BITAND,            // integer & integer -> integer
  OP_BITOR,             // integer | integer -> integer
  OP_BITXOR,            // integer ^ integer -> integer

  // (uses mask 'cmp'), result is bool.
  OP_COMPARE_SIGNED,     // args = pair of signed integer
  OP_COMPARE_UNSIGNED,   // args = pair of enum or unsigned integer
  OP_COMPARE_FLOAT,      // args = pair of floating-point
  OP_COMPARE_PTR,        // args = pair of pointer, function pointer, unsafe pointer, or null pointer

  OP_SHIFT_LEFT_SIGNED,    // signed integer   << signed integer   -> signed integer
  OP_SHIFT_LEFT_UNSIGNED,  // unsigned integer << unsigned integer -> unsigned integer
  OP_SHIFT_RIGHT_SIGNED,   // signed integer   >> signed integer   -> signed integer
  OP_SHIFT_RIGHT_UNSIGNED, // unsigned integer >> unsigned integer -> unsigned integer

  OP_ADD_INT,              // integer + integer -> integer,  enum + integer -> enum
  OP_SUB_INT,              // integer - integer -> integer,  enum - integer -> enum

  OP_MUL_INT_SIGNED,       // signed integer * signed integer  ->  signed integer (promoted integer)
  OP_DIV_INT_SIGNED,       // same
  OP_MOD_INT_SIGNED,       // same

  OP_MUL_INT_UNSIGNED,     // unsigned integer * unsigned integer  ->  unsigned integer (promoted integer)
  OP_DIV_INT_UNSIGNED,     // same
  OP_MOD_INT_UNSIGNED,     // same

  OP_ADD_FLOAT,     // floating-point + floating_point -> floating_point
  OP_SUB_FLOAT,     // same
  OP_MUL_FLOAT,     // same
  OP_DIV_FLOAT,     // same

  OP_ADD_PTR_INT,   // unsafe_ptr + integer    -> unsafe_ptr  ; add integer*sizeof(elem) to unsafe_ptr
  OP_SUB_PTR_INT,   // unsafe_ptr - integer    -> unsafe_ptr  ; sub integer*sizeof(elem) from unsafe_ptr
  OP_SUB_PTR_PTR,   // unsafe_ptr - unsafe_ptr -> uint4       ; (ptr-ptr)/sizeof(elem) (type uint4)

  // 1 param
  OP_CONVERT_INT_INT,     // convert between various enumeration and integer types
  OP_CONVERT_PTR_PTR,     // convert between various unsafe pointers
  OP_CONVERT_FLOAT_INT,   // convert from floating-point to integer
  OP_CONVERT_INT_FLOAT,   // convert from integer to floating-point
  OP_CONVERT_FLOAT_FLOAT, // convert from floating-point to floating-point

  OP_UNARY_PLUS,   // integer -> integer,  or floating-point -> floating_point  (no effect other than promotion)
  OP_UNARY_MINUS,  // integer -> integer,  or floating-point -> floating_point

  OP_BOOL_NOT,     // bool -> bool
  OP_INT_NOT,      // integer -> integer

  OP_PRE_DEC,      // arg = object of type enum, int or unsafe-ptr, returns = value of that type
  OP_PRE_INC,

  OP_POST_DEC,     // arg = object of type enum, int or unsafe-ptr, returns = value of that type
  OP_POST_INC,

  OP_ADDRESS_OF,   // arg = an object, returns = unsafe pointer.
  OP_LENGTH,       // arg = an object of type array and runtime constraint, returns = type int.
  OP_SIZE,         // arg = an object of open type having runtime constraint, returns = type uint.

  OP_NONE,         // used internally only
};


enum KIND_EXPRESSION
{

  // constant value or constant object
  A_CONST_ENUMERATION_VALUE,  // ex: false, color'first.  Must be object for prefix of OP_ADDRESS_OF.
  A_CONST_INTEGER_VALUE,
  A_CONST_FLOAT_VALUE,
  A_CONST_NULL_VALUE,         // compatible with pointer, function pointer, unsafe pointer
  A_POOL_CONSTANT,            // array/struct string literal, constant aggregate,
                              // declared constant or index/slice/field of it.
  // runtime value
  AN_OPERATOR_VALUE,          // runtime value
  A_RUN_CALL,
  A_FUNCTION_VALUE,           // value of type function pointer
  A_FUNCTION_CALL,            // return type can be type_void
  A_DISCRIMINANT_VALUE,
  AN_UNC_ARRAY_AGGREGATE,     // array (context-dependant size)
  AN_AGGREGATE_VALUE,         // array or struct (constant size)
  A_QUALIFIED_EXPRESSION,
  AN_ARRAY_QUALIFIED_EXPRESSION,
  A_STRUCT_QUALIFIED_EXPRESSION,
  AN_ALLOCATOR,

  // runtime object (readonly or readwrite)
  A_GLOBAL_VARIABLE_OBJECT,
  A_LOCAL_VARIABLE_OBJECT,
  A_REFERENCE_OBJECT,
  A_PARAMETER_OBJECT,           // an expression denoting a parameter
  AN_ARRAY_ELEMENT_OBJECT,
  AN_ARRAY_SLICE_OBJECT,
  A_STRUCT_FIELD_OBJECT,
  A_DEREFERENCED_OBJECT,
  AN_UNSAFE_DEREFERENCED_OBJECT,
  AN_ATTR_BYTE_OBJECT,      // has either constant or runtime constraint

  // special conversion objects (can only appear as actual parameters, possibly inlined)
  A_BOXED_OBJECT,       // convert from any packed type into byte[]
  AN_UNBOXED_OBJECT,    // convert from byte[] into any packed type
  A_BOXED_ARRAY_OBJECT, // an array of boxed objects

};

/******************************************/

// used for code generation:
// the following fields are provided on input after pre-analysis
// to avoid that two different offsets are used by "?:" operands

enum USES_TOMBSTONE {_NO, _YES, _YES_BUT_CAN_BE_NULL};

struct TOMBSTONE
{
  USES_TOMBSTONE anchor_provided;
  int4           anchor_offset;     // offset to safe-ptr to tombstone ptr in local frame
}

/******************************************/

struct LIST_OF_EXPRESSIONS
{
  LIST_OF_EXPRESSIONS^ prev, next;
  PEXPRESSION          exp;
  PENTITY              type;  // for aggregates : target type, for parameters : actual base type before boxing
  PENTITY              e;     // for struct aggregate: field or varying_field
                              // for function call: (formal) parameter
  TOMBSTONE            tomb;  // for code generation; stores heap object anchor
                              //   of parameters and array objects.
}

struct CONST_ENUMERATION_VALUE_INFO
{
  uint4 value;
}

struct CONST_INTEGER_VALUE_INFO
{
  int8 value;
}

struct CONST_FLOAT_VALUE_INFO
{
  double value;
}

struct CONST_NULL_VALUE_INFO
{
  bool dummy;
}

struct POOL_CONSTANT_INFO
{
  POOL pool_cte;
}

struct OPERATOR_VALUE_INFO
{
  KIND_OPERATOR   op;
  COMPARISON_FLAG cmp;
  PEXPRESSION     arg[3];
  PENTITY         type;   // promoted type (for OP_COMPARE_SIGNED/_UNSIGNED/_FLOAT)
}

struct RUN_CALL_INFO
{
  PEXPRESSION function_call;
}

struct FUNCTION_VALUE_INFO
{
  PENTITY to_function_declaration_or_generic_function;   // generic function or function declaration entity
}

struct FUNCTION_CALL_INFO
{
  PEXPRESSION          func;   // a value of type function pointer
  LIST_OF_EXPRESSIONS^ param;
}

struct DISCRIMINANT_VALUE_INFO
{
  PEXPRESSION   prefix;      // has runtime constraint
  PENTITY       field;
}

struct UNC_ARRAY_AGGREGATE_INFO  // array (context-dependant size)
{
  PEXPRESSION  element;
}

struct AGGREGATE_VALUE_INFO      // array or struct (constant size)
{
  LIST_OF_EXPRESSIONS^ list;
}

struct QUALIFIED_EXPRESSION_INFO  // expression can be UNCONSTRAINED.
{
  PEXPRESSION  value;        // can be null if used inside an allocator
}

struct ARRAY_QUALIFIED_EXPRESSION_INFO
{
  PEXPRESSION  length;       // runtime array length
  PEXPRESSION  value;        // can be null if used inside an allocator
}

struct STRUCT_QUALIFIED_EXPRESSION_INFO
{
  PEXPRESSION  discriminant; // runtime discriminant value
  PEXPRESSION  value;        // can be null if used inside an allocator
}

struct ALLOCATOR_INFO
{
  PEXPRESSION  value;       // a qualified expression/array_qualified_expression/struct/qualified_expression
  bool         has_header;  // true = aligned header in heap object for length/discriminant
}

struct GLOBAL_VARIABLE_OBJECT_INFO
{
  PENTITY  pobject;      // A_GLOBAL_VARIABLE
}

struct LOCAL_VARIABLE_OBJECT_INFO
{
  PENTITY       pobject;  // A_LOCAL_VARIABLE
  TEXT_POSITION pos;
}

struct REFERENCE_OBJECT_INFO
{
  PENTITY       pobject;   // A_REFERENCE
  TEXT_POSITION pos;
}

struct PARAMETER_OBJECT_INFO   
{
  PENTITY       pobject;     // A_PARAMETER
  TEXT_POSITION pos;
}

struct ARRAY_ELEMENT_OBJECT_INFO
{
  PEXPRESSION   prefix;       // array object, or unsafe-ptr object-or-value
  PEXPRESSION   index;        // can have types int, uint, or enum (for attribute 'string).
}

struct ARRAY_SLICE_OBJECT_INFO
{
  PEXPRESSION   prefix;       // array object, or unsafe-ptr object-or-value
  PEXPRESSION   index;
  PEXPRESSION   length;
}

struct STRUCT_FIELD_OBJECT_INFO
{
  PEXPRESSION   prefix;       // has struct or union type
  PENTITY       field;        // field or varying_field
}

struct DEREFERENCED_OBJECT_INFO
{
  PEXPRESSION   prefix;       // value of type pointer
}

struct UNSAFE_DEREFERENCED_OBJECT_INFO
{
  PEXPRESSION   unsafe_ptr_value;
}

struct ATTR_BYTE_OBJECT_INFO
{
  PEXPRESSION   prefix;
}

struct BOXED_OBJECT_INFO
{
  PEXPRESSION   parameter;   // convert parameter of any packed type into byte[]
}

struct UNBOXED_OBJECT_INFO
{
  PEXPRESSION   parameter;     // convert parameter of type byte[] into any packed type
  bool          to_open_array; // true = formal parameter is open array with constraint
}

struct BOXED_ARRAY_OBJECT_INFO
{
  LIST_OF_EXPRESSIONS^ list;
}


enum FORM
{
  AN_OBJECT,
  A_VALUE,
};

enum ACCESS
{
  ACCESS_CONSTANT,    // constant value or constant object
  ACCESS_READONLY,    // runtime object or runtime value
  ACCESS_READWRITE,   // runtime object
};

struct EXPRESSION (KIND_EXPRESSION kind)
{
  PENTITY         base_type_or_null; // "null" after compilation error
  CONSTRAINT      constraint;
  FORM            form;         // AN_OBJECT / A_VALUE
  ACCESS          access;       // ACCESS_CONSTANT, ACCESS_READONLY, ACCESS_READWRITE

  switch (kind)
  {
    case A_CONST_ENUMERATION_VALUE:
      CONST_ENUMERATION_VALUE_INFO     const_enumeration_value_info;

    case A_CONST_INTEGER_VALUE:
      CONST_INTEGER_VALUE_INFO         const_integer_value_info;

    case A_CONST_FLOAT_VALUE:
      CONST_FLOAT_VALUE_INFO           const_float_value_info;

    case A_CONST_NULL_VALUE:
      CONST_NULL_VALUE_INFO            const_null_value_info;

    case A_POOL_CONSTANT:
      POOL_CONSTANT_INFO               pool_constant_info;

    case AN_OPERATOR_VALUE:
      OPERATOR_VALUE_INFO              operator_value_info;

    case A_RUN_CALL:
      RUN_CALL_INFO                    run_call_info;

    case A_FUNCTION_VALUE:
      FUNCTION_VALUE_INFO              function_value_info;

    case A_FUNCTION_CALL:
      FUNCTION_CALL_INFO               function_call_info;

    case A_DISCRIMINANT_VALUE:
      DISCRIMINANT_VALUE_INFO          discriminant_value_info;

    case AN_UNC_ARRAY_AGGREGATE:
      UNC_ARRAY_AGGREGATE_INFO         unc_array_aggregate_info;

    case AN_AGGREGATE_VALUE:
      AGGREGATE_VALUE_INFO             aggregate_value_info;

    case A_QUALIFIED_EXPRESSION:
      QUALIFIED_EXPRESSION_INFO        qualified_expression_info;

    case AN_ARRAY_QUALIFIED_EXPRESSION:
      ARRAY_QUALIFIED_EXPRESSION_INFO  array_qualified_expression_info;

    case A_STRUCT_QUALIFIED_EXPRESSION:
      STRUCT_QUALIFIED_EXPRESSION_INFO struct_qualified_expression_info;

    case AN_ALLOCATOR:
      ALLOCATOR_INFO                   allocator_info;

    case A_GLOBAL_VARIABLE_OBJECT:
      GLOBAL_VARIABLE_OBJECT_INFO      global_variable_object_info;

    case A_LOCAL_VARIABLE_OBJECT:
      LOCAL_VARIABLE_OBJECT_INFO       local_variable_object_info;

    case A_REFERENCE_OBJECT:
      REFERENCE_OBJECT_INFO            reference_object_info;

    case A_PARAMETER_OBJECT:
      PARAMETER_OBJECT_INFO            parameter_object_info;

    case AN_ARRAY_ELEMENT_OBJECT:
      ARRAY_ELEMENT_OBJECT_INFO        array_element_object_info;

    case AN_ARRAY_SLICE_OBJECT:
      ARRAY_SLICE_OBJECT_INFO          array_slice_object_info;

    case A_STRUCT_FIELD_OBJECT:
      STRUCT_FIELD_OBJECT_INFO         struct_field_object_info;

    case A_DEREFERENCED_OBJECT:
      DEREFERENCED_OBJECT_INFO         dereferenced_object_info;

    case AN_UNSAFE_DEREFERENCED_OBJECT:
      UNSAFE_DEREFERENCED_OBJECT_INFO  unsafe_dereferenced_object_info;

    case AN_ATTR_BYTE_OBJECT:
      ATTR_BYTE_OBJECT_INFO            attr_byte_object_info;

    case A_BOXED_OBJECT:
      BOXED_OBJECT_INFO                boxed_object_info;

    case AN_UNBOXED_OBJECT:
      UNBOXED_OBJECT_INFO              unboxed_object_info;

    case A_BOXED_ARRAY_OBJECT:
      BOXED_ARRAY_OBJECT_INFO          boxed_array_object_info;
  }
}


// entities

enum ENTITY_KIND
{
  //========================================================================
  // entities denoting a type must appear first
  //========================================================================

  AN_INTEGER_TYPE,
  A_FLOAT_TYPE,

  AN_ENUMERATION_TYPE,
  AN_OPEN_ARRAY_TYPE,
  AN_ARRAY_TYPE,
  A_STRUCT_TYPE,        // can be open, can be packed
  A_CONSTRAINED_STRUCT_TYPE,
  A_UNION_TYPE,
  A_POINTER_TYPE,
  A_FUNCTION_POINTER_TYPE,
  AN_UNSAFE_POINTER_TYPE,

  A_RENAMED_TYPE,
  AN_INCOMPLETE_TYPE,
  AN_OPAQUE_TYPE,
  A_GENERIC_TYPE,
  A_NULL_POINTER_TYPE,  // only used internally for 'null' literal expressions
  A_VOID_TYPE,          // only used internally for return values of function_calls

  //========================================================================
  LAST_ENTITY_DENOTING_A_TYPE,   // used as sentinel
  //========================================================================

  AN_INCOMPLETE_TYPE_COMPLETITION,    // used for generic instantiations
  AN_OPAQUE_TYPE_COMPLETITION,        // used for generic instantiations

  A_FIELD,              // of a struct or union type, can be a discriminant
  A_VARYING_FIELD,      // of an open struct, depends on a discriminant value

  AN_ENUMERATION_LITERAL,
  A_CONSTANT,
  A_GLOBAL_VARIABLE,
  A_LOCAL_VARIABLE,
  A_REFERENCE,
  A_PARAMETER,
  A_GENERIC_FUNCTION,
  A_FUNCTION_DECLARATION,
  A_FUNCTION_BODY,
  A_PACKAGE_DECLARATION,   // can be generic or not
  A_PACKAGE_BODY,
  A_UNIT_INTERFACE,  // .h
  A_UNIT_BODY,       // .c

  //========================================================================
  FIRST_ENTITY_DENOTING_A_STATEMENT,   // used as sentinel
  //========================================================================
  A_CLEAR_STATEMENT,
  AN_ASSIGNMENT_STATEMENT,
  A_PRE_OR_POSTFIX_STATEMENT,
  A_FUNCTION_CALL_STATEMENT,
  A_RETURN_STATEMENT,
  A_BREAK_STATEMENT,
  A_CONTINUE_STATEMENT,
  A_FREE_STATEMENT,
  AN_ABORT_STATEMENT,
  AN_ASSERT_STATEMENT,
  A_SLEEP_STATEMENT,
  A_CODE_STATEMENT,
  AN_UNUSED_STATEMENT,
  A_BLOCK_STATEMENT,
  AN_IF_STATEMENT,
  A_SWITCH_STATEMENT,
  A_WHILE_STATEMENT,
  A_FOR_STATEMENT,
  //========================================================================
  LAST_ENTITY_DENOTING_A_STATEMENT,   // used as sentinel
  //========================================================================
};


// ========================================================

enum INTEGER_TYPE {a_int1, a_int2, a_int4, a_int8, a_uint1, a_uint2, a_uint4};

struct INTEGER_INFO
{
  INTEGER_TYPE typ;
  wstring      name;
  TOKEN_KIND   token1;
  TOKEN_KIND   token2;
  bool         is_signed;
  int          size;
  int8         min;
  int8         max;
  int8         mask;
}

const int MAX_INTEGER_TYPES = 7;
const INTEGER_INFO INTEGER_DATA[MAX_INTEGER_TYPES] =
 {{a_int1,  L"int1",  TOKEN_int1,  TOKEN_tiny,   true,  1,   -128,   +127, 0xFF},
  {a_int2,  L"int2",  TOKEN_int2,  TOKEN_short,  true,  2, -32768, +32767, 0xFFFF},
  {a_int4,  L"int4",  TOKEN_int4,  TOKEN_int,    true,  4, -(int8)2147483648, (int8)2147483647, 0xFFFFFFFF},
  {a_int8,  L"int8",  TOKEN_int8,  TOKEN_long,   true,  8, (int8)-9223372036854775807-1, (int8)9223372036854775807, -1},
  {a_uint1, L"uint1", TOKEN_uint1, TOKEN_byte,   false, 1, 0, 255, 0xFF},
  {a_uint2, L"uint2", TOKEN_uint2, TOKEN_ushort, false, 2, 0, 65535, 0xFFFF},
  {a_uint4, L"uint4", TOKEN_uint4, TOKEN_uint,   false, 4, 0, 4294967295, 0xFFFFFFFF}};

// ========================================================

enum FLOAT_TYPE {a_float, a_double};

struct FLOAT_INFO
{
  FLOAT_TYPE typ;
  wstring    name;
  TOKEN_KIND token1;
  TOKEN_KIND token2;
  int        size;
}

const int MAX_FLOAT_TYPES = 2;
const FLOAT_INFO FLOAT_DATA [MAX_FLOAT_TYPES] =
 {{a_float,  L"float",  TOKEN_float,  TOKEN_float4,  4},
  {a_double, L"double", TOKEN_double, TOKEN_float8,  8}};

// ========================================================

struct AN_INTEGER_TYPE_ENTITY
{
  INTEGER_TYPE type;
}

struct A_FLOAT_TYPE_ENTITY
{
  FLOAT_TYPE type;
}

struct AN_ENUMERATION_TYPE_ENTITY
{
  INTEGER_TYPE base;           // must be an unsigned type
  uint4        last;           // must be <= base'last
  POOL         string_table;   // string table of all enumeration literals
}

struct AN_OPEN_ARRAY_TYPE_ENTITY
{
  PENTITY  element;    // element type
}

struct AN_ARRAY_TYPE_ENTITY
{
  PENTITY  open_array;
  uint4    length;
}

struct A_STRUCT_TYPE_ENTITY
{
  bool    is_packed;
  bool    is_open_type;
  bool    is_unsafe;    // contains at least an unsafe pointer field
  PREGION discriminant; // if .is_open_type, region contains just one discriminant field
  PREGION fields;       // fields, varying fields, anonymous type entities
  POOL    size_table;   // for open struct types : uint4 table of struct size per enum value;
                        // null means table was not created by code generator yet.
  int     header_size;  // access object header size for open struct (4 or 8 bytes),
                        // depends on largest alignment requirement of all fields.
}

struct A_CONSTRAINED_STRUCT_TYPE_ENTITY
{
  PENTITY   open_struct;
  uint4     discriminant_value;
}

struct A_UNION_TYPE_ENTITY
{
  bool    is_unsafe;    // contains at least an unsafe pointer field
  PREGION fields;       // fields, anonymous type entities
}

struct A_POINTER_TYPE_ENTITY
{
  PENTITY   designated_type;
}

struct A_FUNCTION_POINTER_TYPE_ENTITY
{
  bool     is_inline;
  bool     is_callback;          // -> means the function can be called by the OS, 
                                 // the function's address must be provided to the OS otherwise it's not included in the executable.
  bool     is_entry;             // -> means the function's symbol is exported and can be called by the OS (Android only)
  bool     is_public;            // -> declared in .h or package decl.
  bool     is_thread_entry_point;// requires M16 stack alignment at entry
  wstring^ extern_dll_or_null;   // -> means there's no body
  bool     is_syscall;
  int4     syscall_number;
  PENTITY  return_type;          // can be type_void
  PREGION  parameters;           // contains parameter entities or anonymous types
}

struct AN_UNSAFE_POINTER_TYPE_ENTITY
{
  PENTITY   designated_type;
}

struct A_RENAMED_TYPE_ENTITY
{
  PENTITY   actual_type;
}

struct AN_INCOMPLETE_TYPE_ENTITY
{
  PENTITY   full_type;              // forward reference
  bool      is_full_type_visible;
}

struct AN_OPAQUE_TYPE_ENTITY
{
  PENTITY   full_type;              // forward reference
  bool      is_full_type_visible;
  bool      is_limited;
}

struct A_GENERIC_TYPE_ENTITY
{
  PENTITY   actual_type;   // used during instantiation and in instantiated package
}

struct A_NULL_POINTER_TYPE_ENTITY
{
  bool dummy;
}

struct A_VOID_TYPE_ENTITY
{
  bool dummy;
}

//========================================================================

struct AN_INCOMPLETE_TYPE_COMPLETITION_ENTITY
{
  PENTITY   incomplete_type;
  PENTITY   full_type;
}

struct AN_OPAQUE_TYPE_COMPLETITION_ENTITY
{
  PENTITY   opaque_type;
  PENTITY   full_type;
}

struct A_FIELD_ENTITY      // of a struct or union type, can be a discriminant
{
  PENTITY   type;
}

struct A_VARYING_FIELD_ENTITY
{
  PENTITY   type;
  uint4     discriminant_value;   // field existance depends on discriminant
}

struct AN_ENUMERATION_LITERAL_ENTITY
{
  PENTITY   type;
  uint4     value;
}

struct A_CONSTANT_ENTITY
{
  PENTITY       type;
  PEXPRESSION   value;
  bool          is_used;   // is used in exp.c
  TEXT_POSITION pos;
}

struct A_GLOBAL_VARIABLE_ENTITY
{
  PENTITY       type;
  PEXPRESSION   initial_value_or_null;   // a constant initial value
  bool          is_volatile;
  int8          offset;
}

struct A_LOCAL_VARIABLE_ENTITY
{
  PENTITY       type;
  PEXPRESSION   initial_value_or_null;
  bool          is_read;
  bool          is_written;
  bool          is_used;      // in any primary or if it has initial value
  TEXT_POSITION pos;
  LOCATION      loc;
  int4          offset;       // always negative
  bool          allocated_on_heap;
}

struct A_REFERENCE_ENTITY
{
  PENTITY       type;
  MODE          mode;         // MODE_IN or MODE_REF
  PEXPRESSION   name;
  bool          is_used;      // is read or used in attribute prefix
  bool          references_heap_object;   // requires a tombstone (code generator)
  TEXT_POSITION pos;
  LOCATION      loc;
  int4          offset;       // always negative
}

struct A_PARAMETER_ENTITY
{
  PENTITY       type;
  MODE          mode;
  PEXPRESSION   default_value_or_null;   // an optional constant initial value
  bool          is_used;      // is read or used in attribute
  bool          init_error;   // used to avoid giving a non-initialized error twice
  TEXT_POSITION pos;          // position of parameter in function body
  int4          offset;       // address offset[EBP] (always positive for Intel)
}

struct A_GENERIC_FUNCTION_ENTITY
{
  PENTITY   to_type;      // to A_FUNCTION_POINTER_TYPE
  PENTITY   actual_func;  // used during instantiation and in instantiated package (to generic function/function decl)
}

struct A_FUNCTION_DECLARATION_ENTITY
{
  PENTITY   to_type;                    // to A_FUNCTION_POINTER_TYPE
  PENTITY   to_function_body_or_null;   // forward reference
  bool      code_generated;             // true=code was generated for this function
  int4      frame_size;                 // always negative (to be subtracted from EBP)
  int4      func_label_nr;              // function label nr (0 = none)
}

struct A_FUNCTION_BODY_ENTITY   // this entity has no identifier
{
  PENTITY   to_function_declaration;
  PREGION   inner;          // contains declarations and statements
  LOCATION  begin_loc;
  LOCATION  end_loc;
  bool      contains_code_statements;
}

struct A_PACKAGE_DECLARATION_ENTITY
{
  PENTITY   to_package_body_or_null;// forward reference
  bool      is_generic;
  bool      is_body_required;
  PREGION   generic_part;   // contains generic type, generic function, anonymous types or constants
  PREGION   declarations;
}

struct A_PACKAGE_BODY_ENTITY    // this entity has no identifier
{
  PENTITY   to_package_declaration;
  PREGION   declarations;
}

struct A_UNIT_INTERFACE_ENTITY
{
  PENTITY   to_unit_body;   // forward reference
  bool      is_body_required;
  bool      unit_is_used;   // is referenced at least once
  PREGION   declarations;
}

struct A_UNIT_BODY_ENTITY      // this entity has no identifier
{
  PENTITY   to_unit_interface;
  PREGION   declarations;
}

//========================================================================

struct A_CLEAR_STATEMENT_ENTITY
{
  PEXPRESSION name;    // object to clear
  LOCATION    loc;
}

enum ASSIGNMENT_OP
{
  _ASSIGN,

  _ASSIGN_ADD_INT,            // for integer, enum
  _ASSIGN_SUB_INT,

  _ASSIGN_ADD_PTR_INT,        // for ptr + int -> ptr+int*size(elem)
  _ASSIGN_SUB_PTR_INT,        // for ptr - int -> ptr-int*size(elem) (type ptr)

  _ASSIGN_MULT_INT_SIGNED,    // for signed integer
  _ASSIGN_DIV_INT_SIGNED,
  _ASSIGN_MOD_INT_SIGNED,

  _ASSIGN_MULT_INT_UNSIGNED,  // for unsigned integer
  _ASSIGN_DIV_INT_UNSIGNED,
  _ASSIGN_MOD_INT_UNSIGNED,

  _ASSIGN_ADD_FLOAT,          // for floating-point
  _ASSIGN_SUB_FLOAT,
  _ASSIGN_MULT_FLOAT,
  _ASSIGN_DIV_FLOAT,

  _ASSIGN_SHIFT_LEFT_SIGNED,      // for integer
  _ASSIGN_SHIFT_LEFT_UNSIGNED,

  _ASSIGN_SHIFT_RIGHT_SIGNED,     // for integer
  _ASSIGN_SHIFT_RIGHT_UNSIGNED,

  _ASSIGN_AND,       // for bool
  _ASSIGN_OR,
  _ASSIGN_XOR,

  _ASSIGN_BITAND,    // for integer
  _ASSIGN_BITOR,
  _ASSIGN_BITXOR,
};

struct AN_ASSIGNMENT_STATEMENT_ENTITY
{
  PEXPRESSION    name;
  ASSIGNMENT_OP  op;
  PEXPRESSION    value;
  LOCATION       loc;
}

struct A_PRE_OR_POSTFIX_STATEMENT_ENTITY
{
  PEXPRESSION  name;
  LOCATION     loc;
}

struct A_FUNCTION_CALL_STATEMENT_ENTITY
{
  PEXPRESSION  name;
  LOCATION     loc;
}

struct A_RETURN_STATEMENT_ENTITY
{
  PEXPRESSION   value;   // null if no return value
  PENTITY       type;
  PENTITY       outer;   // surrounding function body, or block/if/switch/while/for statement.
  TEXT_POSITION pos;
  LOCATION      loc;
}

struct A_BREAK_STATEMENT_ENTITY
{
  PENTITY       outer;   // surrounding function body, or block/if/switch/while/for statement.
  TEXT_POSITION pos;
}

struct A_CONTINUE_STATEMENT_ENTITY
{
  PENTITY       outer;   // surrounding function body, or block/if/switch/while/for statement.
  TEXT_POSITION pos;
}

struct A_FREE_STATEMENT_ENTITY
{
  PEXPRESSION  value;
  LOCATION     loc;
}

struct AN_ABORT_STATEMENT_ENTITY
{
  LOCATION      loc;
  TEXT_POSITION pos;
}

struct AN_ASSERT_STATEMENT_ENTITY
{
  PEXPRESSION  value;
  LOCATION     loc;
}

struct A_SLEEP_STATEMENT_ENTITY
{
  PEXPRESSION  value;
  LOCATION     loc;
}

struct A_CODE_STATEMENT_ENTITY
{
  LOCATION     loc;
  byte[]^      code;
  int          nb_bytes;
}

struct AN_UNUSED_STATEMENT_ENTITY
{
  TEXT_POSITION pos;
  PENTITY       item;
}

struct A_BLOCK_STATEMENT_ENTITY
{
  PREGION  inner;    // contain declarations and statements
  PENTITY  outer;    // surrounding function body, or block/if/switch/while/for statement.
}

struct AN_IF_STATEMENT_ENTITY
{
  PEXPRESSION  condition;
  PREGION      true_branch;
  PREGION      false_branch;
  PENTITY      outer;    // surrounding function body, or block/if/switch/while/for statement.
  LOCATION     loc;
}

struct CASE_CONSTANT
{
  int8            value;
  CASE_CONSTANT^  next;
}

struct FLOW_ALTERNATIVE
{
  bool              is_default;    // true = last default case
  CASE_CONSTANT^    cte_list;
  PREGION           inner;
  FLOW_ALTERNATIVE^ next;
  int               label_nr_drop;      // for code generator
  int               label_nr_no_drop;
  bool              code_generated;
}

struct A_SWITCH_STATEMENT_ENTITY
{
  PEXPRESSION       value;
  uint4             count;     // nb case constants, not counting the last default case.
  FLOW_ALTERNATIVE^ first_alt;
  FLOW_ALTERNATIVE^ last_alt;
  PENTITY           outer;     // surrounding function body, or block/if/switch/while/for statement.
  LOCATION          loc;
}

struct A_WHILE_STATEMENT_ENTITY
{
  PEXPRESSION   condition;
  PREGION       inner;
  PENTITY       outer;    // surrounding function body, or block/if/switch/while/for statement.
  TEXT_POSITION pos;      // while token
  LOCATION      loc;      // of condition
}

struct A_FOR_STATEMENT_ENTITY
{
  PREGION      pre;
  PREGION      exp_region;  // exp_region must be elaborated BEFORE evaluating condition !
  PEXPRESSION  condition;   // null means true
  PREGION      post;
  PREGION      inner;
  PENTITY      outer;    // surrounding function body, or block/if/switch/while/for statement.
  TEXT_POSITION pos;     // for token
  LOCATION     loc;      // of condition
}

/******************************************************************************/

struct ENTITY (ENTITY_KIND kind)
{
  PENTITY       next;                  // next entity in declaration queue
  long          nr;                    // unique entity number (for import/export/pcodes)
  wstring^      identifier_or_null;    // allocated on heap
  PENTITY       ptr[2];                // pointers to left/right identifier btrees
  tiny          bal;                   // -1, 0 or +1 (balancing coefficient for btree node)

  switch (kind)
  {
    case AN_INTEGER_TYPE:
      AN_INTEGER_TYPE_ENTITY            the_integer_type;
    case A_FLOAT_TYPE:
      A_FLOAT_TYPE_ENTITY               the_float_type;
    case AN_ENUMERATION_TYPE:
      AN_ENUMERATION_TYPE_ENTITY        the_enumeration_type;
    case AN_OPEN_ARRAY_TYPE:
      AN_OPEN_ARRAY_TYPE_ENTITY         the_open_array_type;
    case AN_ARRAY_TYPE:
      AN_ARRAY_TYPE_ENTITY              the_array_type;
    case A_STRUCT_TYPE:
      A_STRUCT_TYPE_ENTITY              the_struct_type;
    case A_CONSTRAINED_STRUCT_TYPE:
      A_CONSTRAINED_STRUCT_TYPE_ENTITY  the_constrained_struct_type;
    case A_UNION_TYPE:
      A_UNION_TYPE_ENTITY               the_union_type;
    case A_POINTER_TYPE:
      A_POINTER_TYPE_ENTITY             the_pointer_type;
    case A_FUNCTION_POINTER_TYPE:
      A_FUNCTION_POINTER_TYPE_ENTITY    the_function_pointer_type;
    case AN_UNSAFE_POINTER_TYPE:
      AN_UNSAFE_POINTER_TYPE_ENTITY     the_unsafe_pointer_type;
    case A_RENAMED_TYPE:
      A_RENAMED_TYPE_ENTITY             the_renamed_type;
    case AN_INCOMPLETE_TYPE:
      AN_INCOMPLETE_TYPE_ENTITY         the_incomplete_type;
    case AN_OPAQUE_TYPE:
      AN_OPAQUE_TYPE_ENTITY             the_opaque_type;
    case A_GENERIC_TYPE:
      A_GENERIC_TYPE_ENTITY             the_generic_type;
    case A_NULL_POINTER_TYPE:
      A_NULL_POINTER_TYPE_ENTITY        the_null_pointer_type;
    case A_VOID_TYPE:
      A_VOID_TYPE_ENTITY                the_void_type;
    case AN_INCOMPLETE_TYPE_COMPLETITION:
      AN_INCOMPLETE_TYPE_COMPLETITION_ENTITY the_incomplete_type_completition;
    case AN_OPAQUE_TYPE_COMPLETITION:
      AN_OPAQUE_TYPE_COMPLETITION_ENTITY     the_opaque_type_completition;
    case A_FIELD:
      A_FIELD_ENTITY                    the_field;
    case A_VARYING_FIELD:
      A_VARYING_FIELD_ENTITY            the_varying_field;
    case AN_ENUMERATION_LITERAL:
      AN_ENUMERATION_LITERAL_ENTITY     the_enumeration_literal;
    case A_CONSTANT:
      A_CONSTANT_ENTITY                 the_constant;
    case A_GLOBAL_VARIABLE:
      A_GLOBAL_VARIABLE_ENTITY          the_global_variable;
    case A_LOCAL_VARIABLE:
      A_LOCAL_VARIABLE_ENTITY           the_local_variable;
    case A_REFERENCE:
      A_REFERENCE_ENTITY                the_reference;
    case A_PARAMETER:
      A_PARAMETER_ENTITY                the_parameter;
    case A_GENERIC_FUNCTION:
      A_GENERIC_FUNCTION_ENTITY         the_generic_function;
    case A_FUNCTION_DECLARATION:
      A_FUNCTION_DECLARATION_ENTITY     the_function_declaration;
    case A_FUNCTION_BODY:
      A_FUNCTION_BODY_ENTITY            the_function_body;
    case A_PACKAGE_DECLARATION:
      A_PACKAGE_DECLARATION_ENTITY      the_package_declaration;
    case A_PACKAGE_BODY:
      A_PACKAGE_BODY_ENTITY             the_package_body;
    case A_UNIT_INTERFACE:
      A_UNIT_INTERFACE_ENTITY           the_unit_interface;
    case A_UNIT_BODY:
      A_UNIT_BODY_ENTITY                the_unit_body;
    case A_CLEAR_STATEMENT:
      A_CLEAR_STATEMENT_ENTITY          the_clear_statement;
    case AN_ASSIGNMENT_STATEMENT:
      AN_ASSIGNMENT_STATEMENT_ENTITY    the_assignment_statement;
    case A_PRE_OR_POSTFIX_STATEMENT:
      A_PRE_OR_POSTFIX_STATEMENT_ENTITY the_pre_or_postfix_statement;
    case A_FUNCTION_CALL_STATEMENT:
      A_FUNCTION_CALL_STATEMENT_ENTITY  the_function_call_statement;
    case A_RETURN_STATEMENT:
      A_RETURN_STATEMENT_ENTITY         the_return_statement;
    case A_BREAK_STATEMENT:
      A_BREAK_STATEMENT_ENTITY          the_break_statement;
    case A_CONTINUE_STATEMENT:
      A_CONTINUE_STATEMENT_ENTITY       the_continue_statement;
    case A_FREE_STATEMENT:
      A_FREE_STATEMENT_ENTITY           the_free_statement;
    case AN_ABORT_STATEMENT:
      AN_ABORT_STATEMENT_ENTITY         the_abort_statement;
    case AN_ASSERT_STATEMENT:
      AN_ASSERT_STATEMENT_ENTITY        the_assert_statement;
    case A_SLEEP_STATEMENT:
      A_SLEEP_STATEMENT_ENTITY          the_sleep_statement;
    case A_CODE_STATEMENT:
      A_CODE_STATEMENT_ENTITY           the_code_statement;
    case AN_UNUSED_STATEMENT:
      AN_UNUSED_STATEMENT_ENTITY        the_unused_statement;
    case A_BLOCK_STATEMENT:
      A_BLOCK_STATEMENT_ENTITY          the_block_statement;
    case AN_IF_STATEMENT:
      AN_IF_STATEMENT_ENTITY            the_if_statement;
    case A_SWITCH_STATEMENT:
      A_SWITCH_STATEMENT_ENTITY         the_switch_statement;
    case A_WHILE_STATEMENT:
      A_WHILE_STATEMENT_ENTITY          the_while_statement;
    case A_FOR_STATEMENT:
      A_FOR_STATEMENT_ENTITY            the_for_statement;
  }
}

/******************************************************************************/

struct DECLARATION_QUEUE
{
  PENTITY  first, last;
}

/******************************************************************************/

// for hiding not fully declared entities (ex: object, reference, function, parameters)
// or for providing a list of inner packages.

struct ENTITY_LIST
{
  PENTITY      e;
  ENTITY_LIST^ next;
}

void insert_entity_in_list (PENTITY e, ref ENTITY_LIST^ root);

struct REGION
{
  DECLARATION_QUEUE entities;           // entities in declaration order
  PENTITY           identifier_btree;
  ENTITY_LIST^      packages;
  PREGION           next;
}

/******************************************************************************/

void create_predefined_entities ();

PENTITY  type_bool, type_char, type_wchar, type_byte, type_int, type_uint, type_long;
PENTITY  type_float, type_double;
PENTITY  type_string, type_wstring, type_object, type_array_of_string, type_array_of_object;
PENTITY  type_int_literal, type_float_literal, type_null_literal, type_unsafe_ptr_to_byte;
PENTITY  type_void;
PENTITY  enum_false, enum_true;

PENTITY global_locked_struct_entity;  // locked during struct declaration

/******************************************************************************/

// returns the type entity corresponding to the token,
// or null if the token does not denote a type.

PENTITY type_entity_of_token (TOKEN_KIND kind);

/******************************************************************************/

void create_region_level ();
void unlink_region_level ();

PREGION new_region ();
void append_region (PREGION r);

void cleanup_main_region_after_compilation ();

PENTITY append_new_entity (ENTITY        entity,
                           wstring       identifier,  // can be empty string
                           TEXT_POSITION pos);        // for error if declared twice

/******************************************************************************/

// create new entity and insert it in symbol tree,
// but does not append it in elaboration queue.

PENTITY create_new_entity (ENTITY        entity,
                           wstring       identifier,   // can be empty string
                           TEXT_POSITION pos);

/******************************************************************************/

void append_entity_in_entity_queue (PENTITY e);

/******************************************************************************/

void patch_entity_by_setting_an_identifier (PENTITY       e,
                                            wstring       identifier,   // can be empty string
                                            TEXT_POSITION pos);

/******************************************************************************/

PENTITY append_new_entity_for_instantiation (PENTITY       e0,
                                             TEXT_POSITION pos,
                                             PREGION       pregion);

/******************************************************************************/

void append_new_unit_entity_for_import
                      (PENTITY       e,
                       wstring       identifier,   // can be empty string
                       TEXT_POSITION pos,
                       bool          add_in_package_list);

/******************************************************************************/

// produces error messages if not found or ambiguous
// returns null if not found

PENTITY find_entity (wstring identifier, TEXT_POSITION pos);

/******************************************************************************/

// does not produce error messages
// returns null if not found

PENTITY search_entity_in_local_scope (wstring identifier);

/******************************************************************************/

// returns null if not found

PENTITY find_tree (PENTITY tree, wstring identifier);

/******************************************************************************/

// returns entity surrounding current scope (unit, package, ..)

PENTITY surrounding_entity ();

/******************************************************************************/

// check that the current search point is within the package declaration or package body

bool are_we_in_package (PENTITY epackage);

/******************************************************************************/

// assert: current token is an identifier
// returns null if not found

PENTITY parse_expanded_name ();

/******************************************************************************/

void hide_entity (PENTITY e);
void unhide_all_entities ();

/******************************************************************************/

// insert package declaration in package list of surrounding scope
void append_package_declaration_to_surrounding_scope_package_list (PENTITY e);

/******************************************************************************/

ENTITY_LIST^ build_list_of_surrounding_generic_packages ();

/******************************************************************************/

ENTITY_LIST^         duplicate_entity_list         (ENTITY_LIST^ list);
LIST_OF_EXPRESSIONS^ duplicate_list_of_expressions (LIST_OF_EXPRESSIONS^ list);
CASE_CONSTANT^       duplicate_case_constant_list  (CASE_CONSTANT^ list);

/******************************************************************************/

void store_location (out LOCATION l);

/******************************************************************************/
