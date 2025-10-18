
// package.c : package declaration and body

from std use bintree, strings;
use tokens, lex, entities, decl, ../error, ../common, type, typedecl, complete;

//=================================================================================

struct TRANSLATE_ENTRY
{
  long    nr;
  PENTITY e;
}

package P = new BALANCED_BINARY_TREE (ELEMENT => TRANSLATE_ENTRY, USER_INFO => bool);

struct INSTANTIATION_DATA     // data for a generic instantiation
{
  PENTITY             egen_package_decl;
  PENTITY             epackage_decl;
  ENTITY_LIST^        list;   // list of generic package declations surrounding this instantiation.
                              // Rule: if instantiation of A occurs within B's declaration or body, 
                              //       and instantiation of B occurs within A's declaration or body,
                              //       then this is disallowed (no circularity)
  BINARY_TREE         tree;
  INSTANTIATION_DATA^ next;
}

INSTANTIATION_DATA^  g_instantiation_data_first, g_instantiation_data_last;

//=================================================================================

//  generic_association ::=  identifier  "=>"  type_definition
//                        |  identifier  "=>"  function_name

int parse_generic_association (PENTITY epar)
{
  TEXT_POSITION pos;
  PENTITY       t, f;

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("generic parameter identifier expected", token.pos);
    return -1;
  }

  if (wstrcmp (token.info._identifier.value, epar^.identifier_or_null^) != 0)
  {
    syntax_error ("bad identifier", token.pos);
    return -1;
  }

  get_token();  // skip identifier

  if (token.kind != ARROW)
  {
    syntax_error ("'=>' expected", token.pos);
    return -1;
  }

  get_token();  // skip arrow

  pos = token.pos;

  if (epar^.kind == A_GENERIC_TYPE)
  {
    t = complete_type_of (parse_type_definition (token.pos, null));

    if (is_jagged_type (t))
      semantic_error ("a jagged type is not allowed", pos);
    else if (is_open_type (t))
      semantic_error ("an open type is not allowed", pos);
    else if (t^.kind == AN_INCOMPLETE_TYPE)
      semantic_error ("an incomplete type is not allowed", pos);
    else if (t^.kind == AN_OPAQUE_TYPE)
      semantic_error ("an opaque type is not allowed", pos);
    else if (is_limited_type (t))
      semantic_error ("a limited type is not allowed", pos);

    epar^.the_generic_type.actual_type = t;
  }
  else   // a generic function
  {
    f = parse_expanded_name ();
    if (f == null || (f^.kind != A_GENERIC_FUNCTION && f^.kind != A_FUNCTION_DECLARATION))
      semantic_error ("a function name is expected here", pos);
    else
    {
      if (f^.kind == A_GENERIC_FUNCTION)
      {
        t = f^.the_generic_function.to_type;
      }
      else
      {
        t = f^.the_function_declaration.to_type;
      }

      if (!types_are_equal (t, epar^.the_generic_function.to_type))
        semantic_error ("function profiles do not match", pos);
    }

    epar^.the_generic_function.actual_func = f;
  }

  return 0;
}

//=================================================================================

void unlink_all_generic_parameters (PENTITY epackage)
{
  PENTITY e;

  e = epackage^.the_package_declaration.generic_part^.entities.first;

  while (e != null)
  {
    if (e^.kind == A_GENERIC_TYPE)
      e^.the_generic_type.actual_type = null;

    if (e^.kind == A_GENERIC_FUNCTION)
      e^.the_generic_function.actual_func = null;

    e = e^.next;
  }
}

//=================================================================================

void skip_to_next_id (ref PENTITY pe)
{
  while (pe != null && pe^.identifier_or_null == null)
    pe = pe^.next;
}

//=================================================================================

int compare (bool^           user,
             TRANSLATE_ENTRY data1,
             TRANSLATE_ENTRY data2)
{
  _unused user;
  if (data1.nr < data2.nr)
    return -1;
  if (data1.nr > data2.nr)
    return +1;
  return 0;
}

//=================================================================================

void store_translation (ref INSTANTIATION_DATA p, long nr, PENTITY e2)
{
  TRANSLATE_ENTRY rec;
  int             rc;

  clear rec;
  rec.nr = nr;
  rec.e  = e2;

  rc = insert_btree (ref p.tree, rec);
  if (rc != 0)
    fatal_compiler_error ("store_trans()", token.pos);
}

//=================================================================================

void translate_instance_entity (INSTANTIATION_DATA p, ref PENTITY e)
{
  TRANSLATE_ENTRY rec;
  int             rc;

  if (e == null)    // null entity
    return;

  clear rec;
  rec.nr = e^.nr;

  rc = retrieve_btree (p.tree, ref rec, BT_EQUAL);
  if (rc == 0)
  {
    e = rec.e;
  }
  else if (rc == BT_KEY_NOT_FOUND)
    ;
  else
    fatal_compiler_error ("trans_inst_ent()", token.pos);
}

//=================================================================================

void instantiate_entity (ref INSTANTIATION_DATA p, PENTITY e0, PREGION pregion);
void translate_instance_expression (INSTANTIATION_DATA p, ref PEXPRESSION exp);

//=================================================================================

void translate_instance_region (ref INSTANTIATION_DATA p, ref PREGION rr)
{
  PENTITY      e = rr^.entities.first;
  PREGION      r;
  ENTITY_LIST^ list;

  r = new_region ();

  while (e != null)
  {
    instantiate_entity (ref p, e, r);
    e = e^.next;
  }


  // translate list of visible packages

  r^.packages = duplicate_entity_list (rr^.packages);
  list = r^.packages;
  while (list != null)
  {
    translate_instance_entity (p, ref list^.e);
    list = list^.next;
  }


  rr = r;
}

//=================================================================================

void translate_flow_alternatives (ref INSTANTIATION_DATA p, 
                                  ref FLOW_ALTERNATIVE^  first_alt,
                                  ref FLOW_ALTERNATIVE^  last_alt)
{
  FLOW_ALTERNATIVE^ a, b, n, l;

  a = first_alt;
  b = null;
  l = null;

  while (a != null)
  {
    n = new FLOW_ALTERNATIVE ' (a^);
    n^.cte_list = duplicate_case_constant_list (a^.cte_list);
    translate_instance_region (ref p, ref n^.inner);
    n^.next = null;

    if (l == null)
      b = n;
    else
      l^.next = n;
    l = n;

    a = a^.next;
  }

  first_alt = b;
  last_alt  = l;
}

//=================================================================================

void translate_instance_list_of_expression (INSTANTIATION_DATA p, ref LIST_OF_EXPRESSIONS^ list)
{
  LIST_OF_EXPRESSIONS^ l;

  l = duplicate_list_of_expressions (list);

  list = l;

  while (l != null)
  {
    translate_instance_expression (p, ref l^.exp);
    translate_instance_entity     (p, ref l^.type);
    translate_instance_entity     (p, ref l^.e);
    l = l^.next;
  }
}

//=================================================================================

void translate_instance_expression (INSTANTIATION_DATA p, ref PEXPRESSION exp)
{
  PEXPRESSION e0, e;

  if (exp == null)
    return;

  e0 = exp;
  e = new EXPRESSION ' (e0^);

  translate_instance_entity (p, ref e^.base_type_or_null);

  // when instantiating an expression of a generic type and it becomes an expression of array or struct type,
  // we need to fill the expression's constraint field and recompute the base type.
  if (complete_type_of (e0^.base_type_or_null)^.kind == A_GENERIC_TYPE)
  {
    PENTITY  t = complete_type_of (e^.base_type_or_null);
    uint4    value;

    if (t^.kind == AN_ARRAY_TYPE || t^.kind == A_CONSTRAINED_STRUCT_TYPE)
    {
      if (t^.kind == AN_ARRAY_TYPE)
      {
        value = t^.the_array_type.length;
      }
      else
      {
        value = t^.the_constrained_struct_type.discriminant_value;
      }

      e^.constraint.kind = CONSTANT_CONSTRAINT;
      e^.constraint.value = value;
      e^.base_type_or_null = base_type_of (t);
    }
  }

  switch (e^.kind)
  {
    case A_CONST_ENUMERATION_VALUE:
    case A_CONST_INTEGER_VALUE:
    case A_CONST_FLOAT_VALUE:
    case A_CONST_NULL_VALUE:
    case A_POOL_CONSTANT:
      break;

    case AN_OPERATOR_VALUE:
      translate_instance_expression (p, ref e^.operator_value_info.arg[0]);
      translate_instance_expression (p, ref e^.operator_value_info.arg[1]);
      translate_instance_expression (p, ref e^.operator_value_info.arg[2]);
      break;

    case A_RUN_CALL:
      translate_instance_expression (p, ref e^.run_call_info.function_call);
      break;

    case A_FUNCTION_VALUE:
      translate_instance_entity     (p, ref e^.function_value_info.to_function_declaration_or_generic_function);
      break;

    case A_FUNCTION_CALL:
      translate_instance_expression (p, ref e^.function_call_info.func);
      translate_instance_list_of_expression (p, ref e^.function_call_info.param);
      break;

    case A_DISCRIMINANT_VALUE:
      translate_instance_expression (p, ref e^.discriminant_value_info.prefix);
      translate_instance_entity     (p, ref e^.discriminant_value_info.field);
      break;

    case AN_UNC_ARRAY_AGGREGATE:
      translate_instance_expression (p, ref e^.unc_array_aggregate_info.element);
      break;

    case AN_AGGREGATE_VALUE:
      translate_instance_list_of_expression (p, ref e^.aggregate_value_info.list);
      break;

    case A_QUALIFIED_EXPRESSION:
      translate_instance_expression (p, ref e^.qualified_expression_info.value);
      break;

    case AN_ARRAY_QUALIFIED_EXPRESSION:
      translate_instance_expression (p, ref e^.array_qualified_expression_info.length);
      translate_instance_expression (p, ref e^.array_qualified_expression_info.value);
      break;

    case A_STRUCT_QUALIFIED_EXPRESSION:
      translate_instance_expression (p, ref e^.struct_qualified_expression_info.discriminant);
      translate_instance_expression (p, ref e^.struct_qualified_expression_info.value);
      break;

    case AN_ALLOCATOR:
      translate_instance_expression (p, ref e^.allocator_info.value);
      break;

    case A_GLOBAL_VARIABLE_OBJECT:
      translate_instance_entity     (p, ref e^.global_variable_object_info.pobject);
      break;

    case A_LOCAL_VARIABLE_OBJECT:
      translate_instance_entity     (p, ref e^.local_variable_object_info.pobject);
      break;

    case A_REFERENCE_OBJECT:
      translate_instance_entity     (p, ref e^.reference_object_info.pobject);
      break;

    case A_PARAMETER_OBJECT:
      translate_instance_entity     (p, ref e^.parameter_object_info.pobject);
      break;

    case AN_ARRAY_ELEMENT_OBJECT:
      translate_instance_expression (p, ref e^.array_element_object_info.prefix);
      translate_instance_expression (p, ref e^.array_element_object_info.index);
      break;

    case AN_ARRAY_SLICE_OBJECT:
      translate_instance_expression (p, ref e^.array_slice_object_info.prefix);
      translate_instance_expression (p, ref e^.array_slice_object_info.index);
      translate_instance_expression (p, ref e^.array_slice_object_info.length);
      break;

    case A_STRUCT_FIELD_OBJECT:
      translate_instance_expression (p, ref e^.struct_field_object_info.prefix);
      translate_instance_entity     (p, ref e^.struct_field_object_info.field);
      break;

    case A_DEREFERENCED_OBJECT:
      translate_instance_expression (p, ref e^.dereferenced_object_info.prefix);
      break;

    case AN_UNSAFE_DEREFERENCED_OBJECT:
      translate_instance_expression (p, ref e^.unsafe_dereferenced_object_info.unsafe_ptr_value);
      break;

    case AN_ATTR_BYTE_OBJECT:
      translate_instance_expression (p, ref e^.attr_byte_object_info.prefix);
      break;

    case A_BOXED_OBJECT:
      translate_instance_expression (p, ref e^.boxed_object_info.parameter);
      break;

    case AN_UNBOXED_OBJECT:
      translate_instance_expression (p, ref e^.unboxed_object_info.parameter);
      break;

    case A_BOXED_ARRAY_OBJECT:
      translate_instance_list_of_expression (p, ref e^.boxed_array_object_info.list);
      break;
  
    default:
      abort;
  }

  exp = e;
}

//=================================================================================

void instantiate_entity (ref INSTANTIATION_DATA p, PENTITY e0, PREGION pregion)
{
  PENTITY e2;

  e2 = append_new_entity_for_instantiation (e0,
                                            token.pos,
                                            pregion);

  store_translation (ref p, e0^.nr, e2);


  // translate inner field : entity links in generic package range, regions, expressions

  switch (e0^.kind)
  {
    case AN_INTEGER_TYPE:
    case A_FLOAT_TYPE:
    case AN_ENUMERATION_TYPE:
      break;

    case AN_OPEN_ARRAY_TYPE:
      translate_instance_entity (    p, ref e2^.the_open_array_type.element);
      break;

    case AN_ARRAY_TYPE:
      translate_instance_entity (    p, ref e2^.the_array_type.open_array);
      break;

    case A_STRUCT_TYPE:
      translate_instance_region (ref p, ref e2^.the_struct_type.discriminant);
      translate_instance_region (ref p, ref e2^.the_struct_type.fields);
      break;

    case A_CONSTRAINED_STRUCT_TYPE:
      translate_instance_entity (    p, ref e2^.the_constrained_struct_type.open_struct);
      break;

    case A_UNION_TYPE:
      translate_instance_region (ref p, ref e2^.the_union_type.fields);
      break;

    case A_POINTER_TYPE:
      translate_instance_entity (    p, ref e2^.the_pointer_type.designated_type);
      break;

    case A_FUNCTION_POINTER_TYPE:
      if (e2^.the_function_pointer_type.extern_dll_or_null != null)
      {
        e2^.the_function_pointer_type.extern_dll_or_null = 
              new wstring ' (e0^.the_function_pointer_type.extern_dll_or_null^);
      }

      translate_instance_entity (    p, ref e2^.the_function_pointer_type.return_type);
      translate_instance_region (ref p, ref e2^.the_function_pointer_type.parameters);
      break;

    case AN_UNSAFE_POINTER_TYPE:
      translate_instance_entity (    p, ref e2^.the_unsafe_pointer_type.designated_type);
      break;

    case A_RENAMED_TYPE:
      translate_instance_entity (    p, ref e2^.the_renamed_type.actual_type);
      break;

    case AN_INCOMPLETE_TYPE:
      e2^.the_incomplete_type.full_type = null;
      break;

    case AN_OPAQUE_TYPE:
      e2^.the_opaque_type.full_type = null;
      break;

    case A_GENERIC_TYPE:
      translate_instance_entity (    p, ref e2^.the_generic_type.actual_type);
      break;

    case A_NULL_POINTER_TYPE:
    case A_VOID_TYPE:
      break;

    case AN_INCOMPLETE_TYPE_COMPLETITION:
      translate_instance_entity (    p, ref e2^.the_incomplete_type_completition.incomplete_type);
      translate_instance_entity (    p, ref e2^.the_incomplete_type_completition.full_type);
      e2^.the_incomplete_type_completition.incomplete_type
        ^.the_incomplete_type.full_type =
              e2^.the_incomplete_type_completition.full_type;
      break;

    case AN_OPAQUE_TYPE_COMPLETITION:
      translate_instance_entity (    p, ref e2^.the_opaque_type_completition.opaque_type);
      translate_instance_entity (    p, ref e2^.the_opaque_type_completition.full_type);
      e2^.the_opaque_type_completition.opaque_type
        ^.the_opaque_type.full_type =
              e2^.the_opaque_type_completition.full_type;
      break;

    case A_FIELD:
      translate_instance_entity (    p, ref e2^.the_field.type);
      break;

    case A_VARYING_FIELD:
      translate_instance_entity (    p, ref e2^.the_varying_field.type);
      break;

    case AN_ENUMERATION_LITERAL:
      translate_instance_entity (    p, ref e2^.the_enumeration_literal.type);
      break;

    case A_CONSTANT:
      translate_instance_entity     (    p, ref e2^.the_constant.type);
      translate_instance_expression (    p, ref e2^.the_constant.value);
      break;

    case A_GLOBAL_VARIABLE:
      translate_instance_entity     (    p, ref e2^.the_global_variable.type);
      translate_instance_expression (    p, ref e2^.the_global_variable.initial_value_or_null);
      break;

    case A_LOCAL_VARIABLE:
      translate_instance_entity     (    p, ref e2^.the_local_variable.type);
      translate_instance_expression (    p, ref e2^.the_local_variable.initial_value_or_null);
      break;

    case A_REFERENCE:
      translate_instance_entity     (    p, ref e2^.the_reference.type);
      translate_instance_expression (    p, ref e2^.the_reference.name);
      break;

    case A_PARAMETER:
      translate_instance_entity     (    p, ref e2^.the_parameter.type);
      translate_instance_expression (    p, ref e2^.the_parameter.default_value_or_null);
      break;

    case A_GENERIC_FUNCTION:
      translate_instance_entity (    p, ref e2^.the_generic_function.to_type);
      translate_instance_entity (    p, ref e2^.the_generic_function.actual_func);
      break;

    case A_FUNCTION_DECLARATION:
      translate_instance_entity (    p, ref e2^.the_function_declaration.to_type);
      e2^.the_function_declaration.to_function_body_or_null = null;
      break;

    case A_FUNCTION_BODY:
      translate_instance_entity (    p, ref e2^.the_function_body.to_function_declaration);
      translate_instance_region (ref p, ref e2^.the_function_body.inner);
      e2^.the_function_body.to_function_declaration
        ^.the_function_declaration.to_function_body_or_null = e2;
      break;

    case A_PACKAGE_DECLARATION:
      e2^.the_package_declaration.to_package_body_or_null = null;
      translate_instance_region (ref p, ref e2^.the_package_declaration.generic_part);
      translate_instance_region (ref p, ref e2^.the_package_declaration.declarations);
      break;

    case A_PACKAGE_BODY:
      translate_instance_entity (    p, ref e2^.the_package_body.to_package_declaration);
      translate_instance_region (ref p, ref e2^.the_package_body.declarations);
      e2^.the_package_body.to_package_declaration
        ^.the_package_declaration.to_package_body_or_null = e2;
      break;

    case A_CLEAR_STATEMENT:
      translate_instance_expression (p    , ref e2^.the_clear_statement.name);
      break;

    case AN_ASSIGNMENT_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_assignment_statement.name);
      translate_instance_expression (    p, ref e2^.the_assignment_statement.value);
      break;

    case A_PRE_OR_POSTFIX_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_pre_or_postfix_statement.name);
      break;

    case A_FUNCTION_CALL_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_function_call_statement.name);
      break;

    case A_RETURN_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_return_statement.value);
      translate_instance_entity     (    p, ref e2^.the_return_statement.type);
      translate_instance_entity     (    p, ref e2^.the_return_statement.outer);
      break;

    case A_BREAK_STATEMENT:
      translate_instance_entity     (    p, ref e2^.the_break_statement.outer);
      break;

    case A_CONTINUE_STATEMENT:
      translate_instance_entity     (    p, ref e2^.the_continue_statement.outer);
      break;

    case A_FREE_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_free_statement.value);
      break;

    case AN_ABORT_STATEMENT:
      break;

    case AN_ASSERT_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_assert_statement.value);
      break;

    case A_SLEEP_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_sleep_statement.value);
      break;

    case A_CODE_STATEMENT:
      break;

    case AN_UNUSED_STATEMENT:
      translate_instance_entity     (    p, ref e2^.the_unused_statement.item);
      break;

    case A_BLOCK_STATEMENT:
      translate_instance_region     (ref p, ref e2^.the_block_statement.inner);
      translate_instance_entity     (    p, ref e2^.the_block_statement.outer);
      break;

    case AN_IF_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_if_statement.condition);
      translate_instance_region     (ref p, ref e2^.the_if_statement.true_branch);
      translate_instance_region     (ref p, ref e2^.the_if_statement.false_branch);
      translate_instance_entity     (    p, ref e2^.the_if_statement.outer);
      break;

    case A_SWITCH_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_switch_statement.value);
      translate_flow_alternatives   (ref p, ref e2^.the_switch_statement.first_alt,
                                            ref e2^.the_switch_statement.last_alt);
      translate_instance_entity     (    p, ref e2^.the_switch_statement.outer);
      break;

    case A_WHILE_STATEMENT:
      translate_instance_expression (    p, ref e2^.the_while_statement.condition);
      translate_instance_region     (ref p, ref e2^.the_while_statement.inner);
      translate_instance_entity     (    p, ref e2^.the_while_statement.outer);
      break;

    case A_FOR_STATEMENT:
      translate_instance_region     (ref p, ref e2^.the_for_statement.pre);
      translate_instance_region     (ref p, ref e2^.the_for_statement.exp_region);
      translate_instance_expression (    p, ref e2^.the_for_statement.condition);
      translate_instance_region     (ref p, ref e2^.the_for_statement.post);
      translate_instance_region     (ref p, ref e2^.the_for_statement.inner);
      translate_instance_entity     (    p, ref e2^.the_for_statement.outer);
      break;

    default:
      fatal_compiler_error ("inst.e", token.pos);
      break;
  }
}

//=================================================================================

void free_entity_list (ENTITY_LIST^ list)
{
  ENTITY_LIST^ p, t;

  p = list;
  while (p != null)
  {
    t = p;
    p = p^.next;
    free t;
  }
}

//=================================================================================

void free_inst_node (INSTANTIATION_DATA^ p)
{
  free_entity_list (p^.list);
  close_btree (ref p^.tree);
  free p;
}

//=================================================================================

//             gen-unit  pkg-body-to-fill  surrounding-gen-package-bodies
// -> 1 add_body_inst (gen SWAP, place *1, tree, (SORT*A), (EXTRACT*B))
// -> 2 add_body_inst (gen SWAP, place *2, tree, (SORT*A))
// -> 3 add_body_inst (gen SORT, place *3, tree, *)
// ----> pick a line(3) to instantiate, but no other line must have this entity its
// surrounding-gen-package-bodies(1) !  otherwise pick this line(1) first;
// if nb of tries exceeds nb of table lines -> circular dependancies !!

public
void instantiate_all_pending_package_bodies ()
{
  INSTANTIATION_DATA^ p, l, q;
  ENTITY_LIST^        el;

  // instantiate all remaining bodies

  while (g_instantiation_data_first != null)
  {
    l = null;
    p = g_instantiation_data_first;

    // find an instantiation where no other line has it as its dependancies

    while (p != null)         // loop on all p instantiations
    {
      q = g_instantiation_data_first;
      while (q != null)          // loop on all q instantiations
      {
        // check if any dependances equals p
        el = q^.list;
        while (el != null)
        {
          if (el^.e == p^.egen_package_decl)
            break;    // dependance found
          el = el^.next;
        }

        if (el != null)   // dependancy found
          break;

        q = q^.next;
      }

      if (q == null)   // not dependancy found for p
        break;

// printf ("dependancy found between %S and %S\n",  p^.egen_package_decl^.identifier_or_null, q^.egen_package_decl^.identifier_or_null);

      // p has dependancies : try next p
      l = p;
      p = p^.next;
    }

    if (p != null)  // found line p
    {
      PENTITY egbody, ebody;

      // instantiate package body of entry p

// printf ("instantiate %S\n", p^.egen_package_decl^.identifier_or_null);

      egbody = p^.egen_package_decl^.the_package_declaration.to_package_body_or_null;
      
      if (egbody != null)
      {
        ebody = p^.epackage_decl^.the_package_declaration.to_package_body_or_null;

        free (ebody^.the_package_body.declarations);  // free old dummy region
        ebody^.the_package_body.declarations =
              egbody^.the_package_body.declarations;  // set copy of generic body

        translate_instance_region (ref p^, ref ebody^.the_package_body.declarations);
      }


      // remove p from list and free it

      if (l == null)   // first item
      {
        g_instantiation_data_first = p^.next;
        if (g_instantiation_data_first == null)
          g_instantiation_data_last = null;
      }
      else
      {
        if (g_instantiation_data_last == p)
          g_instantiation_data_last = l;
        l^.next = p^.next;
      }

      free_inst_node (p);
    }
    else
    {
      char buffer[240];
      sprintf (out buffer, "circular references instantiating generic package body %S",
                           g_instantiation_data_first^.egen_package_decl^.identifier_or_null^);
      semantic_error (buffer, token.pos);
      return;
    }
  }
}

//=================================================================================

//  package_instantiation ::=
//     "package"  identifier  "="  "new"  generic_package_name
//    ["(" generic_association {"," generic_association} ")"]
//     ";"

void parse_package_instantiation ()
{
  TEXT_POSITION new_pos;
  wchar         new_package_id[MAX_IDENTIFIER_LENGTH];
  PENTITY       e_gpackage, epar;

  // store new id
  new_pos        = token.pos;
  new_package_id = token.info._identifier.value;

  get_token();  // skip id
  get_token();  // skip =

  if (token.kind != TOKEN_new)
  {
    syntax_error ("keyword 'new' expected", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }

  get_token();  // skip 'new'


  // parse a generic package name

  e_gpackage = parse_expanded_name ();
  if (e_gpackage == null ||
      e_gpackage^.kind != A_PACKAGE_DECLARATION ||
      !e_gpackage^.the_package_declaration.is_generic)
  {
    syntax_error ("generic package name expected", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }


  // check that we're NOT within the generic package or its body

  if (are_we_in_package (e_gpackage))   // we're inside the generic package or its body
  {
    syntax_error ("a generic instantiation is not allowed within the package itself", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }

  epar = e_gpackage^.the_package_declaration.generic_part^.entities.first;
  skip_to_next_id (ref epar);

  if (token.kind == LEFT_PARENTHESIS)
  {
    get_token();  // skip '('

    for (;;)
    {
      if (epar == null)
      {
        semantic_error ("too many generic parameters", token.pos);
        skip_until (SEMICOLON, true);
        unlink_all_generic_parameters (e_gpackage);
        return;
      }

      if (parse_generic_association (epar) < 0)
      {
        skip_until (SEMICOLON, true);
        unlink_all_generic_parameters (e_gpackage);
        return;
      }

      if (epar != null)
      {
        epar = epar^.next;
        skip_to_next_id (ref epar);
      }

      if (token.kind == RIGHT_PARENTHESIS)
        break;

      if (token.kind != COMMA)
      {
        syntax_error ("',' or ')' expected", token.pos);
        skip_until (SEMICOLON, true);
        unlink_all_generic_parameters (e_gpackage);
        return;
      }

      get_token();   // skip comma
    }
    
    get_token();  // skip ')'
  }


  // check that all generic entities have received an association

  if (epar != null)
  {
    syntax_error ("missing generic parameters", token.pos);
    skip_until (SEMICOLON, true);
    unlink_all_generic_parameters (e_gpackage);
    return;
  }


  parse_semicolon ();


  {
    ENTITY (A_PACKAGE_DECLARATION) pdecl;
    ENTITY (A_PACKAGE_BODY)        pbody;
    PENTITY                        edecl, e, ebody;
    INSTANTIATION_DATA^            p;

    clear pdecl;
    pdecl.the_package_declaration.to_package_body_or_null = null;
    pdecl.the_package_declaration.is_generic              = false;
    pdecl.the_package_declaration.is_body_required        = false;
    pdecl.the_package_declaration.generic_part            = e_gpackage^.the_package_declaration.generic_part;
    pdecl.the_package_declaration.declarations            = e_gpackage^.the_package_declaration.declarations;
    edecl = append_new_entity (pdecl, new_package_id, new_pos);


    // compute instantiation data

    p = new INSTANTIATION_DATA;
    p^.egen_package_decl = e_gpackage;
    p^.epackage_decl     = edecl;
    p^.list              = build_list_of_surrounding_generic_packages ();
    create_btree (out p^.tree, null, compare);
//    p^.next = null;
    
    if (g_instantiation_data_last == null)  // empty
      g_instantiation_data_first = p;
    else
      g_instantiation_data_last^.next = p;
    g_instantiation_data_last = p;

    // translate inner regions

    store_translation (ref p^, e_gpackage^.nr, edecl);

    translate_instance_region (ref p^, ref edecl^.the_package_declaration.generic_part);
    translate_instance_region (ref p^, ref edecl^.the_package_declaration.declarations);


    // insert package declaration in package list of surrounding scope

    create_region_level ();
    append_package_declaration_to_surrounding_scope_package_list (edecl);
    unlink_region_level ();


    // make opaque types limited

    e = edecl^.the_package_declaration.declarations^.entities.first;
    while (e != null)
    {
      if (e^.kind == AN_OPAQUE_TYPE)
      {
        e^.the_opaque_type.is_limited = true;
      }

      e = e^.next;
    }




    // body

    clear pbody;
    pbody.the_package_body.to_package_declaration = edecl;
    pbody.the_package_body.declarations = new_region();
    ebody = append_new_entity (pbody, L"", token.pos);

    edecl^.the_package_declaration.to_package_body_or_null = ebody;
  }


  unlink_all_generic_parameters (e_gpackage);
}

//=================================================================================

void parse_end_package (PENTITY edecl)
{
  if (token.kind == TOKEN_end)
  {
    get_token();

    if (token.kind == IDENTIFIER)
    {
      if (wstrcmp (token.info._identifier.value, edecl^.identifier_or_null^) != 0)
        syntax_error ("identifier does not match", token.pos);

      get_token();

      parse_semicolon ();
    }
    else
    {
      syntax_error ("identifier is expected here", token.pos);
      skip_until (SEMICOLON, true);
    }
  }
  else
  {
    syntax_error ("keyword 'end' is expected here", token.pos);
    skip_until (SEMICOLON, true);
  }
}

//=================================================================================

void parse_package_declaration ()
{
  ENTITY (A_PACKAGE_DECLARATION) decl;
  PENTITY                        edecl, e;

  clear decl;
  decl.the_package_declaration.to_package_body_or_null = null;
  decl.the_package_declaration.is_generic              = false;
  decl.the_package_declaration.is_body_required        = false;
  decl.the_package_declaration.generic_part            = new_region ();
  decl.the_package_declaration.declarations            = new_region ();

  edecl = append_new_entity (decl, token.info._identifier.value, token.pos);

  create_region_level ();
  append_region (decl.the_package_declaration.generic_part);
  append_region (decl.the_package_declaration.declarations);

  get_token();

  parse_global_declarations (/* is_body => */ false);


  // insert package declaration in package list of surrounding scope

  append_package_declaration_to_surrounding_scope_package_list (edecl);



  // make opaque types limited

  e = edecl^.the_package_declaration.declarations^.entities.first;
  while (e != null)
  {
    if (e^.kind == AN_OPAQUE_TYPE)
    {
      e^.the_opaque_type.is_limited = true;
    }

    e = e^.next;
  }


  // tests if a package body is required

  edecl^.the_package_declaration.is_body_required =
       is_body_required (edecl^.the_package_declaration.declarations^.entities.first);


  unlink_region_level ();


  parse_end_package (edecl);
}

//=================================================================================

void parse_package_body (TEXT_POSITION pos0)
{
  PENTITY                 edecl, e;
  ENTITY (A_PACKAGE_BODY) pbody;
  PENTITY                 ebody;


  edecl = search_entity_in_local_scope (token.info._identifier.value);

  if (edecl == null ||  // no earlier package declaration of this name
      edecl^.kind != A_PACKAGE_DECLARATION)
  {
    syntax_error ("a package declaration must occur before the body", pos0);
    return;
  }

  clear pbody;
  pbody.the_package_body.to_package_declaration = edecl;
  pbody.the_package_body.declarations = new_region ();

  ebody = append_new_entity (pbody, L"", token.pos);

  edecl^.the_package_declaration.to_package_body_or_null = ebody;

  create_region_level ();
  append_region (edecl^.the_package_declaration.generic_part);
  append_region (edecl^.the_package_declaration.declarations);
  append_region (pbody.the_package_body.declarations);

  get_token();   // skip identifier


  // when entering region (package body. or unit body),
  // scan package decl. or unit interface :
  // - for opaque type :
  //   . set non-limited

  e = edecl^.the_package_declaration.declarations^.entities.first;
  while (e != null)
  {
    if (e^.kind == AN_OPAQUE_TYPE)
    {
      e^.the_opaque_type.is_limited = false;
    }

    e = e^.next;
  }



  parse_global_declarations (/* is_body => */ true);



  check_global_completion (edecl^.the_package_declaration.declarations^.entities.first);
  check_global_completion (ebody^.the_package_body.declarations^.entities.first);



  // when leaving region (package body. or unit body),
  // scan package decl. or unit interface :
  // - for incomplete type :
  //   . if full type's keynr is past the package body nr,
  //     it means they are in different parts ! -> full type no longer available.
  // - for opaque type :
  //   . full type no longer available.

  e = edecl^.the_package_declaration.declarations^.entities.first;
  while (e != null)
  {
    if (e^.kind == AN_INCOMPLETE_TYPE &&
        e^.the_incomplete_type.full_type != null &&
        e^.the_incomplete_type.full_type^.nr > ebody^.nr)
    {
      e^.the_incomplete_type.is_full_type_visible = false;
    }

    if (e^.kind == AN_OPAQUE_TYPE)
    {
      e^.the_opaque_type.is_full_type_visible = false;
      e^.the_opaque_type.is_limited = true;
    }

    e = e^.next;
  }


  unlink_region_level ();


  parse_end_package (edecl);
}

//=================================================================================

public
void parse_package_declaration_or_package_instantiation_or_package_body
                (    bool body_allowed,
                 ref bool body_was_parsed)
{
  TEXT_POSITION pos0;

  pos0 = token.pos;

  get_token();   // skip 'package'

  if (token.kind == IDENTIFIER)
  {
    // check if look-ahead token is "=" in case it's a package instantiation

    get_look_ahead_token ();
    if (look_ahead_token.kind == ASSIGN)
    {
      parse_package_instantiation ();
    }
    else
    {
      parse_package_declaration ();
    }
  }
  else if (token.kind == TOKEN_body)
  {
    if (!body_allowed)
    {
      syntax_error ("a package body is not allowed here", pos0);
      return;
    }

    get_token();   // skip 'body'

    parse_package_body (pos0);

    body_was_parsed = true;
  }
  else
  {
    syntax_error ("an identifier or 'body' is expected here", token.pos);
  }
}

//=================================================================================

//  ["<" identifier  {","  identifier}  ">"]

void parse_generic_type_declarations ()
{
  ENTITY (A_GENERIC_TYPE) gt;

  if (token.kind != SMALLER)
    return;

  get_token();   // skip <

  for (;;)
  {
    if (token.kind != IDENTIFIER)
    {
      syntax_error ("identifier expected", token.pos);
      skip_until (LARGER, true);
      return;
    }

    // create a generic type

    clear gt;
    (void) append_new_entity (gt, token.info._identifier.value, token.pos);


    get_token();   // skip identifier

    if (token.kind == LARGER)
      break;

    if (token.kind != COMMA)
    {
      syntax_error ("',' or '>' expected", token.pos);
      skip_until (LARGER, true);
      return;
    }

    get_token();   // skip comma
  }

  get_token();   // skip >
}

//=================================================================================

void parse_generic_function_specifications ()
{
  bool dummy = false;

  while (token.kind != TOKEN_package && token.kind != LAST_TOKEN)
  {
    parse_function_specification (                        token.pos,
                                                          null,
                                      body_allowed    =>  false,
                                  ref body_was_parsed => dummy,
                                      in_typedef      =>  false,
                                      in_generic_part =>  true);
  }
}

//=================================================================================

//  generic_package_declaration ::= generic_formal_part
//                                  package_declaration
//
//  generic_formal_part ::=
//     "generic"  ["<" identifier  {","  identifier}  ">"]
//     {  function_specification ";"  }

public
void parse_generic_package_declaration ()
{
  ENTITY (A_PACKAGE_DECLARATION) decl;
  PENTITY                        edecl, e;


  get_token();   // skip 'generic'


  clear decl;
  decl.the_package_declaration.to_package_body_or_null = null;
  decl.the_package_declaration.is_generic              = true;
  decl.the_package_declaration.is_body_required        = false;
  decl.the_package_declaration.generic_part            = new_region ();
  decl.the_package_declaration.declarations            = new_region ();

  edecl = append_new_entity (decl, L"", token.pos);


  create_region_level ();

  append_region (decl.the_package_declaration.generic_part);

  parse_generic_type_declarations ();
  parse_generic_function_specifications ();

  unlink_region_level ();



  if (token.kind != TOKEN_package)
  {
    syntax_error ("'package' expected", token.pos);
    return;
  }

  get_token();   // skip 'package'

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("package identifier expected", token.pos);
    return;
  }

  patch_entity_by_setting_an_identifier (edecl, token.info._identifier.value, token.pos);

  get_token();  // skip identifier


  create_region_level ();

  append_region (decl.the_package_declaration.generic_part);
  append_region (decl.the_package_declaration.declarations);

  parse_global_declarations (/* is_body => */ false);


  // make opaque types limited

  e = edecl^.the_package_declaration.declarations^.entities.first;
  while (e != null)
  {
    if (e^.kind == AN_OPAQUE_TYPE)
    {
      e^.the_opaque_type.is_limited = true;
    }

    e = e^.next;
  }


  // tests if a package body is required

  edecl^.the_package_declaration.is_body_required =
       is_body_required (edecl^.the_package_declaration.declarations^.entities.first);


  unlink_region_level ();

  parse_end_package (edecl);
}

//=================================================================================
