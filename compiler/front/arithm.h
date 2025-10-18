
// arithm.h : arithmetic operations with overflow check.

/***************************************************************************/

#if 1
  typedef long INTEGER;
#else
  typedef tiny INTEGER;
#endif

const INTEGER MIN_INTEGER = INTEGER'min;
const INTEGER MAX_INTEGER = INTEGER'max;

/***************************************************************************/

// returns 0 if OK or -1 if overflow

int add_int (INTEGER a, INTEGER b, out INTEGER r);
int sub_int (INTEGER a, INTEGER b, out INTEGER r);
int mul_int (INTEGER a, INTEGER b, out INTEGER r);
int div_int (INTEGER a, INTEGER b, out INTEGER r);
int mod_int (INTEGER a, INTEGER b, out INTEGER r);
int neg_int (INTEGER a, out INTEGER r);

/***************************************************************************/
