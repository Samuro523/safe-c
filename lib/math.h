
// math.h

//--------------------------------------------------------------------------

// basic functions

double fabs  (double x);   // absolute value
double sqrt  (double x);   // square root, with x >= 0.
double round (double x);   // rounds to nearest integer
double ceil  (double x);   // rounds up
double floor (double x);   // rounds down

double fmin (double a, double b);             // minimum
double fmax (double a, double b);             // maximum
double fmin3 (double a, double b, double c);  // minimum of 3 values
double fmax3 (double a, double b, double c);  // maximum of 3 values

//--------------------------------------------------------------------------

// trigonometric functions

const double PI = 3.141592653589793238;
double pi();        // pi in internal 66-bit precision

double sin (double x);
double cos (double x);
double tan (double x);     // same as sin(x)/cos(x), with cos(x) != 0.

double asin (double x);    // with x in [-1..+1]
double acos (double x);    // with x in [-1..+1]
double atan (double x);
double atan2 (double y, double x);   // valid for all values of x and y.

//--------------------------------------------------------------------------

// exponential, logarithm and power functions

double exp   (double x);   // e exponent x
double ln    (double x);   // log e, with x > 0
double log2  (double x);   // log 2, with x > 0
double log10 (double x);   // log 10, with x > 0

// if base == 0, exponent must be >= 0;
// if base < 0, exponent must be an integer value.
double pow (double base, double exponent);      // base to the power of exponent

//--------------------------------------------------------------------------

// hyperbolic functions

double sinh (double x);
double cosh (double x);
double tanh (double x);

double asinh (double x);
double acosh (double x);
double atanh (double x);

//--------------------------------------------------------------------------

// infinite test

bool isINF (double x);     // test if infinite

//--------------------------------------------------------------------------

// returns (int)floor(log2(f)), for f > 0.0
int log2i (float f);

//--------------------------------------------------------------------------
