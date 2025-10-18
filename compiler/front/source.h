
// source.h

/**********************************************************************/

// load source file into heap memory block

int load_source_file (string filename, out byte[]^ source);

/**********************************************************************/

// illegal characters are converted to zero.
// a final zero trailing character is appended.

int convert_source_file_to_utf16 (    byte[]   source,
                                  out wchar[]^ target);

/**********************************************************************/
