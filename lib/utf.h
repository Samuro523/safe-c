
// utf.h : conversion between ascii, utf-8 and utf-16

//--------------------------------------------------------------------------

// compute target length of conversion.

void ascii_to_utf8_target_length (    char[] source,
                                      int    source_length,
                                  out int    target_length);

void ascii_to_utf8 (    char[]  source,
                        int     source_length,
                    out char[]  target,         // must be at least 2X larger than source !
                    out int     target_length);

//--------------------------------------------------------------------------

// compute target length of conversion.
// returns nb of illegal characters that were replaced by the replacement character (0 if none).

int utf8_to_ascii_target_length (    char[] source,
                                     int    source_length,
                                 out int    target_length);


// returns nb of illegal characters that were replaced by the replacement character (0 if none).

int utf8_to_ascii (    char[]  source,
                       int     source_length,
                   out char[]  target,           // must be at least as long as source !
                   out int     target_length,
                       char    replacement_char = '?');

//--------------------------------------------------------------------------

// compute target length of conversion.
// returns nb of illegal characters that were replaced by the replacement character (0 if none).

int utf8_to_utf16_target_length (    char[] source,
                                     int    source_length,
                                 out int    target_length,
                                     wchar  replacement_char = L'?');


// returns nb of illegal characters that were replaced by the replacement character (0 if none).

int utf8_to_utf16 (    char[]  source,
                       int     source_length,
                   out wchar[] target,           // must be at least 2X larger than source !
                   out int     target_length,
                       wchar   replacement_char = L'?');

//--------------------------------------------------------------------------

// compute target length of conversion.
// returns nb of illegal characters that were replaced by the replacement character (0 if none).

int utf16_to_utf8_target_length (    wchar[] source,
                                     int     source_length,
                                 out int     target_length,
                                     wchar   replacement_char = L'?');


// returns nb of illegal characters that were replaced by the replacement character (0 if none).

int utf16_to_utf8 (    wchar[] source,
                       int     source_length,
                   out char[]  target,           // must be at least 3X larger than source !
                   out int     target_length,
                       wchar   replacement_char = L'?');

//--------------------------------------------------------------------------

void ascii_to_utf16 (    char[]  source,
                         int     source_length,
                     out wchar[] target,         // must be at least as long as source !
                     out int     target_length); // will be same as source_length

//--------------------------------------------------------------------------

// returns nb of illegal characters that were replaced by the replacement character (0 if none).

int utf16_to_ascii (    wchar[] source,
                        int     source_length,
                    out char[]  target,           // must be at least as long as source !
                    out int     target_length,    // will be same as source_length
                        char    replacement_char = '?');

//--------------------------------------------------------------------------
