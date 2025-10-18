
// decl.c : declarations

use tokens, lex, entities, typedecl, packages, type;
use ../common, ../error;

//=================================================================================

void parse_object_or_function_declaration_or_function_body (    bool body_allowed,
                                                            ref bool body_was_parsed)
{
  TEXT_POSITION pos0;
  PENTITY       t;

  pos0 = token.pos;

  t = parse_type_definition (pos0, null);

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }

  get_look_ahead_token ();     // fill 'look_ahead_token'

  if (look_ahead_token.kind == LEFT_PARENTHESIS && !is_open_type (t))
  {
    parse_function_declaration_or_function_body (pos0, t, body_allowed, ref body_was_parsed);
  }
  else
  {
    if (body_was_parsed)
      syntax_error ("an object declaration is not allowed after a body", pos0);
    parse_object_declaration (pos0, t, is_global => true);
  }
}

//=================================================================================

// used for unit interfaces, unit bodies,
//          package declaration and package bodies.

public
void parse_global_declarations (bool is_body)
{
  bool body_was_parsed = false;

  for (;;)
  {
    switch (token.kind)
    {
      case TOKEN_enum:
        if (body_was_parsed)
          syntax_error ("an enum declaration is not allowed after a body", token.pos);
        parse_enumeration_declaration ();
        break;

      case TOKEN_typedef:
        if (body_was_parsed)
          syntax_error ("a typedef declaration is not allowed after a body", token.pos);
        parse_typedef_or_incomplete_declaration ();
        break;

      case TOKEN_packed:
      case TOKEN_struct:
        if (body_was_parsed)
          syntax_error ("a struct declaration is not allowed after a body", token.pos);
        parse_struct_or_opaque_declaration ();
        break;

      case TOKEN_union:
        if (body_was_parsed)
          syntax_error ("a union declaration is not allowed after a body", token.pos);
        parse_union_declaration ();
        break;

      case TOKEN_ref:
        semantic_error ("reference declarations are only allowed inside functions", token.pos);
        skip_until (SEMICOLON, true);
        break;

      case TOKEN_package:
        parse_package_declaration_or_package_instantiation_or_package_body
                (is_body, ref body_was_parsed);
        break;

      case TOKEN_generic:
        parse_generic_package_declaration ();
        break;

      case TOKEN_const:
      case TOKEN_volatile:
        {
          TEXT_POSITION pos0;

          if (body_was_parsed)
            syntax_error ("an object declaration is not allowed after a body", token.pos);

          pos0 = token.pos;
          parse_object_declaration (pos0, null, is_global => true);
        }
        break;

      case IDENTIFIER:
      case TOKEN_int1:
      case TOKEN_int2:
      case TOKEN_int4:
      case TOKEN_int8:
      case TOKEN_uint1:
      case TOKEN_uint2:
      case TOKEN_uint4:
      case TOKEN_tiny:
      case TOKEN_short:
      case TOKEN_int:
      case TOKEN_long:
      case TOKEN_byte:
      case TOKEN_ushort:
      case TOKEN_uint:
      case TOKEN_float:
      case TOKEN_float4:
      case TOKEN_float8:
      case TOKEN_double:
      case TOKEN_bool:
      case TOKEN_char:
      case TOKEN_wchar:
      case TOKEN_string:
      case TOKEN_wstring:
      case TOKEN_object:
        parse_object_or_function_declaration_or_function_body (is_body, ref body_was_parsed);
        break;

      case LEFT_BRACKET:
      case TOKEN_public:
      case TOKEN_inline:
      case TOKEN_void:
        {
          TEXT_POSITION pos0;
          pos0 = token.pos;
          parse_function_declaration_or_function_body (pos0, null, is_body, ref body_was_parsed);
        }
        break;

      default:
        return;
    }
  }
}

//=================================================================================

// used for function bodies and block statements.
// 'out_prefix' is an "out" parameter that can either be null,
// or denote an already parsed expanded name that is not a type_name.
// the function sets lex_set_in_statement() to true before returning.

public
void parse_local_declarations (out TEXT_POSITION pos,
                               out PENTITY       out_prefix)
{
  out_prefix = null;

  lex_set_in_statement (false);

  for (;;)
  {
    switch (token.kind)
    {
      case TOKEN_enum:
        parse_enumeration_declaration ();
        break;

      case TOKEN_typedef:
        parse_typedef_or_incomplete_declaration ();
        break;

      case TOKEN_packed:
      case TOKEN_struct:
        parse_struct_or_opaque_declaration ();
        break;

      case TOKEN_union:
        parse_union_declaration ();
        break;

      case TOKEN_ref:
        parse_ref_declaration ();
        break;

      case TOKEN_const:
      case TOKEN_volatile:
      case TOKEN_int1:
      case TOKEN_int2:
      case TOKEN_int4:
      case TOKEN_int8:
      case TOKEN_uint1:
      case TOKEN_uint2:
      case TOKEN_uint4:
      case TOKEN_tiny:
      case TOKEN_short:
      case TOKEN_int:
      case TOKEN_long:
      case TOKEN_byte:
      case TOKEN_ushort:
      case TOKEN_uint:
      case TOKEN_float:
      case TOKEN_float4:
      case TOKEN_float8:
      case TOKEN_double:
      case TOKEN_bool:
      case TOKEN_char:
      case TOKEN_wchar:
      case TOKEN_string:
      case TOKEN_wstring:
      case TOKEN_object:
        {
          TEXT_POSITION pos0;
          pos0 = token.pos;
          parse_object_declaration (pos0, null, is_global => false);
        }
        break;

      case IDENTIFIER:
        {
          TEXT_POSITION pos0;
          PENTITY       e, t;

          lex_set_in_statement (true);   // var *= exp;

          pos0 = token.pos;

          e = parse_expanded_name ();
          if (e == null)    // could not find entity (error already given)
          {
            lex_set_in_statement (false);
            skip_until (SEMICOLON, true);
            break;
          }

          if (e^.kind < LAST_ENTITY_DENOTING_A_TYPE)   // it's a type
          {
            lex_set_in_statement (false);
            t = parse_type_definition (pos0, e);
            parse_object_declaration (pos0, t, is_global => false);
            break;
          }

          // it's a statement
          pos = pos0;
          out_prefix = e;
          return;
        }

      default:
        lex_set_in_statement (true);
        pos = token.pos;
        return;
    }
  }
}

//=================================================================================
