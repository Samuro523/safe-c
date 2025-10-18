
// exp.c : expression syntax & semantic analysis

from std use strings;
use ../error, ../goptions, ../common, ../pool;
use tokens, lex, entities, typedecl, arithm, type, wstrings;

/*****************************************************************************/

CONTEXT g_no_context;

/*****************************************************************************/

// attribute

enum ATTRIBUTE
{
  ATTR_min, ATTR_max, ATTR_first, ATTR_last, ATTR_length, ATTR_byte, ATTR_string, ATTR_size,
  LAST_ATTRIBUTE,  // syntax error if no attribute matches
};

struct ATTRIBUTE_INFO
{
  ATTRIBUTE attr;
  wstring   id;
}

const ATTRIBUTE_INFO attribute_info[] =
 {{ATTR_min,    L"min"},
  {ATTR_max,    L"max"},
  {ATTR_first,  L"first"},
  {ATTR_last,   L"last"},
  {ATTR_length, L"length"},
  {ATTR_byte,   L"byte"},
  {ATTR_string, L"string"},
  {ATTR_size,   L"size"},
 };

/******************************************************************************/

void parse_name (    CONTEXT       context,                 // never null
                     bool          left_parenthesis_parsed,
                     TEXT_POSITION name_position,           // never null
                     PENTITY       type_name,               // can be null
                 out PEXPRESSION   pout);

/*****************************************************************************/

PEXPRESSION duplicate_constant_exp (PEXPRESSION exp)
{
  PEXPRESSION e = new EXPRESSION ' (exp^);

  switch (e^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
    case A_CONST_INTEGER_VALUE:
    case A_CONST_FLOAT_VALUE:
    case A_CONST_NULL_VALUE:
    case A_POOL_CONSTANT:
      // nothing to do
      break;

    default:
      fatal_compiler_error ("dup_cte_exp", token.pos);
      break;
  }

  return e;
}

/*****************************************************************************/

// to check if default expressions of function declarations match.

public
bool constant_expressions_match (PEXPRESSION e1, PEXPRESSION e2)
{
  if (e1 == null && e2 == null)    // both null
    return true;

  if (e1 == null || e2 == null)    // one of both null
    return false;

  if (e1^.kind != e2^.kind)      // not same kind
    return false;

  if (e1^.base_type_or_null == null && e2^.base_type_or_null == null)
    ; // ok
  else if (e1^.base_type_or_null == null || e2^.base_type_or_null == null)
    return false;
  else if (!types_are_equal (e1^.base_type_or_null, e2^.base_type_or_null))
    return false;

  if (e1^.constraint.kind  != e2^.constraint.kind ||
      e1^.constraint.value != e2^.constraint.value ||
      e1^.form             != e2^.form ||
      e1^.access           != e2^.access)
    return false;

  switch (e1^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
      if (e1^.const_enumeration_value_info.value != e2^.const_enumeration_value_info.value)
        return false;
      break;

    case A_CONST_INTEGER_VALUE:
      if (e1^.const_integer_value_info.value != e2^.const_integer_value_info.value)
        return false;
      break;

    case A_CONST_FLOAT_VALUE:
      if (e1^.const_float_value_info.value != e2^.const_float_value_info.value)
        return false;
      break;

    case A_CONST_NULL_VALUE:
      break;

    case A_POOL_CONSTANT:
      return pool_constants_are_identical (e1^.pool_constant_info.pool_cte,
                                           e2^.pool_constant_info.pool_cte);
    default:
      fatal_compiler_error ("cte_exp_match", token.pos);
      break;
  }

  return true;
}

//*****************************************************************************

void free_list_of_exp (LIST_OF_EXPRESSIONS^ list)
{
  LIST_OF_EXPRESSIONS^ p, t;

  p = list;
  while (p != null)
  {
    t = p;
    p = p^.next;
    free_exp (t^.exp);
    free t;
  }
}

//*****************************************************************************

public
void free_exp (PEXPRESSION e)
{
  int i;

  if (e == null)
    return;

  switch (e^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
    case A_CONST_INTEGER_VALUE:
    case A_CONST_FLOAT_VALUE:
    case A_CONST_NULL_VALUE:
    case A_POOL_CONSTANT:
      break;

    case AN_OPERATOR_VALUE:
      for (i=0; i<3; i++)
        free_exp (e^.operator_value_info.arg[i]);
      break;

    case A_RUN_CALL:
      free_exp (e^.run_call_info.function_call);
      break;

    case A_FUNCTION_VALUE:
      break;

    case A_FUNCTION_CALL:
      free_exp (e^.function_call_info.func);
      free_list_of_exp (e^.function_call_info.param);
      break;

    case A_DISCRIMINANT_VALUE:
      free_exp (e^.discriminant_value_info.prefix);
      break;

    case AN_UNC_ARRAY_AGGREGATE:
      free_exp (e^.unc_array_aggregate_info.element);
      break;

    case AN_AGGREGATE_VALUE:
      free_list_of_exp (e^.aggregate_value_info.list);
      break;

    case A_QUALIFIED_EXPRESSION:
      free_exp (e^.qualified_expression_info.value);
      break;

    case AN_ARRAY_QUALIFIED_EXPRESSION:
      free_exp (e^.array_qualified_expression_info.length);
      free_exp (e^.array_qualified_expression_info.value);
      break;

    case A_STRUCT_QUALIFIED_EXPRESSION:
      free_exp (e^.struct_qualified_expression_info.discriminant);
      free_exp (e^.struct_qualified_expression_info.value);
      break;

    case AN_ALLOCATOR:
      free_exp (e^.allocator_info.value);
      break;

    case A_GLOBAL_VARIABLE_OBJECT:
    case A_LOCAL_VARIABLE_OBJECT:
    case A_REFERENCE_OBJECT:
    case A_PARAMETER_OBJECT:
      break;

    case AN_ARRAY_ELEMENT_OBJECT:
      free_exp (e^.array_element_object_info.prefix);
      free_exp (e^.array_element_object_info.index);
      break;

    case AN_ARRAY_SLICE_OBJECT:
      free_exp (e^.array_slice_object_info.prefix);
      free_exp (e^.array_slice_object_info.index);
      free_exp (e^.array_slice_object_info.length);
      break;

    case A_STRUCT_FIELD_OBJECT:
      free_exp (e^.struct_field_object_info.prefix);
      break;

    case A_DEREFERENCED_OBJECT:
      free_exp (e^.dereferenced_object_info.prefix);
      break;

    case AN_UNSAFE_DEREFERENCED_OBJECT:
      free_exp (e^.unsafe_dereferenced_object_info.unsafe_ptr_value);
      break;

    case AN_ATTR_BYTE_OBJECT:
      free_exp (e^.attr_byte_object_info.prefix);
      break;

    case A_BOXED_OBJECT:
      free_exp (e^.boxed_object_info.parameter);
      break;

    case AN_UNBOXED_OBJECT:
      free_exp (e^.unboxed_object_info.parameter);
      break;

    case A_BOXED_ARRAY_OBJECT:
      free_list_of_exp (e^.boxed_array_object_info.list);
      break;

    default:
      fatal_compiler_error ("free_exp", token.pos);
      break;
  }

  free e;
}

/*****************************************************************************/

// returns a constant int, value 1, but undefined type.

public PEXPRESSION dummy_expression ()
{
  PEXPRESSION e;

  e = new EXPRESSION (A_CONST_INTEGER_VALUE);
  e^.base_type_or_null = null;
  e^.form = A_VALUE;
  e^.access = ACCESS_CONSTANT;
  e^.const_integer_value_info.value = 1;

  return e;
}

/*****************************************************************************/

// assertion: expression has non-null integer type.

PENTITY promote_one_int (PEXPRESSION e)
{
  PENTITY t;

  t = e^.base_type_or_null;

  if (t == type_int_literal)
    return type_int_literal;

  if (t == type_long)
    return type_long;

  if (e^.kind == A_CONST_INTEGER_VALUE &&
       (e^.const_integer_value_info.value < INTEGER_DATA[(uint)a_int4].min ||
        e^.const_integer_value_info.value > INTEGER_DATA[(uint)a_uint4].max))
    return type_long;

  if (INTEGER_DATA[(uint)t^.the_integer_type.type].is_signed)
    return type_int;

  return type_uint;
}

/*****************************************************************************/

// assertion:
// - both expressions have non-null integer types.
// returns null if promotion fails.

PENTITY promote_two_ints (PEXPRESSION e1, PEXPRESSION e2)
{
  PENTITY  t1, t2;
  bool     pi1_signed, pi2_signed;
  int      pi1_size, pi2_size;

  t1 = e1^.base_type_or_null;
  t2 = e2^.base_type_or_null;

  if (t1 == type_int_literal && t2 == type_int_literal)
    return type_int_literal;

  if (t1 == type_long || t2 == type_long)
    return type_long;

  if (e1^.kind == A_CONST_INTEGER_VALUE &&
       (e1^.const_integer_value_info.value < INTEGER_DATA[(uint)a_int4].min ||
        e1^.const_integer_value_info.value > INTEGER_DATA[(uint)a_uint4].max))
    return type_long;

  if (e2^.kind == A_CONST_INTEGER_VALUE &&
       (e2^.const_integer_value_info.value < INTEGER_DATA[(uint)a_int4].min ||
        e2^.const_integer_value_info.value > INTEGER_DATA[(uint)a_uint4].max))
    return type_long;

  pi1_signed = INTEGER_DATA[(uint)t1^.the_integer_type.type].is_signed;
  pi2_signed = INTEGER_DATA[(uint)t2^.the_integer_type.type].is_signed;

  pi1_size = INTEGER_DATA[(uint)t1^.the_integer_type.type].size;
  pi2_size = INTEGER_DATA[(uint)t2^.the_integer_type.type].size;

  if (pi1_signed && pi1_size <= 4)
  {
    if ((pi2_signed && pi2_size <= 4) ||
        (t2 == type_int_literal && e2^.kind == A_CONST_INTEGER_VALUE &&
         e2^.const_integer_value_info.value >= INTEGER_DATA[(uint)a_int4].min &&
         e2^.const_integer_value_info.value <= INTEGER_DATA[(uint)a_int4].max))
      return type_int;
  }

  if (pi2_signed && pi2_size <= 4)
  {
    if ((pi1_signed && pi1_size <= 4) ||
        (t1 == type_int_literal && e1^.kind == A_CONST_INTEGER_VALUE &&
         e1^.const_integer_value_info.value >= INTEGER_DATA[(uint)a_int4].min &&
         e1^.const_integer_value_info.value <= INTEGER_DATA[(uint)a_int4].max))
      return type_int;
  }

  if (!pi1_signed && pi1_size <= 4)
  {
    if ((!pi2_signed && pi2_size <= 4) ||
        (t2 == type_int_literal && e2^.kind == A_CONST_INTEGER_VALUE &&
         e2^.const_integer_value_info.value >= INTEGER_DATA[(uint)a_uint4].min &&
         e2^.const_integer_value_info.value <= INTEGER_DATA[(uint)a_uint4].max))
      return type_uint;
  }

  if (!pi2_signed && pi2_size <= 4)
  {
    if ((!pi1_signed && pi1_size <= 4) ||
        (t1 == type_int_literal && e1^.kind == A_CONST_INTEGER_VALUE &&
         e1^.const_integer_value_info.value >= INTEGER_DATA[(uint)a_uint4].min &&
         e1^.const_integer_value_info.value <= INTEGER_DATA[(uint)a_uint4].max))
      return type_uint;
  }

  return null;
}

/*****************************************************************************/

bool too_large_for_float4 (double d)
{
  float f = (float)d;
  return fisinfinite (f);
}

/*****************************************************************************/

// assertion:
// - both expressions have non-null float types.
// never fails.

PENTITY promote_two_floats (PEXPRESSION e1, PEXPRESSION e2, bool result_can_be_float_literal)
{
  PENTITY t1, t2;

  t1 = e1^.base_type_or_null;
  t2 = e2^.base_type_or_null;

  if (t1 == type_float_literal && t2 == type_float_literal && result_can_be_float_literal)
    return type_float_literal;

  if (t1 == type_double || t2 == type_double)
    return type_double;

  if (e1^.kind == A_CONST_FLOAT_VALUE &&
       too_large_for_float4 (e1^.const_float_value_info.value))
    return type_double;

  if (e2^.kind == A_CONST_FLOAT_VALUE &&
       too_large_for_float4 (e2^.const_float_value_info.value))
    return type_double;

  return type_float;
}

/*****************************************************************************/

void type_check_bool_operator (PEXPRESSION e, TEXT_POSITION pos)
{
  PENTITY t;
  t = e^.base_type_or_null;
  if (t != null && t != type_bool)
    semantic_error ("expression must have type bool", pos);
}

/*****************************************************************************/

void convert_type_into_base_type_and_constraint (    PENTITY    type,
                                                     bool       always_constrained,
                                                 out PENTITY    base_type_or_null,
                                                 out CONSTRAINT cons)
{
  PENTITY t;

  clear base_type_or_null;
  clear cons;

  t = complete_type_of (type);

  if (t == null)
    return;

  if (t^.kind == AN_OPEN_ARRAY_TYPE)
  {
    cons.kind = always_constrained ? RUNTIME_CONSTRAINT : UNCONSTRAINED;
  }
  else if (t^.kind == A_STRUCT_TYPE && t^.the_struct_type.is_open_type)
  {
    cons.kind = always_constrained ? RUNTIME_CONSTRAINT : UNCONSTRAINED;
  }
  else if (t^.kind == AN_ARRAY_TYPE)
  {
    cons.kind = CONSTANT_CONSTRAINT;
    cons.value = t^.the_array_type.length;

    t = complete_type_of (t^.the_array_type.open_array);
  }
  else if (t^.kind == A_CONSTRAINED_STRUCT_TYPE)
  {
    cons.kind = CONSTANT_CONSTRAINT;
    cons.value = t^.the_constrained_struct_type.discriminant_value;

    t = complete_type_of (t^.the_constrained_struct_type.open_struct);
  }

  base_type_or_null = t;
}

/*****************************************************************************/

public
void convert_type_into_context (    PENTITY  type,
                                out CONTEXT context)
{
  clear context;

  convert_type_into_base_type_and_constraint (    type               => type,
                                                  always_constrained => false,
                                              out base_type_or_null  => context.base_type_or_null,
                                              out cons               => context.constraint);
  context.address_of = false;
}

/*****************************************************************************/

// stores only simple constants (no jagged types)

void simple_store_constant_into_pool (PEXPRESSION source, int size, POOL pool)
{
  int8    r;
  double  rf;

  switch (source^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
      r = source^.const_enumeration_value_info.value;
      store_integer (pool, 0, r, (uint)size);
      break;

    case A_CONST_INTEGER_VALUE:
      r = source^.const_integer_value_info.value;
      store_integer (pool, 0, r, (uint)size);
      break;

    case A_CONST_FLOAT_VALUE:
      rf = source^.const_float_value_info.value;
      store_float (pool, 0, rf, (uint)size);
      break;

    case A_CONST_NULL_VALUE:     // can be unsafe ptr null value
      // nothing to do : pool zone is zeroed by default
      break;

    case A_POOL_CONSTANT:
      copy_pool_to_pool (source^.pool_constant_info.pool_cte, 0,
                         pool, 0,
                         (uint)size);
      break;

    default:
      fatal_compiler_error ("exp#641", token.pos);
      break;
  }
}

/*****************************************************************************/

void load_constant_object_from_pool (    POOL        pool_cte,
                                         uint        offset,
                                         uint        size,
                                         PENTITY     type,
                                     out PEXPRESSION pout)
{
  PEXPRESSION exp_out;

  if (type^.kind == AN_ENUMERATION_TYPE)
  {
    int8 r;

    load_integer (pool_cte, offset, out r, size, is_signed => false);

    exp_out = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
    exp_out^.base_type_or_null = type;
    exp_out^.form = AN_OBJECT;
    exp_out^.access = ACCESS_CONSTANT;
    exp_out^.const_enumeration_value_info.value = (uint4)r;
  }
  else if (type^.kind == AN_INTEGER_TYPE)
  {
    int8 r;

    load_integer (pool_cte, offset, out r, size, INTEGER_DATA[(uint)type^.the_integer_type.type].is_signed);

    exp_out = new EXPRESSION (A_CONST_INTEGER_VALUE);
    exp_out^.base_type_or_null = type;
    exp_out^.form = AN_OBJECT;
    exp_out^.access = ACCESS_CONSTANT;
    exp_out^.const_integer_value_info.value = r;
  }
  else if (type^.kind == A_FLOAT_TYPE)
  {
    double rf;

    load_float (pool_cte, offset, out rf, size);

    exp_out = new EXPRESSION (A_CONST_FLOAT_VALUE);
    exp_out^.base_type_or_null = type;
    exp_out^.form = AN_OBJECT;
    exp_out^.access = ACCESS_CONSTANT;
    exp_out^.const_float_value_info.value = rf;
  }
  else if (type^.kind == A_POINTER_TYPE ||
           type^.kind == A_FUNCTION_POINTER_TYPE ||
           type^.kind == AN_UNSAFE_POINTER_TYPE)
  {
    exp_out = new EXPRESSION (A_CONST_NULL_VALUE);
    exp_out^.base_type_or_null = type;
    exp_out^.form = AN_OBJECT;
    exp_out^.access = ACCESS_CONSTANT;
  }
  else if (is_open_type (type))   // open type -> copy jag reference as new pool object
  {
    int8 r;

    exp_out = new EXPRESSION (A_POOL_CONSTANT);
    exp_out^.form = AN_OBJECT;
    exp_out^.access = ACCESS_CONSTANT;
    exp_out^.base_type_or_null = complete_type_of (type);
    exp_out^.constraint.kind = CONSTANT_CONSTRAINT;
    exp_out^.pool_constant_info.pool_cte = load_reference_of (pool_cte, offset);
    load_integer (pool_cte, offset+(uint)address_size, out r, 4, is_signed => false);   // fetch array length/discriminant value
    exp_out^.constraint.value = (uint4)r;
  }
  else    // any other constrained type -> copy pool object (array/struct)
  {
    POOL p;

    exp_out = new EXPRESSION (A_POOL_CONSTANT);
    exp_out^.form = AN_OBJECT;
    exp_out^.access = ACCESS_CONSTANT;

    convert_type_into_base_type_and_constraint (    type                => type,
                                                    always_constrained  => true,
                                                out base_type_or_null   => exp_out^.base_type_or_null,
                                                out cons                => exp_out^.constraint);

    p = new_pool_constant (size, align_of_pool_cte (pool_cte));
    copy_pool_to_pool (pool_cte, offset, p, 0, size);
    exp_out^.pool_constant_info.pool_cte = p;
  }

  pout = exp_out;
}

/*****************************************************************************/

// promote literal type into actual runtime types

void promote_parameter (PEXPRESSION e)
{
  if (e^.base_type_or_null == null)
    return;

  if (e^.base_type_or_null == type_int_literal)
  {
    if (e^.kind == A_CONST_INTEGER_VALUE)
    {
      if (e^.const_integer_value_info.value >= INTEGER_DATA[(uint)a_int4].min &&
          e^.const_integer_value_info.value <= INTEGER_DATA[(uint)a_int4].max)
      {
        e^.base_type_or_null = type_int;
      }
      else
      {
        e^.base_type_or_null = type_long;
      }
    }
  }
  else if (e^.base_type_or_null == type_float_literal)
  {
    if (e^.kind == A_CONST_FLOAT_VALUE)
    {
      if (too_large_for_float4 (e^.const_float_value_info.value))
      {
        e^.base_type_or_null = type_double;
      }
      else
      {
        e^.base_type_or_null = type_float;
      }
    }
  }
  else if (e^.base_type_or_null == type_null_literal)
  {
    e^.base_type_or_null = type_unsafe_ptr_to_byte;
    check_unsafe_region ();
  }
}

/*****************************************************************************/

// error message is generated in function
// used for parameter and ref declaration

public
int check_ref_parameter_context_compatibility (CONTEXT target, PEXPRESSION source, TEXT_POSITION pos)
{
  PENTITY s, t;

  s = complete_type_of (source^.base_type_or_null);      // actual
  t = complete_type_of (target.base_type_or_null);       // formal

  if (s == null || t == null)   // previous error
    return -1;

  if (!types_are_equal (s, t))
  {
    semantic_error ("type is not compatible", pos);
    return -1;
  }


  if (s^.kind == AN_OPEN_ARRAY_TYPE)
  {
    if (source^.constraint.kind == CONSTANT_CONSTRAINT &&
        target.constraint.kind == CONSTANT_CONSTRAINT &&
        source^.constraint.value != target.constraint.value)
    {
      semantic_error ("array length mismatch", pos);
      return -1;
    }
  }


  if (s^.kind == A_STRUCT_TYPE)
  {
    if (source^.constraint.kind == CONSTANT_CONSTRAINT &&
        target.constraint.kind == CONSTANT_CONSTRAINT &&
        source^.constraint.value != target.constraint.value)
    {
      semantic_error ("struct discriminant mismatch", pos);
      return -1;
    }
  }


  return 0;
}

/*****************************************************************************/

package EXPS

  struct EXPRESSIONS
  {
    LIST_OF_EXPRESSIONS^ head, tail;
    int                  count;
  }

end EXPS;

/*****************************************************************************/

void append_expression (ref EXPRESSIONS q, EXPRESSION^ exp, PENTITY type, PENTITY e)
{
  LIST_OF_EXPRESSIONS^ p;

  p = new LIST_OF_EXPRESSIONS;

  p^.exp  = exp;
  p^.type = type;
  p^.e    = e;

  if (q.tail == null)
    q.head = p;
  else
  {
    q.tail^.next = p;
    p^.prev = q.tail;
  }
  q.tail = p;

  q.count++;
}

void free_list (ref EXPRESSIONS q)
{
  LIST_OF_EXPRESSIONS^ l, s;
  l = q.head;
  while (l != null)
  {
    s = l;
    l = l^.next;
    free s;
  }
  clear q;
}

/*****************************************************************************/

package ARG_FLAGS
  const int _FMINUS         = 0x01;
  const int _FPLUS          = 0x02;
  const int _FZERO          = 0x04;
  const int _DYN_WIDE       = 0x08;
  const int _DYN_PRECISION  = 0x10;
  const int _SKIP_PAR       = 0x20;
end ARG_FLAGS;

/*****************************************************************************/

// returns -1 if we're out of arguments, 0 if OK (or minor matching errors)

int check_arg (    char                 c,     // '*' means int, d u x e f c C s S
                   int                  flags,
               ref int                  argn,
               ref LIST_OF_EXPRESSIONS^ plist,
                   TEXT_POSITION        pos)
{
  PENTITY type;

  if (plist == null)      // no more actual arguments
  {
    semantic_error ("format string : missing arguments", pos);
    return -1;
  }

  type = plist^.type;
  if (type != null)
  {
    switch (c)
    {
      case '*':
        {
          // int
          if (type != type_int)
            semantic_error ("format string : %* must match 'int' argument", pos);
        }
        break;

      case 'd':
        {
          // any signed integer
          if (type^.kind != AN_INTEGER_TYPE || !INTEGER_DATA[(uint)type^.the_integer_type.type].is_signed)
            semantic_error ("format string : %d must match signed integer argument", pos);

          if ((flags & (_FZERO+_FMINUS)) == (_FZERO+_FMINUS))
            semantic_error ("format string : flags '0' and '-' are not allowed together", pos);
        }
        break;

      case 'u':
        {
          // any unsigned integer or enum
          if (type^.kind == AN_ENUMERATION_TYPE)
            ;
          else if (type^.kind == AN_INTEGER_TYPE && !INTEGER_DATA[(uint)type^.the_integer_type.type].is_signed)
            ;
          else
            semantic_error ("format string : %u must match enum or unsigned integer argument", pos);

          if ((flags & _FPLUS) > 0)
            semantic_error ("format string : flag '+' is not allowed with type %u", pos);

          if ((flags & (_FZERO+_FMINUS)) == (_FZERO+_FMINUS))
            semantic_error ("format string : flags '0' and '-' are not allowed together", pos);
        }
        break;

      case 'x':
        {
          // any integer or enum
          if (type^.kind != AN_ENUMERATION_TYPE && type^.kind != AN_INTEGER_TYPE)
            semantic_error ("format string : %x must match enum or integer argument", pos);

          if ((flags & _FPLUS) > 0)
            semantic_error ("format string : flag '+' is not allowed with type %x", pos);

          if ((flags & (_FZERO+_FMINUS)) == (_FZERO+_FMINUS))
            semantic_error ("format string : flags '0' and '-' are not allowed together", pos);
        }
        break;

      case 'e':
      case 'f':
        {
          // any floating-point
          if (type^.kind != A_FLOAT_TYPE)
            semantic_error ("format string : %f must match float or double argument", pos);

          if ((flags & (_FZERO+_FMINUS)) == (_FZERO+_FMINUS))
            semantic_error ("format string : flags '0' and '-' are not allowed together", pos);
        }
        break;

      case 'c':
        {
          // char or string
          if (type == type_char || types_are_equal (type, type_string))
            ;
          else
            semantic_error ("format string : %c must match char or string argument", pos);

          if ((flags & (_FZERO+_FPLUS)) != 0)
            semantic_error ("format string : illegal flags for type %c", pos);
        }
        break;

      case 'C':
        {
          // wchar or wstring
          if (type == type_wchar || types_are_equal (type, type_wstring))
            ;
          else
            semantic_error ("format string : %C must match wchar or wstring argument", pos);

          if ((flags & (_FZERO+_FPLUS)) != 0)
            semantic_error ("format string : illegal flags for type %C", pos);
        }
        break;

      case 's':
        {
          // string
          if (!types_are_equal (type, type_string))
            semantic_error ("format string : %s must match string argument", pos);

          if ((flags & (_FZERO+_FPLUS)) != 0)
            semantic_error ("format string : illegal flags for type %s", pos);
        }
        break;

      case 'S':
        {
          // wstring
          if (!types_are_equal (type, type_wstring))
            semantic_error ("format string : %S must match wstring argument", pos);

          if ((flags & (_FZERO+_FPLUS)) != 0)
            semantic_error ("format string : illegal flags for type %S", pos);
        }
        break;

      default:
         fatal_compiler_error ("check_arg", pos);
         break;
    }
  }

  argn++;
  plist = plist^.next;

  return 0;
}

/*****************************************************************************/

int next_wchar (POOL p, uint4 size, bool is_wchar, ref uint4 pi)
{
  int8 n;

  if (pi >= size)
    return -1;

  load_integer (p, pi, out n, is_wchar ? 2 : 1, is_signed => false);
  pi += is_wchar ? 2 : 1;

  return (int)n;
}

/*****************************************************************************/

void check_format_string (POOL          p,
                          int4          size,
                          bool          is_wchar,
                          EXPRESSIONS   varying_list,
                          ACCESS        access,       // ACCESS_READONLY, ACCESS_READWRITE
                          TEXT_POSITION pos)
{
  uint4 i;      // string pos
  int   argn;   // count arg
  int   c;
  int   flags;
  LIST_OF_EXPRESSIONS^ list;

  i = 0;
  argn = 0;
  list = varying_list.head;

  for (;;)
  {
    c = next_wchar (p, (uint)size, is_wchar, ref i);
    if (c < 0)
      break;

    if (c != (int)'%')     // skip anything else than %
      continue;

    c = next_wchar (p, (uint)size, is_wchar, ref i);;
    if (c == (int)'%')                  // means %%
      continue;

    flags = 0;

    if (access == ACCESS_READONLY)
    {
      // flags

      while (c == (int)'-' || c == (int)'+' || c == (int)'0')
      {
        if (c == (int)'-')
        {
          flags |= _FMINUS;
          c = next_wchar (p, (uint)size, is_wchar, ref i);;
        }
        else if (c == (int)'+')
        {
          flags |= _FPLUS;
          c = next_wchar (p, (uint)size, is_wchar, ref i);;
        }
        else if (c == (int)'0')
        {
          flags |= _FZERO;
          c = next_wchar (p, (uint)size, is_wchar, ref i);;
        }
      }


      // width

      if (c == (int)'*')
      {
        flags |= _DYN_WIDE;
        c = next_wchar (p, (uint)size, is_wchar, ref i);;
      }
      else if (c >= (int)'0' && c <= (int)'9')
      {
        while (c >= (int)'0' && c <= (int)'9')
          c = next_wchar (p, (uint)size, is_wchar, ref i);;
      }


      // precision

      if (c == (int)'.')
      {
        c = next_wchar (p, (uint)size, is_wchar, ref i);;

        if (c == (int)'*')
        {
          flags |= _DYN_PRECISION;
          c = next_wchar (p, (uint)size, is_wchar, ref i);;
        }
        else if (c >= (int)'0' && c <= (int)'9')
        {
          while (c >= (int)'0' && c <= (int)'9')
            c = next_wchar (p, (uint)size, is_wchar, ref i);;
        }
      }



      if ((flags & _DYN_WIDE) != 0)   // swallow extra int parameter
      {
        if (check_arg (c => '*', flags => 0, ref argn => argn, ref plist => list, pos => pos) < 0)
          return;
      }


      if ((flags & _DYN_PRECISION) != 0)    // swallow extra int parameter
      {
        if (check_arg (c => '*', flags => 0, ref argn => argn, ref plist => list, pos => pos) < 0)
          return;
      }


      // type

      if (c == (int)'d' || c == (int)'u' || c == (int)'x' || c == (int)'f' ||
          c == (int)'e' ||
          c == (int)'c' || c == (int)'C' || c == (int)'s' || c == (int)'S')
      {
        if (check_arg (c => (char)c, flags => flags, ref argn => argn, ref plist => list, pos => pos) < 0)
          return;
      }
      else
      {
        semantic_error ("illegal format string : bad type", pos);
        continue;
      }
    }
    else   // out / ref parameter
    {

      // skip par

      if (c == (int)'*')
      {
        flags |= _SKIP_PAR;
        c = next_wchar (p, (uint)size, is_wchar, ref i);;
      }


      // width

      if (c >= (int)'0' && c <= (int)'9')
      {
        while (c >= (int)'0' && c <= (int)'9')
          c = next_wchar (p, (uint)size, is_wchar, ref i);;
      }


      // type

      if (c == (int)'d' || c == (int)'u' || c == (int)'x' || c == (int)'f' ||
          c == (int)'c' || c == (int)'C' || c == (int)'s' || c == (int)'S')
      {
        if ((flags & _SKIP_PAR) == 0)
        {
          if (check_arg (c => (char)c, flags => 0, ref argn => argn, ref plist => list, pos => pos) < 0)
            return;
        }
      }
      else
      {
        char msg[128];
        sprintf (out msg, "illegal format string : bad type %%%c", (char)c);
        semantic_error (msg, pos);
        continue;
      }
    }
  }


  if (argn < varying_list.count)
    semantic_error ("format string : too many arguments", pos);
}

/*****************************************************************************/

PEXPRESSION box_into_object (PEXPRESSION e, MODE par_mode, TEXT_POSITION pos)
{
  PEXPRESSION n;

  _unused par_mode;


  // case 1 : it's a constant expression

  if (is_constant_exp (e))
  {
    int rc, size, align;

    // returns 0 if OK, -1 if bad type, -2 if size is too large, -3 if other constraint
    rc = size_and_alignment_of_constant_size_exp (e, out size, out align);
    if (rc == 0)
    {
      // create a new pool object of type byte[]

      POOL p;

      n = new EXPRESSION (A_POOL_CONSTANT);
      n^.base_type_or_null = type_object;   // byte[]
      n^.form = AN_OBJECT;
      n^.access = ACCESS_CONSTANT;

      n^.constraint.kind = CONSTANT_CONSTRAINT;
      n^.constraint.value = (uint)size;

      p = new_pool_constant ((uint)size, (uint)align);

      n^.pool_constant_info.pool_cte = p;

      simple_store_constant_into_pool (e, size, p);

      free_exp (e);

      return n;
    }
    else if (rc == -1)
      semantic_error ("type has no size", pos);
    else if (rc == -2)
      semantic_error ("size is too large", pos);
    else
      fatal_compiler_error ("parameter: cannot compute size", pos);

    return e;   // default after error
  }


  // case 2 : it has already type object

  if (types_are_equal (e^.base_type_or_null, type_object))
    return e;


  // case 3 : it has a constant size

  if (e^.constraint.kind == DOES_NOT_APPLY ||
      e^.constraint.kind == CONSTANT_CONSTRAINT)
  {
    int rc, size, align;

    // returns 0 if OK, -1 if bad type, -2 if size is too large, -3 if other constraint
    rc = size_and_alignment_of_constant_size_exp (e, out size, out align);

    _unused align;

    if (rc == 0)
    {
      // compute actual parameter size and fill constraint

      n = new EXPRESSION (A_BOXED_OBJECT);
      n^.base_type_or_null = type_object;
      n^.form = AN_OBJECT;
      n^.access = e^.access;
      n^.boxed_object_info.parameter = e;

      n^.constraint.kind = CONSTANT_CONSTRAINT;
      n^.constraint.value = (uint)size;

      return n;
    }
    else if (rc == -2)
      semantic_error ("size is too large", pos);
    else
      fatal_compiler_error ("parameter: cannot compute size", pos);

    return e;   // default after error
  }


  // case 4 : it has a runtime size

  n = new EXPRESSION (A_BOXED_OBJECT);
  n^.base_type_or_null = type_object;
  n^.form = AN_OBJECT;
  n^.access = e^.access;
  n^.boxed_object_info.parameter = e;

  n^.constraint.kind = RUNTIME_CONSTRAINT;

  return n;
}

/*****************************************************************************/

//  actual_parameter ::=       [identifier "=>"] expression
//                     | "ref" [identifier "=>"] object_designator
//                     | "out" [identifier "=>"] object_designator

void parse_actual_parameter (    PENTITY     param,        // formal parameter (A_PARAMETER)
                                 bool        param_valid,
                                 bool        in_varying_list,
                             ref EXPRESSIONS list,        // actual parameter must be appended to list
                                 int         varying_list_count)
{
  MODE          mode, par_mode;
  PEXPRESSION   e;
  CONTEXT       context;
  TEXT_POSITION mode_pos, pos;
  PENTITY       formal_parameter_type, basetype_before_boxing;

  // mode

  mode_pos = token.pos;

  if (token.kind == TOKEN_ref)
  {
    get_token();   // skip
    mode = MODE_REF;
  }
  else if (token.kind == TOKEN_out)
  {
    get_token();   // skip
    mode = MODE_OUT;
  }
  else
  {
    mode = MODE_IN;
  }

  clear par_mode;
  if (param_valid)
  {
    par_mode = param^.the_parameter.mode;
    if (mode != par_mode)
    {
      if (par_mode == MODE_OUT)
        semantic_error ("keyword 'out' expected", mode_pos);
      else if (par_mode == MODE_REF)
        semantic_error ("keyword 'ref' expected", mode_pos);
      else
        semantic_error ("no parameter mode allowed here", mode_pos);
    }
  }


  // identifier

  if (token.kind == IDENTIFIER)
  {
    get_look_ahead_token ();
    if (look_ahead_token.kind == ARROW)
    {
      if (param_valid && wstrcmp (token.info._identifier.value, param^.identifier_or_null^) != 0)
      {
        char id[MAX_IDENTIFIER_LENGTH + 1];
        char msg[MAX_IDENTIFIER_LENGTH + 128];

        wstring_to_string (param^.identifier_or_null^, out id);
        sprintf (out msg, "parameter '%s' expected", id);

        semantic_error (msg, token.pos);
      }

      get_token();   // skip identifier
      get_token();   // skip "=>"
    }
  }


  clear context;
  clear formal_parameter_type;

  if (param_valid)
  {
    if (in_varying_list)   // type object required
    {
      formal_parameter_type = type_object;  // arbitrary choice - maybe changed later into type_array_of_object
    }
    else
    {
      formal_parameter_type = param^.the_parameter.type;
    }

    convert_type_into_context (formal_parameter_type, out context);
  }


  pos = token.pos;

  if (mode == MODE_IN)
  {
    parse_expression (    context,
                          token.pos,
                          null,
                      out e);
  }
  else
  {
    parse_unary_expression (    context,
                                token.pos,
                                null,
                            out e);
  }


  // box or unbox actual parameter, if necessary

  if (param_valid)
  {
    if (in_varying_list)   // VARYING ARGUMENT LIST : type object[] or packed type required
    {
      promote_parameter (e);

      basetype_before_boxing = e^.base_type_or_null;

      if (e^.base_type_or_null != null)
      {
        // check actual type and set formal type

        if (varying_list_count == 0)  // first varying parameter
        {
          // make sure actual parameter has either type array_of_object or is packed

          if (types_are_equal (e^.base_type_or_null, type_array_of_object))
          {
            formal_parameter_type = type_array_of_object;
            convert_type_into_context (formal_parameter_type, out context);  // fix context
          }
          else if (is_packed_type (e^.base_type_or_null))
          {
            /* ok */
          }
          else
          {
            semantic_error ("parameter must have type object[] or have a packed type", pos);
          }
        }
        else   // not first one
        {
          // make sure actual parameter is packed

          if (!is_packed_type (e^.base_type_or_null))
            semantic_error ("parameter must have packed type", pos);
        }


        if (is_packed_type (e^.base_type_or_null))
        {
          e = box_into_object (e, par_mode, pos);
        }
      }
    }
    else if (types_are_equal (context.base_type_or_null, type_object))   // BOXING (packed type -> type object)
    {
      // formal parameter has type byte[] or byte[N]

      promote_parameter (e);

      basetype_before_boxing = e^.base_type_or_null;

      if (e^.base_type_or_null != null)
      {
        if (!is_packed_type (e^.base_type_or_null))
          semantic_error ("parameter must have packed type", pos);
        else
          e = box_into_object (e, par_mode, pos);
      }
    }
    else if (e^.base_type_or_null != null && types_are_equal (e^.base_type_or_null, type_object))  // UNBOXING
    {
      // actual parameter has type object (= byte[]), formal parameter has packed type

      basetype_before_boxing = e^.base_type_or_null;


      // make sure formal parameter is packed and is not open struct

      if (context.base_type_or_null != null)
      {
        if (!is_packed_type (context.base_type_or_null))
        {
          semantic_error ("parameter has incompatible type", pos);
        }
        else
        {
          // make sure formal parameter is not an open struct

          if (context.base_type_or_null^.kind == A_STRUCT_TYPE &&
              context.base_type_or_null^.the_struct_type.is_open_type)
          {
            semantic_error ("a byte[] parameter is not compatible with an open struct type", pos);
          }
        }
      }



      if (context.constraint.kind == UNCONSTRAINED)  // formal parameter has unconstrained type
      {
        PENTITY array_type, element_type;
        int     rc, element_size, align;


        // formal param is open array (or struct but not allowed)

        if (context.base_type_or_null^.kind == AN_OPEN_ARRAY_TYPE &&
            is_packed_type (context.base_type_or_null))
        {
          array_type = context.base_type_or_null;
          element_type = complete_type_of (array_type^.the_open_array_type.element);

          // returns 0 if OK, -1 if bad type, -2 if size is too large
          rc = size_and_alignment_of_type (element_type, out element_size, out align);

          _unused align;

          if (rc < 0)
            fatal_compiler_error ("parameter element_size", token.pos);
        }
        else
        {
          element_size = 1;
        }


        if (element_size == 0)
        {
          semantic_error ("formal array element size cannot be zero", pos);
          element_size = 1;
        }

        if (e^.constraint.kind == CONSTANT_CONSTRAINT &&
            e^.constraint.value % (uint)element_size != 0)
        {
          semantic_error ("parameter size is truncated", pos);
        }

        if (e^.constraint.kind == RUNTIME_CONSTRAINT && element_size > 1)
        {
          warning ("risk of parameter size truncation", pos);
        }


        if (e^.kind == A_POOL_CONSTANT)    // actual parameter is pool cte of type byte[]
        {
          int         length, size;
          PEXPRESSION n;
          POOL        p;

          length = (int)e^.constraint.value / element_size;
          size = length * element_size;

          n = new EXPRESSION (A_POOL_CONSTANT);
          n^.form = AN_OBJECT;
          n^.access = ACCESS_CONSTANT;
          n^.base_type_or_null = context.base_type_or_null;
          n^.constraint.kind = CONSTANT_CONSTRAINT;
          n^.constraint.value = (uint)length;

          p = new_pool_constant ((uint)size, align_of_pool_cte(e^.pool_constant_info.pool_cte));
          copy_pool_to_pool (source_pool   => e^.pool_constant_info.pool_cte,
                             source_offset => 0,
                             target_pool   => p,
                             target_offset => 0,
                             size          => (uint)size);

          n^.pool_constant_info.pool_cte = p;

          e = n;
        }
        else       // actual parameter is NOT a pool constant
        {
          PEXPRESSION n;

          n = new EXPRESSION (AN_UNBOXED_OBJECT);
          n^.base_type_or_null = context.base_type_or_null;
          n^.form = AN_OBJECT;
          n^.access = e^.access;
          n^.unboxed_object_info.parameter = e;
          n^.unboxed_object_info.to_open_array = true;

          if (e^.constraint.kind == CONSTANT_CONSTRAINT)
          {
            n^.constraint.kind = CONSTANT_CONSTRAINT;
            n^.constraint.value = e^.constraint.value / (uint)element_size;
          }
          else
          {
            n^.constraint.kind = RUNTIME_CONSTRAINT;
          }

          e = n;
        }
      }
      else      // formal parameter has no constraint or constant constraint
      {
        int rc, size, align;

        if (is_packed_type (formal_parameter_type))
        {
          // returns 0 if OK, -1 if bad type, -2 if size is too large
          rc = size_and_alignment_of_type (formal_parameter_type, out size, out align);

          _unused align;

          if (rc < 0)
            fatal_compiler_error ("parameter conversion2", token.pos);

          if (e^.constraint.kind == CONSTANT_CONSTRAINT &&
              e^.constraint.value != (uint4)size)
          {
            semantic_error ("parameter size does not match", pos);
            size = 0;
          }
        }
        else   // earlier error
        {
          size = 0;
        }


        if (e^.kind == A_POOL_CONSTANT)    // actual parameter is pool cte of type byte[]
        {
          PEXPRESSION n;

          // convert pool constant into packed type

          load_constant_object_from_pool (    pool_cte => e^.pool_constant_info.pool_cte,
                                              offset   => 0,
                                              size     => (uint)size,
                                              type     => formal_parameter_type,
                                          out pout     => n);
          free_exp (e);

          e = n;
        }
        else    // convert runtime byte[] actual parameter into constant-size packed type
        {
          PEXPRESSION n;

          n = new EXPRESSION (AN_UNBOXED_OBJECT);
          n^.base_type_or_null = context.base_type_or_null;
          n^.constraint = context.constraint;
          n^.form = AN_OBJECT;
          n^.access = e^.access;
          n^.unboxed_object_info.parameter = e;
          n^.unboxed_object_info.to_open_array = false;

          e = n;
        }
      }
    }
    else
    {
      // normal case : no boxing/unboxing

      basetype_before_boxing = e^.base_type_or_null;
    }



    // check that parameters are compatible

    if (par_mode == MODE_IN)
    {
      (void)check_assignment_context_compatibility (context, e, pos);
    }
    else
    {
      (void)check_ref_parameter_context_compatibility (context, e, pos);
    }


    // check mode of expression (constants are not allowed for modes out and ref)

    if (par_mode != MODE_IN)      // out or ref
    {
      if (e^.access != ACCESS_READWRITE)
        semantic_error ("a read/write object is expected here", pos);
    }


    // append node to list

    append_expression (ref list, e, basetype_before_boxing, param);
  }
}

/*****************************************************************************/

void skip_to_next_param (ref PENTITY pparam)
{
  while (pparam != null && pparam^.kind != A_PARAMETER)
    pparam = pparam^.next;
}

/*****************************************************************************/

// function_call ::= name  "("  actual_parameters  ")"
//
// actual_parameters ::= [actual_parameter {"," actual_parameter}]

void parse_function_call (    TEXT_POSITION prefix_pos,
                              PEXPRESSION   prefix,
                          out PEXPRESSION   pout)
{
  PENTITY       ftype, param, next_param;
  bool          param_valid, in_varying_list;
  EXPRESSIONS   list, varying_list;
  TEXT_POSITION pos;

  _unused prefix_pos;

  param = null;

  param_valid = false;
  in_varying_list = false;
  clear (list);
  clear (varying_list);


  ftype = prefix^.base_type_or_null;

  if (ftype != null)
  {
    if (ftype^.kind == A_FUNCTION_POINTER_TYPE)
    {
      if (ftype^.the_function_pointer_type.is_entry)
        semantic_error ("an entry-point function cannot be called", token.pos);

      if (ftype^.the_function_pointer_type.is_callback)
      {
        if (!lexa.within_unsafe)
          semantic_error ("a callback function can only be called within an unsafe region", token.pos);
      }

      if (ftype^.the_function_pointer_type.extern_dll_or_null != null)
      {
        if (!lexa.within_unsafe)
          semantic_error ("an extern function can only be called within an unsafe region", token.pos);
      }

      if (ftype^.the_function_pointer_type.is_syscall)
      {
        if (!lexa.within_unsafe)
          semantic_error ("a syscall function can only be called within an unsafe region", token.pos);
      }

      param = ftype^.the_function_pointer_type.parameters^.entities.first;
      skip_to_next_param (ref param);
      param_valid = true;
    }
    else
    {
      semantic_error ("prefix must denote a function", token.pos);
      ftype = null;
    }
  }

  if (prefix^.kind == A_CONST_NULL_VALUE)
  {
    semantic_error ("calling null pointer function", token.pos);
    ftype = null;
  }

  get_token();   // skip '('

  pos = token.pos;

  next_param = null;

  if (token.kind != RIGHT_PARENTHESIS)
  {
    for (;;)
    {
      if (param_valid)
      {
        if (param == null)
        {
          semantic_error ("too many parameters", token.pos);
          param_valid = false;
        }
        else if (!in_varying_list)
        {
          // compute next_param
          next_param = param^.next;
          skip_to_next_param (ref next_param);

          // check if param denotes a varying parameter list
          if (next_param == null &&
              types_are_equal (base_type_of (param^.the_parameter.type), type_array_of_object))
          {
            in_varying_list = true;
          }
        }
      }

      if (in_varying_list)
      {
        parse_actual_parameter (    param              => param,
                                    param_valid        => param_valid,
                                    in_varying_list    => true,
                                ref list               => varying_list,
                                    varying_list_count => varying_list.count);
      }
      else
      {
        parse_actual_parameter (    param              => param,
                                    param_valid        => param_valid,
                                    in_varying_list    => false,
                                ref list               => list,
                                    varying_list_count => varying_list.count);
      }

      if (param_valid && (!in_varying_list))
        param = next_param;


      if (token.kind == RIGHT_PARENTHESIS)
        break;

      if (token.kind != COMMA)
      {
        syntax_error ("',' or ')' expected", token.pos);
        break;
      }

      get_token();   // skip comma
    }
  }

  if (token.kind == RIGHT_PARENTHESIS)
    get_token();   // skip ')'


  // check that all formal parameters were processed

  if (param_valid)
  {
    if (param != null)  // some formal parameters were not processed, or varying-list must be closed
    {
      if (in_varying_list)     // a varying-list with at least 1 parameter is being processed and must be closed
      {
        PEXPRESSION e;
        PENTITY     formal_parameter_type;
        CONTEXT     context;

        formal_parameter_type = param^.the_parameter.type;
        convert_type_into_context (formal_parameter_type, out context);

        if (varying_list.count == 1 &&
            varying_list.head^.exp^.base_type_or_null != null &&
            types_are_equal (varying_list.head^.exp^.base_type_or_null, type_array_of_object))
        {
          append_expression (ref list, varying_list.head^.exp, type_array_of_object, varying_list.head^.e);
        }
        else   // a varying list of packed types
        {
          // otherwise append an extra parameter to list : a A_BOXED_ARRAY_OBJECT to 'varying_list'
          e = new EXPRESSION (A_BOXED_ARRAY_OBJECT);
          e^.base_type_or_null = type_array_of_object;
          e^.constraint.kind = CONSTANT_CONSTRAINT;
          e^.constraint.value = (uint)varying_list.count;

          if (context.constraint.kind == CONSTANT_CONSTRAINT &&
              context.constraint.value != (uint)varying_list.count)
            semantic_error ("number of parameters does not match object[]", pos);

          e^.form = AN_OBJECT;
          e^.access = (param^.the_parameter.mode == MODE_IN) ? ACCESS_READONLY : ACCESS_READWRITE;
          e^.boxed_array_object_info.list = varying_list.head;

          append_expression (ref list, e, type_array_of_object, param);
        }
      }
      else   // check if all remaining formal parameters have default expressions
      {      // the last parameter possibly being a varying parameter list

        while (param != null && param^.the_parameter.default_value_or_null != null)
        {
          // append a copy of the default expression to the actual parameter list.

          append_expression (ref list,
                                 duplicate_constant_exp (param^.the_parameter.default_value_or_null),
                                 param^.the_parameter.type,
                                 param);

          param = param^.next;
          skip_to_next_param (ref param);
        }


        // check if there's an additional last parameter of type object[] -> if yes, add an empty table

        if (param != null)
        {
          PENTITY     formal_parameter_type;
          CONTEXT     context;

          formal_parameter_type = param^.the_parameter.type;
          convert_type_into_context (formal_parameter_type, out context);

          next_param = param^.next;
          skip_to_next_param (ref next_param);

          // check if param denotes a varying parameter list
          if (next_param == null &&
              types_are_equal (base_type_of (formal_parameter_type), type_array_of_object))
          {
            PEXPRESSION e;

            // otherwise append an extra parameter to list : a A_BOXED_ARRAY_OBJECT to an empty 'varying_list'
            e = new EXPRESSION (A_BOXED_ARRAY_OBJECT);
            e^.base_type_or_null = type_array_of_object;
            e^.constraint.kind = CONSTANT_CONSTRAINT;
            e^.constraint.value = 0;

            if (context.constraint.kind == CONSTANT_CONSTRAINT &&
                context.constraint.value != 0)
              semantic_error ("number of parameters does not match object[]", pos);

            e^.form = AN_OBJECT;
            e^.access = (param^.the_parameter.mode == MODE_IN) ? ACCESS_READONLY : ACCESS_READWRITE;
            e^.boxed_array_object_info.list  = null;

            append_expression (ref list, e, type_array_of_object, param);


            param = param^.next;
            skip_to_next_param (ref param);
          }
        }


        if (param != null)
          semantic_error ("missing parameters", token.pos);
      }
    }
  }


  // check matching of format string

  if (list.tail != null &&
      list.tail^.exp^.kind == A_BOXED_ARRAY_OBJECT &&   // varying list
      list.tail^.prev != null)
  {
    LIST_OF_EXPRESSIONS^ format;
    PENTITY              type;
    POOL                 p;
    uint4                size;

    format = list.tail^.prev;
    type = format^.exp^.base_type_or_null;

    if (format^.exp^.kind == A_POOL_CONSTANT &&
        type != null &&
        (types_are_equal (type, type_string) ||
         types_are_equal (type, type_wstring)))
    {
      // check if format string matches with varying_list

      p    = format^.exp^.pool_constant_info.pool_cte;
      size = size_of_pool_cte (p);

      check_format_string (p,
                           (int)size,
                           types_are_equal (type, type_wstring),
                           varying_list,
                           list.tail^.exp^.access,
                           pos);
    }
  }


  // create function call

  {
    EXPRESSION^ e = new EXPRESSION (A_FUNCTION_CALL);

    if (ftype != null)
      e^.base_type_or_null = base_type_of (ftype^.the_function_pointer_type.return_type);

    e^.form = A_VALUE;
    e^.access = ACCESS_READONLY;
    e^.function_call_info.func = prefix;
    e^.function_call_info.param = list.head;

    pout = e;
  }
}

/*****************************************************************************/

// used for run call

void count_parameters (    PENTITY param,
                       out int     pcount,
                       out PENTITY pparam)   // returns last A_PARAMETER
{
  PENTITY e = param;
  int     count = 0;

  pparam = null;

  while (e != null)
  {
    if (e^.kind == A_PARAMETER)
    {
      pparam = e;
      count++;
    }
    e = e^.next;
  }

  pcount = count;
}

/*****************************************************************************/

bool is_fixed_object (LIST_OF_EXPRESSIONS^ list)
{
  PEXPRESSION e;

  if (list == null)
    return true;

  e = list^.exp;

  while (e != null && (e^.kind == AN_ARRAY_ELEMENT_OBJECT ||
                       e^.kind == AN_ARRAY_SLICE_OBJECT ||
                       e^.kind == A_STRUCT_FIELD_OBJECT))
  {
    if (e^.kind == AN_ARRAY_ELEMENT_OBJECT)
      e = e^.array_element_object_info.prefix;

    if (e^.kind == AN_ARRAY_SLICE_OBJECT)
      e = e^.array_slice_object_info.prefix;

    if (e^.kind == A_STRUCT_FIELD_OBJECT)
        e = e^.struct_field_object_info.prefix;
  }

  if (e != null)
    return (e^.kind == A_POOL_CONSTANT || e^.kind == A_GLOBAL_VARIABLE_OBJECT);

  return true;  // default in case of earlier error
}

/*****************************************************************************/

// run_call ::= "run"  function_call

void parse_run_call (out PEXPRESSION pout)
{
  TEXT_POSITION                   pos;
  PEXPRESSION                     prefix, exp, exp2;
  PENTITY                         ftype, param1, t;
  int                             count;

  get_token();    // skip token 'run'

  pos = token.pos;

  parse_name (g_no_context, false, token.pos, null, out exp);

  if (exp^.kind != A_FUNCTION_CALL)
    semantic_error ("a function call is expected here", pos);
  else
  {
    // get prefix (a value of function pointer)
    prefix = exp^.function_call_info.func;
    if (prefix != null)
    {
      ftype = prefix^.base_type_or_null;
      if (ftype != null && ftype^.kind == A_FUNCTION_POINTER_TYPE)
      {
        ref A_FUNCTION_POINTER_TYPE_ENTITY pfunc = ftype^.the_function_pointer_type;

        if (pfunc.return_type != type_void)
          semantic_error ("a function returning void is expected here", pos);

        if (pfunc.extern_dll_or_null != null)
          semantic_error ("a function with option extern is not allowed here", pos);

        if (pfunc.is_syscall)
          semantic_error ("a function with option syscall is not allowed here", pos);

        if (pfunc.is_callback)
          semantic_error ("a callback function is not allowed here", pos);

        if (pfunc.is_entry)
          semantic_error ("an entry-point function is not allowed here", pos);

        count_parameters (pfunc.parameters^.entities.first, out count, out param1);

        if (count > 1)
          semantic_error ("only 1 parameter can be passed to a thread", pos);

        if (count == 1)
        {
          t = complete_type_of (param1^.the_parameter.type);

          if (t != null)
          {
            if (is_open_type (t))
              semantic_error ("an open-type formal parameter is not allowed for a thread function", pos);
            else
            {
              t = base_type_of (t);

              if (param1^.the_parameter.mode == MODE_IN)
              {
                if ((t^.kind == AN_INTEGER_TYPE && t != type_long) ||
                    t^.kind == AN_ENUMERATION_TYPE ||
                    t^.kind == A_POINTER_TYPE ||
                    t^.kind == A_FUNCTION_POINTER_TYPE ||
                    t^.kind == AN_UNSAFE_POINTER_TYPE)
                {
                  // ok
                }
                else if (t^.kind == AN_OPEN_ARRAY_TYPE ||
                         t^.kind == A_STRUCT_TYPE ||
                         t^.kind == A_UNION_TYPE)
                {
                  if (!is_fixed_object (exp^.function_call_info.param))
                    semantic_error ("actual parameter of thread function must be a constant or a global variable", pos);
                }
                else
                {
                  semantic_error ("formal parameter of thread function has bad type", pos);
                }
              }
              else     // mode out / ref
              {
                if (!is_fixed_object (exp^.function_call_info.param))
                  semantic_error ("actual parameter of thread function must be a global variable", pos);
              }
            }
          }
        }
      }
    }
  }

  exp2 = new EXPRESSION (A_RUN_CALL);
  exp2^.base_type_or_null = type_int;
  exp2^.form = A_VALUE;
  exp2^.access = ACCESS_READONLY;
  exp2^.run_call_info.function_call = exp;

  pout = exp2;
}

/*****************************************************************************/

//  attribute_id ::= identifier | "string" | "byte"

ATTRIBUTE parse_attr_id ()
{
  int       i;
  ATTRIBUTE attr;

  if (token.kind == IDENTIFIER)
  {
    for (i=0; i<attribute_info'length; i++)
    {
      if (wstrcmp (attribute_info[i].id, token.info._identifier.value) == 0)
        break;
    }

    if (i == attribute_info'length)  // not found
    {
      syntax_error ("unknown attribute", token.pos);
      attr = LAST_ATTRIBUTE;
    }
    else
    {
      attr = attribute_info[i].attr;
    }
  }
  else if (token.kind == TOKEN_byte)
  {
    attr = ATTR_byte;
  }
  else if (token.kind == TOKEN_string)
  {
    attr = ATTR_string;
  }
  else
  {
    syntax_error ("attribute identifier expected", token.pos);
    return LAST_ATTRIBUTE;
  }

  get_token();

  return attr;
}

/*****************************************************************************/

//  attribute    ::= "'" attribute_id ["(" constant_expression ")"]

int parse_array_dimension ()
{
  PEXPRESSION   e;
  TEXT_POSITION pos;
  int8          dim;

  if (token.kind != LEFT_PARENTHESIS)
    return 1;

  get_token();   // skip '('
  pos = token.pos;

  parse_expression (g_no_context, token.pos, null, out e);

  if (e^.kind != A_CONST_INTEGER_VALUE)
  {
    semantic_error ("an integer constant is expected here", pos);
    dim = 1;
  }
  else
  {
    dim = e^.const_integer_value_info.value;

    if (dim < 1 || dim > (1<<30))
    {
      semantic_error ("array dimension is out of valid range", pos);
      dim = 1;
    }

    if (e^.base_type_or_null == type_long)
    {
      semantic_error ("an int or uint constant is expected here", pos);
    }
  }

  if (token.kind == RIGHT_PARENTHESIS)   // ')'
    get_token();   // skip ')'
  else
    syntax_error ("')' expected", token.pos);

  return (int)dim;
}

/*****************************************************************************/

void parse_type_attribute (PENTITY type_name, out PEXPRESSION pout)
{
  PENTITY       type;
  ATTRIBUTE     attr;
  TEXT_POSITION pos;
  PEXPRESSION   n;

  if (token.kind != APOSTROPHE)
  {
    syntax_error ("apostrophe (attribute) expected", token.pos);
    pout = dummy_expression ();
    return;
  }

  pos = token.pos;
  get_token();    // skip apostrophe

  attr = parse_attr_id ();
  if (attr == LAST_ATTRIBUTE)
  {
    pout = dummy_expression ();
    return;
  }

  type = complete_type_of (type_name);

  switch (attr)
  {
    case ATTR_min:
    case ATTR_max:
    {
      INTEGER_TYPE it;

      if (type^.kind != AN_INTEGER_TYPE)
      {
        semantic_error ("prefix must denote an integer type", pos);
        pout = dummy_expression ();
        return;
      }

      it = type^.the_integer_type.type;

      n = new EXPRESSION (A_CONST_INTEGER_VALUE);
      n^.base_type_or_null = type;
      n^.form = A_VALUE;
      n^.access = ACCESS_CONSTANT;
      n^.const_integer_value_info.value =
          (attr == ATTR_min) ? INTEGER_DATA[(uint)it].min : INTEGER_DATA[(uint)it].max;
    }
    break;

    case ATTR_first:
    case ATTR_last:
    {
      if (type^.kind != AN_ENUMERATION_TYPE)
      {
        semantic_error ("prefix must denote an enumeration type", pos);
        pout = dummy_expression ();
        return;
      }

      n = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      n^.base_type_or_null = type;
      n^.form = A_VALUE;
      n^.access = ACCESS_CONSTANT;
      n^.const_enumeration_value_info.value =
           (attr == ATTR_first) ? 0 : type^.the_enumeration_type.last;
    }
    break;

    case ATTR_length:
    {
      PENTITY t;
      int     dim, i;

      if (type^.kind != AN_ARRAY_TYPE)
      {
        semantic_error ("prefix must denote an array", pos);
        pout = dummy_expression ();
        return;
      }

      dim = parse_array_dimension();
      t = type;

      for (i=2; i<=dim; i++)
      {
        t = complete_type_of (t^.the_array_type.open_array);

        if (t == null || t^.kind != AN_OPEN_ARRAY_TYPE)
        {
          t = null;
          break;
        }

        t = complete_type_of (t^.the_open_array_type.element);

        if (t == null || t^.kind != AN_ARRAY_TYPE)
        {
          t = null;
          break;
        }
      }

      if (t == null)
      {
        semantic_error ("prefix must denote an N-dimensional array", pos);
        pout = dummy_expression ();
        return;
      }

      n = new EXPRESSION (A_CONST_INTEGER_VALUE);
      n^.base_type_or_null = type_int;
      n^.form = A_VALUE;
      n^.access = ACCESS_CONSTANT;
      n^.const_integer_value_info.value = t^.the_array_type.length;
    }
    break;

    case ATTR_byte:
      semantic_error ("attribute 'byte requires an object as prefix, not a type", pos);
      pout = dummy_expression ();
      return;

    case ATTR_string:
      semantic_error ("attribute 'string requires a value as prefix, not a type", pos);
      pout = dummy_expression ();
      return;

    case ATTR_size:
    {
      int rc, size, align;

      if (lexa.within_unsafe)
      {
        if (!is_type_having_size (type_name))
        {
          semantic_error ("prefix must not denote a renamed/incomplete/generic/opaque type", pos);
          pout = dummy_expression ();
          return;
        }
      }
      else
      {
        if (!is_packed_type (type_name))
        {
          semantic_error ("prefix must denote a packed type", pos);
          pout = dummy_expression ();
          return;
        }
      }

      if (is_open_type (type_name))
      {
        semantic_error ("prefix must not denote an open type", pos);
        pout = dummy_expression ();
        return;
      }

      // returns 0 if OK, -1 if bad type, -2 if size is too large
      rc = size_and_alignment_of_type (type_name, out size, out align);

      _unused align;

      if (rc == -1)
      {
        semantic_error ("size is not constant", pos);
        pout = dummy_expression ();
        return;
      }
      else if (rc == -2)
      {
        semantic_error ("size is too large", pos);
        pout = dummy_expression ();
        return;
      }

      n = new EXPRESSION (A_CONST_INTEGER_VALUE);
      n^.base_type_or_null = type_uint;
      n^.form = A_VALUE;
      n^.access = ACCESS_CONSTANT;
      n^.const_integer_value_info.value = size;
    }
    break;

    default:
      fatal_compiler_error ("parse_type_attribute", token.pos);
      pout = dummy_expression ();
      return;
  }

  pout = n;
}

/*****************************************************************************/

void parse_attribute (PEXPRESSION prefix, out PEXPRESSION pout)
{
  PENTITY       type;
  ATTRIBUTE     attr;
  TEXT_POSITION pos;
  PEXPRESSION   n;

  if (token.kind != APOSTROPHE)
  {
    syntax_error ("apostrophe (attribute) expected", token.pos);
    pout = dummy_expression ();
    return;
  }

  pos = token.pos;
  get_token();    // skip apostrophe

  attr = parse_attr_id ();
  if (attr == LAST_ATTRIBUTE)
  {
    pout = dummy_expression ();
    return;
  }

  type = prefix^.base_type_or_null;

  if (type == null)    // earlier error occured
  {
    pout = dummy_expression ();
    return;
  }

  switch (attr)
  {
    case ATTR_min:
    case ATTR_max:
    {
      INTEGER_TYPE it;

      if (type^.kind != AN_INTEGER_TYPE || prefix^.form != AN_OBJECT)
      {
        semantic_error ("prefix of attribute 'min/max must denote an integer variable", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }

      if (prefix^.kind == A_CONST_INTEGER_VALUE)
        semantic_error ("only variables are allowed as prefix for this attribute", pos);

      it = type^.the_integer_type.type;

      n = new EXPRESSION (A_CONST_INTEGER_VALUE);
      n^.base_type_or_null = type;
      n^.form = A_VALUE;
      n^.access = ACCESS_CONSTANT;
      n^.const_integer_value_info.value =
              (attr == ATTR_min) ? INTEGER_DATA[(uint)it].min : INTEGER_DATA[(uint)it].max;

      free_exp (prefix);
    }
    break;

    case ATTR_first:
    case ATTR_last:
    {
      if (type^.kind != AN_ENUMERATION_TYPE || prefix^.form != AN_OBJECT)
      {
        semantic_error ("prefix of attribute first/last must denote an enumeration variable", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }

      if (prefix^.kind == A_CONST_ENUMERATION_VALUE)
        semantic_error ("only variables are allowed as prefix for this attribute", pos);

      n = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      n^.base_type_or_null = type;
      n^.form = A_VALUE;
      n^.access = ACCESS_CONSTANT;
      n^.const_enumeration_value_info.value =
           (attr == ATTR_first) ? 0 : type^.the_enumeration_type.last;

      free_exp (prefix);
    }
    break;

    case ATTR_length:
    {
      int dim;

      if (type^.kind != AN_OPEN_ARRAY_TYPE || prefix^.form != AN_OBJECT)
      {
        semantic_error ("prefix of attribute 'length must denote an array object", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }

      dim = parse_array_dimension();

      if (dim == 1)
      {
        if (prefix^.constraint.kind == CONSTANT_CONSTRAINT)
        {
          n = new EXPRESSION (A_CONST_INTEGER_VALUE);
          n^.base_type_or_null = type_int;
          n^.form = A_VALUE;
          n^.access = ACCESS_CONSTANT;
          n^.const_integer_value_info.value = prefix^.constraint.value;

          free_exp (prefix);
        }
        else     // runtime constraint
        {
          n = new EXPRESSION (AN_OPERATOR_VALUE);
          n^.base_type_or_null = type_int;
          n^.form = A_VALUE;
          n^.access = ACCESS_READONLY;
          n^.operator_value_info.op = OP_LENGTH;
          n^.operator_value_info.arg[0] = prefix;
        }
      }
      else   // dim > 1
      {
        PENTITY t;
        int     i;

        t = type;     // an open array type

        t = complete_type_of (t^.the_open_array_type.element);
        if (t == null || t^.kind != AN_ARRAY_TYPE)
          t = null;
        else
        {
          for (i=3; i<=dim; i++)
          {
            t = complete_type_of (t^.the_array_type.open_array);

            if (t == null || t^.kind != AN_OPEN_ARRAY_TYPE)
            {
              t = null;
              break;
            }

            t = complete_type_of (t^.the_open_array_type.element);

            if (t == null || t^.kind != AN_ARRAY_TYPE)
            {
              t = null;
              break;
            }
          }
        }

        if (t == null)
        {
          semantic_error ("prefix must denote an N-dimensional array", pos);
          free_exp (prefix);
          pout = dummy_expression ();
          return;
        }

        n = new EXPRESSION (A_CONST_INTEGER_VALUE);
        n^.base_type_or_null = type_int;
        n^.form = A_VALUE;
        n^.access = ACCESS_CONSTANT;
        n^.const_integer_value_info.value = t^.the_array_type.length;

        free_exp (prefix);
      }
    }
    break;

    case ATTR_byte:
    {
      if (prefix^.form != AN_OBJECT)
      {
        semantic_error ("prefix of attribute 'byte must denote an object", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }

      if (!is_packed_type (type))
      {
        semantic_error ("prefix of attribute 'byte must denote a packed object", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }


      if (is_constant_exp (prefix))
      {
        int  rc, size, align;
        POOL p;

        // create a new pool object of type byte[]

        n = new EXPRESSION (A_POOL_CONSTANT);
        n^.base_type_or_null = type_object;   // byte[]
        n^.form = AN_OBJECT;
        n^.access = ACCESS_CONSTANT;

        // returns 0 if OK, -1 if bad type, -2 if size is too large, -3 if missing/non-constant constraint
        rc = size_and_alignment_of_constant_size_exp (prefix, out size, out align);
        if (rc < 0)
          fatal_compiler_error ("attr_byte : cannot compute size", pos);

        n^.constraint.kind = CONSTANT_CONSTRAINT;
        n^.constraint.value = (uint)size;

        p = new_pool_constant ((uint)size, (uint)align);

        n^.pool_constant_info.pool_cte = p;

        simple_store_constant_into_pool (prefix, size, p);   // simple type or pool constant

        free_exp (prefix);
      }
      else if (prefix^.constraint.kind == DOES_NOT_APPLY ||
               prefix^.constraint.kind == CONSTANT_CONSTRAINT)
      {
        int rc, size, align;

        // runtime with constant size

        n = new EXPRESSION (AN_ATTR_BYTE_OBJECT);
        n^.base_type_or_null = type_object;   // byte[]
        n^.form = AN_OBJECT;
        n^.access = prefix^.access;

        // returns 0 if OK, -1 if bad type, -2 if size is too large, -3 if missing/non-constant constraint
        rc = size_and_alignment_of_constant_size_exp (prefix, out size, out align);
        if (rc < 0)
          fatal_compiler_error ("attr_byte : cannot compute size", pos);

        _unused align;

        n^.constraint.kind = CONSTANT_CONSTRAINT;
        n^.constraint.value = (uint)size;
        n^.attr_byte_object_info.prefix = prefix;
      }
      else    // runtime with runtime size
      {
        n = new EXPRESSION (AN_ATTR_BYTE_OBJECT);
        n^.base_type_or_null = type_object;   // byte[]
        n^.form = AN_OBJECT;
        n^.access = prefix^.access;
        n^.constraint.kind = RUNTIME_CONSTRAINT;
        n^.attr_byte_object_info.prefix = prefix;
      }
    }
    break;

    case ATTR_string:
    {
      if (type^.kind != AN_ENUMERATION_TYPE)
      {
        semantic_error ("prefix of attribute 'string must have an enumeration type", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }

      if (type == type_char || type == type_wchar)
      {
        semantic_error ("prefix of attribute 'string cannot have types char/wchar", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }

      if (prefix^.kind == A_CONST_ENUMERATION_VALUE)
      {
        int   rc, size, align;
        uint4 r;

        r = prefix^.const_enumeration_value_info.value;

        if (r > type^.the_enumeration_type.last)
        {
          semantic_error ("prefix of attribute 'string is too large", pos);
          r = 0;
        }

        // returns 0 if OK, -1 if bad type, -2 if size is too large
        rc = size_and_alignment_of_type (type_string, out size, out align);
        if (rc < 0)
          fatal_compiler_error ("attr_string", token.pos);

        _unused align;

        load_constant_object_from_pool (    pool_cte => type^.the_enumeration_type.string_table,
                                            offset   =>  r * (uint)size,
                                            size     => (uint)size,
                                            type     => type_string,
                                        out pout     => n);
      }
      else   // runtime value
      {
        PEXPRESSION  ex, idx;

        ex = new EXPRESSION (A_POOL_CONSTANT);
        ex^.form   = AN_OBJECT;
        ex^.access = ACCESS_CONSTANT;
        ex^.base_type_or_null = type_array_of_string;
        ex^.constraint.kind = CONSTANT_CONSTRAINT;
        ex^.constraint.value = type^.the_enumeration_type.last + 1;
        ex^.pool_constant_info.pool_cte = type^.the_enumeration_type.string_table;

        idx = new EXPRESSION (AN_OPERATOR_VALUE);
        idx^.form   = A_VALUE;
        idx^.access = ACCESS_READONLY;
        idx^.base_type_or_null = type_int;
        idx^.operator_value_info.op = OP_CONVERT_INT_INT;  // convert enum to int
        idx^.operator_value_info.arg[0] = prefix;

        n = new EXPRESSION (AN_ARRAY_ELEMENT_OBJECT);
        n^.form   = AN_OBJECT;
        n^.access = ACCESS_READONLY;
        n^.base_type_or_null = type_string;
        n^.constraint.kind = RUNTIME_CONSTRAINT;
        n^.array_element_object_info.prefix = ex;
        n^.array_element_object_info.index  = idx;
      }
    }
    break;

    case ATTR_size:
    {
      if (prefix^.form != AN_OBJECT)
      {
        semantic_error ("prefix of attribute 'size must denote an object", pos);
        free_exp (prefix);
        pout = dummy_expression ();
        return;
      }

      if (lexa.within_unsafe)
      {
        if (!is_type_having_size (type))
        {
          semantic_error ("prefix of attribute 'size must not denote a renamed/incomplete/generic/opaque type", pos);
          free_exp (prefix);
          pout = dummy_expression ();
          return;
        }
      }
      else
      {
        if (!is_packed_type (type))
        {
          semantic_error ("prefix of attribute 'size must denote a packed object", pos);
          free_exp (prefix);
          pout = dummy_expression ();
          return;
        }
      }


      // note: 'type' can be an open type.

      if (prefix^.constraint.kind == DOES_NOT_APPLY ||
          prefix^.constraint.kind == CONSTANT_CONSTRAINT)      // constant size
      {
        int rc, size, align;

        // returns 0 if OK, -1 if bad type, -2 if size is too large, -3 if missing/non-constant constraint
        rc = size_and_alignment_of_constant_size_exp (prefix, out size, out align);
        if (rc < 0)
          fatal_compiler_error ("attr_size : cannot compute size", pos);

        _unused align;

        n = new EXPRESSION (A_CONST_INTEGER_VALUE);
        n^.base_type_or_null = type_uint;
        n^.form = A_VALUE;
        n^.access = ACCESS_CONSTANT;
        n^.const_integer_value_info.value = size;

        free_exp (prefix);
      }
      else    // runtime with runtime size
      {
        n = new EXPRESSION (AN_OPERATOR_VALUE);
        n^.base_type_or_null = type_uint;
        n^.form = A_VALUE;
        n^.access = ACCESS_READONLY;
        n^.operator_value_info.op = OP_SIZE;
        n^.operator_value_info.arg[0] = prefix;
      }
    }
    break;

    default:
      fatal_compiler_error ("parse_attribute", token.pos);
      pout = dummy_expression ();
      return;
  }

  pout = n;
}

/*****************************************************************************/

void parse_struct_field_selection (PEXPRESSION e, bool address_of, out PEXPRESSION pout)
{
  PENTITY     discr, field, f, f_type;
  PEXPRESSION n;

  if (e^.base_type_or_null != null)
  {
    if (e^.base_type_or_null^.kind != A_STRUCT_TYPE &&
        e^.base_type_or_null^.kind != A_UNION_TYPE)
    {
      semantic_error ("prefix must denote a struct or union", token.pos);
    }
    else if (e^.form != AN_OBJECT)
    {
      semantic_error ("prefix must denote an object", token.pos);
    }
  }


  //  discriminant_value  ::= "."  discriminant_simple_name
  //  struct_field        ::= "."  field_simple_name

  get_token();    // skip dot or single arrow

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("field identifier expected", token.pos);
    free_exp (e);
    pout = dummy_expression ();
    return;
  }


  // search for identifier in field region

  discr = null;
  field = null;

  if (e^.base_type_or_null != null)
  {
    if (e^.base_type_or_null^.kind == A_STRUCT_TYPE)
    {
      discr = find_tree
                (e^.base_type_or_null^.the_struct_type.discriminant^.identifier_btree,
                 token.info._identifier.value);

      if (discr == null)
      {
        field = find_tree
                 (e^.base_type_or_null^.the_struct_type.fields^.identifier_btree,
                  token.info._identifier.value);
      }
    }
    else if (e^.base_type_or_null^.kind == A_UNION_TYPE)
    {
      field = find_tree
                (e^.base_type_or_null^.the_union_type.fields^.identifier_btree,
                 token.info._identifier.value);
    }
  }

  if (discr == null && field == null)
    semantic_error ("identifier must denote a struct or union field", token.pos);



  // check constraint in case of varying field (if it's constant)

  if (field != null && field^.kind == A_VARYING_FIELD)
  {
    if (e^.constraint.kind == CONSTANT_CONSTRAINT)
    {
      if (e^.constraint.value != field^.the_varying_field.discriminant_value)
        semantic_error ("field does not exist in this struct variant", token.pos);
    }
  }


  // build result node

  if (discr != null)       // it's a discriminant
  {
    if (e^.constraint.kind == CONSTANT_CONSTRAINT)
    {
      n = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      n^.base_type_or_null = complete_type_of (discr^.the_field.type);
      n^.form = A_VALUE;
      n^.access = ACCESS_CONSTANT;
      n^.const_enumeration_value_info.value = e^.constraint.value;

      free_exp (e);
    }
    else     // it's a runtime constraint
    {
      n = new EXPRESSION (A_DISCRIMINANT_VALUE);
      n^.base_type_or_null = complete_type_of (discr^.the_field.type);
      n^.form = A_VALUE;
      n^.access = ACCESS_READONLY;
      n^.discriminant_value_info.prefix = e;
      n^.discriminant_value_info.field = field;
    }
  }
  else if (field != null)   // it's a field
  {
    if (e^.kind == A_POOL_CONSTANT &&   // can only be struct, or open-struct with constant-constraint.
        !address_of)                    // makes sure the whole pool constant is loaded into the executable
    {
      FIELD_DATA  data;

      begin_struct (out data, e^.base_type_or_null);

      f = e^.base_type_or_null^.the_struct_type.fields^.entities.first;

      for (;;)
      {
        skip_to_valid_field (ref f, out f_type, e^.constraint.value);

        begin_field (ref data, f);

        if (f == field)      // found
          break;

        end_field (ref data);

        f = f^.next;
      }

      load_constant_object_from_pool (    pool_cte => e^.pool_constant_info.pool_cte,
                                          offset   => (uint)data.field_offset,
                                          size     => (uint)data.field_size,
                                          type     => f_type,
                                      out pout     => n);
    }
    else            // it's a runtime node
    {
      PENTITY  field_type;

      n = new EXPRESSION (A_STRUCT_FIELD_OBJECT);
      n^.form   = AN_OBJECT;
      n^.access = e^.access;   // constant, readonly, readwrite

      if (field^.kind == A_FIELD)
        field_type = field^.the_field.type;
      else
        field_type = field^.the_varying_field.type;

      convert_type_into_base_type_and_constraint (    type               => field_type,
                                                      always_constrained => true, // (can be jagged)
                                                  out base_type_or_null  => n^.base_type_or_null,
                                                  out cons               => n^.constraint);

      n^.struct_field_object_info.prefix = e;
      n^.struct_field_object_info.field = field;
    }
  }
  else      // some error occured (message already generated) : return dummy expression
  {
    free_exp (e);
    n = dummy_expression ();
  }

  get_token();  // skip identifier

  pout = n;
}

/*****************************************************************************/

//  name ::=
//     | "(" expression ")"
//     | object_name
//     | type_name                    (only used as attribute prefix)
//     | function_name                (returns a function pointer value)
//     | function_call
//     | name  array_element
//     | name  array_slice
//     | name  discriminant_value
//     | name  struct_field
//     | name  dereferenced_object
//     | name  postfixed_object
//     | name  deref_unsafe_field
//     | name  attribute

void parse_name (    CONTEXT       context,                 // never null
                     bool          left_parenthesis_parsed,
                     TEXT_POSITION name_position,           // never null
                     PENTITY       type_name,               // can be null
                 out PEXPRESSION   pout)
{
  PENTITY      name;
  PEXPRESSION  e;

  if (left_parenthesis_parsed)
  {
    parse_expression (context, name_position, type_name, out e);

    if (token.kind == RIGHT_PARENTHESIS)   // ')'
      get_token();   // skip ')'
    else
      syntax_error ("')' expected", token.pos);
  }
  else if (type_name == null && token.kind == LEFT_PARENTHESIS)
  {
    get_token();   // skip '('

    parse_expression (context, token.pos, null, out e);

    if (token.kind == RIGHT_PARENTHESIS)   // ')'
      get_token();   // skip ')'
    else
      syntax_error ("')' expected", token.pos);
  }
  else  // an object, type or function name must follow.
  {
    if (type_name != null)   // name already parsed
    {
      name = type_name;
    }
    else if (token.kind == TOKEN_false)
    {
      name = enum_false;
      get_token();
    }
    else if (token.kind == TOKEN_true)
    {
      name = enum_true;
      get_token();
    }
    else
    {
      name = type_entity_of_token (token.kind);
      if (name != null)
      {
        get_token();    // skip type token
      }
      else if (token.kind == IDENTIFIER)
      {
        name = parse_expanded_name ();
        if (name == null)   // identifier not found (error already given)
        {
          pout = dummy_expression ();
          return;
        }
      }
      else
      {
        syntax_error ("name is expected here", token.pos);
        pout = dummy_expression ();
        return;
      }
    }


    // create expression from name (object_name, type_name, function_name)

    if (name^.kind < LAST_ENTITY_DENOTING_A_TYPE)   // a type
    {
      if (complete_type_of(name) == global_locked_struct_entity)
        semantic_error ("incomplete struct identifier must not be used in expression", name_position);

      parse_type_attribute (name, out e);
    }
    else if (name^.kind == AN_ENUMERATION_LITERAL)
    {
      e = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      e^.base_type_or_null = name^.the_enumeration_literal.type;
      e^.form = A_VALUE;
      e^.access = ACCESS_CONSTANT;
      e^.const_enumeration_value_info.value = name^.the_enumeration_literal.value;
    }
    else if (name^.kind == A_CONSTANT)
    {
      name^.the_constant.is_used = true;     // is referenced

      if (name^.the_constant.value != null &&
          is_constant_exp (name^.the_constant.value))
      {
        e = duplicate_constant_exp (name^.the_constant.value);

        e^.form   = AN_OBJECT;           // convert into constant object
        e^.access = ACCESS_CONSTANT;

        convert_type_into_base_type_and_constraint (    type               => name^.the_constant.type,
                                                        always_constrained => true, // can be jagged
                                                    out base_type_or_null  => e^.base_type_or_null,
                                                    out cons               => e^.constraint);
      }
      else
      {
        e = dummy_expression ();
      }
    }
    else if (name^.kind == A_GLOBAL_VARIABLE)
    {
      e = new EXPRESSION (A_GLOBAL_VARIABLE_OBJECT);
      e^.form   = AN_OBJECT;
      e^.access = ACCESS_READWRITE;
      e^.global_variable_object_info.pobject = name;

      convert_type_into_base_type_and_constraint (    type               => name^.the_global_variable.type,
                                                      always_constrained => true,
                                                  out base_type_or_null  => e^.base_type_or_null,
                                                  out cons               => e^.constraint);
    }
    else if (name^.kind == A_LOCAL_VARIABLE)
    {
      name^.the_local_variable.is_used = true;     // is referenced

      e = new EXPRESSION (A_LOCAL_VARIABLE_OBJECT);
      e^.form   = AN_OBJECT;
      e^.access = ACCESS_READWRITE;
      e^.local_variable_object_info.pobject = name;
      e^.local_variable_object_info.pos = name_position;

      convert_type_into_base_type_and_constraint (    type               => name^.the_local_variable.type,
                                                      always_constrained => true,
                                                  out base_type_or_null  => e^.base_type_or_null,
                                                  out cons               => e^.constraint);
    }
    else if (name^.kind == A_REFERENCE)
    {
      name^.the_reference.is_used = true;     // is referenced

      e = new EXPRESSION (A_REFERENCE_OBJECT);
      e^.form   = AN_OBJECT;
      e^.access = name^.the_reference.mode == MODE_IN ? ACCESS_READONLY : ACCESS_READWRITE;
      e^.reference_object_info.pobject = name;
      e^.reference_object_info.pos = name_position;

      convert_type_into_base_type_and_constraint (    type               => name^.the_reference.type,
                                                      always_constrained => true,
                                                  out base_type_or_null  => e^.base_type_or_null,
                                                  out cons               => e^.constraint);
    }
    else if (name^.kind == A_PARAMETER)
    {
      name^.the_parameter.is_used = true;     // is referenced

      e = new EXPRESSION (A_PARAMETER_OBJECT);
      e^.form   = AN_OBJECT;
      e^.access = name^.the_parameter.mode == MODE_IN ? ACCESS_READONLY : ACCESS_READWRITE;
      e^.parameter_object_info.pobject = name;
      e^.parameter_object_info.pos = name_position;

      convert_type_into_base_type_and_constraint (    type               => name^.the_parameter.type,
                                                      always_constrained => true,
                                                  out base_type_or_null  => e^.base_type_or_null,
                                                  out cons               => e^.constraint);
    }
    else if (name^.kind == A_GENERIC_FUNCTION)
    {
      e = new EXPRESSION (A_FUNCTION_VALUE);
      e^.base_type_or_null = name^.the_generic_function.to_type;
      e^.form   = A_VALUE;
      e^.access = ACCESS_READONLY;
      e^.function_value_info.to_function_declaration_or_generic_function = name;
    }
    else if (name^.kind == A_FUNCTION_DECLARATION)
    {
      e = new EXPRESSION (A_FUNCTION_VALUE);
      e^.base_type_or_null = name^.the_function_declaration.to_type;
      e^.form   = A_VALUE;
      e^.access = ACCESS_READONLY;
      e^.function_value_info.to_function_declaration_or_generic_function = name;
    }
    else
    {
      semantic_error ("a name is expected here", name_position);
      e = dummy_expression ();
    }
  }


  // suffix list

  for (;;)
  {
    switch (token.kind)
    {
      case LEFT_PARENTHESIS:   // function_call
        {
          PEXPRESSION n;
          parse_function_call (name_position, e, out n);
          e = n;
        }
        break;


      case LEFT_BRACKET:   // array_element or array_slice
      {
        PEXPRESSION   e1, e2;
        TEXT_POSITION pos1, pos2;
        bool          valid_range;
        PENTITY       ebt;

        //  array_element ::= "["  expression  "]"
        //  array_slice   ::= "["  expression  ":"  expression "]"


        ebt = e^.base_type_or_null;
        if (ebt != null)
        {
          if (ebt^.kind == AN_OPEN_ARRAY_TYPE)
          {
            if (e^.form != AN_OBJECT)
            {
              semantic_error ("prefix must denote an object", token.pos);
            }
          }
          else if (ebt^.kind == AN_UNSAFE_POINTER_TYPE)
          {
            PENTITY element_type;

            element_type = ebt^.the_unsafe_pointer_type.designated_type;
            if (is_open_type (element_type))
              semantic_error ("prefix must not be an unsafe pointer to an open type", token.pos);

            // ok (accept object or value)
            check_unsafe_region ();
          }
          else
          {
            semantic_error ("prefix must denote an array", token.pos);
          }
        }


        get_token();    // skip [

        pos1 = token.pos;
        parse_expression (g_no_context, token.pos, null, out e1);

        valid_range = true;


        // check type (int or uint)

        if (e1^.base_type_or_null != null)
        {
          if (e1^.base_type_or_null^.kind != AN_INTEGER_TYPE ||
              e1^.base_type_or_null == type_long)
          {
            semantic_error ("index must have type int or uint", pos1);
          }
        }


        if (token.kind != COLON)   // an array element
        {
          // if index is constant, check that it's in int or uint range.

          if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE && e1^.kind == A_CONST_INTEGER_VALUE)
          {
            int8 r;

            r = e1^.const_integer_value_info.value;

            if (r < 0 || r >= 2147483647)
            {
              semantic_error ("index is out of range", pos1);
              valid_range = false;
            }
            else
            {
              // if the array has constant bounds, check them.
              if (e^.constraint.kind == CONSTANT_CONSTRAINT && r >= e^.constraint.value)
              {
                semantic_error ("index is too large", pos1);
                valid_range = false;
              }
            }
          }


          // build result node : either a pool constant, a simple constant, or a runtime.

          if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE && valid_range &&
              e^.kind == A_POOL_CONSTANT && e1^.kind == A_CONST_INTEGER_VALUE &&
              !context.address_of)     // makes sure the whole pool constant is loaded into the executable
          {
            PENTITY     element_type;
            int         elem_size, align, rc, offset;
            int8        r;
            PEXPRESSION n;

            // it's a constant node (result is either a pool constant or a simple constant)

            element_type = complete_type_of (e^.base_type_or_null^.the_open_array_type.element);

            // returns 0 if OK, -1 if bad type, -2 if size is too large
            rc = size_and_alignment_of_type (element_type, out elem_size, out align);
            if (rc < 0)
              fatal_compiler_error ("array_index(elem_type)", token.pos);

            _unused align;

            r = e1^.const_integer_value_info.value;

            offset = (int)r * elem_size;

            load_constant_object_from_pool (    e^.pool_constant_info.pool_cte,
                                                (uint)offset,
                                                (uint)elem_size,
                                                element_type,
                                            out n);
            free_exp (e);
            free_exp (e1);

            e = n;
          }
          else            // it's a runtime node
          {
            PEXPRESSION n;
            PENTITY     element_type;

            n = new EXPRESSION (AN_ARRAY_ELEMENT_OBJECT);
            n^.form   = AN_OBJECT;

            if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE)
            {
              element_type = ebt^.the_open_array_type.element;
              n^.access = e^.access;   // constant, readonly, readwrite
            }
            else if (ebt != null && ebt^.kind == AN_UNSAFE_POINTER_TYPE)
            {
              element_type = ebt^.the_unsafe_pointer_type.designated_type;
              n^.access = ACCESS_READWRITE;
            }
            else
            {
              element_type = null;
              n^.access = e^.access;   // constant, readonly, readwrite
            }

            convert_type_into_base_type_and_constraint (    element_type,
                                                            always_constrained => true, // (can be jagged)
                                                        out n^.base_type_or_null,
                                                        out n^.constraint);

            n^.array_element_object_info.prefix = e;
            n^.array_element_object_info.index  = e1;

            e = n;
          }
        }
        else             // an array slice
        {
          get_token();   // skip ':'
          pos2 = token.pos;
          parse_expression (g_no_context, token.pos, null, out e2);


          // check type (int or uint)

          if (e2^.base_type_or_null != null)
          {
            if (e2^.base_type_or_null^.kind != AN_INTEGER_TYPE ||
                e2^.base_type_or_null == type_long)
            {
              semantic_error ("index must have type int or uint", pos2);
            }
          }


          // if slice offset is constant, check that it's in int or uint range.

          if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE && e1^.kind == A_CONST_INTEGER_VALUE)
          {
            int8 r;

            r = e1^.const_integer_value_info.value;

            if (r < 0 || r > 2147483647)
            {
              semantic_error ("slice offset is out of range", pos1);
              valid_range = false;
            }
            else
            {
              // if the array has constant bounds, check them.
              if (e^.constraint.kind == CONSTANT_CONSTRAINT && r > e^.constraint.value)
              {
                semantic_error ("slice offset is too large", pos1);
                valid_range = false;
              }
            }
          }


          // if slice length is constant, check that it's in int or uint range.

          if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE && e2^.kind == A_CONST_INTEGER_VALUE)
          {
            int8 r;

            r = e2^.const_integer_value_info.value;

            if (r < 0 || r > 2147483647)
            {
              semantic_error ("slice length is out of range", pos2);
              valid_range = false;
            }
            else
            {
              // if the slice length has constant bounds, check them.
              if (e^.constraint.kind == CONSTANT_CONSTRAINT && r > e^.constraint.value)
              {
                semantic_error ("slice length is too large", pos2);
                valid_range = false;
              }
            }
          }


          // if slice offset and length are constant, check that both are <= array'length

          if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE &&
              valid_range &&
              e1^.kind == A_CONST_INTEGER_VALUE &&
              e2^.kind == A_CONST_INTEGER_VALUE &&
              e^.constraint.kind == CONSTANT_CONSTRAINT &&
              e1^.const_integer_value_info.value
               + e2^.const_integer_value_info.value
                 > e^.constraint.value)
          {
            semantic_error ("slice offset+length is too large", pos1);
            valid_range = false;
          }


          // build result node : either a pool constant, or a runtime slice.

          if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE &&
              valid_range &&
              e^.kind == A_POOL_CONSTANT &&
              e1^.kind == A_CONST_INTEGER_VALUE &&
              e2^.kind == A_CONST_INTEGER_VALUE &&
              !context.address_of)     // makes sure the whole pool constant is loaded into the executable
          {
            PENTITY     element_type;
            int         elem_size, align, rc;
            POOL        p;
            int8        ofs, len;
            PEXPRESSION n;

            // it's a constant pool constant

            element_type = complete_type_of (e^.base_type_or_null^.the_open_array_type.element);

            // returns 0 if OK, -1 if bad type, -2 if size is too large
            rc = size_and_alignment_of_type (element_type, out elem_size, out align);
            if (rc < 0)
              fatal_compiler_error ("array_index(elem_type)", token.pos);

            ofs = e1^.const_integer_value_info.value;
            len = e2^.const_integer_value_info.value;

            n = new EXPRESSION (A_POOL_CONSTANT);
            n^.form = AN_OBJECT;
            n^.access = ACCESS_CONSTANT;
            n^.base_type_or_null = e^.base_type_or_null;
            n^.constraint.kind = CONSTANT_CONSTRAINT;
            n^.constraint.value = (uint4)len;

            p = new_pool_constant ((uint4)len * (uint4)elem_size, (uint4)align);

            copy_pool_to_pool (e^.pool_constant_info.pool_cte, (uint4)ofs * (uint4)elem_size,
                               p, 0,
                               (uint4)len * (uint4)elem_size);

            n^.pool_constant_info.pool_cte = p;

            free_exp (e);
            free_exp (e1);
            free_exp (e2);

            e = n;
          }
          else            // it's a runtime node
          {
            PEXPRESSION n;

            n = new EXPRESSION (AN_ARRAY_SLICE_OBJECT);
            n^.form   = AN_OBJECT;

            if (ebt != null && ebt^.kind == AN_OPEN_ARRAY_TYPE)
            {
              n^.base_type_or_null = ebt;
              n^.access = e^.access;   // constant, readonly, readwrite
            }
            else if (ebt != null && ebt^.kind == AN_UNSAFE_POINTER_TYPE)
            {
              PENTITY                     element_type;
              ENTITY(AN_OPEN_ARRAY_TYPE)  open_array;

              element_type = ebt^.the_unsafe_pointer_type.designated_type;

              clear open_array;
              open_array.the_open_array_type.element = element_type;

              n^.base_type_or_null = append_new_entity (open_array, L"", token.pos);

              n^.access = ACCESS_READWRITE;
            }
            else
            {
              n^.base_type_or_null = null;
              n^.access = e^.access;   // constant, readonly, readwrite
            }

            if (valid_range && e2^.kind == A_CONST_INTEGER_VALUE)
            {
              n^.constraint.kind = CONSTANT_CONSTRAINT;
              n^.constraint.value = (uint4)e2^.const_integer_value_info.value;
            }
            else
            {
              n^.constraint.kind = RUNTIME_CONSTRAINT;
            }

            n^.array_slice_object_info.prefix = e;
            n^.array_slice_object_info.index  = e1;
            n^.array_slice_object_info.length = e2;

            e = n;
          }
        }

        if (token.kind == RIGHT_BRACKET)   // ']'
          get_token();   // skip ']'
        else
          syntax_error ("']' expected", token.pos);

        break;
      }


      case DOT:             // discriminant_value or struct_field
      {
        PEXPRESSION n;

        parse_struct_field_selection (e, context.address_of, out n);

        e = n;
      }
      break;


      case MINUS_MINUS:        // postfixed_object
      case PLUS_PLUS:
      {
        PEXPRESSION n;
        PENTITY     type;

        //  postfixed_object    ::= "--" | "++"

        type = e^.base_type_or_null;

        if (e^.access != ACCESS_READWRITE)
        {
          semantic_error ("prefix must denote a read/write object", token.pos);
        }
        else if (e^.form != AN_OBJECT)
        {
          semantic_error ("prefix must denote an object", token.pos);
        }
        else if (type != null)
        {
          if (type^.kind != AN_INTEGER_TYPE &&
              type^.kind != AN_ENUMERATION_TYPE &&
              type^.kind != AN_UNSAFE_POINTER_TYPE)
          {
            semantic_error ("prefix must denote enum, integer or unsafe pointer object", token.pos);
          }
        }

        n = new EXPRESSION (AN_OPERATOR_VALUE);
        n^.base_type_or_null = type;
        n^.form = A_VALUE;
        n^.access = ACCESS_READONLY;
        n^.operator_value_info.op = (token.kind == MINUS_MINUS) ? OP_POST_DEC : OP_POST_INC;
        n^.operator_value_info.arg[0] = e;

        e = n;

        get_token();
      }
      break;


      case SINGLE_ARROW:       // deref_unsafe_field
      {
        PEXPRESSION n;
        PENTITY     type;

        //  deref_unsafe_field  ::= "->" field_simple_name

        type = e^.base_type_or_null;
        if (type != null && type^.kind != AN_UNSAFE_POINTER_TYPE)
          semantic_error ("prefix must be unsafe pointer", token.pos);

        if (e^.kind == A_CONST_NULL_VALUE)
          semantic_error ("dereferencing null pointer is not allowed", token.pos);

        n = new EXPRESSION (AN_UNSAFE_DEREFERENCED_OBJECT);
        n^.form = AN_OBJECT;
        n^.access = ACCESS_READWRITE;
        n^.unsafe_dereferenced_object_info.unsafe_ptr_value = e;

        if (type != null && type^.kind == AN_UNSAFE_POINTER_TYPE)
        {
          PENTITY dt;

          check_unsafe_region ();

          dt = type^.the_unsafe_pointer_type.designated_type;
          if (is_open_type (dt))
            semantic_error ("left operand of '^.' must not be an unsafe pointer to an open type", token.pos);

          convert_type_into_base_type_and_constraint
                  (    type^.the_unsafe_pointer_type.designated_type,
                       always_constrained => true,
                   out n^.base_type_or_null,
                   out n^.constraint);
        }

        e = n;

        parse_struct_field_selection (e, false, out n);
        e = n;
      }
      break;

      case APOSTROPHE:         // attribute
      {
        PEXPRESSION n;
        parse_attribute (e, out n);
        e = n;
      }
      break;

      default:
      {
        if (token.kind == CARET &&                 //  dereferenced_object ::= "^"
            e^.base_type_or_null != null &&
            e^.base_type_or_null^.kind == A_POINTER_TYPE)
        {
          PEXPRESSION n;

          if (e^.kind == A_CONST_NULL_VALUE)
            semantic_error ("dereferencing a null pointer is not allowed", token.pos);

          n = new EXPRESSION (A_DEREFERENCED_OBJECT);
          n^.form   = AN_OBJECT;
          n^.access = ACCESS_READWRITE;

          convert_type_into_base_type_and_constraint (    e^.base_type_or_null^.the_pointer_type.designated_type,
                                                          always_constrained => true,
                                                      out n^.base_type_or_null,
                                                      out n^.constraint);
          n^.dereferenced_object_info.prefix = e;

          e = n;

          get_token();   // skip caret

          break;
        }

        if (is_unsafe_type (e^.base_type_or_null))
          check_unsafe_region ();

        pout = e;
        return;
      }
    }
  }
}

/*****************************************************************************/

// returns 0 if OK, -1 if overflow error

int check_float_overflow_underflow (ref double rf, PENTITY t, TEXT_POSITION pos)
{
  bool overflow  = false;
  bool underflow = false;

  if (t == type_float)
  {
    float f = (float)rf;
    if (fisinfinite (f))
      overflow = true;
    else if (f == 0.0 && rf != 0.0)
      underflow = true;
    rf = f;      // reduce precision (compiler note: be careful that this is not optimized away)
  }
  else  // double
  {
    if (isinfinite (rf))
    {
      overflow = true;
    }
  }

  if (overflow)
  {
    semantic_error ("overflow", pos);
    rf = 0.0;
    return -1;
  }

  if (underflow)
    warning ("underflow", pos);

  return 0;
}

/*****************************************************************************/

// returns 0 if OK, -1 if types are not compatible
// error message is generated in function
// note: this is also used for checking compatibility of in-parameters,
//       opaque types are allowed.

public
int check_assignment_context_compatibility (CONTEXT target, PEXPRESSION source, TEXT_POSITION pos)
{
  PENTITY s, t;

  s = complete_type_of (source^.base_type_or_null);
  t = complete_type_of (target.base_type_or_null);

  if (s == null || t == null)   // previous error
    return -1;

  switch (t^.kind)
  {
    case AN_INTEGER_TYPE:
      if (s^.kind != AN_INTEGER_TYPE)
      {
        semantic_error ("type is not compatible with integer", pos);
        return -1;
      }

      if (s == type_int_literal)   // a constant
      {
        int8 r;

        if (source^.kind != A_CONST_INTEGER_VALUE)
          fatal_compiler_error ("int-literal#1", pos);

        r = source^.const_integer_value_info.value;

        {
          ref INTEGER_INFO pt = INTEGER_DATA[(uint)t^.the_integer_type.type];
          if (r < pt.min || r > pt.max)
          {
            semantic_error ("overflow", pos);
            return -1;
          }
        }

        return 0;
      }


      // check that source type fits into target type

      {
        ref INTEGER_INFO ps = INTEGER_DATA[(uint)s^.the_integer_type.type];
        ref INTEGER_INFO pt = INTEGER_DATA[(uint)t^.the_integer_type.type];

        if (ps.min < pt.min || ps.max > pt.max)
        {
          semantic_error ("type does not fit into target type - use conversion", pos);
          return -1;
        }

      }

      return 0;

    case A_FLOAT_TYPE:
      if (s^.kind != A_FLOAT_TYPE)
      {
        semantic_error ("type is not compatible with floating-point", pos);
        return -1;
      }

      if (s == type_float_literal)   // a constant
      {
        double f;

        if (source^.kind != A_CONST_FLOAT_VALUE)
          fatal_compiler_error ("float-literal#1", pos);

        f = source^.const_float_value_info.value;

        if (check_float_overflow_underflow (ref f, t, pos) < 0)
          return -1;

        return 0;
      }


      // check that source type fits into target type

      if (s == type_double && t == type_float)
      {
        semantic_error ("type does not fit into target type - use conversion", pos);
        return -1;
      }

      return 0;

    case AN_OPEN_ARRAY_TYPE:
      if (s^.kind != AN_OPEN_ARRAY_TYPE || !types_are_equal (s, t))
      {
        semantic_error ("type is not compatible with array", pos);
        return -1;
      }

      if (source^.constraint.kind == CONSTANT_CONSTRAINT &&
          target.constraint.kind == CONSTANT_CONSTRAINT &&
          source^.constraint.value != target.constraint.value)
      {
        semantic_error ("array length does not match", pos);
        return -1;
      }
      return 0;

    case A_STRUCT_TYPE:       // can be open
      if (s^.kind != A_STRUCT_TYPE || !types_are_equal (s, t))
      {
        semantic_error ("type is not compatible with struct", pos);
        return -1;
      }

      if (source^.constraint.kind == CONSTANT_CONSTRAINT &&
          target.constraint.kind == CONSTANT_CONSTRAINT &&
          source^.constraint.value != target.constraint.value)
      {
        semantic_error ("struct discriminant does not match", pos);
        return -1;
      }
      return 0;

    case AN_ENUMERATION_TYPE:
    case A_UNION_TYPE:
    case AN_INCOMPLETE_TYPE:
    case A_GENERIC_TYPE:
    case AN_OPAQUE_TYPE:
      if (!types_are_equal (s, t))
      {
        semantic_error ("type is not compatible", pos);
        return -1;
      }
      return 0;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      if (s^.kind == A_NULL_POINTER_TYPE)
      {
        // null is compatible with any pointer kind
      }
      else if (!types_are_equal (s, t))
      {
        semantic_error ("type is not compatible", pos);
        return -1;
      }
      return 0;

    default:
      semantic_error ("type is not compatible", pos);
      return -1;
  }
}

/*****************************************************************************/

// all checks were done before, no error should occur

void store_constant_into_aggregate_pool (CONTEXT target, PEXPRESSION source, POOL pool, uint offset)
{
  PENTITY      s, t;
  int8         r;
  double       rf;
  POOL         ps;

  s = complete_type_of (source^.base_type_or_null);
  t = complete_type_of (target.base_type_or_null);

  switch (t^.kind)
  {
    case AN_ENUMERATION_TYPE:
      if (s^.kind != AN_ENUMERATION_TYPE || source^.kind != A_CONST_ENUMERATION_VALUE)
        fatal_compiler_error ("exp#791", token.pos);

      r = source^.const_enumeration_value_info.value;

      {
        ref INTEGER_INFO pi = INTEGER_DATA[(uint)t^.the_enumeration_type.base];
        store_integer (pool, offset, r, (uint)pi.size);
      }

      return;


    case AN_INTEGER_TYPE:
      if (s^.kind != AN_INTEGER_TYPE || source^.kind != A_CONST_INTEGER_VALUE)
        fatal_compiler_error ("exp#804", token.pos);

      r = source^.const_integer_value_info.value;

      {
        ref INTEGER_INFO pi = INTEGER_DATA[(uint)t^.the_integer_type.type];
        store_integer (pool, offset, r, (uint)pi.size);
      }

      return;


    case A_FLOAT_TYPE:
      if (s^.kind != A_FLOAT_TYPE || source^.kind != A_CONST_FLOAT_VALUE)
        fatal_compiler_error ("exp#817", token.pos);

      rf = source^.const_float_value_info.value;

      {
        ref FLOAT_INFO pf = FLOAT_DATA[(uint)t^.the_float_type.type];
        store_float (pool, offset, rf, (uint)pf.size);
      }

      return;


    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      if (source^.kind != A_CONST_NULL_VALUE)
        fatal_compiler_error ("exp#834", token.pos);

      // nothing to do : pool zone is zeroed by default

      return;

    case AN_OPEN_ARRAY_TYPE:
    case A_STRUCT_TYPE:       // can be open
      if (source^.kind != A_POOL_CONSTANT)
        fatal_compiler_error ("exp#867", token.pos);

      ps = source^.pool_constant_info.pool_cte;

      if (target.constraint.kind == CONSTANT_CONSTRAINT ||  // array with/struct length constraint
          target.constraint.kind == DOES_NOT_APPLY)         // non-open struct
      {
        copy_pool_to_pool (ps, 0, pool, offset, size_of_pool_cte (ps));  // copy of pool zone
      }
      else    // open array/struct : jagged reference (store address & length/discriminant)
      {
        store_reference (pool, offset, ps);         // jagged reference
        store_integer (pool, offset+(uint)address_size, source^.constraint.value, 4);
      }
      return;


    default:
      fatal_compiler_error ("exp#895", token.pos);
      break;
  }
}

/*****************************************************************************/

//  positional_array_aggregate ::=
//              "{" [ expression {"," expression} [","] ] "}"
//
//  positional_struct_aggregate ::=
//              "{" [  expression {"," expression} [","]  ] "}"

void parse_non_named_aggregate (    CONTEXT       context,        // never null
                                    bool          context_ok,
                                    TEXT_POSITION paggregate_pos,
                                out PEXPRESSION   pout)
{
  bool ok = context_ok;

  if (ok && context.base_type_or_null^.kind == AN_OPEN_ARRAY_TYPE)
  {
    PENTITY       element_type;
    CONTEXT       element_context;
    int           length = 0;
    int           elem_size, size, align, rc;
    PEXPRESSION   e;
    EXPRESSIONS   list;
    TEXT_POSITION pos;
    bool          is_constant = true;

    clear (list);
    clear size, align, elem_size;

    // compute element type
    element_type = complete_type_of (context.base_type_or_null^.the_open_array_type.element);
    convert_type_into_context (element_type, out element_context);

    while (token.kind != RIGHT_ACCOLADE)
    {
      pos = token.pos;
      parse_expression (element_context, pos, null, out e);
      if (length == 2147483647)
      {
        semantic_error ("too many array elements", token.pos);
        length = 0;
        ok = false;
      }
      length++;

      if (check_assignment_context_compatibility (element_context, e, pos) < 0)
        ok = false;
      is_constant &= is_constant_exp (e);
      append_expression (ref list, e, element_type, null);

      if (token.kind == RIGHT_ACCOLADE)
        break;

      if (token.kind != COMMA)
      {
        syntax_error ("',' or '}' expected", token.pos);
        break;
      }

      get_token();
    }

    if (!is_constant)
    {
      if (context.base_type_or_null != null && is_jagged_type (context.base_type_or_null))
        semantic_error ("jagged aggregates must be constant", paggregate_pos);
    }

    if (is_constant && ok)
    {
      // returns 0 if OK, -1 if bad type, -2 if size is too large
      rc = size_and_alignment_of_type (element_type, out elem_size, out align);
      if (rc == -1)
      {
        semantic_error ("array type is not constant", paggregate_pos);
        ok = false;
      }
      else if (rc == -2 || mul_int4 (elem_size, length, out size) < 0)
      {
        semantic_error ("aggregate is too large", paggregate_pos);
        ok = false;
      }
    }

    if (is_constant && ok)
    {
      POOL                 p;
      LIST_OF_EXPRESSIONS^ l;
      uint4                offset;

      e = new EXPRESSION (A_POOL_CONSTANT);
      e^.base_type_or_null = context.base_type_or_null;
      e^.constraint.kind = CONSTANT_CONSTRAINT;
      e^.constraint.value = (uint)length;
      e^.form = A_VALUE;
      e^.access = ACCESS_CONSTANT;

      p = new_pool_constant ((uint)size, (uint)align);

      e^.pool_constant_info.pool_cte = p;

      l = list.head;
      offset = 0;
      while (l != null)
      {
        store_constant_into_aggregate_pool (element_context, l^.exp, p, offset);
        offset += (uint)elem_size;
        l = l^.next;
      }

      free_list (ref list);
    }
    else     // runtime aggregate
    {
      e = new EXPRESSION (AN_AGGREGATE_VALUE);
      e^.base_type_or_null = ok ? context.base_type_or_null : null;
      e^.constraint.kind = CONSTANT_CONSTRAINT;
      e^.constraint.value = (uint)length;
      e^.form = A_VALUE;
      e^.access = ACCESS_READONLY;
      e^.aggregate_value_info.list = list.head;
    }

    pout = e;
  }
  else   // struct, or open-struct with constant constraint, or error.
  {
    PENTITY       field = null;
    PENTITY       field_type = null;
    CONTEXT       field_context;
    PEXPRESSION   e;
    EXPRESSIONS   list;
    TEXT_POSITION pos;
    bool          is_constant = true;
    FIELD_DATA    data;

    clear list, data;

    if (ok)
    {
      field = context.base_type_or_null^.the_struct_type.fields^.entities.first;
      skip_to_valid_field (ref field, out field_type, context.constraint.value);
    }

    while (token.kind != RIGHT_ACCOLADE)
    {
      if (ok && field == null)
      {
        semantic_error ("too many expressions in aggregate", token.pos);
        ok = false;
      }

      convert_type_into_context (field_type, out field_context);

      pos = token.pos;
      parse_expression (field_context, pos, null, out e);

      if (ok)
      {
        if (check_assignment_context_compatibility (field_context, e, pos) < 0)
          ok = false;
        is_constant &= is_constant_exp (e);
        append_expression (ref list, e, field_type, field);

        if (field != null)
          field = field^.next;
        skip_to_valid_field (ref field, out field_type, context.constraint.value);
      }
      else
      {
        free_exp (e);
      }

      if (token.kind == RIGHT_ACCOLADE)
        break;

      if (token.kind != COMMA)
      {
        syntax_error ("',' or '}' expected", token.pos);
        break;
      }

      get_token();
    }


    if (ok && field != null)
    {
      semantic_error ("missing fields in aggregate", token.pos);
      ok = false;
    }


    if (!is_constant)
    {
      if (context.base_type_or_null != null && is_jagged_type (context.base_type_or_null))
        semantic_error ("jagged aggregates must be constant", paggregate_pos);
    }


    if (is_constant && ok)
    {
      LIST_OF_EXPRESSIONS^ l;

      begin_struct (out data, context.base_type_or_null);

      l = list.head;
      while (l != null)
      {
        begin_field (ref data, l^.e);
        end_field (ref data);
        l = l^.next;
      }

      end_struct (ref data);

      if (data.invalid_type)
      {
        semantic_error ("struct type is not constant", paggregate_pos);
        ok = false;
      }
      else if (data.size_overflow)
      {
        semantic_error ("aggregate is too large", paggregate_pos);
        ok = false;
      }
    }

    if (is_constant && ok)
    {
      POOL p;
      LIST_OF_EXPRESSIONS^ l;

      e = new EXPRESSION (A_POOL_CONSTANT);
      e^.base_type_or_null = context.base_type_or_null;

      if (context.base_type_or_null^.kind == A_STRUCT_TYPE &&
          context.base_type_or_null^.the_struct_type.is_open_type)
      {
        e^.constraint.kind = CONSTANT_CONSTRAINT;
        e^.constraint.value = context.constraint.value;
      }

      e^.form = A_VALUE;
      e^.access = ACCESS_CONSTANT;

      p = new_pool_constant ((uint)data.struct_size, (uint)data.struct_alignment);

      e^.pool_constant_info.pool_cte = p;

      begin_struct (out data, context.base_type_or_null);

      l = list.head;
      while (l != null)
      {
        begin_field (ref data, l^.e);

        convert_type_into_context (l^.type, out field_context);
        store_constant_into_aggregate_pool (field_context, l^.exp, p, (uint)data.field_offset);

        end_field (ref data);
        l = l^.next;
      }

      end_struct (ref data);

      free_list (ref list);
    }
    else     // runtime aggregate
    {
      e = new EXPRESSION (AN_AGGREGATE_VALUE);
      e^.base_type_or_null = ok ? context.base_type_or_null : null;

      if (context.base_type_or_null != null &&
          context.base_type_or_null^.kind == A_STRUCT_TYPE &&
          context.base_type_or_null^.the_struct_type.is_open_type)
      {
        e^.constraint.kind = CONSTANT_CONSTRAINT;
        e^.constraint.value = context.constraint.value;
      }

      e^.form   = A_VALUE;
      e^.access = ACCESS_READONLY;
      e^.aggregate_value_info.list = list.head;
    }

    pout = e;
  }

  if (token.kind == RIGHT_ACCOLADE)
    get_token();
}

/*****************************************************************************/

//  open_array_aggregate ::= "{"  "all"  "=>"  expression  "}"
//
//  named_struct_aggregate ::=
//         "{"  [ identifier "=>" expression
//                 {"," identifier "=>" expression} [","]  ] "}"

void parse_named_aggregate (    CONTEXT       context,        // never null
                                bool          context_ok,
                                TEXT_POSITION paggregate_pos,
                            out PEXPRESSION   pout)
{
  bool ok = context_ok;

  if (ok && context.base_type_or_null^.kind == AN_OPEN_ARRAY_TYPE)
  {
    PEXPRESSION   e, e2;
    PENTITY       element_type;
    CONTEXT       element_context;
    uint          length = 0;
    TEXT_POSITION pos;
    bool          is_constant = true;
    int           elem_size=0, size=0;
    int           align=0, rc;

    if (token.kind != IDENTIFIER || wstrcmp (token.info._identifier.value, L"all") != 0)
    {
      syntax_error ("'all' is expected for an array aggregate", token.pos);
      pout = dummy_expression ();
      return;
    }

    if (context.constraint.kind != CONSTANT_CONSTRAINT &&
        context.constraint.kind != RUNTIME_CONSTRAINT)
    {
      syntax_error ("an array type with constraint is required as context", paggregate_pos);
      ok = false;
    }

    get_token();

    if (token.kind != ARROW)
    {
      syntax_error ("'=>' expected", token.pos);
      pout = dummy_expression ();
      return;
    }

    get_token();

    // compute element type
    element_type = complete_type_of (context.base_type_or_null^.the_open_array_type.element);

    convert_type_into_context (element_type, out element_context);

    pos = token.pos;
    parse_expression (element_context, pos, null, out e);
    is_constant &= is_constant_exp (e);
    if (check_assignment_context_compatibility (element_context, e, pos) < 0)
      ok = false;

    if (token.kind != RIGHT_ACCOLADE)
      syntax_error ("'}' expected", token.pos);


    if (!is_constant)
    {
      if (context.base_type_or_null != null && is_jagged_type (context.base_type_or_null))
        semantic_error ("jagged aggregates must be constant", paggregate_pos);
    }


    if (context.constraint.kind == CONSTANT_CONSTRAINT && is_constant && ok)
    {
      length = context.constraint.value;

      // returns 0 if OK, -1 if bad type, -2 if size is too large
      rc = size_and_alignment_of_type (element_type, out elem_size, out align);
      if (rc == -1)
      {
        semantic_error ("array type is not constant", paggregate_pos);
        ok = false;
      }
      else if (rc == -2 || mul_int4 (elem_size, (int)length, out size) < 0)
      {
        semantic_error ("aggregate is too large", paggregate_pos);
        ok = false;
      }
    }


    // if constant context length and constant expression, it's a constant array pool object.

    if (context.constraint.kind == CONSTANT_CONSTRAINT && is_constant && ok)
    {
      POOL  p;
      uint4 offset, i;

      e2 = new EXPRESSION (A_POOL_CONSTANT);
      e2^.base_type_or_null = context.base_type_or_null;
      e2^.constraint.kind = CONSTANT_CONSTRAINT;
      e2^.constraint.value = length;
      e2^.form = A_VALUE;
      e2^.access = ACCESS_CONSTANT;

//      if (length > 256)
//        warning ("constant array takes a lot of ram", paggregate_pos);

      p = new_pool_constant ((uint)size, (uint)align);

      e2^.pool_constant_info.pool_cte = p;

      offset = 0;
      for (i=0; i<length; i++)
      {
        store_constant_into_aggregate_pool (element_context, e, p, offset);
        offset += (uint)elem_size;
      }

      e = e2;
    }
    else     // runtime aggregate
    {
      e2 = new EXPRESSION (AN_UNC_ARRAY_AGGREGATE);
      e2^.base_type_or_null = ok ? context.base_type_or_null : null;
      e2^.constraint = context.constraint;
      e2^.form = A_VALUE;
      e2^.access = ACCESS_READONLY;
      e2^.unc_array_aggregate_info.element = e;

      e = e2;
    }

    pout = e;
  }
  else   // struct, or open-struct with constant constraint, or error.
  {
    PENTITY       field = null;
    PENTITY       field_type = null;
    CONTEXT       field_context;
    PEXPRESSION   e;
    EXPRESSIONS   list;
    TEXT_POSITION pos;
    bool          is_constant = true;
    FIELD_DATA    data;

    clear list, data;

    if (ok)
    {
      field = context.base_type_or_null^.the_struct_type.fields^.entities.first;
      skip_to_valid_field (ref field, out field_type, context.constraint.value);
    }

    while (token.kind != RIGHT_ACCOLADE)
    {
      if (ok && field == null)
      {
        semantic_error ("too many expressions in aggregate", token.pos);
        ok = false;
      }

      if (token.kind != IDENTIFIER)
      {
        syntax_error ("'identifier =>' expected", token.pos);
        break;
      }

      if (field != null &&
          field^.identifier_or_null != null &&
          wstrcmp (token.info._identifier.value, field^.identifier_or_null^) != 0)
      {
        syntax_error ("identifier is not a struct field", token.pos);
        ok = false;
        break;
      }

      get_token();

      if (token.kind != ARROW)
      {
        syntax_error ("'=>' expected", token.pos);
        break;
      }

      get_token();


      convert_type_into_context (field_type, out field_context);  // works even if field_type is null

      pos = token.pos;
      parse_expression (field_context, pos, null, out e);


      if (ok)
      {
        if (check_assignment_context_compatibility (field_context, e, pos) < 0)
          ok = false;
        is_constant &= is_constant_exp (e);
        append_expression (ref list, e, field_type, field);

        if (field != null)
          field = field^.next;
        skip_to_valid_field (ref field, out field_type, context.constraint.value);
      }
      else
      {
        free_exp (e);
      }

      if (token.kind == RIGHT_ACCOLADE)
        break;

      if (token.kind != COMMA)
      {
        syntax_error ("',' or '}' expected", token.pos);
        break;
      }

      get_token();
    }


    if (ok && field != null)
    {
      semantic_error ("missing fields in aggregate", token.pos);
      ok = false;
    }


    if (!is_constant)
    {
      if (context.base_type_or_null != null && is_jagged_type (context.base_type_or_null))
        semantic_error ("jagged aggregates must be constant", paggregate_pos);
    }


    if (is_constant && ok)
    {
      LIST_OF_EXPRESSIONS^ l;

      begin_struct (out data, context.base_type_or_null);

      l = list.head;
      while (l != null)
      {
        begin_field (ref data, l^.e);
        end_field (ref data);
        l = l^.next;
      }

      end_struct (ref data);

      if (data.invalid_type)
      {
        semantic_error ("struct type is not constant", paggregate_pos);
        ok = false;
      }
      else if (data.size_overflow)
      {
        semantic_error ("aggregate is too large", paggregate_pos);
        ok = false;
      }
    }

    if (is_constant && ok)
    {
      POOL p;
      LIST_OF_EXPRESSIONS^ l;

      e = new EXPRESSION (A_POOL_CONSTANT);
      e^.base_type_or_null = context.base_type_or_null;

      if (context.base_type_or_null^.kind == A_STRUCT_TYPE &&
          context.base_type_or_null^.the_struct_type.is_open_type)
      {
        e^.constraint.kind = CONSTANT_CONSTRAINT;
        e^.constraint.value = context.constraint.value;
      }

      e^.form = A_VALUE;
      e^.access = ACCESS_CONSTANT;

      p = new_pool_constant ((uint)data.struct_size, (uint)data.struct_alignment);

      e^.pool_constant_info.pool_cte = p;

      begin_struct (out data, context.base_type_or_null);

      l = list.head;
      while (l != null)
      {
        begin_field (ref data, l^.e);

        convert_type_into_context (l^.type, out field_context);
        store_constant_into_aggregate_pool (field_context, l^.exp, p, (uint)data.field_offset);

        end_field (ref data);
        l = l^.next;
      }

      end_struct (ref data);

      free_list (ref list);
    }
    else     // runtime aggregate
    {
      e = new EXPRESSION (AN_AGGREGATE_VALUE);
      e^.base_type_or_null = ok ? context.base_type_or_null : null;

      if (context.base_type_or_null != null &&
          context.base_type_or_null^.kind == A_STRUCT_TYPE &&
          context.base_type_or_null^.the_struct_type.is_open_type)
      {
        e^.constraint.kind = CONSTANT_CONSTRAINT;
        e^.constraint.value = context.constraint.value;
      }

      e^.form   = A_VALUE;
      e^.access = ACCESS_READONLY;
      e^.aggregate_value_info.list = list.head;
    }

    pout = e;
  }

  if (token.kind == RIGHT_ACCOLADE)
    get_token();
}

/*****************************************************************************/

// aggregate ::= positional_array_aggregate
//             | open_array_aggregate
//             | positional_struct_aggregate
//             | named_struct_aggregate

void parse_aggregate (    CONTEXT     context,        // never null
                      out PEXPRESSION pout)
{
  bool          context_ok;
  TEXT_POSITION aggregate_pos;

  if (token.kind != LEFT_ACCOLADE)
  {
    syntax_error ("'{' expected", token.pos);
    pout = dummy_expression ();
    return;
  }


  // check that the context defines either:
  // - an array
  // - an open array
  // - a non-open struct context
  // (an open struct context is not allowed)

  context_ok = true;   // default

  if (context.base_type_or_null == null)
  {
    syntax_error ("an array or struct context is expected here", token.pos);
    context_ok = false;
  }
  else if (context.base_type_or_null^.kind == AN_OPEN_ARRAY_TYPE)
  {
    // array context
  }
  else if (context.base_type_or_null^.kind == A_STRUCT_TYPE)    // struct context
  {
    if (context.base_type_or_null^.the_struct_type.is_open_type)
    {
      if (context.constraint.kind != CONSTANT_CONSTRAINT)
      {
        syntax_error ("an open struct context without constant constraint is not allowed here", token.pos);
        context_ok = false;
      }
    }
  }
  else
  {
    syntax_error ("an array or struct context is expected here", token.pos);
    context_ok = false;
  }


  aggregate_pos = token.pos;

  get_token();  // skip '{'

  if (token.kind != IDENTIFIER)
  {
    parse_non_named_aggregate (context, context_ok, aggregate_pos, out pout);
    return;
  }

  get_look_ahead_token ();
  if (look_ahead_token.kind == ARROW)
  {
    parse_named_aggregate (context, context_ok, aggregate_pos, out pout);
  }
  else
  {
    parse_non_named_aggregate (context, context_ok, aggregate_pos, out pout);
  }
}

/*****************************************************************************/

//  runtime_type ::= type_name
//                 | type_name "["  "]"
//                 | type_name "[" expression "]"
//                 | type_name "(" expression ")"

// returns one of the following, with field .value being null :
//
//  typedef struct {
//    PEXPRESSION  value;        // can be null if used inside an allocator
//  } QUALIFIED_EXPRESSION_INFO;
//
//  typedef struct {
//    PEXPRESSION  length;       // runtime array length
//    PEXPRESSION  value;        // can be null if used inside an allocator
//  } ARRAY_QUALIFIED_EXPRESSION_INFO;
//
//  typedef struct {
//    PEXPRESSION  discriminant; // runtime discriminant value
//    PEXPRESSION  value;        // can be null if used inside an allocator
//  } STRUCT_QUALIFIED_EXPRESSION_INFO;
//

void parse_runtime_type (    PENTITY       type_name,  // is NEVER null !
                         out TEXT_POSITION pruntime_exp_pos,
                         out PEXPRESSION   pout)
{
  PEXPRESSION nout;

  clear pruntime_exp_pos;

  if (token.kind == LEFT_BRACKET)   // array
  {
    ENTITY (AN_OPEN_ARRAY_TYPE) open_array;
    PENTITY                     t;


    // create open array
    clear open_array;
    open_array.the_open_array_type.element = type_name;

    t = append_new_entity (open_array, L"", token.pos);


    get_token();   // skip '['

    if (token.kind == RIGHT_BRACKET)   // ']'
    {
      get_token();   // skip ']'

      nout = new EXPRESSION (A_QUALIFIED_EXPRESSION);
      nout^.base_type_or_null = t;
      nout^.constraint.kind = UNCONSTRAINED;
      nout^.constraint.value = 0;
      nout^.form = A_VALUE;
      nout^.access = ACCESS_READONLY;

      pout = nout;
    }
    else    // an array
    {
      PEXPRESSION   e;
      TEXT_POSITION pos;
      int8          value;

      pos = token.pos;
      parse_expression (g_no_context, pos, null, out e);

      if (e^.base_type_or_null != null)
      {
        if (e^.base_type_or_null^.kind != AN_INTEGER_TYPE ||
            e^.base_type_or_null == type_long)
        {
          semantic_error ("an int or uint expression is expected here", pos);
        }
      }


      // 2 cases : constant or runtime length

      if (e^.kind == A_CONST_INTEGER_VALUE)    // constant length
      {
        value = e^.const_integer_value_info.value;

        if (value < 0 || value > 2147483647)
          semantic_error ("expression is out of range", pos);

        nout = new EXPRESSION (A_QUALIFIED_EXPRESSION);
        nout^.base_type_or_null = t;
        nout^.constraint.kind = CONSTANT_CONSTRAINT;
        nout^.constraint.value = (uint4)value;
        nout^.form = A_VALUE;
        nout^.access = ACCESS_READONLY;

        free_exp (e);

        pout = nout;
      }
      else   // runtime length
      {
        nout = new EXPRESSION (AN_ARRAY_QUALIFIED_EXPRESSION);
        nout^.base_type_or_null = t;
        nout^.constraint.kind = RUNTIME_CONSTRAINT;
        nout^.constraint.value = 0;
        nout^.form = A_VALUE;
        nout^.access = ACCESS_READONLY;
        nout^.array_qualified_expression_info.length = e;

        pruntime_exp_pos = pos;

        pout = nout;
      }

      if (token.kind == RIGHT_BRACKET)   // ']'
        get_token();   // skip ']'
      else
        syntax_error ("']' expected", token.pos);
    }
  }
  else if (token.kind == LEFT_PARENTHESIS)   // open array or open struct
  {
    PEXPRESSION   e;
    TEXT_POSITION pos0, pos;
    int8          value;
    PENTITY       t;

    pos0 = token.pos;

    get_token();

    pos = token.pos;
    parse_expression (g_no_context, pos, null, out e);

    t = complete_type_of (type_name);

    if (t^.kind == AN_OPEN_ARRAY_TYPE)
    {
      if (e^.base_type_or_null != null)
      {
        if (e^.base_type_or_null^.kind != AN_INTEGER_TYPE ||
            e^.base_type_or_null == type_long)
        {
          semantic_error ("an int or uint expression is expected here", pos);
        }
      }


      // 2 cases : constant or runtime length

      if (e^.kind == A_CONST_INTEGER_VALUE)    // constant length
      {
        value = e^.const_integer_value_info.value;

        if (value < 0 || value > 2147483647)
          semantic_error ("expression is out of range", pos);

        nout = new EXPRESSION (A_QUALIFIED_EXPRESSION);
        nout^.base_type_or_null = t;
        nout^.constraint.kind = CONSTANT_CONSTRAINT;
        nout^.constraint.value = (uint4)value;
        nout^.form = A_VALUE;
        nout^.access = ACCESS_READONLY;

        free_exp (e);

        pout = nout;
      }
      else   // runtime length
      {
        nout = new EXPRESSION (AN_ARRAY_QUALIFIED_EXPRESSION);
        nout^.base_type_or_null = t;
        nout^.constraint.kind = RUNTIME_CONSTRAINT;
        nout^.constraint.value = 0;
        nout^.form = A_VALUE;
        nout^.access = ACCESS_READONLY;
        nout^.array_qualified_expression_info.length = e;

        pruntime_exp_pos = pos;

        pout = nout;
      }
    }
    else if (t^.kind == A_STRUCT_TYPE && t^.the_struct_type.is_open_type)
    {
      PENTITY discrim, discrim_type;

      // get the discriminant
      discrim = t^.the_struct_type.discriminant^.entities.first;

      // get the discriminant's type (it is garanteed to be an enumeration type)
      discrim_type = discrim^.the_field.type;

      if (e^.base_type_or_null != null)
      {
        if (e^.base_type_or_null != discrim_type)
        {
          semantic_error ("this expression must be compatible with the discriminant", pos);
        }
      }


      // 2 cases : constant or runtime discriminant

      if (e^.kind == A_CONST_ENUMERATION_VALUE)    // constant discriminant
      {
        value = e^.const_enumeration_value_info.value;

        if (value < 0 || value > discrim_type^.the_enumeration_type.last)
          semantic_error ("expression is out of range", pos);

        nout = new EXPRESSION (A_QUALIFIED_EXPRESSION);
        nout^.base_type_or_null = t;
        nout^.constraint.kind = CONSTANT_CONSTRAINT;
        nout^.constraint.value = (uint4)value;
        nout^.form = A_VALUE;
        nout^.access = ACCESS_READONLY;

        free_exp (e);

        pout = nout;
      }
      else   // runtime length
      {
        nout = new EXPRESSION (A_STRUCT_QUALIFIED_EXPRESSION);
        nout^.base_type_or_null = t;
        nout^.constraint.kind = RUNTIME_CONSTRAINT;
        nout^.constraint.value = 0;
        nout^.form = A_VALUE;
        nout^.access = ACCESS_READONLY;
        nout^.struct_qualified_expression_info.discriminant = e;

        pruntime_exp_pos = pos;

        pout = nout;
      }
    }
    else
    {
      CONTEXT context;

      semantic_error ("this constraint is only allowed for an open array or struct type", pos0);

      convert_type_into_context (type_name, out context);

      nout = new EXPRESSION (A_QUALIFIED_EXPRESSION);
      nout^.base_type_or_null = context.base_type_or_null;
      nout^.constraint = context.constraint;
      nout^.form = A_VALUE;
      nout^.access = ACCESS_READONLY;

      pout = nout;

      free_exp (e);
    }

    if (token.kind == RIGHT_PARENTHESIS)   // ')'
      get_token();   // skip ')'
    else
      syntax_error ("')' expected", token.pos);
  }
  else    // return type itself
  {
    CONTEXT context;

    convert_type_into_context (type_name, out context);

    nout = new EXPRESSION (A_QUALIFIED_EXPRESSION);
    nout^.base_type_or_null = context.base_type_or_null;
    nout^.constraint = context.constraint;
    nout^.form = A_VALUE;
    nout^.access = ACCESS_READONLY;

    pout = nout;
  }
}

/*****************************************************************************/

// merge constraints of (context and runtime_type) into a new context.

// possible values are : DOES_NOT_APPLY, UNCONSTRAINED,
//                       CONSTANT_CONSTRAINT, RUNTIME_CONSTRAINT.

// note: the context only helps if the runtime type is unconstrained.

void merge_constraints (    CONTEXT     context,     // in
                            PEXPRESSION qual,        // in (runtime type)
                        out CONTEXT     context2)    // out
{
  clear context2;
  context2.base_type_or_null = qual^.base_type_or_null;

  if (qual^.constraint.kind == DOES_NOT_APPLY)   // not array/struct type
  {
    context2.constraint = qual^.constraint;
  }
  else if (qual^.constraint.kind == UNCONSTRAINED)   // s = string ' ( exp ) + ".";
  {
    if (context.constraint.kind == CONSTANT_CONSTRAINT ||
        context.constraint.kind == RUNTIME_CONSTRAINT)
    {
      // only if base types are compatible, otherwise byte[] conversion of parameters
      // might use wrong context constraint.

      if (context.base_type_or_null != null && qual^.base_type_or_null != null &&
          types_are_equal (context.base_type_or_null, qual^.base_type_or_null))
      {
        context2.constraint = context.constraint;
      }
      else
      {
        context2.constraint = qual^.constraint;
      }
    }
    else   // context is DOES_NOT_APPLY or UNCONSTRAINED.
    {
      context2.constraint = qual^.constraint;
    }
  }
  else   // CONSTANT_CONSTRAINT or RUNTIME_CONSTRAINT
  {
    context2.constraint = qual^.constraint;
  }
}

/*****************************************************************************/

//  qualified_expression ::= runtime_type  "'"  "(" expression ")"
//                         | runtime_type  "'"  aggregate

void parse_qualified_expression (    CONTEXT     context,        // never null
                                     PENTITY     type_name,      // is NEVER null !
                                 out PEXPRESSION pout)
{
  PEXPRESSION   qual, e;
  CONTEXT       inner_context, qual_context;
  TEXT_POSITION runtime_exp_pos, pos;

  parse_runtime_type (type_name, out runtime_exp_pos, out qual);
  if (qual^.kind != A_QUALIFIED_EXPRESSION)
    semantic_error ("expression must be constant or be removed", runtime_exp_pos);

  lex_force_into_apostrophe ();
  if (token.kind != APOSTROPHE)
  {
    syntax_error ("apostrophe ' expected", token.pos);
    pout = qual;
    return;
  }

  get_token();   // skip apostrophe


  if (is_limited_type (type_name))
    semantic_error ("an expression of a limited type is not allowed here", token.pos);


  merge_constraints (context, qual, out inner_context);


  if (token.kind == LEFT_PARENTHESIS)
  {
    pos = token.pos;

    get_token();   // skip '('

    parse_expression (inner_context, token.pos, null, out e);

    if (token.kind == RIGHT_PARENTHESIS)   // ')'
      get_token();   // skip ')'
    else
      syntax_error ("')' expected", token.pos);
  }
  else   // aggregate
  {
    pos = token.pos;
    parse_aggregate (inner_context, out e);
  }


  if (qual^.kind == A_QUALIFIED_EXPRESSION)
    qual^.qualified_expression_info.value = e;
  else if (qual^.kind == AN_ARRAY_QUALIFIED_EXPRESSION)
    qual^.array_qualified_expression_info.value = e;
  else if (qual^.kind == A_STRUCT_QUALIFIED_EXPRESSION)
    qual^.struct_qualified_expression_info.value = e;


  clear qual_context;
  qual_context.base_type_or_null = qual^.base_type_or_null;
  qual_context.constraint = qual^.constraint;


  // check if expression is assignment-compatible with runtime type.

  if (check_assignment_context_compatibility (qual_context, e, pos) == 0)  // ok
  {
    // in case the expression is a constant and there is no runtime constraint,
    // remove the node to keep just the constant;
    // this applies for any simple or array/struct type.

    if (is_constant_exp (e) && qual^.constraint.kind != RUNTIME_CONSTRAINT)
    {
      e^.base_type_or_null = qual^.base_type_or_null;  // keep exact int/float/enum type
      free (qual);
      qual = e;
    }
    else
    {
      // if runtime type is unconstrained, use constraint from expression which is more precise
      if (qual^.constraint.kind == UNCONSTRAINED)
        qual^.constraint = e^.constraint;

      // if runtime type is runtime, use constraint from expression which is maybe constant
      if (qual^.constraint.kind == RUNTIME_CONSTRAINT)
        qual^.constraint = e^.constraint;
    }
  }

  pout = qual;
}

/*****************************************************************************/

//  new_allocator ::= "new"  qualified_expression
//                  | "new"  runtime_type

void parse_new_allocator (    CONTEXT     context,
                          out PEXPRESSION pout)
{
  PENTITY       type_name, designated_type;
  PEXPRESSION   qual, alloc, e;
  CONTEXT       designated_type_context;
  CONTEXT       inner_context, qual_context;
  TEXT_POSITION pos0, pos, posq, typ_pos, runtime_exp_pos;

  if (context.base_type_or_null == null ||
      complete_type_of (context.base_type_or_null)^.kind != A_POINTER_TYPE)
  {
    syntax_error ("a pointer type context is required here", token.pos);
    pout = dummy_expression ();
    return;
  }

  designated_type =
      complete_type_of (
             complete_type_of (context.base_type_or_null)
              ^.the_pointer_type.designated_type
                       );


  pos0 = token.pos;
  get_token();   // skip 'new'

  typ_pos = token.pos;
  type_name = parse_type_name ();
  if (type_name == null)
  {
    pout = dummy_expression ();
    return;
  }

  if (is_jagged_type (type_name))
    semantic_error ("a jagged type is not allowed here", typ_pos);


  posq = token.pos;
  parse_runtime_type (type_name, out runtime_exp_pos, out qual);


  // note: the designated_type can be an open type, but cannot be jagged.

  convert_type_into_context (designated_type, out designated_type_context);


  merge_constraints (designated_type_context, qual, out inner_context);


  lex_force_into_apostrophe ();
  if (token.kind == APOSTROPHE)  // a qualified expression
  {
    get_token();   // skip apostrophe

    if (is_limited_type (type_name))
      semantic_error ("an expression of a limited type is not allowed here", token.pos);

    if (token.kind == LEFT_PARENTHESIS)
    {
      if (qual^.kind != A_QUALIFIED_EXPRESSION)
        semantic_error ("expression must be constant or be removed", runtime_exp_pos);

      pos = token.pos;

      get_token();   // skip '('

      parse_expression (inner_context, token.pos, null, out e);

      if (token.kind == RIGHT_PARENTHESIS)   // ')'
        get_token();   // skip ')'
      else
        syntax_error ("')' expected", token.pos);
    }
    else   // aggregate
    {
      pos = token.pos;
      parse_aggregate (inner_context, out e);

      if (qual^.kind != A_QUALIFIED_EXPRESSION)   // runtime constraint
      {
        if (e^.kind != AN_UNC_ARRAY_AGGREGATE || (!is_open_type (designated_type)))
          semantic_error ("expression must be constant or be removed", runtime_exp_pos);
      }
    }


    if (qual^.kind == A_QUALIFIED_EXPRESSION)
      qual^.qualified_expression_info.value = e;
    else if (qual^.kind == AN_ARRAY_QUALIFIED_EXPRESSION)
      qual^.array_qualified_expression_info.value = e;
    else if (qual^.kind == A_STRUCT_QUALIFIED_EXPRESSION)
      qual^.struct_qualified_expression_info.value = e;


    clear qual_context;
    qual_context.base_type_or_null = qual^.base_type_or_null;
    qual_context.constraint = qual^.constraint;

    if (check_assignment_context_compatibility (qual_context, e, pos) == 0)  // ok
    {
      // if runtime type is unconstrained, use constraint from expression which is more precise
      if (qual^.constraint.kind == UNCONSTRAINED)
        qual^.constraint = e^.constraint;

      // if runtime type is runtime, use constraint from expression which is maybe constant
      if (qual^.constraint.kind == RUNTIME_CONSTRAINT)
        qual^.constraint = e^.constraint;


      // check that designated_type base-type matches with qual base-type.

      if (!types_are_equal (base_type_of (designated_type), qual^.base_type_or_null))
        semantic_error ("pointer types do not match", pos0);
      else
      {
        // if designated_type is constrained, constraints must match.
        if (designated_type_context.constraint.kind == CONSTANT_CONSTRAINT &&
            qual^.constraint.kind == CONSTANT_CONSTRAINT &&
            designated_type_context.constraint.value != qual^.constraint.value)
        {
          semantic_error ("designated arrays/structs have different length/discriminant", pos0);
        }
      }
    }


    // add an extra allocator node

    alloc = new EXPRESSION (AN_ALLOCATOR);
    alloc^.base_type_or_null = complete_type_of (context.base_type_or_null);
    alloc^.form = A_VALUE;
    alloc^.access = ACCESS_READONLY;
    alloc^.allocator_info.value = qual;
    alloc^.allocator_info.has_header = is_open_type (designated_type);

    pout = alloc;
  }
  else   // just a runtime_type (allocates a zero-filled heap object)
  {
    // check that a runtime constraint is only allowed for an open designated type

    if (qual^.kind != A_QUALIFIED_EXPRESSION &&   // runtime constraint
        (!is_open_type (designated_type)))        // designated type is not open
      semantic_error ("expression must be constant", runtime_exp_pos);


    // check that the runtime type is not an open type (because it has no size)

    if (qual^.constraint.kind == UNCONSTRAINED)
      semantic_error ("a length/discriminant constraint is required here", posq);


    // check that designated_type base-type matches with qual base-type.

    if (!types_are_equal (base_type_of (designated_type), qual^.base_type_or_null))
      semantic_error ("pointer types do not match", pos0);
    else
    {
      // if designated_type is constrained, constraints must match.
      if (designated_type_context.constraint.kind == CONSTANT_CONSTRAINT &&
          qual^.constraint.kind == CONSTANT_CONSTRAINT &&
          designated_type_context.constraint.value != qual^.constraint.value)
      {
        semantic_error ("designated arrays/structs have different length/discriminant", pos0);
      }
    }


    // add an extra allocator node

    alloc = new EXPRESSION (AN_ALLOCATOR);
    alloc^.base_type_or_null = complete_type_of (context.base_type_or_null);
    alloc^.form = A_VALUE;
    alloc^.access = ACCESS_READONLY;
    alloc^.allocator_info.value = qual;
    alloc^.allocator_info.has_header = is_open_type (designated_type);

    pout = alloc;
  }
}

/*****************************************************************************/

void parse_primary_starting_with_a_name (    CONTEXT       context,
                                             TEXT_POSITION prefix_pos,
                                             PENTITY       prefix_name,    // is NEVER null !
                                         out PEXPRESSION   pout)
{
  if (prefix_name^.kind > LAST_ENTITY_DENOTING_A_TYPE)    // it's not a type
  {
    parse_name (context, false, prefix_pos, prefix_name, out pout);
    return;
  }


  // either a name or a qualified expression

  if (token.kind == LEFT_BRACKET || token.kind == LEFT_PARENTHESIS)
  {
    parse_qualified_expression (context, prefix_name, out pout);
    return;
  }

  lex_force_into_apostrophe ();
  if (token.kind == APOSTROPHE)
  {
    get_look_ahead_token ();
    if (look_ahead_token.kind == LEFT_PARENTHESIS || look_ahead_token.kind == LEFT_ACCOLADE)
    {
      parse_qualified_expression (context, prefix_name, out pout);
      return;
    }
  }

  parse_name (context, false, prefix_pos, prefix_name, out pout);
}

/*****************************************************************************/

//  primary ::= integer_literal           (integer)
//            | floating_point_literal    (floating-point)
//            | character_literal         (char, wchar)
//            | string_literal            (string, wstring)
//            | "nul"                     (char)
//            | "Lnul"                    (wchar)
//            | "null"                    (pointer, function/unsafe pointer)
//            | name                      (value or object)
//            | run_call                  (int)
//            | aggregate                 (array, struct)
//            | qualified_expression      (value)
//            | new_allocator             (pointer)

void parse_primary (    CONTEXT       context,
                        TEXT_POSITION prefix_pos,
                        PENTITY       prefix_name,    // can be null
                    out PEXPRESSION   pout)
{
  PEXPRESSION nout;
  bool        wide;
  uint        len, charsize, i;
  POOL        p;

  if (prefix_name != null)   // it's a name
  {
    parse_primary_starting_with_a_name (context, prefix_pos, prefix_name, out pout);
    return;
  }

  switch (token.kind)
  {
    case INTEGER_LITERAL:
      nout = new EXPRESSION (A_CONST_INTEGER_VALUE);
      nout^.base_type_or_null = token.info._integer.has_suffix_L ? type_long : type_int_literal;
      nout^.form = A_VALUE;
      nout^.access = ACCESS_CONSTANT;
      nout^.const_integer_value_info.value = token.info._integer.value;

      get_token();
      break;

    case FLOAT_LITERAL:
      nout = new EXPRESSION (A_CONST_FLOAT_VALUE);

      if (token.info._float.suffix == 'F')
        nout^.base_type_or_null = type_float;
      else if (token.info._float.suffix == 'D')
        nout^.base_type_or_null = type_double;
      else
        nout^.base_type_or_null = type_float_literal;     // compatible with any float type

      nout^.form = A_VALUE;
      nout^.access = ACCESS_CONSTANT;
      nout^.const_float_value_info.value = token.info._float.value;

      get_token();
      break;

    case CHAR_LITERAL:
      nout = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      nout^.base_type_or_null = token.info._char.has_prefix_L ? type_wchar : type_char;
      nout^.form = A_VALUE;
      nout^.access = ACCESS_CONSTANT;
      nout^.const_enumeration_value_info.value = (uint)token.info._char.value;

      get_token();
      break;

    case STRING_LITERAL:
      wide = token.info._string.has_prefix_L;
      len = (uint4)token.info._string.length;
      charsize = wide ? 2 : 1;

      nout = new EXPRESSION (A_POOL_CONSTANT);
      nout^.base_type_or_null = wide ? type_wstring : type_string;
      nout^.constraint.kind = CONSTANT_CONSTRAINT;
      nout^.constraint.value = len;
      nout^.form = A_VALUE;
      nout^.access = ACCESS_CONSTANT;
      nout^.pool_constant_info.pool_cte = new_pool_constant (charsize * len, charsize);

      p = nout^.pool_constant_info.pool_cte;
      for (i=0; i<len; i++)
        store_integer (p, i*charsize, (int)token.info._string.value[i], charsize);

      get_token();
      break;

    case TOKEN_nul:      // (char)
      nout = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      nout^.base_type_or_null = type_char;
      nout^.form = A_VALUE;
      nout^.access = ACCESS_CONSTANT;
      nout^.const_enumeration_value_info.value = 0;

      get_token();
      break;

    case TOKEN_Lnul:     // (wchar)
      nout = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      nout^.base_type_or_null = type_wchar;
      nout^.form = A_VALUE;
      nout^.access = ACCESS_CONSTANT;
      nout^.const_enumeration_value_info.value = 0;

      get_token();
      break;

    case TOKEN_null:     // (pointer, function/unsafe pointer)
      nout = new EXPRESSION (A_CONST_NULL_VALUE);
      nout^.base_type_or_null = type_null_literal;     // compatible with any pointer, function pointer or unsafe pointer.
      nout^.form = A_VALUE;
      nout^.access = ACCESS_CONSTANT;

      get_token();
      break;

    case TOKEN_run:      // run_call
      parse_run_call (out nout);
      break;

    case TOKEN_new:      // new_allocator (pointer)
      parse_new_allocator (context, out nout);
      break;

    case LEFT_ACCOLADE:   // aggregate (array, struct)
      parse_aggregate (context, out nout);
      break;

    case LEFT_PARENTHESIS:  // parentheized expression name
      parse_name (context, false, token.pos, null, out nout);
      break;

    default:    // maybe it's a name or a qualified expression
      {
        PENTITY       e2;
        TEXT_POSITION name_pos;

        name_pos = token.pos;
        e2 = type_entity_of_token (token.kind);
        if (e2 != null)
        {
          get_token();    // skip type token
        }
        else if (token.kind == IDENTIFIER)
        {
          e2 = parse_expanded_name ();
          if (e2 == null)  // could not find entity (error already given)
          {
            parse_name (context, false, name_pos, e2, out pout);
            return;
          }
        }
        else
        {
          parse_name (context, false, name_pos, null, out pout);
          return;
        }

        // e2 denotes a type

        parse_primary_starting_with_a_name (context, name_pos, e2, out pout);
        return;
      }
  }

  pout = nout;
}

/*****************************************************************************/

package CONVERSION_TABLE

  struct CONVERSION
  {
    ENTITY_KIND   source;
    ENTITY_KIND   target;
    KIND_OPERATOR op;
  }

  const CONVERSION conversion_table[] =
  {
   {AN_INTEGER_TYPE,             AN_INTEGER_TYPE,             OP_CONVERT_INT_INT},
   {AN_ENUMERATION_TYPE,         AN_INTEGER_TYPE,             OP_CONVERT_INT_INT},
   {A_FLOAT_TYPE,                AN_INTEGER_TYPE,             OP_CONVERT_FLOAT_INT},
   {AN_INTEGER_TYPE,             AN_ENUMERATION_TYPE,         OP_CONVERT_INT_INT},
   {A_FLOAT_TYPE,                A_FLOAT_TYPE,                OP_CONVERT_FLOAT_FLOAT},
   {AN_INTEGER_TYPE,             A_FLOAT_TYPE,                OP_CONVERT_INT_FLOAT},
   {AN_UNSAFE_POINTER_TYPE,      AN_UNSAFE_POINTER_TYPE,      OP_CONVERT_PTR_PTR},
   {A_NULL_POINTER_TYPE,         AN_UNSAFE_POINTER_TYPE,      OP_NONE},  // null to unsafe ptr
   {LAST_ENTITY_DENOTING_A_TYPE, LAST_ENTITY_DENOTING_A_TYPE, OP_NONE},
  };

end CONVERSION_TABLE;

/*****************************************************************************/

// unary_expression  ::= { unary_operator | conversion }  primary
// unary_operator  ::=  "+" | "-" | "!" | "~" | "&" | "*" | "--" | "++"
// conversion      ::= "("  restricted_type_definition  ")"
// restricted_type_definition ::= type_name {"*"}

public
void parse_unary_expression (    CONTEXT       context,
                                 TEXT_POSITION prefix_position,
                                 PENTITY       prefix_name,    // can be null
                             out PEXPRESSION   pout)
{
  KIND_OPERATOR op;
  PEXPRESSION   e, e2;
  PENTITY       t, t2;
  TEXT_POSITION pos, posc, pos_name;
  int8          r, r2;
  double        rf;
  int           rc;
  int           i;

  if (prefix_name != null)
  {
    parse_primary_starting_with_a_name (context, prefix_position, prefix_name, out pout);
    return;
  }

  if (token.kind == LEFT_PARENTHESIS)    // either conversion or name
  {
    pos = token.pos;
    get_token();    // skip (


    // check if now comes a type_name followed either by '*' or ')'

    pos_name = token.pos;
    t2 = type_entity_of_token (token.kind);
    if (t2 != null)
    {
      get_token();    // skip type token
    }
    else if (token.kind == IDENTIFIER)
    {
      t2 = parse_expanded_name ();

      if (t2 == null ||  // could not find entity (error already given)
          t2^.kind > LAST_ENTITY_DENOTING_A_TYPE)
      {
        parse_name (context, true, pos_name, t2, out pout);
        return;
      }
    }
    else
    {
      parse_name (context, true, pos_name, null, out pout);
      return;
    }

    // we have parsed a type_name

    if (token.kind == STAR || token.kind == RIGHT_PARENTHESIS)
    {
      // it's a type conversion

      t2 = complete_type_of (t2);

      while (token.kind == STAR)   // produce unsafe pointer type
      {
        ENTITY (AN_UNSAFE_POINTER_TYPE) ptr;

        if (is_jagged_type (t2))
          semantic_error ("a jagged type is not allowed here", pos_name);

        clear (ptr);
        ptr.the_unsafe_pointer_type.designated_type = t2;

        t2 = append_new_entity (ptr, L"", token.pos);

        get_token();    // skip *
      }

      if (token.kind == RIGHT_PARENTHESIS)
        get_token();    // skip )
      else
        syntax_error ("')' expected", token.pos);

      parse_unary_expression (    g_no_context,  // no context for a conversion
                                  token.pos,
                                  null,  // no prefix
                              out e);

      t = e^.base_type_or_null;


      if (is_unsafe_type (t2))
        check_unsafe_region ();


      // check that the conversion from 't' to 't2' is allowed

      op = OP_NONE;    // default

      if (t != null)
      {
        for (i=0; conversion_table[i].source != LAST_ENTITY_DENOTING_A_TYPE; i++)
        {
          if (conversion_table[i].source == t^.kind &&
              conversion_table[i].target == t2^.kind)
            break;
        }

        if (conversion_table[i].source == LAST_ENTITY_DENOTING_A_TYPE)
        {
          semantic_error ("conversion is not allowed", pos_name);
          t = null;     // indicates forbidden conversion
        }

        op = conversion_table[i].op;
      }


      if (t != null &&
          e^.kind == A_CONST_NULL_VALUE &&    // null to unsafe-ptr
          t^.kind == A_NULL_POINTER_TYPE)
      {
        e2 = new EXPRESSION (A_CONST_NULL_VALUE);
        e2^.base_type_or_null = t2;
        e2^.form = A_VALUE;
        e2^.access = ACCESS_CONSTANT;

        free_exp (e);
      }
      else if (t != null &&
                   (e^.kind == A_CONST_ENUMERATION_VALUE ||
                    e^.kind == A_CONST_INTEGER_VALUE ||
                    e^.kind == A_CONST_FLOAT_VALUE) &&       // conversion of a constant
                   (t^.kind == AN_ENUMERATION_TYPE ||
                    t^.kind == AN_INTEGER_TYPE ||
                    t^.kind == A_FLOAT_TYPE))
      {
        r = 0;
        rf = 0.0;


        // retrieve value

        if (e^.kind == A_CONST_ENUMERATION_VALUE)
          r = e^.const_enumeration_value_info.value;

        if (e^.kind == A_CONST_INTEGER_VALUE)
          r = e^.const_integer_value_info.value;

        if (e^.kind == A_CONST_FLOAT_VALUE)
          rf = e^.const_float_value_info.value;


        // convert between float and int

        if (op == OP_CONVERT_FLOAT_INT)   // float to int
        {
          if (rf < (double)INTEGER_DATA[(uint)a_int8].min || rf > (double)INTEGER_DATA[(uint)a_int8].max)
            semantic_error ("overflow", pos);
          else
            r = (int8)rf;
        }


        if (op == OP_CONVERT_INT_FLOAT)   // int to float
        {
          rf = (double)r;   // cannot overflow
        }


        // check that value fits into target type

        if (t2^.kind == AN_ENUMERATION_TYPE)
        {
          if (r < 0 || r > t2^.the_enumeration_type.last)
          {
            semantic_error ("overflow", pos);
            r = 0;
          }

          e2 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
          e2^.base_type_or_null = t2;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_CONSTANT;
          e2^.const_enumeration_value_info.value = (uint4)r;
        }
        else if (t2^.kind == AN_INTEGER_TYPE)
        {
          // check if r outside range of t2

          ref INTEGER_INFO pi = INTEGER_DATA[(uint)t2^.the_integer_type.type];

          if (r < pi.min || r > pi.max)
          {
            semantic_error ("overflow", pos);
            r = 0;
          }

          e2 = new EXPRESSION (A_CONST_INTEGER_VALUE);
          e2^.base_type_or_null = t2;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_CONSTANT;
          e2^.const_integer_value_info.value = r;
        }
        else if (t2^.kind == A_FLOAT_TYPE)
        {
          check_float_overflow_underflow (ref rf, t2, pos);

          e2 = new EXPRESSION (A_CONST_FLOAT_VALUE);
          e2^.base_type_or_null = t2;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_CONSTANT;
          e2^.const_float_value_info.value = rf;
        }
        else
        {
          e2 = null;
          fatal_compiler_error ("conversion", token.pos);
        }

        free_exp (e);
      }
      else   // runtime
      {
        e2 = new EXPRESSION (AN_OPERATOR_VALUE);
        e2^.base_type_or_null = t2;
        e2^.form = A_VALUE;
        e2^.access = ACCESS_READONLY;
        e2^.operator_value_info.op = op;
        e2^.operator_value_info.arg[0] = e;
      }

      pout = e2;
    }
    else
    {
      // it's a parentheized name
      parse_name (context, true, pos_name, t2, out pout);
    }

    return;
  }


  if (token.kind == STAR)  // --> AN_UNSAFE_DEREFERENCED_OBJECT
  {
    check_unsafe_region ();

    get_token();
    pos = token.pos;
    parse_unary_expression (context, pos, prefix_name, out e);

    t = e^.base_type_or_null;
    if (t != null && t^.kind != AN_UNSAFE_POINTER_TYPE)
      semantic_error ("unsafe pointer value expected", pos);

    if (e^.kind == A_CONST_NULL_VALUE)
      semantic_error ("dereferencing null pointer is not allowed", token.pos);

    e2 = new EXPRESSION (AN_UNSAFE_DEREFERENCED_OBJECT);
    e2^.form = AN_OBJECT;
    e2^.access = ACCESS_READWRITE;
    e2^.unsafe_dereferenced_object_info.unsafe_ptr_value = e;

    if (t != null && t^.kind == AN_UNSAFE_POINTER_TYPE)
    {
      PENTITY dt;

      dt = t^.the_unsafe_pointer_type.designated_type;
      if (is_open_type (dt))
        semantic_error ("operand of '*' must not be an unsafe pointer to an open type", token.pos);

      convert_type_into_base_type_and_constraint
              (    dt,
                   always_constrained => true,
               out e2^.base_type_or_null,
               out e2^.constraint);
    }

    pout = e2;

    return;
  }


  switch (token.kind)
  {
    case PLUS:      // +
      op = OP_UNARY_PLUS;
      break;

    case MINUS:     // -
      op = OP_UNARY_MINUS;
      break;

    case NOT:       // !
      op = OP_BOOL_NOT;
      break;

    case TILDE:     // ~
      op = OP_INT_NOT;
      break;

    case AMPERSAND: // &
      op = OP_ADDRESS_OF;
      break;

    case MINUS_MINUS:  // --
      op = OP_PRE_DEC;
      break;

    case PLUS_PLUS:    // ++
      op = OP_PRE_INC;
      break;

    default:
      op = OP_NONE;
      break;
  }

  if (op == OP_NONE)
  {
    parse_primary (context, prefix_position, prefix_name, out pout);
    return;
  }

  posc = token.pos;
  get_token();
  pos = token.pos;

  {
    CONTEXT context2;
    context2 = context;
    if (op == OP_ADDRESS_OF)
      context2.address_of = true;
    parse_unary_expression (context2, pos, prefix_name, out e);
  }

  t = e^.base_type_or_null;
  if (t == null)
  {
    pout = e;
    return;
  }


  switch (op)
  {
    case OP_UNARY_PLUS:
    case OP_UNARY_MINUS:

      if (t^.kind == AN_INTEGER_TYPE)
      {
        t2 = promote_one_int (e);

        if (e^.kind == A_CONST_INTEGER_VALUE)
        {
          r = e^.const_integer_value_info.value;

          if (op == OP_UNARY_MINUS)
          {
            rc = neg_int (r, out r2);
            if (rc < 0)
            {
              semantic_error ("overflow", posc);
              r = 0;
            }
            else
            {
              r = r2;
            }
          }


          // check if r outside range of t2

          {
            ref INTEGER_INFO pi = INTEGER_DATA[(uint)t2^.the_integer_type.type];
            if (r < pi.min || r > pi.max)
            {
              semantic_error ("overflow", posc);
              r = 0;
            }
          }

          e2 = new EXPRESSION (A_CONST_INTEGER_VALUE);
          e2^.base_type_or_null = t2;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_CONSTANT;
          e2^.const_integer_value_info.value = r;

          free_exp (e);

          pout = e2;
        }
        else  // runtime
        {
          e2 = new EXPRESSION (AN_OPERATOR_VALUE);
          e2^.base_type_or_null = t2;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_READONLY;
          e2^.operator_value_info.op = op;
          e2^.operator_value_info.arg[0] = e;

          pout = e2;
        }
      }
      else if (t^.kind == A_FLOAT_TYPE)
      {
        if (e^.kind == A_CONST_FLOAT_VALUE)
        {
          rf = e^.const_float_value_info.value;

          if (op == OP_UNARY_MINUS)
            rf = -rf;

          e2 = new EXPRESSION (A_CONST_FLOAT_VALUE);
          e2^.base_type_or_null = t;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_CONSTANT;
          e2^.const_float_value_info.value = rf;

          free_exp (e);

          pout = e2;
        }
        else  // runtime
        {
          e2 = new EXPRESSION (AN_OPERATOR_VALUE);
          e2^.base_type_or_null = t;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_READONLY;
          e2^.operator_value_info.op = op;
          e2^.operator_value_info.arg[0] = e;

          pout = e2;
        }
      }
      else
      {
        semantic_error ("illegal operand", pos);
        pout = e;
      }
      break;


    case OP_BOOL_NOT:
      if (t == type_bool)
      {
        if (e^.kind == A_CONST_ENUMERATION_VALUE)
        {
          r = e^.const_enumeration_value_info.value;

          if (r == 0)
            r = 1;
          else
            r = 0;

          e2 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
          e2^.base_type_or_null = type_bool;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_CONSTANT;
          e2^.const_enumeration_value_info.value = (uint4)r;

          free_exp (e);

          pout = e2;
        }
        else  // runtime
        {
          e2 = new EXPRESSION (AN_OPERATOR_VALUE);
          e2^.base_type_or_null = type_bool;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_READONLY;
          e2^.operator_value_info.op = op;
          e2^.operator_value_info.arg[0] = e;

          pout = e2;
        }
      }
      else
      {
        semantic_error ("bool operand expected", pos);
        pout = e;
      }
      break;


    case OP_INT_NOT:
      if (t^.kind == AN_INTEGER_TYPE)
      {
        t2 = promote_one_int (e);

        if (e^.kind == A_CONST_INTEGER_VALUE)
        {
          r = e^.const_integer_value_info.value;

          r = ~r;

          // mask with range of t2

          {
            ref INTEGER_INFO pi = INTEGER_DATA[(uint)t2^.the_integer_type.type];
            if (!pi.is_signed)
              r &= pi.mask;        // reduce r to the length of t2
          }

          e2 = new EXPRESSION (A_CONST_INTEGER_VALUE);
          e2^.base_type_or_null = t2;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_CONSTANT;
          e2^.const_integer_value_info.value = r;

          free_exp (e);

          pout = e2;
        }
        else  // runtime
        {
          e2 = new EXPRESSION (AN_OPERATOR_VALUE);
          e2^.base_type_or_null = t2;
          e2^.form = A_VALUE;
          e2^.access = ACCESS_READONLY;
          e2^.operator_value_info.op = op;
          e2^.operator_value_info.arg[0] = e;

          pout = e2;
        }
      }
      else
      {
        semantic_error ("operand must have integer type", pos);
        pout = e;
      }
      break;


    case OP_ADDRESS_OF:
      if (e^.form != AN_OBJECT)
      {
        semantic_error ("an object is expected here", pos);
        pout = e;
        return;
      }

      check_unsafe_region ();

      if (is_jagged_type (t))
        semantic_error ("a jagged object is not allowed here", pos);

      {
        ENTITY (AN_UNSAFE_POINTER_TYPE) ptr;

        clear (ptr);

        if (base_type_of (t)^.kind == AN_OPEN_ARRAY_TYPE)
          t = base_type_of (t)^.the_open_array_type.element;

        ptr.the_unsafe_pointer_type.designated_type = t;

        t = append_new_entity (ptr, L"", posc);
      }

      e2 = new EXPRESSION (AN_OPERATOR_VALUE);
      e2^.base_type_or_null = t;
      e2^.form = A_VALUE;
      e2^.access = ACCESS_READONLY;
      e2^.operator_value_info.op = op;
      e2^.operator_value_info.arg[0] = e;

      pout = e2;

      break;


    case OP_PRE_DEC:
    case OP_PRE_INC:
      if (e^.access != ACCESS_READWRITE)
      {
        semantic_error ("a read/write object is expected here", pos);
      }
      else if (e^.form != AN_OBJECT)
      {
        semantic_error ("an object is expected here", pos);
      }
      else if (t^.kind != AN_INTEGER_TYPE &&
               t^.kind != AN_ENUMERATION_TYPE &&
               t^.kind != AN_UNSAFE_POINTER_TYPE)
      {
        semantic_error ("enum, integer or unsafe pointer expected here", pos);
      }

      e2 = new EXPRESSION (AN_OPERATOR_VALUE);
      e2^.base_type_or_null = t;
      e2^.form = A_VALUE;
      e2^.access = ACCESS_READONLY;
      e2^.operator_value_info.op = op;
      e2^.operator_value_info.arg[0] = e;

      pout = e2;

      break;


    default:
      fatal_compiler_error ("bad operand", token.pos);
      pout = e;
      break;
  }
}

/*****************************************************************************/

void parse_term (    CONTEXT       context,
                     TEXT_POSITION prefix_position,
                     PENTITY       prefix_name,    // can be null
                 out PEXPRESSION   pout)
{
  PEXPRESSION   e1, e2, e3;
  TOKEN_KIND    tk;
  KIND_OPERATOR op;
  PENTITY       t1, t2, t3;
  TEXT_POSITION pos;
  int8          v1, v2, r;
  double        vf1, vf2, rf;
  int           rc;
  bool          is_signed;

  parse_unary_expression (context, prefix_position, prefix_name, out e1);

  while (token.kind == STAR || token.kind == SLASH || token.kind == PERCENT)
  {
    tk = token.kind;
    pos = token.pos;
    get_token();     // skip * / %

    parse_unary_expression (g_no_context, token.pos, null, out e2);

    // e1 = e1 op e2

    // check type

    t1 = e1^.base_type_or_null;
    t2 = e2^.base_type_or_null;

    if (t1 == null)   // error during earlier evaluation
    {
      free_exp (e2);
      e3 = e1;
    }
    else if (t2 == null)   // error during earlier evaluation
    {
      free_exp (e1);
      e3 = e2;
    }
    else
    {
      if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
      {
        t3 = promote_two_ints (e1, e2);
        if (t3 == null)
          semantic_error ("incompatible int/uint operands", pos);

        if (tk == SLASH || tk == PERCENT)
        {
          if (e2^.kind == A_CONST_INTEGER_VALUE && e2^.const_integer_value_info.value == 0)
            semantic_error ("division by zero", pos);
        }

        if (e1^.kind == A_CONST_INTEGER_VALUE && e2^.kind == A_CONST_INTEGER_VALUE)
        {
          v1 = e1^.const_integer_value_info.value;
          v2 = e2^.const_integer_value_info.value;

          if (tk == STAR)
          {
            rc = mul_int (v1, v2, out r);
          }
          else if (tk == SLASH)
          {
            rc = div_int (v1, v2, out r);
          }
          else
          {
            rc = mod_int (v1, v2, out r);
          }

          if (rc < 0)
          {
            semantic_error ("overflow", pos);
            r = 0;
          }
          else if (t3 != null)
          {
            // check if r outside range of t3

            ref INTEGER_INFO pi = INTEGER_DATA[(uint)t3^.the_integer_type.type];
            if (r < pi.min || r > pi.max)
            {
              semantic_error ("overflow", pos);
              r = 0;
            }
          }

          e3 = new EXPRESSION (A_CONST_INTEGER_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_CONSTANT;
          e3^.const_integer_value_info.value = r;

          free_exp (e1);
          free_exp (e2);
        }
        else  // runtime
        {
          e3 = new EXPRESSION (AN_OPERATOR_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_READONLY;

          is_signed = false;
          if (t3 != null)
          {
            ref INTEGER_INFO pi = INTEGER_DATA[(uint)t3^.the_integer_type.type];
            if (pi.is_signed)
              is_signed = true;
          }

          if (tk == STAR)
            op = is_signed ? OP_MUL_INT_SIGNED : OP_MUL_INT_UNSIGNED;
          else if (tk == SLASH)
            op = is_signed ? OP_DIV_INT_SIGNED : OP_DIV_INT_UNSIGNED;
          else
            op = is_signed ? OP_MOD_INT_SIGNED : OP_MOD_INT_UNSIGNED;

          e3^.operator_value_info.op = op;
          e3^.operator_value_info.arg[0] = e1;
          e3^.operator_value_info.arg[1] = e2;
        }
      }
      else if (t1^.kind == A_FLOAT_TYPE && t2^.kind == A_FLOAT_TYPE)
      {
        t3 = promote_two_floats (e1, e2, true);

        if (tk == PERCENT)
          semantic_error ("operator '%' is not allowed for float types", pos);
        else if (tk == SLASH)
        {
          if (e2^.kind == A_CONST_FLOAT_VALUE && e2^.const_float_value_info.value == 0.0)
            semantic_error ("division by zero", pos);
        }

        if (e1^.kind == A_CONST_FLOAT_VALUE && e2^.kind == A_CONST_FLOAT_VALUE)
        {
          vf1 = e1^.const_float_value_info.value;
          vf2 = e2^.const_float_value_info.value;

          if (tk == STAR)
          {
            rf = vf1 * vf2;
          }
          else
          {
            rf = vf1 / vf2;
          }

          check_float_overflow_underflow (ref rf, t3, pos);

          e3 = new EXPRESSION (A_CONST_FLOAT_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_CONSTANT;
          e3^.const_float_value_info.value = rf;

          free_exp (e1);
          free_exp (e2);
        }
        else  // runtime
        {
          e3 = new EXPRESSION (AN_OPERATOR_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_READONLY;
          e3^.operator_value_info.op = (tk == STAR) ? OP_MUL_FLOAT : OP_DIV_FLOAT;
          e3^.operator_value_info.arg[0] = e1;
          e3^.operator_value_info.arg[1] = e2;
        }
      }
      else
      {
        semantic_error ("illegal operands", pos);
        free_exp (e2);
        e3 = e1;
      }
    }

    e1 = e3;
  }

  pout = e1;
}

/*****************************************************************************/

void parse_sum (    CONTEXT       context,
                    TEXT_POSITION prefix_position,
                    PENTITY       prefix_name,    // can be null
                out PEXPRESSION   pout)
{
  PEXPRESSION   e1, e2, e3;
  TOKEN_KIND    tk;
  PENTITY       t1, t2, t3;
  TEXT_POSITION pos;
  int8          v1, v2, r;
  double        vf1, vf2, rf;
  int           rc;

  parse_term (context, prefix_position, prefix_name, out e1);

  while (token.kind == PLUS || token.kind == MINUS)
  {
    tk = token.kind;
    pos = token.pos;
    get_token();     // skip + or -

    parse_term (g_no_context, token.pos, null, out e2);

    // e3 = e1 op e2

    // check type

    t1 = e1^.base_type_or_null;
    t2 = e2^.base_type_or_null;

    if (t1 == null)   // error during earlier evaluation
    {
      free_exp (e2);
      e3 = e1;
    }
    else if (t2 == null)   // error during earlier evaluation
    {
      free_exp (e1);
      e3 = e2;
    }
    else
    {
      if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
      {
        t3 = promote_two_ints (e1, e2);
        if (t3 == null)
          semantic_error ("incompatible int/uint operands", pos);

        if (e1^.kind == A_CONST_INTEGER_VALUE && e2^.kind == A_CONST_INTEGER_VALUE)
        {
          v1 = e1^.const_integer_value_info.value;
          v2 = e2^.const_integer_value_info.value;

          if (tk == PLUS)
          {
            rc = add_int (v1, v2, out r);
          }
          else
          {
            rc = sub_int (v1, v2, out r);
          }

          if (rc < 0)
          {
            semantic_error ("overflow", pos);
            r = 0;
          }
          else if (t3 != null)
          {
            // check if r outside range of t3

            ref INTEGER_INFO pi = INTEGER_DATA[(uint)t3^.the_integer_type.type];
            if (r < pi.min || r > pi.max)
            {
              semantic_error ("overflow", pos);
              r = 0;
            }
          }

          e3 = new EXPRESSION (A_CONST_INTEGER_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_CONSTANT;
          e3^.const_integer_value_info.value = r;

          free_exp (e1);
          free_exp (e2);
        }
        else  // runtime
        {
          e3 = new EXPRESSION (AN_OPERATOR_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_READONLY;
          e3^.operator_value_info.op = (tk == PLUS) ? OP_ADD_INT : OP_SUB_INT;
          e3^.operator_value_info.arg[0] = e1;
          e3^.operator_value_info.arg[1] = e2;
        }
      }
      else if (t1^.kind == A_FLOAT_TYPE && t2^.kind == A_FLOAT_TYPE)
      {
        t3 = promote_two_floats (e1, e2, true);

        if (e1^.kind == A_CONST_FLOAT_VALUE && e2^.kind == A_CONST_FLOAT_VALUE)
        {
          vf1 = e1^.const_float_value_info.value;
          vf2 = e2^.const_float_value_info.value;

          if (tk == PLUS)
          {
            rf = vf1 + vf2;
          }
          else
          {
            rf = vf1 - vf2;
          }

          check_float_overflow_underflow (ref rf, t3, pos);

          e3 = new EXPRESSION (A_CONST_FLOAT_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_CONSTANT;
          e3^.const_float_value_info.value = rf;

          free_exp (e1);
          free_exp (e2);
        }
        else  // runtime
        {
          e3 = new EXPRESSION (AN_OPERATOR_VALUE);
          e3^.base_type_or_null = t3;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_READONLY;
          e3^.operator_value_info.op = (tk == PLUS) ? OP_ADD_FLOAT : OP_SUB_FLOAT;
          e3^.operator_value_info.arg[0] = e1;
          e3^.operator_value_info.arg[1] = e2;
        }
      }
      else if (t1^.kind == AN_ENUMERATION_TYPE && t2^.kind == AN_INTEGER_TYPE)
      {
        if (e1^.kind == A_CONST_ENUMERATION_VALUE && e2^.kind == A_CONST_INTEGER_VALUE)
        {
          v1 = e1^.const_enumeration_value_info.value;
          v2 = e2^.const_enumeration_value_info.value;

          if (tk == PLUS)
          {
            r = v1 + v2;
          }
          else
          {
            r = v1 - v2;
          }

          if (r < 0 || r > t1^.the_enumeration_type.last)
          {
            semantic_error ("overflow", pos);
            r = 0;
          }

          e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
          e3^.base_type_or_null = t1;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_CONSTANT;
          e3^.const_enumeration_value_info.value = (uint4)r;

          free_exp (e1);
          free_exp (e2);
        }
        else  // runtime
        {
          e3 = new EXPRESSION (AN_OPERATOR_VALUE);
          e3^.base_type_or_null = t1;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_READONLY;
          e3^.operator_value_info.op = (tk == PLUS) ? OP_ADD_INT : OP_SUB_INT;
          e3^.operator_value_info.arg[0] = e1;
          e3^.operator_value_info.arg[1] = e2;
        }
      }
      else if (t1^.kind == AN_UNSAFE_POINTER_TYPE && t2^.kind == AN_INTEGER_TYPE)
      {
        PENTITY element_type;

        element_type = t1^.the_unsafe_pointer_type.designated_type;
        if (is_open_type (element_type))
          semantic_error ("left operand must not be an unsafe pointer to an open type", pos);

        if (t2 == type_long)
          semantic_error ("right operand must not have type long", pos);

        e3 = new EXPRESSION (AN_OPERATOR_VALUE);
        e3^.base_type_or_null = t1;
        e3^.form = A_VALUE;
        e3^.access = ACCESS_READONLY;
        e3^.operator_value_info.op = (tk == PLUS) ? OP_ADD_PTR_INT : OP_SUB_PTR_INT;
        e3^.operator_value_info.arg[0] = e1;
        e3^.operator_value_info.arg[1] = e2;
      }
      else if (tk == MINUS && t1^.kind == AN_UNSAFE_POINTER_TYPE && t2^.kind == AN_UNSAFE_POINTER_TYPE)
      {
        PENTITY element_type;

        if (!types_are_equal (t1, t2))
          semantic_error ("incompatible operands", pos);
        else
        {
          element_type = t1^.the_unsafe_pointer_type.designated_type;
          if (is_open_type (element_type))
            semantic_error ("operands must not be unsafe pointers to an open type", pos);
        }

        e3 = new EXPRESSION (AN_OPERATOR_VALUE);
        e3^.base_type_or_null = type_uint;
        e3^.form = A_VALUE;
        e3^.access = ACCESS_READONLY;
        e3^.operator_value_info.op = OP_SUB_PTR_PTR;
        e3^.operator_value_info.arg[0] = e1;
        e3^.operator_value_info.arg[1] = e2;
      }
      else if (tk == PLUS &&
               (t1 == type_char || t1 == type_wchar || types_are_equal (t1, type_string) || types_are_equal (t1, type_wstring)) &&
               (t2 == type_char || t2 == type_wchar || types_are_equal (t2, type_string) || types_are_equal (t2, type_wstring)))
      {
        if ((e1^.kind != A_POOL_CONSTANT && e1^.kind != A_CONST_ENUMERATION_VALUE) ||
            (e2^.kind != A_POOL_CONSTANT && e2^.kind != A_CONST_ENUMERATION_VALUE))
        {
          semantic_error ("operands must be constant", pos);
          free_exp (e2);
          e3 = e1;
        }
        else
        {
          uint len, target_charsize, i, offset;
          bool wide;
          POOL p, src;
          int8 v;

          // build pool constant with concatenated string constants

          len = 0;

          if (t1 == type_char || t1 == type_wchar)
            len++;
          else
            len += e1^.constraint.value;

          if (t2 == type_char || t2 == type_wchar)
            len++;
          else
            len += e2^.constraint.value;

          // if any argument is wide, then the result will have type wstring,
          // otherwise the result has type string.
          wide = (t1 == type_wchar || types_are_equal (t1, type_wstring) ||
                  t2 == type_wchar || types_are_equal (t2, type_wstring));

          e3 = new EXPRESSION (A_POOL_CONSTANT);
          e3^.base_type_or_null = wide ? type_wstring : type_string;
          e3^.constraint.kind = CONSTANT_CONSTRAINT;
          e3^.constraint.value = len;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_CONSTANT;

          target_charsize = wide ? 2 : 1;

          p = new_pool_constant (target_charsize * len, target_charsize);
          offset = 0;

          e3^.pool_constant_info.pool_cte = p;

          if (t1 == type_char || t1 == type_wchar)
          {
            store_integer (p, offset, e1^.const_enumeration_value_info.value, target_charsize);
            offset += target_charsize;
          }
          else
          {
            int source_charsize = types_are_equal (t1, type_string) ? 1 : 2;
            src = e1^.pool_constant_info.pool_cte;
            for (i=0; i<e1^.constraint.value; i++)
            {
              load_integer (src, i*(uint)source_charsize, out v, (uint)source_charsize, is_signed => false);
              store_integer (p, offset, v, target_charsize);
              offset += target_charsize;
            }
          }

          if (t2 == type_char || t2 == type_wchar)
          {
            store_integer (p, offset, e2^.const_enumeration_value_info.value, target_charsize);
            offset += target_charsize;
          }
          else
          {
            int source_charsize = types_are_equal (t2, type_string) ? 1 : 2;
            src = e2^.pool_constant_info.pool_cte;
            for (i=0; i<e2^.constraint.value; i++)
            {
              load_integer (src, i*(uint)source_charsize, out v, (uint)source_charsize, is_signed => false);
              store_integer (p, offset, v, target_charsize);
              offset += target_charsize;
            }
          }

          free_exp (e1);
          free_exp (e2);
        }
      }
      else
      {
        semantic_error ("incompatible operands", pos);
        free_exp (e2);
        e3 = e1;
      }
    }

    e1 = e3;
  }

  pout = e1;
}

/*****************************************************************************/

void parse_simple_expression (    CONTEXT      context,
                                  TEXT_POSITION prefix_position,
                                  PENTITY       prefix_name,    // can be null
                              out PEXPRESSION   pout)
{
  PEXPRESSION   e1, e2, e3;
  PENTITY       t1, t2, t3;
  KIND_OPERATOR op;
  TOKEN_KIND    tk;
  TEXT_POSITION pos;
  int8          v1, v2, r;

  parse_sum (context, prefix_position, prefix_name, out e1);

  tk = token.kind;

  if (tk != SHIFT_LEFT && tk != SHIFT_RIGHT)
  {
    pout = e1;
    return;
  }

  pos = token.pos;
  get_token();     // skip << or >>
  parse_sum (g_no_context, token.pos, null, out e2);


  // check types

  t1 = e1^.base_type_or_null;
  t2 = e2^.base_type_or_null;

  if (t1 == null || t2 == null)   // error during earlier evaluation
  {
    free_exp (e2);
    pout = e1;
    return;
  }


  if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
  {
    t3 = promote_two_ints (e1, e2);
    if (t3 == null)
      semantic_error ("incompatible int/uint operands", pos);

    if (e2^.kind == A_CONST_INTEGER_VALUE && t3 != null)
    {
      v2 = e2^.const_integer_value_info.value;

      if (t3 == type_long || t3 == type_int_literal)
      {
        if (v2 < 0 || v2 > 63)
        {
          semantic_error ("right operand must be in range 0..63", pos);
          v2 = 0;
        }
      }
      else
      {
        if (v2 < 0 || v2 > 31)
        {
          semantic_error ("right operand must be in range 0..31", pos);
          v2 = 0;
        }
      }
    }

    if (e1^.kind == A_CONST_INTEGER_VALUE && e2^.kind == A_CONST_INTEGER_VALUE)
    {
      v1 = e1^.const_integer_value_info.value;
      v2 = e2^.const_integer_value_info.value;

      if (tk == SHIFT_LEFT)
      {
        r = v1 << v2;
        if (r >> v2 != v1)
        {
          semantic_error ("overflow", pos);
          r = 0;
        }
      }
      else
      {
        r = v1 >> v2;
      }

      if (t3 != null)
      {
        // check if r outside range of t3

        ref INTEGER_INFO pi = INTEGER_DATA[(uint)t3^.the_integer_type.type];
        if (r < pi.min || r > pi.max)
        {
          semantic_error ("overflow", pos);
          r = 0;
        }
      }

      e3 = new EXPRESSION (A_CONST_INTEGER_VALUE);
      e3^.base_type_or_null = t3;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_CONSTANT;
      e3^.const_integer_value_info.value = r;

      free_exp (e1);
      free_exp (e2);
    }
    else  // runtime
    {
      if (t3 != null && INTEGER_DATA[(uint)t3^.the_integer_type.type].is_signed)
        op = (tk == SHIFT_LEFT) ? OP_SHIFT_LEFT_SIGNED : OP_SHIFT_RIGHT_SIGNED;
      else
        op = (tk == SHIFT_LEFT) ? OP_SHIFT_LEFT_UNSIGNED : OP_SHIFT_RIGHT_UNSIGNED;

      e3 = new EXPRESSION (AN_OPERATOR_VALUE);
      e3^.base_type_or_null = t3;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_READONLY;
      e3^.operator_value_info.op = op;
      e3^.operator_value_info.arg[0] = e1;
      e3^.operator_value_info.arg[1] = e2;
    }
  }
  else
  {
    semantic_error ("operands must have type integer", pos);
    free_exp (e2);
    e3 = e1;
  }

  pout = e3;
}

/*****************************************************************************/

bool compare (int8 x, COMPARISON_FLAG cmp, int8 y)
{
  bool b;

  switch (cmp)
  {
    case CMP_SMALLER:
      b = x < y;
      break;

    case CMP_EQUAL:
      b = x == y;
      break;

    case CMP_SMALLER_OR_EQUAL:
      b = x <= y;
      break;

    case CMP_LARGER:
      b = x > y;
      break;

    case CMP_NOT_EQUAL:
      b = x != y;
      break;

    case CMP_LARGER_OR_EQUAL:
      b = x >= y;
      break;

    default:
      fatal_compiler_error ("compare()", token.pos);
      b = false;
      break;
  }

  return b;
}

/*****************************************************************************/

bool fcompare (double x, COMPARISON_FLAG cmp, double y)
{
  bool b;

  switch (cmp)
  {
    case CMP_SMALLER:
      b = x < y;
      break;

    case CMP_EQUAL:
      b = x == y;
      break;

    case CMP_SMALLER_OR_EQUAL:
      b = x <= y;
      break;

    case CMP_LARGER:
      b = x > y;
      break;

    case CMP_NOT_EQUAL:
      b = x != y;
      break;

    case CMP_LARGER_OR_EQUAL:
      b = x >= y;
      break;

    default:
      fatal_compiler_error ("fcompare()", token.pos);
      b = false;
      break;
  }

  return b;
}

/*****************************************************************************/

void verify_real_function (PENTITY f, TEXT_POSITION pos)
{
  if (f^.kind == A_NULL_POINTER_TYPE)
    return;

  assert f^.kind == A_FUNCTION_POINTER_TYPE;

  {
    ref A_FUNCTION_POINTER_TYPE_ENTITY pfunc = f^.the_function_pointer_type;

    if (pfunc.extern_dll_or_null != null)
      semantic_error ("a function with option extern is not allowed here", pos);

    if (pfunc.is_syscall)
      semantic_error ("a function with option syscall is not allowed here", pos);

    if (pfunc.is_entry)
      semantic_error ("an entry-point function is not allowed here", pos);
  }
}

/*****************************************************************************/

void parse_relation (    CONTEXT       context,
                         TEXT_POSITION prefix_position,
                         PENTITY       prefix_name,    // can be null
                     out PEXPRESSION   pout)
{
  TEXT_POSITION   pos;
  PEXPRESSION     e1, e2, e3;
  KIND_OPERATOR   op;
  PENTITY         t1, t2, t3;
  bool            b;
  COMPARISON_FLAG cmp;

  parse_simple_expression (context, prefix_position, prefix_name, out e1);

  pos = token.pos;
  switch (token.kind)
  {
    case SMALLER:
      cmp = CMP_SMALLER;
      break;

    case EQUAL:
      cmp = CMP_EQUAL;
      break;

    case SMALLER_OR_EQUAL:
      cmp = CMP_SMALLER_OR_EQUAL;
      break;

    case LARGER:
      cmp = CMP_LARGER;
      break;

    case NOT_EQUAL:
      cmp = CMP_NOT_EQUAL;
      break;

    case LARGER_OR_EQUAL:
      cmp = CMP_LARGER_OR_EQUAL;
      break;

    default:
      cmp = CMP_FILLER;
      break;
  }

  if (cmp == CMP_FILLER)
  {
    pout = e1;
    return;
  }


  get_token();
  parse_simple_expression (g_no_context, token.pos, null, out e2);


  // check types

  t1 = e1^.base_type_or_null;
  t2 = e2^.base_type_or_null;

  if (t1 == null || t2 == null)   // error during earlier evaluation
  {
    e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
    e3^.base_type_or_null = type_bool;
    e3^.form = A_VALUE;
    e3^.access = ACCESS_CONSTANT;
    e3^.const_enumeration_value_info.value = 0;

    free_exp (e1);
    free_exp (e2);

    pout = e3;
    return;
  }


  if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
  {
    t3 = promote_two_ints (e1, e2);
    if (t3 == null)
      semantic_error ("incompatible int/uint operands", pos);

    if (e1^.kind == A_CONST_INTEGER_VALUE && e2^.kind == A_CONST_INTEGER_VALUE)
    {
      b = compare (e1^.const_integer_value_info.value,
                   cmp,
                   e2^.const_integer_value_info.value);

      e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_CONSTANT;
      e3^.const_enumeration_value_info.value = (uint)b;

      free_exp (e1);
      free_exp (e2);
    }
    else  // runtime
    {
      if (e1^.kind == A_CONST_INTEGER_VALUE)
      {
        if (e1^.const_integer_value_info.value == INTEGER_DATA[(uint)t2^.the_integer_type.type].min)
        {
          if (cmp == CMP_LARGER)   // t'min > e -> false
            warning ("always false", pos);
          else if (cmp == CMP_SMALLER_OR_EQUAL)  // t'min <= e -> true
            warning ("always true", pos);
        }

        if (e1^.const_integer_value_info.value == INTEGER_DATA[(uint)t2^.the_integer_type.type].max)
        {
          if (cmp == CMP_SMALLER)   // t'max < e -> false
            warning ("always false", pos);
          else if (cmp == CMP_LARGER_OR_EQUAL)  // t'max >= e -> true
            warning ("always true", pos);
        }
      }
      else if (e2^.kind == A_CONST_INTEGER_VALUE)
      {
        if (e2^.const_integer_value_info.value == INTEGER_DATA[(uint)t1^.the_integer_type.type].min)
        {
          if (cmp == CMP_SMALLER)   // e < t'min -> false
            warning ("always false", pos);
          else if (cmp == CMP_LARGER_OR_EQUAL)  // e >= t'min -> true
            warning ("always true", pos);
        }

        if (e2^.const_integer_value_info.value == INTEGER_DATA[(uint)t1^.the_integer_type.type].max)
        {
          if (cmp == CMP_LARGER)   // e > t'max -> false
            warning ("always false", pos);
          else if (cmp == CMP_SMALLER_OR_EQUAL)  // e <= t'max -> true
            warning ("always true", pos);
        }
      }

      if (t3 != null && INTEGER_DATA[(uint)t3^.the_integer_type.type].is_signed)
        op = OP_COMPARE_SIGNED;
      else
        op = OP_COMPARE_UNSIGNED;

      e3 = new EXPRESSION (AN_OPERATOR_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_READONLY;
      e3^.operator_value_info.op = op;
      e3^.operator_value_info.cmp = cmp;
      e3^.operator_value_info.arg[0] = e1;
      e3^.operator_value_info.arg[1] = e2;
      e3^.operator_value_info.type = t3;
    }
  }
  else if (t1^.kind == A_FLOAT_TYPE && t2^.kind == A_FLOAT_TYPE)
  {
    t3 = promote_two_floats (e1, e2, true);

    if (e1^.kind == A_CONST_FLOAT_VALUE && e2^.kind == A_CONST_FLOAT_VALUE)
    {
      b = fcompare (e1^.const_float_value_info.value,
                    cmp,
                    e2^.const_float_value_info.value);

      e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_CONSTANT;
      e3^.const_enumeration_value_info.value = (uint)b;

      free_exp (e1);
      free_exp (e2);
    }
    else  // runtime
    {
      e3 = new EXPRESSION (AN_OPERATOR_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_READONLY;
      e3^.operator_value_info.op = OP_COMPARE_FLOAT;
      e3^.operator_value_info.cmp = cmp;
      e3^.operator_value_info.arg[0] = e1;
      e3^.operator_value_info.arg[1] = e2;
      e3^.operator_value_info.type = t3;
    }
  }
  else if (t1^.kind == AN_ENUMERATION_TYPE && t2^.kind == AN_ENUMERATION_TYPE)
  {
    if (!types_are_equal (t1, t2))
      semantic_error ("incompatible enum operands", pos);

    if (e1^.kind == A_CONST_ENUMERATION_VALUE && e2^.kind == A_CONST_ENUMERATION_VALUE)
    {
      b = compare (e1^.const_enumeration_value_info.value,
                   cmp,
                   e2^.const_enumeration_value_info.value);

      e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_CONSTANT;
      e3^.const_enumeration_value_info.value = (uint)b;

      free_exp (e1);
      free_exp (e2);
    }
    else  // runtime
    {
      if (e1^.kind == A_CONST_ENUMERATION_VALUE)
      {
        if (e1^.const_enumeration_value_info.value == 0)
        {
          if (cmp == CMP_LARGER)   // 0 > e -> false
            warning ("always false", pos);
          else if (cmp == CMP_SMALLER_OR_EQUAL)  // 0 <= e -> true
            warning ("always true", pos);
        }
      }
      else if (e2^.kind == A_CONST_ENUMERATION_VALUE)
      {
        if (e2^.const_enumeration_value_info.value == 0)
        {
          if (cmp == CMP_SMALLER)   // e < 0 -> false
            warning ("always false", pos);
          else if (cmp == CMP_LARGER_OR_EQUAL)  // e >= 0 -> true
            warning ("always true", pos);
        }
      }

      e3 = new EXPRESSION (AN_OPERATOR_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_READONLY;
      e3^.operator_value_info.op = OP_COMPARE_UNSIGNED;
      e3^.operator_value_info.cmp = cmp;
      e3^.operator_value_info.arg[0] = e1;
      e3^.operator_value_info.arg[1] = e2;
      e3^.operator_value_info.type = t1;
    }
  }
  else if (t1^.kind == A_NULL_POINTER_TYPE && t2^.kind == A_NULL_POINTER_TYPE)
  {
    if (cmp != CMP_EQUAL && cmp != CMP_NOT_EQUAL)
      semantic_error ("order comparison of null pointer is not allowed", pos);

    b = compare (0, cmp, 0);

    e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
    e3^.base_type_or_null = type_bool;
    e3^.form = A_VALUE;
    e3^.access = ACCESS_CONSTANT;
    e3^.const_enumeration_value_info.value = (uint)b;

    free_exp (e1);
    free_exp (e2);
  }
  else if ((t1^.kind == A_POINTER_TYPE || t1^.kind == A_NULL_POINTER_TYPE) &&
           (t2^.kind == A_POINTER_TYPE || t2^.kind == A_NULL_POINTER_TYPE))
  {
    if (t1^.kind == A_POINTER_TYPE && t2^.kind == A_POINTER_TYPE && (!types_are_equal (t1, t2)))
      semantic_error ("incompatible pointers", pos);

    if (cmp != CMP_EQUAL && cmp != CMP_NOT_EQUAL)
      semantic_error ("order comparison of pointers is not allowed", pos);

    if (e1^.kind == A_CONST_NULL_VALUE && e2^.kind == A_CONST_NULL_VALUE)
    {
      b = compare (0, cmp, 0);

      e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_CONSTANT;
      e3^.const_enumeration_value_info.value = (uint)b;

      free_exp (e1);
      free_exp (e2);
    }
    else  // runtime
    {
      e3 = new EXPRESSION (AN_OPERATOR_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_READONLY;
      e3^.operator_value_info.op = OP_COMPARE_PTR;
      e3^.operator_value_info.cmp = cmp;
      e3^.operator_value_info.arg[0] = e1;
      e3^.operator_value_info.arg[1] = e2;
    }
  }
  else if ((t1^.kind == A_FUNCTION_POINTER_TYPE || t1^.kind == A_NULL_POINTER_TYPE) &&
           (t2^.kind == A_FUNCTION_POINTER_TYPE || t2^.kind == A_NULL_POINTER_TYPE))
  {
    if (t1^.kind == A_FUNCTION_POINTER_TYPE && t2^.kind == A_FUNCTION_POINTER_TYPE && (!types_are_equal (t1, t2)))
      semantic_error ("incompatible function pointers", pos);

    verify_real_function (t1, pos);
    verify_real_function (t2, pos);

    if (cmp != CMP_EQUAL && cmp != CMP_NOT_EQUAL)
      semantic_error ("order comparison of function pointers is not allowed", pos);

    if (e1^.kind == A_CONST_NULL_VALUE && e2^.kind == A_CONST_NULL_VALUE)
    {
      b = compare (0, cmp, 0);

      e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_CONSTANT;
      e3^.const_enumeration_value_info.value = (uint)b;

      free_exp (e1);
      free_exp (e2);
    }
    else  // runtime
    {
      e3 = new EXPRESSION (AN_OPERATOR_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_READONLY;
      e3^.operator_value_info.op = OP_COMPARE_PTR;
      e3^.operator_value_info.cmp = cmp;
      e3^.operator_value_info.arg[0] = e1;
      e3^.operator_value_info.arg[1] = e2;
    }
  }
  else if ((t1^.kind == AN_UNSAFE_POINTER_TYPE || t1^.kind == A_NULL_POINTER_TYPE) &&
           (t2^.kind == AN_UNSAFE_POINTER_TYPE || t2^.kind == A_NULL_POINTER_TYPE))
  {
    if (t1^.kind == AN_UNSAFE_POINTER_TYPE && t2^.kind == AN_UNSAFE_POINTER_TYPE && (!types_are_equal (t1, t2)))
      semantic_error ("incompatible unsafe pointers", pos);

    if (e1^.kind == A_CONST_NULL_VALUE && e2^.kind == A_CONST_NULL_VALUE)
    {
      b = compare (0, cmp, 0);

      e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_CONSTANT;
      e3^.const_enumeration_value_info.value = (uint)b;

      free_exp (e1);
      free_exp (e2);
    }
    else  // runtime
    {
      e3 = new EXPRESSION (AN_OPERATOR_VALUE);
      e3^.base_type_or_null = type_bool;
      e3^.form = A_VALUE;
      e3^.access = ACCESS_READONLY;
      e3^.operator_value_info.op = OP_COMPARE_PTR;
      e3^.operator_value_info.cmp = cmp;
      e3^.operator_value_info.arg[0] = e1;
      e3^.operator_value_info.arg[1] = e2;
    }
  }
  else
  {
    semantic_error ("illegal operands for comparison", pos);

    e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
    e3^.base_type_or_null = type_bool;
    e3^.form = A_VALUE;
    e3^.access = ACCESS_CONSTANT;
    e3^.const_enumeration_value_info.value = 0;

    free_exp (e1);
    free_exp (e2);
  }

  pout = e3;
}

/*****************************************************************************/

void parse_sub_expression (    CONTEXT       context,
                               TEXT_POSITION prefix_position,
                               PENTITY       prefix_name,    // can be null
                           out PEXPRESSION   pout)
{
  TEXT_POSITION pos1, pos2, pos;
  PEXPRESSION   e1, e2, e3;
  PENTITY       t1, t2, t3;
  int8          v1, v2, v3;
  TOKEN_KIND    tk;
  KIND_OPERATOR op;

  pos1 = token.pos;

  parse_relation (context, prefix_position, prefix_name, out e1);

  switch (token.kind)
  {
    case DOUBLE_AMPERSAND:     // &&
    case DOUBLE_VERTICAL_BAR:  // ||

      tk = token.kind;

      while (token.kind == tk)
      {
        get_token();
        pos2 = token.pos;
        parse_relation (g_no_context, token.pos, null, out e2);

        // e3 = e1 op e2;

        type_check_bool_operator (e1, pos1);
        type_check_bool_operator (e2, pos2);

        // compute result

        if (e1^.kind == A_CONST_ENUMERATION_VALUE && e2^.kind == A_CONST_ENUMERATION_VALUE)
        {
          v1 = e1^.const_enumeration_value_info.value;
          v2 = e2^.const_enumeration_value_info.value;

          e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
          e3^.base_type_or_null = type_bool;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_CONSTANT;
          e3^.const_enumeration_value_info.value =
             (uint4)((tk==DOUBLE_AMPERSAND) ? ((bool)v1 && (bool)v2) : ((bool)v1 || (bool)v2));

          free_exp (e1);
          free_exp (e2);
        }
        else   // runtime
        {
          e3 = new EXPRESSION (AN_OPERATOR_VALUE);
          e3^.base_type_or_null = type_bool;
          e3^.form = A_VALUE;
          e3^.access = ACCESS_READONLY;
          e3^.operator_value_info.op =
              (tk==DOUBLE_AMPERSAND) ? OP_SHORT_CIRCUIT_AND : OP_SHORT_CIRCUIT_OR;
          e3^.operator_value_info.arg[0] = e1;
          e3^.operator_value_info.arg[1] = e2;
        }

        e1 = e3;
      }
      break;


    case AMPERSAND:     // &
    case VERTICAL_BAR:  // |
    case CARET:         // ^

      tk = token.kind;

      while (token.kind == tk)
      {
        pos = token.pos;
        get_token();
        pos2 = token.pos;
        parse_relation (g_no_context, token.pos, null, out e2);

        // e3 = e1 op e2;

        t1 = e1^.base_type_or_null;
        t2 = e2^.base_type_or_null;

        if (t1 == null)   // error during earlier evaluation
        {
          free_exp (e2);
          e3 = e1;
        }
        else if (t2 == null)   // error during earlier evaluation
        {
          free_exp (e1);
          e3 = e2;
        }
        else
        {
          if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
          {
            t3 = promote_two_ints (e1, e2);
            if (t3 == null)
              semantic_error ("incompatible int/uint operands", pos);

            if (e1^.kind == A_CONST_INTEGER_VALUE && e2^.kind == A_CONST_INTEGER_VALUE)
            {
              v1 = e1^.const_integer_value_info.value;
              v2 = e2^.const_integer_value_info.value;

              if (tk == AMPERSAND)
                v3 = v1 & v2;
              else if (tk == VERTICAL_BAR)
                v3 = v1 | v2;
              else
                v3 = v1 ^ v2;

              e3 = new EXPRESSION (A_CONST_INTEGER_VALUE);
              e3^.base_type_or_null = t3;
              e3^.form = A_VALUE;
              e3^.access = ACCESS_CONSTANT;
              e3^.const_integer_value_info.value = v3;

              free_exp (e1);
              free_exp (e2);
            }
            else  // runtime
            {
              e3 = new EXPRESSION (AN_OPERATOR_VALUE);
              e3^.base_type_or_null = t3;
              e3^.form = A_VALUE;
              e3^.access = ACCESS_READONLY;

              if (tk == AMPERSAND)
                op = OP_BITAND;
              else if (tk == VERTICAL_BAR)
                op = OP_BITOR;
              else
                op = OP_BITXOR;

              e3^.operator_value_info.op = op;

              e3^.operator_value_info.arg[0] = e1;
              e3^.operator_value_info.arg[1] = e2;
            }
          }
          else if (t1 == type_bool && t2 == type_bool)
          {
            if (e1^.kind == A_CONST_ENUMERATION_VALUE && e2^.kind == A_CONST_ENUMERATION_VALUE)
            {
              v1 = e1^.const_enumeration_value_info.value;
              v2 = e2^.const_enumeration_value_info.value;

              if (tk == AMPERSAND)
                v3 = (uint)((bool)v1 && (bool)v2);
              else if (tk == VERTICAL_BAR)
                v3 = (uint)((bool)v1 || (bool)v2);
              else
                v3 = (uint)((bool)v1 ^ (bool)v2);

              e3 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
              e3^.base_type_or_null = type_bool;
              e3^.form = A_VALUE;
              e3^.access = ACCESS_CONSTANT;
              e3^.const_enumeration_value_info.value = (uint4)v3;

              free_exp (e1);
              free_exp (e2);
            }
            else  // runtime
            {
              e3 = new EXPRESSION (AN_OPERATOR_VALUE);
              e3^.base_type_or_null = type_bool;
              e3^.form = A_VALUE;
              e3^.access = ACCESS_READONLY;

              if (tk == AMPERSAND)
                op = OP_AND;
              else if (tk == VERTICAL_BAR)
                op = OP_OR;
              else
                op = OP_XOR;

              e3^.operator_value_info.op = op;
              e3^.operator_value_info.arg[0] = e1;
              e3^.operator_value_info.arg[1] = e2;
            }
          }
          else
          {
            semantic_error ("operator has bad operands", pos);
            e3 = e1;
            free_exp (e2);
          }
        }

        e1 = e3;
      }
      break;

    default:
      break;
  }

  pout = e1;
}

/*****************************************************************************/

bool in_range (int8 value, INTEGER_TYPE typ)
{
  return value >= INTEGER_DATA[(uint)typ].min && value <= INTEGER_DATA[(uint)typ].max;
}

/*****************************************************************************/

public
void parse_expression (    CONTEXT       context,
                           TEXT_POSITION prefix_position,
                           PENTITY       prefix_name,     // can be null
                       out PEXPRESSION   pout)
{
  TEXT_POSITION pos, posc;
  PEXPRESSION   e1, e2, e3, e4;
  PENTITY       t1, t2, t3, t4;
  CONTEXT       inner_context;

  parse_sub_expression (context, prefix_position, prefix_name, out e1);

  if (token.kind != QUESTION_MARK)
  {
    pout = e1;
    return;
  }

  pos = token.pos;
  get_token();     // skip '?'

  inner_context = context;
  if (inner_context.constraint.kind == RUNTIME_CONSTRAINT)
    inner_context.constraint.kind = UNCONSTRAINED;

  parse_sub_expression (inner_context, token.pos, null, out e2);

  if (token.kind != COLON)
  {
    syntax_error ("':' expected", token.pos);
    free_exp (e1);
    pout = e2;
    return;
  }

  posc = token.pos;
  get_token();     // skip ':'

  parse_sub_expression (inner_context, token.pos, null, out e3);


  // type check

  t1 = e1^.base_type_or_null;
  t2 = e2^.base_type_or_null;
  t3 = e3^.base_type_or_null;

  if (t1 != null && t1 != type_bool)
    semantic_error ("prefix must have type bool", pos);

  if (t2 == null)   // error during earlier evaluation
  {
    free_exp (e1);
    free_exp (e3);
    e4 = e2;
  }
  else if (t3 == null)   // error during earlier evaluation
  {
    free_exp (e1);
    free_exp (e2);
    e4 = e3;
  }
  else
  {
    if (t2^.kind == AN_INTEGER_TYPE && t3^.kind == AN_INTEGER_TYPE)
    {
      t4 = promote_two_ints (e2, e3);
      if (t4 == null)
        semantic_error ("incompatible int/uint types", pos);

      if (e1^.kind == A_CONST_ENUMERATION_VALUE &&
          e2^.kind == A_CONST_INTEGER_VALUE &&
          e3^.kind == A_CONST_INTEGER_VALUE)
      {
        e4 = new EXPRESSION (A_CONST_INTEGER_VALUE);
        e4^.base_type_or_null = t4;
        e4^.form = A_VALUE;
        e4^.access = ACCESS_CONSTANT;
        e4^.const_integer_value_info.value =
             (e1^.const_enumeration_value_info.value == 1)
             ? e2^.const_integer_value_info.value
             : e3^.const_integer_value_info.value;

        free_exp (e1);
        free_exp (e2);
        free_exp (e3);
      }
      else
      {
        e4 = new EXPRESSION (AN_OPERATOR_VALUE);

        // special case for: context = b ? e1 : e2
        // if e1 and e2 are integer literals without type
        // when context is unsigned and the literals fit in uint,
        // then change e1 e2 and result type t4 to uint.

        t4 = promote_two_ints (e2, e3);
        
        if (t4 == null)
        {
          semantic_error ("incompatible int/uint types", pos);
        }
        else if (t4 == type_int_literal)
        {
          // we need to force both literals into real types

          bool unsigned_context =
                 context.base_type_or_null != null &&
                 context.base_type_or_null^.kind == AN_INTEGER_TYPE &&
                 !INTEGER_DATA[(uint)context.base_type_or_null^.the_integer_type.type].is_signed;

          if (unsigned_context &&    // prefer uint if there is an unsigned context
              in_range (e2^.const_integer_value_info.value, a_uint4) &&
              in_range (e3^.const_integer_value_info.value, a_uint4))
          {
            t4 = type_uint;
          }
          else if (in_range (e2^.const_integer_value_info.value, a_int4) &&
                   in_range (e3^.const_integer_value_info.value, a_int4))
          {
            t4 = type_int;   // prefer int4 if it's in range
          }
          else
          {
            t4 = type_long;  // in other cases, use long
          }
        }

        e4^.base_type_or_null = t4;
        e4^.form = A_VALUE;
        e4^.access = ACCESS_READONLY;
        e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
        e4^.operator_value_info.arg[0] = e1;
        e4^.operator_value_info.arg[1] = e2;
        e4^.operator_value_info.arg[2] = e3;
      }
    }
    else if (t2^.kind == A_FLOAT_TYPE && t3^.kind == A_FLOAT_TYPE)
    {
      t4 = promote_two_floats (e2, e3, true);

      if (e1^.kind == A_CONST_ENUMERATION_VALUE &&
          e2^.kind == A_CONST_FLOAT_VALUE &&
          e3^.kind == A_CONST_FLOAT_VALUE)
      {
        e4 = new EXPRESSION (A_CONST_FLOAT_VALUE);
        e4^.base_type_or_null = t4;
        e4^.form = A_VALUE;
        e4^.access = ACCESS_CONSTANT;
        e4^.const_float_value_info.value =
             (e1^.const_enumeration_value_info.value == 1)
             ? e2^.const_float_value_info.value
             : e3^.const_float_value_info.value;

        free_exp (e1);
        free_exp (e2);
        free_exp (e3);
      }
      else
      {
        e4 = new EXPRESSION (AN_OPERATOR_VALUE);
        e4^.base_type_or_null = promote_two_floats (e2, e3, false);
        e4^.form = A_VALUE;
        e4^.access = ACCESS_READONLY;
        e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
        e4^.operator_value_info.arg[0] = e1;
        e4^.operator_value_info.arg[1] = e2;
        e4^.operator_value_info.arg[2] = e3;
      }
    }
    else if (t2^.kind == AN_ENUMERATION_TYPE && t3^.kind == AN_ENUMERATION_TYPE)
    {
      if (!types_are_equal (t2, t3))
        semantic_error ("incompatible enum types", posc);

      t4 = t2;

      if (e1^.kind == A_CONST_ENUMERATION_VALUE &&
          e2^.kind == A_CONST_ENUMERATION_VALUE &&
          e3^.kind == A_CONST_ENUMERATION_VALUE)
      {
        e4 = new EXPRESSION (A_CONST_ENUMERATION_VALUE);
        e4^.base_type_or_null = t4;
        e4^.form = A_VALUE;
        e4^.access = ACCESS_CONSTANT;
        e4^.const_enumeration_value_info.value =
             (e1^.const_enumeration_value_info.value == 1)
             ? e2^.const_enumeration_value_info.value
             : e3^.const_enumeration_value_info.value;

        free_exp (e1);
        free_exp (e2);
        free_exp (e3);
      }
      else
      {
        e4 = new EXPRESSION (AN_OPERATOR_VALUE);
        e4^.base_type_or_null = t4;
        e4^.form = A_VALUE;
        e4^.access = ACCESS_READONLY;
        e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
        e4^.operator_value_info.arg[0] = e1;
        e4^.operator_value_info.arg[1] = e2;
        e4^.operator_value_info.arg[2] = e3;
      }
    }
    else if (t2^.kind == A_NULL_POINTER_TYPE && t3^.kind == A_NULL_POINTER_TYPE)
    {
      e4 = new EXPRESSION (A_CONST_NULL_VALUE);
      e4^.base_type_or_null = type_null_literal;
      e4^.form = A_VALUE;
      e4^.access = ACCESS_CONSTANT;

      free_exp (e1);
      free_exp (e2);
      free_exp (e3);
    }
    else if ((t2^.kind == A_POINTER_TYPE || t2^.kind == A_NULL_POINTER_TYPE) &&
             (t3^.kind == A_POINTER_TYPE || t3^.kind == A_NULL_POINTER_TYPE))
    {
      t4 = (t2^.kind == A_POINTER_TYPE) ? t2 : t3;

      if (t2^.kind == A_POINTER_TYPE && t3^.kind == A_POINTER_TYPE && (!types_are_equal (t2, t3)))
        semantic_error ("incompatible pointer types", posc);

      e4 = new EXPRESSION (AN_OPERATOR_VALUE);
      e4^.base_type_or_null = t4;
      e4^.form = A_VALUE;
      e4^.access = ACCESS_READONLY;
      e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
      e4^.operator_value_info.arg[0] = e1;
      e4^.operator_value_info.arg[1] = e2;
      e4^.operator_value_info.arg[2] = e3;
    }
    else if ((t2^.kind == A_FUNCTION_POINTER_TYPE || t2^.kind == A_NULL_POINTER_TYPE) &&
             (t3^.kind == A_FUNCTION_POINTER_TYPE || t3^.kind == A_NULL_POINTER_TYPE))
    {
      verify_real_function (t2, posc);
      verify_real_function (t3, posc);

      t4 = (t2^.kind == A_FUNCTION_POINTER_TYPE) ? t2 : t3;

      if (t2^.kind == A_FUNCTION_POINTER_TYPE && t3^.kind == A_FUNCTION_POINTER_TYPE && (!types_are_equal (t2, t3)))
        semantic_error ("incompatible function pointer types", posc);

      e4 = new EXPRESSION (AN_OPERATOR_VALUE);
      e4^.base_type_or_null = t4;
      e4^.form = A_VALUE;
      e4^.access = ACCESS_READONLY;
      e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
      e4^.operator_value_info.arg[0] = e1;
      e4^.operator_value_info.arg[1] = e2;
      e4^.operator_value_info.arg[2] = e3;
    }
    else if ((t2^.kind == AN_UNSAFE_POINTER_TYPE || t2^.kind == A_NULL_POINTER_TYPE) &&
             (t3^.kind == AN_UNSAFE_POINTER_TYPE || t3^.kind == A_NULL_POINTER_TYPE))
    {
      t4 = (t2^.kind == AN_UNSAFE_POINTER_TYPE) ? t2 : t3;

      if (t2^.kind == AN_UNSAFE_POINTER_TYPE && t3^.kind == AN_UNSAFE_POINTER_TYPE && (!types_are_equal (t2, t3)))
        semantic_error ("incompatible unsafe pointer types", posc);

      e4 = new EXPRESSION (AN_OPERATOR_VALUE);
      e4^.base_type_or_null = t4;
      e4^.form = A_VALUE;
      e4^.access = ACCESS_READONLY;
      e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
      e4^.operator_value_info.arg[0] = e1;
      e4^.operator_value_info.arg[1] = e2;
      e4^.operator_value_info.arg[2] = e3;
    }
    else if (t2^.kind == AN_OPEN_ARRAY_TYPE && t3^.kind == AN_OPEN_ARRAY_TYPE)
    {
      if (!types_are_equal (t2, t3))
        semantic_error ("incompatible operands", posc);

      e4 = new EXPRESSION (AN_OPERATOR_VALUE);
      e4^.base_type_or_null = t2;

      // check array constraints
      // (e2 const or runtime, e3 const or runtime, context unc., const or runtime)

      if (context.constraint.kind == UNCONSTRAINED)
      {
        // in an unconstrained context, the lengths can (but need not) be different.

        if (e2^.constraint.kind == CONSTANT_CONSTRAINT &&
            e3^.constraint.kind == CONSTANT_CONSTRAINT &&
            e2^.constraint.value == e3^.constraint.value)
        {
          e4^.constraint.kind = CONSTANT_CONSTRAINT;
          e4^.constraint.value = e2^.constraint.value;
        }
        else   // different or runtime
        {
          e4^.constraint.kind = RUNTIME_CONSTRAINT;
          e4^.constraint.value = 0;
        }
      }
      else  // the context is constrained  (or does not apply)
      {
        // both lengths must be equal

        if (e2^.constraint.kind == CONSTANT_CONSTRAINT &&
            e3^.constraint.kind == CONSTANT_CONSTRAINT &&
            e2^.constraint.value != e3^.constraint.value)
        {
          semantic_error ("array lengths do not match", posc);
        }

        if (e2^.constraint.kind == CONSTANT_CONSTRAINT ||
            e3^.constraint.kind == CONSTANT_CONSTRAINT)
        {
          e4^.constraint.kind = CONSTANT_CONSTRAINT;

          if (e2^.constraint.kind == CONSTANT_CONSTRAINT)
            e4^.constraint.value = e2^.constraint.value;
          else
            e4^.constraint.value = e3^.constraint.value;
        }
        else   // both runtime
        {
          e4^.constraint.kind = RUNTIME_CONSTRAINT;
          e4^.constraint.value = 0;
        }
      }

      e4^.form = A_VALUE;
      e4^.access = ACCESS_READONLY;
      e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
      e4^.operator_value_info.arg[0] = e1;
      e4^.operator_value_info.arg[1] = e2;
      e4^.operator_value_info.arg[2] = e3;
    }
    else if (t2^.kind == A_STRUCT_TYPE && t2^.the_struct_type.is_open_type)
    {
      if (!types_are_equal (t2, t3))
        semantic_error ("incompatible operands", posc);

      e4 = new EXPRESSION (AN_OPERATOR_VALUE);
      e4^.base_type_or_null = t2;

      // check struct constraints
      // (e2 const or runtime, e3 const or runtime, context unc., const or runtime)

      if (context.constraint.kind == UNCONSTRAINED)
      {
        // in an unconstrained context, the discriminants can (but need not) be different.

        if (e2^.constraint.kind == CONSTANT_CONSTRAINT &&
            e3^.constraint.kind == CONSTANT_CONSTRAINT &&
            e2^.constraint.value == e3^.constraint.value)
        {
          e4^.constraint.kind = CONSTANT_CONSTRAINT;
          e4^.constraint.value = e2^.constraint.value;
        }
        else   // different or runtime
        {
          e4^.constraint.kind = RUNTIME_CONSTRAINT;
          e4^.constraint.value = 0;
        }
      }
      else  // the context is constrained.
      {
        // both discriminants must be equal

        if (e2^.constraint.kind == CONSTANT_CONSTRAINT &&
            e3^.constraint.kind == CONSTANT_CONSTRAINT &&
            e2^.constraint.value != e3^.constraint.value)
        {
          semantic_error ("struct discriminants do not match", posc);
        }

        if (e2^.constraint.kind == CONSTANT_CONSTRAINT ||
            e3^.constraint.kind == CONSTANT_CONSTRAINT)
        {
          e4^.constraint.kind = CONSTANT_CONSTRAINT;

          if (e2^.constraint.kind == CONSTANT_CONSTRAINT)
            e4^.constraint.value = e2^.constraint.value;
          else
            e4^.constraint.value = e3^.constraint.value;
        }
        else   // both runtime
        {
          e4^.constraint.kind = RUNTIME_CONSTRAINT;
          e4^.constraint.value = 0;
        }
      }

      e4^.form = A_VALUE;
      e4^.access = ACCESS_READONLY;
      e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
      e4^.operator_value_info.arg[0] = e1;
      e4^.operator_value_info.arg[1] = e2;
      e4^.operator_value_info.arg[2] = e3;
    }
    else if ((t2^.kind == A_STRUCT_TYPE && !t2^.the_struct_type.is_open_type) ||
             t2^.kind == A_UNION_TYPE ||
             t2^.kind == AN_OPAQUE_TYPE ||
             t2^.kind == A_GENERIC_TYPE)
    {
      if (!types_are_equal (t2, t3))
        semantic_error ("incompatible operands", posc);

      e4 = new EXPRESSION (AN_OPERATOR_VALUE);
      e4^.base_type_or_null = t2;
      e4^.form = A_VALUE;
      e4^.access = ACCESS_READONLY;
      e4^.operator_value_info.op = OP_CONDITIONAL_TEST;
      e4^.operator_value_info.arg[0] = e1;
      e4^.operator_value_info.arg[1] = e2;
      e4^.operator_value_info.arg[2] = e3;
    }
    else
    {
      semantic_error ("illegal operands", posc);
      free_exp (e1);
      free_exp (e3);
      e4 = e2;
    }
  }

  pout = e4;
}

/*****************************************************************************/

public
int8 parse_constant_int_or_uint_expression ()
{
  TEXT_POSITION pos;
  PEXPRESSION   e;
  int8          value;

  pos = token.pos;

  parse_expression (g_no_context, token.pos, null, out e);

  if (e^.kind != A_CONST_INTEGER_VALUE)
  {
    semantic_error ("must be integer constant expression", pos);
    value = 1;
  }
  else
  {
    value = e^.const_integer_value_info.value;
    if (e^.base_type_or_null == type_long)
      semantic_error ("must be int/uint constant expression", pos);
  }

  return value;
}

/*****************************************************************************/

public
PEXPRESSION parse_constant_expression_with_context (CONTEXT context)
{
  TEXT_POSITION pos;
  PEXPRESSION   e;

  pos = token.pos;

  parse_expression (context, token.pos, null, out e);

  if (!is_constant_exp (e))
  {
    semantic_error ("must be constant expression", pos);
    return dummy_expression();
  }

  return e;
}

/*****************************************************************************/

public
PEXPRESSION parse_constant_expression ()
{
  return parse_constant_expression_with_context (g_no_context);
}

/*****************************************************************************/
