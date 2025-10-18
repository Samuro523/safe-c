
// integer.h : operations on very large unsigned integers

const uint INTEGER_SIZE = 1024;    // bytes

struct INTEGER;    // private integer type of 1024 bytes

//--------------------------------------------------------------------------

// build an integer
void make_integer (uint value, out INTEGER result);

//--------------------------------------------------------------------------

// build an integer from a byte sequence (lowest index = lower byte)
void make_large_integer (byte[] value, out INTEGER result);

//--------------------------------------------------------------------------

// retrieve an integer (the function aborts if a > uint'max)
uint integer_value (INTEGER a);

//--------------------------------------------------------------------------

// fast copy 'source' to 'target'
void copy_integer (INTEGER source, out INTEGER target);

//--------------------------------------------------------------------------

// the following functions return :
//  0 if OK,
// -1 in case of overflow, underflow or division by zero.
// note: a, b, n and result can reference identical variables

int add_integer      (INTEGER a, INTEGER b, out INTEGER result);
int subtract_integer (INTEGER a, INTEGER b, out INTEGER result);
int multiply_integer (INTEGER a, INTEGER b, out INTEGER result);
int divide_integer   (INTEGER a, INTEGER b, out INTEGER result);
int modulo_integer   (INTEGER a, INTEGER b, out INTEGER result);

int divide_modulo_integer (INTEGER a, INTEGER b, out INTEGER quotient, out INTEGER remainder);

// a*b mod n
int multiply_modulo_integer (INTEGER a, INTEGER b, INTEGER n, out INTEGER result);

// a^b mod n
int exponent_modulo_integer (INTEGER a, INTEGER b, INTEGER n, out INTEGER result);

//--------------------------------------------------------------------------

// returns -1 if a<b, 0 if a==b, +1 if a>b
int compare_integer     (INTEGER a, INTEGER b);
int compare_integer_int (INTEGER a, uint    b);

//--------------------------------------------------------------------------

// convert integer into decimal integer string.
// buffer size examples :
//
//       INTEGER_SIZE   bits    buffer_size
//       ============   ====    ===========
//           8           64        20
//          16          128        39
//          32          256        78
//          64          512       155
//         128         1024       309
//         256         2048       617
//         512         4096      1234
//        1024         8192      2467
//
// the function aborts if the buffer size is too small.

void integer_to_string (INTEGER a, out string buffer);

//--------------------------------------------------------------------------

// convert integer into hexadecimal integer string.
// the buffer size should be >= (2*INTEGER_SIZE+1)
// the function aborts if the buffer size is too small.

void integer_to_hex_string (INTEGER a, out string buffer);

//--------------------------------------------------------------------------

// convert decimal/hexadecimal integer string into an INTEGER.         
// returns 0 if OK, -1 in case of overflow, -2 in case of syntax error 

int string_to_integer     (string str, out INTEGER result);
int hex_string_to_integer (string str, out INTEGER result);

//--------------------------------------------------------------------------

// returns nb of bits needed to hold 'a' (range 0 .. 8*INTEGER_SIZE)
uint integer_bit_size (INTEGER a);

//--------------------------------------------------------------------------

// returns nb of bytes needed to hold 'a' (range 0 .. INTEGER_SIZE)
uint integer_byte_size (INTEGER a);

//--------------------------------------------------------------------------

// check that 'a' has a valid intern format.
bool integer_is_valid (INTEGER a);

// check that 'a' has a valid intern format. the program aborts if not.
void check_integer (INTEGER a);

//--------------------------------------------------------------------------

// retrieve lower bytes of integer sequence
void extract_integer (INTEGER a, out byte[] value);

//--------------------------------------------------------------------------

// computes the largest common divisor of 'a' and 'b'      
// 'a' and 'b' must be >= 1, otherwise the program aborts. 

void pgcd (INTEGER a, INTEGER b, out INTEGER result);

//--------------------------------------------------------------------------

// computes the smallest common multiple of 'a' and 'b'    
// 'a' and 'b' must be >= 1, otherwise the program aborts. 
// returns 0 if OK, -1 in case of overflow.                

int ppcm (INTEGER a, INTEGER b, out INTEGER result);

//--------------------------------------------------------------------------

// given the equation: (a * x) mod n = 1  
// with  0 < a < n  and  pgcd (a, n) = 1, 
// the function computes x.               
// returns 0 if OK, -1 if the constraints 
// on 'a' and 'n' are violated.           

int inverse_multiply_modulo_integer (INTEGER a, INTEGER n, out INTEGER x);

//--------------------------------------------------------------------------
