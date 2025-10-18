
// strformat.h : low-level routines for string formatting

//---------------------------------------------------------------

// convert to decimal string.
// returns active length of result string.

int itoa (long value, out string(20) buffer);
int witoa (long value, out wstring(20) buffer);

//---------------------------------------------------------------

// convert to unsigned hex string.
// returns active length of result string.

int itoh (long value, out string(16) buffer);
int witoh (long value, out wstring(16) buffer);

//---------------------------------------------------------------

#begin unsafe

typedef byte* PUT_CONTEXT;

// user function that returns 0 if OK, non-zero if error.
typedef int PUT (PUT_CONTEXT context, string s);

// returns 0 if OK, non-zero if put failed.
int format_string (PUT_CONTEXT context, PUT put, string format, object[] arg);

#end unsafe

//---------------------------------------------------------------

#begin unsafe

typedef short* WPUT_CONTEXT;

// user function that returns 0 if OK, non-zero if error.
typedef int WPUT (WPUT_CONTEXT context, wstring s);

// returns 0 if OK, non-zero if put failed.
int wformat_string (WPUT_CONTEXT context, WPUT put, wstring format, object[] arg);

#end unsafe

//---------------------------------------------------------------

#begin unsafe

typedef byte* GET_CONTEXT;

// user function that must return 0 if OK, -1 if read error, -2 if end-of-stream.
typedef int GET (GET_CONTEXT context, out char c);

typedef void UNGET (GET_CONTEXT context);

// returns 0 if OK, non-zero if an error occured.
// In particular :
// . -1 if a read-error occured during GET.
// . -2 if end-of-stream occured before all arguments received a value;
// . -3 if end-of-format string occured before all arguments received a value;
// . -4 if a syntax error occured in the stream;
// . -5 if an overflow occured while reading a numeric value or storing it in a numeric argument.

// a runtime occurs when the format string has an invalid format.

int scan_string (GET_CONTEXT context, GET get, UNGET unget, string format, out object[] arg);

#end unsafe

//---------------------------------------------------------------

#begin unsafe

typedef short* WGET_CONTEXT;

// user function that must return 0 if OK, -1 if read error, -2 if end-of-stream.
typedef int WGET (WGET_CONTEXT context, out wchar c);

typedef void WUNGET (WGET_CONTEXT context);

int wscan_string (WGET_CONTEXT context, WGET get, WUNGET unget, wstring format, out object[] arg);

#end unsafe

//---------------------------------------------------------------
