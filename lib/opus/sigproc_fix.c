
use opus_types;

#begin unsafe

/********************************************************************/
/*                                MACROS                            */
/********************************************************************/

/* Rotate a32 right by 'rot' bits. Negative rot values result in rotating
   left. Output is 32bit int.
   Note: contemporary compilers recognize the C expression below and
   compile it into a 'ror' instruction if available. No need for inline ASM! */
public opus_int32 silk_ROR32( opus_int32 a32, int rot )
{
    opus_uint32 x = (opus_uint32) a32;
    opus_uint32 r = (opus_uint32) rot;
    opus_uint32 m = (opus_uint32) -rot;
    if( rot == 0 ) {
        return a32;
    } else if( rot < 0 ) {
        return (opus_int32) ((x << m) | (x >> (32 - m)));
    } else {
        return (opus_int32) ((x << (32 - r)) | (x >> r));
    }
}

/* Allocate opus_int16 alligned to 4-byte memory address */



/* Useful Macros that can be adjusted to other platforms */

/* Fixed point macros */

/* (a32 * b32) output have to be 32bit int */

/* (a32 * b32) output have to be 32bit uint */

/* a32 + (b32 * c32) output have to be 32bit int */

/* a32 + (b32 * c32) output have to be 32bit uint */

/* ((a32 >> 16)  * (b32 >> 16)) output have to be 32bit int */

/* a32 + ((a32 >> 16)  * (b32 >> 16)) output have to be 32bit int */

/* (a32 * b32) */

/* Adds two signed 32-bit values in a way that can overflow, while not relying on undefined behaviour
   (just standard two's complement implementation-specific behaviour) */

/* Subtractss two signed 32-bit values in a way that can overflow, while not relying on undefined behaviour
   (just standard two's complement implementation-specific behaviour) */

/* Multiply-accumulate macros that allow overflow in the addition (ie, no asserts in debug mode) */

/* These macros enables checking for overflow in silk_API_Debug.h*/

/* Saturation for positive input values */

/* Add with saturation for positive input values */

/* saturates before shifting */

/* Requires that shift > 0 */

/* Number of rightshift required to fit the multiplication */

/* Macro to convert floating-point      ants to fixed-point */

/* silk_min() versions with typecast in the function call */
public   int silk_min_int(int a, int b)
{
    return (((a) < (b)) ? (a) : (b));
}
public   opus_int16 silk_min_16(opus_int16 a, opus_int16 b)
{
    return (opus_int16)((((a) < (b)) ? (a) : (b)));
}
public  opus_int32 silk_min_32(opus_int32 a, opus_int32 b)
{
    return (((a) < (b)) ? (a) : (b));
}
public  int8 silk_min_64(int8 a, int8 b)
{
    return (((a) < (b)) ? (a) : (b));
}

/* silk_min() versions with typecast in the function call */
public  int silk_max_int(int a, int b)
{
    return (((a) > (b)) ? (a) : (b));
}
public  opus_int16 silk_max_16(opus_int16 a, opus_int16 b)
{
    return (opus_int16)( (((a) > (b)) ? (a) : (b)) );
}
public  opus_int32 silk_max_32(opus_int32 a, opus_int32 b)
{
    return (((a) > (b)) ? (a) : (b));
}
public  int8 silk_max_64(int8 a, int8 b)
{
    return (((a) > (b)) ? (a) : (b));
}

/* PSEUDO-RANDOM GENERATOR                                                          */
/* Make sure to store the result as the seed for the next call (also in between     */
/* frames), otherwise result won't be random at all. When only using some of the    */
/* bits, take the most significant bits by right-shifting.                          */

/*  Add some multiplication functions that can be easily mapped to ARM. */

/*    silk_SMMUL: Signed top word multiply.
          ARMv6        2 instruction cycles.
          ARMv3M+      3 instruction cycles. use SMULL and ignore LSB registers.(except xM)*/
/*#define silk_SMMUL(a32, b32)                (opus_int32)silk_RSHIFT(silk_SMLAL(silk_SMULWB((a32), (b32)), (a32), silk_RSHIFT_ROUND((b32), 16)), 16)*/
/* the following seems faster on x86 */







/* Insertion sort (fast for already almost sorted arrays):   */
/* Best case:  O(n)   for an already sorted array            */
/* Worst case: O(n^2) for an inversely sorted array          */
/*                                                           */
/* Shell short:    http://en.wikipedia.org/wiki/Shell_sort   */



public void silk_insertion_sort_increasing(
    opus_int32           *a,             /* I/O   Unsorted / Sorted vector               */
    int             *idx,           /* O     Index vector for the sorted elements   */
          int       L,              /* I     Vector length                          */
          int       K               /* I     Number of correctly sorted positions   */
)
{
    opus_int32    value;
    int        i, j;

    /* Safety checks */
    ;
    ;
    ;

    /* Write start indices in index vector */
    for( i = 0; i < K; i++ ) {
        idx[ i ] = i;
    }

    /* Sort vector elements by value, increasing order */
    for( i = 1; i < K; i++ ) {
        value = a[ i ];
        for( j = i - 1; ( j >= 0 ) && ( value < a[ j ] ); j-- ) {
            a[ j + 1 ]   = a[ j ];       /* Shift value */
            idx[ j + 1 ] = idx[ j ];     /* Shift index */
        }
        a[ j + 1 ]   = value;   /* Write value */
        idx[ j + 1 ] = i;       /* Write index */
    }

    /* If less than L values are asked for, check the remaining values, */
    /* but only spend CPU to ensure that the K first values are correct */
    for( i = K; i < L; i++ ) {
        value = a[ i ];
        if( value < a[ K - 1 ] ) {
            for( j = K - 2; ( j >= 0 ) && ( value < a[ j ] ); j-- ) {
                a[ j + 1 ]   = a[ j ];       /* Shift value */
                idx[ j + 1 ] = idx[ j ];     /* Shift index */
            }
            a[ j + 1 ]   = value;   /* Write value */
            idx[ j + 1 ] = i;       /* Write index */
        }
    }
}

public void silk_insertion_sort_increasing_all_values_int16(
     opus_int16                 *a,                 /* I/O   Unsorted / Sorted vector                                   */
           int             L                   /* I     Vector length                                              */
)
{
    int    value;
    int    i, j;

    /* Safety checks */
    ;

    /* Sort vector elements by value, increasing order */
    for( i = 1; i < L; i++ ) {
        value = a[ i ];
        for( j = i - 1; ( j >= 0 ) && ( value < a[ j ] ); j-- ) {
            a[ j + 1 ] = a[ j ]; /* Shift value */
        }
        a[ j + 1 ] = (opus_int16)value; /* Write value */
    }
}

#end unsafe
