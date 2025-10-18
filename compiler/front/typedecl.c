
// typedecl.c : type declarations

from std use bintree, strings;
use ../pool, ../common, ../goptions, ../error, ../fixup, ../codgen;
use tokens, lex, entities, decl, statem, type, exp, complete, undefined;

//=================================================================================

public
void parse_semicolon ()
{
  if (token.kind == SEMICOLON)
    get_token();
  else
  {
    syntax_error ("';' expected", token.pos);
    skip_until (SEMICOLON, true);
  }
}

//=================================================================================

public
void check_unsafe_region ()
{
  if (!lexa.within_unsafe)
    semantic_error ("unsafe pointers are only allowed within unsafe regions", token.pos);
}

//=================================================================================

// The expanded name of a type_name must denote a type.
// returns null in case of error.

public
PENTITY parse_type_name ()
{
  PENTITY       p;
  TEXT_POSITION pos;

  p = type_entity_of_token (token.kind);
  if (p != null)
  {
    get_token();    // skip type token
    return p;
  }

  if (token.kind == IDENTIFIER)
  {
    pos = token.pos;

    p = parse_expanded_name ();
    if (p == null)    // could not find entity (error already given)
      return null;

    if (p^.kind > LAST_ENTITY_DENOTING_A_TYPE)
    {
      syntax_error ("identifier must denote a type_name", pos);
      return null;
    }

    if (is_unsafe_type (p))
      check_unsafe_region ();

    return p;
  }

  syntax_error ("type_name expected", token.pos);
  return null;
}

//=================================================================================

// checks that the type is not incomplete, otherwise set it to type_int.

void check_not_incomplete_type (ref PENTITY t, TEXT_POSITION pos)
{
  if (complete_type_of(t)^.kind == AN_INCOMPLETE_TYPE)
  {
    syntax_error ("an incomplete type is not allowed here", pos);
    t = type_int;
  }
}

//=================================================================================

//  type_definition ::= type_name { type_specifier }
//
//  declarator ::=  identifier  { type_specifier }
//
//  type_specifier ::= array_specification
//                   | open_array_specification
//                   | array_constraint
//                   | discriminant_constraint
//                   | pointer_specification
//                   | unsafe_pointer_specification
//
//  array_specification          ::= "[" constant_expression "]"
//  open_array_specification     ::= "[" "]"
//  array_constraint             ::= "(" constant_expression ")"
//  discriminant_constraint      ::= "(" constant_expression ")"
//  pointer_specification        ::= "^"
//  unsafe_pointer_specification ::= "*"

// t0 can denote an already parsed expanded name or null

public PENTITY parse_type_definition (TEXT_POSITION pos0, PENTITY t0)
{
  PENTITY t;

  t = t0;

  if (t == null)
    t = parse_type_name ();

  if (t == null)  // earlier error
    t = type_int;

  for (;;)
  {
    if (token.kind == LEFT_BRACKET)
    {
      check_not_incomplete_type (ref t, pos0);

      get_token();    // skip [

      if (token.kind == RIGHT_BRACKET)   // open array
      {
        ENTITY (AN_OPEN_ARRAY_TYPE) open_array;

        clear open_array;
        open_array.the_open_array_type.element = t;

        t = append_new_entity (open_array, L"", token.pos);

        get_token();    // skip ]
      }
      else
      {
        ENTITY (AN_OPEN_ARRAY_TYPE) open_array;
        ENTITY (AN_ARRAY_TYPE)      array;
        TEXT_POSITION               pos;
        int8                        value;

        clear open_array;
        open_array.the_open_array_type.element = t;

        t = append_new_entity (open_array, L"", token.pos);

        pos = token.pos;

        value = parse_constant_int_or_uint_expression ();

        if (value < 0 || value > 2147483647)
        {
          semantic_error ("expression is out of range", pos);
          value = 1;
        }

        clear array;
        array.the_array_type.open_array = t;
        array.the_array_type.length = (uint4)value;

        t = append_new_entity (array, L"", token.pos);

        if (token.kind == RIGHT_BRACKET)
        {
          get_token();    // skip ]
        }
        else
        {
          syntax_error ("']' expected here", token.pos);
          skip_until (IDENTIFIER, false);
          break;
        }
      }
    }
    else if (complete_type_of(t)^.kind == AN_OPEN_ARRAY_TYPE && token.kind == LEFT_PARENTHESIS)
    {
      ENTITY (AN_ARRAY_TYPE) array;
      TEXT_POSITION          pos;
      int8                   value;

      get_token();    // skip (

      pos = token.pos;

      value = parse_constant_int_or_uint_expression ();

      if (value < 0 || value > 2147483647)
      {
        semantic_error ("expression is out of range", pos);
        value = 1;
      }

      clear array;
      array.the_array_type.open_array = t;
      array.the_array_type.length = (uint4)value;

      t = append_new_entity (array, L"", token.pos);

      if (token.kind == RIGHT_PARENTHESIS)
      {
        get_token();    // skip )
      }
      else
      {
        syntax_error ("')' expected here", token.pos);
        skip_until (IDENTIFIER, false);
        break;
      }
    }
    else if (complete_type_of(t)^.kind == A_STRUCT_TYPE &&
             complete_type_of(t)^.the_struct_type.is_open_type &&
             token.kind == LEFT_PARENTHESIS)
    {
      ENTITY (A_CONSTRAINED_STRUCT_TYPE) cons;
      PENTITY                            dis, enumer;
      TEXT_POSITION                      pos;
      PEXPRESSION                        exp;
      int8                               value;

      get_token();    // skip (

      pos = token.pos;

      exp = parse_constant_expression ();


      // check if expression's type and range are valid

      value = 0;   // default

      dis = complete_type_of(t)^.the_struct_type.discriminant^.entities.first;
      if (dis != null && dis^.kind == A_FIELD && exp^.base_type_or_null != null)
      {
        enumer = dis^.the_field.type;
        if (enumer^.kind == AN_ENUMERATION_TYPE)
        {
          if (!types_are_equal (exp^.base_type_or_null, enumer))
          {
            semantic_error ("expression must have discriminant's type", pos);
          }
          else if (exp^.kind != A_CONST_ENUMERATION_VALUE)
          {
            semantic_error ("expression must be constant", pos);
          }
          else
          {
            value = exp^.const_enumeration_value_info.value;
            if (value < 0 || value > enumer^.the_enumeration_type.last)
            {
              semantic_error ("expression is out of range", pos);
              value = 0;
            }
          }
        }
      }

      clear cons;
      cons.the_constrained_struct_type.open_struct = t;
      cons.the_constrained_struct_type.discriminant_value = (uint4)value;

      t = append_new_entity (cons, L"", token.pos);


      if (token.kind == RIGHT_PARENTHESIS)
      {
        get_token();    // skip )
      }
      else
      {
        syntax_error ("')' expected here", token.pos);
        skip_until (IDENTIFIER, false);
        break;
      }
    }
    else if (token.kind == CARET)
    {
      ENTITY (A_POINTER_TYPE) ptr;

      if (is_jagged_type (t))
        semantic_error ("^ not allowed for a jagged type", token.pos);

      clear ptr;
      ptr.the_pointer_type.designated_type = t;

      t = append_new_entity (ptr, L"", token.pos);

      get_token();    // skip ^
    }
    else if (token.kind == STAR)
    {
      ENTITY (AN_UNSAFE_POINTER_TYPE) ptr;

      if (is_jagged_type (t))
        semantic_error ("* not allowed for a jagged type", token.pos);

      clear ptr;
      ptr.the_unsafe_pointer_type.designated_type = t;

      t = append_new_entity (ptr, L"", token.pos);

      check_unsafe_region ();

      get_token();    // skip *
    }
    else
    {
      break;
    }
  }

  return t;
}

//=================================================================================

PENTITY inner_parse_declarator_suffix (PENTITY       t0,
                                       TEXT_POSITION pos0,
                                       bool          incomplete_type_allowed)
{
  PENTITY t;

  t = t0;

  if (token.kind == LEFT_BRACKET)
  {
    get_token();    // skip [

    if (token.kind == RIGHT_BRACKET)   // open array
    {
      ENTITY (AN_OPEN_ARRAY_TYPE) open_array;

      get_token();    // skip ]

      clear open_array;
      open_array.the_open_array_type.element = inner_parse_declarator_suffix (t, pos0, false);

      return append_new_entity (open_array, L"", pos0);
    }
    else
    {
      ENTITY (AN_OPEN_ARRAY_TYPE) open_array;
      ENTITY (AN_ARRAY_TYPE)      array;
      TEXT_POSITION               pos;
      int8                        value;
      PENTITY                     t2;

      pos = token.pos;

      value = parse_constant_int_or_uint_expression ();

      if (value < 0 || value > 2147483647)
      {
        semantic_error ("expression is out of range", pos);
        value = 1;
      }

      if (token.kind == RIGHT_BRACKET)
      {
        get_token();    // skip ]
      }
      else
      {
        syntax_error ("']' expected here", token.pos);
        skip_until (IDENTIFIER, false);
      }

      clear open_array;
      open_array.the_open_array_type.element = inner_parse_declarator_suffix (t, pos0, false);

      t2 = append_new_entity (open_array, L"", pos0);

      clear array;
      array.the_array_type.open_array = t2;
      array.the_array_type.length = (uint4)value;

      return append_new_entity (array, L"", pos0);
    }
  }
  else if (token.kind == CARET)
  {
    ENTITY (A_POINTER_TYPE) ptr;
    TEXT_POSITION           pos;

    pos = token.pos;
    get_token();    // skip ^

    clear ptr;
    ptr.the_pointer_type.designated_type = inner_parse_declarator_suffix (t, pos0, true);

    if (is_jagged_type (ptr.the_pointer_type.designated_type))
      semantic_error ("^ not allowed for a jagged type", pos);

    return append_new_entity (ptr, L"", pos);
  }
  else if (token.kind == STAR)
  {
    ENTITY (AN_UNSAFE_POINTER_TYPE) ptr;
    TEXT_POSITION                   pos;

    pos = token.pos;
    get_token();    // skip *

    check_unsafe_region ();

    clear ptr;
    ptr.the_unsafe_pointer_type.designated_type = inner_parse_declarator_suffix (t, pos0, true);

    if (is_jagged_type (ptr.the_unsafe_pointer_type.designated_type))
      semantic_error ("* not allowed for a jagged type", pos);

    return append_new_entity (ptr, L"", pos);
  }
  else if (token.kind == LEFT_PARENTHESIS)
  {
    TEXT_POSITION   pos;
    PEXPRESSION     exp;
    PENTITY         suffix;

    get_token();    // skip (

    pos = token.pos;

    exp = parse_constant_expression ();

    if (token.kind == RIGHT_PARENTHESIS)
    {
      get_token();    // skip )
    }
    else
    {
      syntax_error ("')' expected here", token.pos);
      skip_until (IDENTIFIER, false);
    }

    suffix = inner_parse_declarator_suffix (t, pos0, false);

    if (complete_type_of(suffix)^.kind == AN_OPEN_ARRAY_TYPE)
    {
      ENTITY (AN_ARRAY_TYPE) array;
      int8                   value;


      // check that expression has an int or uint type

      if (exp^.kind != A_CONST_INTEGER_VALUE)
      {
        semantic_error ("a constant integer is expected here", pos);
        value = 1;
      }
      else
      {
        value = exp^.const_integer_value_info.value;

        if (value < 0 || value > 2147483647)
        {
          semantic_error ("expression is out of range", pos);
          value = 1;
        }
        else if (exp^.base_type_or_null == type_long)
        {
          semantic_error ("a constant of type int or uint is expected here", pos);
        }
      }

      clear array;
      array.the_array_type.open_array = suffix;
      array.the_array_type.length = (uint4)value;

      return append_new_entity (array, L"", pos);
    }
    else if (complete_type_of(suffix)^.kind == A_STRUCT_TYPE &&
             complete_type_of(suffix)^.the_struct_type.is_open_type)
    {
      ENTITY (A_CONSTRAINED_STRUCT_TYPE) cons;
      PENTITY                            dis, enumer;
      int8                               value;


      // check if expression's type and range are valid

      value = 0;   // default

      dis = complete_type_of(suffix)^.the_struct_type.discriminant^.entities.first;
      if (dis != null && dis^.kind == A_FIELD && exp^.base_type_or_null != null)
      {
        enumer = dis^.the_field.type;
        if (enumer^.kind == AN_ENUMERATION_TYPE)
        {
          if (exp^.kind != A_CONST_ENUMERATION_VALUE ||
              !types_are_equal (exp^.base_type_or_null, enumer))
          {
            semantic_error ("expression must have discriminant's type", pos);
          }
          else
          {
            value = exp^.const_enumeration_value_info.value;
            if (value < 0 || value > enumer^.the_enumeration_type.last)
            {
              semantic_error ("expression is out of range", pos);
              value = 0;
            }
          }
        }
      }

      clear cons;
      cons.the_constrained_struct_type.open_struct = suffix;
      cons.the_constrained_struct_type.discriminant_value = (uint4)value;

      return append_new_entity (cons, L"", pos);
    }
    else
    {
      semantic_error ("'(' is only allowed for an open type", pos);
      return t;
    }
  }
  else
  {
    if (!incomplete_type_allowed)
    {
      check_not_incomplete_type (ref t, pos0);
    }

    return t;
  }
}

//=================================================================================

PENTITY parse_declarator_suffix (PENTITY t, TEXT_POSITION pos0)
{
  return inner_parse_declarator_suffix (t, pos0, false);
}

//=================================================================================

PENTITY get_incomplete_type (wstring s)
{
  PENTITY incomplete;

  incomplete = search_entity_in_local_scope (s);

  if (incomplete != null &&
      incomplete^.kind == AN_INCOMPLETE_TYPE &&
      incomplete^.the_incomplete_type.full_type == null)
  {
    return incomplete;
  }

  return null;
}

//=================================================================================

PENTITY get_opaque_type (wstring s)
{
  PENTITY opaque;

  opaque = search_entity_in_local_scope (s);

  if (opaque != null &&
      opaque^.kind == AN_OPAQUE_TYPE &&
      opaque^.the_opaque_type.full_type == null)
  {
    return opaque;
  }

  return null;
}

//=================================================================================

void fill_incomplete_type (PENTITY incomplete, PENTITY full_type, TEXT_POSITION pos)
{
  ENTITY (AN_INCOMPLETE_TYPE_COMPLETITION) com;

  if (incomplete != null)
  {
    if (is_jagged_type (full_type))
      semantic_error ("a jagged type is not allowed as the full type of an earlier incomplete type", pos);
    incomplete^.the_incomplete_type.full_type = full_type;
    incomplete^.the_incomplete_type.is_full_type_visible = true;

    clear com;
    com.the_incomplete_type_completition.incomplete_type = incomplete;
    com.the_incomplete_type_completition.full_type       = full_type;
    (void)append_new_entity (com, L"", token.pos);
  }
}

//=================================================================================

void fill_opaque_type (PENTITY opaque, PENTITY full_type, TEXT_POSITION pos)
{
  ENTITY (AN_OPAQUE_TYPE_COMPLETITION) com;
  ENTITY_KIND                          surrounding_kind;

  if (opaque != null)
  {
    surrounding_kind = surrounding_entity()^.kind;
    if (surrounding_kind != A_UNIT_BODY && surrounding_kind != A_PACKAGE_BODY)
      semantic_error ("the full type of an opaque type declaration is only allowed within an interface body or package body", pos);

    opaque^.the_opaque_type.full_type = full_type;
    opaque^.the_opaque_type.is_full_type_visible = true;

    clear com;
    com.the_opaque_type_completition.opaque_type = opaque;
    com.the_opaque_type_completition.full_type   = full_type;
    (void)append_new_entity (com, L"", token.pos);
  }
}

//=================================================================================

//  enumeration_declaration ::=
//        "enum" identifier   [ "("  base_type_name  ")" ]
//        "{"  enumeration_literal {"," enumeration_literal}
//        [","]  "}" ";"
//
//  base_type_name ::= type_name
//  enumeration_literal ::= identifier

public void parse_enumeration_declaration ()
{
  PENTITY         type_enum, base, incomplete;
  TEXT_POSITION   pos;
  int8            count;

  get_token();    // skip 'enum'

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected here", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }


  // check if it's the full type of an earlier incomplete type

  incomplete = get_incomplete_type (token.info._identifier.value);


  {
    ENTITY (AN_ENUMERATION_TYPE) enumer;

    clear enumer;
    enumer.the_enumeration_type.base = a_uint4;
    enumer.the_enumeration_type.last = 0;

    type_enum = append_new_entity (enumer,
                                   incomplete != null ? L"" : token.info._identifier.value,
                                   token.pos);

    fill_incomplete_type (incomplete, type_enum, token.pos);
  }


  get_token();      // skip identifier

  if (token.kind == LEFT_PARENTHESIS)   // base type specified
  {
    get_token();      // skip '('

    pos = token.pos;

    base = complete_type_of (parse_type_name());
    if (base != null)         // no earlier error
    {
      if (base^.kind != AN_INTEGER_TYPE ||
          (base^.the_integer_type.type != a_uint1 &&
           base^.the_integer_type.type != a_uint2 &&
           base^.the_integer_type.type != a_uint4))
      {
        semantic_error ("type_name must be uint1, uint2 or uint4", pos);
      }
      else
      {
        type_enum^.the_enumeration_type.base = base^.the_integer_type.type;
      }
    }

    if (token.kind != RIGHT_PARENTHESIS)
    {
      syntax_error ("')' expected here", token.pos);
      skip_until (LEFT_ACCOLADE, false);
    }

    get_token();      // skip ')'
  }

  if (token.kind != LEFT_ACCOLADE)
  {
    syntax_error ("'{' expected here", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }
  get_token();      // skip {

  count = 0;

  for (;;)
  {
    if (token.kind != IDENTIFIER)
    {
      syntax_error ("identifier expected here", token.pos);
      skip_until (SEMICOLON, true);
      return;
    }


    // create the enumeration literal entity

    {
      ENTITY (AN_ENUMERATION_LITERAL) lit;

      clear lit;
      lit.the_enumeration_literal.type = type_enum;
      lit.the_enumeration_literal.value = (uint4)count;
      (void)append_new_entity (lit, token.info._identifier.value, token.pos);
    }


    type_enum^.the_enumeration_type.last = (uint4)count;
    count++;

    get_token();      // skip identifier

    if (token.kind == RIGHT_ACCOLADE)
      break;

    if (token.kind != COMMA)
    {
      syntax_error ("'}' or ',' expected here", token.pos);
      skip_until (SEMICOLON, true);
      return;
    }
    get_token();      // skip comma

    if (token.kind == RIGHT_ACCOLADE)
      break;
  }

  if (type_enum^.the_enumeration_type.last
        > INTEGER_DATA[(uint)type_enum^.the_enumeration_type.base].max)
  {
    semantic_error ("too many enumeration literals", token.pos);
  }

  get_token();      // skip }
  parse_semicolon ();


  create_enumeration_literal_string_table (type_enum);
}

//=================================================================================

void parse_typedef_declaration_of_type_definition (PENTITY t0, TEXT_POSITION typ_pos)
{
  wchar                   id[MAX_IDENTIFIER_LENGTH+1];
  TEXT_POSITION           pos;
  PENTITY                 t, incomplete, full_type;
  ENTITY (A_RENAMED_TYPE) ren;

  wstrcpy (out id, token.info._identifier.value);


  // check if it's the full type of an earlier incomplete type

  incomplete = get_incomplete_type (token.info._identifier.value);


  pos = token.pos;

  get_token();      // skip identifier

  t = parse_declarator_suffix (t0, typ_pos);

  clear ren;
  ren.the_renamed_type.actual_type = t;

  full_type = append_new_entity (ren,
                                 incomplete != null ? L"" : id,
                                 pos);

  fill_incomplete_type (incomplete, full_type, pos);

  parse_semicolon ();
}

//=================================================================================

//  formal_parameters ::= [formal_parameter {"," formal_parameter}]
//  formal_parameter ::= [mode] type_definition declarator
//                                       ["=" constant_expression]
//  mode ::=  "ref" | "out"
//
//  If no mode is specified, the mode "in" is implicitly chosen.
//
//  Parameterless functions have an empty set of parentheizes.
//
//  Parameters can have any type except type incomplete, except within typedef declaration.

void parse_parentheized_formal_parameters (bool inside_typedef_declaration,
                                           bool is_callback_or_entry_or_extern_dll)
{
  MODE                 mode;
  wchar                id[MAX_IDENTIFIER_LENGTH+1];
  TEXT_POSITION        pos, typ_pos, exp_pos;
  PENTITY              t, typ, par;
  ENTITY (A_PARAMETER) param;
  PEXPRESSION          exp;
  CONTEXT              context;

  if (token.kind != LEFT_PARENTHESIS)
  {
    syntax_error ("'(' expected here", token.pos);
    return;
  }

  get_token();      // skip '('


  if (token.kind != RIGHT_PARENTHESIS)
  {
    for (;;)
    {
      if (token.kind == TOKEN_ref)
      {
        mode = MODE_REF;
        get_token();
      }
      else if (token.kind == TOKEN_out)
      {
        mode = MODE_OUT;
        get_token();
      }
      else
      {
        mode = MODE_IN;
      }

      typ_pos = token.pos;
      t = parse_type_definition (token.pos, null);

      if (token.kind != IDENTIFIER)
      {
        syntax_error ("identifier expected", token.pos);
        skip_until (RIGHT_PARENTHESIS, true);
        return;
      }

      wstrcpy (out id, token.info._identifier.value);
      pos = token.pos;

      get_token();      // skip identifier

      typ = inner_parse_declarator_suffix (t, typ_pos, incomplete_type_allowed => inside_typedef_declaration);


      if (mode != MODE_IN)  // forbid jagged types except object[]
      {
        if (is_jagged_type (typ) && !types_are_equal (typ, type_array_of_object))
          semantic_error ("a jagged type is not allowed for mode out/ref", typ_pos);

        // example: int sscanf (string buffer, string format, out object[] arg);
      }

      if (is_callback_or_entry_or_extern_dll)   // forbid open array/structs
      {
        if (is_open_type (typ))
          semantic_error ("open types are not allowed for callback/entry/dll calls", typ_pos);
      }


      clear param;
      param.the_parameter.type = typ;
      param.the_parameter.mode = mode;
      param.the_parameter.default_value_or_null = null;
      param.the_parameter.is_used    = false;
      param.the_parameter.init_error = false;
      param.the_parameter.pos        = pos;

      par = create_new_entity (param, id, pos);

      hide_entity (par);

      if (token.kind == ASSIGN)
      {
        if (mode != MODE_IN)
          semantic_error ("a default expression is only allowed for parameters of mode in", token.pos);
        else if (is_limited_type (typ))
          semantic_error ("a default expression is not allowed for parameters of a limited type", token.pos);

        get_token();      // skip '='

        exp_pos = token.pos;

        convert_type_into_context (typ, out context);

        exp = parse_constant_expression_with_context (context);

        check_assignment_context_compatibility (context, exp, exp_pos);

        par^.the_parameter.default_value_or_null = exp;
      }

      append_entity_in_entity_queue (par);  // append late to avoid forward entity references


      if (token.kind != COMMA)
        break;
      get_token();      // skip comma
    }
  }


  if (token.kind != RIGHT_PARENTHESIS)
  {
    syntax_error ("')' expected here", token.pos);
    return;
  }

  get_token();      // skip ')'
}

//=================================================================================

// assertion: both entities denote function pointer types

// Two functions are compatible if they have the same return type, function
// options ("callback", "extern"), number of parameters, and, for each
// parameter : same mode, identical type.
// The keywords "public" and "inline", the function name and parameter
// names or any default expressions need not match.

bool are_compatible_function_types (PENTITY func_type1, PENTITY func_type2)
{
  ref A_FUNCTION_POINTER_TYPE_ENTITY fa = func_type1^.the_function_pointer_type;
  ref A_FUNCTION_POINTER_TYPE_ENTITY fb = func_type2^.the_function_pointer_type;
  PENTITY e1, e2;

  // return type

  if (!types_are_equal (fa.return_type, fb.return_type))
    return false;

  if (fa.is_callback != fb.is_callback)
    return false;

  
  // entry, syscall and dll are NEVER compatible with anything else

  if (fa.is_entry || fb.is_entry)
    return false;

  if (fa.is_syscall || fb.is_syscall)
    return false;

  if ((fa.extern_dll_or_null != null) || (fb.extern_dll_or_null != null))
    return false;



  e1 = fa.parameters^.entities.first;
  e2 = fb.parameters^.entities.first;

  for (;;)
  {
    // skip anonymous declarations
    while (e1 != null && e1^.identifier_or_null == null)
      e1 = e1^.next;

    while (e2 != null && e2^.identifier_or_null == null)
      e2 = e2^.next;

    if (e1 == null && e2 == null)   // both null -> done
      break;

    if (e1 == null || e2 == null)  // one null -> incompatible
      return false;

    if (e1^.the_parameter.mode != e2^.the_parameter.mode)
      return false;

    if (!types_are_equal (e1^.the_parameter.type,
                          e2^.the_parameter.type))
      return false;

    e1 = e1^.next;
    e2 = e2^.next;
  }

  return true;
}

//=================================================================================

// check if types from function declaration and body match.
// (they can differ due to opaque types becoming complete or redeclaration of arrays/pointers)

public
bool types_are_equal (PENTITY t1, PENTITY t2)
{
  PENTITY ta, tb;

  ta = t1;
  tb = t2;

  for (;;)
  {
    ta = complete_type_of (ta);
    tb = complete_type_of (tb);

    if (ta^.kind != tb^.kind)
      return false;

    switch (ta^.kind)
    {
      case AN_OPEN_ARRAY_TYPE:
        ta = ta^.the_open_array_type.element;
        tb = tb^.the_open_array_type.element;
        break;

      case AN_ARRAY_TYPE:
        if (ta^.the_array_type.length != tb^.the_array_type.length)
          return false;
        ta = ta^.the_array_type.open_array;
        tb = tb^.the_array_type.open_array;
        break;

      case A_CONSTRAINED_STRUCT_TYPE:
        if (ta^.the_constrained_struct_type.discriminant_value != tb^.the_constrained_struct_type.discriminant_value)
          return false;
        ta = ta^.the_constrained_struct_type.open_struct;
        tb = tb^.the_constrained_struct_type.open_struct;
        break;

      case A_POINTER_TYPE:
        ta = ta^.the_pointer_type.designated_type;
        tb = tb^.the_pointer_type.designated_type;
        break;

      case A_FUNCTION_POINTER_TYPE:
        return are_compatible_function_types (ta, tb);

      case AN_UNSAFE_POINTER_TYPE:
        ta = ta^.the_unsafe_pointer_type.designated_type;
        tb = tb^.the_unsafe_pointer_type.designated_type;
        break;

      default:
        return ta == tb;
    }
  }
}

//=================================================================================

// The function specification of a function declaration and body must match.
// In particular : the function name, the function options ("callback"),
// the return type, the number of parameters, and for each parameter :
// its name, type, default value presence and value if any.
// The keywords "inline" and "public" need not match.

void parse_function_specification_for_body
                   (TEXT_POSITION                  pos0,
//                    PENTITY                        edecl,
                    A_FUNCTION_POINTER_TYPE_ENTITY decl,
                    A_FUNCTION_POINTER_TYPE_ENTITY bdy)   // has no parameter info
{
  MODE             mode;
  wchar            id[MAX_IDENTIFIER_LENGTH+1];
  TEXT_POSITION    pos, typ_pos;
  PENTITY          t, typ, e;
  PEXPRESSION      exp;
  CONTEXT          context;
  TEXT_POSITION    exp_pos;
  bool             match;


  match = true;


  // check if return types match

  if (!types_are_equal (decl.return_type, bdy.return_type))
    match = false;


  e = decl.parameters^.entities.first;

  // skip anonymous declarations in parameter region
  while (e != null && e^.identifier_or_null == null)
    e = e^.next;


  get_token();    // skip identifier


  // check if parameters match

  if (token.kind != LEFT_PARENTHESIS)
  {
    syntax_error ("'(' expected here", token.pos);
    return;
  }

  get_token();      // skip '('


  if (token.kind != RIGHT_PARENTHESIS)
  {
    for (;;)
    {
      if (token.kind == TOKEN_ref)
      {
        mode = MODE_REF;
        get_token();
      }
      else if (token.kind == TOKEN_out)
      {
        mode = MODE_OUT;
        get_token();
      }
      else
      {
        mode = MODE_IN;
      }

      typ_pos = token.pos;
      t = parse_type_definition (token.pos, null);

      if (token.kind != IDENTIFIER)
      {
        syntax_error ("identifier expected", token.pos);
        skip_until (RIGHT_PARENTHESIS, true);
        return;
      }

      wstrcpy (out id, token.info._identifier.value);
      pos = token.pos;

      get_token();      // skip identifier

      typ = parse_declarator_suffix (t, typ_pos);

      exp = null;
      if (token.kind == ASSIGN)
      {
        get_token();      // skip '='
        exp_pos = token.pos;

        convert_type_into_context (typ, out context);

        exp = parse_constant_expression_with_context (context);

        check_assignment_context_compatibility (context, exp, exp_pos);
      }

      if (e == null ||
          e^.kind != A_PARAMETER ||
          wstrcmp (e^.identifier_or_null^, id) != 0 ||
          (!types_are_equal (e^.the_parameter.type, typ)) ||
          e^.the_parameter.mode != mode ||
          !constant_expressions_match (e^.the_parameter.default_value_or_null, exp))
      {
        match = false;
        if (e != null && e^.kind == A_PARAMETER)
        {
          e^.the_parameter.is_used = true;   // avoid another error later
        }
      }
      else   // advance e to next parameter
      {
        // set position of parameter in function body
        e^.the_parameter.pos = pos;

        e = e^.next;

        // skip anonymous declarations in parameter region
        while (e != null && e^.identifier_or_null == null)
         e = e^.next;
      }

      free_exp (exp);

      if (token.kind != COMMA)
        break;
      get_token();      // skip comma
    }
  }

  if (token.kind != RIGHT_PARENTHESIS)
  {
    syntax_error ("')' expected here", token.pos);
    return;
  }

  get_token();      // skip ')'

  if (e != null)     // declaration has more parameters
    match = false;

  if (!match)
    semantic_error ("function return type/parameters do not match with declaration", pos0);
}

//=================================================================================

bool region_contains_break_statement (PREGION r)
{
  PENTITY e;

  e = r^.entities.first;

  while (e != null)
  {
    switch (e^.kind)
    {
      case A_BREAK_STATEMENT:
        return true;

      case A_BLOCK_STATEMENT:
        if (region_contains_break_statement (e^.the_block_statement.inner))
          return true;
        break;

      case AN_IF_STATEMENT:
        if (region_contains_break_statement (e^.the_if_statement.true_branch) ||
            region_contains_break_statement (e^.the_if_statement.false_branch))
          return true;
        break;

      default:
        break;
    }

    e = e^.next;
  }

  return false;
}

//=================================================================================
bool region_contains_stopper (PREGION r);
//=================================================================================

bool is_stopper (PENTITY e)
{
  switch (e^.kind)
  {
    case A_RETURN_STATEMENT:
      return true;

    case AN_ABORT_STATEMENT:
      return true;

    case A_BLOCK_STATEMENT:
      return region_contains_stopper (e^.the_block_statement.inner);

    case AN_IF_STATEMENT:
      return region_contains_stopper (e^.the_if_statement.true_branch)
          && region_contains_stopper (e^.the_if_statement.false_branch);

    case A_SWITCH_STATEMENT:
      {
        FLOW_ALTERNATIVE^ f;
        f = e^.the_switch_statement.first_alt;
        while (f != null)
        {
          if (!region_contains_stopper (f^.inner))
            return false;
          f = f^.next;
        }
      }
      return true;

    case A_WHILE_STATEMENT:
      {
        PEXPRESSION exp = e^.the_while_statement.condition;
        if ((exp == null || (exp^.kind == A_CONST_ENUMERATION_VALUE && exp^.const_enumeration_value_info.value == 1))
            && !region_contains_break_statement (e^.the_while_statement.inner))
        return true;
      }
      return false;

    case A_FOR_STATEMENT:
      {
        PEXPRESSION exp = e^.the_for_statement.condition;
        if ((exp == null || (exp^.kind == A_CONST_ENUMERATION_VALUE && exp^.const_enumeration_value_info.value == 1))
            && !region_contains_break_statement (e^.the_for_statement.inner))
        return true;
      }
      return false;

    default:
      return false;
  }
}

//=================================================================================

bool region_contains_stopper (PREGION r)
{
  PENTITY e;

  e = r^.entities.last;

  if (e == null)    // region contains no entities
    return false;

  if (is_stopper (e))   // return statement is usually last entity
    return true;

  // check all region for any stopper
  e = r^.entities.first;
  while (e != null)
  {
    if (is_stopper (e))
      return true;
    e = e^.next;
  }

  return false;
}

//=================================================================================

// assertion: current token is LEFT_ACCOLADE.

// function_body ::= function_specification
//                    "{"  declarations  statements "}"

void parse_function_body (PENTITY edecl)
{
  ENTITY (A_FUNCTION_BODY) fbody;
  PENTITY                  ebody, out_prefix;
  TEXT_POSITION            out_pos;

  clear fbody;
  fbody.the_function_body.to_function_declaration = edecl;
  fbody.the_function_body.inner = new_region ();
  store_location (out fbody.the_function_body.begin_loc);

  get_token();      // skip '{'

  ebody = append_new_entity (fbody, L"", token.pos);

  edecl^.the_function_declaration.to_function_body_or_null = ebody;

  create_region_level ();
  append_region (edecl^.the_function_declaration.to_type
                      ^.the_function_pointer_type.parameters);
  append_region (fbody.the_function_body.inner);


  // 'out_prefix' is an "out" parameter that can either be null,
  // or denote an already parsed expanded name that is not a type_name.
  parse_local_declarations (out out_pos, out out_prefix);

  check_local_completion (ebody^.the_function_body.inner^.entities.first);

  parse_statements (out_pos, out_prefix);
  lex_set_in_statement (false);


  if (!ebody^.the_function_body.contains_code_statements)
  {
    // the last statement of a function body having a non-void return type
    // must be a return statement.

    if (edecl^.the_function_declaration.to_type^.the_function_pointer_type.return_type != type_void)
    {
      if (!region_contains_stopper (ebody^.the_function_body.inner))
        semantic_error ("a return statement is expected here", token.pos);
    }

    detect_uninitialized_variables
       (edecl^.the_function_declaration.to_type^.the_function_pointer_type.parameters,
        ebody^.the_function_body.inner);
  }

  store_location (out ebody^.the_function_body.end_loc);

  if (token.kind == RIGHT_ACCOLADE)
    get_token();
  else
  {
    syntax_error ("'}' expected", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
  }

  unlink_region_level ();
}

//=================================================================================

// this is for a shared object (android target) without main function :
// we need to register all entry points.

void register_entry_function (PENTITY func, TEXT_POSITION pos)
{
  if (g_entry_function_list == null)
    g_entry_function_list = new PENTITY[1] ' {func};
  else
  {
    PENTITY[]^ old = g_entry_function_list;
    int        len = old^'length;
    g_entry_function_list = new PENTITY[len + 1];
    g_entry_function_list^[0:len] = old^;
    g_entry_function_list^[len] = func;
    free old;
  }

  if (!fixup.export_register (func^.identifier_or_null^))
    semantic_error ("[entry] function is declared twice", pos);
}

//=================================================================================

// for function declaration, body, typedef or generic function parameter.

// when t is non-null, a type definition has been parsed earlier and an identifier follows.
// when t is null, current token is : "[", public, inline or void.

public
void parse_function_specification (    TEXT_POSITION pos0,
                                       PENTITY       t,
                                       bool          body_allowed,
                                   ref bool          body_was_parsed,
                                       bool          in_typedef,
                                       bool          in_generic_part)
{
  ENTITY (A_FUNCTION_POINTER_TYPE) func;
  wchar                            id[MAX_IDENTIFIER_LENGTH+1];
  TEXT_POSITION                    id_pos, public_pos;
  PENTITY                          functype, e, func_base_type, incomplete;
  ENTITY_KIND                      surrounding_kind;

  public_pos = pos0;

  clear func;
//  func.the_function_pointer_type.is_inline           = false;
//  func.the_function_pointer_type.is_callback         = false;
//  func.the_function_pointer_type.is_entry            = false;
//  func.the_function_pointer_type.is_public           = false;
//  func.the_function_pointer_type.extern_dll_or_null  = null;
//  func.the_function_pointer_type.is_syscall     = false;
//  func.the_function_pointer_type.syscall_number = 0;
  func.the_function_pointer_type.return_type         = t;
//  func.the_function_pointer_type.parameters          = null;

  if (t == null)   // current token is : "[", public, inline or void.
  {
    if (token.kind == LEFT_BRACKET)
    {
      get_token();   // skip '['

      if (token.kind == IDENTIFIER && wstrcmp (token.info._identifier.value, L"entry") == 0)
      {
        if (goptions.g_target != ANDROID)
          syntax_error ("[entry] is only allowed for ANDROID target", token.pos);

        func.the_function_pointer_type.is_entry = true;  // exported symbol for android
        get_token();   // skip 'entry'
      }
      else
      {
        if (!lexa.within_unsafe)
          semantic_error ("function options are only allowed within unsafe regions", token.pos);

        if (token.kind == IDENTIFIER && wstrcmp (token.info._identifier.value, L"callback") == 0)
        {
          func.the_function_pointer_type.is_callback = true;   // called by operating system, included in exec only if address is given to it
          get_token();   // skip 'callback'
        }
        else if (token.kind == IDENTIFIER && wstrcmp (token.info._identifier.value, L"extern") == 0)
        {
          get_token();   // skip 'extern'

          if (token.kind != STRING_LITERAL)
          {
            syntax_error ("string literal expected", token.pos);
            skip_until (RIGHT_BRACKET, false);
          }
          else
          {
            func.the_function_pointer_type.extern_dll_or_null =
                  new wstring ' (token.info._string.value[0 : token.info._string.length]);   // imported DLL or symbol

            get_token();   // skip 'extern'
          }
        }
        else if (token.kind == IDENTIFIER && wstrcmp (token.info._identifier.value, L"syscall") == 0)
        {
          if (goptions.g_target != ANDROID)
            syntax_error ("[syscall] is only allowed for ANDROID target", token.pos);

          get_token();   // skip 'syscall'

          if (token.kind != LEFT_PARENTHESIS)
          {
            syntax_error ("'(' expected", token.pos);
            skip_until (RIGHT_BRACKET, false);
          }
          else
          {
            get_token();   // skip '('
            
            if (token.kind != INTEGER_LITERAL)
              syntax_error ("syscall number expected", token.pos);
            else
            {
              func.the_function_pointer_type.is_syscall = true;
              func.the_function_pointer_type.syscall_number = (int)token.info._integer.value;
              get_token();   // skip integer literal
            }

            if (token.kind != RIGHT_PARENTHESIS)
              syntax_error ("')' expected", token.pos);
            else
              get_token();   // skip ')'
          }
        }
        else
        {
          syntax_error ("'entry', 'callback', 'syscall' or 'extern' expected", token.pos);
          skip_until (RIGHT_BRACKET, false);
        }
      }

      if (token.kind == RIGHT_BRACKET)
      {
        get_token ();
      }
      else
      {
        syntax_error ("']' expected", token.pos);
        skip_until (RIGHT_BRACKET, true);
      }
    }

    public_pos = token.pos;
    if (token.kind == TOKEN_public)
    {
      func.the_function_pointer_type.is_public = true;
      get_token ();
    }

    if (token.kind == TOKEN_inline)
    {
      func.the_function_pointer_type.is_inline = true;
      get_token ();
    }

    if (token.kind == TOKEN_void)
    {
      func.the_function_pointer_type.return_type = type_void;
      get_token ();
    }
    else
    {
      func.the_function_pointer_type.return_type = parse_type_definition (token.pos, null);
    }


    // In a typedef declaration, the option "callback" is allowed,
    // the option "extern", "entry" and the keywords "public" and "inline" are not allowed.

    if (in_typedef)
    {
      if (func.the_function_pointer_type.extern_dll_or_null != null ||
          func.the_function_pointer_type.is_entry ||
          func.the_function_pointer_type.is_public ||
          func.the_function_pointer_type.is_syscall ||
          func.the_function_pointer_type.is_inline)
      {
        semantic_error ("option not allowed in a typedef declaration", pos0);
        free (func.the_function_pointer_type.extern_dll_or_null);
        func.the_function_pointer_type.extern_dll_or_null = null;
        func.the_function_pointer_type.is_entry = false;
        func.the_function_pointer_type.is_public = false;
        func.the_function_pointer_type.is_syscall = false;
        func.the_function_pointer_type.is_inline = false;
      }
    }

    // In a generic formal part, the options "callback", "entry", "extern", "syscall"
    // and the keywords "public" and "inline" are not allowed.

    if (in_generic_part)
    {
      if (func.the_function_pointer_type.extern_dll_or_null != null ||
          func.the_function_pointer_type.is_entry ||
          func.the_function_pointer_type.is_public ||
          func.the_function_pointer_type.is_syscall ||
          func.the_function_pointer_type.is_inline ||
          func.the_function_pointer_type.is_callback)
      {
        semantic_error ("option not allowed in generic part", pos0);
        free (func.the_function_pointer_type.extern_dll_or_null);
        func.the_function_pointer_type.extern_dll_or_null = null;
        func.the_function_pointer_type.is_entry = false;
        func.the_function_pointer_type.is_public = false;
        func.the_function_pointer_type.is_syscall = false;
        func.the_function_pointer_type.is_callback = false;
        func.the_function_pointer_type.is_inline = false;
      }
    }
  }


  // A function's return type is defined either by the keyword 'void',
  // or by a type definition which can only denote one of the following types :
  //   integer, enumeration, floating-point, pointer, function pointer, unsafe pointer.
  // It cannot denote an array/struct/union/opaque/limited/generic type.

  func_base_type = base_type_of (func.the_function_pointer_type.return_type);

  if (func_base_type != null)
  {
    if (func_base_type^.kind != AN_INTEGER_TYPE &&
        func_base_type^.kind != A_FLOAT_TYPE &&
        func_base_type^.kind != AN_ENUMERATION_TYPE &&
        func_base_type^.kind != A_POINTER_TYPE &&
        func_base_type^.kind != A_FUNCTION_POINTER_TYPE &&
        func_base_type^.kind != AN_UNSAFE_POINTER_TYPE &&
        func_base_type^.kind != A_VOID_TYPE)
    {
      semantic_error ("function has an illegal return type", pos0);
      func.the_function_pointer_type.return_type = type_int;
    }
  }


  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }
  id_pos = token.pos;


  if ((!in_typedef) && (!in_generic_part) && body_allowed)
  {
    // it could be a function body for an earlier function declaration;
    // search for an entity having the same identifier immediately within this region.

    e = search_entity_in_local_scope (token.info._identifier.value);

    if (e != null && e^.kind == A_FUNCTION_DECLARATION)
    {
      ref A_FUNCTION_POINTER_TYPE_ENTITY ft = e^.the_function_declaration.to_type^.the_function_pointer_type;

      if ((ft.extern_dll_or_null == null) &&    // can have a body if :       1) no dll
          (!ft.is_entry) &&                     //                            2) no entry
          (!ft.is_syscall) &&                   //                            3) no syscall
          (e^.the_function_declaration.to_function_body_or_null == null))  // 4) no body yet
      {
        // it is a function body for an earlier declaration

        if (ft.is_public && !func.the_function_pointer_type.is_public)
          semantic_error ("keyword 'public' is required when the function was declared earlier in a .h unit or package declaration", public_pos);
        if (!ft.is_public && func.the_function_pointer_type.is_public)
          semantic_error ("keyword 'public' is not allowed", public_pos);
        if (ft.is_callback != func.the_function_pointer_type.is_callback)
          semantic_error ("keyword 'callback' does not match with function declaration", pos0);
        
        if (func.the_function_pointer_type.extern_dll_or_null != null)
          semantic_error ("keyword 'extern' is not allowed", pos0);
        if (func.the_function_pointer_type.is_entry)
          semantic_error ("keyword 'entry' is not allowed", pos0);
        if (func.the_function_pointer_type.is_syscall)
          semantic_error ("keyword 'syscall' is not allowed", pos0);

        parse_function_specification_for_body (pos0, ft, func.the_function_pointer_type);

        if (token.kind == LEFT_ACCOLADE)
        {
          if (ft.is_entry)
            register_entry_function (e, id_pos);
          parse_function_body (e);
        }
        else
        {
          syntax_error ("'{' expected", token.pos);
          skip_until (SEMICOLON, true);
        }

        body_was_parsed = true;
        return;
      }
    }
  }


  func.the_function_pointer_type.parameters = new_region ();

  // save identifier for later function declaration
  wstrcpy (out id, token.info._identifier.value);
  id_pos = token.pos;



  // The keyword "public" is allowed only for a function body; it must be
  // specified if the function was declared earlier in a .h unit or in a package
  // declaration.

  if ((!in_typedef) && (!in_generic_part))
  {
    surrounding_kind = surrounding_entity()^.kind;

    // stand-alone function declaration or function body

    if (surrounding_kind == A_UNIT_INTERFACE || surrounding_kind == A_PACKAGE_DECLARATION)
    {
      if (func.the_function_pointer_type.is_public)     // keyword public was specified (but is not allowed)
      {
        if (surrounding_kind == A_PACKAGE_DECLARATION)
          semantic_error ("keyword 'public' is not allowed in a package declaration", public_pos);
        else
          semantic_error ("keyword 'public' is not allowed in a .h unit", public_pos);
      }
      func.the_function_pointer_type.is_public = true;   // set .is_public to true so that keyword 'public' is required for the body.
    }
    else   // we're in a unit body or package body
    {
      if (func.the_function_pointer_type.is_public)
      {
        semantic_error ("keyword 'public' is not allowed", public_pos);
        func.the_function_pointer_type.is_public = false;
      }
    }
  }


  if (in_typedef)
  {
    // check if it's the full type of an earlier incomplete type
    incomplete = get_incomplete_type (id);

    functype = append_new_entity (func,
                                  incomplete != null ? L"" : id,
                                  id_pos);

    fill_incomplete_type (incomplete, functype, id_pos);

    hide_entity (functype);

    if (incomplete != null)
      hide_entity (incomplete);
  }
  else  // anonymous function type entity
  {
    functype = append_new_entity (func, L"", token.pos);
  }

  get_token();    // skip identifier

  create_region_level ();
  append_region (func.the_function_pointer_type.parameters);

  parse_parentheized_formal_parameters
    (inside_typedef_declaration => in_typedef,
     is_callback_or_entry_or_extern_dll  => func.the_function_pointer_type.is_callback
                                         || func.the_function_pointer_type.is_entry
                                         || (func.the_function_pointer_type.extern_dll_or_null != null
                                         || func.the_function_pointer_type.is_entry
                                         || func.the_function_pointer_type.is_syscall));

  unlink_region_level ();

  unhide_all_entities ();


  if (in_generic_part)
  {
    ENTITY (A_GENERIC_FUNCTION) ren;

    clear ren;
    ren.the_generic_function.to_type = functype;

    e = append_new_entity (ren, id, id_pos);
  }
  else if (!in_typedef)      // declare a function declaration with the saved identifier
  {
    ENTITY (A_FUNCTION_DECLARATION) ren;

    clear ren;
    ren.the_function_declaration.to_type                  = functype;
    ren.the_function_declaration.to_function_body_or_null = null;

    e = append_new_entity (ren, id, id_pos);

    if (wstrcmp (id, L"main") == 0)
    {
      PENTITY par;
      int     nb_par;

      if (func.the_function_pointer_type.is_callback)
        semantic_error ("function main() must not be callback", public_pos);

      if (func.the_function_pointer_type.is_entry)
        semantic_error ("function main() must not be entry", public_pos);

      if (func.the_function_pointer_type.extern_dll_or_null != null)
        semantic_error ("function main() must not be extern", public_pos);

      if (func.the_function_pointer_type.return_type != type_void && !types_are_equal (func.the_function_pointer_type.return_type, type_int))
        semantic_error ("function main() must have return type 'void' or 'int'", public_pos);

      par = func.the_function_pointer_type.parameters^.entities.first;
      nb_par = 0;
      while (par != null)
      {
        if (par^.kind == A_PARAMETER)
        {
          nb_par++;
          if (!types_are_equal (par^.the_parameter.type, type_array_of_string))
          {
            nb_par = 10;   // will trigger error below
            break;
          }
        }
        par = par^.next;
      }

      g_function_main_has_parameter_array_of_string = (nb_par > 0);

      if (nb_par > 1)
        semantic_error ("function main() must have either no parameters or one parameter of type 'string[]'", public_pos);

      if (g_func_main != null)
        semantic_error ("only one function main() is allowed in a program", public_pos);

      g_func_main = e;
    }
  }
  else  // in_typedef
  {
    e = null;
  }

  if (in_typedef || in_generic_part || (!body_allowed) ||
      func.the_function_pointer_type.extern_dll_or_null != null ||
      func.the_function_pointer_type.is_syscall ||
      token.kind != LEFT_ACCOLADE)
  {
    parse_semicolon ();
    return;
  }

  if (func.the_function_pointer_type.is_entry)
    register_entry_function (e, id_pos);

  parse_function_body (e);
  body_was_parsed = true;
}

//=================================================================================

void parse_typedef_declaration ()
{
  TEXT_POSITION pos0;
  PENTITY       t;

  pos0 = token.pos;

  if (token.kind == LEFT_BRACKET || token.kind == TOKEN_public ||
      token.kind == TOKEN_inline || token.kind == TOKEN_void)
  {
    bool  dummy = false;
    parse_function_specification (    pos0            => pos0,
                                      t               => null,
                                      body_allowed    => false,
                                  ref body_was_parsed => dummy,
                                      in_typedef      => true,
                                      in_generic_part => false);
    return;
  }

  t = parse_type_definition (token.pos, null);

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }

  get_look_ahead_token ();     // fill 'look_ahead_token'

  if (look_ahead_token.kind == LEFT_PARENTHESIS && !is_open_type (t))
  {
    bool dummy = false;
    parse_function_specification (    pos0            => pos0,
                                      t               => t,
                                      body_allowed    => false,
                                  ref body_was_parsed => dummy,
                                      in_typedef      => true,
                                      in_generic_part => false);
  }
  else
  {
    parse_typedef_declaration_of_type_definition (t, pos0);
  }
}

//=================================================================================

// when t is non-null, a type definition has been parsed earlier and an identifier follows.
// when t is null, current token is : "[", public, inline or void.

public
void parse_function_declaration_or_function_body (    TEXT_POSITION pos0,
                                                      PENTITY       t,
                                                      bool          body_allowed,
                                                  ref bool          body_was_parsed)
{
  parse_function_specification (    pos0            => pos0,
                                    t               => t,
                                    body_allowed    => body_allowed,
                                ref body_was_parsed => body_was_parsed,
                                    in_typedef      => false,
                                    in_generic_part => false);
}

//=================================================================================

void parse_incomplete_declaration ()
{
  ENTITY (AN_INCOMPLETE_TYPE) inc;

  clear inc;

  append_new_entity (inc, token.info._identifier.value, token.pos);

  get_token();    // skip identifier
  get_token();    // skip semicolon
}

//=================================================================================

public
void parse_typedef_or_incomplete_declaration ()
{
  get_token();      // skip 'typedef'

  if (token.kind == IDENTIFIER)
  {
    get_look_ahead_token ();     // fill 'look_ahead_token'

    if (look_ahead_token.kind == SEMICOLON)
    {
      parse_incomplete_declaration ();
      return;
    }
  }

  parse_typedef_declaration ();
}

//=================================================================================

void parse_opaque_type_declaration ()
{
  ENTITY (AN_OPAQUE_TYPE) opa;
  ENTITY_KIND             surrounding_kind;

  clear opa;
  opa.the_opaque_type.full_type = null;
  opa.the_opaque_type.is_full_type_visible = false;
  opa.the_opaque_type.is_limited = false;

  append_new_entity (opa, token.info._identifier.value, token.pos);

  surrounding_kind = surrounding_entity()^.kind;
  if (surrounding_kind != A_UNIT_INTERFACE && surrounding_kind != A_PACKAGE_DECLARATION)
    semantic_error ("an opaque type declaration is only allowed within an interface unit or package declaration", token.pos);

  get_token();    // skip identifier
  get_token();    // skip semicolon
}

//=================================================================================

// used for struct and union types.

//  field_declaration ::=
//    type_definition  declarator  {"," declarator}  ";"

void parse_field_declarations (    PENTITY estru,   // struct or union
                                   bool    is_packed,
                                   bool    in_variant,
                                   uint4   discriminant_value,
                               ref bool    is_unsafe)
{
  for (;;)
  {
    PENTITY                  t, typ;
    wchar                    id[MAX_IDENTIFIER_LENGTH+1];
    TEXT_POSITION            typ_pos, pos;
    ENTITY (A_FIELD)         field;
    ENTITY (A_VARYING_FIELD) vfield;

    if (token.kind == TOKEN_switch || token.kind == TOKEN_case || token.kind == RIGHT_ACCOLADE)
      break;

    typ_pos = token.pos;
    t = parse_type_definition (token.pos, null);
    if (t == null)    // some error (not a type)
    {
      skip_until (RIGHT_ACCOLADE, false);
      break;
    }

    for (;;)
    {
      if (token.kind != IDENTIFIER)
      {
        syntax_error ("identifier expected here", token.pos);
        skip_until (SEMICOLON, true);
        break;
      }

      pos = token.pos;
      wstrcpy (out id, token.info._identifier.value);

      get_token();      // skip identifier

      typ = parse_declarator_suffix (t, typ_pos);

      if (type_has_circular_dependences (estru, typ, in_variant))
      {
        semantic_error ("field type has circular dependencies", typ_pos);
        typ = type_int;
      }
      else if (is_packed || estru^.kind == A_UNION_TYPE)  // must be packed, non-open, non-jagged
      {
        if (!is_packed_type (typ))
        {
          semantic_error ("must be a packed type", typ_pos);
          typ = type_int;
        }
        else if (is_open_type (typ))
        {
          semantic_error ("type must have a constraint", typ_pos);
          typ = type_int;
        }
        else if (is_jagged_type (typ))
        {
          semantic_error ("a jagged type is not allowed", typ_pos);
          typ = type_int;
        }
      }


      if (is_unsafe_type (typ))
        is_unsafe = true;


      if (in_variant)
      {
        clear vfield;
        vfield.the_varying_field.type = typ;
        vfield.the_varying_field.discriminant_value = discriminant_value;
        append_new_entity (vfield, id, pos);
      }
      else
      {
        clear field;
        field.the_field.type = typ;
        append_new_entity (field, id, pos);
      }

      if (token.kind == COMMA)
      {
        get_token();
      }
      else if (token.kind == SEMICOLON)
      {
        get_token();
        break;
      }
      else
      {
        syntax_error ("',' or ';' expected", token.pos);
        skip_until (SEMICOLON, true);
        break;
      }
    }
  }
}

//=================================================================================

package P = new BALANCED_BINARY_TREE (ELEMENT => uint4, USER_INFO => bool);

int sw_compare (bool^ user, uint4 data1, uint4 data2)
{
  _unused user;
  if (data1 < data2)
    return -1;
  if (data1 > data2)
    return +1;
  return 0;
}

//=================================================================================

//  variant_part ::= "switch" "(" identifier ")"
//                   "{" {struct_variant} "}"
//
//  struct_variant ::= "case" constant_expression ":"
//                     variant_declarations
//
//  variant_declarations ::=
//        "null"  ";"
//      | field_declaration { field_declaration }

void parse_variant_part (    PENTITY estru,
                             bool    is_packed,
                             PENTITY discr,
                         ref bool    is_unsafe)
{
  TEXT_POSITION pos;
  PEXPRESSION   exp;
  PENTITY       enumer;
  uint4         discriminant_value;
  BINARY_TREE   btree;
  int           rc;

  get_token();    // skip 'switch'

  if (token.kind != LEFT_PARENTHESIS)
  {
    syntax_error ("'(' expected here", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
    return;
  }

  get_token();   // skip '('

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected here", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
  }

  // check if identifier matches discriminant identifier

  if (wstrcmp (discr^.identifier_or_null^, token.info._identifier.value) != 0)
    semantic_error ("identifier must match with the discriminant name", token.pos);

  get_token();      // skip identifier

  if (token.kind != RIGHT_PARENTHESIS)
  {
    syntax_error ("')' expected here", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
    return;
  }

  get_token();   // skip ')'

  if (token.kind != LEFT_ACCOLADE)
  {
    syntax_error ("'{' expected here", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
    return;
  }

  get_token();   // skip '{'


  create_btree (out btree, null, sw_compare);


  for (;;)
  {
    if (token.kind != TOKEN_case)
      break;

    get_token ();   // skip 'case'

    pos = token.pos;

    exp = parse_constant_expression ();


    // check if expression's type and range are valid

    discriminant_value = 0;   // default

    if (exp^.base_type_or_null != null)
    {
      enumer = complete_type_of (discr^.the_field.type);

      if (!types_are_equal (exp^.base_type_or_null, enumer))
      {
        semantic_error ("expression must have discriminant's type", pos);
      }
      else if (exp^.kind != A_CONST_ENUMERATION_VALUE)
      {
        semantic_error ("expression must be constant", pos);
      }
      else
      {
        discriminant_value = exp^.const_enumeration_value_info.value;
        if (discriminant_value > enumer^.the_enumeration_type.last)
        {
          semantic_error ("expression is out of range", pos);
          discriminant_value = 0;
        }
        else
        {
          // check that discriminant_value is unique among cases

          rc = insert_btree (ref btree, discriminant_value);
          if (rc == 0)
            ;
          else if (rc == BT_DUPLICATE_KEY)
            semantic_error ("case constant given twice", pos);
          else
            fatal_compiler_error ("insert_case_key", pos);
        }
      }
    }

    if (token.kind != COLON)
    {
      syntax_error ("':' expected here", token.pos);
      skip_until (RIGHT_ACCOLADE, true);
      close_btree (ref btree);
      return;
    }

    get_token();   // skip ':'

    if (token.kind == TOKEN_null)
    {
      get_token();         // skip 'null'
      parse_semicolon ();
    }
    else  // field declarations
    {
      if (token.kind == TOKEN_case || token.kind == RIGHT_ACCOLADE)
        syntax_error ("field declaration expected here", token.pos);
      else
        parse_field_declarations (estru, is_packed, true, discriminant_value, ref is_unsafe);
    }
  }


  close_btree (ref btree);


  if (token.kind != RIGHT_ACCOLADE)
  {
    syntax_error ("token 'case' or '}' expected here", token.pos);

    skip_until (RIGHT_ACCOLADE, true);
    return;
  }

  get_token();   // skip '}'
}

//=================================================================================

//  opaque_declaration ::=  "struct"  identifier  ";"
//
//  struct_declaration ::=
//    ["packed"] "struct" identifier [ discriminant_part ]
//    "{"  { field_declaration }  [ variant_part ]  "}"
//
//  discriminant_part ::= "(" type_name identifier ")"
//
//  The type name of a discriminant part must denote an enumeration type.
//  A struct variant must be present if and only if there is a discriminant
//  part, the struct type denotes then an open struct type.

public
void parse_struct_or_opaque_declaration ()
{
  bool                   is_packed;
  TEXT_POSITION          pos0, stru_pos;
  ENTITY (A_STRUCT_TYPE) stru;
  PENTITY                incomplete, opaque, estru, discr = null;

  pos0 = token.pos;

  is_packed = false;
  if (token.kind == TOKEN_packed)
  {
    is_packed = true;
    get_token();
  }

  if (token.kind != TOKEN_struct)
  {
    syntax_error ("'struct' expected", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
    return;
  }

  get_token();

  stru_pos = token.pos;

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
    return;
  }

  get_look_ahead_token ();     // fill 'look_ahead_token'

  if (look_ahead_token.kind == SEMICOLON)   // opaque declaration
  {
    if (is_packed)
      syntax_error ("keyword 'packed' is not allowed for an opaque type", pos0);
    parse_opaque_type_declaration ();
    return;
  }


  // check if it's the full type of an earlier incomplete or opaque type

  incomplete = get_incomplete_type (token.info._identifier.value);
  opaque     = get_opaque_type     (token.info._identifier.value);


  clear stru;
  stru.the_struct_type.is_packed    = is_packed;
  stru.the_struct_type.is_open_type = (look_ahead_token.kind == LEFT_PARENTHESIS);
  stru.the_struct_type.discriminant = new_region ();
  stru.the_struct_type.fields       = new_region ();

  estru = append_new_entity
      (stru,
       (incomplete != null || opaque != null) ? L"" : token.info._identifier.value,
       token.pos);


  fill_incomplete_type (incomplete, estru, token.pos);
  fill_opaque_type     (opaque,     estru, token.pos);


  global_locked_struct_entity = estru;


  get_token();    // skip identifier

  create_region_level ();

  append_region (stru.the_struct_type.discriminant);

  if (token.kind == LEFT_PARENTHESIS)    // discriminant part
  {
    TEXT_POSITION    pos;
    PENTITY          t;
    ENTITY (A_FIELD) field;

    get_token();    // skip '('

    pos = token.pos;

    t = parse_type_name ();

    if (t == null)         // earlier error
      t = type_wchar;

    if (complete_type_of(t)^.kind != AN_ENUMERATION_TYPE)
    {
      semantic_error ("an enumeration type is expected here", pos);
      t = type_wchar;     // force enumeration type
    }

    if (token.kind != IDENTIFIER)
    {
      syntax_error ("an identifier is expected here", token.pos);
      wstrcpy (out token.info._identifier.value, L"$");      // dummy identifier
    }

    clear field;
    field.the_field.type = t;

    discr = append_new_entity (field, token.info._identifier.value, token.pos);

    if (token.kind == IDENTIFIER)
      get_token();

    if (token.kind == RIGHT_PARENTHESIS)
      get_token();    // skip ')'
    else
    {
      syntax_error ("')' is expected here", token.pos);
      skip_until (LEFT_ACCOLADE, false);
    }
  }

  append_region (stru.the_struct_type.fields);

  if (token.kind == LEFT_ACCOLADE)
  {
    get_token();
  }
  else
  {
    syntax_error ("'{' expected here", token.pos);
    skip_until (LEFT_ACCOLADE, true);
  }

  parse_field_declarations (    estru,
                                is_packed,
                                false,
                                0,
                            ref estru^.the_struct_type.is_unsafe);

  if (token.kind == TOKEN_switch)
  {
    if (!stru.the_struct_type.is_open_type)
    {
      syntax_error ("a variant part is only allowed if the type has a discriminant", token.pos);
      skip_until (RIGHT_ACCOLADE, true);
    }
    else
    {
      parse_variant_part (    estru,
                              is_packed,
                              discr,
                          ref estru^.the_struct_type.is_unsafe);
    }
  }
  else
  {
    if (stru.the_struct_type.is_open_type)
      syntax_error ("keyword 'switch' is expected here", token.pos);
  }

  unlink_region_level ();
  global_locked_struct_entity = null;


  // repeat this test now that all fields have been appended to the struct

  if (incomplete != null)
  {
    if (is_jagged_type (estru))
      semantic_error ("a jagged type is not allowed as the full type of an earlier incomplete type", stru_pos);
  }

  if (opaque != null)
  {
    if (is_jagged_type (estru))
      semantic_error ("a jagged type is not allowed as the full type of an earlier opaque type", stru_pos);
    else if (is_open_type (estru))
      semantic_error ("an open type is not allowed as the full type of an earlier opaque type", stru_pos);
  }


  if (token.kind == RIGHT_ACCOLADE)
  {
    get_token();
  }
  else
  {
    syntax_error ("'}' expected here", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
  }
}

//=================================================================================

public
void parse_union_declaration ()
{
  ENTITY (A_UNION_TYPE) un;
  PENTITY               incomplete, u;

  get_token();   // skip 'union'

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
    return;
  }


  // check if it's the full type of an earlier incomplete type

  incomplete = get_incomplete_type (token.info._identifier.value);


  clear un;
  un.the_union_type.fields = new_region ();

  u = append_new_entity (un,
                         incomplete != null ? L"" : token.info._identifier.value,
                         token.pos);

  fill_incomplete_type (incomplete, u, token.pos);

  hide_entity (u);

  if (incomplete != null)
    hide_entity (incomplete);


  get_token();    // skip identifier


  create_region_level ();

  append_region (un.the_union_type.fields);

  if (token.kind == LEFT_ACCOLADE)
  {
    get_token();
  }
  else
  {
    syntax_error ("'{' expected here", token.pos);
    skip_until (LEFT_ACCOLADE, true);
  }

  parse_field_declarations (    u,
                                is_packed          => true,
                                in_variant         => false,
                                discriminant_value => 0,
                            ref is_unsafe          => u^.the_union_type.is_unsafe);

  unlink_region_level ();

  unhide_all_entities ();

  if (token.kind == RIGHT_ACCOLADE)
  {
    get_token();
  }
  else
  {
    syntax_error ("'}' expected here", token.pos);
    skip_until (RIGHT_ACCOLADE, true);
  }
}

//=================================================================================

public
void parse_ref_declaration ()
{
  PENTITY              t, typ, rf;
  wchar                id[MAX_IDENTIFIER_LENGTH+1];
  TEXT_POSITION        pos, typ_pos, exp_pos;
  ENTITY (A_REFERENCE) refer;
  CONTEXT              context;
  PEXPRESSION          exp;

  get_token();

  typ_pos = token.pos;

  t = parse_type_definition (token.pos, null);

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }

  wstrcpy (out id, token.info._identifier.value);
  pos = token.pos;

  get_token();      // skip identifier

  typ = parse_declarator_suffix (t, typ_pos);

  clear refer;
  refer.the_reference.type = typ;       // can be changed later
  refer.the_reference.mode = MODE_IN;   // default
  refer.the_reference.is_used = false;
  refer.the_reference.references_heap_object = false;
  refer.the_reference.pos = pos;
  store_location (out refer.the_reference.loc);

  rf = create_new_entity (refer, id, pos);

  if (token.kind != ASSIGN)
  {
    syntax_error ("'=' expected here", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }

  get_token();      // skip '='

  hide_entity (rf);

  exp_pos = token.pos;

  convert_type_into_context (typ, out context);

  parse_unary_expression (context, token.pos, null, out exp);

  unhide_all_entities ();

  if (exp^.form != AN_OBJECT)
    semantic_error ("an object is expected here", exp_pos);


  // if 'typ' denotes an unconstrained array/struct,
  // replace 'typ' by a constrained type built from the expression's constraint.

  if (is_open_type (typ) && exp^.constraint.kind == CONSTANT_CONSTRAINT)
  {
    PENTITY tt;

    tt = complete_type_of (typ);

    if (tt^.kind == AN_OPEN_ARRAY_TYPE)
    {
      ENTITY (AN_ARRAY_TYPE) array;

      clear array;
      array.the_array_type.open_array = typ;
      array.the_array_type.length = (uint4)exp^.constraint.value;

      typ = append_new_entity (array, L"", token.pos);
    }
    else    // A STRUCT TYPE
    {
      ENTITY (A_CONSTRAINED_STRUCT_TYPE) cons;

      clear cons;
      cons.the_constrained_struct_type.open_struct = typ;
      cons.the_constrained_struct_type.discriminant_value = (uint4)exp^.constraint.value;

      typ = append_new_entity (cons, L"", token.pos);
    }
  }


  // check that object is type compatible with type name
  // if type_name has a constraint, it must match !

  (void) check_ref_parameter_context_compatibility (context, exp, exp_pos);


  rf^.the_reference.mode = (exp^.access == ACCESS_READWRITE) ? MODE_REF : MODE_IN;
  rf^.the_reference.type = typ;
  rf^.the_reference.name = exp;


  append_entity_in_entity_queue (rf);


  parse_semicolon ();
}

//=================================================================================

// when t is non-null, a type definition has been parsed earlier and an identifier follows.
// when t is null, current token is : const or volatile.

public
void parse_object_declaration (TEXT_POSITION pos0, PENTITY t0, bool is_global)
{
  PENTITY       t;
  bool          is_const    = false;
  bool          is_volatile = false;
  CONTEXT       context;
  PEXPRESSION   exp;
  TEXT_POSITION typ_pos, exp_pos;
  bool          is_valid;

  typ_pos = pos0;
  t = t0;

  if (t == null)
  {
    if (token.kind == TOKEN_const)
    {
      is_const = true;
      get_token();
    }
    else if (token.kind == TOKEN_volatile)
    {
      is_volatile = true;
      get_token();
    }

    typ_pos = token.pos;
    t = parse_type_definition (token.pos, null);
  }


  // declare object list of type t

  for (;;)
  {
    wchar          id[MAX_IDENTIFIER_LENGTH+1];
    TEXT_POSITION  id_pos;
    PENTITY        typ, obj;

    if (token.kind != IDENTIFIER)
    {
      syntax_error ("identifier expected", token.pos);
      skip_until (SEMICOLON, true);
      return;
    }

    wstrcpy (out id, token.info._identifier.value);
    id_pos = token.pos;

    get_token();      // skip identifier

    typ = parse_declarator_suffix (t, typ_pos);


    if (is_const)
    {
      ENTITY (A_CONSTANT) cte;

      clear cte;
      cte.the_constant.type    = typ;      // can later be replaced by a constrained type
      cte.the_constant.value   = null;     // temporary value
      cte.the_constant.is_used = false;
      cte.the_constant.pos     = id_pos;

      obj = create_new_entity (cte, id, id_pos);
    }
    else if (is_global)
    {
      ENTITY (A_GLOBAL_VARIABLE) global;

      clear global;
      global.the_global_variable.type                  = typ;
      global.the_global_variable.initial_value_or_null = null;
      global.the_global_variable.is_volatile           = is_volatile;

      obj = create_new_entity (global, id, id_pos);

      if (is_volatile)
      {
        PENTITY tt;

        tt = complete_type_of (typ);

        if (tt^.kind == AN_INTEGER_TYPE && INTEGER_DATA[(int)tt^.the_integer_type.type].size <= 4)
        {
          // ok
        }
        else if (tt^.kind == A_FLOAT_TYPE && FLOAT_DATA[(int)tt^.the_float_type.type].size <= 4)
        {
          // ok
        }
        else if (tt^.kind == AN_ENUMERATION_TYPE     || tt^.kind == A_POINTER_TYPE ||
                 tt^.kind == A_FUNCTION_POINTER_TYPE || tt^.kind == AN_UNSAFE_POINTER_TYPE)
        {
          // ok
        }
        else
        {
          semantic_error ("keyword volatile is not allowed for this type", pos0);
        }
      }
    }
    else   // local
    {
      ENTITY (A_LOCAL_VARIABLE) local;

      clear local;
      local.the_local_variable.type                  = typ;
      local.the_local_variable.initial_value_or_null = null;
      local.the_local_variable.is_read               = false;
      local.the_local_variable.is_written            = false;
      local.the_local_variable.pos                   = id_pos;
      store_location (out local.the_local_variable.loc);

      obj = create_new_entity (local, id, id_pos);

      if (is_volatile)
        semantic_error ("keyword volatile is only allowed for global variables", pos0);
    }


    is_valid = true;

    if (token.kind == ASSIGN)
    {
      if (is_limited_type (typ))
        semantic_error ("a default expression is not allowed for a limited type", token.pos);

      get_token();      // skip '='

      hide_entity (obj);

      exp_pos = token.pos;

      convert_type_into_context (typ, out context);

      parse_expression (context, token.pos, null, out exp);

      if (check_assignment_context_compatibility (context, exp, exp_pos) < 0)
      {
        typ = type_int;
        free_exp (exp);
        exp = dummy_expression ();
        is_valid = false;
      }

      unhide_all_entities ();
    }
    else
    {
      exp_pos = token.pos;
      if (is_const)
      {
        syntax_error ("'=' expected", token.pos);    // exp mandatory for a constant
        exp = dummy_expression ();
        typ = type_int;
        is_valid = false;
      }
      else
      {
        exp = null;
      }
    }



    // fill entity with type and expression

    if (is_const)
    {
      if (!is_constant_exp (exp))
      {
        semantic_error ("a constant expression is expected here", exp_pos);
        free_exp (exp);
        exp = dummy_expression ();
        typ = type_int;
        is_valid = false;
      }
      else if (is_valid)
      {
        // if 'typ' denotes an unconstrained array/struct,
        // replace 'typ' by a constrained type built from the expression's constraint.

        if (is_open_type (typ))
        {
          PENTITY tt;

          if (exp^.constraint.kind != CONSTANT_CONSTRAINT)
            fatal_compiler_error ("object decl", token.pos);

          tt = complete_type_of (typ);

          if (tt^.kind == AN_OPEN_ARRAY_TYPE)
          {
            ENTITY (AN_ARRAY_TYPE) array;

            clear array;
            array.the_array_type.open_array = typ;
            array.the_array_type.length = (uint4)exp^.constraint.value;

            typ = append_new_entity (array, L"", token.pos);
          }
          else    // A STRUCT TYPE
          {
            ENTITY (A_CONSTRAINED_STRUCT_TYPE) cons;

            clear cons;
            cons.the_constrained_struct_type.open_struct = typ;
            cons.the_constrained_struct_type.discriminant_value = (uint4)exp^.constraint.value;

            typ = append_new_entity (cons, L"", token.pos);
          }
        }
      }

      obj^.the_constant.type  = typ;
      obj^.the_constant.value = exp;
    }
    else    // global or local variable
    {
      if (is_open_type (typ))
        semantic_error ("type of variable must have a constraint, or must be a constant", id_pos);
      else if (is_jagged_type (typ))
        semantic_error ("type of variable must not be jagged, or must be a constant", id_pos);

      if (is_global)
      {
        if (exp != null && !is_constant_exp (exp))
        {
          semantic_error ("a constant initial expression is expected for a global variable", exp_pos);
          free_exp (exp);
          exp = dummy_expression ();
        }

        obj^.the_global_variable.initial_value_or_null = exp;
      }
      else   // local
      {
        obj^.the_local_variable.initial_value_or_null = exp;
      }
    }


    append_entity_in_entity_queue (obj);


    if (token.kind == COMMA)
    {
      get_token();
    }
    else if (token.kind == SEMICOLON)
    {
      get_token();
      break;
    }
    else
    {
      syntax_error ("',' or ';' expected", token.pos);
      skip_until (SEMICOLON, true);
      break;
    }
  }
}

//=================================================================================

public
void create_enumeration_literal_string_table (PENTITY t)
{
  int     size, align;
  uint4   i, j, len;
  POOL    p, p0;
  PENTITY e;

  // create a table of strings, one per enumeration literal

  (void)size_and_alignment_of_type (type_array_of_string, out size, out align);

  p = new_pool_constant ((uint)size * (t^.the_enumeration_type.last + 1), (uint)align);

  e = t;
  for (i=0; i<=t^.the_enumeration_type.last; i++)
  {
    e = e^.next;

    if (e^.kind != AN_ENUMERATION_LITERAL)
      fatal_compiler_error ("create_enum_lit", token.pos);

    len = (uint)wstrlen(e^.identifier_or_null^);
    p0 = new_pool_constant (len, 1);

    for (j=0; j<len; j++)
      store_integer (p0, j, (int8)e^.identifier_or_null^[j], 1);

    store_reference (p, (uint)size*i, p0);                       // jagged reference
    store_integer (p, (uint)size*i+(uint)address_size, len, 4);  // int4 (length)
  }

  t^.the_enumeration_type.string_table = p;
}

//=================================================================================
