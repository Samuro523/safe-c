
// lex.c : lexical analyzer for Safe-C compiler - 17 to 28 avril 2010.
//                                           Safe-c version 14/8/2024

from std use console, strings, thread;
use ../common, ../error;
use tokens, symbols, gsymb, wstrings;

/**********************************************************************/

struct CONTEXT 
{
  LEXICAL_ANALYZER_CONTEXT lexa;
  TOKEN_INFO               token;
  TOKEN_INFO               look_ahead_token;
  CONTEXT^                 next;
}

CONTEXT^ pcontext;

/**********************************************************************/

// inits lexa and token.

public
void lex_start_new_context (wstring^ source_text)  // Lnul-terminated
{
  CONTEXT^ p;


  // save context

  p = new CONTEXT;

  p^.lexa             = lexa;
  p^.token            = token;
  p^.look_ahead_token = look_ahead_token;
  p^.next             = pcontext;
  
  pcontext = p;


  // initialize structure

  clear lexa;
  lexa.source_text = source_text;
  lexa.max_length  = source_text^'length;

  lexa.pos = 0;
  lexa.bol = -1;
  lexa.can_be_quote = false;
  lexa.quote_pos = 0;

  lexa.multiline_comment_ended_on_line = 0;
  lexa.directive_on_line = 0;
  lexa.line_of_last_token = 0;
  lexa.within_unsafe = false;
  lexa.if_directive_nesting = 0;

  lexa.global_symbols = new BINARY_TREE;
  create_symbol_btree (out lexa.global_symbols^);
  lexa.local_symbols = new BINARY_TREE;
  create_symbol_btree (out lexa.local_symbols^);

  lexa.look_head_flag = false;

  clear token;
  token.pos.col = 1;
  token.pos.line = 1;

  clear look_ahead_token;
}

/**********************************************************************/

public
void lex_close_context ()
{
  CONTEXT^ p;

  if (pcontext == null)
  {
    printf ("intern error: context not open\n");
    exit (-1);
  }  


  free_symbol_btree (ref lexa.global_symbols^);
  free lexa.global_symbols;
  free_symbol_btree (ref lexa.local_symbols^);
  free lexa.local_symbols;

  // restore context

  lexa             = pcontext^.lexa;
  token            = pcontext^.token;
  look_ahead_token = pcontext^.look_ahead_token;

  p = pcontext;
  pcontext = pcontext^.next;
  free p;
}

/**********************************************************************/

package KEYWORDS

  struct KEYWORD_ENTRY
  {
    wstring    keyword;
    TOKEN_KIND kind;
  }

  const KEYWORD_ENTRY keyword_a[] =
    {{L"abort",  TOKEN_abort},
     {L"assert", TOKEN_assert}};

  const KEYWORD_ENTRY keyword_b[] =
    {{L"body",  TOKEN_body},
     {L"bool",  TOKEN_bool},
     {L"break", TOKEN_break},
     {L"byte",  TOKEN_byte}};

  const KEYWORD_ENTRY keyword_c[] =
    {{L"case",     TOKEN_case},
     {L"char",     TOKEN_char},
     {L"clear",    TOKEN_clear},
     {L"const",    TOKEN_const},
     {L"continue", TOKEN_continue}};

  const KEYWORD_ENTRY keyword_d[] =
    {{L"default",  TOKEN_default},
     {L"double",   TOKEN_double}};

  const KEYWORD_ENTRY keyword_e[] =
    {{L"else",   TOKEN_else},
     {L"end",    TOKEN_end},
     {L"enum",   TOKEN_enum}};

  const KEYWORD_ENTRY keyword_f[] =
    {{L"false",  TOKEN_false},
     {L"float",  TOKEN_float},
     {L"float4", TOKEN_float4},
     {L"float8", TOKEN_float8},
     {L"for",    TOKEN_for},
     {L"free",   TOKEN_free},
     {L"from",   TOKEN_from}};

  const KEYWORD_ENTRY keyword_g[] =
    {{L"generic", TOKEN_generic}};

  const KEYWORD_ENTRY keyword_h[] =
    {};

  const KEYWORD_ENTRY keyword_i[] =
    {{L"if",     TOKEN_if},
     {L"inline", TOKEN_inline},
     {L"int",    TOKEN_int},
     {L"int1",   TOKEN_int1},
     {L"int2",   TOKEN_int2},
     {L"int4",   TOKEN_int4},
     {L"int8",   TOKEN_int8}};

  const KEYWORD_ENTRY keyword_j[] =
    {};

  const KEYWORD_ENTRY keyword_k[] =
    {};

  const KEYWORD_ENTRY keyword_l[] =
    {{L"long", TOKEN_long}};

  const KEYWORD_ENTRY keyword_m[] =
    {};

  const KEYWORD_ENTRY keyword_n[] =
    {{L"new",    TOKEN_new},
     {L"nul",    TOKEN_nul},
     {L"null",   TOKEN_null}};

  const KEYWORD_ENTRY keyword_o[] =
    {{L"object",  TOKEN_object},
     {L"out",     TOKEN_out}};

  const KEYWORD_ENTRY keyword_p[] =
    {{L"package",  TOKEN_package},
     {L"packed",   TOKEN_packed},
     {L"public",   TOKEN_public}};

  const KEYWORD_ENTRY keyword_q[] =
    {};

  const KEYWORD_ENTRY keyword_r[] =
    {{L"ref",      TOKEN_ref},
     {L"return",   TOKEN_return},
     {L"run",      TOKEN_run}};

  const KEYWORD_ENTRY keyword_s[] =
    {{L"short",    TOKEN_short},
     {L"sleep",    TOKEN_sleep},
     {L"string",   TOKEN_string},
     {L"struct",   TOKEN_struct},
     {L"switch",   TOKEN_switch}};

  const KEYWORD_ENTRY keyword_t[] =
    {{L"tiny",     TOKEN_tiny},
     {L"true",     TOKEN_true},
     {L"typedef",  TOKEN_typedef}};

  const KEYWORD_ENTRY keyword_u[] =
    {{L"uint",    TOKEN_uint},
     {L"uint1",   TOKEN_uint1},
     {L"uint2",   TOKEN_uint2},
     {L"uint4",   TOKEN_uint4},
     {L"union",   TOKEN_union},
     {L"use",     TOKEN_use},
     {L"ushort",  TOKEN_ushort}};

  const KEYWORD_ENTRY keyword_v[] =
    {{L"void",     TOKEN_void},
     {L"volatile", TOKEN_volatile}};

  const KEYWORD_ENTRY keyword_w[] =
    {{L"wchar",    TOKEN_wchar},
     {L"while",    TOKEN_while},
     {L"wstring",  TOKEN_wstring}};

  const KEYWORD_ENTRY keyword_x[] =
    {};

  const KEYWORD_ENTRY keyword_y[] =
    {};

  const KEYWORD_ENTRY keyword_z[] =
    {};

  const KEYWORD_ENTRY[] KEYWORD_TABLE[26] =
    {keyword_a, keyword_b, keyword_c, keyword_d, keyword_e,
     keyword_f, keyword_g, keyword_h, keyword_i, keyword_j,
     keyword_k, keyword_l, keyword_m, keyword_n, keyword_o,
     keyword_p, keyword_q, keyword_r, keyword_s, keyword_t,
     keyword_u, keyword_v, keyword_w, keyword_x, keyword_y,
     keyword_z};

  const KEYWORD_ENTRY DIRECTIVES_TABLE[] =
    {{L"#begin",   TOKEN_DIRECTIVE_begin},
     {L"#define",  TOKEN_DIRECTIVE_define},
     {L"#elif",    TOKEN_DIRECTIVE_elif},
     {L"#else",    TOKEN_DIRECTIVE_else},
     {L"#end",     TOKEN_DIRECTIVE_end},
     {L"#endif",   TOKEN_DIRECTIVE_endif},
     {L"#error",   TOKEN_DIRECTIVE_error},
     {L"#if",      TOKEN_DIRECTIVE_if},
     {L"#warning", TOKEN_DIRECTIVE_warning}};

end KEYWORDS;

/**********************************************************************/

package SEQUENCES

  struct SEQUENCE
  {
    wchar character;
    uint2 code;
  }

  const SEQUENCE SEQUENCE_TABLE[] =
    {{L'\'', 0x0027},  // Single quote
     {L'\"', 0x0022},  // Double quote
     {L'\\', 0x005C},  // Backslash
     {L'0',  0x0000},  // Null
     {L'a',  0x0007},  // Alert
     {L'b',  0x0008},  // Backspace
     {L'f',  0x000C},  // Form feed
     {L'n',  0x000A},  // New line
     {L'r',  0x000D},  // Carriage return
     {L't',  0x0009},  // Horizontal tab
     {L'v',  0x000B},  // Vertical tab
  };

end SEQUENCES;

/**********************************************************************/

int parse_escape_sequence (bool is_wide)
{
  wchar         c;
  int           i, length, value, digit;
  TEXT_POSITION pos;
  bool          errors_found;

  lexa.pos++;    // skip \

  c = lexa.source_text^[lexa.pos];

  for (i=0; i<SEQUENCE_TABLE'length; i++)  
  {
    if (SEQUENCE_TABLE[i].character == c)
    {
      lexa.pos++;    // skip character
      return SEQUENCE_TABLE[i].code;
    }
  }

  clear pos;
  pos.line = token.pos.line;
  pos.col = lexa.pos - lexa.bol;

  if (c != L'x')
  {
    lexical_error ("illegal escape sequence", pos);

    // skip illegal character
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9'))
      lexa.pos++;

    return 0;
  }


  // parse hexadecimal sequence

  lexa.pos++;    // skip x

  if (is_wide)
    length = 4;
  else
    length = 2;

  errors_found = false;

  value = 0;

  for (i=0; i<length; i++)
  {
    c = lexa.source_text^[lexa.pos];

    if (c >= L'0' && c <= L'9')
      digit = (int)c - (int)L'0';
    else if (c >= L'A' && c <= L'F')
      digit = (int)c - (int)L'A' + 10;
    else if (c >= L'a' && c <= L'f')
      digit = (int)c - (int)L'a' + 10;
    else
    {
      digit = -1;
      errors_found = true;
    }

    if (digit >= 0)
    {
      lexa.pos++;
      value = value * 16 + digit;
    }
  }

  if (errors_found)
    lexical_error ("illegal escape sequence", pos);

  return value;
}

/**********************************************************************/

// current position at : L' or '

void scan_character_literal ()
{
  wchar c;
  bool  can_be_quote;

  token.kind = CHAR_LITERAL;

  c = lexa.source_text^[lexa.pos];
  if (c == L'L')
  {
    token.info._char.has_prefix_L = true;
    lexa.pos++;
    can_be_quote = false;
  }
  else
  {
    token.info._char.has_prefix_L = false;
    can_be_quote = true;
  }

  lexa.pos++;    // skip quote

  lexa.quote_pos = lexa.pos;    // save position in case of conversion into apostrophe


  // parse either escape sequence or printable character

  c = lexa.source_text^[lexa.pos];

  if (c == L'\\')   // escape sequence
  {
    c = (wchar) parse_escape_sequence (token.info._char.has_prefix_L);
    can_be_quote = false;
  }
  else
  {
    if ((!token.info._char.has_prefix_L) && ((int2)c) > 255)
    {
      c = Lnul;
      lexical_error ("unicode character requires L prefix", token.pos);
      can_be_quote = false;
    }

    lexa.pos++;
  }

  token.info._char.value = (uint2)c;

  c = lexa.source_text^[lexa.pos];
  if (c == L'\'')
    lexa.pos++;
  else
  {
    lexical_error ("malformed character literal", token.pos);
    can_be_quote = false;
  }

  lexa.can_be_quote = can_be_quote;
}

/**********************************************************************/

public
void lex_force_into_apostrophe ()
{
  if (token.kind == CHAR_LITERAL && lexa.can_be_quote)
  {
    lexa.pos = lexa.quote_pos;
    lexa.can_be_quote = false;
    token.kind = APOSTROPHE;
  }
}

/**********************************************************************/

public
void lex_set_in_statement (bool flag)
{
  lexa.in_statement = flag;
}

/**********************************************************************/

void scan_string_literal ()
{
  wchar c;
  int   length;
  bool  unicode_error = false;

  token.kind = STRING_LITERAL;
  token.info._string.length = 0;

  c = lexa.source_text^[lexa.pos];
  if (c == L'L')
  {
    token.info._string.has_prefix_L = true;
    lexa.pos++;
  }
  else
  {
    token.info._string.has_prefix_L = false;
  }

  lexa.pos++;    // skip quotation mark


  // parse either escape sequence, printable character, or quotation mark

  length = 0;

  for (;;)
  {
    c = lexa.source_text^[lexa.pos];
    if (c == Lnul || c == (wchar)13 || c == (wchar)10)
    {
      lexical_error ("string literal not closed", token.pos);
      break;
    }

    if (c == L'\"')
    {
      lexa.pos++;    // skip quotation mark
      break;
    }

    if (c < L' ')   // illegal characters
    {
      lexical_error ("string literal contains control characters", token.pos);
      for (;;)
      {
        c = lexa.source_text^[lexa.pos];
        if (c == Lnul || c == (wchar)10)
          break;
        if (c == L'\"')
        {
          lexa.pos++;    // skip quotation mark
          break;
        }

        lexa.pos++;
      }
      break;
    }

    if (c == L'\\')   // escape sequence
    {
      c = (wchar) parse_escape_sequence (token.info._string.has_prefix_L);
    }
    else
    {
      if ((!token.info._string.has_prefix_L) && ((int2)c) > 255)
      {
        c = Lnul;
        unicode_error = true;
      }

      lexa.pos++;
    }

    if (length < MAX_STRING_LITERAL_LENGTH)
    {
      token.info._string.value[length] = c;
      length++;
      token.info._string.length = length;
    }
    else if (length == MAX_STRING_LITERAL_LENGTH)
    {
      char str[256];
      sprintf (out str, "string literal is too long (max %d characters)", MAX_STRING_LITERAL_LENGTH);
      lexical_error (str, token.pos);
      length++;
    }
  }

  if (unicode_error)
    lexical_error ("unicode characters require an L prefix", token.pos);
}

/**********************************************************************/

public
bool isinfinite (double d)
{
  int8 h;
  h'byte = d'byte;
  return (h & 0x7FF0000000000000) == 0x7FF0000000000000;
}

/**********************************************************************/

public
bool fisinfinite (float f)
{
  uint4 u;
  u'byte = f'byte;
  return (u & 0x7F800000) == 0x7F800000;
}

/**********************************************************************/

//  digit_sequence  "."  digit_sequence
//     [("e"|"E") ["+"|"-"] digit_sequence] ['F'|'f'|'D'|'d']

void scan_base10_float_literal ()
{
  bool    dot_found, digit_found, syntax_error_found, overflow, underflow, correction_factor;
  int     leading_zeroes_after_dot, digits_before, digits_after, len;
  int8    mantissa;
  int     exponent, exp_sign, k;
  double  fexponent, result;
  wchar   c;

  dot_found = false;
  syntax_error_found = false;
  overflow = false;
  underflow = false;
  leading_zeroes_after_dot = 0;
  digits_before = 0;    // nb non-zero digits before dot
  digits_after  = 0;    // nb digits after dot


  // parse leading zeroes

  while (lexa.source_text^[lexa.pos] == L'0')
  {
    lexa.pos++;

    if (dot_found)
    {
      digits_after++;
      leading_zeroes_after_dot++;
    }

    if (lexa.source_text^[lexa.pos] == L'_')
    {
      lexa.pos++;
      c = lexa.source_text^[lexa.pos];
      if (c < L'0' || c > L'9')
        syntax_error_found = true;
    }
    else if (lexa.source_text^[lexa.pos] == L'.' && (!dot_found))
    {
      dot_found = true;
      lexa.pos++;
      c = lexa.source_text^[lexa.pos];
      if (c < L'0' || c > L'9')
        syntax_error_found = true;
    }
  }


  // get first 18 digits into mantissa

  digit_found = true;  // if we find no digit it's not an error

  mantissa = 0;
  len = 0;

  for (;;)
  {
    c = lexa.source_text^[lexa.pos];

    if (c < L'0' || c > L'9')
      break;

    digit_found = true;

    if (len < 18)
    {
      mantissa = mantissa * 10 + ((int)c - (int)L'0');
      len++;
    }

    lexa.pos++;

    if (dot_found)
      digits_after++;
    else
      digits_before++;

    if (lexa.source_text^[lexa.pos] == L'_')
    {
      lexa.pos++;
      digit_found = false;
    }
    else if (lexa.source_text^[lexa.pos] == L'.' && (!dot_found))
    {
      dot_found = true;
      lexa.pos++;
      digit_found = false;
    }
  }

  if (!digit_found)
    syntax_error_found = true;

  if (digits_after == 0)   // no digits after dot, or no dot
    syntax_error_found = true;


  exponent = 0;

  if (lexa.source_text^[lexa.pos] == L'e' || lexa.source_text^[lexa.pos] == L'E')
  {
    lexa.pos++;

    if (lexa.source_text^[lexa.pos] == L'-')
    {
      lexa.pos++;
      exp_sign = -1;
    }
    else
    {
      if (lexa.source_text^[lexa.pos] == L'+')
        lexa.pos++;
      exp_sign = +1;
    }

    // check start digit

    digit_found = false;

    while (lexa.source_text^[lexa.pos] >= L'0' && lexa.source_text^[lexa.pos] <= L'9')
    {
      digit_found = true;

      exponent = exponent * 10 + ((int)lexa.source_text^[lexa.pos] - (int)L'0');

      if (exponent >= 512)
        overflow = true;

      lexa.pos++;

      if (lexa.source_text^[lexa.pos] == L'_')
      {
        lexa.pos++;
        digit_found = false;
      }
    }

    if (!digit_found)
      syntax_error_found = true;

    if (exp_sign < 0)
      exponent = -exponent;
  }


  // parsing is finished, now compute double result


  exponent += (digits_before - len - leading_zeroes_after_dot);

  if (exponent < 0)
  {
    exponent = -exponent;
    exp_sign = -1;

    correction_factor = false;
    if (exponent > 256)   // fexponent might overflow (at 1.7E+308)
    {
      exponent -= 128;
      correction_factor = true;   // correction factor is 1.0E128
    }
  }
  else
  {
    correction_factor = false;
    exp_sign = +1;
  }


  fexponent = 1.0;

  if (exponent >= 512)   // prevent table overflow
    overflow = true;
  else
  {
    const int8 power10[9] = {4621819117588971520,   // 1.0e1
                             4636737291354636288,   // 1.0e2
                             4666723172467343360,   // 1.0e4
                             4726483295884279808,   // 1.0e8
                             4846369599423283200,   // 1.0e16
                             5085611494797045271,   // 1.0e32
                             5564284217833028085,   // 1.0e64
                             6521906365687930162,   // 1.0e128
                             8436737289693151036};  // 1.0e256

    k = 0;
    while (exponent > 0)
    {
      if ((exponent & 1) != 0)
      {
        double dvalue;
        dvalue'byte = power10[k]'byte;
        fexponent *= dvalue;
        exponent--;
      }

      exponent >>= 1;
      k++;
    }
  }


  if (exp_sign >= 0)
  {
    result = (double)mantissa * fexponent;
    if (isinfinite (result))
      overflow = true;
  }
  else
  {
    if (correction_factor)
    {
      result = ((double)mantissa / 1.0E128) / fexponent;
      if (result == 0.0 && mantissa > 0)
        underflow = true;
    }
    else
      result = (double)mantissa / fexponent;
  }

  if (lexa.source_text^[lexa.pos] == L'd' || lexa.source_text^[lexa.pos] == L'D')
  {
    token.info._float.suffix = 'D';
    lexa.pos++;
  }
  else if (lexa.source_text^[lexa.pos] == L'f' || lexa.source_text^[lexa.pos] == L'F')
  {
    float f = (float)result;
    if (fisinfinite (f))
      overflow = true;
    else if (f == 0.0 && mantissa > 0)
      underflow = true;
    result = f;    // reduce precision (compiler note: be careful that this is not optimized away)
    token.info._float.suffix = 'F';
    lexa.pos++;
  }
  else
  {
    token.info._float.suffix = nul;
  }

  if (syntax_error_found)
  {
    lexical_error ("syntax error", token.pos);
  }
  else if (overflow)
  {
    lexical_error ("overflow", token.pos);
    result = 1.0;
  }
  else if (underflow)
  {
    lexical_error ("underflow", token.pos);
  }

  token.kind = FLOAT_LITERAL;
  token.info._float.value = result;
}

/**********************************************************************/

// "0x" hex_digit_sequence  "."  hex_digit_sequence
//     [("p"|"P") ["+"|"-"] digit_sequence] ['F'|'f'|'D'|'d']

void scan_base16_float_literal ()
{
  bool    dot_found, digit_found, syntax_error_found, overflow, underflow, correction_factor;
  int     leading_zeroes_after_dot, digit, digits_before, digits_after, len;
  int8    mantissa;
  int     exponent, exp_sign;
  double  fbase, fexponent, result;
  wchar   c;

  dot_found = false;
  syntax_error_found = false;
  overflow = false;
  underflow = false;
  leading_zeroes_after_dot = 0;
  digits_before = 0;    // nb non-zero digits before dot
  digits_after  = 0;    // nb digits after dot


  c = lexa.source_text^[lexa.pos];
  if ((c < L'0' || c > L'9') && (c < L'A' || c > L'F') && (c < L'a' || c > L'f'))
    syntax_error_found = true;


  // parse leading zeroes

  while (lexa.source_text^[lexa.pos] == L'0')
  {
    lexa.pos++;

    if (dot_found)
    {
      digits_after++;
      leading_zeroes_after_dot++;
    }

    if (lexa.source_text^[lexa.pos] == L'_')
    {
      lexa.pos++;
      digit_found = false;
      c = lexa.source_text^[lexa.pos];
      if ((c < L'0' || c > L'9') && (c < L'A' || c > L'F') && (c < L'a' || c > L'f'))
        syntax_error_found = true;
    }
    else if (lexa.source_text^[lexa.pos] == L'.' && (!dot_found))
    {
      dot_found = true;
      lexa.pos++;
      c = lexa.source_text^[lexa.pos];
      if ((c < L'0' || c > L'9') && (c < L'A' || c > L'F') && (c < L'a' || c > L'f'))
        syntax_error_found = true;
    }
  }


  // get first 15 hex digits into mantissa

  digit_found = true;   // if we find no digit, it's ok

  mantissa = 0;
  len = 0;

  for (;;)
  {
    c = lexa.source_text^[lexa.pos];

    if (c >= L'0' && c <= L'9')
      digit = (int)c - (int)L'0';
    else if (c >= L'A' && c <= L'F')
      digit = (int)c - (int)L'A' + 10;
    else if (c >= L'a' && c <= L'f')
      digit = (int)c - (int)L'a' + 10;
    else
      break;

    digit_found = true;

    if (len < 15)
    {
      mantissa = mantissa * 16 + digit;
      len++;
    }

    lexa.pos++;

    if (dot_found)
      digits_after++;
    else
      digits_before++;

    if (lexa.source_text^[lexa.pos] == L'_')
    {
      lexa.pos++;
      digit_found = false;
    }
    else if (lexa.source_text^[lexa.pos] == L'.' && (!dot_found))
    {
      dot_found = true;
      lexa.pos++;
      digit_found = false;
    }
  }

  if (!digit_found)
    syntax_error_found = true;

  if (digits_after == 0)   // no digits after dot, or no dot
    syntax_error_found = true;


  exponent = 0;

  if (lexa.source_text^[lexa.pos] == L'p' || lexa.source_text^[lexa.pos] == L'P')
  {
    lexa.pos++;

    if (lexa.source_text^[lexa.pos] == L'-')
    {
      lexa.pos++;
      exp_sign = -1;
    }
    else
    {
      if (lexa.source_text^[lexa.pos] == L'+')
        lexa.pos++;
      exp_sign = +1;
    }

    // check start digit

    digit_found = false;

    while (lexa.source_text^[lexa.pos] >= L'0' && lexa.source_text^[lexa.pos] <= L'9')
    {
      digit_found = true;

      exponent = exponent * 10 + ((int)lexa.source_text^[lexa.pos] - (int)L'0');

      if (exponent >= 1100)
        overflow = true;

      lexa.pos++;

      if (lexa.source_text^[lexa.pos] == L'_')
      {
        lexa.pos++;
        digit_found = false;
      }
    }

    if (!digit_found)
      syntax_error_found = true;

    if (exp_sign < 0)
      exponent = -exponent;
  }


  // parsing is finished, now compute double result


  exponent += (digits_before - len - leading_zeroes_after_dot) * 4;

  if (exponent < 0)
  {
    exponent = -exponent;
    exp_sign = -1;

    correction_factor = false;
    if (exponent > 900)   // fexponent might overflow at -1024
    {
      exponent -= 62;
      correction_factor = true;   // correction factor is 2^62
    }
  }
  else
  {
    correction_factor = false;
    exp_sign = +1;
  }


  fexponent = 1.0;

  if (exponent >= 1100)    // approx. -1024 .. +1023
    overflow = true;
  else
  {
    fbase = 2.0;

    while (exponent > 0)
    {
      if ((exponent & 1) != 0)
      {
        fexponent *= fbase;
        exponent--;
      }

      exponent >>= 1;
      fbase = fbase * fbase;
    }
  }


  if (exp_sign >= 0)
  {
    result = (double)mantissa * fexponent;
    if (isinfinite (result))
      overflow = true;
  }
  else
  {
    if (correction_factor)
    {
      result = ((double)mantissa / (double)(((int8)1) << 62)) / fexponent;
      if (result == 0.0 && mantissa > 0)
        underflow = true;
    }
    else
      result = (double)mantissa / fexponent;
  }

  if (lexa.source_text^[lexa.pos] == L'd' || lexa.source_text^[lexa.pos] == L'D')
  {
    token.info._float.suffix = 'D';
    lexa.pos++;
  }
  else if (lexa.source_text^[lexa.pos] == L'f' || lexa.source_text^[lexa.pos] == L'F')
  {
    float f = (float)result;
    if (fisinfinite (f))
      overflow = true;
    else if (f == 0.0 && mantissa > 0)
      underflow = true;
    result = f;      // reduce precision
    token.info._float.suffix = 'F';
    lexa.pos++;
  }
  else
  {
    token.info._float.suffix = nul;
  }

  if (syntax_error_found)
  {
    lexical_error ("syntax error", token.pos);
  }
  else if (overflow)
  {
    lexical_error ("overflow", token.pos);
    result = 1.0;
  }
  else if (underflow)
  {
    lexical_error ("underflow", token.pos);
  }

  token.kind = FLOAT_LITERAL;
  token.info._float.value = result;
}

/**********************************************************************/

void scan_base10_integer_literal ()
{
  wchar   c;
  int8    value;
  int     digit;
  bool    overflow = false;
  bool    digit_found = false;

  value = 0;

  for (;;)
  {
    c = lexa.source_text^[lexa.pos];
    if (c >= L'0' && c <= L'9')
      digit = (int)c - (int)L'0';
    else
      break;

    digit_found = true;

    if (value > 9223372036854775807 / 10 ||
        value * 10 > 9223372036854775807 - digit)
      overflow = true;
    else
      value = value * 10 + digit;

    lexa.pos++;

    if (lexa.source_text^[lexa.pos] == L'_')
    {
      lexa.pos++;
      digit_found = false;
    }
  }

  token.kind = INTEGER_LITERAL;
  token.info._integer.value = value;

  if (c == L'L')  // L suffix
  {
    token.info._integer.has_suffix_L = true;
    lexa.pos++;
  }
  else
    token.info._integer.has_suffix_L = false;

  if (!digit_found)
    lexical_error ("syntax error", token.pos);
  else if (overflow)
    lexical_error ("overflow", token.pos);
}

/**********************************************************************/

void scan_base16_integer_literal ()
{
  wchar   c;
  int8    value;
  int     digit;
  bool    overflow = false;
  bool    digit_found = false;

  value = 0;

  for (;;)
  {
    c = lexa.source_text^[lexa.pos];
    if (c >= L'0' && c <= L'9')
      digit = (int)c - (int)L'0';
    else if (c >= L'A' && c <= L'F')
      digit = (int)c - (int)L'A' + 10;
    else if (c >= L'a' && c <= L'f')
      digit = (int)c - (int)L'a' + 10;
    else
      break;

    digit_found = true;

    if (value > 9223372036854775807 / 16 ||
        value * 16 > 9223372036854775807 - digit)
      overflow = true;
    else
      value = value * 16 + digit;

    lexa.pos++;

    if (lexa.source_text^[lexa.pos] == L'_')
    {
      lexa.pos++;
      digit_found = false;
    }
  }

  token.kind = INTEGER_LITERAL;
  token.info._integer.value = value;

  if (c == L'L')  // L suffix
  {
    token.info._integer.has_suffix_L = true;
    lexa.pos++;
  }
  else
    token.info._integer.has_suffix_L = false;

  if (!digit_found)
    lexical_error ("syntax error", token.pos);
  else if (overflow)
    lexical_error ("overflow", token.pos);
}

/**********************************************************************/

void scan_base2_integer_literal ()
{
  wchar   c;
  int8    value;
  int     digit;
  bool    overflow = false;
  bool    digit_found = false;
  bool    bad_digit = false;

  value = 0;

  for (;;)
  {
    c = lexa.source_text^[lexa.pos];
    if (c >= L'0' && c <= L'9')
      digit = (int)c - (int)L'0';
    else
      break;

    digit_found = true;

    if (digit > 1)
    {
      bad_digit = true;
      digit = 0;
    }

    if (value > 9223372036854775807 / 2 ||
        value * 2 > 9223372036854775807 - digit)
      overflow = true;
    else
      value = value * 2 + digit;

    lexa.pos++;

    if (lexa.source_text^[lexa.pos] == L'_')
    {
      lexa.pos++;
      digit_found = false;
    }
  }

  token.kind = INTEGER_LITERAL;
  token.info._integer.value = value;

  if (c == L'L')  // L suffix
  {
    token.info._integer.has_suffix_L = true;
    lexa.pos++;
  }
  else
    token.info._integer.has_suffix_L = false;

  if (!digit_found)
    lexical_error ("syntax error", token.pos);
  else if (overflow)
    lexical_error ("overflow", token.pos);
  else if (bad_digit)
    lexical_error ("bad digit in binary literal", token.pos);
}

/**********************************************************************/

void scan_integer_or_float_literal ()
{
  wchar c;
  int   x;

  c = lexa.source_text^[lexa.pos];
  if (c == L'0' && lexa.source_text^[lexa.pos+1] == L'x')  // hexadecimal
  {
    lexa.pos += 2;

    // test if it's an integer or a float
    x = lexa.pos;
    for (;;)
    {
      c = lexa.source_text^[x++];
      if (c >= L'0' && c <= L'9')
        continue;
      if (c >= L'A' && c <= L'F')
        continue;
      if (c >= L'a' && c <= L'f')
        continue;
      if (c == L'_')
        continue;
      break;
    }

    if (c == L'.')
      scan_base16_float_literal ();
    else
      scan_base16_integer_literal ();
  }
  else if (c == L'0' && lexa.source_text^[lexa.pos+1] == L'b')  // binary
  {
    lexa.pos += 2;
    scan_base2_integer_literal ();
  }
  else   // decimal
  {
    if (c == L'0' && lexa.source_text^[lexa.pos+1] >= L'0' && lexa.source_text^[lexa.pos+1] <= L'9')
      warning ("octal numbers are not supported : they are parsed in decimal", token.pos);

    // test if it's an integer or a float
    x = lexa.pos;
    for (;;)
    {
      c = lexa.source_text^[x++];
      if (c >= L'0' && c <= L'9')
        continue;
      if (c == L'_')
        continue;
      break;
    }

    if (c == L'.')
      scan_base10_float_literal ();
    else
      scan_base10_integer_literal ();
  }
}

/**********************************************************************/

bool looks_like_character_literal ()
{
  return (lexa.source_text^[lexa.pos+1] == L'\\' ||    // escape sequence
           (lexa.source_text^[lexa.pos+1] >= L' ' && lexa.source_text^[lexa.pos+2] == L'\''));
}

/**********************************************************************/

void parse_multiline_comment ()
{
  bool          was_on_directive, newlines_found;
  TEXT_POSITION start;
  wchar         c;

  was_on_directive = (lexa.directive_on_line == token.pos.line);
  newlines_found = false;
  start = token.pos;

  for (;;)
  {
    lexa.pos++;

    c = lexa.source_text^[lexa.pos];

    if (c == (wchar)10)   // new line
    {
      if (was_on_directive && (!newlines_found))
      {
        lexical_error ("multi-line comment not allowed after a preprocessor directive", start);
        newlines_found = true;   // avoid further errors
      }

      lexa.bol = lexa.pos;
      token.pos.line++;
    }
    else if (c == L'/' && lexa.source_text^[lexa.pos+1] == L'*')
    {
      token.pos.col = lexa.pos - lexa.bol;
      warning ("possible nested comment", token.pos);
    }
    else if (c == L'*' && lexa.source_text^[lexa.pos+1] == L'/')
    {
      lexa.pos += 2;
      break;
    }
    else if (c == Lnul)
    {
      if (lexa.pos+1 == lexa.max_length)
      {
        lexical_error ("comment not closed", start);
        break;
      }
      else   // illegal character
      {
        token.pos.col = lexa.pos - lexa.bol;
        lexical_error ("illegal character", token.pos);
      }
    }
  }

  lexa.multiline_comment_ended_on_line = token.pos.line;

  if (was_on_directive)
    lexa.directive_on_line = token.pos.line;
}

/**********************************************************************/

void lex_get_token ()
{
  wchar c;

  for (;;)
  {
    for (;;)    // skip white space, newlines and illegal characters
    {
      for (;;)      // skip spaces
      {
        c = lexa.source_text^[lexa.pos];
        if (c != L' ')
          break;
        lexa.pos++;
      }

      if (c > L' ')
        break;

      if (c == (wchar)10)   // newline
      {
        if (lexa.directive_on_line == token.pos.line)
        {
          lexa.directive_on_line = 0;
          token.pos.col = lexa.pos - lexa.bol;
          token.kind = TOKEN_END_OF_DIRECTIVE;
          return;
        }

        lexa.bol = lexa.pos;
        token.pos.line++;
      }
      else if (c == Lnul)
      {
        if (lexa.pos+1 == lexa.max_length)
        {
          if (lexa.directive_on_line == token.pos.line)
          {
            lexa.directive_on_line = 0;
            token.pos.col = lexa.pos - lexa.bol;
            token.kind = TOKEN_END_OF_DIRECTIVE;
            return;
          }

          token.kind = LAST_TOKEN;
          token.pos.col = 1;
          token.pos.line++;
          return;
        }
        else   // illegal character
        {
          token.pos.col = lexa.pos - lexa.bol;
          lexical_error ("illegal character", token.pos);
        }
      }

      lexa.pos++;    // skip character
    }


    // set correct column for next token
    token.pos.col = lexa.pos - lexa.bol;


    // we have a character larger than space

    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c == L'_'))
    {
      int start, len;

      // identifier, keyword, character or string literal with L prefix.

      start = lexa.pos;

      for (;;)
      {
        c = lexa.source_text^[++lexa.pos];

        if (c >= L'a' && c <= L'z')
          continue;

        if (c >= L'A' && c <= L'Z')
          continue;

        if (c >= L'0' && c <= L'9')
          continue;

        if (c == L'_')
          continue;

        break;
      }

      len = lexa.pos - start;

      if (len == 1 && lexa.source_text^[start] == L'L')  // L
      {
        if (c == L'\'' && looks_like_character_literal())
        {
          lexa.pos--;   // undo L prefix
          scan_character_literal ();
          return;
        }

        if (c == L'"')
        {
          lexa.pos--;
          scan_string_literal ();
          return;
        }
      }


      // test if it is a keyword (they always start with a lower case letter)

      if (lexa.source_text^[start] >= L'a' && lexa.source_text^[start] <= L'z')
      {
        ref KEYWORD_ENTRY[] e = KEYWORD_TABLE[(int)lexa.source_text^[start] - (int)L'a'];
        int                 i;

        for (i=0; i<e'length; i++)
        {
          if (e[i].keyword'length == len &&
              memcmp (e[i].keyword, lexa.source_text^[start:len]) == 0)
          {
            token.kind = e[i].kind;
            return;
          }
        }
      }

      // extra test for Lnul (it's not in the keyword table because
      // it starts with an uppercase letter)

      if (len == 4 &&
          lexa.source_text^[start] == L'L' &&
          lexa.source_text^[start+1] == L'n' &&
          lexa.source_text^[start+2] == L'u' &&
          lexa.source_text^[start+3] == L'l')
      {
        token.kind = TOKEN_Lnul;
        return;
      }


      // extra test for _asm

      if (len == 4 &&
          lexa.source_text^[start] == L'_' &&
          lexa.source_text^[start+1] == L'a' &&
          lexa.source_text^[start+2] == L's' &&
          lexa.source_text^[start+3] == L'm')
      {
        token.kind = TOKEN_asm;
        return;
      }


      // extra test for _unused

      if (len == 7 &&
          lexa.source_text^[start] == L'_' &&
          lexa.source_text^[start+1] == L'u' &&
          lexa.source_text^[start+2] == L'n' &&
          lexa.source_text^[start+3] == L'u' &&
          lexa.source_text^[start+4] == L's' &&
          lexa.source_text^[start+5] == L'e' &&
          lexa.source_text^[start+6] == L'd')
      {
        token.kind = TOKEN_unused;
        return;
      }


      if (len > MAX_IDENTIFIER_LENGTH)
      {
        char str[256];
        sprintf (out str, "identifier exceeds %d characters", MAX_IDENTIFIER_LENGTH);
        lexical_error (str, token.pos);
        len = MAX_IDENTIFIER_LENGTH;
      }

      token.kind = IDENTIFIER;
      token.info._identifier.value[0:len] = lexa.source_text^[start:len];
      if (len < MAX_IDENTIFIER_LENGTH)
        token.info._identifier.value[len] = Lnul;
      return;
    }
    else if (c >= L'0' && c <= L'9')
    {
      scan_integer_or_float_literal ();
      return;
    }
    else
    {
      switch (c)
      {
        case L'!':
          lexa.pos++;

          c = lexa.source_text^[lexa.pos];
          if (c == L'=')
          {
            lexa.pos++;
            token.kind = NOT_EQUAL;
          }
          else
          {
            token.kind = NOT;
          }
          return;

        case L'\"':    // string literal
          scan_string_literal ();
          return;

        case L'#':     // preprocessor directive
        {
          int start, len, i;

          start = lexa.pos;

          for (;;)
          {
            c = lexa.source_text^[++lexa.pos];

            if (c >= L'a' && c <= L'z')
              continue;

            if (c >= L'A' && c <= L'Z')
              continue;

            if (c >= L'0' && c <= L'9')
              continue;

            if (c == L'_')
              continue;

            break;
          }

          len = lexa.pos - start;


          if (lexa.multiline_comment_ended_on_line == token.pos.line)
            lexical_error ("preprocessor directive not allowed after a comment", token.pos);

          lexa.directive_on_line = token.pos.line;

          for (i=0; i<DIRECTIVES_TABLE'length; i++)
          {
            if (DIRECTIVES_TABLE[i].keyword'length == len &&
                memcmp (DIRECTIVES_TABLE[i].keyword, lexa.source_text^[start:len]) == 0)
            {
              token.kind = DIRECTIVES_TABLE[i].kind;
              return;
            }
          }

          token.kind = TOKEN_UNKNOWN_DIRECTIVE;
        }
        return;


        case L'%':
          lexa.pos++;

          c = lexa.source_text^[lexa.pos];
          if (c == L'=')
          {
            lexa.pos++;
            token.kind = PERCENT_EQUAL;
          }
          else
          {
            token.kind = PERCENT;
          }
          return;


        case L'&':
          lexa.pos++;

          c = lexa.source_text^[lexa.pos];
          if (c == L'&')
          {
            lexa.pos++;
            token.kind = DOUBLE_AMPERSAND;
          }
          else if (c == L'=')
          {
            lexa.pos++;
            token.kind = AMPERSAND_EQUAL;
          }
          else
          {
            token.kind = AMPERSAND;
          }
          return;


        case L'\'':   // character literal or apostrophe
          if (looks_like_character_literal())
          {
            scan_character_literal ();
            return;
          }

          lexa.pos++;
          token.kind = APOSTROPHE;
          return;

        case L'(':
          lexa.pos++;
          token.kind = LEFT_PARENTHESIS;
          return;

        case L')':
          lexa.pos++;
          token.kind = RIGHT_PARENTHESIS;
          return;

        case L'*':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (lexa.in_statement && c == L'=')
          {
            lexa.pos++;
            token.kind = STAR_EQUAL;
          }
          else
          {
            token.kind = STAR;
          }
          return;

        case L'+':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'+')
          {
            lexa.pos++;
            token.kind = PLUS_PLUS;
          }
          else if (c == L'=')
          {
            lexa.pos++;
            token.kind = PLUS_EQUAL;
          }
          else
          {
            token.kind = PLUS;
          }
          return;

        case L',':
          lexa.pos++;
          token.kind = COMMA;
          return;

        case L'-':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'-')
          {
            lexa.pos++;
            token.kind = MINUS_MINUS;
          }
          else if (c == L'=')
          {
            lexa.pos++;
            token.kind = MINUS_EQUAL;
          }
          else if (c == L'>')
          {
            lexa.pos++;
            token.kind = SINGLE_ARROW;
          }
          else
          {
            token.kind = MINUS;
          }
          return;

        case L'.':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'.')
          {
            lexa.pos++;
            token.kind = DOUBLE_DOT;
          }
          else
          {
            token.kind = DOT;
          }
          return;

        case L'/':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'=')
          {
            lexa.pos++;
            token.kind = SLASH_EQUAL;
          }
          else if (c == L'*')    // multi-line comments
          {
            parse_multiline_comment ();
            break;  // leave case statement here, without return
          }
          else if (c == L'/')
          {
            // skip all characters til new_line or end-of-source
            for (;;)
            {
              lexa.pos++;
              c = lexa.source_text^[lexa.pos];
              if (c == Lnul || c == (wchar)10)
                break;
            }
            break;  // leave case statement here, without return
          }
          else
          {
            token.kind = SLASH;
            return;
          }
          return;


        case L':':
          lexa.pos++;
          token.kind = COLON;
          return;

        case L';':
          lexa.pos++;
          token.kind = SEMICOLON;
          return;

        case L'<':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'<')
          {
            lexa.pos++;

            c = lexa.source_text^[lexa.pos];
            if (c == L'=')
            {
              lexa.pos++;
              token.kind = SHIFT_LEFT_EQUAL;
            }
            else
            {
              token.kind = SHIFT_LEFT;
            }
          }
          else if (c == L'=')
          {
            lexa.pos++;
            token.kind = SMALLER_OR_EQUAL;
          }
          else
          {
            token.kind = SMALLER;
          }
          return;

        case L'=':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'=')
          {
            lexa.pos++;
            token.kind = EQUAL;
          }
          else if (c == L'>')
          {
            lexa.pos++;
            token.kind = ARROW;
          }
          else
          {
            token.kind = ASSIGN;
          }
          return;

        case L'>':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'=')
          {
            lexa.pos++;
            token.kind = LARGER_OR_EQUAL;
          }
          else if (c == L'>')
          {
            lexa.pos++;

            c = lexa.source_text^[lexa.pos];
            if (c == L'=')
            {
              lexa.pos++;
              token.kind = SHIFT_RIGHT_EQUAL;
            }
            else
            {
              token.kind = SHIFT_RIGHT;
            }
          }
          else
          {
            token.kind = LARGER;
          }
          return;

        case L'?':
          lexa.pos++;
          token.kind = QUESTION_MARK;
          return;

        case L'[':
          lexa.pos++;
          token.kind = LEFT_BRACKET;
          return;

        case L']':
          lexa.pos++;
          token.kind = RIGHT_BRACKET;
          return;

        case L'^':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (lexa.in_statement && c == L'=')
          {
            lexa.pos++;
            token.kind = CARET_EQUAL;
          }
          else
          {
            token.kind = CARET;
          }
          return;

        case L'{':
          lexa.pos++;
          token.kind = LEFT_ACCOLADE;
          return;

        case L'|':
          lexa.pos++;
          c = lexa.source_text^[lexa.pos];
          if (c == L'=')
          {
            lexa.pos++;
            token.kind = VERTICAL_BAR_EQUAL;
          }
          else if (c == L'|')
          {
            lexa.pos++;
            token.kind = DOUBLE_VERTICAL_BAR;
          }
          else
          {
            token.kind = VERTICAL_BAR;
          }
          return;

        case L'}':
          lexa.pos++;
          token.kind = RIGHT_ACCOLADE;
          return;

        case L'~':
          lexa.pos++;
          token.kind = TILDE;
          return;
 
        default:
          lexical_error ("illegal token", token.pos);
          lexa.pos++;
          break;
      }
    }
  }
}

/**********************************************************************/

// retrieve rest of line after directive;
// if it's longer than 128 chars it is cut.

void lex_get_rest_of_directive_line (out wchar line[128+1])
{
  wchar c;
  int   i;

  clear line;
  lexa.directive_on_line = 0;

  for (i=0; ; )
  {
    c = lexa.source_text^[lexa.pos];
    if (c == (wchar)10 || c == Lnul)
      break;
    if (i < 128 && c >= L' ' && (i > 0 || c != L' '))
      line[i++] = c;
    lexa.pos++;
  }
//  line[i] = Lnul;

  lex_get_token ();
}

/**********************************************************************/

// skip rest of line after directive.

void lex_skip_rest_of_directive_line ()
{
  wchar c;

  lexa.directive_on_line = 0;

  for (;;)
  {
    c = lexa.source_text^[lexa.pos];
    if (c == (wchar)10 || c == Lnul)
      break;
    lexa.pos++;
  }

  lex_get_token ();
}

/**********************************************************************/

// skips newline, then searches for a directive at the start of a newline.
// retrieves either a directive or LAST_TOKEN.

void lex_get_next_directive()
{
  wchar c;

  for (;;)
  {
    for (;;)   // pass next newline
    {
      c = lexa.source_text^[lexa.pos];
      if (c == (wchar)10)
      {
        lexa.bol = lexa.pos;
        token.pos.line++;
        lexa.pos++;
        break;
      }
      else if (c == Lnul)
      {
        if (lexa.pos+1 == lexa.max_length)
        {
          lex_get_token ();    // get LAST_TOKEN
          return;
        }
        else   // illegal character
        {
          token.pos.col = lexa.pos - lexa.bol;
          lexical_error ("illegal character", token.pos);
        }
      }

      lexa.pos++;
    }

    // look for directive

    for (;;)
    {
      c = lexa.source_text^[lexa.pos];
      if (c == Lnul || c == (wchar)10)
        break;

      if (c == L'#')    // directive found
      {
        lex_get_token ();
        return;
      }

      if (c > L' ')   // some token other than #
        break;

      lexa.pos++;    // skip white space
    }
  }
}

/**********************************************************************/

// skips token until LAST_TOKEN or a directive.

void skip_if_directive ()
{
  bool else_found = false;

  lex_get_next_directive ();  // skip #if

  for (;;)
  {
    if (token.kind == LAST_TOKEN)
      break;

    if (token.kind == TOKEN_DIRECTIVE_if)
    {
      skip_if_directive();   // recursive call
    }
    else if (token.kind == TOKEN_DIRECTIVE_elif)
    {
      if (else_found)
        lexical_error ("#elsif not allowed after #else", token.pos);
      lex_get_next_directive ();
    }
    else if (token.kind == TOKEN_DIRECTIVE_else)
    {
      if (else_found)
        lexical_error ("double #else not allowed", token.pos);
      else_found = true;
      lex_get_next_directive ();
    }
    else if (token.kind == TOKEN_DIRECTIVE_endif)
    {
      lex_get_next_directive ();
      break;
    }
    else
    {
      lex_get_next_directive ();
    }
  }
}

/**********************************************************************/
bool p_expression ();
/**********************************************************************/

bool p_primary ()
{
  bool b;
  int  rc;
  char str[256];

  if (token.kind == LEFT_PARENTHESIS)
  {
    lex_get_token ();
    b = p_expression ();
    if (token.kind != RIGHT_PARENTHESIS)
      lexical_error ("')' expected", token.pos);
    else
      lex_get_token ();
    return b;
  }
  else if (token.kind == TOKEN_true)
  {
    lex_get_token ();
    return true;
  }
  else if (token.kind == TOKEN_false)
  {
    lex_get_token ();
    return false;
  }
  else if (token.kind == INTEGER_LITERAL)
  {
    if (token.info._integer.has_suffix_L ||
        token.info._integer.value < 0 ||
        token.info._integer.value > 1)
      lexical_error ("only 0 or 1 are allowed", token.pos);

    b = (token.info._integer.value != 0);
    lex_get_token ();
    return b;
  }
  else if (token.kind == IDENTIFIER)
  {
    if (!has_at_least_one_letter (token.info._identifier.value))
    {
      lexical_error ("a symbol must at least contain one letter a-z", token.pos);
      b = false;
    }
    else
    {
      if (is_all_in_lowercase (token.info._identifier.value))
      {
        rc = get_symbol_value (ref lexa.local_symbols^, token.info._identifier.value, out b);
        if (rc < 0)
          fatal_out_of_memory_error ("get_symbol_value()");
        else if (rc == +1)
        {
          sprintf (out str, "local symbol '%.32S' was not defined", token.info._identifier.value);
          lexical_error (str, token.pos);
        }
      }
      else if (is_all_in_uppercase (token.info._identifier.value))
      {
        rc = get_symbol_value (ref global_symbols, token.info._identifier.value, out b);
        if (rc < 0)
          fatal_out_of_memory_error ("get_symbol_value()");
        else if (rc == +1)
        {
          sprintf (out str, "global symbol '%.32S' was not defined in config file",
                   token.info._identifier.value);
          lexical_error (str, token.pos);
        }
        else
        {
          // add symbol in global btree for this source file
          rc = insert_symbol (ref lexa.global_symbols^, token.info._identifier.value, b);  
          if (rc < 0)
            fatal_out_of_memory_error ("insert_symbol()");
        }
      }
      else
      {
        lexical_error ("a symbol must appear all in upper or lower case", token.pos);
        b = false;
      }
    }

    lex_get_token ();
    return b;
  }
  else
  {
    lexical_error ("p_expression expected", token.pos);
    if (token.kind != TOKEN_END_OF_DIRECTIVE)
      lex_get_token ();
    return true;
  }
}

/**********************************************************************/

bool p_unary ()
{
  if (token.kind == NOT)
  {
    lex_get_token ();
    return !p_primary ();
  }
  else
  {
    return p_primary ();
  }
}

/**********************************************************************/

bool p_expression ()
{
  bool b, b2;

  if (token.kind == NOT)
  {
    b = p_unary ();
  }
  else
  {
    b = p_primary ();

    if (token.kind == DOUBLE_VERTICAL_BAR)
    {
      while (token.kind == DOUBLE_VERTICAL_BAR)
      {
        lex_get_token ();
        b2 = p_unary ();
        b |= b2;
      }
    }
    else if (token.kind == DOUBLE_AMPERSAND)
    {
      while (token.kind == DOUBLE_AMPERSAND)
      {
        lex_get_token ();
        b2 = p_unary ();
        b &= b2;
      }
    }
    else if (token.kind == CARET)
    {
      while (token.kind == CARET)
      {
        lex_get_token ();
        b2 = p_unary ();
        b ^= b2;
      }
    }
  }

  return b;
}

/**********************************************************************/

// ends with end-directive

bool evaluate_p_expression ()
{
  bool b;

  b = p_expression ();

  if (token.kind != TOKEN_END_OF_DIRECTIVE)
    lexical_error ("syntax error", token.pos);

  while (token.kind != TOKEN_END_OF_DIRECTIVE)
    lex_get_token ();

  return b;
}

/**********************************************************************/

public
void get_token ()
{
  bool b;

  if (lexa.look_head_flag)
  {
    token = look_ahead_token;
    lexa.look_head_flag = false;
    return;
  }

  lex_get_token();

  for (;;)
  {
    if (token.kind < TOKEN_DIRECTIVE_begin || token.kind > TOKEN_END_OF_DIRECTIVE)
    {
      if (token.kind == LAST_TOKEN)
      {
        if (lexa.if_directive_nesting > 0)
          lexical_error ("missing #endif", token.pos);
        if (lexa.within_unsafe)
          lexical_error ("missing '#end unsafe'", token.pos);
      }

      lexa.line_of_last_token = token.pos.line;
      return;
    }

    if (lexa.line_of_last_token == token.pos.line)
      lexical_error ("a directive must stand alone on a line", token.pos);

    switch (token.kind)
    {
      case TOKEN_DIRECTIVE_begin:
        lex_get_token();
        if (token.kind != IDENTIFIER || wstrcmp (token.info._identifier.value, L"unsafe") != 0)
          lexical_error ("'unsafe' expected here", token.pos);
        else
        {
          if (lexa.within_unsafe)
            lexical_error ("'#begin unsafe' directives cannot be nested", token.pos);
          lexa.within_unsafe = true;

          lex_get_token();  // skip 'unsafe'

          if (token.kind != TOKEN_END_OF_DIRECTIVE)
            lexical_error ("syntax error", token.pos);
        }
        while (token.kind != TOKEN_END_OF_DIRECTIVE)
          lex_get_token();

        lex_get_token();
        break;

      case TOKEN_DIRECTIVE_end:
        lex_get_token();
        if (token.kind != IDENTIFIER || wstrcmp (token.info._identifier.value, L"unsafe") != 0)
          lexical_error ("'unsafe' expected here", token.pos);
        else
        {
          if (!lexa.within_unsafe)
            lexical_error ("'#end unsafe' must follow '#begin unsafe'", token.pos);
          lexa.within_unsafe = false;

          lex_get_token();  // skip 'unsafe'

          if (token.kind != TOKEN_END_OF_DIRECTIVE)
            lexical_error ("syntax error", token.pos);
        }
        while (token.kind != TOKEN_END_OF_DIRECTIVE)
          lex_get_token();

        lex_get_token();
        break;

      case TOKEN_DIRECTIVE_define:
        lex_get_token();
        if (token.kind != IDENTIFIER)
          lexical_error ("an identifier is expected here", token.pos);
        else
        {
          wchar id[MAX_IDENTIFIER_LENGTH];
          int   rc;

          id = token.info._identifier.value;

          if (!is_all_in_lowercase (id))
            lexical_error ("a local symbol must appear all in lower case", token.pos);

          if (!has_at_least_one_letter (id))
            lexical_error ("a local symbol must at least contain one letter a-z", token.pos);

          lex_get_token();  // skip identifier

          b = evaluate_p_expression ();

          rc = insert_symbol (ref lexa.local_symbols^, id, b);
          if (rc < 0)
            fatal_compiler_error ("insert_symbol()", token.pos);
          else if (rc == +1)
          {
            char str[256];
            sprintf (out str, "local symbol '%.64S' was defined twice", id);
            lexical_error (str, token.pos);
          }
        }
        while (token.kind != TOKEN_END_OF_DIRECTIVE)
          lex_get_token();

        lex_get_token();
        break;

      case TOKEN_DIRECTIVE_if:
        if (lexa.if_directive_nesting < MAX_IF_DIRECTIVE_NESTING)
        {
          lexa.if_directive_nesting++;
          lexa.else_found[lexa.if_directive_nesting] = false;
        }
        else
        {
          lexical_error ("too many nested #if directives", token.pos);
        }

        lex_get_token ();

        b = evaluate_p_expression ();  // ends with end-directive
        if (b)
        {
          lex_get_token ();   // skip 'end-directive'
          break;
        }

        lex_get_next_directive ();

        for (;;)
        {
          if (token.kind == LAST_TOKEN)
            break;

          if (token.kind == TOKEN_DIRECTIVE_if)
          {
            skip_if_directive();
          }
          else if (token.kind == TOKEN_DIRECTIVE_elif)
          {
            lex_get_token ();
            b = evaluate_p_expression ();  // ends with end-directive
            if (b)
            {
              lex_get_token ();   // skip 'end-directive'
              break;
            }

            lex_get_next_directive ();
          }
          else if (token.kind == TOKEN_DIRECTIVE_else)
          {
            lexa.else_found[lexa.if_directive_nesting] = true;
            lex_get_token ();
            if (token.kind != TOKEN_END_OF_DIRECTIVE)
              lexical_error ("syntax error", token.pos);
            lex_skip_rest_of_directive_line ();
            break;
          }
          else if (token.kind == TOKEN_DIRECTIVE_endif)
          {
            lex_get_token ();
            if (token.kind != TOKEN_END_OF_DIRECTIVE)
              lexical_error ("syntax error", token.pos);
            lex_skip_rest_of_directive_line ();
            lexa.if_directive_nesting--;
            break;
          }
          else
          {
            lex_get_next_directive ();
          }
        }
        break;

      case TOKEN_DIRECTIVE_elif:
        if (lexa.if_directive_nesting == 0)
        {
          lexical_error ("no previous #if directive", token.pos);
          lexa.if_directive_nesting++;
          lexa.else_found[lexa.if_directive_nesting] = false;
        }

        if (lexa.else_found[lexa.if_directive_nesting])
          lexical_error ("#elif not allowed after #else", token.pos);

        lex_get_token ();

        b = evaluate_p_expression ();  // ends with end-directive

        lex_get_next_directive ();

        for (;;)
        {
          if (token.kind == LAST_TOKEN)
            break;

          if (token.kind == TOKEN_DIRECTIVE_if)
          {
            skip_if_directive();
          }
          else if (token.kind == TOKEN_DIRECTIVE_elif)
          {
            if (lexa.else_found[lexa.if_directive_nesting])
              lexical_error ("#elif not allowed after #else", token.pos);

            lex_get_token ();
            b = evaluate_p_expression ();  // swallows end-directive
            lex_get_next_directive ();
          }
          else if (token.kind == TOKEN_DIRECTIVE_else)
          {
            if (lexa.else_found[lexa.if_directive_nesting])
              lexical_error ("double #else not allowed", token.pos);
            lexa.else_found[lexa.if_directive_nesting] = true;

            lex_get_token ();
            if (token.kind != TOKEN_END_OF_DIRECTIVE)
              lexical_error ("syntax error", token.pos);

            lex_get_next_directive ();
          }
          else if (token.kind == TOKEN_DIRECTIVE_endif)
          {
            lex_get_token ();
            if (token.kind != TOKEN_END_OF_DIRECTIVE)
              lexical_error ("syntax error", token.pos);
            lex_skip_rest_of_directive_line ();

            if (lexa.if_directive_nesting > 0)
              lexa.if_directive_nesting--;
            break;
          }
          else
          {
            lex_get_next_directive ();
          }
        }
        break;

      case TOKEN_DIRECTIVE_else:
        if (lexa.if_directive_nesting == 0)
        {
          lexical_error ("no previous #if directive", token.pos);
          lexa.if_directive_nesting++;
          lexa.else_found[lexa.if_directive_nesting] = false;
        }

        if (lexa.else_found[lexa.if_directive_nesting])
          lexical_error ("double #else not allowed", token.pos);

        lex_get_token ();
        if (token.kind != TOKEN_END_OF_DIRECTIVE)
          lexical_error ("syntax error", token.pos);

        lex_get_next_directive ();

        for (;;)
        {
          if (token.kind == LAST_TOKEN)
            break;

          if (token.kind == TOKEN_DIRECTIVE_if)
          {
            skip_if_directive();
          }
          else if (token.kind == TOKEN_DIRECTIVE_else ||
                   token.kind == TOKEN_DIRECTIVE_elif)
          {
            lexical_error ("no previous #if directive", token.pos);
            lex_get_next_directive ();
          }
          else if (token.kind == TOKEN_DIRECTIVE_endif)
          {
            lex_get_token ();
            if (token.kind != TOKEN_END_OF_DIRECTIVE)
              lexical_error ("syntax error", token.pos);
            lex_skip_rest_of_directive_line ();

            if (lexa.if_directive_nesting > 0)
              lexa.if_directive_nesting--;
            break;
          }
          else
          {
            lex_get_next_directive ();
          }
        }
        break;

      case TOKEN_DIRECTIVE_endif:
        if (lexa.if_directive_nesting == 0)
        {
          lexical_error ("no previous #if directive", token.pos);
          lexa.if_directive_nesting++;
          lexa.else_found[lexa.if_directive_nesting] = false;
        }

        lex_get_token ();
        if (token.kind != TOKEN_END_OF_DIRECTIVE)
          lexical_error ("syntax error", token.pos);
        lex_skip_rest_of_directive_line ();

        if (lexa.if_directive_nesting > 0)
          lexa.if_directive_nesting--;
        break;

      case TOKEN_DIRECTIVE_error:
        {
          wchar line[128+1];
          char line2[128+1];
          lex_get_rest_of_directive_line (out line);
          wstring_to_string (line, out line2);
          lexical_error (line2, token.pos);
        }
        break;

      case TOKEN_DIRECTIVE_warning:
        {
          wchar line[128+1];
          char line2[128+1];
          lex_get_rest_of_directive_line (out line);
          wstring_to_string (line, out line2);
          warning (line2, token.pos);
        }
        break;

      case TOKEN_UNKNOWN_DIRECTIVE:
        lexical_error ("unknown directive", token.pos);
        lex_skip_rest_of_directive_line ();
        break;

      case TOKEN_END_OF_DIRECTIVE:
        fatal_compiler_error ("get_token(1)", token.pos);
        token.kind = LAST_TOKEN;
        break;

      default:
        fatal_compiler_error ("get_token(2)", token.pos);
        token.kind = LAST_TOKEN;
        break;
    }
  }
}

/**********************************************************************/

public
void get_look_ahead_token ()
{
  TOKEN_INFO token0;

  if (lexa.look_head_flag)
    return;

  token0 = token;

  get_token ();

  look_ahead_token = token;

  token = token0;

  lexa.look_head_flag = true;
}

/**********************************************************************/

// skip token until specified token kind, or LAST_TOKEN

public
void skip_until (TOKEN_KIND kind, bool token_included)
{
  while (token.kind != LAST_TOKEN && token.kind != kind)
    get_token();

  if (token_included && token.kind != LAST_TOKEN)
    get_token();
}

/**********************************************************************/
