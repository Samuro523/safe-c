
// lex.h : lexical analyzer for Safe-C compiler - 17 to 28 avril 2010.

use tokens, symbols;

/**********************************************************************/

const int MAX_IF_DIRECTIVE_NESTING = 16;

struct LEXICAL_ANALYZER_CONTEXT
{
  wstring^ source_text;    // Lnul-terminated
  int      max_length;     // source text maximum characters
  int      pos;            // current scan position
  int      bol;            // begin of line position
  bool     can_be_quote;   // true if CHAR_LITERAL could be APOSTROPHE
  int      quote_pos;      // saved position in case of conversion into apostrophe
  bool     in_statement;   // indicates if we're within a statement

  // a #directive must not begin on the same line
  int   multiline_comment_ended_on_line;

  // a multiline comment on this line must not contain newlines
  int   directive_on_line;

  int   line_of_last_token;

  bool  within_unsafe;

  int   if_directive_nesting;   // 0 to MAX_IF_DIRECTIVE_NESTING
  bool  else_found[MAX_IF_DIRECTIVE_NESTING+1];

  BINARY_TREE^ global_symbols;
  BINARY_TREE^ local_symbols;

  bool look_head_flag;
}

/**********************************************************************/

LEXICAL_ANALYZER_CONTEXT lexa;
TOKEN_INFO token;   // current token

/**********************************************************************/

// inits lexa and token.
void lex_start_new_context (wstring^ source_text);  // Lnul-terminated

void lex_close_context ();

/**********************************************************************/

void get_token ();

/**********************************************************************/

void lex_force_into_apostrophe ();
void lex_set_in_statement (bool flag);

/**********************************************************************/

TOKEN_INFO look_ahead_token;

void get_look_ahead_token ();

/**********************************************************************/

// skip token until specified token kind, or LAST_TOKEN
void skip_until (TOKEN_KIND kind, bool token_included);

/**********************************************************************/

bool isinfinite (double d);
bool fisinfinite (float f);

/**********************************************************************/
