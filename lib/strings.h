
// strings.h : strings with optionally terminating nul character.

//---------------------------------------------------------------

// clears dest, then copies src into dest.
// causes a runtime error if the target buffer is too small.
// returns the actual length of the result.

int strcpy (out string dest, string src);

//---------------------------------------------------------------

// clears dest, then copies src into dest but stops after 'len' chars.
// causes a runtime error if the target buffer is too small.
// returns the actual length of the result.
// a runtime occurs if len < 0.

int strncpy (out string dest, string src, int len);

//---------------------------------------------------------------

// append a string to another string.
// causes a runtime error if the target buffer is too small.
// returns the actual length of the result.

int strcat (ref string dest, string src);

//---------------------------------------------------------------

// returns active length of string

int strlen (string s);

//---------------------------------------------------------------

// compare two strings
// returns -1 if a<b, 0 if equal, +1 if a>b

int strcmp (string a, string b);

//---------------------------------------------------------------

// compare two strings with insensitive case
// returns -1 if a<b, 0 if equal, +1 if a>b

int stricmp (string a, string  b);

//---------------------------------------------------------------

// compare two strings but stop after 'maxlen' chars
// returns -1 if a<b, 0 if equal, +1 if a>b
// a runtime error occurs if maxlen < 0.

int strncmp (string a, string b, int maxlen);

//---------------------------------------------------------------

// compare two strings with insensitive case but stop after 'maxlen' chars
// returns -1 if a<b, 0 if equal, +1 if a>b
// a runtime error occurs if maxlen < 0.

int strnicmp (string a, string  b, int maxlen);

//---------------------------------------------------------------

// compare two memory area
// returns -1 if a<b, 0 if equal, +1 if a>b
// a runtime error occurs if both areas don't have the same length

int memcmp (byte[] a, byte[] b);

//---------------------------------------------------------------

// find a char in a string
// returns position or -1 if not found

int strchr (string s, char c);

//---------------------------------------------------------------

// find last occurence of char in a string
// returns position or -1 if not found

int strrchr (string s, char c);

//---------------------------------------------------------------

// find a fragment in a text
// returns position, or -1 if not found

int strstr (string text, string fragment);

//---------------------------------------------------------------

// find a fragment in a text with insensitive case
// returns position, or -1 if not found

int stristr (string text, string fragment);

//---------------------------------------------------------------

// clears buffer, then fills it with the format and the arguments provided.
// causes a runtime error if the target buffer is too small
// or if the format string has a bad format.
// returns the actual length of the result.

int sprintf (out string buffer, string format, object[] arg);

// 'format' has the following structure :
//
//  "%[flags][width][.precision]type"
//
//   type                             output
//   ----                             ------
//   %                                '%%' will write '%'
//   d  any signed integer            decimal number (-61)
//   u  any unsigned integer or enum  unsigned number (12)
//   x  any integer or enum           hexadecimal number (7fa)
//   e  float, double                 scientific (3.9265e+2)
//   f  float, double                 floating-point (392.65)
//   c  char or string                all char, or 'precision' char
//   C  wchar or wstring              all wchar, or 'precision' wchar
//   s  string                        stops at nul, or 'precision' char
//   S  wstring                       stops at Lnul, or 'precision' wchar
//
//   width
//   -----
//   (number)   Minimum number of characters to be printed (padded
//              with spaces or zeroes if necessary).
//              The value is not truncated even if the result is larger.
//   *          The width is not specified in the format string but as an
//              additional int value parameter preceding the parameter to be
//              formatted.
//
//   precision
//   ---------
//   .number   A dot without number means precision zero.
//             For f : this is the number of digits to be printed after the
//                     decimal point. Default precision is 6.
//                     No decimal point is printed for precision zero.
//             For s/S : this is the maximum number of characters to be
//                       printed. Default is all characters.
//   .*        The precision is not specified in the format string but as an
//             additional int value parameter preceding the parameter to be
//             formatted.
//
//   flags
//   -----
//   -      for all           : left-justify within the given field width
//                              (default is right-justify).
//   +      for d, e, f       : prefix '+' for positive or zero numbers.
//   0      for d, e, f, x, u : prefix '0' digits instead of spaces;
//                              (not valid together with flag '-')

//---------------------------------------------------------------

// extract arguments from string.
//
// returns 0 if OK, a non-zero value otherwise.
// In particular :
// . +1 : warning : extra characters remain in the buffer after all arguments received values;
// . -1 : (never occurs)
// . -2 : short buffer : the buffer was exhausted before format was completely scanned;
// . -3 : short format : the format was exhausted before all arguments received a value;
// . -4 : a syntax error occured while scanning a numeric value;
// . -5 : an overflow occured while scanning a numeric value or storing it in a numeric argument;
//
// a runtime occurs when the format string has an invalid format.

int sscanf (string buffer, string format, out object[] arg);

// 'format' can contain :
//
//  white space (ascii 1 to 32)
//  ---------------------------
//  effect: ignore zero or more white space characters in input stream.
//
//  Non-whitespace character, except percentage sign (%)
//  ----------------------------------------------------
//  effect: character must match with input stream, or the function fails (-4).
//
//  Format specifiers:  "%[*][width]type"
//
//    *        data is read but not stored (there is no corresponding parameter).
//   width     maximum number of characters to be read.
//
//   type                     input
//   ----                     -----
//   %                        '%%' will read '%'
//
//   d   any signed integer   decimal number preceded by optional + or - (-61)
//
//   u   any unsigned integer
//       or enum              unsigned number (12)
//
//   x   same as d or u       hexadecimal number (7fa)
//
//   f   float or double      floating-point number (0.5)  (12.4E+3)
//   e   same as f
//
//   c   char or string       fill arg, or read max 'width' chars.
//   C   wchar or wstring     fill arg, or read max 'width' wchars.
//   s   char or string       same as c but stop at first white-space.
//   S   wchar or wstring     same as C but stop at first white-space.

//---------------------------------------------------------------

// same as sprintf, but append result to the end of buffer

int strcatf (ref string buffer, string format, object[] arg);

//---------------------------------------------------------------

// convert to lower/upper case

char tolower (char c);
char toupper (char c);

//---------------------------------------------------------------

bool isalpha (char c);   // A-Z a-z
bool isdigit (char c);   // 0-9
bool isxdigit (char c);  // 0-9 A-F a-f

//---------------------------------------------------------------

// remove all accents, i.e. change all 'é', 'è', 'ê' into 'e'
char normalize_extended_character (char c);

// source and target must be different
void normalize_extended_characters (string source, out string target);

//---------------------------------------------------------------------------------

void trim (ref string s);    // remove leading and trailing white space (<= 32)
void ltrim (ref string s);   // remove leading white space (<= 32)
void rtrim (ref string s);   // remove trailing white space (<= 32)

//---------------------------------------------------------------------------------

// return cost to convert string a to string b by inserting, deleting or replacing characters.
// returns 0 if strings are identical.
uint2 levenshtein_distance (string a,
                            string b,
                            uint2  cost_insert  = 1,
                            uint2  cost_delete  = 1,
                            uint2  cost_replace = 1);

//---------------------------------------------------------------

// return similarity coefficient [0.0 .. 1.0] between the set of characters in strings a and b.
// note: this function evaluates only letters [A..Z], upper and lower case, possibly with accents,
// and ignores all other symbols, i.e. space and punctuation.
// when asymetric is true, the function checks only if the letters of A exist in B, not if those in B exist in A.
float jaccard_coefficient (string a, string b, bool asymetric = false);

//---------------------------------------------------------------

int wstrcpy (out wstring dest, wstring src);
int wstrncpy (out wstring dest, wstring src, int len);
int wstrcat (ref wstring dest, wstring src);
int wstrlen (wstring s);
int wstrcmp (wstring a, wstring b);
int wstricmp (wstring a, wstring  b);
int wstrncmp (wstring a, wstring b, int maxlen);
int wstrnicmp (wstring a, wstring  b, int maxlen);
int wstrchr (wstring s, wchar c);
int wstrrchr (wstring s, wchar c);
int wstrstr (wstring text, wstring fragment);
int wstristr (wstring text, wstring fragment);
int wsprintf (out wstring buffer, wstring format, object[] arg);
int wsscanf (wstring buffer, wstring format, out object[] arg);
int wstrcatf (ref wstring buffer, wstring format, object[] arg);
wchar wtolower (wchar c);
wchar wtoupper (wchar c);
bool wisalpha (wchar c);   // A-Z a-z
bool wisdigit (wchar c);   // 0-9
bool wisxdigit (wchar c);  // 0-9 A-F a-f
wchar wnormalize_extended_character (wchar c);
void wnormalize_extended_characters (wstring source, out wstring target);
void wtrim (ref wstring s);    // remove leading and trailing white space (<= 32)
void wltrim (ref wstring s);   // remove leading white space (<= 32)
void wrtrim (ref wstring s);   // remove trailing white space (<= 32)

//---------------------------------------------------------------------------------
uint2 wlevenshtein_distance (wstring a,
                             wstring b,
                             uint2   cost_insert  = 1,
                             uint2   cost_delete  = 1,
                             uint2   cost_replace = 1);
float wjaccard_coefficient (wstring a, wstring b, bool asymetric = false);

//---------------------------------------------------------------
