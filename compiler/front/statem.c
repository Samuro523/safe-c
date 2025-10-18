
// statem.c : statements analysis

from std use bintree;
use ../error, ../common;
use type, tokens, lex, entities, typedecl, exp, decl, complete;

//=================================================================================

PENTITY surrounding_statement (PENTITY e)
{
  switch (e^.kind)
  {
    case A_BLOCK_STATEMENT  :  return e^.the_block_statement.outer;
    case AN_IF_STATEMENT    :  return e^.the_if_statement.outer;
    case A_SWITCH_STATEMENT :  return e^.the_switch_statement.outer;
    case A_WHILE_STATEMENT  :  return e^.the_while_statement.outer;
    case A_FOR_STATEMENT    :  return e^.the_for_statement.outer;
    default:   fatal_compiler_error ("surrounding_statement", token.pos); return null;
  }
}

//=================================================================================

void parse_asm_statement ()
{
  int8                      n;
  TEXT_POSITION             pos;
  ENTITY (A_CODE_STATEMENT) cs;
  byte[]^                   buf;
  int                       size, len;
  PENTITY                   e;
  const int CHUNK = 16;

  if (!lexa.within_unsafe)
    semantic_error ("code statements are only allowed within unsafe regions", token.pos);

  get_token();   // skip _asm

  if (token.kind != LEFT_ACCOLADE)
  {
    syntax_error ("'{' expected", token.pos);
    skip_until (SEMICOLON, false);
    return;
  }

  get_token();   // skip '{'

  pos = token.pos;

  len = 0;
  size = CHUNK;
  buf = new byte[size];

  while (token.kind != RIGHT_ACCOLADE && token.kind != LAST_TOKEN)
  {
    pos = token.pos;
    n = parse_constant_int_or_uint_expression ();
    if (n < 0 || n > 255)
      syntax_error ("constant must be in range 0 .. 255", pos);

    if (len == size)
    {
      byte[]^ old;
      old = buf;
      buf = new byte[size + CHUNK];
      buf^[0:size] = old^;
      free old;
      size += CHUNK;
    }

    buf^[len++] = (byte)n;
  }

  if (token.kind != RIGHT_ACCOLADE)
  {
    syntax_error ("'}' expected", token.pos);
    return;
  }

  get_token();   // skip '}'


  // create statement entity

  clear cs;
  store_location (out cs.the_code_statement.loc);
  cs.the_code_statement.code = buf;
  cs.the_code_statement.nb_bytes = len;
  (void)append_new_entity (cs, L"", pos);


  // find surrounding function

  e = surrounding_entity ();
  while (e^.kind != A_FUNCTION_BODY)
    e = surrounding_statement (e);
  e^.the_function_body.contains_code_statements = true;
}

//=================================================================================

void parse_unused_statement ()
{
  TEXT_POSITION                pos;
  ENTITY (AN_UNUSED_STATEMENT) us;
  PENTITY                      e;

  get_token();   // skip _unused

  for (;;)
  {
    if (token.kind != IDENTIFIER)
    {
      syntax_error ("variable or parameter expected", token.pos);
      skip_until (SEMICOLON, false);
      return;
    }

    pos = token.pos;

    e = parse_expanded_name ();
    if (e == null)   // identifier not found (error already given)
      return;

    if (e^.kind == A_PARAMETER)
    {
      e^.the_parameter.is_used = true;
    }
    else if (e^.kind == A_LOCAL_VARIABLE)
    {
      e^.the_local_variable.is_used = true;
    }
    else
    {
      semantic_error ("variable or parameter name expected", pos);
      skip_until (SEMICOLON, false);
      return;
    }

    clear us;
    us.the_unused_statement.pos = pos;
    us.the_unused_statement.item = e;
    (void)append_new_entity (us, L"", pos);

  
    // parse commas

    if (token.kind != COMMA && token.kind != SEMICOLON)
    {
      syntax_error (", or ; expected", token.pos);
      return;
    }

    if (token.kind == SEMICOLON)
      break;

    get_token ();  // skip comma
  }
}

//=================================================================================

void parse_clear_statement ()
{
  get_token();   // skip clear  

  for (;;)
  {
    CONTEXT                    context;
    PEXPRESSION                e;
    TEXT_POSITION              pos;
    ENTITY (A_CLEAR_STATEMENT) cl;

    clear context;
    pos = token.pos;

    parse_unary_expression (context, token.pos, null, out e);


    // check that e denotes a read/write object of a non-jagged type

    if (e^.form != AN_OBJECT)
      semantic_error ("name must denote an object", pos);
    else if (e^.access != ACCESS_READWRITE)
      semantic_error ("name must denote a read/write object", pos);
    else if (e^.base_type_or_null != null && is_jagged_type (e^.base_type_or_null))
      semantic_error ("name must not have a jagged type", pos);
    else if (e^.base_type_or_null != null &&
             (e^.base_type_or_null^.kind == AN_INCOMPLETE_TYPE ||
              e^.base_type_or_null^.kind == A_NULL_POINTER_TYPE ||
              e^.base_type_or_null^.kind == A_VOID_TYPE))
      semantic_error ("illegal object name", pos);


    // create statement entity

    clear cl;
    cl.the_clear_statement.name = e;
    store_location (out cl.the_clear_statement.loc);
    (void)append_new_entity (cl, L"", pos);


    // parse commas

    if (token.kind != COMMA && token.kind != SEMICOLON)
    {
      syntax_error (", or ; expected", token.pos);
      return;
    }

    if (token.kind == SEMICOLON)
      break;

    get_token ();  // skip comma
  }
}

//=================================================================================

void parse_statement_starting_with_exp (TEXT_POSITION pos0,
                                        PENTITY       prefix,
                                        bool          pre_post_statement_allowed,
                                        bool          function_call_allowed)
{
  TEXT_POSITION pos, assign_pos;
  bool          function_call_with_return_value_expected = false;
  PEXPRESSION   e, e2;
  CONTEXT       context;
  TOKEN_KIND    tk;
  ASSIGNMENT_OP op;

  pos = pos0;

  if (function_call_allowed && prefix == null && token.kind == LEFT_PARENTHESIS)    // (void)
  {
    get_look_ahead_token ();
    if (look_ahead_token.kind == TOKEN_void)
    {
      get_token ();  // skip '('
      get_token ();  // skip void

      if (token.kind != RIGHT_PARENTHESIS)
        syntax_error ("')' expected", token.pos);
      else
        get_token ();  // skip ')'

      function_call_with_return_value_expected = true;
      pos = token.pos;
    }
  }


  clear context;

  parse_unary_expression (context, pos, prefix, out e);


  if (function_call_with_return_value_expected)
  {
    if (e^.kind != A_FUNCTION_CALL)
      semantic_error ("a function call is expected here", pos);
    else if (e^.base_type_or_null == type_void)
      semantic_error ("(void) is only allowed for a function that returns a value", pos);
  }
  else    // check either for function call or post/pre inc/dec node
  {
    if (function_call_allowed && e^.kind == A_FUNCTION_CALL)
    {
    }
    else if (pre_post_statement_allowed && e^.kind == AN_OPERATOR_VALUE)
    {
      KIND_OPERATOR kop;

      kop = e^.operator_value_info.op;

      if (kop != OP_PRE_DEC && kop != OP_PRE_INC && kop != OP_POST_DEC && kop != OP_POST_INC)
      {
        if (function_call_allowed)
          semantic_error ("assignment, function call or increment/decrement statement expected", pos);
        else
          semantic_error ("assignment or increment/decrement statement expected", pos);
      }
    }
    else if (e^.form == AN_OBJECT)
    {
      if (e^.access != ACCESS_READWRITE)
        semantic_error ("name must denote a read/write object", pos);
      else if (e^.base_type_or_null != null && is_jagged_type (e^.base_type_or_null))
        semantic_error ("name must not have a jagged type", pos);
      else if (e^.base_type_or_null != null && is_limited_type (e^.base_type_or_null))
        semantic_error ("name must not have a limited type", pos);
      else if (e^.base_type_or_null != null &&
               (e^.base_type_or_null^.kind == AN_INCOMPLETE_TYPE  ||
                e^.base_type_or_null^.kind == AN_OPAQUE_TYPE ||
                e^.base_type_or_null^.kind == A_NULL_POINTER_TYPE ||
                e^.base_type_or_null^.kind == A_VOID_TYPE))
        semantic_error ("illegal object name", pos);
    }
    else
    {
      if (function_call_allowed)
        semantic_error ("assignment, function call or increment/decrement statement expected", pos);
      else if (pre_post_statement_allowed)
        semantic_error ("assignment or increment/decrement statement expected", pos);
      else
        semantic_error ("assignment statement expected", pos);
    }
  }


  if (e^.form == AN_OBJECT)
  {
    switch (token.kind)
    {
      case ASSIGN:
      case PLUS_EQUAL:
      case MINUS_EQUAL:
      case STAR_EQUAL:
      case SLASH_EQUAL:
      case PERCENT_EQUAL:
      case SHIFT_LEFT_EQUAL:
      case SHIFT_RIGHT_EQUAL:
      case AMPERSAND_EQUAL:
      case VERTICAL_BAR_EQUAL:
      case CARET_EQUAL:
        tk = token.kind;
        break;

      default:
        semantic_error ("'=' or other assignment symbol expected", token.pos);
        return;
    }

    assign_pos = token.pos;
    get_token ();  // skip assign symbol


    // parse expression

    clear context;
    context.base_type_or_null = e^.base_type_or_null;
    context.constraint = e^.constraint;

    parse_expression (context, token.pos, null, out e2);


    // check that expression matches designator

    op = _ASSIGN;   // default

    if (e^.base_type_or_null != null && e2^.base_type_or_null != null)
    {
      PENTITY t1, t2;

      t1 = e^.base_type_or_null;
      t2 = e2^.base_type_or_null;

      switch (tk)
      {
        case ASSIGN:
          // returns 0 if OK, -1 if types are not compatible
          // error message is generated in function
          (void) check_assignment_context_compatibility (context, e2, assign_pos);

          op = _ASSIGN;
          break;


        case PLUS_EQUAL:
        case MINUS_EQUAL:
          if (t1^.kind == AN_ENUMERATION_TYPE && t2^.kind == AN_INTEGER_TYPE)
          {
            op = (tk == PLUS_EQUAL) ? _ASSIGN_ADD_INT : _ASSIGN_SUB_INT;
          }
          else if (t1^.kind == AN_UNSAFE_POINTER_TYPE && t2^.kind == AN_INTEGER_TYPE)
          {
            op = (tk == PLUS_EQUAL) ? _ASSIGN_ADD_PTR_INT : _ASSIGN_SUB_PTR_INT;
            if (t2 == type_long)
              semantic_error ("right operand must not have type long", assign_pos);
          }
          else if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
          {
            (void) check_assignment_context_compatibility (context, e2, assign_pos);
            op = (tk == PLUS_EQUAL) ? _ASSIGN_ADD_INT : _ASSIGN_SUB_INT;
          }
          else if (t1^.kind == A_FLOAT_TYPE && t2^.kind == A_FLOAT_TYPE)
          {
            (void) check_assignment_context_compatibility (context, e2, assign_pos);
            op = (tk == PLUS_EQUAL) ? _ASSIGN_ADD_FLOAT : _ASSIGN_SUB_FLOAT;
          }
          else
          {
            semantic_error ("incompatible expression", assign_pos);
            op = _ASSIGN_ADD_INT;
          }
          break;


        case STAR_EQUAL:
        case SLASH_EQUAL:
        case PERCENT_EQUAL:

          if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
          {
            bool is_signed;

            if (tk == SLASH_EQUAL || tk == PERCENT_EQUAL)
            {
              if (e2^.kind == A_CONST_INTEGER_VALUE && e2^.const_integer_value_info.value == 0)
                semantic_error ("division by zero", assign_pos);
            }

            // source type must fit into target type
            (void) check_assignment_context_compatibility (context, e2, assign_pos);


            is_signed = INTEGER_DATA[(uint)t1^.the_integer_type.type].is_signed;
            if (tk == STAR_EQUAL)
              op = is_signed ? _ASSIGN_MULT_INT_SIGNED : _ASSIGN_MULT_INT_UNSIGNED;
            else if (tk == SLASH_EQUAL)
              op = is_signed ? _ASSIGN_DIV_INT_SIGNED  : _ASSIGN_DIV_INT_UNSIGNED;
            else
              op = is_signed ? _ASSIGN_MOD_INT_SIGNED  : _ASSIGN_MOD_INT_UNSIGNED ;
          }
          else if (t1^.kind == A_FLOAT_TYPE && t2^.kind == A_FLOAT_TYPE)
          {
            if (tk == PERCENT_EQUAL)
              semantic_error ("operator '%=' is not allowed for float types", assign_pos);
            else if (tk == SLASH_EQUAL)
            {
              if (e2^.kind == A_CONST_FLOAT_VALUE && e2^.const_float_value_info.value == 0.0)
                semantic_error ("division by zero", pos);
            }

            // source type must fit into target type
            (void) check_assignment_context_compatibility (context, e2, assign_pos);

            op = (tk == STAR_EQUAL) ? _ASSIGN_MULT_FLOAT : _ASSIGN_DIV_FLOAT;
          }
          else
          {
            semantic_error ("incompatible expression", assign_pos);
            op = _ASSIGN_MULT_FLOAT;
          }
          break;


        case SHIFT_LEFT_EQUAL:
        case SHIFT_RIGHT_EQUAL:

          if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
          {
            int8 v2;
            bool is_signed;

            // source type must fit into target type
            if (check_assignment_context_compatibility (context, e2, assign_pos) == 0)
            {
              if (e2^.kind == A_CONST_INTEGER_VALUE)
              {
                v2 = e2^.const_integer_value_info.value;

                if (t1 == type_long || t1 == type_int_literal)
                {
                  if (v2 < 0 || v2 > 63)
                    semantic_error ("right operand must be in range 0..63", assign_pos);
                }
                else
                {
                  if (v2 < 0 || v2 > 31)
                    semantic_error ("right operand must be in range 0..31", assign_pos);
                }
              }
            }

            is_signed = INTEGER_DATA[(uint)t1^.the_integer_type.type].is_signed;

            if (tk == SHIFT_LEFT_EQUAL)
              op = is_signed ? _ASSIGN_SHIFT_LEFT_SIGNED  : _ASSIGN_SHIFT_LEFT_UNSIGNED;
            else
              op = is_signed ? _ASSIGN_SHIFT_RIGHT_SIGNED : _ASSIGN_SHIFT_RIGHT_UNSIGNED;
          }
          else
          {
            semantic_error ("incompatible expression", assign_pos);
            op = _ASSIGN_SHIFT_LEFT_SIGNED;
          }
          break;


        case AMPERSAND_EQUAL:
        case VERTICAL_BAR_EQUAL:
        case CARET_EQUAL:

          if (t1^.kind == AN_INTEGER_TYPE && t2^.kind == AN_INTEGER_TYPE)
          {
            // source type must fit into target type
            (void) check_assignment_context_compatibility (context, e2, assign_pos);

            if (tk == AMPERSAND_EQUAL)
              op = _ASSIGN_BITAND;
            else if (tk == VERTICAL_BAR_EQUAL)
              op = _ASSIGN_BITOR;
            else
              op = _ASSIGN_BITXOR;
          }
          else if (t1 == type_bool && t2 == type_bool)
          {
            // source type must fit into target type
            (void) check_assignment_context_compatibility (context, e2, assign_pos);

            if (tk == AMPERSAND_EQUAL)
              op = _ASSIGN_AND;
            else if (tk == VERTICAL_BAR_EQUAL)
              op = _ASSIGN_OR;
            else
              op = _ASSIGN_XOR;
          }
          else
          {
            semantic_error ("incompatible expression", assign_pos);
            op = _ASSIGN_BITAND;
          }
          break;

        default:
          semantic_error ("'=' or other assignment symbol expected", token.pos);
          return;
      }
    }
  
    
    {
      ENTITY (AN_ASSIGNMENT_STATEMENT) en;

      clear en;
      en.the_assignment_statement.name = e;
      en.the_assignment_statement.op = op;
      en.the_assignment_statement.value = e2;
      store_location (out en.the_assignment_statement.loc);
      (void)append_new_entity (en, L"", pos);
    }
  }
  else if (e^.kind == A_FUNCTION_CALL)
  {
    ENTITY (A_FUNCTION_CALL_STATEMENT) en;

    clear en;
    en.the_function_call_statement.name = e;
    store_location (out en.the_function_call_statement.loc);
    (void)append_new_entity (en, L"", pos);
  }
  else
  {
    ENTITY (A_PRE_OR_POSTFIX_STATEMENT) en;

    clear en;
    en.the_pre_or_postfix_statement.name = e;
    store_location (out en.the_pre_or_postfix_statement.loc);
    (void)append_new_entity (en, L"", pos);
  }
}

//=================================================================================

void parse_return_statement ()
{
  PENTITY                     e;
  PEXPRESSION                 exp;
  CONTEXT                     context;
  ENTITY (A_RETURN_STATEMENT) en;
  TEXT_POSITION               pos0, pos;

  pos0 = token.pos;
  
  get_token ();  // skip 'return'


  // find surrounding function and its return type

  e = surrounding_entity ();

  while (e^.kind != A_FUNCTION_BODY)
    e = surrounding_statement (e);

  e = e^.the_function_body.to_function_declaration;

  e = e^.the_function_declaration.to_type;

  e = e^.the_function_pointer_type.return_type;


  convert_type_into_context (e, out context);


  if (token.kind == SEMICOLON)
  {
    if (e != type_void)
      syntax_error ("return expression expected", token.pos);
    exp = null;
  }
  else
  {
    if (e == type_void)
      syntax_error (" ; expected", token.pos);

    pos = token.pos;
    parse_expression (context, token.pos, null, out exp);

    if (e != type_void)
      (void)check_assignment_context_compatibility (context, exp, pos);
  }


  clear en;
  en.the_return_statement.value = exp;
  en.the_return_statement.type  = e;
  en.the_return_statement.outer = surrounding_entity ();
  en.the_return_statement.pos   = pos0;
  store_location (out en.the_return_statement.loc);
  (void)append_new_entity (en, L"", token.pos);
}

//=================================================================================

void parse_break_statement ()
{
  PENTITY                    e;
  ENTITY (A_BREAK_STATEMENT) en;


  // check if we're within a "while", "for", or "switch" statement

  e = surrounding_entity ();

  for (;;)
  {
    if (e^.kind == A_FUNCTION_BODY || e^.kind == A_WHILE_STATEMENT ||
        e^.kind == A_FOR_STATEMENT || e^.kind == A_SWITCH_STATEMENT)
      break;
    e = surrounding_statement (e);
  }

  if (e^.kind == A_FUNCTION_BODY)
    semantic_error ("a break statement is only allowed in while/for/switch statements", token.pos);

  clear en;
  en.the_break_statement.outer = surrounding_entity ();
  en.the_break_statement.pos = token.pos;
  (void)append_new_entity (en, L"", token.pos);

  get_token ();  // skip 'break'
}

//=================================================================================

void parse_continue_statement ()
{
  PENTITY                       e;
  ENTITY (A_CONTINUE_STATEMENT) en;


  // check if we're within a "while" or "for" statement

  e = surrounding_entity ();

  for (;;)
  {
    if (e^.kind == A_FUNCTION_BODY || e^.kind == A_WHILE_STATEMENT || e^.kind == A_FOR_STATEMENT)
      break;
    e = surrounding_statement (e);
  }

  if (e^.kind == A_FUNCTION_BODY)
    semantic_error ("a break statement is only allowed in while/for statements", token.pos);

  clear en;
  en.the_continue_statement.outer = surrounding_entity ();
  en.the_continue_statement.pos = token.pos;
  (void)append_new_entity (en, L"", token.pos);

  get_token ();  // skip 'continue'
}

//=================================================================================

void parse_free_statement ()
{
  PEXPRESSION                exp;
  CONTEXT                    context;
  PENTITY                    t;
  TEXT_POSITION              pos;
  ENTITY (A_FREE_STATEMENT)  en;

  get_token ();  // skip 'free'

  for (;;)
  {
    clear context;

    pos = token.pos;
    parse_expression (context, token.pos, null, out exp);

    t = exp^.base_type_or_null;
    if (t != null && t^.kind != A_POINTER_TYPE && t^.kind != A_NULL_POINTER_TYPE)
      semantic_error ("a pointer value is expected here", pos);

    clear en;
    en.the_free_statement.value = exp;
    store_location (out en.the_free_statement.loc);
    (void)append_new_entity (en, L"", pos);


    // parse commas

    if (token.kind != COMMA && token.kind != SEMICOLON)
    {
      syntax_error (", or ; expected", token.pos);
      return;
    }

    if (token.kind == SEMICOLON)
      break;

    get_token ();  // skip comma
  }
}

//=================================================================================

void parse_assert_statement ()
{
  PEXPRESSION                  exp;
  CONTEXT                      context;
  PENTITY                      t;
  TEXT_POSITION                pos;
  ENTITY (AN_ASSERT_STATEMENT) en;

  get_token ();  // skip 'assert'


  clear context;

  pos = token.pos;
  parse_expression (context, token.pos, null, out exp);

  t = exp^.base_type_or_null;
  if (t != null && t != type_bool)
    semantic_error ("a bool expression is expected here", pos);

  if (exp^.kind == A_CONST_ENUMERATION_VALUE && exp^.const_enumeration_value_info.value == 0)
    semantic_error ("assertion failed", pos);

  clear en;
  en.the_assert_statement.value = exp;
  store_location (out en.the_assert_statement.loc);
  (void)append_new_entity (en, L"", pos);
}

//=================================================================================

void parse_abort_statement ()
{
  ENTITY (AN_ABORT_STATEMENT) en;
  TEXT_POSITION               pos;

  pos = token.pos;

  get_token ();  // skip 'abort'

  clear en;
  store_location (out en.the_abort_statement.loc);
  en.the_abort_statement.pos = pos;
  (void)append_new_entity (en, L"", pos);
}

//=================================================================================

void parse_sleep_statement ()
{
  PEXPRESSION                exp;
  CONTEXT                    context;
  PENTITY                    t;
  TEXT_POSITION              pos;
  ENTITY (A_SLEEP_STATEMENT) en;

  get_token ();  // skip 'sleep'


  clear context;

  pos = token.pos;
  parse_expression (context, token.pos, null, out exp);

  t = exp^.base_type_or_null;
  if (t != null && t^.kind != AN_INTEGER_TYPE && t^.kind != A_FLOAT_TYPE)
    semantic_error ("a numeric expression is expected here", pos);

  if (exp^.kind == A_CONST_INTEGER_VALUE)
  {
    if (exp^.const_integer_value_info.value < -86400 ||
        exp^.const_integer_value_info.value > 86400)
      semantic_error ("value must be in range -86400 .. +86400", pos);
  }

  if (exp^.kind == A_CONST_FLOAT_VALUE)
  {
    if (exp^.const_float_value_info.value < -86400.0 ||
        exp^.const_float_value_info.value > 86400.0)
      semantic_error ("value must be in range -86400 .. +86400", pos);
  }

  clear en;
  en.the_sleep_statement.value = exp;
  store_location (out en.the_sleep_statement.loc);
  (void)append_new_entity (en, L"", pos);
}

//=================================================================================

void parse_block_statement ()
{
  ENTITY (A_BLOCK_STATEMENT) en;
  PENTITY                    out_prefix;
  TEXT_POSITION              out_pos;
  PENTITY                    eblock;

  get_token();      // skip '{'

  clear en;
  en.the_block_statement.inner = new_region ();
  en.the_block_statement.outer = surrounding_entity ();
  eblock = append_new_entity (en, L"", token.pos);

  create_region_level ();
  append_region (en.the_block_statement.inner);


  // 'out_prefix' is an "out" parameter that can either be null,
  // or denote an already parsed expanded name that is not a type_name.
  parse_local_declarations (out out_pos, out out_prefix);

  check_local_completion (eblock^.the_block_statement.inner^.entities.first);

  parse_statements (out_pos, out_prefix);

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

void parse_statement (TEXT_POSITION pos,
                      PENTITY       prefix,
                      bool          within_if_true_branch);

//=================================================================================

PEXPRESSION parse_an_expression ()
{
  CONTEXT       context;
  PEXPRESSION   exp;

  clear context;
  parse_expression (context, token.pos, null, out exp);

  return exp;
}

//=================================================================================

PEXPRESSION parse_bool_expression ()
{
  PENTITY       t;
  PEXPRESSION   exp;
  TEXT_POSITION pos;

  pos = token.pos;

  exp = parse_an_expression ();

  t = exp^.base_type_or_null;
  if (t != null && t != type_bool)
    semantic_error ("a bool expression is expected here", pos);

  return exp;
}

//=================================================================================

PEXPRESSION parse_bool_parenthesized_expression ()
{
  PEXPRESSION  exp;

  if (token.kind == LEFT_PARENTHESIS)
    get_token ();  // skip '('
  else
    syntax_error ("'(' is expected here", token.pos);

  exp = parse_bool_expression ();

  if (token.kind == RIGHT_PARENTHESIS)
    get_token ();  // skip ')'
  else
    syntax_error ("')' is expected here", token.pos);

  return exp;
}

//=================================================================================

PEXPRESSION parse_parenthesized_expression ()
{
  PEXPRESSION  exp;

  if (token.kind == LEFT_PARENTHESIS)
    get_token ();  // skip '('
  else
    syntax_error ("'(' is expected here", token.pos);

  exp = parse_an_expression ();

  if (token.kind == RIGHT_PARENTHESIS)
    get_token ();  // skip ')'
  else
    syntax_error ("')' is expected here", token.pos);

  return exp;
}

//=================================================================================

void parse_if_statement (bool within_if_true_branch)
{
  PEXPRESSION              exp;
  ENTITY (AN_IF_STATEMENT) en;


  get_token ();  // skip 'if'

  clear en;
  store_location (out en.the_if_statement.loc);

  exp = parse_bool_parenthesized_expression ();

  en.the_if_statement.condition    = exp;
  en.the_if_statement.true_branch  = new_region ();
  en.the_if_statement.false_branch = new_region ();
  en.the_if_statement.outer        = surrounding_entity ();

  (void) append_new_entity (en, L"", token.pos);


  create_region_level ();
  append_region (en.the_if_statement.true_branch);

  parse_statement (token.pos, null, within_if_true_branch => true);

  unlink_region_level ();


  if (token.kind == TOKEN_else)
  {
    if (within_if_true_branch)
      syntax_error ("ambiguous 'else', please enclose if-statement in {}", token.pos);

    get_token();


    create_region_level ();
    append_region (en.the_if_statement.false_branch);

    parse_statement (token.pos, null, within_if_true_branch => false);

    unlink_region_level ();
  }
}

//=================================================================================

bool is_statement_breaking_control_flow (PENTITY e0)
{
  PENTITY e;

  e = e0;

  for (;;)
  {
    if (e == null)
      return false;

    if (e^.kind == A_RETURN_STATEMENT   || e^.kind == A_BREAK_STATEMENT ||
        e^.kind == A_CONTINUE_STATEMENT || e^.kind == AN_ABORT_STATEMENT)
      return true;

    if (e^.kind != A_BLOCK_STATEMENT)
      return false;

    e = e^.the_block_statement.inner^.entities.last;
  }
}

//=================================================================================

package P = new BALANCED_BINARY_TREE (ELEMENT => int8, USER_INFO => bool);

//=================================================================================

void parse_case_alternative (    PENTITY     sw,       // sw = switch_statement entity
                                 PENTITY     sw_type,  // type of switch expression
                             ref BINARY_TREE btree)
{
  CASE_CONSTANT^     list, nl;
  TEXT_POSITION      pos, pos_last_stat;
  PEXPRESSION        exp;
  PENTITY            t;
  int8               case_cte;
  bool               is_valid;
  FLOW_ALTERNATIVE^  fl;
  int                rc;

  list = null;

  while (token.kind == TOKEN_case)
  {
    get_token ();  // skip 'case'

    pos = token.pos;

    exp = parse_an_expression ();
    t = exp^.base_type_or_null;

    case_cte = 0;
    is_valid = false;

    if (!is_constant_exp (exp))
    {
      semantic_error ("a constant expression is expected here", pos);
    }
    else if (sw_type != null && t != null)    // check that types match
    {
      if (sw_type^.kind == AN_ENUMERATION_TYPE)
      {
        if (!types_are_equal (sw_type, t))
          semantic_error ("constant has bad type", pos);
        else
        {
          case_cte = exp^.const_enumeration_value_info.value;

          if (case_cte < 0 || case_cte > sw_type^.the_enumeration_type.last)
            semantic_error ("constant is out of range", pos);
          else
            is_valid = true;
        }
      }
      else if (sw_type^.kind == AN_INTEGER_TYPE)
      {
        if (t^.kind != AN_INTEGER_TYPE)
          semantic_error ("constant has bad type", pos);
        else
        {
          // check that case constant is in switch's type range

          case_cte = exp^.const_integer_value_info.value;
          
          {
            ref INTEGER_INFO pi = INTEGER_DATA[(uint)sw_type^.the_integer_type.type];
            if (case_cte < pi.min || case_cte > pi.max)
              semantic_error ("constant is out of range", pos);
            else
              is_valid = true;
          }
        }
      }
    }


    if (is_valid)
    {
      // update count

      sw^.the_switch_statement.count++;


      // check that all constant values are distinct

      rc = insert_btree (ref btree, case_cte);
      if (rc == 0)
        ;
      else if (rc == BT_DUPLICATE_KEY)
        semantic_error ("constant given twice", pos);
      else
        fatal_compiler_error ("insert_case_key", pos);


      // insert case constant in list

      nl = new CASE_CONSTANT;
      nl^.value = case_cte;
      nl^.next  = list;

      list = nl;
    }


    if (token.kind == COLON)
      get_token ();  // skip ':'
    else
      syntax_error ("':' is expected here", token.pos);
  }



  fl = new FLOW_ALTERNATIVE;
  fl^.is_default = false;
  fl^.cte_list   = list;
  fl^.inner      = new_region ();
  fl^.next       = null;

  create_region_level ();
  append_region (fl^.inner);



  if (sw^.the_switch_statement.first_alt == null)
    sw^.the_switch_statement.first_alt = fl;
  else  
    sw^.the_switch_statement.last_alt^.next = fl;

  sw^.the_switch_statement.last_alt = fl;


  // parse statements

  pos_last_stat = token.pos;

  while (token.kind != TOKEN_case &&
         token.kind != TOKEN_default &&
         token.kind != RIGHT_ACCOLADE &&
         token.kind != LAST_TOKEN)
  {
    pos_last_stat = token.pos;
    parse_statement (token.pos, null, false);
  }


  // the last statement of a case alternative must be either a
  // break, continue, return statement, abort statement or nested block statement.
  // (it is not allowed to "fall through" into the next case)

  if (!is_statement_breaking_control_flow (fl^.inner^.entities.last))
    semantic_error ("break, return or continue is expected as last statement in a case", pos_last_stat);

  unlink_region_level ();
}

//=================================================================================

//  default_case_alternative ::= "default"  ":"
//                                statements

void parse_default_alternative (PENTITY sw)    // sw = switch_statement entity
{
  TEXT_POSITION     pos_last_stat;
  FLOW_ALTERNATIVE^ fl;

  get_token ();  // skip 'default'

  if (token.kind == COLON)
    get_token ();  // skip ':'
  else
    syntax_error ("':' is expected here", token.pos);


  fl = new FLOW_ALTERNATIVE;
  fl^.is_default = true;
  fl^.cte_list   = null;
  fl^.inner      = new_region ();
  fl^.next       = null;

  create_region_level ();
  append_region (fl^.inner);



  if (sw^.the_switch_statement.first_alt == null)
    sw^.the_switch_statement.first_alt = fl;
  else  
    sw^.the_switch_statement.last_alt^.next = fl;

  sw^.the_switch_statement.last_alt = fl;


  // parse statements

  pos_last_stat = token.pos;

  while (token.kind != TOKEN_case &&
         token.kind != TOKEN_default &&
         token.kind != RIGHT_ACCOLADE &&
         token.kind != LAST_TOKEN)
  {
    pos_last_stat = token.pos;
    parse_statement (token.pos, null, false);
  }


  // the last statement of a case alternative must be either a
  // break, continue, return statement, abort statement or nested block statement.
  // (it is not allowed to "fall through" into the next case)

  if (!is_statement_breaking_control_flow (fl^.inner^.entities.last))
    semantic_error ("break, return or continue is expected as last statement in a case", pos_last_stat);

  unlink_region_level ();
}

//=================================================================================

int sw_compare (bool^ user,
                int8  data1,
                int8  data2)
{
  _unused user;
  if (data1 < data2)
    return -1;
  if (data1 > data2)
    return +1;
  return 0;
}

//=================================================================================

void parse_switch_statement ()
{
  TEXT_POSITION               pos;
  PEXPRESSION                 exp;
  PENTITY                     e, t;
  ENTITY (A_SWITCH_STATEMENT) en;
  BINARY_TREE                 tree;

  get_token ();  // skip 'switch'

  pos = token.pos;

  clear en;
  store_location (out en.the_switch_statement.loc);

  exp = parse_parenthesized_expression ();

  t = exp^.base_type_or_null;
  if (t != null && t^.kind != AN_ENUMERATION_TYPE && t^.kind != AN_INTEGER_TYPE)
    semantic_error ("an enumeration or integer expression is expected here", pos);


  en.the_switch_statement.value     = exp;
  en.the_switch_statement.count     = 0;           // nb case constants
  en.the_switch_statement.first_alt = null;
  en.the_switch_statement.last_alt  = null;
  en.the_switch_statement.outer     = surrounding_entity ();

  e = append_new_entity (en, L"", token.pos);


  if (token.kind == LEFT_ACCOLADE)
    get_token ();  // skip '{'
  else
    syntax_error ("'{' is expected here", token.pos);


  create_btree (out tree, null, sw_compare);


  while (token.kind == TOKEN_case)
  {
    parse_case_alternative (e, t, ref tree);
  }


  close_btree (ref tree);


  if (token.kind == TOKEN_default)
  {
    parse_default_alternative (e);
  }
  else
  {
    syntax_error ("'default:' is expected here", token.pos);
  }


  if (token.kind == RIGHT_ACCOLADE)
    get_token ();  // skip '}'
  else
    syntax_error ("'}' is expected here", token.pos);
}

//=================================================================================

void parse_while_statement (bool within_if_true_branch)
{
  TEXT_POSITION              pos;
  PEXPRESSION                exp;
  ENTITY (A_WHILE_STATEMENT) en;

  pos = token.pos;
  get_token ();  // skip 'while'

  clear en;
  store_location (out en.the_while_statement.loc);

  exp = parse_bool_parenthesized_expression ();

  en.the_while_statement.condition = exp;
  en.the_while_statement.inner = new_region ();
  en.the_while_statement.outer = surrounding_entity ();
  en.the_while_statement.pos   = pos;
  
  (void) append_new_entity (en, L"", token.pos);


  create_region_level ();
  append_region (en.the_while_statement.inner);

  parse_statement (token.pos, null, within_if_true_branch);

  unlink_region_level ();
}

//=================================================================================

void parse_for_statement (bool within_if_true_branch)
{
  TEXT_POSITION            pos;
  ENTITY (A_FOR_STATEMENT) en;
  PENTITY                  e;

  pos = token.pos;
  get_token ();  // skip 'for'

  clear en;
  en.the_for_statement.pre        = new_region ();
  en.the_for_statement.exp_region = new_region ();
  en.the_for_statement.condition  = null;
  en.the_for_statement.post       = new_region ();
  en.the_for_statement.inner      = new_region ();
  en.the_for_statement.outer      = surrounding_entity ();
  en.the_for_statement.pos        = pos;

  e = append_new_entity (en, L"", token.pos);

  if (token.kind == LEFT_PARENTHESIS)
    get_token ();  // skip '('
  else
    syntax_error ("'(' is expected here", token.pos);

  if (token.kind != SEMICOLON && token.kind != LAST_TOKEN)
  {
    create_region_level ();
    append_region (en.the_for_statement.pre);

    for (;;)
    {
      // parse assignment statement
      parse_statement_starting_with_exp (token.pos, null, false, false);

      if (token.kind != COMMA)
        break;

      get_token ();  // skip comma
    }

    unlink_region_level ();
  }


  if (token.kind == SEMICOLON)
    get_token ();  // skip ';'
  else
    syntax_error ("';' is expected here", token.pos);


  store_location (out e^.the_for_statement.loc);

  if (token.kind != SEMICOLON)    // there is a condition
  {
    create_region_level ();
    append_region (en.the_for_statement.exp_region);

    e^.the_for_statement.condition = parse_bool_expression ();

    unlink_region_level ();
  }


  if (token.kind == SEMICOLON)
    get_token ();  // skip ';'
  else
    syntax_error ("';' is expected here", token.pos);


  if (token.kind != RIGHT_PARENTHESIS && token.kind != LAST_TOKEN)
  {
    create_region_level ();
    append_region (en.the_for_statement.post);

    for (;;)
    {
      // parse assignment or post/pre statement
      parse_statement_starting_with_exp (token.pos, null, true, false);

      if (token.kind != COMMA)
        break;

      get_token ();  // skip comma
    }

    unlink_region_level ();
  }


  if (token.kind == RIGHT_PARENTHESIS)
    get_token ();  // skip ')'
  else
    syntax_error ("')' is expected here", token.pos);



  create_region_level ();
  append_region (en.the_for_statement.inner);

  parse_statement (token.pos, null, within_if_true_branch);

  unlink_region_level ();
}

//=================================================================================

// 'prefix' can either be null,
// or denote an already parsed expanded name that is not a type_name.

void parse_statement (TEXT_POSITION pos,
                      PENTITY       prefix,
                      bool          within_if_true_branch)
{
  if (prefix != null)
  {
    parse_statement_starting_with_exp (pos, prefix, true, true);
    parse_semicolon ();
  }
  else
  {
    switch (token.kind)
    {
      case SEMICOLON:     // null statement
        parse_semicolon ();
        break;

      case TOKEN_asm:
        parse_asm_statement ();
        parse_semicolon ();
        break;

      case TOKEN_unused:
        parse_unused_statement ();
        parse_semicolon ();
        break;

      case TOKEN_clear:
        parse_clear_statement ();
        parse_semicolon ();
        break;

      case IDENTIFIER:
        {
          TEXT_POSITION pos0;
          PENTITY       e;

          pos0 = token.pos;

          e = parse_expanded_name ();
          if (e == null)    // could not find entity (error already given)
          {
            skip_until (SEMICOLON, true);
            break;
          }
          parse_statement_starting_with_exp (pos0, e, true, true);
          parse_semicolon ();
        }
        break;

      case STAR:                 // *p = 0;
      case PLUS_PLUS:            // ++exp;
      case MINUS_MINUS:          // --exp;
      case LEFT_PARENTHESIS:     // (void)function_call   or   (int *)p = 0;
        parse_statement_starting_with_exp (token.pos, null, true, true);
        parse_semicolon ();
        break;

      case TOKEN_return:
        parse_return_statement ();
        parse_semicolon ();
        break;

      case TOKEN_break:
        parse_break_statement ();
        parse_semicolon ();
        break;

      case TOKEN_continue:
        parse_continue_statement ();
        parse_semicolon ();
        break;

      case TOKEN_free:
        parse_free_statement ();
        parse_semicolon ();
        break;

      case TOKEN_abort:
        parse_abort_statement ();
        parse_semicolon ();
        break;

      case TOKEN_assert:
        parse_assert_statement ();
        parse_semicolon ();
        break;

      case TOKEN_sleep:
        parse_sleep_statement ();
        parse_semicolon ();
        break;

      case LEFT_ACCOLADE:
        parse_block_statement ();
        break;

      case TOKEN_if:
        parse_if_statement (within_if_true_branch);
        break;

      case TOKEN_switch:
        parse_switch_statement ();
        break;

      case TOKEN_while:
        parse_while_statement (within_if_true_branch);
        break;

      case TOKEN_for:
        parse_for_statement (within_if_true_branch);
        break;

      default:
        semantic_error ("statement expected", token.pos);
        parse_semicolon ();
        break;
    }
  }
}

//=================================================================================

// 'prefix' can either be null,
// or denote an already parsed expanded name that is not a type_name.

public
void parse_statements (TEXT_POSITION pos, PENTITY prefix)
{
  if (prefix != null)
    parse_statement (pos, prefix, false);

  while (token.kind != RIGHT_ACCOLADE && token.kind != LAST_TOKEN)
    parse_statement (token.pos, null, false);
}

//=================================================================================
