
// libmath.h

#begin unsafe

[extern "libm.so"]
double fabs (double x);

[extern "libm.so"]
double sqrt (double x);

[extern "libm.so"]
double round (double x);

[extern "libm.so"]
double sin (double x);

[extern "libm.so"]
double cos (double x);

[extern "libm.so"]
double atan2 (double y, double x);

[extern "libm.so"]
double log (double x);

[extern "libm.so"]
double log2 (double x);

[extern "libm.so"]
double log10 (double x);

#end unsafe
