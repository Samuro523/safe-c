
// codexp.c : code generator for expressions, declarations and simple statements

from std use arithm, strings;
use ../pcodes, ../error, ../goptions, ../codout, ../dllnames, ../pool, ../common, ../codgen;
use entities, type, exp, overlap, heaptype, tokens, wstrings;

/*****************************************************************************/

struct EXP_INPUT
{
  // true = we need to compute a value or address,
  // false = compute address only if needed for constraint.
  bool      data_must_be_computed;

  bool      constraint_must_be_computed;

  TOMBSTONE tomb;    // stores tombstone anchor if heap object
}

/*******************************************************************************************/

enum EXP_KIND
{
  EXP_NONE,                 // (must be first literal)

  EXP_CONST_INT,
  EXP_CONST_FLOAT,
  EXP_CONST_NULL,

  EXP_ON_INT_STACK_B,
  EXP_ON_INT_STACK_I4,     // uses .sign
  EXP_ON_INT_STACK_I8,
  EXP_ON_FLOAT_STACK_F4,
  EXP_ON_FLOAT_STACK_F8,

  EXP_AT_ADDR_OFFSET,      // at addr_stack + offset, there's the object
  EXP_AT_ADDR_INDIRECT,    // at addr_stack, there's a pointer to the object
};

enum SIGN   // used for EXP_ON_INT_STACK_I4
{
  _DONTCARE,   // don't care because bit 31 is always zero
  _SIGNED,
  _UNSIGNED
};

struct EXP_INFO
{
  EXP_KIND kind;

  // constants
  int8     value;      // for EXP_CONST_INT
  double   fvalue;     // for EXP_CONST_FLOAT

  SIGN     sign;       // for EXP_ON_INT_STACK_I4

  int4     offset;     // for EXP_AT_ADDR_OFFSET
}

enum CONSTRAINT_PLACE
{
  CT_NONE,           // no constraint available  (must be first literal)
  CT_CTE,            // constraint is uint4 constant in .cte
  CT_TEMP,           // constraint is uint4 local variable at .temp_offset[EBP]
  CT_ON_INT_STACK,   // constraint is uint4 on int_stack
  CT_AT_ADDR_OFFSET, // constraint is uint4 at addr_stack + .offset
};

struct CONSTRAINT_INFO
{
  CONSTRAINT_PLACE place;
  uint4            cte;               // for CT_CTE
  int4             temp_offset;       // for CT_TEMP
  int4             offset;            // for CT_AT_ADDR_OFFSET
}

struct EXP_OUTPUT
{
  PENTITY         base_type;    // used for storing simple types in pool or temp variables
  EXP_INFO        exp;
  CONSTRAINT_INFO constraint;
}

/*******************************************************************************************/

struct ARM_PARAM_ALLOCATION
{
  int x_reg;  // 0 to 7
  int f_reg;  // 0 to 7
  int stack;  // 0, increment by 8
}

/*******************************************************************************************/

typedef int[3] IFA_STACK_SLOTS;   // counts nb of entries in istack, fstack, astack

/*******************************************************************************************/

struct PARAMETER_LOCATION
{
  byte type;   // 0 = x reg, 1 = f reg, 2 = stack
  int4 nr;     // register number or stack offset
  bool is_signed;
}

/*******************************************************************************************/

void count_ifa_slots_simple_type (ref IFA_STACK_SLOTS ifa, PENTITY type)
{
  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
    case AN_INTEGER_TYPE:
      ifa[0]++;
      break;

    case A_FLOAT_TYPE:
      ifa[1]++;
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      ifa[2]++;
      break;

    default:
      abort;
  }
}

/*******************************************************************************************/

void compute_arm_register_info (ref ARM_PARAM_ALLOCATION arm,
                                    bool                 is_float,
                                    bool                 is_signed,
                                    bool                 is_hint,
                                out PARAMETER_LOCATION   loc)
{
  clear loc;

  if (is_float)
  {
    if (arm.f_reg < 8)
    {
      loc.type = 1;
      loc.nr   = arm.f_reg;

      if (!is_hint)
        arm.f_reg++;
    }
    else
    {
      loc.type = 2;
      loc.nr   = arm.stack;

      if (!is_hint)
        arm.stack += 8;
    }
  }
  else
  {
    if (arm.x_reg < 8)
    {
      loc.type = 0;
      loc.nr   = arm.x_reg;

      if (!is_hint)
        arm.x_reg++;
    }
    else
    {
      loc.type = 2;
      loc.nr   = arm.stack;

      if (!is_hint)
        arm.stack += 8;
    }
  }

  loc.is_signed = is_signed;
}

/*******************************************************************************************/

// for address + constraint

void compute_pair_of_arm_register_info (ref ARM_PARAM_ALLOCATION arm,
                                        out PARAMETER_LOCATION   loc_address,
                                        out PARAMETER_LOCATION   loc_constraint)
{
  clear loc_address, loc_constraint;

  if (arm.x_reg+1 < 8)   // enough space for 2 registers
  {
    loc_address.type = 0;   // registers
    loc_address.nr   = arm.x_reg;
    arm.x_reg++;

    loc_constraint.type = 0;   // registers
    loc_constraint.nr   = arm.x_reg;
    arm.x_reg++;
  }
  else   // otherwise on stack
  {
    loc_address.type = 2;    // on stack
    loc_address.nr   = arm.stack;
    arm.stack += 8;

    loc_constraint.type = 2;    // on stack
    loc_constraint.nr   = arm.stack;
    arm.stack += 8;
  }
}

/*******************************************************************************************/

void put_arm_location (PARAMETER_LOCATION loc, bool is_hint)
{
  put_byte (loc.type);  // 0 = x reg, 1 = f reg, 2 = stack
  put_int4 (loc.nr);    // register number or stack offset

  if (!is_hint)
    put_byte ((byte)loc.is_signed);
}

/*******************************************************************************************/

void put_arm_register_reg_nr_info (ref ARM_PARAM_ALLOCATION arm, bool is_float, bool is_signed, bool is_hint)
{
  PARAMETER_LOCATION  loc;
  compute_arm_register_info (ref arm, is_float, is_signed, is_hint, out loc);
  put_arm_location (loc, is_hint);
}

/*******************************************************************************************/

void generate_code_for_expression
  (    PEXPRESSION e,
       EXP_INPUT   input,
   ref FRAME_INFO  frame,    // to allocate temporary variables and small aggregate space
   out EXP_OUTPUT  output);

/*******************************************************************************************/

// for objects, returns either _YES or _NO.
// for values, returns _YES, _NO or _YES_BUT_CAN_BE_NULL.

public
USES_TOMBSTONE exp_requires_tombstone_anchor (PEXPRESSION exp)
{
  USES_TOMBSTONE u1, u2;

  if (!pointer_checks_enabled)
    return _NO;

  switch (exp^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
    case A_CONST_INTEGER_VALUE:
    case A_CONST_FLOAT_VALUE:
    case A_CONST_NULL_VALUE:
    case A_POOL_CONSTANT:
      return _NO;

    case AN_OPERATOR_VALUE:
      if (exp^.operator_value_info.op != OP_CONDITIONAL_TEST)
        return _NO;

      {
        PENTITY type;
        type = complete_type_of (exp^.base_type_or_null);
        if (type^.kind == AN_INTEGER_TYPE || type^.kind == A_FLOAT_TYPE            || type^.kind == AN_ENUMERATION_TYPE ||
            type^.kind == A_POINTER_TYPE  || type^.kind == A_FUNCTION_POINTER_TYPE || type^.kind == AN_UNSAFE_POINTER_TYPE)
          return _NO;      // don't preallocate a tombstone for simple types
      }

      u1 = exp_requires_tombstone_anchor (exp^.operator_value_info.arg[1]);
      u2 = exp_requires_tombstone_anchor (exp^.operator_value_info.arg[2]);

      if (u1 == _YES && u2 == _YES)
        return _YES;
      if (u1 == _NO && u2 == _NO)
        return _NO;
      return _YES_BUT_CAN_BE_NULL;

    case A_RUN_CALL:
    case A_FUNCTION_VALUE:
    case A_FUNCTION_CALL:
    case A_DISCRIMINANT_VALUE:
    case AN_UNC_ARRAY_AGGREGATE:
    case AN_AGGREGATE_VALUE:
      return _NO;

    case A_QUALIFIED_EXPRESSION:
      if (exp^.qualified_expression_info.value == null)
        return _NO;

      {
        PENTITY type;
        type = complete_type_of (exp^.base_type_or_null);
        if (type^.kind == AN_INTEGER_TYPE || type^.kind == A_FLOAT_TYPE            || type^.kind == AN_ENUMERATION_TYPE ||
            type^.kind == A_POINTER_TYPE  || type^.kind == A_FUNCTION_POINTER_TYPE || type^.kind == AN_UNSAFE_POINTER_TYPE)
          return _NO;      // don't preallocate a tombstone for simple types
      }

      return exp_requires_tombstone_anchor (exp^.qualified_expression_info.value);

    case AN_ALLOCATOR:
    case A_GLOBAL_VARIABLE_OBJECT:
    case A_LOCAL_VARIABLE_OBJECT:
    case A_REFERENCE_OBJECT:
    case A_PARAMETER_OBJECT:
      return _NO;

    case AN_ARRAY_ELEMENT_OBJECT:
      return exp_requires_tombstone_anchor (exp^.array_element_object_info.prefix);

    case AN_ARRAY_SLICE_OBJECT:
      return exp_requires_tombstone_anchor (exp^.array_slice_object_info.prefix);

    case A_STRUCT_FIELD_OBJECT:
      return exp_requires_tombstone_anchor (exp^.struct_field_object_info.prefix);

    case A_DEREFERENCED_OBJECT:
      return _YES;

    case AN_UNSAFE_DEREFERENCED_OBJECT:
      return _NO;

    case AN_ATTR_BYTE_OBJECT:
      return exp_requires_tombstone_anchor (exp^.attr_byte_object_info.prefix);

    case A_BOXED_OBJECT:
      {
        PENTITY     type;
        ENTITY_KIND k;

        type = complete_type_of (exp^.boxed_object_info.parameter^.base_type_or_null);
        k = type^.kind;

        if (exp^.access == ACCESS_READONLY &&      // parameter of mode 'in' of simple type
            (k == AN_INTEGER_TYPE || k == A_FLOAT_TYPE            || k == AN_ENUMERATION_TYPE ||
             k == A_POINTER_TYPE  || k == A_FUNCTION_POINTER_TYPE || k == AN_UNSAFE_POINTER_TYPE))
          return _NO;
      }
      return exp_requires_tombstone_anchor (exp^.boxed_object_info.parameter);

    case AN_UNBOXED_OBJECT:
      return exp_requires_tombstone_anchor (exp^.unboxed_object_info.parameter);

    case A_BOXED_ARRAY_OBJECT:
      return _NO;   // for function calls, tombstones of inner objects are passed in a list.

    default:
      fatal_compiler_error0 ("exp_requires_tombstone_anchor()");
      return _NO;
  }
}

/*******************************************************************************************/

void allocate_tombstone (    PEXPRESSION exp,
                         ref TOMBSTONE   tomb,
                         ref FRAME_INFO  frame)
{
  USES_TOMBSTONE u;

  u = exp_requires_tombstone_anchor (exp);

  if (u != _NO)
  {
    tomb.anchor_provided = u;
    tomb.anchor_offset = allocate_temp_variable (address_size, ref frame);
  }
}

/*****************************************************************************/

void allocate_tombstone_anchor (    PEXPRESSION exp,
                                ref EXP_INPUT   input,
                                ref FRAME_INFO  frame)
{
  allocate_tombstone (exp, ref input.tomb, ref frame);
}

/*****************************************************************************/

void clear_tombstone_anchor (EXP_INPUT input)
{
  if (input.tomb.anchor_provided == _NO)
    return;

  put_code (P_LOAD_LOCAL);
  put_int4 (input.tomb.anchor_offset);
  put_code (P_CTE_NULL);
  put_code (P_STORE_ADDR);
}

/*****************************************************************************/

void release_tombstone (TOMBSTONE tomb)
{
  USES_TOMBSTONE u;

  u = tomb.anchor_provided;

  if (u == _YES)
  {
    put_code (P_UNDEREF);
    put_int4 (tomb.anchor_offset);
    put_int4 (get_new_near_label_nr());
  }
  else if (u == _YES_BUT_CAN_BE_NULL)
  {
    int label_nr = get_new_near_label_nr ();

    put_code (P_LOAD_LOCAL);
    put_int4 (tomb.anchor_offset);
    put_code (P_VALUE_ADDR);
    put_code (P_CTE_NULL);
    put_code (P_CMP_ADDR);
    put_byte ((byte)CMP_EQUAL);
    put_code (P_BTRUE);
    put_int4 (label_nr);

    put_code (P_UNDEREF);
    put_int4 (tomb.anchor_offset);
    put_int4 (get_new_near_label_nr());

    put_code (P_NEAR_LABEL);
    put_int4 (label_nr);
  }
}

/*****************************************************************************/

void release_tombstone_anchor (EXP_INPUT input)
{
  release_tombstone (input.tomb);
}

/*****************************************************************************/

void flush_constraint (ref EXP_OUTPUT output)
{
  switch (output.constraint.place)
  {
    case CT_CTE:            // constraint is uint4 constant in .cte
      put_code (P_CTE_4);
      put_int4 ((int)output.constraint.cte);
      break;

    case CT_TEMP:           // constraint is uint4 local variable at .temp_offset[EBP]
      put_code (P_LOAD_LOCAL);
      put_int4 (output.constraint.temp_offset);
      put_code (P_VALUE_4);
      break;

    case CT_ON_INT_STACK:   // constraint is uint4 on int_stack
      break;

    case CT_AT_ADDR_OFFSET: // constraint is uint4 at addr_stack + .offset
      put_code (P_GET_CONSTR0);
      put_int4 (output.constraint.offset);
      break;

    default:
      fatal_compiler_error0 ("flush_constraint()");
      break;
  }

  output.constraint.place = CT_ON_INT_STACK;
}

/*****************************************************************************/

// used for assignment or copy into boxed_array_object
// when there's already an address on addr_stack

void flush_constraint_over_address (ref EXP_OUTPUT output)
{
  if (output.constraint.place == CT_AT_ADDR_OFFSET)  // constraint is uint4 at addr_stack + .offset
  {
    put_code (P_GET_CONSTR1);
    put_int4 (output.constraint.offset);
    output.constraint.place = CT_ON_INT_STACK;
  }
  else
  {
    flush_constraint (ref output);
  }
}

/*****************************************************************************/

void discard_constraint (ref EXP_OUTPUT output)
{
  if (output.constraint.place == CT_ON_INT_STACK)   // constraint is uint4 on int_stack
  {
    put_code (P_DROP_4);
  }

  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

// convert any simple constants into pool objects and take their address;
// add offset, or perform indirection of addresses;
// discard any constraint.

void flush_address (ref EXP_OUTPUT output)
{
  int  rc, size, align;
  POOL p;

  switch (output.exp.kind)
  {
    case EXP_CONST_INT:   // can be enumeration or integer type

      rc = size_and_alignment_of_type (output.base_type, out size, out align);
      if (rc != 0)
        fatal_compiler_error0 ("flush_address(1)");

      // reserve on pool
      p = new_pool_constant ((uint)size, (uint)align);
      store_integer (p, 0, output.exp.value, (uint)size);

      put_code (P_LOAD_CONST);
      put_int8 (serial_nr_of_pool_cte (p));
      break;

    case EXP_CONST_FLOAT:
      rc = size_and_alignment_of_type (output.base_type, out size, out align);
      if (rc != 0)
        fatal_compiler_error0 ("flush_address(2)");

      // reserve on pool
      p = new_pool_constant ((uint)size, (uint)align);
      store_float (p, 0, output.exp.fvalue, (uint)size);

      put_code (P_LOAD_CONST);
      put_int8 (serial_nr_of_pool_cte (p));
      break;

    case EXP_CONST_NULL:
      // reserve on pool
      p = new_pool_constant ((uint)address_size, (uint)address_size);     // pool zone is zeroed by default

      put_code (P_LOAD_CONST);
      put_int8 (serial_nr_of_pool_cte (p));

      break;

    case EXP_AT_ADDR_OFFSET:
      if (output.exp.offset != 0)
      {
        put_code (P_ADD_OFFSET);
        put_int4 (output.exp.offset);
      }
      break;

    case EXP_AT_ADDR_INDIRECT:
      put_code (P_VALUE_ADDR);
      break;

    default:
      fatal_compiler_error0 ("flush_address()");
      break;
  }

  output.exp.kind = EXP_AT_ADDR_OFFSET;
  output.exp.offset = 0;

  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

// used for assignment when there's already an address on top stack position

void flush_address_over_address (ref EXP_OUTPUT output)
{
  switch (output.exp.kind)
  {
    case EXP_AT_ADDR_OFFSET:
      if (output.exp.offset != 0)
      {
        put_code (P_ADD_OFFSET1);
        put_int4 (output.exp.offset);
      }
      break;

    case EXP_AT_ADDR_INDIRECT:
      put_code (P_VALUE_ADDR1);
      break;

    default:
      fatal_compiler_error0 ("flush_address_over_address()");
      break;
  }

  output.exp.kind = EXP_AT_ADDR_OFFSET;
  output.exp.offset = 0;

  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

// discard address on addr_stack, but only if it's not needed for constraint anymore

void discard_address (ref EXP_OUTPUT output)
{
  if (output.exp.kind == EXP_AT_ADDR_OFFSET ||
      output.exp.kind == EXP_AT_ADDR_INDIRECT)
  {
    if (output.constraint.place != CT_AT_ADDR_OFFSET)
    {
      put_code (P_DROP_ADDR);
    }
  }

  output.exp.kind = EXP_NONE;
}

/*****************************************************************************/

void generate_size_table_for_open_struct (PENTITY struct_type)
{
  int8        length;
  POOL        p;
  int         i, rc, size, align;
  PEXPRESSION exp;

  if (struct_type^.the_struct_type.size_table != null)   // table already exists
    return;


  length = struct_type^.the_struct_type.discriminant^.entities.first
                      ^.the_field.type
                      ^.the_enumeration_type.last + (int8)1;

  if ((length*4) > 2147483647)
    fatal_compiler_error0 ("generate_size_table_for_open_struct()");

  p = new_pool_constant (((uint4)length)*4, 4);
  struct_type^.the_struct_type.size_table = p;

  exp = new EXPRESSION (A_GLOBAL_VARIABLE_OBJECT);
  exp^.base_type_or_null = struct_type;
  exp^.constraint.kind = CONSTANT_CONSTRAINT;

  for (i=0; i<(int)length; i++)
  {
    exp^.constraint.value = (uint)i;

    // compute open struct size with discriminant value i
    // can be jagged !
    rc = size_and_alignment_of_constant_size_exp (exp, out size, out align);
    if (rc < 0)
      fatal_compiler_error0 ("generate_size_table_for_open_struct(1) : struct is too large");

    _unused align;

    if (size > 2147483647 - 8)    // 8 bytes for aligned header of heap object
      fatal_compiler_error0 ("generate_size_table_for_open_struct(2) : struct is too large");

    store_integer (p, (uint)i*4, (uint)size, 4);
  }

  free_exp (exp);
}

/*****************************************************************************/

void compute_header_size_of_open_struct (PENTITY struct_type)
{
  FIELD_DATA data;
  PENTITY    field;

  if (struct_type^.the_struct_type.header_size > 0)   // already done
    return;

  begin_struct (out data, struct_type);

  field = struct_type^.the_struct_type.fields^.entities.first;
  while (field != null)
  {
    if (field^.kind == A_FIELD || field^.kind == A_VARYING_FIELD)
    {
      begin_field (ref data, field);
      end_field (ref data);
    }

    field = field^.next;
  }

  end_struct (ref data);

  struct_type^.the_struct_type.header_size = max (4, data.struct_alignment);
}

/*****************************************************************************/

// returns 4 or 8 bytes

int access_object_header_size_for_open_type (PENTITY open_type)
{
  PENTITY base_type;

  // compute header size, for array and for struct

  base_type = complete_type_of (open_type);

  if (base_type^.kind == AN_OPEN_ARRAY_TYPE)
  {
    int rc, size, align;
    rc = size_and_alignment_of_type (base_type^.the_open_array_type.element,
                                     out size, out align);
    if (rc < 0)
      fatal_compiler_error0 ("header_size_of_open_access_type()");

    _unused size;

    return max (4, align);
  }
  else    // an open struct type
  {
    compute_header_size_of_open_struct (base_type);
    return base_type^.the_struct_type.header_size;
  }
}

/*****************************************************************************/

void convert_runtime_constraint_into_size_constraint (ref EXP_OUTPUT output)
{
  PENTITY  base_type;
  int      rc, element_size, align;

  base_type = complete_type_of (output.base_type);

  if (base_type^.kind == AN_OPEN_ARRAY_TYPE)
  {
    // for array, multiply by element size.
    rc = size_and_alignment_of_type (base_type^.the_open_array_type.element, out element_size, out align);
    if (rc < 0)
      fatal_compiler_error0 ("convert_runtime_constraint_into_size_constraint(1)");

    _unused align;

    if (element_size != 1)
    {
      flush_constraint (ref output);

      put_code (P_ARRAY_SIZE);
      put_int4 (element_size);
    }
  }
  else if (base_type^.kind == A_STRUCT_TYPE) // for open struct type, select element from size table
  {
    generate_size_table_for_open_struct (base_type);  // check if size table has been generated before

    flush_constraint (ref output);

    put_code (P_LOAD_CONST);
    put_int8 (serial_nr_of_pool_cte (base_type^.the_struct_type.size_table));

    if (array_checks_enabled)
    {
      // push length of size table
      put_code (P_CTE_4);
      put_int4 ((int)base_type^.the_struct_type.discriminant^.entities.first
                              ^.the_field.type
                              ^.the_enumeration_type.last + 1);

      put_code (P_CHECK_INDEX);
      put_int4 (get_new_near_label_nr());
    }

    put_code (P_ADD_INDEX);
    put_int4 (4);

    put_code (P_VALUE_4);
  }
  else
  {
    fatal_compiler_error0 ("convert_runtime_constraint_into_size_constraint(2)");
  }
}

/*****************************************************************************/

// convert constraint into expression and discard any unused address
// we must not return an address here because the node denotes a value !

void convert_runtime_constraint_into_exp (ref EXP_OUTPUT input, out EXP_OUTPUT output)
{
  switch (input.constraint.place)
  {
    case CT_TEMP:             // constraint is uint4 local variable at .temp_offset[EBP]
      discard_address (ref input);       // discard any address that will never be used

      put_code (P_LOAD_LOCAL);
      put_int4 (input.constraint.temp_offset);
      put_code (P_VALUE_4);

      break;

    case CT_ON_INT_STACK:     // constraint is uint4 on int_stack
      discard_address (ref input);  // discard any address that will never be used
      break;

    case CT_AT_ADDR_OFFSET:   // constraint is uint4 at addr_stack + .offset
      {
        int4 offset;

        offset = input.constraint.offset;

        if (offset != 0)
        {
          put_code (P_ADD_OFFSET);
          put_int4 (offset);
        }

        put_code (P_VALUE_4);
      }
      break;

    default:
      fatal_compiler_error0 ("convert_runtime_constraint_into_exp()");
      break;
  }

  clear output;
  output.exp.kind = EXP_ON_INT_STACK_I4;
  output.exp.sign = _DONTCARE;
  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

void flush_simple_value (ref EXP_OUTPUT  output,
                             PEXPRESSION exp,
                             PENTITY     target_type);

/*****************************************************************************/

void convert_exp_into_constraint (ref EXP_OUTPUT  input,
                                      PEXPRESSION exp,
                                  ref EXP_OUTPUT  output)
{
  switch (input.exp.kind)
  {
    case EXP_CONST_INT:
      output.constraint.place = CT_CTE;
      output.constraint.cte   = (uint4)input.exp.value;
      break;

    case EXP_ON_INT_STACK_I4:
      output.constraint.place = CT_ON_INT_STACK;
      break;

    case EXP_AT_ADDR_OFFSET:      // at addr_stack + offset, there's the object
    case EXP_AT_ADDR_INDIRECT:    // at addr_stack, there's a pointer to the object
      flush_simple_value (ref input, exp, type_int);
      output.constraint.place = CT_ON_INT_STACK;
      break;

    default:
      fatal_compiler_error0 ("convert_exp_into_constraint()");
      break;
  }
}

/*****************************************************************************/

void convert_addr_into_simple_value (ref EXP_OUTPUT  output,
                                         PEXPRESSION exp)
{
  PENTITY type;

  if (exp^.form == A_VALUE)   // it's a pointer, leave address on addr_stack
    return;

  // determine the simple type and load on xx_stack

  // compute size of simple type
  type = complete_type_of (exp^.base_type_or_null);

  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_enumeration_type.base].size)
      {
        case 1:
          if (type == type_bool)
          {
            put_code (P_VALUE_BOOL);
            output.exp.kind = EXP_ON_INT_STACK_B;
          }
          else
          {
            put_code (P_VALUE_U1);
            output.exp.kind = EXP_ON_INT_STACK_I4;
            output.exp.sign = _DONTCARE;
          }
          break;

        case 2:
          put_code (P_VALUE_U2);
          output.exp.kind = EXP_ON_INT_STACK_I4;
          output.exp.sign = _DONTCARE;
          break;

        case 4:
          put_code (P_VALUE_4);
          output.exp.kind = EXP_ON_INT_STACK_I4;
          output.exp.sign = _UNSIGNED;
          break;

        default:
          fatal_compiler_error0 ("convert_addr_into_simple_value(1)");
          break;
      }
      break;

    case AN_INTEGER_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_integer_type.type].size)
      {
        case 1:
          if (INTEGER_DATA[(uint)type^.the_integer_type.type].is_signed)
          {
            put_code (P_VALUE_I1);
            output.exp.sign = _SIGNED;
          }
          else
          {
            put_code (P_VALUE_U1);
            output.exp.sign = _DONTCARE;
          }
          output.exp.kind = EXP_ON_INT_STACK_I4;
          break;

        case 2:
          if (INTEGER_DATA[(uint)type^.the_integer_type.type].is_signed)
          {
            put_code (P_VALUE_I2);
            output.exp.sign = _SIGNED;
          }
          else
          {
            put_code (P_VALUE_U2);
            output.exp.sign = _DONTCARE;
          }
          output.exp.kind = EXP_ON_INT_STACK_I4;
          break;

        case 4:
          if (INTEGER_DATA[(uint)type^.the_integer_type.type].is_signed)
          {
            put_code (P_VALUE_4);
            output.exp.sign = _SIGNED;
          }
          else
          {
            put_code (P_VALUE_4);
            output.exp.sign = _UNSIGNED;
          }
          output.exp.kind = EXP_ON_INT_STACK_I4;
          break;

        case 8:
          put_code (P_VALUE_8);
          output.exp.kind = EXP_ON_INT_STACK_I8;
          break;

        default:
          fatal_compiler_error0 ("convert_addr_into_simple_value(2)");
          break;
      }
      break;

    case A_FLOAT_TYPE:
      switch (FLOAT_DATA[(uint)type^.the_float_type.type].size)
      {
        case 4:
          put_code (P_VALUE_FLT4);
          output.exp.kind = EXP_ON_FLOAT_STACK_F4;
          break;

        case 8:
          put_code (P_VALUE_FLT8);
          output.exp.kind = EXP_ON_FLOAT_STACK_F8;
          break;

        default:
          fatal_compiler_error0 ("convert_addr_into_simple_value(3)");
          break;
      }
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      put_code (P_VALUE_ADDR);
      break;

    default:
      fatal_compiler_error0 ("convert_addr_into_simple_value(4)");
      break;
  }
}

/*****************************************************************************/

// can only be used after flushing a simple value.

void sync_simple_value_in_register (EXP_OUTPUT output)
{
  PCODE code;

  switch (output.exp.kind)
  {
    case EXP_ON_INT_STACK_B:
      code = P_SYNC_BOOL;
      break;

    case EXP_ON_INT_STACK_I4:
      code = P_SYNC_4;
      break;

    case EXP_ON_INT_STACK_I8:
      code = P_SYNC_8;
      break;

    case EXP_ON_FLOAT_STACK_F4:
      code = P_SYNC_FLT4;
      break;

    case EXP_ON_FLOAT_STACK_F8:
      code = P_SYNC_FLT8;
      break;

    case EXP_AT_ADDR_OFFSET:
      code = P_SYNC_ADDR;
      break;

    default:
      fatal_compiler_error0 ("sync_simple_value_in_register()");
      code = P_SYNC_BOOL;
      break;
  }

  put_code (code);
}

/*****************************************************************************/

// can only be used after flushing a simple value.

void correct_stack_after_sync_simple_value_in_register (EXP_OUTPUT output)
{
  int i, f, a;

  i = 0;
  f = 0;
  a = 0;

  switch (output.exp.kind)
  {
    case EXP_ON_INT_STACK_B:
      i = 1;
      break;

    case EXP_ON_INT_STACK_I4:
      i = 1;
      break;

    case EXP_ON_INT_STACK_I8:
      i = 1;
      break;

    case EXP_ON_FLOAT_STACK_F4:
      f = 1;
      break;

    case EXP_ON_FLOAT_STACK_F8:
      f = 1;
      break;

    case EXP_AT_ADDR_OFFSET:
      a = 1;
      break;

    default:
      fatal_compiler_error0 ("correct_stack_after_sync_simple_value_in_register()");
      break;
  }

  put_code (P_SYNC_STACKS);
  put_int4 (i);
  put_int4 (f);
  put_int4 (a);
  sync_stacks (i,f,a);
}

/*****************************************************************************/

// can only be used after flushing a simple value.

void force_simple_value_in_register (EXP_OUTPUT output)
{
  PCODE code;

  switch (output.exp.kind)
  {
    case EXP_ON_INT_STACK_B:
      code = P_FORCE_BOOL;
      break;

    case EXP_ON_INT_STACK_I4:
      code = P_FORCE_4;
      break;

    case EXP_ON_INT_STACK_I8:
      code = P_FORCE_8;
      break;

    case EXP_ON_FLOAT_STACK_F4:
      code = P_FORCE_FLT4;
      break;

    case EXP_ON_FLOAT_STACK_F8:
      code = P_FORCE_FLT8;
      break;

    case EXP_AT_ADDR_OFFSET:
      code = P_FORCE_ADDR;
      break;

    default:
      fatal_compiler_error0 ("force_simple_value_in_register()");
      code = P_FORCE_BOOL;
      break;
  }

  put_code (code);
}

/*****************************************************************************/

// can only be used after flushing a simple value.

void force_simple_value_of_heap_object_in_register (EXP_INPUT  input,
                                                    EXP_OUTPUT output)
{
  if (input.tomb.anchor_provided != _NO)
    force_simple_value_in_register (output);
}

/*****************************************************************************/

SIGN sign_of_type (PENTITY type)
{
  PENTITY t;

  t = complete_type_of (type);

  if (t^.kind == AN_ENUMERATION_TYPE)
  {
    return (INTEGER_DATA[(uint)t^.the_enumeration_type.base].size < 4) ? _DONTCARE : _UNSIGNED;
  }
  else if (t^.kind == AN_INTEGER_TYPE)
  {
    return (INTEGER_DATA[(uint)t^.the_integer_type.type].is_signed) ? _SIGNED : _UNSIGNED;
  }
  else
  {
    return _DONTCARE;
  }
}

/*****************************************************************************/

// set sign of int4/uint4 result according to result type

void set_result_sign (ref EXP_OUTPUT  output,
                          PENTITY     target_type)
{
  if (output.exp.kind == EXP_ON_INT_STACK_I4)     // we must set the result sign
  {
    output.exp.sign = sign_of_type (target_type);
  }
}

/*****************************************************************************/

package CONVERSION_DATA

  struct CONVERSION_INFO
  {
    EXP_KIND fromt;
    SIGN     from_sign;
    EXP_KIND to;
    SIGN     to_sign;
    PCODE    pcode;
  }

  const int MAX_CONV = (17);

  const CONVERSION_INFO CONV_TABLE[MAX_CONV] =
    {{EXP_ON_INT_STACK_I4,   _UNSIGNED, EXP_ON_INT_STACK_I8,   _DONTCARE, P_CONV_UINT_LONG},
     {EXP_ON_INT_STACK_I4,   _SIGNED,   EXP_ON_INT_STACK_I8,   _DONTCARE, P_CONV_INT_LONG},
     {EXP_ON_INT_STACK_I4,   _UNSIGNED, EXP_ON_FLOAT_STACK_F4, _DONTCARE, P_CONV_UINT_FLT4},
     {EXP_ON_INT_STACK_I4,   _SIGNED,   EXP_ON_FLOAT_STACK_F4, _DONTCARE, P_CONV_INT_FLT4},
     {EXP_ON_INT_STACK_I4,   _UNSIGNED, EXP_ON_FLOAT_STACK_F8, _DONTCARE, P_CONV_UINT_FLT8},
     {EXP_ON_INT_STACK_I4,   _SIGNED,   EXP_ON_FLOAT_STACK_F8, _DONTCARE, P_CONV_INT_FLT8},

     {EXP_ON_INT_STACK_I8,   _DONTCARE, EXP_ON_INT_STACK_I4,   _DONTCARE, P_CONV_LONG_INT},
     {EXP_ON_INT_STACK_I8,   _DONTCARE, EXP_ON_FLOAT_STACK_F4, _DONTCARE, P_CONV_LONG_FLT4},
     {EXP_ON_INT_STACK_I8,   _DONTCARE, EXP_ON_FLOAT_STACK_F8, _DONTCARE, P_CONV_LONG_FLT8},

     {EXP_ON_FLOAT_STACK_F4, _DONTCARE, EXP_ON_INT_STACK_I4,   _SIGNED,   P_CONV_FLT4_INT},  // signed first
     {EXP_ON_FLOAT_STACK_F4, _DONTCARE, EXP_ON_INT_STACK_I4,   _UNSIGNED, P_CONV_FLT4_UINT},
     {EXP_ON_FLOAT_STACK_F4, _DONTCARE, EXP_ON_INT_STACK_I8,   _DONTCARE, P_CONV_FLT4_LONG},
     {EXP_ON_FLOAT_STACK_F4, _DONTCARE, EXP_ON_FLOAT_STACK_F8, _DONTCARE, P_CONV_FLT4_FLT8},

     {EXP_ON_FLOAT_STACK_F8, _DONTCARE, EXP_ON_INT_STACK_I4,   _SIGNED,   P_CONV_FLT8_INT},  // signed first
     {EXP_ON_FLOAT_STACK_F8, _DONTCARE, EXP_ON_INT_STACK_I4,   _UNSIGNED, P_CONV_FLT8_UINT},
     {EXP_ON_FLOAT_STACK_F8, _DONTCARE, EXP_ON_INT_STACK_I8,   _DONTCARE, P_CONV_FLT8_LONG},
     {EXP_ON_FLOAT_STACK_F8, _DONTCARE, EXP_ON_FLOAT_STACK_F4, _DONTCARE, P_CONV_FLT8_FLT4}};

end CONVERSION_DATA;

/*****************************************************************************/

// assertion: value has been flushed

void convert_to_simple_target_type (ref EXP_OUTPUT  output,
                                        PEXPRESSION exp,
                                        PENTITY     target_type)
{
  EXP_KIND kind1, kind2;
  int      size2, rc, align, i;
  PENTITY  type2;
  bool     final_convert_to_bool;
  SIGN     sign1, sign2;


  // compute kind1 (source)

  kind1 = output.exp.kind;
  if (kind1 == EXP_ON_INT_STACK_B ||
      kind1 == EXP_ON_INT_STACK_I4 || kind1 == EXP_ON_FLOAT_STACK_F4 ||
      kind1 == EXP_ON_INT_STACK_I8 || kind1 == EXP_ON_FLOAT_STACK_F8)
  {
    // ok
  }
  else
  {
    return;    // don't convert pointer types
  }


  // compute kind2 (target)

  type2 = complete_type_of (target_type);
  if (type2 == type_bool)
  {
    size2 = 1;
    kind2 = EXP_ON_INT_STACK_B;
  }
  else
  {
    rc = size_and_alignment_of_type (type2, out size2, out align);
    if (rc != 0)
      fatal_compiler_error0 ("convert_to_simple_target_type(1)");

    _unused align;

    if (size2 <= 4)
    {
      kind2 = (type2^.kind == A_FLOAT_TYPE) ? EXP_ON_FLOAT_STACK_F4 : EXP_ON_INT_STACK_I4;
    }
    else
    {
      kind2 = (type2^.kind == A_FLOAT_TYPE) ? EXP_ON_FLOAT_STACK_F8 : EXP_ON_INT_STACK_I8;
    }
  }

  output.exp.kind = kind2;

  if (kind1 == EXP_ON_INT_STACK_I4)
    sign1 = output.exp.sign;
  else
    sign1 = _DONTCARE;

  sign2 = sign_of_type (target_type);


  if (kind1 == EXP_ON_INT_STACK_B && kind2 != EXP_ON_INT_STACK_B)  // extend from bool to int
  {
    put_code (P_CONV_BOOL_INT);
    kind1 = EXP_ON_INT_STACK_I4;
    sign1 = _DONTCARE;
  }
  else if (exp^.kind == A_FUNCTION_CALL && kind1 == EXP_ON_INT_STACK_I4)
  {
    int size1;

    rc = size_and_alignment_of_type (exp^.base_type_or_null, out size1, out align);    // function return type
    if (rc != 0)
      fatal_compiler_error0 ("convert_to_simple_target_type(2)");

    if (size1 <= 2 && size2 > size1)  // function return type is a 1 or 2 byte type and the target type is larger
    {
      SIGN sign = sign_of_type (exp^.base_type_or_null);    // sign of function return type
      put_code (P_EXTEND);
      if (size1 == 1)
        put_byte ((byte)(sign != _SIGNED ? 0 : 2));  // 0=extend uint1 to uint4, 2=extend int1 to int4
      else if (size1 == 2)
        put_byte ((byte)(sign != _SIGNED ? 1 : 3));  // 1=extend uint2 to uint4, 3=extend int2 to int4.
      else
        fatal_compiler_error0 ("convert_to_simple_target_type(3)");
    }
  }
  else if (exp^.kind == AN_OPERATOR_VALUE && exp^.operator_value_info.op == OP_CONVERT_INT_INT)
  {
    // if we flush a cast, we need to extend the value in case the cast type is a 1-or-2-byte type and the target type is larger.
    PENTITY arg_type = exp^.operator_value_info.arg[0]^.base_type_or_null;   // type of argument inside cast
    int     size0, size1;

    rc = size_and_alignment_of_type (exp^.base_type_or_null, out size1, out align);    // cast type
    if (rc != 0)
      fatal_compiler_error0 ("convert_to_simple_target_type(4)");

    if (size1 <= 2 && size2 > size1)    // cast type is a 1 or 2 byte type and the target type is larger : extend after cast
    {
      SIGN sign = sign_of_type (exp^.base_type_or_null);    // sign of cast type

      rc = size_and_alignment_of_type (arg_type, out size0, out align);  // type of value inside cast
      if (rc != 0)
        fatal_compiler_error0 ("convert_to_simple_target_type(5)");

      // cast type is smaller than argument type, or equal and having different sign.
      if (size1 < size0 || (size1 == size0 &&
                            sign                    != sign_of_type(arg_type) &&
                            sign                    != _DONTCARE &&
                            sign_of_type (arg_type) != _DONTCARE))
      {
        put_code (P_EXTEND);
        if (size1 == 1)
          put_byte ((byte)(sign != _SIGNED ? 0 : 2));  // uint1, int1
        else if (size1 == 2)
          put_byte ((byte)(sign != _SIGNED ? 1 : 3));  // uint2, int2
        else
          fatal_compiler_error0 ("convert_to_simple_target_type(6)");
      }
    }
  }

  // shrink from int to bool

  final_convert_to_bool = false;
  if (kind1 != EXP_ON_INT_STACK_B && kind2 == EXP_ON_INT_STACK_B)
  {
    kind2 = EXP_ON_INT_STACK_I4;
    sign2 = _DONTCARE;
    final_convert_to_bool = true;
  }


  // we will use first matching line in table, if no line matches it's a conversion to itself.

  for (i=0; i<MAX_CONV; i++)
  {
    if (CONV_TABLE[i].fromt == kind1 && CONV_TABLE[i].to == kind2)
    {
      if ((kind1 != EXP_ON_INT_STACK_I4 ||
           sign1 == _DONTCARE ||
           CONV_TABLE[i].from_sign == _DONTCARE ||
           sign1 == CONV_TABLE[i].from_sign)
       && (kind2 != EXP_ON_INT_STACK_I4 ||
           sign2 == _DONTCARE ||
           CONV_TABLE[i].to_sign == _DONTCARE ||
           sign2 == CONV_TABLE[i].to_sign))
      {
        put_code (CONV_TABLE[i].pcode);
        break;
      }
    }
  }

  if (final_convert_to_bool)
  {
    put_code (P_CONV_INT_BOOL);
  }
  else if (exp^.kind == AN_OPERATOR_VALUE && exp^.operator_value_info.op == OP_CONVERT_FLOAT_INT && kind2 == EXP_ON_INT_STACK_I4)
  {
    // example:  uint u = (int2)f;
    int size1;
    rc = size_and_alignment_of_type (exp^.base_type_or_null, out size1, out align);    // cast type
    if (rc != 0)
      fatal_compiler_error0 ("convert_to_simple_target_type(7)");

    if (size1 <= 2 && size2 > size1)    // cast type is a 1 or 2 byte type and the target type is larger
    {
      SIGN sign = sign_of_type (exp^.base_type_or_null);    // sign of cast type
      put_code (P_EXTEND);
      if (size1 == 1)
        put_byte ((byte)(sign != _SIGNED ? 0 : 2));  // uint1, int1
      else if (size1 == 2)
        put_byte ((byte)(sign != _SIGNED ? 1 : 3));  // uint2, int2
      else
        fatal_compiler_error0 ("convert_to_simple_target_type(8)");
    }
  }

  set_result_sign (ref output, target_type);
}

/*****************************************************************************/

// flush simple value
// (bool, int/uint, long, float, double, pointer) on int_/float_/addr_stack

void flush_simple_value (ref EXP_OUTPUT  output,
                             PEXPRESSION exp,
                             PENTITY     target_type)
{
  switch (output.exp.kind)
  {
    case EXP_CONST_INT:   // can be enumeration or integer type
      {
        PENTITY type;
        int     rc, size, align;

        type = complete_type_of (target_type);
        rc = size_and_alignment_of_type (type, out size, out align);
        if (rc != 0)
          fatal_compiler_error0 ("flush_simple_value(1)");

        _unused align;

        if (type == type_bool)
        {
          put_code (P_CTE_BOOL);
          put_byte ((byte)output.exp.value);

          output.exp.kind = EXP_ON_INT_STACK_B;
        }
        else if (size <= 4)
        {
          if (type^.kind == A_FLOAT_TYPE)
          {
            put_code (P_CTE_FLT4);
            put_float ((float)output.exp.value);

            output.exp.kind = EXP_ON_FLOAT_STACK_F4;
          }
          else
          {
            put_code (P_CTE_4);
            put_int4 ((int4)output.exp.value);

            output.exp.kind = EXP_ON_INT_STACK_I4;

            if (output.exp.value < 0)
              output.exp.sign = _SIGNED;
            else if (output.exp.value > 2147483647)
              output.exp.sign = _UNSIGNED;
            else
              output.exp.sign = _DONTCARE;
          }
        }
        else
        {
          if (type^.kind == A_FLOAT_TYPE)
          {
            put_code (P_CTE_FLT8);
            put_double (output.exp.fvalue);

            output.exp.kind = EXP_ON_FLOAT_STACK_F8;
          }
          else
          {
            put_code (P_CTE_8);
            put_int8 (output.exp.value);

            output.exp.kind = EXP_ON_INT_STACK_I8;
          }
        }
      }
      break;

    case EXP_CONST_FLOAT:
      {
        PENTITY type;
        int     rc, size, align;

        type = complete_type_of (target_type);
        rc = size_and_alignment_of_type (type, out size, out align);
        if (rc != 0)
          fatal_compiler_error0 ("flush_simple_value(2)");

        _unused align;

        if (type == type_bool)
        {
          put_code (P_CTE_BOOL);
          put_byte ((byte)output.exp.fvalue);

          output.exp.kind = EXP_ON_INT_STACK_B;
        }
        else if (size <= 4)
        {
          if (type^.kind == A_FLOAT_TYPE)
          {
            put_code (P_CTE_FLT4);
            put_float ((float)output.exp.fvalue);

            output.exp.kind = EXP_ON_FLOAT_STACK_F4;
          }
          else
          {
            put_code (P_CTE_4);
            put_int4 ((int4)output.exp.fvalue);

            output.exp.kind = EXP_ON_INT_STACK_I4;

            if (output.exp.fvalue < 0.0)
              output.exp.sign = _SIGNED;
            else if (output.exp.fvalue > 2147483647.0)
              output.exp.sign = _UNSIGNED;
            else
              output.exp.sign = _DONTCARE;
          }
        }
        else
        {
          if (type^.kind == A_FLOAT_TYPE)
          {
            put_code (P_CTE_FLT8);
            put_double (output.exp.fvalue);

            output.exp.kind = EXP_ON_FLOAT_STACK_F8;
          }
          else
          {
            put_code (P_CTE_8);
            put_int8 ((int8)output.exp.fvalue);

            output.exp.kind = EXP_ON_INT_STACK_I8;
          }
        }
      }
      break;

    case EXP_CONST_NULL:
      put_code (P_CTE_NULL);
      output.exp.kind   = EXP_AT_ADDR_OFFSET;
      output.exp.offset = 0;
      output.constraint.place = CT_NONE;
      break;

    case EXP_ON_INT_STACK_B:
    case EXP_ON_INT_STACK_I4:
    case EXP_ON_INT_STACK_I8:
    case EXP_ON_FLOAT_STACK_F4:
    case EXP_ON_FLOAT_STACK_F8:
      output.constraint.place = CT_NONE;
      convert_to_simple_target_type (ref output, exp, target_type);
      break;

    case EXP_AT_ADDR_OFFSET:
      if (output.exp.offset != 0)
      {
        put_code (P_ADD_OFFSET);
        put_int4 (output.exp.offset);
        output.exp.offset = 0;
      }
      convert_addr_into_simple_value (ref output, exp);
      output.constraint.place = CT_NONE;
      convert_to_simple_target_type (ref output, exp, target_type);
      break;

    case EXP_AT_ADDR_INDIRECT:
      put_code (P_VALUE_ADDR);
      convert_addr_into_simple_value (ref output, exp);
      output.constraint.place = CT_NONE;
      convert_to_simple_target_type (ref output, exp, target_type);
      break;

    default:
      fatal_compiler_error0 ("flush_simple_value(9)");
      break;
  }
}

/*****************************************************************************/

void store_simple_value (PENTITY type)
{
  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_enumeration_type.base].size)
      {
        case 1:
          if (type == type_bool)
            put_code (P_STORE_BOOL);
          else
            put_code (P_STORE_1);
          break;

        case 2:
          put_code (P_STORE_2);
          break;

        case 4:
          put_code (P_STORE_4);
          break;

        default:
          fatal_compiler_error0 ("store_simple_value(1)");
          break;
      }
      break;

    case AN_INTEGER_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_integer_type.type].size)
      {
        case 1:
          put_code (P_STORE_1);
          break;

        case 2:
          put_code (P_STORE_2);
          break;

        case 4:
          put_code (P_STORE_4);
          break;

        case 8:
          put_code (P_STORE_8);
          break;

        default:
          fatal_compiler_error0 ("store_simple_value(2)");
          break;
      }
      break;

    case A_FLOAT_TYPE:
      switch (FLOAT_DATA[(uint)type^.the_float_type.type].size)
      {
        case 4:
          put_code (P_STORE_FLT4);
          break;

        case 8:
          put_code (P_STORE_FLT8);
          break;

        default:
          fatal_compiler_error0 ("store_simple_value(3)");
          break;
      }
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      put_code (P_STORE_ADDR);
      break;

    default:
      fatal_compiler_error0 ("store_simple_value(4)");
      break;
  }
}

/*****************************************************************************/

void store_simple_value_in_field (PENTITY type, int4 offset)
{
  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_enumeration_type.base].size)
      {
        case 1:
          if (type == type_bool)
            put_code (P_STORE_FIELD_BOOL);
          else
            put_code (P_STORE_FIELD_1);
          break;

        case 2:
          put_code (P_STORE_FIELD_2);
          break;

        case 4:
          put_code (P_STORE_FIELD_4);
          break;

        default:
          fatal_compiler_error0 ("store_simple_value_in_field(1)");
          break;
      }
      break;

    case AN_INTEGER_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_integer_type.type].size)
      {
        case 1:
          put_code (P_STORE_FIELD_1);
          break;

        case 2:
          put_code (P_STORE_FIELD_2);
          break;

        case 4:
          put_code (P_STORE_FIELD_4);
          break;

        case 8:
          put_code (P_STORE_FIELD_8);
          break;

        default:
          fatal_compiler_error0 ("store_simple_value_in_field(2)");
          break;
      }
      break;

    case A_FLOAT_TYPE:
      switch (FLOAT_DATA[(uint)type^.the_float_type.type].size)
      {
        case 4:
          put_code (P_STORE_FIELD_FLT4);
          break;

        case 8:
          put_code (P_STORE_FIELD_FLT8);
          break;

        default:
          fatal_compiler_error0 ("store_simple_value_in_field(3)");
          break;
      }
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      put_code (P_STORE_FIELD_ADDR);
      break;

    default:
      fatal_compiler_error0 ("store_simple_value_in_field(4)");
      break;
  }

  put_int4 (offset);
}

/*****************************************************************************/

// used for 64bit for generating table of first 4 parameters to load in RCX, RDX, R8, R9.
// returns one of b (bool), i (int4), l (int8), f (float4), d (float8), a (address)

public
char code_datatype (PENTITY type)
{
  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
      return (type == type_bool) ? 'b' : 'i';

    case AN_INTEGER_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_integer_type.type].size)
      {
        case 1:
        case 2:
        case 4:
          return 'i';

        case 8:
          return 'l';

        default:
          break;
      }
      break;

    case A_FLOAT_TYPE:
      switch (FLOAT_DATA[(uint)type^.the_float_type.type].size)
      {
        case 4:
          return 'f';

        case 8:
          return 'd';

        default:
          break;
      }
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      return 'a';

    default:
      break;
  }

  fatal_compiler_error0 ("code_datatype(1)");
  return ' ';
}

/*****************************************************************************/

// returns size of argument pushed.

int push_simple_value (PENTITY type)
{
  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
      if (type == type_bool)
      {
        if (goptions.g_target == INTEL)
          put_code (P_PUSH_BOOL);
        else if (goptions.g_target == ANDROID)
          put_code (P_STORE_PARAM_BOOL);
        else
          abort;
        return 1;
      }
      else
      {
        if (goptions.g_target == INTEL)
          put_code (P_PUSH_4);
        else if (goptions.g_target == ANDROID)
          put_code (P_STORE_PARAM_INT4);
        else
          abort;
        return 4;
      }
      break;

    case AN_INTEGER_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_integer_type.type].size)
      {
        case 1:
        case 2:
        case 4:
          if (goptions.g_target == INTEL)
            put_code (P_PUSH_4);
          else if (goptions.g_target == ANDROID)
            put_code (P_STORE_PARAM_INT4);
          else
            abort;
          return 4;

        case 8:
          if (goptions.g_target == INTEL)
            put_code (P_PUSH_8);
          else if (goptions.g_target == ANDROID)
            put_code (P_STORE_PARAM_INT8);
          else
            abort;
          return 8;

        default:
          break;
      }
      break;

    case A_FLOAT_TYPE:
      switch (FLOAT_DATA[(uint)type^.the_float_type.type].size)
      {
        case 4:
          if (goptions.g_target == INTEL)
            put_code (P_PUSH_FLT4);
          else if (goptions.g_target == ANDROID)
            put_code (P_STORE_PARAM_FLT4);
          else
            abort;
          return 4;

        case 8:
          if (goptions.g_target == INTEL)
            put_code (P_PUSH_FLT8);
          else if (goptions.g_target == ANDROID)
            put_code (P_STORE_PARAM_FLT8);
          else
            abort;
          return 8;

        default:
          break;
      }
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      if (goptions.g_target == INTEL)
        put_code (P_PUSH_ADDR);
      else if (goptions.g_target == ANDROID)
        put_code (P_STORE_PARAM_ADDR);
      else
        abort;
      return address_size;

    default:
      break;
  }

  fatal_compiler_error0 ("push_simple_value(1)");
  return 0;
}

/*****************************************************************************/

void bool_operator2 (    PCODE       p_code,
                         PEXPRESSION e,
                     ref FRAME_INFO  frame,
                     ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;

  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, type_bool);

  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, type_bool);

  put_code (p_code);

  release_tombstone_anchor (left_in);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = EXP_ON_INT_STACK_B;
  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

void add_sub_bool_int (    PCODE       p_code,
                           PEXPRESSION e,
                       ref FRAME_INFO  frame,
                       ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;

  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, type_int);

  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, type_int);

  put_code (p_code);   // add/sub int4
  put_code (P_CONV_INT_BOOL);

  release_tombstone_anchor (left_in);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = EXP_ON_INT_STACK_B;
  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

void bool_operator1 (    PCODE       p_code,
                         PEXPRESSION e,
                     ref FRAME_INFO  frame,
                     ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in;
  EXP_OUTPUT  left_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;

  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear left_in;
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, type_bool);

  put_code (p_code);

  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = EXP_ON_INT_STACK_B;
  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

void operator2 (    PCODE       p_code_int4,
                    PCODE       p_code_int8,  // or 0
                    PEXPRESSION e,
                ref FRAME_INFO  frame,
                ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;

  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, e^.base_type_or_null);

  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, e^.base_type_or_null);


  if (left_out.exp.kind == EXP_ON_INT_STACK_I4 || left_out.exp.kind == EXP_ON_FLOAT_STACK_F4)
    put_code (p_code_int4);
  else if (left_out.exp.kind == EXP_ON_INT_STACK_I8 || left_out.exp.kind == EXP_ON_FLOAT_STACK_F8)
  {
    if (p_code_int8 == (PCODE)0)
      fatal_compiler_error0 ("operator2(1)");
    put_code (p_code_int8);

    if (p_code_int8 == P_ASL8 || p_code_int8 == P_ASR8)
      put_int4 (get_new_near_label_nr());
  }
  else
    fatal_compiler_error0 ("operator2(2)");

  if (left_out.exp.kind != right_out.exp.kind)
    fatal_compiler_error0 ("operator2(3)");

  release_tombstone_anchor (left_in);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = left_out.exp.kind;
  set_result_sign (ref output, e^.base_type_or_null);

  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

// unsafe_ptr +/- integer -> unsafe_ptr  ; add integer*sizeof(elem) to unsafe_ptr

void operator_add_sub_ptr_int (    PCODE       p_code,
                                   PEXPRESSION e,
                               ref FRAME_INFO  frame,
                               ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  int         rc, size, align;
  PENTITY     element_type;
  PEXPRESSION exp;
  int8        saved_frame_offset;

  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, e^.base_type_or_null);

  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, type_int);


  // compute size of designated type as an int4
  element_type = e^.base_type_or_null^.the_unsafe_pointer_type.designated_type;

  rc = size_and_alignment_of_type (element_type, out size, out align);
  if (rc != 0)
    fatal_compiler_error0 ("operator_add_sub_ptr_int()");

  _unused align;

  put_code (p_code);
  put_int4 (size);

  release_tombstone_anchor (left_in);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = left_out.exp.kind;
  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

// subtraction of unsafe_ptrs

void operator_sub_ptr_ptr (    PEXPRESSION e,
                           ref FRAME_INFO  frame,
                           ref EXP_OUTPUT  output)
{
  EXP_INPUT  left_in, right_in;
  EXP_OUTPUT left_out, right_out;
  int        rc, size, align;
  PENTITY    element_type;
  PEXPRESSION exp;
  int8        saved_frame_offset;

  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  // compute size of designated type as an int4
  element_type = exp^.base_type_or_null^.the_unsafe_pointer_type.designated_type;

  rc = size_and_alignment_of_type (element_type, out size, out align);
  if (rc != 0)
    fatal_compiler_error0 ("operator_sub_ptr_ptr()");

  _unused align;


  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, exp^.base_type_or_null);


  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, exp^.base_type_or_null);

  put_code (P_SUB_PTRS);

  if (size != 1)   // divide by element size
  {
    put_code (P_CTE_4);
    put_int4 (size);

    put_code (P_UDIV4);
  }

  release_tombstone_anchor (left_in);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = EXP_ON_INT_STACK_I4;
  output.exp.sign = _UNSIGNED;
  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

void set_bool_result (ref EXP_OUTPUT output)
{
  put_code (P_SETBOOL);

  output.exp.kind = EXP_ON_INT_STACK_B;
  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

// same as operator2, but result type is taken from promoted type, not from result type
// and result type is bool.
// always followed by P_SETBOOL, P_BTRUE or P_BFALSE.

void cmp_operator2 (    PCODE       p_code_int4,
                        PCODE       p_code_int8,  // or 0
                        PEXPRESSION e,
                    ref FRAME_INFO  frame)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, e^.operator_value_info.type);
  force_simple_value_of_heap_object_in_register (left_in, left_out);
  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, e^.operator_value_info.type);
  force_simple_value_of_heap_object_in_register (right_in, right_out);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  if (left_out.exp.kind == EXP_ON_INT_STACK_I4 || left_out.exp.kind == EXP_ON_FLOAT_STACK_F4)
    put_code (p_code_int4);
  else if (left_out.exp.kind == EXP_ON_INT_STACK_I8 || left_out.exp.kind == EXP_ON_FLOAT_STACK_F8)
  {
    if (p_code_int8 == (PCODE)0)
      fatal_compiler_error0 ("cmp_operator2(1)");
    put_code (p_code_int8);
  }
  else
    fatal_compiler_error0 ("cmp_operator2(2)");

  put_byte ((byte)e^.operator_value_info.cmp);

  if (left_out.exp.kind != right_out.exp.kind)
    fatal_compiler_error0 ("cmp_operator2(3)");
}

/*****************************************************************************/

// always followed by P_SETBOOL, P_BTRUE or P_BFALSE.

void cmp_bool2 (    PEXPRESSION e,
                ref FRAME_INFO  frame)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, type_bool);
  force_simple_value_of_heap_object_in_register (left_in, left_out);
  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, type_bool);
  force_simple_value_of_heap_object_in_register (right_in, right_out);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset


  put_code (P_CMP_BOOL);
  put_byte ((byte)e^.operator_value_info.cmp);
}

/*****************************************************************************/

// always followed by P_SETBOOL, P_BTRUE or P_BFALSE.

void cmp_ptr2 (    PEXPRESSION e,
               ref FRAME_INFO  frame)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, exp^.base_type_or_null);
  force_simple_value_of_heap_object_in_register (left_in, left_out);
  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[1];

  clear (right_in);
  right_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref right_in, ref frame);
  generate_code_for_expression (exp, right_in, ref frame, out right_out);
  flush_simple_value (ref right_out, exp, exp^.base_type_or_null);
  force_simple_value_of_heap_object_in_register (right_in, right_out);
  release_tombstone_anchor (right_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset


  put_code (P_CMP_ADDR);
  put_byte ((byte)e^.operator_value_info.cmp);
}

/*****************************************************************************/

void operator_unary_minus (    PEXPRESSION e,
                           ref FRAME_INFO  frame,
                           ref EXP_OUTPUT  output)
{
  EXP_INPUT  left_in;
  EXP_OUTPUT left_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, e^.base_type_or_null);

  switch (left_out.exp.kind)
  {
    case EXP_ON_INT_STACK_I4:
      put_code (P_NEG4);
      break;

    case EXP_ON_INT_STACK_I8:
      put_code (P_NEG8);
      break;

    case EXP_ON_FLOAT_STACK_F4:
      put_code (P_NEG_FLT4);
      break;

    case EXP_ON_FLOAT_STACK_F8:
      put_code (P_NEG_FLT8);
      break;

    default:
      fatal_compiler_error0 ("operator_unary_minus");
      break;
  }

  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = left_out.exp.kind;
  set_result_sign (ref output, e^.base_type_or_null);

  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

void operator1 (    PCODE       p_code_int4,
                    PCODE       p_code_int8,
                    PEXPRESSION e,
                ref FRAME_INFO  frame,
                ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in;
  EXP_OUTPUT  left_out;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, e^.base_type_or_null);

  if (left_out.exp.kind == EXP_ON_INT_STACK_I4)
    put_code (p_code_int4);
  else if (left_out.exp.kind == EXP_ON_INT_STACK_I8)
    put_code (p_code_int8);
  else
    fatal_compiler_error0 ("operator1(1)");

  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  output.exp.kind = left_out.exp.kind;
  set_result_sign (ref output, e^.base_type_or_null);

  output.constraint.place = CT_NONE;
}

/*****************************************************************************/

void operator_conversion (    PEXPRESSION e,
                          ref FRAME_INFO  frame,
                          ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out output);
  flush_simple_value (ref output, exp, e^.base_type_or_null);
  force_simple_value_of_heap_object_in_register (left_in, output);
  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset
}

/*****************************************************************************/

void operator_inc_dec_ptr (    PCODE       p_code,
                               PEXPRESSION e,
                           ref FRAME_INFO  frame,
                           ref EXP_OUTPUT  output)
{
  EXP_INPUT   prefix_in;
  EXP_OUTPUT  prefix_out;
  PENTITY     element_type;
  int         rc, size, align;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;


  exp = e^.operator_value_info.arg[0];

  clear (prefix_in);
  prefix_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
  generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

  discard_constraint (ref prefix_out);
  flush_address (ref prefix_out);

  // compute size of designated type as an int4
  element_type = e^.base_type_or_null^.the_unsafe_pointer_type.designated_type;

  rc = size_and_alignment_of_type (element_type, out size, out align);
  if (rc != 0)
    fatal_compiler_error0 ("operator_inc_dec_ptr()");

  _unused align;

  put_code (p_code);
  put_int4 (size);

  output.exp.kind = EXP_AT_ADDR_OFFSET;
  output.exp.offset = 0;
  output.constraint.place = CT_NONE;

  if (prefix_in.tomb.anchor_provided != _NO)
  {
    flush_simple_value (ref output, e, e^.base_type_or_null);
    force_simple_value_of_heap_object_in_register (prefix_in, output);
    release_tombstone_anchor (prefix_in);
    frame.frame_offset = saved_frame_offset;    // reset frame offset
  }
}

/*****************************************************************************/

void operator_pre_inc_dec_discrete
         (    PCODE       p_code1,
              PCODE       p_code2,
              PCODE       p_code4,
              PCODE       p_code8,
              PEXPRESSION e,
          ref FRAME_INFO  frame,
          ref EXP_OUTPUT  output)
{
  EXP_INPUT   prefix_in;
  int         rc, size, align;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;


  exp = e^.operator_value_info.arg[0];

  clear (prefix_in);
  prefix_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
  generate_code_for_expression (exp, prefix_in, ref frame, out output);

  discard_constraint (ref output);
  flush_address (ref output);

  put_code (P_DUP_ADDR);

  // compute size of object
  rc = size_and_alignment_of_type (exp^.base_type_or_null, out size, out align);
  if (rc != 0)
    fatal_compiler_error0 ("operator_pre_inc_dec_discrete(1)");

  _unused align;

  if (size == 1)
    put_code (p_code1);
  else if (size == 2)
    put_code (p_code2);
  else if (size == 4)
    put_code (p_code4);
  else if (size == 8)
    put_code (p_code8);
  else
    fatal_compiler_error0 ("operator_pre_inc_dec_discrete(2)");

  flush_simple_value (ref output, exp, e^.base_type_or_null);
  set_result_sign (ref output, e^.base_type_or_null);
  output.constraint.place = CT_NONE;

  if (prefix_in.tomb.anchor_provided != _NO)
  {
    force_simple_value_of_heap_object_in_register (prefix_in, output);
    release_tombstone_anchor (prefix_in);
    frame.frame_offset = saved_frame_offset;    // reset frame offset
  }
}

/*****************************************************************************/

void operator_post_inc_dec_discrete
         (    PCODE       p_code1,
              PCODE       p_code2,
              PCODE       p_code4,
              PCODE       p_code8,
              PEXPRESSION e,
          ref FRAME_INFO  frame,
          ref EXP_OUTPUT  output)
{
  EXP_INPUT   prefix_in;
  int         rc, size, align;
  PEXPRESSION exp;
  int8        saved_frame_offset;


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  clear (prefix_in);
  prefix_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
  generate_code_for_expression (exp, prefix_in, ref frame, out output);

  discard_constraint (ref output);
  flush_address (ref output);

  put_code (P_DUP_ADDR);

  flush_simple_value (ref output, exp, e^.base_type_or_null);
  force_simple_value_in_register (output);


  // compute size of object
  rc = size_and_alignment_of_type (exp^.base_type_or_null, out size, out align);
  if (rc != 0)
    fatal_compiler_error0 ("operator_post_inc_dec_discrete(1)");

  _unused align;

  if (size == 1)
    put_code (p_code1);
  else if (size == 2)
    put_code (p_code2);
  else if (size == 4)
    put_code (p_code4);
  else if (size == 8)
    put_code (p_code8);
  else
    fatal_compiler_error0 ("operator_post_inc_dec_discrete(2)");

  set_result_sign (ref output, e^.base_type_or_null);
  output.constraint.place = CT_NONE;

  if (prefix_in.tomb.anchor_provided != _NO)
  {
    force_simple_value_of_heap_object_in_register (prefix_in, output);
    release_tombstone_anchor (prefix_in);
    frame.frame_offset = saved_frame_offset;    // reset frame offset
  }
}

/*****************************************************************************/

void evaluate_bool_expression (    PEXPRESSION exp,
                               ref FRAME_INFO  frame)
{
  int8        saved_frame_offset;
  EXP_INPUT   exp_in;
  EXP_OUTPUT  exp_out;

  saved_frame_offset = frame.frame_offset;

  clear (exp_in);
  exp_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
  generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
  flush_simple_value (ref exp_out, exp, type_bool);
  force_simple_value_of_heap_object_in_register (exp_in, exp_out);
  release_tombstone_anchor (exp_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset
}

/*****************************************************************************/

void operator_short_circuit (    PCODE       p_code,
                                 PEXPRESSION e,
                             ref FRAME_INFO  frame,
                             ref EXP_OUTPUT  output)
{
  int         nr;
  PEXPRESSION exp;

  exp = e^.operator_value_info.arg[0];

  if (exp^.kind == A_CONST_ENUMERATION_VALUE)
  {
    if ((p_code == P_CAND && exp^.const_enumeration_value_info.value == 1) ||
        (p_code == P_COR && exp^.const_enumeration_value_info.value == 0))
    {
      // evaluate only second operand (which is never constant too)
      exp = e^.operator_value_info.arg[1];
      evaluate_bool_expression (exp, ref frame);

      output.exp.kind = EXP_ON_INT_STACK_B;
      output.constraint.place = CT_NONE;
    }
    else   // final value
    {
      // P_CAND -> false
      // P_COR -> true
      output.exp.kind = EXP_CONST_INT;
      output.exp.value = (p_code == P_CAND) ? 0 : 1;
      output.constraint.place = CT_NONE;
    }
  }
  else   // first operand is not a constant
  {
    evaluate_bool_expression (exp, ref frame);   // evaluate first operand

    exp = e^.operator_value_info.arg[1];
    if (exp^.kind == A_CONST_ENUMERATION_VALUE)   // second operand is a constant
    {
      if ((p_code == P_CAND && exp^.const_enumeration_value_info.value == 1) ||
          (p_code == P_COR && exp^.const_enumeration_value_info.value == 0))
      {
        // result is first operand, ignore second operand
        output.exp.kind = EXP_ON_INT_STACK_B;
        output.constraint.place = CT_NONE;
      }
      else
      {
        // throw away first parameter
        put_code (P_DROP_BOOL);

        output.exp.kind = EXP_CONST_INT;
        output.exp.value = (p_code == P_CAND) ? 0 : 1;
        output.constraint.place = CT_NONE;
      }
    }
    else    // second operand is also not a constant
    {
      nr = get_new_near_label_nr ();
      put_code (p_code);
      put_int4 (nr);

      evaluate_bool_expression (exp, ref frame);

      put_code (P_SYNC_BOOL);

      put_code (P_NEAR_LABEL);
      put_int4 (nr);

      output.exp.kind = EXP_ON_INT_STACK_B;
      output.constraint.place = CT_NONE;
    }
  }
}

/*****************************************************************************/

void operator_condition (    PEXPRESSION e,
                             EXP_INPUT   input,
                         ref FRAME_INFO  frame,
                         ref EXP_OUTPUT  output)
{
  EXP_INPUT   left_in, right_in;
  EXP_OUTPUT  left_out, right_out;
  int         nr1, nr2;
  PENTITY     type;
  PEXPRESSION       exp;
  USES_TOMBSTONE    u;
  int8              saved_frame_offset;

  nr1 = get_new_near_label_nr ();
  nr2 = get_new_near_label_nr ();


  saved_frame_offset = frame.frame_offset;

  exp = e^.operator_value_info.arg[0];

  generate_code_for_branching (exp, false, nr1, ref frame);

  frame.frame_offset = saved_frame_offset;    // reset frame offset


  exp = e^.operator_value_info.arg[1];

  type = complete_type_of (e^.base_type_or_null);

  if (type^.kind == AN_INTEGER_TYPE || type^.kind == A_FLOAT_TYPE            || type^.kind == AN_ENUMERATION_TYPE ||
      type^.kind == A_POINTER_TYPE  || type^.kind == A_FUNCTION_POINTER_TYPE || type^.kind == AN_UNSAFE_POINTER_TYPE)
  {
    saved_frame_offset = frame.frame_offset;

    clear (left_in);
    left_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref left_in, ref frame);
    generate_code_for_expression (exp, left_in, ref frame, out left_out);
    flush_simple_value (ref left_out, exp, type);
    sync_simple_value_in_register (left_out);
    release_tombstone_anchor (left_in);
    sync_simple_value_in_register (left_out);
    correct_stack_after_sync_simple_value_in_register (left_out);
    frame.frame_offset = saved_frame_offset;    // reset frame offset

    output = left_out;
  }
  else if (type^.kind == AN_OPEN_ARRAY_TYPE ||
           (type^.kind == A_STRUCT_TYPE && type^.the_struct_type.is_open_type))
  {
    clear (left_in);
    left_in.data_must_be_computed       = true;
    left_in.constraint_must_be_computed = true;

    u = exp_requires_tombstone_anchor (exp);
    if (u == _NO)
      clear_tombstone_anchor (input);
    else
    {
      left_in.tomb.anchor_provided = u;   // _YES or _YES_BUT_CAN_BE_NULL
      left_in.tomb.anchor_offset = input.tomb.anchor_offset;
    }

    generate_code_for_expression (exp, left_in, ref frame, out left_out);

    if (e^.constraint.kind == CONSTANT_CONSTRAINT)   // result has constant constraint
    {
      if (left_out.constraint.place != CT_CTE)         // argument has runtime constraint
      {
        if (array_checks_enabled)
        {
          flush_constraint (ref left_out);     // check that argument has correct constraint
          put_code (P_CHECK_SAME_1);
          put_int4 ((int)e^.constraint.value);
          put_int4 (get_new_near_label_nr());
        }
        else
        {
          discard_constraint (ref left_out);
        }
      }

      output.constraint.place = CT_CTE;
      output.constraint.cte   = e^.constraint.value;

      flush_address (ref left_out);
      put_code (P_SYNC_ADDR);

      put_code (P_SYNC_STACKS);
      put_int4 (0);
      put_int4 (0);
      put_int4 (1);  // remove addr
      sync_stacks (0,0,1);
    }
    else    // result has runtime constraint
    {
      flush_constraint (ref left_out);
      output.constraint.place = CT_ON_INT_STACK;

      flush_address (ref left_out);
      put_code (P_SYNC_ADDR_INT4);

      put_code (P_SYNC_STACKS);
      put_int4 (1);  // remove int
      put_int4 (0);
      put_int4 (1);  // remove addr
      sync_stacks (1,0,1);
    }

    output.exp.kind = EXP_AT_ADDR_OFFSET;
    output.exp.offset = 0;
  }
  else   // any other struct/union type
  {
    clear (left_in);
    left_in.data_must_be_computed = true;

    u = exp_requires_tombstone_anchor (exp);
    if (u == _NO)
      clear_tombstone_anchor (input);
    else
    {
      left_in.tomb.anchor_provided = u;   // _YES or _YES_BUT_CAN_BE_NULL
      left_in.tomb.anchor_offset = input.tomb.anchor_offset;
    }

    generate_code_for_expression (exp, left_in, ref frame, out left_out);

    discard_constraint (ref left_out);
    flush_address (ref left_out);
    put_code (P_SYNC_ADDR);

    put_code (P_SYNC_STACKS);
    put_int4 (0);
    put_int4 (0);
    put_int4 (1);  // remove addr
    sync_stacks (0,0,1);

    output = left_out;
  }

  put_code (P_GOTO);
  put_int4 (nr2);

  put_code (P_NEAR_LABEL);
  put_int4 (nr1);

  exp = e^.operator_value_info.arg[2];

  if (type^.kind == AN_INTEGER_TYPE || type^.kind == A_FLOAT_TYPE            || type^.kind == AN_ENUMERATION_TYPE ||
      type^.kind == A_POINTER_TYPE  || type^.kind == A_FUNCTION_POINTER_TYPE || type^.kind == AN_UNSAFE_POINTER_TYPE)
  {
    saved_frame_offset = frame.frame_offset;

    clear (right_in);
    right_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref right_in, ref frame);
    generate_code_for_expression (exp, right_in, ref frame, out right_out);
    flush_simple_value (ref right_out, exp, type);
    sync_simple_value_in_register (right_out);
    release_tombstone_anchor (right_in);
    sync_simple_value_in_register (right_out);
    frame.frame_offset = saved_frame_offset;    // reset frame offset
  }
  else if (type^.kind == AN_OPEN_ARRAY_TYPE ||
           (type^.kind == A_STRUCT_TYPE && type^.the_struct_type.is_open_type))
  {
    clear (right_in);
    right_in.data_must_be_computed       = true;
    right_in.constraint_must_be_computed = true;

    u = exp_requires_tombstone_anchor (exp);
    if (u == _NO)
      clear_tombstone_anchor (input);
    else
    {
      right_in.tomb.anchor_provided = u;   // _YES or _YES_BUT_CAN_BE_NULL
      right_in.tomb.anchor_offset = input.tomb.anchor_offset;
    }

    generate_code_for_expression (exp, right_in, ref frame, out right_out);

    if (e^.constraint.kind == CONSTANT_CONSTRAINT)   // result has constant constraint
    {
      if (right_out.constraint.place != CT_CTE)         // argument has runtime constraint
      {
        if (array_checks_enabled)
        {
          flush_constraint (ref right_out);     // check that argument has correct constraint
          put_code (P_CHECK_SAME_1);
          put_int4 ((int)e^.constraint.value);
          put_int4 (get_new_near_label_nr());
        }
        else
        {
          discard_constraint (ref right_out);
        }
      }

      flush_address (ref right_out);
      put_code (P_SYNC_ADDR);
    }
    else    // result has runtime constraint
    {
      flush_constraint (ref right_out);
      flush_address (ref right_out);
      put_code (P_SYNC_ADDR_INT4);
    }
  }
  else   // any other struct/union type
  {
    clear (right_in);
    right_in.data_must_be_computed = true;

    u = exp_requires_tombstone_anchor (exp);
    if (u == _NO)
      clear_tombstone_anchor (input);
    else
    {
      right_in.tomb.anchor_provided = u;   // _YES or _YES_BUT_CAN_BE_NULL
      right_in.tomb.anchor_offset = input.tomb.anchor_offset;
    }

    generate_code_for_expression (exp, right_in, ref frame, out right_out);

    discard_constraint (ref right_out);
    flush_address (ref right_out);
    put_code (P_SYNC_ADDR);
  }

  put_code (P_NEAR_LABEL);
  put_int4 (nr2);
}

/*****************************************************************************/

void evaluate_and_assign_field (    PEXPRESSION exp,
                                    PENTITY     subtype,
                                    uint4       offset,
                                ref FRAME_INFO  frame)
{
  PENTITY     type;
  ENTITY_KIND kind;
  int8        saved_frame_offset;


  type = base_type_of (subtype);
  kind = type^.kind;

  if (kind == AN_ENUMERATION_TYPE || kind == AN_INTEGER_TYPE         || kind == A_FLOAT_TYPE ||
      kind == A_POINTER_TYPE      || kind == A_FUNCTION_POINTER_TYPE || kind == AN_UNSAFE_POINTER_TYPE)
  {
    EXP_INPUT   value_in;
    EXP_OUTPUT  value_out;

    saved_frame_offset = frame.frame_offset;

    clear (value_in);
    value_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref value_in, ref frame);
    generate_code_for_expression (exp,  value_in, ref frame, out value_out);
    flush_simple_value (ref value_out, exp, type);
    store_simple_value_in_field (type, (int)offset);
    release_tombstone_anchor (value_in);

    frame.frame_offset = saved_frame_offset;    // reset frame offset
  }
  else if (kind == AN_OPEN_ARRAY_TYPE ||
             (kind == A_STRUCT_TYPE && type^.the_struct_type.is_open_type))
  {
    int         rc, size, align;
    EXP_INPUT   value_in;
    EXP_OUTPUT  value_out;
    CONTEXT     context;

    // compute size of field
    rc = size_and_alignment_of_type (subtype, out size, out align);
    if (rc < 0)
      code_generator_error ("evaluate_and_assign_field()");

    _unused align;

    convert_type_into_context (subtype, out context);

    saved_frame_offset = frame.frame_offset;

    clear (value_in);
    value_in.data_must_be_computed       = true;
    value_in.constraint_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref value_in, ref frame);
    generate_code_for_expression (exp, value_in, ref frame, out value_out);

    if (value_out.constraint.place != CT_CTE)         // exp has runtime constraint
    {
      if (array_checks_enabled)
      {
        flush_constraint (ref value_out);     // check that argument has correct constraint
        put_code (P_CHECK_SAME_1);
        put_int4 ((int)context.constraint.value);
        put_int4 (get_new_near_label_nr());
      }
      else
      {
        discard_constraint (ref value_out);
      }
    }

    flush_address (ref value_out);

    put_code (P_STORE_FIELD_BLOCK);
    put_int4 ((int)offset);
    put_int4 (size);

    release_tombstone_anchor (value_in);
    frame.frame_offset = saved_frame_offset;    // reset frame offset
  }
  else   // any other struct/union type
  {
    int         rc, size, align;
    EXP_INPUT   value_in;
    EXP_OUTPUT  value_out;

    // compute size of field
    rc = size_and_alignment_of_type (subtype, out size, out align);
    if (rc < 0)
      code_generator_error ("evaluate_and_assign_field()");

    _unused align;

    saved_frame_offset = frame.frame_offset;

    clear (value_in);
    value_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref value_in, ref frame);
    generate_code_for_expression (exp, value_in, ref frame, out value_out);
    flush_address (ref value_out);

    put_code (P_STORE_FIELD_BLOCK);
    put_int4 ((int)offset);
    put_int4 (size);

    release_tombstone_anchor (value_in);
    frame.frame_offset = saved_frame_offset;    // reset frame offset
  }
}

/*****************************************************************************/

// store all aggregate fields

void store_aggregate_fields (    PEXPRESSION e,
                                 int         header_offset,
                             ref FRAME_INFO  frame)
{
  if (e^.base_type_or_null^.kind == AN_OPEN_ARRAY_TYPE)    // array aggregate
  {
    int                  rc, element_size, element_align;
    LIST_OF_EXPRESSIONS^ l;
    uint4                offset;

    rc = size_and_alignment_of_type (e^.base_type_or_null^.the_open_array_type.element,
                                     out element_size, out element_align);
    if (rc < 0)
      code_generator_error ("generate_code_for_expression() : store_aggregate_fields");

    _unused element_align;

    l = e^.aggregate_value_info.list;
    offset = (uint4)header_offset;

    while (l != null)
    {
      evaluate_and_assign_field (l^.exp, l^.type, offset, ref frame);
      l = l^.next;
      offset += (uint4)element_size;
    }
  }
  else   // struct aggregate
  {
    FIELD_DATA           data;
    LIST_OF_EXPRESSIONS^ l;

    begin_struct (out data, e^.base_type_or_null);

    l = e^.aggregate_value_info.list;
    while (l != null)
    {
      begin_field (ref data, l^.e);

      evaluate_and_assign_field (l^.exp, l^.type, (uint)(header_offset + data.field_offset), ref frame);

      end_field (ref data);
      l = l^.next;
    }

    end_struct (ref data);
  }
}

/*****************************************************************************/

void generate_code_for_local_variable (    PENTITY    e,
                                           EXP_INPUT  input,
                                       out EXP_OUTPUT output)
{
  CONTEXT context;

  clear output;

  if (input.data_must_be_computed)
  {
    put_code (P_LOAD_LOCAL);
    put_int4 (e^.the_local_variable.offset);

    if (e^.the_local_variable.allocated_on_heap)
    {
      output.exp.kind = EXP_AT_ADDR_INDIRECT;
    }
    else
    {
      output.exp.kind = EXP_AT_ADDR_OFFSET;
      output.exp.offset = 0;
    }
  }

  convert_type_into_context (e^.the_local_variable.type, out context);
  output.base_type = context.base_type_or_null;

  if (input.constraint_must_be_computed)
  {
    if (context.constraint.kind == DOES_NOT_APPLY)    // has no constraint
    {
      output.constraint.place = CT_NONE;
    }
    else if (context.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct
    {
      output.constraint.place = CT_CTE;
      output.constraint.cte = context.constraint.value;
    }
    else
    {
      fatal_compiler_error0 ("generate_code_for_local_variable()");
    }
  }
}

/*****************************************************************************/

void pcode_drop (PENTITY type)
{
  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_enumeration_type.base].size)
      {
        case 1:
        case 2:
        case 4:
          if (type == type_bool)
          {
            put_code (P_DROP_BOOL);
          }
          else
          {
            put_code (P_DROP_4);
          }
          break;

        default:
          fatal_compiler_error0 ("pcode_drop(1)");
          break;
      }
      break;

    case AN_INTEGER_TYPE:
      switch (INTEGER_DATA[(uint)type^.the_integer_type.type].size)
      {
        case 1:
        case 2:
        case 4:
          put_code (P_DROP_4);
          break;

        case 8:
          put_code (P_DROP_8);
          break;

        default:
          fatal_compiler_error0 ("pcode_drop(2)");
          break;
      }
      break;

    case A_FLOAT_TYPE:
      switch (FLOAT_DATA[(uint)type^.the_float_type.type].size)
      {
        case 4:
          put_code (P_DROP_FLT_4);
          break;

        case 8:
          put_code (P_DROP_FLT_8);
          break;

        default:
          fatal_compiler_error0 ("pcode_drop(3)");
          break;
      }
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      put_code (P_DROP_ADDR);
      break;

    default:
      fatal_compiler_error0 ("pcode_drop(4)");
      break;
  }

}

/*****************************************************************************/

// output:
//  value:
//  - constant (int, float)
//  - xx_stack (int_stack, float_stack)
//  or
//  address:
//  - constant (null)
//  - at address (addr_stack+offset, indirect addr_stack)
//
//  constraint : for array/open struct
//  - cte
//  - temp (local)
//  - xx_stack (int_stack)
//  - at address (addr_stack+offset)
//
// some expressions might generate an addr_stack for both address and constraint.
//
// evaluate object:
// -> 1) flush_constraint on int_stack : using pcode to get constraint
//                                       without touching the address on addr_stack;
//       or discard_constraint : drop int_stack value (if on int_stack)
// -> 2) flush_address on addr_stack; (convert any constant into pool object)
//       or discard_address : drop addr_stack (if address on addr_stack)
//
// evaluate value:
//
// -> 1) flush_value (on int_stack, float_stack or addr_stack for pointer)
//

void generate_code_for_expression
  (    PEXPRESSION e,
       EXP_INPUT   input,
   ref FRAME_INFO  frame,    // to allocate temporary variables and small aggregate space
   out EXP_OUTPUT  output)
{
  clear output;
  output.base_type = e^.base_type_or_null;

  switch (e^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
      output.exp.kind = EXP_CONST_INT;
      output.exp.value = e^.const_enumeration_value_info.value;  // uint4
      output.constraint.place = CT_NONE;
      break;

    case A_CONST_INTEGER_VALUE:
      output.exp.kind = EXP_CONST_INT;
      output.exp.value = e^.const_integer_value_info.value;    // int8
      output.constraint.place = CT_NONE;
      break;

    case A_CONST_FLOAT_VALUE:
      output.exp.kind = EXP_CONST_FLOAT;
      output.exp.fvalue = e^.const_float_value_info.value;    // double
      output.constraint.place = CT_NONE;
      break;

    case A_CONST_NULL_VALUE:
      output.exp.kind = EXP_CONST_NULL;
      output.constraint.place = CT_NONE;
      break;

    case A_POOL_CONSTANT:     // array/struct string literal, constant aggregate, declared constant or index/slice/field of it.
      if (input.data_must_be_computed)
      {
        put_code (P_LOAD_CONST);
        put_int8 (serial_nr_of_pool_cte (e^.pool_constant_info.pool_cte));

        output.exp.kind = EXP_AT_ADDR_OFFSET;
        output.exp.offset = 0;
      }

      if (input.constraint_must_be_computed)
      {
        if (e^.constraint.kind == DOES_NOT_APPLY)    // non-open struct
        {
          output.constraint.place = CT_NONE;
        }
        else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct
        {
          output.constraint.place = CT_CTE;
          output.constraint.cte = e^.constraint.value;
        }
        else
        {
          fatal_compiler_error0 ("generate_code_for_expression(A_POOL_CONSTANT)");
        }
      }
      break;

    case AN_OPERATOR_VALUE:   // runtime value
      switch (e^.operator_value_info.op)
      {
        // 3 params
        case OP_CONDITIONAL_TEST:  // "? :"
          operator_condition (e, input, ref frame, ref output);
          break;

        // 2 params
        case OP_SHORT_CIRCUIT_AND: // bool && bool -> bool
          operator_short_circuit (P_CAND, e, ref frame, ref output);
          break;

        case OP_SHORT_CIRCUIT_OR:  // bool || bool -> bool
          operator_short_circuit (P_COR, e, ref frame, ref output);
          break;

        case OP_AND:               // bool & bool -> bool
          bool_operator2 (P_AND_BOOL, e, ref frame, ref output);
          break;

        case OP_OR:                // bool | bool -> bool
          bool_operator2 (P_OR_BOOL, e, ref frame, ref output);
          break;

        case OP_XOR:               // bool ^ bool -> bool
          bool_operator2 (P_XOR_BOOL, e, ref frame, ref output);
          break;

        case OP_BITAND:            // integer & integer -> integer
          operator2 (P_BITAND4, P_BITAND8, e, ref frame, ref output);
          break;

        case OP_BITOR:             // integer | integer -> integer
          operator2 (P_BITOR4, P_BITOR8, e, ref frame, ref output);
          break;

        case OP_BITXOR:            // integer ^ integer -> integer
          operator2 (P_BITXOR4, P_BITXOR8, e, ref frame, ref output);
          break;

        case OP_COMPARE_SIGNED:     // args = pair of signed integer
          cmp_operator2 (P_CMP_S4, P_CMP_S8, e, ref frame);
          set_bool_result (ref output);
          break;

        case OP_COMPARE_UNSIGNED:   // args = pair of enum or unsigned integer
          if (complete_type_of (e^.operator_value_info.type) == type_bool)
            cmp_bool2 (e, ref frame);
          else
            cmp_operator2 (P_CMP_U4, (PCODE)0, e, ref frame);
          set_bool_result (ref output);
          break;

        case OP_COMPARE_FLOAT:      // args = pair of floating-point
          cmp_operator2 (P_CMP_FLT4, P_CMP_FLT8, e, ref frame);
          set_bool_result (ref output);
          break;

        case OP_COMPARE_PTR:        // args = pair of pointer, function pointer, unsafe pointer, or null pointer
          cmp_ptr2 (e, ref frame);
          set_bool_result (ref output);
          break;

        case OP_SHIFT_LEFT_SIGNED:   // signed integer   << signed integer   -> signed integer
          operator2 (P_ASL4, P_ASL8, e, ref frame, ref output);
          break;

        case OP_SHIFT_LEFT_UNSIGNED:  // unsigned integer << unsigned integer -> unsigned integer
          operator2 (P_SHL4, (PCODE)0, e, ref frame, ref output);
          break;

        case OP_SHIFT_RIGHT_SIGNED:   // signed integer   >> signed integer   -> signed integer
          operator2 (P_ASR4, P_ASR8, e, ref frame, ref output);
          break;

        case OP_SHIFT_RIGHT_UNSIGNED: // unsigned integer >> unsigned integer -> unsigned integer
          operator2 (P_SHR4, (PCODE)0, e, ref frame, ref output);
          break;

        case OP_ADD_INT:              // integer + integer -> integer,  enum + integer -> enum
          if (complete_type_of (e^.base_type_or_null) == type_bool)
            add_sub_bool_int (P_ADD4, e, ref frame, ref output);
          else
            operator2 (P_ADD4, P_ADD8, e, ref frame, ref output);
          break;

        case OP_SUB_INT:              // integer - integer -> integer,  enum - integer -> enum
          if (complete_type_of (e^.base_type_or_null) == type_bool)
            add_sub_bool_int (P_SUB4, e, ref frame, ref output);
          else
            operator2 (P_SUB4, P_SUB8, e, ref frame, ref output);
          break;

        case OP_MUL_INT_SIGNED:       // signed integer * signed integer  ->  signed integer (promoted integer)
          operator2 (P_SMUL4, P_SMUL8, e, ref frame, ref output);
          break;

        case OP_DIV_INT_SIGNED:       // same
          operator2 (P_SDIV4, P_SDIV8, e, ref frame, ref output);
          break;

        case OP_MOD_INT_SIGNED:       // same
          operator2 (P_SMOD4, P_SMOD8, e, ref frame, ref output);
          break;

        case OP_MUL_INT_UNSIGNED:     // unsigned integer * unsigned integer  ->  unsigned integer (promoted integer)
          operator2 (P_UMUL4, (PCODE)0, e, ref frame, ref output);
          break;

        case OP_DIV_INT_UNSIGNED:     // same
          operator2 (P_UDIV4, (PCODE)0, e, ref frame, ref output);
          break;

        case OP_MOD_INT_UNSIGNED:     // same
          operator2 (P_UMOD4, (PCODE)0, e, ref frame, ref output);
          break;

        case OP_ADD_FLOAT:      // floating-point + floating_point -> floating_point
          operator2 (P_ADD_FLT4, P_ADD_FLT8, e, ref frame, ref output);
          break;

        case OP_SUB_FLOAT:     // same
          operator2 (P_SUB_FLT4, P_SUB_FLT8, e, ref frame, ref output);
          break;

        case OP_MUL_FLOAT:     // same
          operator2 (P_MUL_FLT4, P_MUL_FLT8, e, ref frame, ref output);
          break;

        case OP_DIV_FLOAT:     // same
          operator2 (P_DIV_FLT4, P_DIV_FLT8, e, ref frame, ref output);
          break;

        case OP_ADD_PTR_INT:   // unsafe_ptr + integer    -> unsafe_ptr  ; add integer*sizeof(elem) to unsafe_ptr
          operator_add_sub_ptr_int (P_ADD_PTR_OFFSET, e, ref frame, ref output);
          break;

        case OP_SUB_PTR_INT:   // unsafe_ptr - integer    -> unsafe_ptr  ; sub integer*sizeof(elem) from unsafe_ptr
          operator_add_sub_ptr_int (P_SUB_PTR_OFFSET, e, ref frame, ref output);
          break;

        case OP_SUB_PTR_PTR:   // unsafe_ptr - unsafe_ptr -> uint4       ; (ptr-ptr)/sizeof(elem) (type uint4)
          operator_sub_ptr_ptr (e, ref frame, ref output);
          break;

        // 1 param
        case OP_CONVERT_INT_INT:     // convert between various enumeration and integer types
        case OP_CONVERT_FLOAT_INT:   // convert from floating-point to integer
        case OP_CONVERT_INT_FLOAT:   // convert from integer to floating-point
        case OP_CONVERT_FLOAT_FLOAT: // convert from floating-point to floating-point
        case OP_UNARY_PLUS:   // integer -> integer,  or floating-point -> floating_point  (no effect other than promotion)
        case OP_CONVERT_PTR_PTR:     // convert between various unsafe pointers (does flush)
          operator_conversion (e, ref frame, ref output);
          break;

        case OP_UNARY_MINUS:  // integer -> integer,  or floating-point -> floating_point
          operator_unary_minus (e, ref frame, ref output);
          break;

        case OP_BOOL_NOT:     // bool -> bool
          bool_operator1 (P_NOT_BOOL, e, ref frame, ref output);
          break;

        case OP_INT_NOT:      // integer -> integer
          operator1 (P_NOT4, P_NOT8, e, ref frame, ref output);
          break;

        case OP_PRE_DEC:      // arg = object of type enum, int or unsafe-ptr, returns = value of that type
          if (e^.base_type_or_null^.kind == AN_UNSAFE_POINTER_TYPE)
            operator_inc_dec_ptr (P_DEC_VALUE_ADDR, e, ref frame, ref output);
          else
            operator_pre_inc_dec_discrete (P_DEC1, P_DEC2, P_DEC4, P_DEC8, e, ref frame, ref output);
          break;

        case OP_PRE_INC:
          if (e^.base_type_or_null^.kind == AN_UNSAFE_POINTER_TYPE)
            operator_inc_dec_ptr (P_INC_VALUE_ADDR, e, ref frame, ref output);
          else
            operator_pre_inc_dec_discrete (P_INC1, P_INC2, P_INC4, P_INC8, e, ref frame, ref output);
          break;

        case OP_POST_DEC:     // arg = object of type enum, int or unsafe-ptr, returns = value of that type
          if (e^.base_type_or_null^.kind == AN_UNSAFE_POINTER_TYPE)
            operator_inc_dec_ptr (P_VALUE_ADDR_DEC, e, ref frame, ref output);
          else
            operator_post_inc_dec_discrete (P_DEC1, P_DEC2, P_DEC4, P_DEC8, e, ref frame, ref output);
          break;

        case OP_POST_INC:
          if (e^.base_type_or_null^.kind == AN_UNSAFE_POINTER_TYPE)
            operator_inc_dec_ptr (P_VALUE_ADDR_INC, e, ref frame, ref output);
          else
            operator_post_inc_dec_discrete (P_INC1, P_INC2, P_INC4, P_INC8, e, ref frame, ref output);
          break;

        case OP_ADDRESS_OF:   // arg = an object, returns = unsafe pointer.
          {
            EXP_INPUT   prefix_in;
            PEXPRESSION exp;
            int8        saved_frame_offset;

            saved_frame_offset = frame.frame_offset;

            exp = e^.operator_value_info.arg[0];

            clear (prefix_in);
            prefix_in.data_must_be_computed = true;

            allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
            generate_code_for_expression (exp, prefix_in, ref frame, out output);
            discard_constraint (ref output);
            flush_address (ref output);
            force_simple_value_of_heap_object_in_register (prefix_in, output);
            release_tombstone_anchor (prefix_in);

            frame.frame_offset = saved_frame_offset;    // reset frame offset
          }
          break;

        case OP_LENGTH:       // arg = an object of type array and runtime constraint, returns = type int.
          {
            EXP_INPUT   prefix_in;
            EXP_OUTPUT  prefix_out;
            PEXPRESSION exp;
            int8        saved_frame_offset;

            saved_frame_offset = frame.frame_offset;

            exp = e^.operator_value_info.arg[0];

            clear (prefix_in);
            prefix_in.constraint_must_be_computed = true;

            allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
            generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

            if (prefix_in.tomb.anchor_provided != _NO)
            {
              flush_constraint (ref prefix_out);
              put_code (P_FORCE_4);
              release_tombstone_anchor (prefix_in);
              frame.frame_offset = saved_frame_offset;    // reset frame offset
            }

            convert_runtime_constraint_into_exp (ref prefix_out, out output);
          }
          break;

        case OP_SIZE:         // arg = an object of open type having runtime constraint, returns = type uint.
          {
            EXP_INPUT  prefix_in;
            EXP_OUTPUT prefix_out;
            PEXPRESSION exp;
            int8        saved_frame_offset;

            saved_frame_offset = frame.frame_offset;

            exp = e^.operator_value_info.arg[0];

            clear (prefix_in);
            prefix_in.constraint_must_be_computed = true;

            allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
            generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

            if (prefix_in.tomb.anchor_provided != _NO)
            {
              flush_constraint (ref prefix_out);
              put_code (P_FORCE_4);
              release_tombstone_anchor (prefix_in);
              frame.frame_offset = saved_frame_offset;    // reset frame offset
            }

            convert_runtime_constraint_into_size_constraint (ref prefix_out);
            convert_runtime_constraint_into_exp (ref prefix_out, out output);
          }
          break;

        default:
          fatal_compiler_error0 ("generate_code_for_expression(operator)");
          break;
      }
      break;

    case A_RUN_CALL:
      {
        PEXPRESSION          func_call, exp;
        EXP_INPUT            prefix_in;
        EXP_OUTPUT           prefix_out;
        LIST_OF_EXPRESSIONS^ list;
        PENTITY              param, type;
        int8                 saved_frame_offset0, saved_frame_offset;

        saved_frame_offset0 = frame.frame_offset;

        func_call = e^.run_call_info.function_call;


        // evaluate function address

        saved_frame_offset = frame.frame_offset;

        exp = func_call^.function_call_info.func;
        complete_type_of (exp^.base_type_or_null)^.the_function_pointer_type.is_thread_entry_point = true;

        clear (prefix_in);
        prefix_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
        generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);
        discard_constraint (ref prefix_out);
        flush_address (ref prefix_out);
        force_simple_value_of_heap_object_in_register (prefix_in, prefix_out);
        release_tombstone_anchor (prefix_in);
        frame.frame_offset = saved_frame_offset;    // reset frame offset

        // evaluate the single parameter, if any

        list = func_call^.function_call_info.param;
        while (list != null && list^.e^.kind != A_PARAMETER)
          list = list^.next;

        if (list == null)
        {
          put_code (P_RUN_VOID_PARAM);
          put_int4 (get_new_near_label_nr ());
        }
        else
        {
          EXP_INPUT   arg_in;
          EXP_OUTPUT  arg_out;

          param = list^.e;
          type = base_type_of (param^.the_parameter.type);

          if (param^.the_parameter.mode == MODE_IN &&
              (type^.kind == AN_INTEGER_TYPE ||
               type^.kind == AN_ENUMERATION_TYPE ||
               type^.kind == A_POINTER_TYPE ||
               type^.kind == A_FUNCTION_POINTER_TYPE ||
               type^.kind == AN_UNSAFE_POINTER_TYPE))
          {
            saved_frame_offset = frame.frame_offset;

            clear (arg_in);
            arg_in.data_must_be_computed = true;

            allocate_tombstone_anchor (list^.exp, ref arg_in, ref frame);
            generate_code_for_expression (list^.exp, arg_in, ref frame, out arg_out);

            if (type^.kind == AN_ENUMERATION_TYPE)
              flush_simple_value (ref arg_out, list^.exp, type_int);
            else
              flush_simple_value (ref arg_out, list^.exp, type);
            discard_constraint (ref arg_out);

            force_simple_value_of_heap_object_in_register (arg_in, arg_out);
            release_tombstone_anchor (arg_in);
            frame.frame_offset = saved_frame_offset;    // reset frame offset

            if (type^.kind == AN_INTEGER_TYPE || type^.kind == AN_ENUMERATION_TYPE)
            {
              put_code (P_RUN_INT4_PARAM);
            }
            else
              put_code (P_RUN_ADDR_PARAM);
            put_int4 (get_new_near_label_nr ());
          }
          else   // array, struct, union or any out/ref parameter
          {
            clear (arg_in);
            arg_in.data_must_be_computed = true;

            // no tombstone anchor needed because heap objects are not allowed
            generate_code_for_expression (list^.exp, arg_in, ref frame, out arg_out);

            discard_constraint (ref arg_out);
            flush_address (ref arg_out);

            put_code (P_RUN_ADDR_PARAM);
            put_int4 (get_new_near_label_nr ());
          }
        }

        frame.frame_offset = saved_frame_offset0;    // reset frame offset

        output.exp.kind = EXP_ON_INT_STACK_I4;
        output.exp.sign = _SIGNED;
      }
      break;

    case A_FUNCTION_VALUE:    // value of type function pointer
      if (input.data_must_be_computed)
      {
        PENTITY  f;
        wstring^ dll;

        f = e^.function_value_info.to_function_declaration_or_generic_function;

        while (f^.kind == A_GENERIC_FUNCTION)
          f = f^.the_generic_function.actual_func;

        if (f^.kind != A_FUNCTION_DECLARATION)
          fatal_compiler_error0 ("generate_code_for_expression(a_function_name(1))");

        if (f^.the_function_declaration.to_type^.the_function_pointer_type.is_syscall)
        {
          put_code (P_LOAD_SYSCALL);
          put_int4 (f^.the_function_declaration.to_type^.the_function_pointer_type.syscall_number);

          output.exp.kind = EXP_AT_ADDR_OFFSET;
          output.exp.offset = 0;
        }
        else
        {
          dll = f^.the_function_declaration.to_type
                 ^.the_function_pointer_type.extern_dll_or_null;

          if (dll == null)
          {
            if (f^.the_function_declaration.func_label_nr == 0)
              f^.the_function_declaration.func_label_nr = get_new_func_label_nr();

            put_code (P_LOAD_CODE);
            put_int4 (f^.the_function_declaration.func_label_nr);

            output.exp.kind = EXP_AT_ADDR_OFFSET;
            output.exp.offset = 0;

            add_function_to_pending_functions (f);
          }
          else   // indirect address of a DLL function
          {
            int  pos;
            char dllname[MAX_STRING_LITERAL_LENGTH+1];
            char funcname[MAX_IDENTIFIER_LENGTH+1];

            pos = wstrchr (dll^, L':');
            if (pos == -1)
            {
              wstring_to_string (dll^, out dllname);
              wstring_to_string (f^.identifier_or_null^, out funcname);
            }
            else
            {
              wstring_to_string (dll^[0:pos], out dllname);
              wstring_to_string (dll^[pos+1:wstrlen(dll^) - (pos+1)], out funcname);
            }
            
            put_code (P_LOAD_DLL);
            put_int4 ((int)insert_dll_name (dllname, funcname));

            output.exp.kind = EXP_AT_ADDR_INDIRECT;
            output.exp.offset = 0;
          }
        }
      }
      break;

    case A_FUNCTION_CALL:
      {
        PEXPRESSION          exp;
        EXP_INPUT            prefix_in;
        EXP_OUTPUT           prefix_out;
        PENTITY              rtype;
        bool                 left_to_right;
        LIST_OF_EXPRESSIONS^ list;
        int8                 saved_frame_offset0, saved_frame_offset;
        int                  size, nb_86_entries_pushed = 0, extra_shadow_stack_space_size, parameter_size, alignment_size;
        char                 parameter_table[4];
        ARM_PARAM_ALLOCATION arm_param_allocation;
        IFA_STACK_SLOTS      ifa;

        saved_frame_offset0 = frame.frame_offset;

        exp = e^.function_call_info.func;

        clear (prefix_in);
        prefix_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
        generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);
        discard_constraint (ref prefix_out);

        if (goptions.g_target == ANDROID)   // for android we do this early
        {
          // set as hint to store this in X8
          put_code (P_HINT_PARAM);
          put_byte (0);  // type : X register
          put_int4 (8);  // nr   : 8

          flush_simple_value (ref prefix_out, exp, exp^.base_type_or_null);

          if (prefix_in.tomb.anchor_provided != _NO)
          {
            put_code (P_FORCE_ADDR);
            release_tombstone_anchor (prefix_in);
          }

          // clear hint
          put_code (P_HINT_PARAM);
          put_byte (0);  // type : X register
          put_int4 (0);  // nr   : 0
        }

        if (goptions.g_target == INTEL)
        {
          left_to_right = (exp^.base_type_or_null
                              ^.the_function_pointer_type.extern_dll_or_null == null &&
                           !exp^.base_type_or_null
                               ^.the_function_pointer_type.is_callback);
        }
        else if (goptions.g_target == ANDROID)
        {
          left_to_right = true;
        }
        else
          abort;

        list = e^.function_call_info.param;
        if (!left_to_right)
        {
          while (list != null && list^.next != null)
            list = list^.next;
        }


        extra_shadow_stack_space_size = 0;
        parameter_size = 0;
        alignment_size = 0;
        parameter_table = "....";

        // for Intel 64-bit : compute extra bytes to reserve on stack for shadow space and alignment
        if (goptions.g_target == INTEL && address_size == 8)
        {
          int                  parameter_count;
          LIST_OF_EXPRESSIONS^ li;

          // compute nb parameters
          parameter_count = 0;
          li = e^.function_call_info.param;
          while (li != null)          // looping on parameters in left to right order
          {
            PENTITY     param, type;
            ENTITY_KIND k;
            CONTEXT     context;

            param = li^.e;
            type = complete_type_of (param^.the_parameter.type);
            k = type^.kind;

            if (param^.the_parameter.mode == MODE_IN &&    // parameter of mode 'in' of simple type
                (k == AN_INTEGER_TYPE || k == A_FLOAT_TYPE            || k == AN_ENUMERATION_TYPE ||
                 k == A_POINTER_TYPE  || k == A_FUNCTION_POINTER_TYPE || k == AN_UNSAFE_POINTER_TYPE))
            {
              if (parameter_count < 4)
                parameter_table[parameter_count] = code_datatype (type);
              parameter_count++;
            }
            else
            {
              if (parameter_count < 4)
                parameter_table[parameter_count] = 'a';
              parameter_count++;

              convert_type_into_context (type, out context);
              if (context.constraint.kind == UNCONSTRAINED)
              {
                if (parameter_count < 4)
                  parameter_table[parameter_count] = 'i';
                parameter_count++;   // count also array length & struct discriminant
              }
            }

            li = li^.next;
          }

          parameter_size = parameter_count * 8;

          if (!left_to_right)   // 64-bit operating system call
          {
            if (parameter_size < 32)
              extra_shadow_stack_space_size = 32 - parameter_size;     // extend shadow space to 32 bytes
          }

          if (((frame.currently_pushed_offset + extra_shadow_stack_space_size + parameter_size) & 15) != 0)
            alignment_size = 8;                      // used to align to M16

          // extra space we must reserve on stack before pushing the parameters
          put_code (P_SYSCALL_EXTRA_STACK_SPACE);
          put_int4 (alignment_size + extra_shadow_stack_space_size);

          frame.currently_pushed_offset += (alignment_size + extra_shadow_stack_space_size);

        }   // end Intel


        clear arm_param_allocation;   // for android
        clear ifa;


        // evaluate all parameters

        while (list != null)
        {
          PENTITY     param, type;
          ENTITY_KIND k;
          EXP_INPUT   arg_in;
          EXP_OUTPUT  arg_out;
          CONTEXT     context;

          param = list^.e;

          type = complete_type_of (param^.the_parameter.type);
          k = type^.kind;

          if (param^.the_parameter.mode == MODE_IN &&    // parameter of mode 'in' of simple type
              (k == AN_INTEGER_TYPE || k == A_FLOAT_TYPE            || k == AN_ENUMERATION_TYPE ||
               k == A_POINTER_TYPE  || k == A_FUNCTION_POINTER_TYPE || k == AN_UNSAFE_POINTER_TYPE))
          {
            int slots;

            // evaluate, flush and push simple value

            saved_frame_offset = frame.frame_offset;

            if (goptions.g_target == INTEL)
              ;
            else if (goptions.g_target == ANDROID)
            {
              put_code (P_HINT_PARAM);
              put_arm_register_reg_nr_info (ref arm       => arm_param_allocation,
                                                is_float  => type^.kind == A_FLOAT_TYPE,
                                                is_signed => false,  // don't care for hint
                                                is_hint   => true);
            }
            else
              abort;

            clear (arg_in);
            arg_in.data_must_be_computed = true;

            allocate_tombstone_anchor    (list^.exp, ref arg_in, ref frame);
            generate_code_for_expression (list^.exp, arg_in, ref frame, out arg_out);

            flush_simple_value (ref arg_out, list^.exp, type);
            discard_constraint (ref arg_out);

            size = push_simple_value (type);

            if (goptions.g_target == ANDROID)
            {
              put_arm_register_reg_nr_info
                   (ref arm       => arm_param_allocation,
                        is_float  => type^.kind == A_FLOAT_TYPE,
                        is_signed => type^.kind == AN_INTEGER_TYPE && INTEGER_DATA[(uint)type^.the_integer_type.type].is_signed,
                        is_hint   => false);

              count_ifa_slots_simple_type (ref ifa, type);
            }

            slots = (size <= address_size) ? 1 : 2;

            nb_86_entries_pushed          += slots;
            frame.currently_pushed_offset += slots * address_size;

            release_tombstone_anchor (arg_in);

            frame.frame_offset = saved_frame_offset;    // reset frame offset

            // store NO anchor in list
            list^.tomb.anchor_provided = _NO;
          }
          else
          {
            PARAMETER_LOCATION  loc_address, loc_constraint;

            clear loc_address, loc_constraint;


            // in case of a boxed array object parameter, preallocate tombstone anchors.

            if (list^.exp^.kind == A_BOXED_ARRAY_OBJECT)
            {
              LIST_OF_EXPRESSIONS^ list2;

              list2 = list^.exp^.boxed_array_object_info.list;
              while (list2 != null)
              {
                allocate_tombstone (list2^.exp, ref list2^.tomb, ref frame);
                list2 = list2^.next;
              }
            }

            // evaluate, possibly push constraint, push address

            if (goptions.g_target == INTEL)
              ;
            else if (goptions.g_target == ANDROID)
            {
              // this hint is for the address, as it is stored in the first register of an (address, constraint) pair
              put_code (P_HINT_PARAM);
              put_arm_register_reg_nr_info (ref arm       => arm_param_allocation,
                                                is_float  => false,
                                                is_signed => false,  // don't care for hint
                                                is_hint   => true);
            }
            else
              abort;

            clear (arg_in);
            arg_in.data_must_be_computed = true;
            arg_in.constraint_must_be_computed = true;

            allocate_tombstone_anchor    (list^.exp, ref arg_in, ref frame);
            generate_code_for_expression (list^.exp, arg_in, ref frame, out arg_out);  // can generate int4 constraint on istack

            convert_type_into_context (type, out context);

            if (goptions.g_target == ANDROID)
            {
              if (context.constraint.kind == UNCONSTRAINED)  // a pair (address + constraint) must be passed
              {
                // we need to check if both can be stored in registers (if not, both must be stored on stack)
                // and then address must receive the first register, and the constraint the second.
                compute_pair_of_arm_register_info (ref arm_param_allocation,
                                                   out loc_address,
                                                   out loc_constraint);
              }
              else
              {
                compute_arm_register_info (ref arm_param_allocation,
                                                is_float  => false,
                                                is_signed => false,
                                                is_hint   => false,
                                            out loc_address);
              }
            }

            if (context.constraint.kind == UNCONSTRAINED)
            {
              if (goptions.g_target == ANDROID)
              {
                put_code (P_HINT_PARAM);
                put_arm_location (loc_constraint, is_hint => true);
              }

              flush_constraint (ref arg_out);   // address must be on astack in case it's a open type heap object

              if (goptions.g_target == INTEL)                // constraint is pushed first
                put_code (P_PUSH_4);
              else if (goptions.g_target == ANDROID)
              {
                put_code (P_STORE_PARAM_INT4);
                put_arm_location (loc_constraint, is_hint => false);
                ifa[0]++;   // add an istack slot
              }
              else
                abort;

              nb_86_entries_pushed++;
              frame.currently_pushed_offset += address_size;
            }
            else if (context.constraint.kind == CONSTANT_CONSTRAINT && arg_out.constraint.place != CT_CTE)
            {
              if (array_checks_enabled)
              {
                flush_constraint (ref arg_out);   // address must be on astack in case it's a heap object
                put_code (P_CHECK_SAME_1);
                put_int4 ((int)context.constraint.value);
                put_int4 (get_new_near_label_nr());
              }
              else
              {
                discard_constraint (ref arg_out);
              }
            }


            flush_address (ref arg_out);

            if (goptions.g_target == INTEL)          // intel : address is pushed after
            {
              put_code (P_PUSH_ADDR);
            }
            else if (goptions.g_target == ANDROID)
            {
              put_code (P_STORE_PARAM_ADDR);
              put_arm_location (loc_address, is_hint => false);
              ifa[2]++;   // add an astack slot
            }
            else
              abort;

            nb_86_entries_pushed++;
            frame.currently_pushed_offset += address_size;

            // store anchor in list
            list^.tomb = arg_in.tomb;
          }

          if (left_to_right)
            list = list^.next;
          else
            list = list^.prev;
        }


        if (goptions.g_target == INTEL)  // for intel, we do this late
        {
          flush_simple_value (ref prefix_out, exp, exp^.base_type_or_null);

          if (prefix_in.tomb.anchor_provided != _NO)
          {
            put_code (P_FORCE_ADDR);
            release_tombstone_anchor (prefix_in);
          }
        }

        // perform call

        if (goptions.g_target == INTEL)
        {
          put_code (P_CALL_INTEL);
          put_int4 (alignment_size + extra_shadow_stack_space_size);
          put_int4 (nb_86_entries_pushed);
          put_byte ((byte)(!left_to_right));   // true = calling operating system
          put_byte ((byte)parameter_table[0]);
          put_byte ((byte)parameter_table[1]);
          put_byte ((byte)parameter_table[2]);
          put_byte ((byte)parameter_table[3]);
        }
        else if (goptions.g_target == ANDROID)
        {
          put_code (P_CALL_ARM);
          put_int4 (ifa[0]);   // for android: nb of (istack, fstack, astack) slots used by parameters
          put_int4 (ifa[1]);
          put_int4 (ifa[2]);
          put_int4 (arm_param_allocation.stack);  // for android : size of parameters allocated on stack
        }
        else
          abort;


        frame.currently_pushed_offset -= nb_86_entries_pushed * address_size + alignment_size + extra_shadow_stack_space_size;


        if (goptions.g_target == ANDROID)
        {
          // drop all parameters

          list = e^.function_call_info.param;

          while (list != null && list^.next != null)
            list = list^.next;

          while (list != null)   // reverse order
          {
            PENTITY     param, type;
            ENTITY_KIND k;
            CONTEXT     context;

            param = list^.e;
            type = complete_type_of (param^.the_parameter.type);
            k = type^.kind;

            if (param^.the_parameter.mode == MODE_IN &&    // parameter of mode 'in' of simple type
                (k == AN_INTEGER_TYPE || k == A_FLOAT_TYPE            || k == AN_ENUMERATION_TYPE ||
                 k == A_POINTER_TYPE  || k == A_FUNCTION_POINTER_TYPE || k == AN_UNSAFE_POINTER_TYPE))
            {
              pcode_drop (type);
            }
            else
            {
              convert_type_into_context (type, out context);
              if (context.constraint.kind == UNCONSTRAINED)
                put_code (P_DROP_4);
              put_code (P_DROP_ADDR);
            }

            list = list^.prev;
          }

          put_code (P_DROP_ADDR);    // drop function call address
        }


        // return type

        rtype = complete_type_of (e^.base_type_or_null);
        if (rtype == type_void)
        {
          put_code (P_RETVALUE_VOID);
          output.exp.kind = EXP_NONE;
        }
        else
        {
          switch (rtype^.kind)
          {
            case AN_ENUMERATION_TYPE:
              switch (INTEGER_DATA[(uint)rtype^.the_enumeration_type.base].size)
              {
                case 1:
                case 2:
                  if (rtype == type_bool)
                  {
                    put_code (P_RETVALUE_BOOL);
                    output.exp.kind = EXP_ON_INT_STACK_B;
                  }
                  else
                  {
                    put_code (P_RETVALUE_4);
                    output.exp.kind = EXP_ON_INT_STACK_I4;
                    output.exp.sign = _DONTCARE;
                  }
                  break;

                case 4:
                  put_code (P_RETVALUE_4);
                  output.exp.kind = EXP_ON_INT_STACK_I4;
                  output.exp.sign = _UNSIGNED;
                  break;

                default:
                  fatal_compiler_error0 ("generate_code_for_expression(a_function_call(1))");
                  break;
              }
              break;

            case AN_INTEGER_TYPE:
              switch (INTEGER_DATA[(uint)rtype^.the_integer_type.type].size)
              {
                case 1:
                case 2:
                  put_code (P_RETVALUE_4);
                  output.exp.kind = EXP_ON_INT_STACK_I4;

                  if (INTEGER_DATA[(uint)rtype^.the_integer_type.type].is_signed)
                    output.exp.sign = _SIGNED;
                  else
                    output.exp.sign = _DONTCARE;
                  break;

                case 4:
                  put_code (P_RETVALUE_4);
                  output.exp.kind = EXP_ON_INT_STACK_I4;

                  if (INTEGER_DATA[(uint)rtype^.the_integer_type.type].is_signed)
                    output.exp.sign = _SIGNED;
                  else
                    output.exp.sign = _UNSIGNED;
                  break;

                case 8:
                  put_code (P_RETVALUE_8);
                  output.exp.kind = EXP_ON_INT_STACK_I8;
                  break;

                default:
                  fatal_compiler_error0 ("generate_code_for_expression(a_function_call(2))");
                  break;
              }
              break;

            case A_FLOAT_TYPE:
              switch (FLOAT_DATA[(uint)rtype^.the_float_type.type].size)
              {
                case 4:
                  put_code (P_RETVALUE_FLT4);
                  output.exp.kind = EXP_ON_FLOAT_STACK_F4;
                  break;

                case 8:
                  put_code (P_RETVALUE_FLT8);
                  output.exp.kind = EXP_ON_FLOAT_STACK_F8;
                  break;

                default:
                  fatal_compiler_error0 ("generate_code_for_expression(a_function_call(3))");
                  break;
              }
              break;

            case A_POINTER_TYPE:
            case A_FUNCTION_POINTER_TYPE:
            case AN_UNSAFE_POINTER_TYPE:
              put_code (P_RETVALUE_ADDR);
              output.exp.kind = EXP_AT_ADDR_OFFSET;
              output.exp.offset = 0;
              break;

            default:
              fatal_compiler_error0 ("generate_code_for_expression(a_function_call(4))");
              break;
          }
        }

        // release all tombstones

        list = e^.function_call_info.param;

        while (list != null)
        {
          release_tombstone (list^.tomb);

          if (list^.exp^.kind == A_BOXED_ARRAY_OBJECT)
          {
            LIST_OF_EXPRESSIONS^ list2;

            list2 = list^.exp^.boxed_array_object_info.list;
            while (list2 != null)
            {
              release_tombstone (list2^.tomb);
              list2 = list2^.next;
            }
          }

          list = list^.next;
        }


        frame.frame_offset = saved_frame_offset0;    // reset frame offset
      }
      break;

    case A_DISCRIMINANT_VALUE:   // prefix is an object with a runtime constraint, return enum value
      {
        EXP_INPUT   prefix_in;
        EXP_OUTPUT  prefix_out;
        PEXPRESSION exp;
        int8        saved_frame_offset;

        saved_frame_offset = frame.frame_offset;

        exp = e^.discriminant_value_info.prefix;

        clear (prefix_in);
        prefix_in.constraint_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
        generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

        if (prefix_in.tomb.anchor_provided != _NO)
        {
          flush_constraint (ref prefix_out);
          put_code (P_FORCE_4);
          release_tombstone_anchor (prefix_in);
          frame.frame_offset = saved_frame_offset;    // reset frame offset
        }

        convert_runtime_constraint_into_exp (ref prefix_out, out output);
      }
      break;

    case AN_UNC_ARRAY_AGGREGATE:
      {
        int     rc, aggregate_size, align;
        int4    aggregate_ofs;
        int4    element_ofs;
        int     element_size;
        PENTITY element_subtype;
        int8    saved_frame_offset;


        // reserve aggregate on stack

        rc = size_and_alignment_of_constant_size_exp (e, out aggregate_size, out align);
        if (rc < 0)
          code_generator_error ("generate_code_for_expression() : unc_agg(1) is too large");

        _unused align;

        aggregate_ofs = allocate_temp_variable (aggregate_size, ref frame);

        put_code (P_LOAD_LOCAL);
        put_int4 (aggregate_ofs);


        // reserve element on stack

        element_subtype = e^.base_type_or_null^.the_open_array_type.element;

        rc = size_and_alignment_of_type (element_subtype, out element_size, out align);
        if (rc < 0)
          code_generator_error ("generate_code_for_expression() : unc_agg(2) is too large");

        _unused align;

        saved_frame_offset = frame.frame_offset;

        element_ofs = allocate_temp_variable (element_size, ref frame);


        // evaluate element expression and store it in temp variable

        put_code (P_LOAD_LOCAL);
        put_int4 (element_ofs);

        evaluate_and_assign_field (    e^.unc_array_aggregate_info.element,
                                       element_subtype,
                                       0,
                                   ref frame);
        // note: address of temp_ofs remains on addr_stack

        // load constant constraint
        if (e^.constraint.kind != CONSTANT_CONSTRAINT)
          code_generator_error ("generate_code_for_expression() : unc_agg(3) : constraint expected");

        put_code (P_CTE_4);
        put_int4 ((int)e^.constraint.value);

        put_code (P_MULTI_COPY);   // multi_copy (target, repeat, source, size);
        put_int4 (element_size);
        put_int4 (get_new_near_label_nr());
        put_int4 (get_new_near_label_nr());

        frame.frame_offset = saved_frame_offset;    // reset frame offset

        output.exp.kind = EXP_AT_ADDR_OFFSET;
        output.exp.offset = 0;

        output.constraint.place = CT_CTE;
        output.constraint.cte   = e^.constraint.value;
      }
      break;

    case AN_AGGREGATE_VALUE:
      {
        int   rc, size, align;
        int4  temp_ofs;

        rc = size_and_alignment_of_constant_size_exp (e, out size, out align);
        if (rc < 0)
          code_generator_error ("generate_code_for_expression() : aggregate is too large");

        _unused align;

        temp_ofs = allocate_temp_variable (size, ref frame);

        put_code (P_LOAD_LOCAL);
        put_int4 (temp_ofs);

        store_aggregate_fields (e, 0, ref frame);

        output.exp.kind = EXP_AT_ADDR_OFFSET;
        output.exp.offset = 0;

        if (e^.constraint.kind == CONSTANT_CONSTRAINT)
        {
          output.constraint.place = CT_CTE;
          output.constraint.cte   = e^.constraint.value;
        }
      }
      break;

    case A_QUALIFIED_EXPRESSION:
      {
        PENTITY     type;
        ENTITY_KIND kind;
        PEXPRESSION exp;
        int8        saved_frame_offset;

        exp = e^.qualified_expression_info.value;

        type = complete_type_of (e^.base_type_or_null);
        kind = type^.kind;

        if (kind == AN_ENUMERATION_TYPE || kind == AN_INTEGER_TYPE         || kind == A_FLOAT_TYPE ||
            kind == A_POINTER_TYPE      || kind == A_FUNCTION_POINTER_TYPE || kind == AN_UNSAFE_POINTER_TYPE)
        {
          EXP_INPUT value_in;

          saved_frame_offset = frame.frame_offset;

          clear (value_in);
          value_in.data_must_be_computed = true;

          allocate_tombstone_anchor    (exp, ref value_in, ref frame);
          generate_code_for_expression (exp, value_in, ref frame, out output);
          flush_simple_value (ref output, e, type);
          force_simple_value_of_heap_object_in_register (value_in, output);
          release_tombstone_anchor (value_in);
          frame.frame_offset = saved_frame_offset;    // reset frame offset
        }
        else if (kind == AN_OPEN_ARRAY_TYPE ||
                  (kind == A_STRUCT_TYPE && type^.the_struct_type.is_open_type))
        {
          EXP_INPUT value_in;

          clear (value_in);
          value_in.data_must_be_computed       = true;
          value_in.constraint_must_be_computed = true;

          value_in.tomb = input.tomb;
          generate_code_for_expression (exp, value_in, ref frame, out output);

          if (e^.constraint.kind == CONSTANT_CONSTRAINT)   // result has constant constraint
          {
            if (output.constraint.place != CT_CTE)         // argument has runtime constraint
            {
              if (array_checks_enabled)
              {
                flush_constraint (ref output);     // check that argument has correct constraint
                put_code (P_CHECK_SAME_1);
                put_int4 ((int)e^.constraint.value);
                put_int4 (get_new_near_label_nr());
              }
              else
              {
                discard_constraint (ref output);
              }
            }

            output.constraint.place = CT_CTE;
            output.constraint.cte   = e^.constraint.value;
          }
        }
        else   // any other struct/union type
        {
          EXP_INPUT value_in;

          clear (value_in);
          value_in.data_must_be_computed = true;

          value_in.tomb = input.tomb;
          generate_code_for_expression (exp, value_in, ref frame, out output);
          discard_constraint (ref output);
        }
      }
      break;

    case AN_ARRAY_QUALIFIED_EXPRESSION:
      fatal_compiler_error0 ("generate_code_for_expression(AN_ARRAY_QUALIFIED_EXPRESSION)");
      break;

    case A_STRUCT_QUALIFIED_EXPRESSION:
      fatal_compiler_error0 ("generate_code_for_expression(A_STRUCT_QUALIFIED_EXPRESSION)");
      break;

    case AN_ALLOCATOR:
      {
        PENTITY     designated_subtype;
        int         header_size;
        PEXPRESSION qual;
        int8        saved_frame_offset0;

        saved_frame_offset0 = frame.frame_offset;

        designated_subtype = complete_type_of (e^.base_type_or_null^.the_pointer_type.designated_type);

        header_size = 0;
        if (e^.allocator_info.has_header)
          header_size = access_object_header_size_for_open_type (designated_subtype);

        qual = e^.allocator_info.value;

        switch (qual^.kind)
        {
          case A_QUALIFIED_EXPRESSION:        // has sometimes a header
            {
              PEXPRESSION inner_exp;

              inner_exp = qual^.qualified_expression_info.value;

              if (inner_exp == null)    // AN_ALLOCATOR + A_QUALIFIED_EXPRESSION (null)
              {
                int rc, size, align;

                // constant size
                rc = size_and_alignment_of_constant_size_exp (qual, out size, out align);
                if (rc < 0)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc1)");

                _unused align;

                if (size > 2147483647 - header_size)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc2)");

                put_code (P_CTE_4);
                put_int4 (header_size + size);

                put_code (P_MALLOC);
                put_byte (1); //  <bool_fill_zeroes>

                if (header_size > 0)    // if open: fill header with constant
                {
                  put_code (P_CTE_4);
                  put_int4 ((int)qual^.constraint.value);

                  put_code (P_STORE_FIELD_4);
                  put_int4 (0);
                }

                output.exp.kind = EXP_AT_ADDR_OFFSET;
                output.exp.offset = 0;
              }
              else if (inner_exp^.kind == AN_AGGREGATE_VALUE)  // AN_ALLOCATOR + A_QUALIFIED_EXPRESSION + AN_AGGREGATE_VALUE
              {
                int rc, size, align;

                // constant size
                rc = size_and_alignment_of_constant_size_exp (inner_exp, out size, out align);
                if (rc < 0)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc3)");

                _unused align;

                if (size > 2147483647 - header_size)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc4)");

                put_code (P_CTE_4);
                put_int4 (header_size + size);

                put_code (P_MALLOC);
                put_byte (0); //  <bool_fill_zeroes>

                if (header_size > 0)    // if open: fill header with constant
                {
                  put_code (P_CTE_4);
                  put_int4 ((int)inner_exp^.constraint.value);

                  put_code (P_STORE_FIELD_4);
                  put_int4 (0);
                }

                store_aggregate_fields (inner_exp, header_size, ref frame);

                output.exp.kind = EXP_AT_ADDR_OFFSET;
                output.exp.offset = 0;
              }
              else if (inner_exp^.kind == AN_UNC_ARRAY_AGGREGATE)  // AN_ALLOCATOR + A_QUALIFIED_EXPRESSION + AN_UNC_ARRAY_AGGREGATE
              {
                int     rc, size, align;
                PENTITY element_subtype;
                int     element_size;
                int4    element_ofs;


                // reserve aggregate on heap

                rc = size_and_alignment_of_constant_size_exp (inner_exp, out size, out align);
                if (rc < 0)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc5)");

                _unused align;

                if (size > 2147483647 - header_size)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc6)");

                put_code (P_CTE_4);
                put_int4 (header_size + size);

                put_code (P_MALLOC);
                put_byte (0); //  <bool_fill_zeroes>

                if (header_size > 0)    // if open: fill header with constant
                {
                  put_code (P_CTE_4);
                  put_int4 ((int)inner_exp^.constraint.value);

                  put_code (P_STORE_FIELD_4);
                  put_int4 (0);


                  // add header offset to address
                  put_code (P_ADD_OFFSET);
                  put_int4 (header_size);
                }


                // reserve element on stack

                element_subtype = inner_exp^.base_type_or_null^.the_open_array_type.element;

                rc = size_and_alignment_of_type (element_subtype, out element_size, out align);
                if (rc < 0)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc7)");

                _unused align;

                element_ofs = allocate_temp_variable (element_size, ref frame);


                // evaluate element expression and store it in element_ofs variable

                put_code (P_LOAD_LOCAL);
                put_int4 (element_ofs);

                evaluate_and_assign_field (    inner_exp^.unc_array_aggregate_info.element,
                                               element_subtype,
                                               0,
                                           ref frame);
                // note: address of temp_ofs remains on addr_stack

                // load constant constraint
                if (inner_exp^.constraint.kind != CONSTANT_CONSTRAINT)
                  code_generator_error ("generate_code_for_expression(alloc8)");

                put_code (P_CTE_4);
                put_int4 ((int)inner_exp^.constraint.value);

                put_code (P_MULTI_COPY);   // multi_copy (target, repeat, source, size);
                put_int4 (element_size);
                put_int4 (get_new_near_label_nr());
                put_int4 (get_new_near_label_nr());

                if (header_size > 0)
                {
                  // remove again header offset from address
                  put_code (P_ADD_OFFSET);
                  put_int4 (- header_size);
                }

                output.exp.kind = EXP_AT_ADDR_OFFSET;
                output.exp.offset = 0;
              }
              else   // AN_ALLOCATOR + A_QUALIFIED_EXPRESSION (can be unconstrained or constrained with constant)
              {      //              + <any-expression>  (can have constant size or runtime size)
                PENTITY     type;
                ENTITY_KIND kind;

                type = complete_type_of (qual^.base_type_or_null);
                kind = type^.kind;

                if (kind == AN_ENUMERATION_TYPE || kind == AN_INTEGER_TYPE         || kind == A_FLOAT_TYPE ||
                    kind == A_POINTER_TYPE      || kind == A_FUNCTION_POINTER_TYPE || kind == AN_UNSAFE_POINTER_TYPE)
                {
                  int rc, size, align;

                  // constant size
                  rc = size_and_alignment_of_constant_size_exp (qual, out size, out align);
                  if (rc < 0)
                    fatal_compiler_error0 ("generate_code_for_expression(alloc12)");

                  _unused align;

                  // allocate simple value on heap (cannot be open)

                  put_code (P_CTE_4);
                  put_int4 (size);

                  put_code (P_MALLOC);
                  put_byte (0); //  <bool_fill_zeroes>


                  evaluate_and_assign_field (inner_exp, type, 0, ref frame);

                  output.exp.kind = EXP_AT_ADDR_OFFSET;
                  output.exp.offset = 0;
                }
                else if (kind == AN_OPEN_ARRAY_TYPE ||
                          (kind == A_STRUCT_TYPE && type^.the_struct_type.is_open_type))
                {
                  if (qual^.constraint.kind == CONSTANT_CONSTRAINT)   // result has constant size
                  {
                    int         rc, size, align;
                    EXP_INPUT   value_in;
                    EXP_OUTPUT  value_out;

                    // constant size
                    rc = size_and_alignment_of_constant_size_exp (qual, out size, out align);
                    if (rc < 0)
                      fatal_compiler_error0 ("generate_code_for_expression(alloc13)");

                    _unused align;

                    if (size > 2147483647 - header_size)
                      fatal_compiler_error0 ("generate_code_for_expression(alloc14)");

                    put_code (P_CTE_4);
                    put_int4 (size + header_size);

                    put_code (P_MALLOC);
                    put_byte (0); //  <bool_fill_zeroes>

                    if (header_size > 0)    // if open: fill header with constant
                    {
                      put_code (P_CTE_4);
                      put_int4 ((int)qual^.constraint.value);

                      put_code (P_STORE_FIELD_4);
                      put_int4 (0);
                    }


                    clear (value_in);
                    value_in.data_must_be_computed       = true;
                    value_in.constraint_must_be_computed = true;

                    // ! evaluate qualified expression + inner_expression !
                    // this includes any subtype check.

                    allocate_tombstone_anchor    (qual, ref value_in, ref frame);
                    generate_code_for_expression (qual, value_in, ref frame, out value_out);

                    if (value_out.constraint.place != CT_CTE)
                      fatal_compiler_error0 ("generate_code_for_expression(alloc15)");

                    flush_address (ref value_out);

                    put_code (P_STORE_FIELD_BLOCK);
                    put_int4 (header_size);
                    put_int4 (size);

                    release_tombstone_anchor (value_in);

                    output.exp.kind = EXP_AT_ADDR_OFFSET;
                    output.exp.offset = 0;
                  }
                  else     // result has runtime size
                  {
                    EXP_INPUT   value_in;
                    EXP_OUTPUT  value_out;
                    int4        temp_source_addr_offset, temp_target_addr_offset;
                    int4        temp_constraint_offset, temp_size_offset;

                    temp_source_addr_offset = allocate_temp_variable (address_size, ref frame);
                    temp_target_addr_offset = allocate_temp_variable (address_size, ref frame);
                    temp_constraint_offset  = allocate_temp_variable (4, ref frame);
                    temp_size_offset        = allocate_temp_variable (4, ref frame);

                    // temp to store source address
                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_source_addr_offset);

                    clear (value_in);
                    value_in.data_must_be_computed       = true;
                    value_in.constraint_must_be_computed = true;

                    // ! evaluate qualified expression + inner_expression !
                    // this includes any subtype check.

                    allocate_tombstone_anchor    (qual, ref value_in, ref frame);
                    generate_code_for_expression (qual, value_in, ref frame, out value_out);

                    // store constraint
                    flush_constraint (ref value_out);
                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_constraint_offset);
                    put_code (P_STORE_4);

                    // store source address
                    flush_address (ref value_out);
                    put_code (P_STORE_ADDR);

                    value_out.constraint.place       = CT_TEMP;
                    value_out.constraint.temp_offset = temp_constraint_offset;

                    convert_runtime_constraint_into_size_constraint (ref value_out);
                    flush_constraint (ref value_out);

                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_size_offset);
                    put_code (P_STORE_4);


                    // allocate heap object

                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_size_offset);
                    put_code (P_VALUE_4);

                    if (header_size > 0)
                    {
                      put_code (P_CTE_4);
                      put_int4 (header_size);
                      put_code (P_ADD4);
                    }

                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_target_addr_offset);

                    put_code (P_MALLOC);
                    put_byte (0); //  <bool_fill_zeroes>

                    put_code (P_STORE_ADDR);


                    // fill header
                    if (header_size > 0)
                    {
                      put_code (P_LOAD_LOCAL);
                      put_int4 (temp_target_addr_offset);
                      put_code (P_VALUE_ADDR);

                      put_code (P_LOAD_LOCAL);
                      put_int4 (temp_constraint_offset);
                      put_code (P_VALUE_4);

                      put_code (P_STORE_4);
                    }


                    // copy expression object into heap object

                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_target_addr_offset);
                    put_code (P_VALUE_ADDR);

                    put_code (P_ADD_OFFSET);
                    put_int4 (header_size);

                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_source_addr_offset);
                    put_code (P_VALUE_ADDR);

                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_size_offset);
                    put_code (P_VALUE_4);

                    put_code (P_COPY_BLOCK);

                    release_tombstone_anchor (value_in);

                    // result address

                    put_code (P_LOAD_LOCAL);
                    put_int4 (temp_target_addr_offset);
                    put_code (P_VALUE_ADDR);

                    output.exp.kind = EXP_AT_ADDR_OFFSET;
                    output.exp.offset = 0;
                  }
                }
                else   // any other struct/union type
                {
                  int rc, size, align;

                  // constant size
                  rc = size_and_alignment_of_constant_size_exp (qual, out size, out align);
                  if (rc < 0)
                    fatal_compiler_error0 ("generate_code_for_expression(alloc15)");

                  _unused align;

                  // allocate simple value on heap (cannot be open)

                  put_code (P_CTE_4);
                  put_int4 (size);

                  put_code (P_MALLOC);
                  put_byte (0); //  <bool_fill_zeroes>

                  evaluate_and_assign_field (inner_exp, type, 0, ref frame);

                  output.exp.kind = EXP_AT_ADDR_OFFSET;
                  output.exp.offset = 0;
                }
              }
            }
            break;

          case AN_ARRAY_QUALIFIED_EXPRESSION:     // has always a header, designated subtype is always open
            {
              PEXPRESSION inner_exp;
              PENTITY     element_subtype;
              int         rc, element_size, align;

              element_subtype = complete_type_of (complete_type_of (qual^.base_type_or_null)
                                                                        ^.the_open_array_type.element);

              rc = size_and_alignment_of_type (element_subtype, out element_size, out align);
              if (rc != 0)
                fatal_compiler_error0 ("generate_code_for_expression(AN_ARRAY_QUALIFIED_EXPRESSION-2)");

              _unused align;

              inner_exp = qual^.array_qualified_expression_info.value;

              if (inner_exp == null)     // AN_ALLOCATOR + AN_ARRAY_QUALIFIED_EXPRESSION(null)
              {
                EXP_INPUT   value_in;
                EXP_OUTPUT  value_out;
                PEXPRESSION exp;
                int4        temp_ofs;
                int8        saved_frame_offset;

                temp_ofs = allocate_temp_variable (4, ref frame);

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);

                saved_frame_offset = frame.frame_offset;

                exp = qual^.array_qualified_expression_info.length;

                clear (value_in);
                value_in.data_must_be_computed = true;

                allocate_tombstone_anchor    (exp, ref value_in, ref frame);
                generate_code_for_expression (exp, value_in, ref frame, out value_out);
                flush_simple_value (ref value_out, exp, type_uint);
                put_code (P_STORE_4);         // store at temp_ofs
                release_tombstone_anchor (value_in);
                frame.frame_offset = saved_frame_offset;    // reset frame offset

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);
                put_code (P_VALUE_4);

                if (array_checks_enabled)
                {
                  put_code (P_CTE_4);

                  if (element_size == 0)
                    put_int4 (1 + (2147483647 - header_size));                 // limit
                  else
                    put_int4 (1 + (2147483647 - header_size) / element_size);  // limit

                  put_code (P_CHECK_INDEX);     // if index >= limit -> error (uint4 comparison)
                  put_int4 (get_new_near_label_nr());
                }

                put_code (P_ARRAY_SIZE);
                put_int4 (element_size);

                put_code (P_CTE_4);
                put_int4 (header_size);
                put_code (P_ADD4);

                put_code (P_MALLOC);
                put_byte (1); //  <bool_fill_zeroes>

                // fill header

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);
                put_code (P_VALUE_4);

                put_code (P_STORE_FIELD_4);
                put_int4 (0);

                output.exp.kind = EXP_AT_ADDR_OFFSET;
                output.exp.offset = 0;
              }
              else if (inner_exp^.kind == AN_UNC_ARRAY_AGGREGATE)  // AN_ALLOCATOR + AN_ARRAY_QUALIFIED_EXPRESSION
              {                                                    //              + AN_UNC_ARRAY_AGGREGATE
                EXP_INPUT   value_in;
                EXP_OUTPUT  value_out;
                PEXPRESSION exp;
                int4        temp_ofs;
                int4        element_ofs;
                int8        saved_frame_offset;

                temp_ofs = allocate_temp_variable (4, ref frame);

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);


                saved_frame_offset = frame.frame_offset;

                exp = qual^.array_qualified_expression_info.length;

                clear (value_in);
                value_in.data_must_be_computed = true;

                allocate_tombstone_anchor    (exp, ref value_in, ref frame);
                generate_code_for_expression (exp, value_in, ref frame, out value_out);
                flush_simple_value (ref value_out, exp, type_uint);
                put_code (P_STORE_4);         // store at temp_ofs
                release_tombstone_anchor (value_in);

                frame.frame_offset = saved_frame_offset;    // reset frame offset


                // compute size

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);
                put_code (P_VALUE_4);

                if (array_checks_enabled)
                {
                  put_code (P_CTE_4);

                  if (element_size == 0)
                    put_int4 (1 + (2147483647 - header_size));                 // limit
                  else
                    put_int4 (1 + (2147483647 - header_size) / element_size);  // limit

                  put_code (P_CHECK_INDEX);     // if index >= limit -> error (uint4 comparison)
                  put_int4 (get_new_near_label_nr());
                }

                put_code (P_ARRAY_SIZE);
                put_int4 (element_size);

                put_code (P_CTE_4);
                put_int4 (header_size);
                put_code (P_ADD4);

                put_code (P_MALLOC);
                put_byte (0); //  <bool_fill_zeroes>

                // fill header

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);
                put_code (P_VALUE_4);

                put_code (P_STORE_FIELD_4);
                put_int4 (0);

                // add header offset to address
                put_code (P_ADD_OFFSET);
                put_int4 (header_size);


                // reserve element on stack

                element_subtype = inner_exp^.base_type_or_null^.the_open_array_type.element;

                rc = size_and_alignment_of_type (element_subtype, out element_size, out align);
                if (rc < 0)
                  fatal_compiler_error0 ("generate_code_for_expression(alloc10)");

                element_ofs = allocate_temp_variable (element_size, ref frame);


                // evaluate element expression and store it in element_ofs variable

                put_code (P_LOAD_LOCAL);
                put_int4 (element_ofs);

                evaluate_and_assign_field (    inner_exp^.unc_array_aggregate_info.element,
                                               element_subtype,
                                               0,
                                           ref frame);
                // note: address of temp_ofs remains on addr_stack

                // load constraint
                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);
                put_code (P_VALUE_4);

                put_code (P_MULTI_COPY);   // multi_copy (target, repeat, source, size);
                put_int4 (element_size);
                put_int4 (get_new_near_label_nr());
                put_int4 (get_new_near_label_nr());

                // remove again header offset from address
                put_code (P_ADD_OFFSET);
                put_int4 (- header_size);

                output.exp.kind = EXP_AT_ADDR_OFFSET;
                output.exp.offset = 0;
              }
              else
              {
                fatal_compiler_error0 ("generate_code_for_expression(alloc_array_qual");
              }
            }
            break;

          case A_STRUCT_QUALIFIED_EXPRESSION:     // has always a header, designated subtype is always open
            {
              PEXPRESSION inner_exp;

              inner_exp = qual^.struct_qualified_expression_info.value;

              if (inner_exp == null)    // AN_ALLOCATOR + A_STRUCT_QUALIFIED_EXPRESSION (null)
              {
                EXP_INPUT   value_in;
                EXP_OUTPUT  value_out;
                PEXPRESSION exp;
                int4        temp_ofs;
                int8        saved_frame_offset;

                temp_ofs = allocate_temp_variable (4, ref frame);

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);

                saved_frame_offset = frame.frame_offset;

                exp = qual^.struct_qualified_expression_info.discriminant;

                clear (value_in);
                value_in.data_must_be_computed = true;

                allocate_tombstone_anchor    (exp, ref value_in, ref frame);
                generate_code_for_expression (exp, value_in, ref frame, out value_out);
                flush_simple_value (ref value_out, exp, type_uint);
                put_code (P_STORE_4);         // store at temp_ofs
                release_tombstone_anchor (value_in);
                frame.frame_offset = saved_frame_offset;    // reset frame offset


                generate_size_table_for_open_struct (designated_subtype);  // check if size table has been generated before

                put_code (P_LOAD_CONST);
                put_int8 (serial_nr_of_pool_cte (designated_subtype^.the_struct_type.size_table));

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);
                put_code (P_VALUE_4);

                if (array_checks_enabled)
                {
                  // push length of size table
                  put_code (P_CTE_4);
                  put_int4 ((int)(designated_subtype^.the_struct_type.discriminant^.entities.first
                                                    ^.the_field.type
                                                    ^.the_enumeration_type.last + 1));
                  put_code (P_CHECK_INDEX);
                  put_int4 (get_new_near_label_nr());
                }

                put_code (P_ADD_INDEX);
                put_int4 (4);

                put_code (P_VALUE_4);      // aggregate size is now on int_stack

                // add header size

                put_code (P_CTE_4);
                put_int4 (header_size);
                put_code (P_ADD4);

                put_code (P_MALLOC);
                put_byte (1); //  <bool_fill_zeroes>

                // fill header

                put_code (P_LOAD_LOCAL);
                put_int4 (temp_ofs);
                put_code (P_VALUE_4);

                put_code (P_STORE_FIELD_4);
                put_int4 (0);

                output.exp.kind = EXP_AT_ADDR_OFFSET;
                output.exp.offset = 0;
              }
              else
              {
                fatal_compiler_error0 ("generate_code_for_expression(alloc_struct_qual");
              }
            }
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_expression(AN_ALLOCATOR)");
            break;
        }

        if (pointer_checks_enabled)
        {
          put_code (P_ALLOC_TOMB);
          put_int4 (heap_object_type_unique_nr (designated_subtype)); // decorated type of heap object (to fill)
        }

        frame.frame_offset = saved_frame_offset0;    // reset frame offset
      }
      break;

    case A_GLOBAL_VARIABLE_OBJECT:
      if (input.data_must_be_computed)
      {
        int     rc;
        int4    size, align;
        PENTITY glob;

        glob = e^.global_variable_object_info.pobject;

        if (glob^.the_global_variable.offset == 0)  // was never used before
        {
          rc = size_and_alignment_of_type (glob^.the_global_variable.type, out size, out align);
          if (rc != 0)
          {
            if (rc == -2)
              fatal_compiler_error0 ("global variable is too large");
            else
              fatal_compiler_error0 ("generate_code_for_expression(A_GLOBAL_VARIABLE_OBJECT)");
          }

          _unused align;

          glob^.the_global_variable.offset = allocate_global_variable (size);

          if (glob^.the_global_variable.initial_value_or_null != null)
            add_global_variable_to_list (glob);
        }

        put_code (P_LOAD_GLOBAL);  // <global_offset_8>       ; load addr on addr_stack
        put_int8 (glob^.the_global_variable.offset);

        output.exp.kind = EXP_AT_ADDR_OFFSET;
        output.exp.offset = 0;
      }

      if (input.constraint_must_be_computed)
      {
        if (e^.constraint.kind == DOES_NOT_APPLY)    // has no constraint
        {
          output.constraint.place = CT_NONE;
        }
        else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct
        {
          output.constraint.place = CT_CTE;
          output.constraint.cte = e^.constraint.value;
        }
        else
        {
          fatal_compiler_error0 ("generate_code_for_expression(A_GLOBAL_VARIABLE_OBJECT-2)");
        }
      }
      break;

    case A_LOCAL_VARIABLE_OBJECT:
      generate_code_for_local_variable (e^.local_variable_object_info.pobject, input, out output);
      break;

    case A_REFERENCE_OBJECT:
      if (input.data_must_be_computed)
      {
        put_code (P_LOAD_LOCAL);
        put_int4 (e^.reference_object_info.pobject
                   ^.the_reference.offset);

        output.exp.kind = EXP_AT_ADDR_INDIRECT;
      }

      if (input.constraint_must_be_computed)
      {
        if (e^.constraint.kind == DOES_NOT_APPLY)    // has no constraint
        {
          output.constraint.place = CT_NONE;
        }
        else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct (1)
        {
          output.constraint.place = CT_CTE;
          output.constraint.cte = e^.constraint.value;
        }
        else if (e^.constraint.kind == RUNTIME_CONSTRAINT) // array or open struct (2)
        {
          output.constraint.place = CT_TEMP;
          output.constraint.temp_offset =
                 e^.reference_object_info.pobject
                  ^.the_reference.offset + address_size;
        }
        else
        {
          fatal_compiler_error0 ("generate_code_for_expression(A_REFERENCE_OBJECT)");
        }
      }
      break;

    case A_PARAMETER_OBJECT:
      {
        ENTITY_KIND k = complete_type_of
               (e^.parameter_object_info.pobject^.the_parameter.type)
              ^.kind;

        if (input.data_must_be_computed)
        {
          put_code (P_LOAD_LOCAL);
          put_int4 (e^.parameter_object_info.pobject
                     ^.the_parameter.offset);

          if (e^.parameter_object_info.pobject^.the_parameter.mode != MODE_IN ||
              k == AN_OPEN_ARRAY_TYPE || k == AN_ARRAY_TYPE ||
              k == A_STRUCT_TYPE      || k == A_CONSTRAINED_STRUCT_TYPE ||
              k == A_UNION_TYPE)
          {
            output.exp.kind = EXP_AT_ADDR_INDIRECT;
          }
          else
          {
            output.exp.kind = EXP_AT_ADDR_OFFSET;
            output.exp.offset = 0;
          }
        }

        if (input.constraint_must_be_computed)
        {
          if (e^.constraint.kind == DOES_NOT_APPLY)    // has no constraint
          {
            output.constraint.place = CT_NONE;
          }
          else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct (1)
          {
            output.constraint.place = CT_CTE;
            output.constraint.cte = e^.constraint.value;
          }
          else if (e^.constraint.kind == RUNTIME_CONSTRAINT) // array or open struct (2)
          {
            output.constraint.place = CT_TEMP;
            output.constraint.temp_offset =
                   e^.parameter_object_info.pobject
                    ^.the_parameter.offset + address_size;
          }
          else
          {
            fatal_compiler_error0 ("generate_code_for_expression(A_PARAMETER_OBJECT)");
          }
        }
      }
      break;

    case AN_ARRAY_ELEMENT_OBJECT:
      {
        EXP_INPUT   prefix_in, index_in;
        EXP_OUTPUT  prefix_out, index_out;
        int         rc, element_size, align;
        PEXPRESSION exp_index;

        clear (prefix_in);
        prefix_in.data_must_be_computed       = true;
        prefix_in.constraint_must_be_computed = true;  // no constraint anyway if access value

        // evaluate array object or unsafe pointer value
        prefix_in.tomb = input.tomb;
        generate_code_for_expression (    e^.array_element_object_info.prefix,
                                          prefix_in,
                                      ref frame,
                                      out prefix_out);


        // element type size or designated type size is constant (element type can be jagged !)
        rc = size_and_alignment_of_constant_size_exp (e, out element_size, out align);
        if (rc != 0)
          fatal_compiler_error0 ("generate_code_for_expression(AN_ARRAY_ELEMENT_OBJECT1)");

        _unused align;

        exp_index = e^.array_element_object_info.index;


        // prefix base type is either AN_OPEN_ARRAY_TYPE or AN_UNSAFE_POINTER_TYPE.

        if (prefix_out.constraint.place == CT_NONE)    // unsafe-access-object or value
        {
          int8 saved_frame_offset;

          saved_frame_offset = frame.frame_offset;

          clear (index_in);
          index_in.data_must_be_computed = true;

          // evaluate index value (type enum or integer, but not long)
          allocate_tombstone_anchor    (exp_index, ref index_in, ref frame);
          generate_code_for_expression (exp_index, index_in, ref frame, out index_out);
          discard_constraint (ref prefix_out);


          if (index_out.exp.kind == EXP_CONST_INT)   // constant index
          {
            uint4 offset;

            // flush unsafe-access value
            flush_simple_value (ref prefix_out,
                                    e^.array_element_object_info.prefix,
                                    e^.array_element_object_info.prefix^.base_type_or_null);

            offset = (uint)index_out.exp.value * (uint)element_size;

            if (offset != 0)
            {
              put_code (P_ADD_OFFSET);
              put_int4 ((int)offset);
            }
          }
          else     // non-constant index
          {
            // flush index on int_stack
            flush_simple_value (ref index_out, exp_index, type_int);

            // flush unsafe-access value
            flush_simple_value (ref prefix_out,
                                    e^.array_element_object_info.prefix,
                                    e^.array_element_object_info.prefix^.base_type_or_null);

            put_code (P_ADD_PTR_OFFSET);
            put_int4 (element_size);

            release_tombstone_anchor (index_in);
          }

          frame.frame_offset = saved_frame_offset;    // reset frame offset
        }
        else     // array type
        {
          int8 saved_frame_offset;

          saved_frame_offset = frame.frame_offset;

          if (prefix_out.constraint.place == CT_ON_INT_STACK)
          {
            int4 temp_ofs;

            // we must store int_stack value in some temp,
            // because it must not be on int_stack for next pcode.

            temp_ofs = allocate_temp_variable (4, ref frame);

            put_code (P_LOAD_LOCAL);
            put_int4 (temp_ofs);

            put_code (P_STORE_4);

            prefix_out.constraint.place = CT_TEMP;
            prefix_out.constraint.temp_offset = temp_ofs;
          }


          clear (index_in);
          index_in.data_must_be_computed = true;

          // evaluate index value (type enum or integer, but not long)
          allocate_tombstone_anchor    (exp_index, ref index_in, ref frame);
          generate_code_for_expression (exp_index, index_in, ref frame, out index_out);

          if (index_out.exp.kind == EXP_CONST_INT &&
              prefix_out.constraint.place == CT_CTE)   // both constants
          {
            uint4 offset;

            offset = (uint)index_out.exp.value * (uint)element_size;

            discard_constraint (ref prefix_out);
            flush_address (ref prefix_out);

            if (offset != 0)
            {
              put_code (P_ADD_OFFSET);
              put_int4 ((int)offset);
            }
          }
          else
          {
            // flush index on int_stack
            flush_simple_value (ref index_out, exp_index, type_int);

            if (array_checks_enabled)
            {
              // push prefix constraint on int_stack (length)
              flush_constraint (ref prefix_out);

              put_code (P_CHECK_INDEX);
              put_int4 (get_new_near_label_nr());
            }
            else
            {
              discard_constraint (ref prefix_out);
            }

            flush_address (ref prefix_out);

            put_code (P_ADD_INDEX);
            put_int4 (element_size);

            release_tombstone_anchor (index_in);
          }

          frame.frame_offset = saved_frame_offset;    // reset frame offset

        }  // end array type
      }


      if (e^.constraint.kind == RUNTIME_CONSTRAINT) // jagged array or open struct
      {
        output.exp.kind = EXP_AT_ADDR_INDIRECT;
      }
      else
      {
        output.exp.kind = EXP_AT_ADDR_OFFSET;
        output.exp.offset = 0;
      }


      if (input.constraint_must_be_computed)
      {
        if (e^.constraint.kind == DOES_NOT_APPLY)    // has no constraint
        {
          output.constraint.place = CT_NONE;
        }
        else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct (1)
        {
          output.constraint.place = CT_CTE;
          output.constraint.cte = e^.constraint.value;
        }
        else if (e^.constraint.kind == RUNTIME_CONSTRAINT) // jagged array or open struct (2)
        {
          output.constraint.place = CT_AT_ADDR_OFFSET;
          output.constraint.offset = address_size;      // constraint after address
        }
        else
        {
          fatal_compiler_error0 ("generate_code_for_expression(AN_ARRAY_ELEMENT_OBJECT2)");
        }
      }
      break;

    case AN_ARRAY_SLICE_OBJECT:
      {
        EXP_INPUT  prefix_in, offset_in, length_in;
        EXP_OUTPUT prefix_out, offset_out, length_out;
        int        rc, element_size, align;
        PEXPRESSION exp_offset, exp_length;

        clear (prefix_in);
        prefix_in.data_must_be_computed       = true;
        prefix_in.constraint_must_be_computed = true;  // no constraint anyway if access value

        // evaluate array object or unsafe pointer value
        prefix_in.tomb = input.tomb;
        generate_code_for_expression (    e^.array_slice_object_info.prefix,
                                          prefix_in,
                                      ref frame,
                                      out prefix_out);


        // element size or designated type has constant size,
        // element type can be jagged !
        rc = size_and_alignment_of_type (e^.base_type_or_null^.the_open_array_type.element,
                                         out element_size, out align);
        if (rc != 0)
          fatal_compiler_error0 ("generate_code_for_expression(AN_ARRAY_SLICE_OBJECT1)");

        _unused align;

        exp_offset = e^.array_slice_object_info.index;
        exp_length = e^.array_slice_object_info.length;


        // prefix base type is either AN_OPEN_ARRAY_TYPE or AN_UNSAFE_POINTER_TYPE.

        if (prefix_out.constraint.place == CT_NONE)    // unsafe-access-object or value
        {
          int8 saved_frame_offset;

          saved_frame_offset = frame.frame_offset;

          discard_constraint (ref prefix_out);

          clear (offset_in);
          offset_in.data_must_be_computed = true;

          // evaluate offset value (type enum or integer, but not long)
          allocate_tombstone_anchor    (exp_offset, ref offset_in, ref frame);
          generate_code_for_expression (exp_offset, offset_in, ref frame, out offset_out);

          if (offset_out.exp.kind == EXP_CONST_INT)   // constant offset
          {
            uint4 offset;

            // flush unsafe-access value
            flush_simple_value (ref prefix_out,
                                    e^.array_slice_object_info.prefix,
                                    e^.array_slice_object_info.prefix^.base_type_or_null);

            offset = (uint)offset_out.exp.value * (uint)element_size;

            if (offset != 0)
            {
              put_code (P_ADD_OFFSET);
              put_int4 ((int)offset);
            }
          }
          else     // non-constant offset
          {
            // flush offset on int_stack
            flush_simple_value (ref offset_out, exp_offset, type_int);

            // flush unsafe-access value
            flush_simple_value (ref prefix_out,
                                    e^.array_slice_object_info.prefix,
                                    e^.array_slice_object_info.prefix^.base_type_or_null);

            put_code (P_ADD_PTR_OFFSET);
            put_int4 (element_size);

            release_tombstone_anchor (offset_in);
          }


          output.exp.kind = EXP_AT_ADDR_OFFSET;
          output.exp.offset = 0;


          // evaluate length value (type enum or integer, but not long)

          clear (length_in);
          length_in.data_must_be_computed = true;

          allocate_tombstone_anchor    (exp_length, ref length_in, ref frame);
          generate_code_for_expression (exp_length, length_in, ref frame, out length_out);

          if (length_in.tomb.anchor_provided != _NO)
          {
            flush_simple_value (ref length_out, exp_length, type_int);
            put_code (P_FORCE_4);
            release_tombstone_anchor (length_in);
            frame.frame_offset = saved_frame_offset;    // reset frame offset
          }

          convert_exp_into_constraint (ref length_out, exp_length, ref output);

          frame.frame_offset = saved_frame_offset;    // reset frame offset
        }
        else     // array type
        {
          int  case_nr;
          int8 saved_frame_offset;
          int4 temp2_ofs = 0;


          // there are 4 different cases :

          if (prefix_out.constraint.place == CT_CTE &&
              e^.array_slice_object_info.index^.kind == A_CONST_INTEGER_VALUE &&
              e^.array_slice_object_info.length^.kind == A_CONST_INTEGER_VALUE)
          {
            // case a) offset, len and length are constant
            case_nr = 1;
          }
          else if (prefix_out.constraint.place == CT_CTE &&
                   e^.array_slice_object_info.length^.kind == A_CONST_INTEGER_VALUE)
          {
            // case b) len and length are constant
            case_nr = 2;
          }
          else if (e^.array_slice_object_info.length^.kind == A_CONST_INTEGER_VALUE)
          {
            // case c) len is constant
            case_nr = 3;
          }
          else
          {
            // case d) general case
            case_nr = 4;

            // allocate temp for result constraint (outside this frame)
            temp2_ofs = allocate_temp_variable (4, ref frame);
          }



          saved_frame_offset = frame.frame_offset;


          if (prefix_out.constraint.place == CT_ON_INT_STACK)
          {
            int4 temp_ofs;

            // we must store int_stack value in some temp,
            // because it must not be on int_stack for next pcode.

            temp_ofs = allocate_temp_variable (4, ref frame);

            put_code (P_LOAD_LOCAL);
            put_int4 (temp_ofs);

            put_code (P_STORE_4);

            prefix_out.constraint.place = CT_TEMP;
            prefix_out.constraint.temp_offset = temp_ofs;
          }



          // there are 4 different cases :

          switch (case_nr)
          {
            case 1:      // case a) offset, len and length are constant
            {
              uint4 offset;

              flush_address (ref prefix_out);

              offset = (uint)e^.array_slice_object_info.index
                              ^.const_integer_value_info.value
                     * (uint)element_size;

              if (offset != 0)
              {
                put_code (P_ADD_OFFSET);
                put_int4 ((int)offset);
              }

              output.exp.kind = EXP_AT_ADDR_OFFSET;
              output.exp.offset = 0;

              output.constraint.place = CT_CTE;
              output.constraint.cte   = (uint4)(int4)e^.array_slice_object_info.length
                                                       ^.const_integer_value_info.value;
            }
            break;

            case 2:            // case b) len and length are constant
            {
              // evaluate offset

              clear (offset_in);
              offset_in.data_must_be_computed = true;

              allocate_tombstone_anchor    (exp_offset, ref offset_in, ref frame);
              generate_code_for_expression (exp_offset, offset_in, ref frame, out offset_out);
              flush_simple_value (ref offset_out, exp_offset, type_int);

              if (offset_in.tomb.anchor_provided != _NO)
              {
                put_code (P_FORCE_4);
                release_tombstone_anchor (offset_in);
              }

              if (array_checks_enabled)
              {
                put_code (P_CHECK_SLICE_0);   //  <(length-len)_uint4>  <near_label4>
                put_int4 ((int4)prefix_out.constraint.cte
                           - (int4)e^.array_slice_object_info.length
                                    ^.const_integer_value_info.value);
                put_int4 (get_new_near_label_nr());
              }

              flush_address (ref prefix_out);

              put_code (P_ADD_INDEX);
              put_int4 (element_size);

              output.exp.kind = EXP_AT_ADDR_OFFSET;
              output.exp.offset = 0;

              output.constraint.place = CT_CTE;
              output.constraint.cte   = (uint4)(int4)e^.array_slice_object_info.length
                                                       ^.const_integer_value_info.value;
            }
            break;

            case 3:    // case c) len is constant
            {
              // evaluate offset

              clear (offset_in);
              offset_in.data_must_be_computed = true;

              allocate_tombstone_anchor    (exp_offset, ref offset_in, ref frame);
              generate_code_for_expression (exp_offset, offset_in, ref frame, out offset_out);
              flush_simple_value (ref offset_out, exp_offset, type_int);

              if (offset_in.tomb.anchor_provided != _NO)
              {
                put_code (P_FORCE_4);
                release_tombstone_anchor (offset_in);
              }


              // flush length

              if (array_checks_enabled)
              {
                flush_constraint (ref prefix_out);

                put_code (P_CHECK_SLICE_1);   //  <(len)_uint4> <near_label4>
                put_int4 ((int4)e^.array_slice_object_info.length
                                 ^.const_integer_value_info.value);
                put_int4 (get_new_near_label_nr());
              }
              else
              {
                discard_constraint (ref prefix_out);
              }

              flush_address (ref prefix_out);

              put_code (P_ADD_INDEX);
              put_int4 (element_size);

              output.exp.kind = EXP_AT_ADDR_OFFSET;
              output.exp.offset = 0;

              output.constraint.place = CT_CTE;
              output.constraint.cte   = (uint4)(int4)e^.array_slice_object_info.length
                                                       ^.const_integer_value_info.value;
            }
            break;

            case 4:    // case d) general case
            {
              // evaluate offset

              clear (offset_in);
              offset_in.data_must_be_computed = true;

              allocate_tombstone_anchor    (exp_offset, ref offset_in, ref frame);
              generate_code_for_expression (exp_offset, offset_in, ref frame, out offset_out);
              flush_simple_value (ref offset_out, exp_offset, type_int);

              if (offset_in.tomb.anchor_provided != _NO)
              {
                put_code (P_FORCE_4);
                release_tombstone_anchor (offset_in);
              }

              // evaluate len

              clear (length_in);
              length_in.data_must_be_computed = true;

              allocate_tombstone_anchor    (exp_length, ref length_in, ref frame);
              generate_code_for_expression (exp_length, length_in, ref frame, out length_out);
              flush_simple_value (ref length_out, exp_length, type_int);

              if (length_in.tomb.anchor_provided != _NO)
              {
                put_code (P_FORCE_4);
                release_tombstone_anchor (length_in);
              }


              // store len into some temp

              put_code (P_LOAD_LOCAL);
              put_int4 (temp2_ofs);

              put_code (P_STORE_4);


              // flush length

              if (array_checks_enabled)
              {
                flush_constraint (ref prefix_out);

                put_code (P_CHECK_SLICE_2);   //  <(len)_uint4>  <near_label4>
                put_int4 (temp2_ofs);
                put_int4 (get_new_near_label_nr());
              }
              else
              {
                discard_constraint (ref prefix_out);
              }

              flush_address (ref prefix_out);

              put_code (P_ADD_INDEX);
              put_int4 (element_size);

              output.exp.kind = EXP_AT_ADDR_OFFSET;
              output.exp.offset = 0;

              if (input.constraint_must_be_computed)
              {
                output.constraint.place       = CT_TEMP;
                output.constraint.temp_offset = temp2_ofs;
              }
            }
            break;

            default:
              abort;
          }

          frame.frame_offset = saved_frame_offset;    // reset frame offset

        }  // end array type
      }
      break;

    case A_STRUCT_FIELD_OBJECT:    // struct or union field
      {
        PEXPRESSION prefix;
        PENTITY     field, prefix_type, f, field_type;
        uint4       discriminant_value = 0;
        FIELD_DATA  data;
        EXP_INPUT   prefix_in;
        EXP_OUTPUT  prefix_out;

        prefix = e^.struct_field_object_info.prefix;
        field = e^.struct_field_object_info.field;

        prefix_type = complete_type_of (prefix^.base_type_or_null);
        if (prefix_type^.kind == A_STRUCT_TYPE)
          f = prefix_type^.the_struct_type.fields^.entities.first;
        else
          f = prefix_type^.the_union_type.fields^.entities.first;

        if (field^.kind == A_VARYING_FIELD)
          discriminant_value = field^.the_varying_field.discriminant_value;

        begin_struct (out data, prefix_type);

        for (;;)
        {
          skip_to_valid_field (ref f, out field_type, discriminant_value);
          _unused field_type;

          begin_field (ref data, f);

          if (f == field)      // found
            break;

          end_field (ref data);

          f = f^.next;
        }

        clear (prefix_in);
        prefix_in.data_must_be_computed = true;
        if (field^.kind == A_VARYING_FIELD)
          prefix_in.constraint_must_be_computed = true;

        prefix_in.tomb = input.tomb;
        generate_code_for_expression (prefix, prefix_in, ref frame, out prefix_out);

        if (field^.kind == A_VARYING_FIELD && prefix^.constraint.kind == RUNTIME_CONSTRAINT)
        {
          if (array_checks_enabled)
          {
            flush_constraint (ref prefix_out);
            put_code (P_CHECK_SAME_1);
            put_int4 ((int)discriminant_value);
            put_int4 (get_new_near_label_nr());
          }
          else
          {
            discard_constraint (ref prefix_out);
          }
        }
        else
        {
          discard_constraint (ref prefix_out);
        }

        if (prefix_out.exp.kind != EXP_AT_ADDR_OFFSET)
          flush_address (ref prefix_out);

        if (prefix_type^.kind == A_STRUCT_TYPE)   // only for struct, not for union
          prefix_out.exp.offset += data.field_offset;


        // compute output

        output.exp = prefix_out.exp;

        if (e^.constraint.kind == RUNTIME_CONSTRAINT) // jagged array or open struct
        {
          flush_address (ref output);
          output.exp.kind = EXP_AT_ADDR_INDIRECT;
        }

        if (input.constraint_must_be_computed)
        {
          if (e^.constraint.kind == DOES_NOT_APPLY)    // has no constraint
          {
            output.constraint.place = CT_NONE;
          }
          else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct (1)
          {
            output.constraint.place = CT_CTE;
            output.constraint.cte = e^.constraint.value;
          }
          else if (e^.constraint.kind == RUNTIME_CONSTRAINT) // jagged array or open struct (2)
          {
            output.constraint.place = CT_AT_ADDR_OFFSET;
            output.constraint.offset = address_size;      // constraint after address
          }
          else
          {
            fatal_compiler_error0 ("generate_code_for_expression(A_STRUCT_FIELD_OBJECT)");
          }
        }
      }
      break;

    case A_DEREFERENCED_OBJECT:
      {
        EXP_INPUT   prefix_in;
        EXP_OUTPUT  prefix_out;
        PEXPRESSION exp;
        PENTITY     designated_subtype;
        int8        saved_frame_offset;

        saved_frame_offset = frame.frame_offset;

        exp = e^.dereferenced_object_info.prefix;


        clear (prefix_in);
        prefix_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
        generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

        // result is an address on addr_stack; either an object (variable, ..) or a value (function return value, ..)

        discard_constraint (ref prefix_out);
        flush_address (ref prefix_out);
        convert_addr_into_simple_value (ref prefix_out, exp);    // convert object into ptr

        if (prefix_in.tomb.anchor_provided != _NO)
        {
          put_code (P_FORCE_ADDR);
          release_tombstone_anchor (prefix_in);
          frame.frame_offset = saved_frame_offset;    // reset frame offset
        }

        if (pointer_checks_enabled)
        {
          put_code (P_DEREF);   // <type4>  <local_addr4>  <near_label4> <near_label4>  (  <pointer_addr>  -->  <heap_object_addr>  )

          designated_subtype = exp^.base_type_or_null^.the_pointer_type.designated_type;

          put_int4 (heap_object_type_unique_nr (designated_subtype)); // decorated type of heap object (to fill)

          if (input.tomb.anchor_provided != _YES)
            fatal_compiler_error0 ("generate_code_for_expression(A_DEREFERENCED_OBJECT) : missing tombstone anchor");
          put_int4 (input.tomb.anchor_offset);    // anchor variable where we must store the pointer value

          put_int4 (get_new_near_label_nr());
          put_int4 (get_new_near_label_nr());
        }
      }


      output.exp.kind = EXP_AT_ADDR_OFFSET;

      if (e^.constraint.kind == RUNTIME_CONSTRAINT) // open array / open struct (2)
      {
        output.exp.offset = access_object_header_size_for_open_type (output.base_type);
      }
      else
      {
        output.exp.offset = 0;
      }

      if (input.constraint_must_be_computed)
      {
        if (e^.constraint.kind == DOES_NOT_APPLY)    // has no constraint
        {
          output.constraint.place = CT_NONE;
        }
        else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct (1)
        {
          output.constraint.place = CT_CTE;
          output.constraint.cte = e^.constraint.value;
        }
        else if (e^.constraint.kind == RUNTIME_CONSTRAINT) // open array / open struct (2)
        {
          output.constraint.place = CT_AT_ADDR_OFFSET;
          output.constraint.offset = 0;
        }
        else
        {
          fatal_compiler_error0 ("generate_code_for_expression(A_DEREFERENCED_OBJECT)");
        }
      }
      break;

    case AN_UNSAFE_DEREFERENCED_OBJECT:     // prefix is unsafe_pointer value (is never null)
      {
        EXP_INPUT   prefix_in;
        EXP_OUTPUT  prefix_out;
        PEXPRESSION exp;
        int8        saved_frame_offset;

        saved_frame_offset = frame.frame_offset;

        exp = e^.unsafe_dereferenced_object_info.unsafe_ptr_value;

        clear (prefix_in);
        prefix_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
        generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

        // result is an address on addr_stack; either an object (variable, ..) or a value (function return value, ..)

        discard_constraint (ref prefix_out);
        flush_address (ref prefix_out);
        convert_addr_into_simple_value (ref prefix_out, exp);    // convert object into ptr

        if (prefix_in.tomb.anchor_provided != _NO)
        {
          put_code (P_FORCE_ADDR);
          release_tombstone_anchor (prefix_in);
          frame.frame_offset = saved_frame_offset;    // reset frame offset
        }

        output.exp.kind = EXP_AT_ADDR_OFFSET;
        output.exp.offset = 0;
      }

      if (input.constraint_must_be_computed)
      {
        if (e^.constraint.kind == DOES_NOT_APPLY)    // has no constraint
        {
          output.constraint.place = CT_NONE;
        }
        else if (e^.constraint.kind == CONSTANT_CONSTRAINT) // array or open struct
        {
          output.constraint.place = CT_CTE;
          output.constraint.cte = e^.constraint.value;
        }
        else
        {
          fatal_compiler_error0 ("generate_code_for_expression(AN_UNSAFE_DEREFERENCED_OBJECT)");
        }
      }
      break;

    case AN_ATTR_BYTE_OBJECT:
      {
        EXP_INPUT  prefix_in;
        EXP_OUTPUT prefix_out;

        clear (prefix_in);
        prefix_in.data_must_be_computed       = input.data_must_be_computed;
        prefix_in.constraint_must_be_computed = input.constraint_must_be_computed;

        prefix_in.tomb = input.tomb;
        generate_code_for_expression (    e^.attr_byte_object_info.prefix,
                                          prefix_in,
                                      ref frame,
                                      out prefix_out);

        if (input.constraint_must_be_computed)
        {
          if (e^.constraint.kind == CONSTANT_CONSTRAINT)    // result has constant constraint
          {
            discard_constraint (ref prefix_out);

            output.constraint.place = CT_CTE;
            output.constraint.cte = e^.constraint.value;
          }
          else    // result has runtime constraint
          {
            convert_runtime_constraint_into_size_constraint (ref prefix_out);
            output.constraint = prefix_out.constraint;
          }
        }
        else
        {
          discard_constraint (ref prefix_out);
        }

        if (input.data_must_be_computed)
        {
          output.exp = prefix_out.exp;
        }
        else
        {
          discard_address (ref prefix_out);
        }
      }
      break;

    case A_BOXED_OBJECT:  // convert a variable of constant or runtime size into an array of byte
      {
        PEXPRESSION exp;
        PENTITY     type;
        ENTITY_KIND k;
        EXP_INPUT   prefix_in;
        EXP_OUTPUT  prefix_out;
        int         rc, size, align;
        int4        temp_ofs;
        int8        saved_frame_offset;

        exp = e^.boxed_object_info.parameter;

        type = complete_type_of (e^.boxed_object_info.parameter^.base_type_or_null);
        k = type^.kind;

        if (e^.access == ACCESS_READONLY &&      // parameter of mode 'in' of simple type
            (k == AN_INTEGER_TYPE || k == A_FLOAT_TYPE            || k == AN_ENUMERATION_TYPE ||
             k == A_POINTER_TYPE  || k == A_FUNCTION_POINTER_TYPE || k == AN_UNSAFE_POINTER_TYPE))
        {
          // prepare temp variable for storing result value

          rc = size_and_alignment_of_type (type, out size, out align);
          if (rc != 0)
            fatal_compiler_error0 ("generate_code_for_expression(A_BOXED_OBJECT)(1)");

          _unused align;

          temp_ofs = allocate_temp_variable (size, ref frame);

          put_code (P_LOAD_LOCAL);
          put_int4 (temp_ofs);


          // evaluate and flush value


          saved_frame_offset = frame.frame_offset;

          clear (prefix_in);
          prefix_in.data_must_be_computed       = true;
          prefix_in.constraint_must_be_computed = true;

          allocate_tombstone_anchor    (exp, ref prefix_in, ref frame);
          generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);
          flush_simple_value (ref prefix_out, exp, type);
          discard_constraint (ref prefix_out);

          store_simple_value (type);

          release_tombstone_anchor (prefix_in);
          frame.frame_offset = saved_frame_offset;    // reset frame offset

          if (input.data_must_be_computed)
          {
            put_code (P_LOAD_LOCAL);
            put_int4 (temp_ofs);

            output.exp.kind = EXP_AT_ADDR_OFFSET;
            output.exp.offset = 0;
          }

          if (input.constraint_must_be_computed)
          {
            output.constraint.place = CT_CTE;
            output.constraint.cte = (uint)size;
          }
        }
        else if (e^.access != ACCESS_READONLY ||    // parameter of mode 'out' or 'ref', or compound type
                 k == AN_OPEN_ARRAY_TYPE || k == A_STRUCT_TYPE || k == A_UNION_TYPE)
        {
          // compute address and size of object

          clear (prefix_in);
          prefix_in.data_must_be_computed       = input.data_must_be_computed;
          prefix_in.constraint_must_be_computed = input.constraint_must_be_computed;

          prefix_in.tomb = input.tomb;
          generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

          if (input.constraint_must_be_computed)
          {
            if (e^.constraint.kind == CONSTANT_CONSTRAINT)    // result has constant constraint
            {
              discard_constraint (ref prefix_out);

              output.constraint.place = CT_CTE;
              output.constraint.cte = e^.constraint.value;
            }
            else    // result has runtime constraint
            {
              convert_runtime_constraint_into_size_constraint (ref prefix_out);
              output.constraint = prefix_out.constraint;
            }
          }
          else
          {
            discard_constraint (ref prefix_out);
          }

          if (input.data_must_be_computed)
          {
            output.exp = prefix_out.exp;
          }
          else
          {
            discard_address (ref prefix_out);
          }
        }
        else
        {
          fatal_compiler_error0 ("generate_code_for_expression(A_BOXED_OBJECT)(2)");
        }
      }
      break;

    case AN_UNBOXED_OBJECT:     // convert from byte[] into any packed type
      {
        PEXPRESSION exp;
        PENTITY     result_type, element_type;
//        ENTITY_KIND k;
        EXP_INPUT   prefix_in;
        EXP_OUTPUT  prefix_out;
        int         rc, result_size, element_size, align;

        exp = e^.unboxed_object_info.parameter;

        result_type = complete_type_of (e^.base_type_or_null);
//        k = result_type^.kind;

        if (e^.unboxed_object_info.to_open_array)    // convert to unconstrained open array
        {
          // compute address and size of object

          clear (prefix_in);
          prefix_in.data_must_be_computed       = true;
          prefix_in.constraint_must_be_computed = true;

          prefix_in.tomb = input.tomb;
          generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

          element_type = complete_type_of (result_type^.the_open_array_type.element);

          // returns 0 if OK, -1 if bad type, -2 if size is too large
          rc = size_and_alignment_of_type (element_type, out element_size, out align);
          if (rc < 0)
            fatal_compiler_error0 ("generate_code_for_expression(AN_UNBOXED_OBJECT)(1)");

          _unused align;

          if (input.constraint_must_be_computed)
          {
            if (e^.constraint.kind == CONSTANT_CONSTRAINT)    // result has constant constraint
            {
              discard_constraint (ref prefix_out);

              output.constraint.place = CT_CTE;
              output.constraint.cte = e^.constraint.value;
            }
            else    // result has runtime constraint
            {
              flush_constraint (ref prefix_out);

              if (element_size != 1)
              {
                put_code (P_CTE_4);              // divide by array element size
                put_int4 (element_size);
                put_code (P_UDIV4);
              }

              output.constraint = prefix_out.constraint;
            }
          }
          else
          {
            discard_constraint (ref prefix_out);
          }

          if (input.data_must_be_computed)
          {
            output.exp = prefix_out.exp;
          }
          else
          {
            discard_address (ref prefix_out);
          }
        }
        else       // result is a constrained type
        {
          // compute address and size of object

          clear (prefix_in);
          prefix_in.data_must_be_computed       = true;
          prefix_in.constraint_must_be_computed = true;

          prefix_in.tomb = input.tomb;
          generate_code_for_expression (exp, prefix_in, ref frame, out prefix_out);

          // returns 0 if OK, -1 if bad type, -2 if size is too large
          rc = size_and_alignment_of_constant_size_exp (e, out result_size, out align);
          if (rc < 0)
            fatal_compiler_error0 ("generate_code_for_expression(AN_UNBOXED_OBJECT)(2)");

          if (prefix_out.constraint.place != CT_CTE)   // runtime constraint
          {
            // check that constraint matches the result type size

            if (array_checks_enabled)
            {
              flush_constraint (ref prefix_out);
              put_code (P_CHECK_SAME_1);
              put_int4 (result_size);
              put_int4 (get_new_near_label_nr());
            }
            else
            {
              discard_constraint (ref prefix_out);
            }
          }

          if (input.constraint_must_be_computed)
          {
            if (e^.constraint.kind == CONSTANT_CONSTRAINT)    // result has constant constraint
            {
              output.constraint.place = CT_CTE;
              output.constraint.cte = e^.constraint.value;
            }
          }

          if (input.data_must_be_computed)
          {
            output.exp = prefix_out.exp;
          }
          else
          {
            discard_address (ref prefix_out);
          }
        }
      }
      break;

    case A_BOXED_ARRAY_OBJECT:  // an array of boxed objects
      {
        uint                 count;
        int                  i;
        int4                 temp_ofs;
        LIST_OF_EXPRESSIONS^ list;
        EXP_INPUT            prefix_in;
        EXP_OUTPUT           prefix_out;

        // allocate boxing array on stack
        count = e^.constraint.value;
        temp_ofs = allocate_temp_variable ((address_size*2) * (int)count, ref frame);

        // evaluate all parameters and assign them to boxing array
        list = e^.boxed_array_object_info.list;
        i = 0;
        while (list != null)
        {
          put_code (P_LOAD_LOCAL);                     // for address
          put_int4 (temp_ofs + (address_size*2) * i);

          clear (prefix_in);
          prefix_in.data_must_be_computed       = true;
          prefix_in.constraint_must_be_computed = true;

          prefix_in.tomb = list^.tomb;
          generate_code_for_expression (list^.exp, prefix_in, ref frame, out prefix_out);

          put_code (P_LOAD_LOCAL);                     // for constraint
          put_int4 (temp_ofs + (address_size*2) * i + address_size);

          // flush constraint needs latest object's address
          flush_constraint_over_address (ref prefix_out);
          put_code (P_STORE_4);

          flush_address (ref prefix_out);
          put_code (P_STORE_ADDR);

          list = list^.next;
          i++;
        }

        put_code (P_LOAD_LOCAL);
        put_int4 (temp_ofs);

        output.exp.kind = EXP_AT_ADDR_OFFSET;
        output.exp.offset = 0;
        output.constraint.place = CT_CTE;
        output.constraint.cte = count;
      }
      break;

    default:
      fatal_compiler_error0 ("generate_code_for_expression(unknown)");
      break;
  }

  output.base_type = e^.base_type_or_null;
}

/*****************************************************************************/

bool exp_is_aggregate (PEXPRESSION exp)
{
  PEXPRESSION e;

  e = exp;
  while (e^.kind == A_QUALIFIED_EXPRESSION)
    e = e^.qualified_expression_info.value;

  return (e^.kind == AN_AGGREGATE_VALUE || e^.kind == AN_UNC_ARRAY_AGGREGATE);
}

/*****************************************************************************/

void generate_code_for_inline_aggregate (ref EXP_OUTPUT   pobject_out,
                                             PEXPRESSION  exp,
                                         ref FRAME_INFO   frame)
{
  PEXPRESSION agg;

  agg = exp;
  while (agg^.kind == A_QUALIFIED_EXPRESSION)
    agg = agg^.qualified_expression_info.value;

  if (agg^.kind == AN_AGGREGATE_VALUE)
  {
    // if context has runtime constraint,
    // check that it matches aggregate constraint.

    if (agg^.constraint.kind == CONSTANT_CONSTRAINT &&
        pobject_out.constraint.place != CT_CTE)
    {
      if (array_checks_enabled)
      {
        flush_constraint (ref pobject_out);
        put_code (P_CHECK_SAME_1);
        put_int4 ((int)agg^.constraint.value);
        put_int4 (get_new_near_label_nr());
      }
      else
      {
        discard_constraint (ref pobject_out);
      }
    }

    flush_address (ref pobject_out);
    store_aggregate_fields (agg, 0, ref frame);
    put_code (P_DROP_ADDR);
  }
  else    // AN_UNC_ARRAY_AGGREGATE
  {
    PENTITY element_subtype;
    int     rc, align;
    int4    element_ofs;
    int     element_size;

    if (pobject_out.constraint.place == CT_CTE &&
        agg^.constraint.kind == CONSTANT_CONSTRAINT)
    {
      put_code (P_CTE_4);
      put_int4 ((int)agg^.constraint.value);
    }
    else if (pobject_out.constraint.place != CT_CTE &&
             agg^.constraint.kind == CONSTANT_CONSTRAINT)
    {
      if (array_checks_enabled)
      {
        flush_constraint (ref pobject_out);
        put_code (P_CHECK_SAME_1);
        put_int4 ((int)agg^.constraint.value);
        put_int4 (get_new_near_label_nr());
      }
      else
      {
        discard_constraint (ref pobject_out);
      }

      put_code (P_CTE_4);
      put_int4 ((int)agg^.constraint.value);
    }
    else if (pobject_out.constraint.place != CT_CTE &&
             agg^.constraint.kind != CONSTANT_CONSTRAINT)
    {
      flush_constraint (ref pobject_out);
    }
    else
    {
      code_generator_error ("generate_code_for_expression() : inline_unc_agg bad case\n");
    }

    flush_address (ref pobject_out);


    // reserve element on stack
    element_subtype = agg^.base_type_or_null^.the_open_array_type.element;

    rc = size_and_alignment_of_type (element_subtype, out element_size, out align);
    if (rc < 0)
      code_generator_error ("generate_code_for_expression() : inline_unc_agg is too large");

    _unused align;

    if (is_constant_exp (agg^.unc_array_aggregate_info.element))
    {
      EXP_INPUT   elem_in;
      EXP_OUTPUT  elem_out;

      clear (elem_in);
      elem_in.data_must_be_computed       = true;
      elem_in.constraint_must_be_computed = true;
      // no tombstone needed because it's a constant expression

      generate_code_for_expression (agg^.unc_array_aggregate_info.element,
                                    elem_in, ref frame, out elem_out);

      // fix to avoid literal_float as element type, which reserves 8 bytes instead of 4.
      elem_out.base_type = element_subtype;

      flush_address (ref elem_out);   // converts any simple type in pool constant
    }
    else
    {
      element_ofs = allocate_temp_variable (element_size, ref frame);


      // evaluate element expression and store it in temp variable

      put_code (P_LOAD_LOCAL);
      put_int4 (element_ofs);

      evaluate_and_assign_field (    agg^.unc_array_aggregate_info.element,
                                     element_subtype,
                                     0,
                                 ref frame);
    }

    put_code (P_MULTI_COPY);   // multi_copy (target, repeat, source, size);
    put_int4 (element_size);
    put_int4 (get_new_near_label_nr());
    put_int4 (get_new_near_label_nr());

    put_code (P_DROP_ADDR);
  }
}

/*****************************************************************************/

public
void generate_code_for_assignment_statement (    PENTITY    e,
                                                 wstring    func_id,
                                             ref FRAME_INFO frame)
{
  PEXPRESSION   obj, exp;
  ASSIGNMENT_OP op;

  _unused func_id;

  generate_p_location (e^.the_assignment_statement.loc);

  obj = e^.the_assignment_statement.name;
  exp = e^.the_assignment_statement.value;
  op  = e^.the_assignment_statement.op;

  if (op == _ASSIGN)
  {
    if (exp_is_aggregate (exp) && !object_used_in_expression (obj, exp))
    {
      EXP_INPUT   object_in;
      EXP_OUTPUT  object_out;

      clear (object_in);
      object_in.data_must_be_computed       = true;
      object_in.constraint_must_be_computed = (obj^.constraint.kind != DOES_NOT_APPLY);

      allocate_tombstone_anchor    (obj, ref object_in, ref frame);
      generate_code_for_expression (obj, object_in, ref frame, out object_out);
      generate_code_for_inline_aggregate (ref object_out, exp, ref frame);
      release_tombstone_anchor (object_in);
    }
    else
    {
      PENTITY     type;
      ENTITY_KIND kind;

      type = complete_type_of (obj^.base_type_or_null);
      kind = type^.kind;

      if (kind == AN_ENUMERATION_TYPE || kind == AN_INTEGER_TYPE         || kind == A_FLOAT_TYPE ||
          kind == A_POINTER_TYPE      || kind == A_FUNCTION_POINTER_TYPE || kind == AN_UNSAFE_POINTER_TYPE)
      {
        EXP_INPUT   object_in;
        EXP_OUTPUT  object_out;
        EXP_INPUT   exp_in;
        EXP_OUTPUT  exp_out;

        clear (object_in);
        object_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (obj, ref object_in, ref frame);
        generate_code_for_expression (obj, object_in, ref frame, out object_out);
        discard_constraint (ref object_out);
        flush_address (ref object_out);

        clear (exp_in);
        exp_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        store_simple_value (type);

        release_tombstone_anchor (object_in);
        release_tombstone_anchor (exp_in);
      }
      else if (kind == AN_OPEN_ARRAY_TYPE ||
               (kind == A_STRUCT_TYPE && type^.the_struct_type.is_open_type))
      {
        EXP_INPUT   object_in;
        EXP_OUTPUT  object_out;
        EXP_INPUT   exp_in;
        EXP_OUTPUT  exp_out;

        clear (object_in);
        object_in.data_must_be_computed       = true;
        object_in.constraint_must_be_computed = true;

        allocate_tombstone_anchor    (obj, ref object_in, ref frame);
        generate_code_for_expression (obj, object_in, ref frame, out object_out);

        clear (exp_in);
        exp_in.data_must_be_computed       = true;
        exp_in.constraint_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);

        // constraints can be : CT_CTE, CT_TEMP, CT_ON_INT_STACK, CT_AT_ADDR_OFFSET.

        // 1) check if constraints match (if they're not constant)

        if (object_out.constraint.place == CT_CTE &&     // both constraints are constants
            exp_out.constraint.place == CT_CTE)
        {
          // constraint matching was already checked earlier
        }
        else if (object_out.constraint.place == CT_CTE)   // object constraint constant
        {
          if (array_checks_enabled)
          {
            flush_constraint (ref exp_out);
            put_code (P_CHECK_SAME_1);
            put_int4 ((int)object_out.constraint.cte);
            put_int4 (get_new_near_label_nr());
          }
          else
          {
            discard_constraint (ref exp_out);
          }
        }
        else if (exp_out.constraint.place == CT_CTE)    // expression constraint is constant
        {
          if (array_checks_enabled)
          {
            flush_constraint_over_address (ref object_out);
            put_code (P_CHECK_SAME_1);
            put_int4 ((int)exp_out.constraint.cte);
            put_int4 (get_new_near_label_nr());
          }
          else
          {
            discard_constraint (ref object_out);
          }
        }
        else    // both runtime constraints
        {
          flush_constraint (ref exp_out);

          if (array_checks_enabled)
          {
            flush_constraint_over_address (ref object_out);
            put_code (P_CHECK_SAME_2);    // leaves one constraint on int_stack
            put_int4 (get_new_near_label_nr());
          }
          else
          {
            discard_constraint (ref object_out);
          }
        }


        // 2) compute & flush size of block

        if (object_out.constraint.place == CT_CTE)
        {
          int rc, size, align;

          rc = size_and_alignment_of_constant_size_exp (obj, out size, out align);
          if (rc < 0)
            fatal_compiler_error0 ("generate_code_for_expression(assignment-1)");

          _unused align;

          put_code (P_CTE_4);
          put_int4 (size);
        }
        else if (exp_out.constraint.place == CT_CTE)    // expression constraint is constant
        {
          int rc, size, align;

          rc = size_and_alignment_of_constant_size_exp (exp, out size, out align);
          if (rc < 0)
            fatal_compiler_error0 ("generate_code_for_expression(assignment-2)");

          _unused align;

          put_code (P_CTE_4);
          put_int4 (size);
        }
        else   // runtime size
        {
          convert_runtime_constraint_into_size_constraint (ref exp_out);
          flush_constraint (ref exp_out);
        }


        // 3) flush addresses and copy block

        flush_address (ref exp_out);
        flush_address_over_address (ref object_out);

        if (object_shares_memory_with_expression (obj, exp))
          put_code (P_ORDERED_COPY_BLOCK);
        else
          put_code (P_COPY_BLOCK);

        release_tombstone_anchor (object_in);
        release_tombstone_anchor (exp_in);
      }
      else    // union or struct without discriminant
      {
        EXP_INPUT   object_in;
        EXP_OUTPUT  object_out;
        EXP_INPUT   exp_in;
        EXP_OUTPUT  exp_out;

        clear (object_in);
        object_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (obj, ref object_in, ref frame);
        generate_code_for_expression (obj, object_in, ref frame, out object_out);

        clear (exp_in);
        exp_in.data_must_be_computed = true;

        allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);

        {
          int rc, size, align;

          rc = size_and_alignment_of_constant_size_exp (obj, out size, out align);
          if (rc < 0)
            fatal_compiler_error0 ("generate_code_for_expression(assignment-3)");

          _unused align;

          put_code (P_CTE_4);
          put_int4 (size);
        }

        flush_address (ref exp_out);
        flush_address_over_address (ref object_out);

        if (object_shares_memory_with_expression (obj, exp))
          put_code (P_ORDERED_COPY_BLOCK);
        else
          put_code (P_COPY_BLOCK);

        release_tombstone_anchor (object_in);
        release_tombstone_anchor (exp_in);
      }
    }
  }
  else    // combined assignment
  {
    EXP_INPUT   object_in;
    EXP_OUTPUT  object_out;
    int         rc, size, align;
    EXP_INPUT   exp_in;
    EXP_OUTPUT  exp_out;

    clear (object_in);
    object_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (obj, ref object_in, ref frame);
    generate_code_for_expression (obj, object_in, ref frame, out object_out);

    discard_constraint (ref object_out);
    flush_address (ref object_out);

    rc = size_and_alignment_of_type (obj^.base_type_or_null, out size, out align);
    if (rc != 0)
      fatal_compiler_error0 ("generate_code_for_assignment_statement(1)");

     _unused align;

    clear (exp_in);
    exp_in.data_must_be_computed = true;

    allocate_tombstone_anchor (exp, ref exp_in, ref frame);

    switch (op)
    {
      case _ASSIGN_ADD_INT:          // enum += integer,  integer += integer
      case _ASSIGN_SUB_INT:          // enum -= integer,  integer -= integer
      {
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);

        // flush and convert to object type (avoid type bool because next pcode needs an int)
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null == type_bool ? type_int : obj^.base_type_or_null);

        switch (size)
        {
          case 1:
            put_code (op == _ASSIGN_ADD_INT ? P_ADD1_TO : P_SUB1_TO);
            break;

          case 2:
            put_code (op == _ASSIGN_ADD_INT ? P_ADD2_TO : P_SUB2_TO);
            break;

          case 4:
            put_code (op == _ASSIGN_ADD_INT ? P_ADD4_TO : P_SUB4_TO);
            break;

          case 8:
            put_code (op == _ASSIGN_ADD_INT ? P_ADD8_TO : P_SUB8_TO);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(2)");
            break;
        }
      }
      break;

      case _ASSIGN_ADD_PTR_INT:        // for unsafe-ptr + int -> ptr+int*size(elem) (type ptr)
      case _ASSIGN_SUB_PTR_INT:        // for unsafe-ptr - int -> ptr-int*size(elem)
      {
        PENTITY element_type;
        int     rc2, size2, align2;

        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, type_int);

        put_code ((op == _ASSIGN_ADD_PTR_INT) ? P_UNSAFE_ADD_TO : P_UNSAFE_SUB_TO);

        element_type = obj^.base_type_or_null^.the_unsafe_pointer_type.designated_type;
        rc2 = size_and_alignment_of_type (element_type, out size2, out align2);
        if (rc2 != 0)
          fatal_compiler_error0 ("generate_code_for_assignment_statement(3)");

        _unused align2;

        put_int4 (size2);
      }
      break;

      case _ASSIGN_MULT_INT_SIGNED:    // for signed integer
      case _ASSIGN_DIV_INT_SIGNED:
      case _ASSIGN_MOD_INT_SIGNED:
      {
        put_code (P_DUP_ADDR);

        switch (size)
        {
          case 1:
            put_code (P_VALUE_I1);
            break;

          case 2:
            put_code (P_VALUE_I2);
            break;

          case 4:
            put_code (P_VALUE_4);
            break;

          case 8:
            put_code (P_VALUE_8);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(4)");
            break;
        }

        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        if (op == _ASSIGN_MULT_INT_SIGNED)
          put_code ((size <= 4) ? P_SMUL4 : P_SMUL8);
        else if (op == _ASSIGN_DIV_INT_SIGNED)
          put_code ((size <= 4) ? P_SDIV4 : P_SDIV8);
        else
          put_code ((size <= 4) ? P_SMOD4 : P_SMOD8);

        switch (size)
        {
          case 1:
            put_code (P_STORE_1);
            break;

          case 2:
            put_code (P_STORE_2);
            break;

          case 4:
            put_code (P_STORE_4);
            break;

          case 8:
            put_code (P_STORE_8);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(5)");
            break;
        }
      }
      break;

      case _ASSIGN_MULT_INT_UNSIGNED:  // for unsigned integer
      case _ASSIGN_DIV_INT_UNSIGNED:
      case _ASSIGN_MOD_INT_UNSIGNED:
      {
        put_code (P_DUP_ADDR);

        switch (size)
        {
          case 1:
            put_code (P_VALUE_I1);
            break;

          case 2:
            put_code (P_VALUE_I2);
            break;

          case 4:
            put_code (P_VALUE_4);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(6)");
            break;
        }

        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        if (op == _ASSIGN_MULT_INT_UNSIGNED)
          put_code (P_UMUL4);
        else if (op == _ASSIGN_DIV_INT_UNSIGNED)
          put_code (P_UDIV4);
        else
          put_code (P_UMOD4);

        switch (size)
        {
          case 1:
            put_code (P_STORE_1);
            break;

          case 2:
            put_code (P_STORE_2);
            break;

          case 4:
            put_code (P_STORE_4);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(7)");
            break;
        }
      }
      break;

      case _ASSIGN_ADD_FLOAT:          // for floating-point
      case _ASSIGN_SUB_FLOAT:
      case _ASSIGN_MULT_FLOAT:
      case _ASSIGN_DIV_FLOAT:
      {
        put_code (P_DUP_ADDR);

        switch (size)
        {
          case 4:
            put_code (P_VALUE_FLT4);
            break;

          case 8:
            put_code (P_VALUE_FLT8);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(8)");
            break;
        }

        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        if (op == _ASSIGN_ADD_FLOAT)
          put_code ((size <= 4) ? P_ADD_FLT4 : P_ADD_FLT8);
        else if (op == _ASSIGN_SUB_FLOAT)
          put_code ((size <= 4) ? P_SUB_FLT4 : P_SUB_FLT8);
        else if (op == _ASSIGN_MULT_FLOAT)
          put_code ((size <= 4) ? P_MUL_FLT4 : P_MUL_FLT8);
        else
          put_code ((size <= 4) ? P_DIV_FLT4 : P_DIV_FLT8);

        put_code ((size <= 4) ? P_STORE_FLT4 : P_STORE_FLT8);
      }
      break;


      case _ASSIGN_SHIFT_LEFT_SIGNED:      // for integer
      case _ASSIGN_SHIFT_RIGHT_SIGNED:     // for integer
      {
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        switch (size)
        {
          case 1:
            put_code ((op == _ASSIGN_SHIFT_LEFT_SIGNED) ? P_ASL1_TO : P_ASR1_TO);
            break;

          case 2:
            put_code ((op == _ASSIGN_SHIFT_LEFT_SIGNED) ? P_ASL2_TO : P_ASR2_TO);
            break;

          case 4:
            put_code ((op == _ASSIGN_SHIFT_LEFT_SIGNED) ? P_ASL4_TO : P_ASR4_TO);
            break;

          case 8:
            put_code ((op == _ASSIGN_SHIFT_LEFT_SIGNED) ? P_ASL8_TO : P_ASR8_TO);
            put_int4 (get_new_near_label_nr());
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(9)");
            break;
        }
      }
      break;

      case _ASSIGN_SHIFT_LEFT_UNSIGNED:
      case _ASSIGN_SHIFT_RIGHT_UNSIGNED:
      {
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        switch (size)
        {
          case 1:
            put_code ((op == _ASSIGN_SHIFT_LEFT_UNSIGNED) ? P_SHL1_TO : P_SHR1_TO);
            break;

          case 2:
            put_code ((op == _ASSIGN_SHIFT_LEFT_UNSIGNED) ? P_SHL2_TO : P_SHR2_TO);
            break;

          case 4:
            put_code ((op == _ASSIGN_SHIFT_LEFT_UNSIGNED) ? P_SHL4_TO : P_SHR4_TO);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(10)");
            break;
        }
      }
      break;


      case _ASSIGN_AND:       // for bool
      case _ASSIGN_OR:
      case _ASSIGN_XOR:
      {
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        if (op == _ASSIGN_AND)
          put_code (P_AND_BOOL_TO);
        else if (op == _ASSIGN_OR)
          put_code (P_OR_BOOL_TO);
        else
          put_code (P_XOR_BOOL_TO);
      }
      break;


      case _ASSIGN_BITAND:    // for integer
      case _ASSIGN_BITOR:
      case _ASSIGN_BITXOR:
      {
        generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
        flush_simple_value (ref exp_out, exp, obj^.base_type_or_null);

        switch (size)
        {
          case 1:
            if (op == _ASSIGN_BITAND)
              put_code (P_AND1_TO);
            else if (op == _ASSIGN_BITOR)
              put_code (P_OR1_TO);
            else
              put_code (P_XOR1_TO);
            break;

          case 2:
            if (op == _ASSIGN_BITAND)
              put_code (P_AND2_TO);
            else if (op == _ASSIGN_BITOR)
              put_code (P_OR2_TO);
            else
              put_code (P_XOR2_TO);
            break;

          case 4:
            if (op == _ASSIGN_BITAND)
              put_code (P_AND4_TO);
            else if (op == _ASSIGN_BITOR)
              put_code (P_OR4_TO);
            else
              put_code (P_XOR4_TO);
            break;

          case 8:
            if (op == _ASSIGN_BITAND)
              put_code (P_AND8_TO);
            else if (op == _ASSIGN_BITOR)
              put_code (P_OR8_TO);
            else
              put_code (P_XOR8_TO);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_assignment_statement(11)");
            break;
        }
      }
      break;

      default:
        fatal_compiler_error0 ("generate_code_for_assignment_statement(invalid_op)");
        break;
    }

    release_tombstone_anchor (object_in);
    release_tombstone_anchor (exp_in);
  }
}

/*****************************************************************************/

public
void generate_code_for_local_variable_declaration (    PENTITY    e,
                                                       wstring    func_id,
                                                   ref FRAME_INFO frame)
{
  PEXPRESSION  exp;
  int          rc, size, align;

  _unused func_id;

  rc = size_and_alignment_of_type (e^.the_local_variable.type, out size, out align);
  if (rc < 0)    // error was already handled earlier
    size = 0;

  _unused align;


  // allocate it if on heap

  if (e^.the_local_variable.allocated_on_heap)
  {
    generate_p_location (e^.the_local_variable.loc);

    put_code (P_LOAD_LOCAL);
    put_int4 (e^.the_local_variable.offset);

    put_code (P_CTE_4);
    put_int4 (size);
    put_code (P_MALLOC);
    put_byte (0);

    put_code (P_STORE_ADDR);
  }


  // initialize it

  exp = e^.the_local_variable.initial_value_or_null;
  if (exp == null)
    return;           // done

  generate_p_location (e^.the_local_variable.loc);


  if (exp_is_aggregate (exp))
  {
    EXP_INPUT   object_in;
    EXP_OUTPUT  object_out;

    clear (object_in);
    object_in.data_must_be_computed       = true;
    object_in.constraint_must_be_computed = true;

    generate_code_for_local_variable (e, object_in, out object_out);
    generate_code_for_inline_aggregate (ref object_out, exp, ref frame);
  }
  else
  {
    PENTITY     type;
    ENTITY_KIND kind;

    type = base_type_of (e^.the_local_variable.type);
    kind = type^.kind;

    if (kind == AN_ENUMERATION_TYPE || kind == AN_INTEGER_TYPE         || kind == A_FLOAT_TYPE ||
        kind == A_POINTER_TYPE      || kind == A_FUNCTION_POINTER_TYPE || kind == AN_UNSAFE_POINTER_TYPE)
    {
      EXP_INPUT   object_in;
      EXP_OUTPUT  object_out;
      EXP_INPUT   exp_in;
      EXP_OUTPUT  exp_out;

      clear (object_in);
      object_in.data_must_be_computed = true;

      generate_code_for_local_variable (e, object_in, out object_out);

      discard_constraint (ref object_out);
      flush_address (ref object_out);

      clear (exp_in);
      exp_in.data_must_be_computed = true;

      allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
      generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
      flush_simple_value (ref exp_out, exp, type);
      store_simple_value (type);
      release_tombstone_anchor (exp_in);
    }
    else if (kind == AN_OPEN_ARRAY_TYPE ||
             (kind == A_STRUCT_TYPE && type^.the_struct_type.is_open_type))
    {
      EXP_INPUT   object_in;
      EXP_OUTPUT  object_out;
      EXP_INPUT   exp_in;
      EXP_OUTPUT  exp_out;

      clear (object_in);
      object_in.data_must_be_computed       = true;
      object_in.constraint_must_be_computed = true;

      generate_code_for_local_variable (e, object_in, out object_out);

      clear (exp_in);
      exp_in.data_must_be_computed       = true;
      exp_in.constraint_must_be_computed = true;

      allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
      generate_code_for_expression (exp, exp_in, ref frame, out exp_out);

      if (exp_out.constraint.place != CT_CTE)   // expression has runtime constraint
      {
        if (array_checks_enabled)
        {
          flush_constraint (ref exp_out);
          put_code (P_CHECK_SAME_1);
          put_int4 ((int)object_out.constraint.cte);
          put_int4 (get_new_near_label_nr());
        }
        else
        {
          discard_constraint (ref exp_out);
        }
      }

      put_code (P_CTE_4);
      put_int4 ((int)size);

      flush_address (ref exp_out);
      flush_address_over_address (ref object_out);

      put_code (P_COPY_BLOCK);

      release_tombstone_anchor (exp_in);
    }
    else    // union or struct without discriminant
    {
      EXP_INPUT   object_in;
      EXP_OUTPUT  object_out;
      EXP_INPUT   exp_in;
      EXP_OUTPUT  exp_out;

      clear (object_in);
      object_in.data_must_be_computed = true;

      generate_code_for_local_variable (e, object_in, out object_out);

      clear (exp_in);
      exp_in.data_must_be_computed = true;

      allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
      generate_code_for_expression (exp, exp_in, ref frame, out exp_out);

      put_code (P_CTE_4);
      put_int4 ((int)size);

      flush_address (ref exp_out);
      flush_address_over_address (ref object_out);

      put_code (P_COPY_BLOCK);

      release_tombstone_anchor (exp_in);
    }
  }
}

/*****************************************************************************/

public
void generate_code_for_global_variable_declaration (    PENTITY    e,
                                                    ref FRAME_INFO frame)  // dummy
{
  PEXPRESSION  exp;
  int          rc, size, align;

  rc = size_and_alignment_of_type (e^.the_global_variable.type, out size, out align);
  if (rc < 0)    // error was already handled earlier
    size = 0;

  _unused align;


  // initialize it

  exp = e^.the_global_variable.initial_value_or_null;
  if (exp == null)
    return;           // done


  {
    PENTITY     type;
    ENTITY_KIND kind;

    type = base_type_of (e^.the_global_variable.type);
    kind = type^.kind;

    if (kind == AN_ENUMERATION_TYPE || kind == AN_INTEGER_TYPE || kind == A_FLOAT_TYPE)
    {
      EXP_INPUT   exp_in;
      EXP_OUTPUT  exp_out;

      if (kind == AN_ENUMERATION_TYPE && exp^.const_enumeration_value_info.value == 0)
        return;

      if (kind == AN_INTEGER_TYPE && exp^.const_integer_value_info.value == 0L)
        return;

      if (kind == A_FLOAT_TYPE && exp^.const_float_value_info.value == 0.0)
        return;

      put_code (P_LOAD_GLOBAL);
      put_int8 (e^.the_global_variable.offset);

      clear (exp_in);
      exp_in.data_must_be_computed = true;

      generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
      flush_simple_value (ref exp_out, exp, type);

      store_simple_value (type);
    }
    else if (kind == A_POINTER_TYPE || kind == A_FUNCTION_POINTER_TYPE || kind == AN_UNSAFE_POINTER_TYPE)
    {
      // don't initialize because the only constant exp is null
      // and the constants are already set to zeroes.
    }
    else   // pool constant
    {
      EXP_INPUT   exp_in;
      EXP_OUTPUT  exp_out;

      put_code (P_LOAD_GLOBAL);
      put_int8 (e^.the_global_variable.offset);

      clear (exp_in);
      exp_in.data_must_be_computed = true;
      generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
      flush_address (ref exp_out);

      put_code (P_CTE_4);
      put_int4 ((int)size);

      put_code (P_COPY_BLOCK);
    }
  }
}

/*****************************************************************************/

public
void generate_code_for_reference_declaration (    PENTITY    e,
                                                  wstring    func_id,
                                              ref FRAME_INFO frame)
{
  PENTITY     type;
  ENTITY_KIND kind;
  bool        has_constraint;
  EXP_INPUT   object_in;
  EXP_OUTPUT  object_out;

  _unused func_id;

  type = complete_type_of (e^.the_reference.type);
  kind = type^.kind;
  has_constraint = (kind == AN_OPEN_ARRAY_TYPE) || (kind == A_STRUCT_TYPE && is_open_type (type));

  generate_p_location (e^.the_reference.loc);

  // for storing address
  put_code (P_LOAD_LOCAL);
  put_int4 (e^.the_reference.offset);

  clear (object_in);
  object_in.data_must_be_computed       = true;
  object_in.constraint_must_be_computed = has_constraint;

  if (e^.the_reference.references_heap_object)
  {
    object_in.tomb.anchor_provided = _YES;
    object_in.tomb.anchor_offset =
       e^.the_reference.offset + (has_constraint ? 2*address_size : address_size);
  }

  generate_code_for_expression (e^.the_reference.name, object_in, ref frame, out object_out);

  if (has_constraint)
  {
    flush_constraint (ref object_out);
    put_code (P_LOAD_LOCAL);
    put_int4 (e^.the_reference.offset + address_size);
    put_code (P_STORE_4);
  }

  flush_address (ref object_out);
  put_code (P_STORE_ADDR);
}

/*****************************************************************************/

public
void generate_code_for_clear_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame)
{
  PEXPRESSION exp;
  EXP_INPUT   object_in;
  EXP_OUTPUT  object_out;

  _unused func_id;

  generate_p_location (e^.the_clear_statement.loc);

  exp = e^.the_clear_statement.name;

  clear (object_in);
  object_in.data_must_be_computed       = true;
  object_in.constraint_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref object_in, ref frame);
  generate_code_for_expression (exp, object_in, ref frame, out object_out);

  if (object_out.constraint.place == CT_NONE || object_out.constraint.place == CT_CTE)
  {
    int rc, size, align;

    discard_constraint (ref object_out);

    rc = size_and_alignment_of_constant_size_exp (exp, out size, out align);
    if (rc < 0)
      fatal_compiler_error0 ("generate_code_for_clear_statement()");

    _unused align;

    put_code (P_CTE_4);
    put_int4 (size);
  }
  else
  {
    convert_runtime_constraint_into_size_constraint (ref object_out);
    flush_constraint (ref object_out);
  }

  flush_address (ref object_out);
  put_code (P_CLEAR);

  release_tombstone_anchor (object_in);
}

/*****************************************************************************/

public
void generate_code_for_pre_or_postfix_statement (    PENTITY    e,
                                                     wstring    func_id,
                                                 ref FRAME_INFO frame)
{
  PEXPRESSION   exp, obj;
  KIND_OPERATOR op;
  EXP_INPUT     object_in;
  EXP_OUTPUT    object_out;
  int           rc, size, align;
  bool          dec;

  _unused func_id;

  generate_p_location (e^.the_pre_or_postfix_statement.loc);

  exp = e^.the_pre_or_postfix_statement.name;

  obj = exp^.operator_value_info.arg[0];
  op  = exp^.operator_value_info.op;

  clear (object_in);
  object_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (obj, ref object_in, ref frame);
  generate_code_for_expression (obj, object_in, ref frame, out object_out);

  flush_address (ref object_out);

  dec = (op == OP_PRE_DEC || op == OP_POST_DEC);

  if (exp^.base_type_or_null^.kind == AN_UNSAFE_POINTER_TYPE)
  {
    PENTITY element_type;

    // inc/dec by size of designated type

    // compute size of designated type
    element_type = exp^.base_type_or_null^.the_unsafe_pointer_type.designated_type;

    rc = size_and_alignment_of_type (element_type, out size, out align);
    if (rc != 0)
      fatal_compiler_error0 ("generate_code_for_pre_or_postfix_statement(0)");

    _unused align;

    put_code (dec ? P_DEC_ADDR : P_INC_ADDR);
    put_int4 (size);
  }
  else
  {
    rc = size_and_alignment_of_type (obj^.base_type_or_null, out size, out align);
    if (rc != 0)
      fatal_compiler_error0 ("generate_code_for_pre_or_postfix_statement(1)");

    _unused align;

    if (size == 1)
      put_code (dec ? P_DEC1 : P_INC1);
    else if (size == 2)
      put_code (dec ? P_DEC2 : P_INC2);
    else if (size == 4)
      put_code (dec ? P_DEC4 : P_INC4);
    else if (size == 8)
      put_code (dec ? P_DEC8 : P_INC8);
    else
      fatal_compiler_error0 ("generate_code_for_pre_or_postfix_statement(2)");
  }

  release_tombstone_anchor (object_in);
}

/*****************************************************************************/

public
void generate_code_for_function_call_statement (    PENTITY    e,
                                                    wstring    func_id,
                                                ref FRAME_INFO frame)
{
  PEXPRESSION exp;
  PENTITY     type;
  EXP_INPUT   exp_in;
  EXP_OUTPUT  exp_out;

  _unused func_id;

  generate_p_location (e^.the_function_call_statement.loc);

  exp = e^.the_function_call_statement.name;
  type = complete_type_of (exp^.base_type_or_null);

  clear (exp_in);
  exp_in.data_must_be_computed = true;
  // cannot be a heap object, so no tombstone anchor needed

  generate_code_for_expression (exp, exp_in, ref frame, out exp_out);

  if (type == type_void)    // no return value -> done
    return;

  flush_simple_value (ref exp_out, exp, type);

  pcode_drop (type);
}

/*****************************************************************************/

public
void generate_code_for_free_statement (    PENTITY    e,
                                           wstring    func_id,
                                       ref FRAME_INFO frame)
{
  PEXPRESSION exp;
  EXP_INPUT   exp_in;
  EXP_OUTPUT  exp_out;
  PENTITY     designated_subtype;

  _unused func_id;

  generate_p_location (e^.the_free_statement.loc);

  exp = e^.the_free_statement.value;

  clear (exp_in);
  exp_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
  generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
  flush_simple_value (ref exp_out, exp, exp^.base_type_or_null);

  if (pointer_checks_enabled)
  {
    put_code (P_FREE_TOMB);
    designated_subtype = exp^.base_type_or_null^.the_pointer_type.designated_type;
    put_int4 (heap_object_type_unique_nr (designated_subtype)); // decorated type of heap object (to fill)
  }
  else
  {
    put_code (P_FREE);
    put_byte (0);         // bool false : indicates ptr can be null
  }

  release_tombstone_anchor (exp_in);
}

/*******************************************************************************************/

public
void generate_code_for_sleep_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame)
{
  PEXPRESSION exp;
  EXP_INPUT   exp_in;
  EXP_OUTPUT  exp_out;

  _unused func_id;

  generate_p_location (e^.the_sleep_statement.loc);

  exp = e^.the_sleep_statement.value;

  clear (exp_in);
  exp_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
  generate_code_for_expression (exp, exp_in, ref frame, out exp_out);

  if (exp_out.exp.kind == EXP_CONST_INT)
  {
    if (exp_out.exp.value >= 0)    // keep 0 to switch threads
    {
      put_code (P_SLEEP_CTE);
      put_int4 ((int)exp_out.exp.value * 1000);
    }
  }
  else if (exp_out.exp.kind == EXP_CONST_FLOAT)
  {
    if (exp_out.exp.fvalue >= 0.0)    // keep 0.0 to switch threads
    {
      put_code (P_SLEEP_CTE);
      put_int4 ((int)(exp_out.exp.fvalue * 1000.0));
    }
  }
  else if (exp^.base_type_or_null^.kind == AN_INTEGER_TYPE)
  {
    flush_simple_value (ref exp_out, exp, type_int);
    force_simple_value_of_heap_object_in_register (exp_in, exp_out);
    release_tombstone_anchor (exp_in);

    put_code (P_SLEEP_INT4);
    put_int4 (get_new_near_label_nr ());
  }
  else  // float runtime
  {
    flush_simple_value (ref exp_out, exp, type_float);
    force_simple_value_of_heap_object_in_register (exp_in, exp_out);
    release_tombstone_anchor (exp_in);

    put_code (P_SLEEP_FLT4);
    put_int4 (get_new_near_label_nr ());
  }
}

/*******************************************************************************************/

// returns true if code after statement is unreachable

public
bool generate_code_for_abort_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame)
{
  _unused func_id, frame;

  generate_p_location (e^.the_abort_statement.loc);
  put_code (P_ABORT);
  return true;
}

/*******************************************************************************************/

void throw_away_simple_exp (    PEXPRESSION exp,
                                PENTITY     promoted_type,
                            ref FRAME_INFO  frame)
{
  EXP_INPUT   left_in;
  EXP_OUTPUT  left_out;
  int8        saved_frame_offset;

  saved_frame_offset = frame.frame_offset;

  clear (left_in);
  left_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref left_in, ref frame);
  generate_code_for_expression (exp, left_in, ref frame, out left_out);
  flush_simple_value (ref left_out, exp, promoted_type);
  force_simple_value_of_heap_object_in_register (left_in, left_out);
  release_tombstone_anchor (left_in);

  frame.frame_offset = saved_frame_offset;    // reset frame offset

  if (left_out.exp.kind == EXP_ON_INT_STACK_B)
    put_code (P_DROP_BOOL);
  else if (left_out.exp.kind == EXP_ON_INT_STACK_I4)
    put_code (P_DROP_4);
  else if (left_out.exp.kind == EXP_ON_INT_STACK_I8)
    put_code (P_DROP_8);
  else if (left_out.exp.kind == EXP_ON_FLOAT_STACK_F4)
    put_code (P_DROP_FLT_4);
  else if (left_out.exp.kind == EXP_ON_FLOAT_STACK_F8)
    put_code (P_DROP_FLT_8);
  else if (left_out.exp.kind == EXP_AT_ADDR_OFFSET || left_out.exp.kind == EXP_AT_ADDR_INDIRECT)
    put_code (P_DROP_ADDR);
  else
    fatal_compiler_error0 ("throw_away_simple_exp()");
}

/*****************************************************************************/

// throw away most code, keep only bord effects

void throw_away_simple_bool_exp (    PEXPRESSION exp,
                                 ref FRAME_INFO  frame)
{
  if (exp^.kind == AN_OPERATOR_VALUE)
  {
    KIND_OPERATOR op = exp^.operator_value_info.op;

    if (op == OP_BOOL_NOT)
    {
      throw_away_simple_bool_exp (exp^.operator_value_info.arg[0], ref frame);
      return;
    }

    if (op == OP_SHORT_CIRCUIT_AND || op == OP_SHORT_CIRCUIT_OR ||
        op == OP_AND || op == OP_OR || op == OP_XOR)
    {
      throw_away_simple_bool_exp (exp^.operator_value_info.arg[0], ref frame);
      throw_away_simple_bool_exp (exp^.operator_value_info.arg[1], ref frame);
      return;
    }


    // comparison

    if (op == OP_COMPARE_SIGNED || op == OP_COMPARE_UNSIGNED || op == OP_COMPARE_FLOAT)
    {
      throw_away_simple_exp (    exp^.operator_value_info.arg[0],
                                 exp^.operator_value_info.type,
                             ref frame);

      throw_away_simple_exp (    exp^.operator_value_info.arg[1],
                                 exp^.operator_value_info.type,
                             ref frame);
      return;
    }

    if (op == OP_COMPARE_PTR)
    {
      throw_away_simple_exp (    exp^.operator_value_info.arg[0],
                                 exp^.operator_value_info.arg[0]^.base_type_or_null,
                             ref frame);

      throw_away_simple_exp (    exp^.operator_value_info.arg[1],
                                 exp^.operator_value_info.arg[1]^.base_type_or_null,
                             ref frame);
      return;
    }
  }


  // default processing : evaluate boolean expression and throw it away.

  {
    EXP_INPUT   exp_in;
    EXP_OUTPUT  exp_out;
    int8        saved_frame_offset;

    saved_frame_offset = frame.frame_offset;

    clear (exp_in);
    exp_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
    generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
    flush_simple_value (ref exp_out, exp, type_bool);
    force_simple_value_of_heap_object_in_register (exp_in, exp_out);
    release_tombstone_anchor (exp_in);

    frame.frame_offset = saved_frame_offset;    // reset frame offset

    put_code (P_DROP_BOOL);
  }
}

/*******************************************************************************************/

// returns true if code after statement is unreachable

public
bool generate_code_for_assert_statement (    PENTITY    e,
                                             wstring    func_id,
                                         ref FRAME_INFO frame)
{
  PEXPRESSION exp;
  int4        label_nr;

  _unused func_id;

  exp = e^.the_assert_statement.value;

  if (exp^.kind == A_CONST_ENUMERATION_VALUE && exp^.const_enumeration_value_info.value == 1) // true
    ;
  else  // constant false or runtime
  {
    generate_p_location (e^.the_assert_statement.loc);

    if (assertion_checks_enabled)
    {
      label_nr = get_new_near_label_nr ();

      generate_code_for_branching (exp, false, label_nr, ref frame);

      put_code (P_ASSERT);
      put_int4 (label_nr);
    }
    else
    {
      throw_away_simple_bool_exp (exp, ref frame);
    }

    if (exp^.kind == A_CONST_ENUMERATION_VALUE && exp^.const_enumeration_value_info.value == 0) // false
      return true;
  }

  return false;
}

/*******************************************************************************************/

public
void generate_code_for_code_statement (    PENTITY    e,
                                           wstring    func_id,
                                       ref FRAME_INFO frame)
{
  int  i;
  byte[]^ p   = e^.the_code_statement.code;
  int     len = e^.the_code_statement.nb_bytes;

  _unused func_id, frame;

  generate_p_location (e^.the_code_statement.loc);

  for (i=0; i<len; i++)
  {
    put_code (P_CODE);
    put_byte (p^[i]);
  }
}

/*******************************************************************************************/

public
void generate_code_for_return_statement (    PENTITY    e,
                                             wstring    func_id,
                                         ref FRAME_INFO frame,
                                             PENTITY    e_parent)
{
  PEXPRESSION exp;
  PENTITY     type;
  EXP_INPUT   exp_in;
  EXP_OUTPUT  exp_out;

  _unused func_id;

  generate_p_location (e^.the_return_statement.loc);

  exp = e^.the_return_statement.value;

  if (exp == null)   // there is a no return value
  {
    release_objects_in_outer_scope (0, e_parent, true);
    put_code (P_RETURN_VOID);
  }
  else  // there is a return value
  {
    type = complete_type_of (e^.the_return_statement.type);

    clear (exp_in);
    exp_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
    generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
    flush_simple_value (ref exp_out, exp, type);
    force_simple_value_of_heap_object_in_register (exp_in, exp_out);
    release_tombstone_anchor (exp_in);

    release_objects_in_outer_scope (0, e_parent, true);

    switch (type^.kind)
    {
      case AN_ENUMERATION_TYPE:
        switch (INTEGER_DATA[(uint)type^.the_enumeration_type.base].size)
        {
          case 1:
          case 2:
          case 4:
            if (type == type_bool)
            {
              put_code (P_RETURN_BOOL);
            }
            else
            {
              put_code (P_RETURN_4);
            }
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_return_statement(1)");
            break;
        }
        break;

      case AN_INTEGER_TYPE:
        switch (INTEGER_DATA[(uint)type^.the_integer_type.type].size)
        {
          case 1:
          case 2:
          case 4:
            put_code (P_RETURN_4);
            break;

          case 8:
            put_code (P_RETURN_8);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_return_statement(2)");
            break;
        }
        break;

      case A_FLOAT_TYPE:
        switch (FLOAT_DATA[(uint)type^.the_float_type.type].size)
        {
          case 4:
            put_code (P_RETURN_FLT4);
            break;

          case 8:
            put_code (P_RETURN_FLT8);
            break;

          default:
            fatal_compiler_error0 ("generate_code_for_return_statement(3)");
            break;
        }
        break;

      case A_POINTER_TYPE:
      case A_FUNCTION_POINTER_TYPE:
      case AN_UNSAFE_POINTER_TYPE:
        put_code (P_RETURN_ADDR);
        break;

      default:
        fatal_compiler_error0 ("generate_code_for_return_statement(4)");
        break;
    }
  }

  generate_leave_ret (frame);
}

/*******************************************************************************************/

public
void generate_code_for_branching (    PEXPRESSION exp,
                                      bool        branch_iftrue,
                                      int4        label_nr,
                                  ref FRAME_INFO  frame)
{
  if (exp^.kind == AN_OPERATOR_VALUE)
  {
    // not operator

    if (exp^.operator_value_info.op == OP_BOOL_NOT)
    {
      generate_code_for_branching (    exp^.operator_value_info.arg[0],
                                       !branch_iftrue,
                                       label_nr,
                                   ref frame);
      return;
    }


    // short circuit AND

    if (exp^.operator_value_info.op == OP_SHORT_CIRCUIT_AND)
    {
      int label_after = 0;

      if (branch_iftrue)
        label_after = get_new_near_label_nr ();

      generate_code_for_branching (    exp^.operator_value_info.arg[0],
                                       false,
                                       branch_iftrue ? label_after : label_nr,
                                   ref frame);

      generate_code_for_branching (exp^.operator_value_info.arg[1],
                                   branch_iftrue,
                                   label_nr,
                                   ref frame);
      if (branch_iftrue)
      {
        put_code (P_NEAR_LABEL);
        put_int4 (label_after);
      }

      return;
    }


    // short circuit OR

    if (exp^.operator_value_info.op == OP_SHORT_CIRCUIT_OR)
    {
      int label_after = 0;

      if (!branch_iftrue)
        label_after = get_new_near_label_nr ();

      generate_code_for_branching (    exp^.operator_value_info.arg[0],
                                       true,
                                       branch_iftrue ? label_nr : label_after,
                                   ref frame);

      generate_code_for_branching (    exp^.operator_value_info.arg[1],
                                       branch_iftrue,
                                       label_nr,
                                   ref frame);
      if (!branch_iftrue)
      {
        put_code (P_NEAR_LABEL);
        put_int4 (label_after);
      }

      return;
    }


    // comparison

    if (exp^.operator_value_info.op == OP_COMPARE_SIGNED)
    {
      cmp_operator2 (P_CMP_S4, P_CMP_S8, exp, ref frame);
      put_code (branch_iftrue ? P_BTRUE : P_BFALSE);
      put_int4 (label_nr);
      return;
    }

    if (exp^.operator_value_info.op == OP_COMPARE_UNSIGNED)
    {
      if (complete_type_of (exp^.operator_value_info.type) == type_bool)
        cmp_bool2 (exp, ref frame);
      else
        cmp_operator2 (P_CMP_U4, (PCODE)0, exp, ref frame);

      put_code (branch_iftrue ? P_BTRUE : P_BFALSE);
      put_int4 (label_nr);
      return;
    }

    if (exp^.operator_value_info.op == OP_COMPARE_FLOAT)
    {
      cmp_operator2 (P_CMP_FLT4, P_CMP_FLT8, exp, ref frame);
      put_code (branch_iftrue ? P_BTRUE : P_BFALSE);
      put_int4 (label_nr);
      return;
    }

    if (exp^.operator_value_info.op == OP_COMPARE_PTR)
    {
      cmp_ptr2 (exp, ref frame);
      put_code (branch_iftrue ? P_BTRUE : P_BFALSE);
      put_int4 (label_nr);
      return;
    }
  }


  // default processing : evaluate boolean expression, test boolean and branch.

  {
    EXP_INPUT   exp_in;
    EXP_OUTPUT  exp_out;
    int8        saved_frame_offset;

    saved_frame_offset = frame.frame_offset;

    clear (exp_in);
    exp_in.data_must_be_computed = true;

    allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
    generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
    flush_simple_value (ref exp_out, exp, type_bool);
    force_simple_value_of_heap_object_in_register (exp_in, exp_out);
    release_tombstone_anchor (exp_in);

    frame.frame_offset = saved_frame_offset;    // reset frame offset

    put_code (P_TSTBOOL);
    put_code (branch_iftrue ? P_BTRUE : P_BFALSE);
    put_int4 (label_nr);
  }
}

/*******************************************************************************************/

public
void generate_code_for_bool_expression (    PEXPRESSION exp,
                                        ref FRAME_INFO  frame)
{
  EXP_INPUT   exp_in;
  EXP_OUTPUT  exp_out;

  clear (exp_in);
  exp_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
  generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
  flush_simple_value (ref exp_out, exp, type_bool);
  force_simple_value_of_heap_object_in_register (exp_in, exp_out);
  release_tombstone_anchor (exp_in);
}

/*******************************************************************************************/

public
void generate_code_for_discrete_expression (    PEXPRESSION exp,
                                            ref FRAME_INFO  frame)
{
  EXP_INPUT   exp_in;
  EXP_OUTPUT  exp_out;

  clear (exp_in);
  exp_in.data_must_be_computed = true;

  allocate_tombstone_anchor    (exp, ref exp_in, ref frame);
  generate_code_for_expression (exp, exp_in, ref frame, out exp_out);
  flush_simple_value (ref exp_out, exp, exp^.base_type_or_null);
  force_simple_value_of_heap_object_in_register (exp_in, exp_out);
  release_tombstone_anchor (exp_in);
}

/*******************************************************************************************/
