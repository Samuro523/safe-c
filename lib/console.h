
//----------------------------------------------------------------------------
// console.h : read and write on command line console.
//----------------------------------------------------------------------------

// causes a runtime error if the format string is a variable and has a bad format.
// see strings.h for a description of parameter 'format'.

void printf (string format, object[] arg);

//----------------------------------------------------------------------------

#if WINDOWS

// returns 0 if OK, non-zero if an error occured.
// In particular :
// . -1 if a read-error occured during reading characters;
// . -2 if end-of-stream occured before all arguments received a value;
// . -3 if end-of-format string occured before all arguments received a value;
// . -4 if a syntax error occured in the stream;
// . -5 if an overflow occured while reading a numeric value or storing it in a numeric argument.

// causes a runtime error if the format string is a variable and has a bad format.
// see strings.h for a description of parameter 'format'.

// bugs: cannot be called from several threads simultaneously
//    as it uses a global variable for sharing buffered input.

int scanf (string format, out object[] arg);

#endif

//----------------------------------------------------------------------------
