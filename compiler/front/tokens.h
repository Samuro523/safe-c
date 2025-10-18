
// token.h : lexical analysis tokens

use ../common;

enum TOKEN_KIND {
  LEFT_ACCOLADE,        //  {
  RIGHT_ACCOLADE,       //  }
  LEFT_PARENTHESIS,     //  (
  RIGHT_PARENTHESIS,    //  )
  LEFT_BRACKET,         //  [
  RIGHT_BRACKET,        //  ]
  APOSTROPHE,           //  '
  COLON,                //  :
  COMMA,                //  ,
  SEMICOLON,            //  ;
  QUESTION_MARK,        //  ?    (10)
  TILDE,                //  ~
  DOT,                  //  .
  DOUBLE_DOT,           //  ..
  PLUS,                 //  +
  PLUS_EQUAL,           //  +=
  PLUS_PLUS,            //  ++
  MINUS,                //  -
  MINUS_EQUAL,          //  -=
  MINUS_MINUS,          //  --
  SINGLE_ARROW,         //  ->   (20)
  STAR,                 //  *
  STAR_EQUAL,           //  *=
  SLASH,                //  /
  SLASH_EQUAL,          //  /=
  PERCENT,              //  %
  PERCENT_EQUAL,        //  %=
  NOT,                  //  !
  NOT_EQUAL,            //  !=
  ASSIGN,               //  =
  EQUAL,                //  ==   (30)
  ARROW,                //  =>
  AMPERSAND,            //  &
  DOUBLE_AMPERSAND,     //  &&
  AMPERSAND_EQUAL,      //  &=
  VERTICAL_BAR,         //  |
  DOUBLE_VERTICAL_BAR,  //  ||
  VERTICAL_BAR_EQUAL,   //  |=
  CARET,                //  ^
  CARET_EQUAL,          //  ^=
  SMALLER,              //  <   (40)
  SMALLER_OR_EQUAL,     //  <=
  SHIFT_LEFT,           //  <<
  SHIFT_LEFT_EQUAL,     //  <<=
  LARGER,               //  >
  LARGER_OR_EQUAL,      //  >=
  SHIFT_RIGHT,          //  >>
  SHIFT_RIGHT_EQUAL,    //  >>=

  IDENTIFIER,                                   // (48)
  INTEGER_LITERAL, FLOAT_LITERAL,
  CHAR_LITERAL,    STRING_LITERAL,


  /*******************************************************************/
  TOKEN_DIRECTIVE_begin,    // #begin     (53)
  TOKEN_DIRECTIVE_end,      // #end
  TOKEN_DIRECTIVE_define,   // #define
  TOKEN_DIRECTIVE_elif,
  TOKEN_DIRECTIVE_else,
  TOKEN_DIRECTIVE_endif,
  TOKEN_DIRECTIVE_error,
  TOKEN_DIRECTIVE_if,       // (60)
  TOKEN_DIRECTIVE_warning,
  TOKEN_UNKNOWN_DIRECTIVE,  // (62) preprocessor must give error
  TOKEN_END_OF_DIRECTIVE,   // (63) when reaching end of directive (can be on another line if commented)
  /*******************************************************************/


  TOKEN_Lnul,    // (64)
  TOKEN_asm,     TOKEN_unused,   // have leading underscore

  TOKEN_abort,   TOKEN_assert,
  TOKEN_body,    TOKEN_bool,     TOKEN_break,   TOKEN_byte,
  TOKEN_case,    TOKEN_char,     TOKEN_clear,   TOKEN_const,   TOKEN_continue,
  TOKEN_default, TOKEN_double,
  TOKEN_else,    TOKEN_end,      TOKEN_enum,
  TOKEN_false,   TOKEN_float,    TOKEN_float4,  TOKEN_float8,  TOKEN_for,  TOKEN_free,  TOKEN_from,
  TOKEN_generic,
  TOKEN_if,      TOKEN_inline,   TOKEN_int,     TOKEN_int1,    TOKEN_int2,
  TOKEN_int4,    TOKEN_int8,
  TOKEN_long,
  TOKEN_new,     TOKEN_nul,      TOKEN_null,
  TOKEN_object,  TOKEN_out,
  TOKEN_package, TOKEN_packed,   TOKEN_public,
  TOKEN_ref,     TOKEN_return,   TOKEN_run,
  TOKEN_short,   TOKEN_sleep,    TOKEN_string,  TOKEN_struct,  TOKEN_switch,
  TOKEN_tiny,    TOKEN_true,     TOKEN_typedef,
  TOKEN_uint,    TOKEN_uint1,    TOKEN_uint2,   TOKEN_uint4,
  TOKEN_union,   TOKEN_use,      TOKEN_ushort,
  TOKEN_void,    TOKEN_volatile,
  TOKEN_wchar,   TOKEN_while,    TOKEN_wstring,

  LAST_TOKEN
};


const int MAX_IDENTIFIER_LENGTH = 128;

packed struct TOKEN_INFO_IDENTIFIER
{
  wchar value[MAX_IDENTIFIER_LENGTH];
}

packed struct TOKEN_INFO_INTEGER_LITERAL
{
  int8 value;
  bool has_suffix_L;
}

packed struct TOKEN_INFO_FLOAT_LITERAL
{
  double value;
  char   suffix;   // nul, 'F' or 'D'
}

packed struct TOKEN_INFO_CHARACTER_LITERAL
{
  uint2 value;
  bool  has_prefix_L;
}

const int MAX_STRING_LITERAL_LENGTH = 128;

packed struct TOKEN_INFO_STRING_LITERAL
{
  wchar value[MAX_STRING_LITERAL_LENGTH];   // can contain zeroes
  int   length;
  bool  has_prefix_L;
}

union TOKEN_INFO_EXTRA
{
  TOKEN_INFO_IDENTIFIER        _identifier;
  TOKEN_INFO_INTEGER_LITERAL   _integer;
  TOKEN_INFO_FLOAT_LITERAL     _float;
  TOKEN_INFO_CHARACTER_LITERAL _char;
  TOKEN_INFO_STRING_LITERAL    _string;
}

struct TOKEN_INFO
{
  TOKEN_KIND       kind;
  TEXT_POSITION    pos;
  TOKEN_INFO_EXTRA info;
}
