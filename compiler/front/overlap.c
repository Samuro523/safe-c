
// overlap.c : check for overlapping memory zones

use entities, ../error, type;

/********************************************************************************************************/

enum OVERLAP_CATEGORY
{
  OVL_CONST,
  OVL_GLOBAL,
  OVL_LOCAL,
  OVL_PARAM_VALUE,  // simple type of mode IN
  OVL_PARAM_ADDR,   // simple type of mode OUT/REF, or array/struct/union of any mode.
  OVL_ACCESS,
  OVL_UNSAFE,
};

enum OVL_TYP
{
  OVL_NO,        // don't overlap
  OVL_YES,       // overlap
  OVL_IF_ID,     // overlap if entities are identical
  OVL_ERROR,     // should not occur (bad object form)
};

const OVL_TYP OVERLAP_TABLE[1+(int)OVL_UNSAFE][1+(int)OVL_UNSAFE] =   // [object][expression]
//(object)      constant   global     local      par-by-value  par-by-addr   access     unsafe    (expression)
/* cte    */  {{OVL_ERROR, OVL_ERROR, OVL_ERROR, OVL_ERROR,    OVL_ERROR,    OVL_ERROR, OVL_ERROR},
/* global */   {OVL_NO,    OVL_IF_ID, OVL_NO,    OVL_NO,       OVL_YES,      OVL_NO,    OVL_YES},
/* local  */   {OVL_NO,    OVL_NO,    OVL_IF_ID, OVL_NO,       OVL_NO,       OVL_NO,    OVL_YES},
/* par-by-v */ {OVL_ERROR, OVL_ERROR, OVL_ERROR, OVL_ERROR,    OVL_ERROR,    OVL_ERROR, OVL_ERROR},
/* par-by-a */ {OVL_NO,    OVL_YES,   OVL_NO,    OVL_NO,       OVL_YES,      OVL_YES,   OVL_YES},
/* access */   {OVL_NO,    OVL_NO,    OVL_NO,    OVL_NO,       OVL_YES,      OVL_YES,   OVL_YES},
/* unsafe */   {OVL_NO,    OVL_YES,   OVL_YES,   OVL_YES,      OVL_YES,      OVL_YES,   OVL_YES}};

/********************************************************************************************************/

struct OBJECT_OVL_INFO
{
  OVERLAP_CATEGORY typ;
  PENTITY          e;     // global or local variable object, or null
}

/********************************************************************************************************/

void evaluate_object_ovl (PEXPRESSION obj, out OBJECT_OVL_INFO o)
{
  PEXPRESSION prefix;

  clear o;

  switch (obj^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
    case A_CONST_INTEGER_VALUE:
    case A_CONST_FLOAT_VALUE:
    case A_CONST_NULL_VALUE:
    case A_POOL_CONSTANT:
      o.typ = OVL_CONST;
      break;

    case A_GLOBAL_VARIABLE_OBJECT:
      o.typ = OVL_GLOBAL;
      o.e   = obj^.global_variable_object_info.pobject;
      break;

    case A_LOCAL_VARIABLE_OBJECT:
      o.typ = OVL_LOCAL;
      o.e   = obj^.local_variable_object_info.pobject;
      break;

    case A_REFERENCE_OBJECT:
      evaluate_object_ovl (obj^.reference_object_info.pobject^.the_reference.name, out o);
      break;

    case A_PARAMETER_OBJECT:
      {
        PENTITY e, typ;

        e = obj^.parameter_object_info.pobject;
        typ = complete_type_of (e^.the_parameter.type);

        if (e^.the_parameter.mode != MODE_IN ||
            typ^.kind == AN_OPEN_ARRAY_TYPE || typ^.kind == AN_ARRAY_TYPE ||
            typ^.kind == A_STRUCT_TYPE      || typ^.kind == A_CONSTRAINED_STRUCT_TYPE ||
            typ^.kind == A_UNION_TYPE)
        {
          o.typ = OVL_PARAM_ADDR;
        }
        else
        {
          o.typ = OVL_PARAM_VALUE;
        }
      }
      break;

    case AN_ARRAY_ELEMENT_OBJECT:
      prefix = obj^.array_element_object_info.prefix;
      if (prefix^.base_type_or_null == null || prefix^.base_type_or_null^.kind == AN_UNSAFE_POINTER_TYPE)
        o.typ = OVL_UNSAFE;
      else
        evaluate_object_ovl (prefix, out o);
      break;

    case AN_ARRAY_SLICE_OBJECT:
      prefix = obj^.array_slice_object_info.prefix;
      if (prefix^.base_type_or_null == null || prefix^.base_type_or_null^.kind == AN_UNSAFE_POINTER_TYPE)
        o.typ = OVL_UNSAFE;
      else
        evaluate_object_ovl (prefix, out o);
      break;

    case A_STRUCT_FIELD_OBJECT:
      evaluate_object_ovl (obj^.struct_field_object_info.prefix, out o);
      break;

    case A_DEREFERENCED_OBJECT:
      o.typ = OVL_ACCESS;
      break;

    case AN_UNSAFE_DEREFERENCED_OBJECT:
      o.typ = OVL_UNSAFE;
      break;

    case AN_ATTR_BYTE_OBJECT:
      evaluate_object_ovl (obj^.attr_byte_object_info.prefix, out o);
      break;

    case A_BOXED_OBJECT:
      evaluate_object_ovl (obj^.boxed_object_info.parameter, out o);
      break;

    case AN_UNBOXED_OBJECT:
      evaluate_object_ovl (obj^.unboxed_object_info.parameter, out o);
      break;

    default:
      code_generator_error ("evaluate_object_ovl(9)");
      break;
  }
}

/********************************************************************************************************/

bool object_overlaps_with_object_exp (OBJECT_OVL_INFO o, PEXPRESSION exp)
{
  OBJECT_OVL_INFO o2;

  evaluate_object_ovl (exp, out o2);

  switch (OVERLAP_TABLE[(int)o.typ][(int)o2.typ])
  {
    case OVL_NO:
      return false;

    case OVL_YES:
      return true;

    case OVL_IF_ID:
      return o.e == o2.e;

    case OVL_ERROR:
      code_generator_error ("object_overlaps_with_object_exp(8)");
      return true;     // to avoid warning

    default:
      code_generator_error ("object_overlaps_with_object_exp(9)");
      return true;     // to avoid warning
  }
}

/********************************************************************************************************/

bool object_overlaps_with_exp (OBJECT_OVL_INFO o,
                               PEXPRESSION     exp,
                               bool            evaluate_simple_inner_expressions);

/********************************************************************************************************/

bool object_overlaps_with_exp_list (OBJECT_OVL_INFO o, LIST_OF_EXPRESSIONS^ list)
{
  LIST_OF_EXPRESSIONS^ l;

  l = list;

  while (l != null)
  {
    if (object_overlaps_with_exp (o, l^.exp, true))
      return true;

    l = l^.next;
  }

  return false;
}

/********************************************************************************************************/

// evaluate_simple_inner_expressions
// false: test if the object or value of the expression overlaps with o (excluding any indexes or parameters)
// true : test if any part of the expression overlaps with o

bool object_overlaps_with_exp (OBJECT_OVL_INFO o,
                               PEXPRESSION     exp,
                               bool            evaluate_simple_inner_expressions)
{
  if (exp == null)   // denotes earlier compiler error or no expression within an allocator.
    return false;

  switch (exp^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
    case A_CONST_INTEGER_VALUE:
    case A_CONST_FLOAT_VALUE:
    case A_CONST_NULL_VALUE:
    case A_POOL_CONSTANT:
      return false;

    case AN_OPERATOR_VALUE:
      {
        int i;
        if (evaluate_simple_inner_expressions)
        {
          for (i=0; i<3; i++)
          {
            if (object_overlaps_with_exp (o, exp^.operator_value_info.arg[i], evaluate_simple_inner_expressions => true))
              return true;
          }
        }
        else if (exp^.operator_value_info.op == OP_CONDITIONAL_TEST)   // either conditional expression is object/value
        {
          for (i=1; i<=2; i++)
          {
            if (object_overlaps_with_exp (o, exp^.operator_value_info.arg[i], evaluate_simple_inner_expressions => false))
              return true;
          }
        }
      }
      return false;

    case A_RUN_CALL:
      return evaluate_simple_inner_expressions &&
             object_overlaps_with_exp (o, exp^.run_call_info.function_call, evaluate_simple_inner_expressions => true);

    case A_FUNCTION_VALUE:
      return false;

    case A_FUNCTION_CALL:
      return evaluate_simple_inner_expressions &&
             (object_overlaps_with_exp (o, exp^.function_call_info.func, evaluate_simple_inner_expressions => true) ||
              object_overlaps_with_exp_list (o, exp^.function_call_info.param));

    case A_DISCRIMINANT_VALUE:
      return evaluate_simple_inner_expressions &&
             object_overlaps_with_exp (o, exp^.discriminant_value_info.prefix, evaluate_simple_inner_expressions => true);

    case AN_UNC_ARRAY_AGGREGATE:
      return false;  // only 1 value evaluated before the copy

    case AN_AGGREGATE_VALUE:
      return evaluate_simple_inner_expressions &&
             object_overlaps_with_exp_list (o, exp^.aggregate_value_info.list);

    case A_QUALIFIED_EXPRESSION:
      return object_overlaps_with_exp (o, exp^.qualified_expression_info.value, evaluate_simple_inner_expressions);

    case AN_ARRAY_QUALIFIED_EXPRESSION:
      return (evaluate_simple_inner_expressions && object_overlaps_with_exp (o, exp^.array_qualified_expression_info.length, true)) ||
             object_overlaps_with_exp (o, exp^.array_qualified_expression_info.value, evaluate_simple_inner_expressions);

    case A_STRUCT_QUALIFIED_EXPRESSION:
      return (evaluate_simple_inner_expressions && object_overlaps_with_exp (o, exp^.struct_qualified_expression_info.discriminant, true)) ||
             object_overlaps_with_exp (o, exp^.struct_qualified_expression_info.value, evaluate_simple_inner_expressions);

    case AN_ALLOCATOR:
      return evaluate_simple_inner_expressions && object_overlaps_with_exp (o, exp^.allocator_info.value, true);

    case A_GLOBAL_VARIABLE_OBJECT:
    case A_LOCAL_VARIABLE_OBJECT:
    case A_REFERENCE_OBJECT:
    case A_PARAMETER_OBJECT:
      return object_overlaps_with_object_exp (o, exp);

    case AN_ARRAY_ELEMENT_OBJECT:
      return object_overlaps_with_object_exp (o, exp) ||
             object_overlaps_with_exp (o, exp^.array_element_object_info.prefix, evaluate_simple_inner_expressions) ||
             (evaluate_simple_inner_expressions && object_overlaps_with_exp (o, exp^.array_element_object_info.index, true));

    case AN_ARRAY_SLICE_OBJECT:
      return object_overlaps_with_object_exp (o, exp) ||
             object_overlaps_with_exp (o, exp^.array_slice_object_info.prefix, evaluate_simple_inner_expressions) ||
             (evaluate_simple_inner_expressions && object_overlaps_with_exp (o, exp^.array_slice_object_info.index, true)) ||
             (evaluate_simple_inner_expressions && object_overlaps_with_exp (o, exp^.array_slice_object_info.length, true));

    case A_STRUCT_FIELD_OBJECT:
      return object_overlaps_with_exp (o, exp^.struct_field_object_info.prefix, evaluate_simple_inner_expressions);

    case A_DEREFERENCED_OBJECT:
      return object_overlaps_with_object_exp (o, exp) ||
             object_overlaps_with_exp (o, exp^.dereferenced_object_info.prefix, evaluate_simple_inner_expressions);

    case AN_UNSAFE_DEREFERENCED_OBJECT:
      return object_overlaps_with_object_exp (o, exp) ||
             object_overlaps_with_exp (o, exp^.unsafe_dereferenced_object_info.unsafe_ptr_value, evaluate_simple_inner_expressions);

    case AN_ATTR_BYTE_OBJECT:
      return object_overlaps_with_exp (o, exp^.attr_byte_object_info.prefix, evaluate_simple_inner_expressions);

    case A_BOXED_OBJECT:
      return object_overlaps_with_exp (o, exp^.boxed_object_info.parameter, evaluate_simple_inner_expressions);

    case AN_UNBOXED_OBJECT:
      return object_overlaps_with_exp (o, exp^.unboxed_object_info.parameter, evaluate_simple_inner_expressions);

    case A_BOXED_ARRAY_OBJECT:
      return object_overlaps_with_exp_list (o, exp^.boxed_array_object_info.list);

    default:
      code_generator_error ("object_overlaps_with_exp(9)");
      return true;   // to avoid warning
  }
}

/********************************************************************************************************/

// for assignement statement:
// use to test if an array/struct/union assignment can be done in any memory order, or needs an ordered copy.

public
bool object_shares_memory_with_expression (PEXPRESSION obj, PEXPRESSION exp)
{
  OBJECT_OVL_INFO o;

  evaluate_object_ovl (obj, out o);

  return object_overlaps_with_exp (o, exp, evaluate_simple_inner_expressions => false);
}

/********************************************************************************************************/

// for assignement statement:
// used to test if an aggregate can be evaluated "inline", 
// i.e. directly writing to the target object, element by element.

public
bool object_used_in_expression (PEXPRESSION obj, PEXPRESSION exp)
{
  OBJECT_OVL_INFO o;

  evaluate_object_ovl (obj, out o);

  return object_overlaps_with_exp (o, exp, evaluate_simple_inner_expressions => true);
}

/********************************************************************************************************/
