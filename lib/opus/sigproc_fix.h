#begin unsafe
/***********************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the 
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

/*#define silk_MACRO_COUNT */          /* Used to enable WMOPS counting */

use opus_types;

/********************************************************************/
/*                    SIGNAL PROCESSING FUNCTIONS                   */
/********************************************************************/


void silk_insertion_sort_increasing(
    opus_int32                  *a,                 /* I/O   Unsorted / Sorted vector                                   */
    int                    *idx,               /* O     Index vector for the sorted elements                       */
          int              L,                  /* I     Vector length                                              */
          int              K                   /* I     Number of correctly sorted positions                       */
);




void silk_insertion_sort_increasing_all_values_int16(
     opus_int16                 *a,                 /* I/O   Unsorted / Sorted vector                                   */
           int             L                   /* I     Vector length                                              */
);




/********************************************************************/
/*                                MACROS                            */
/********************************************************************/

/* Rotate a32 right by 'rot' bits. Negative rot values result in rotating
   left. Output is 32bit int.
   Note: contemporary compilers recognize the C expression below and
   compile it into a 'ror' instruction if available. No need for inline ASM! */
opus_int32 silk_ROR32( opus_int32 a32, int rot );


/* Macro to convert floating-point      ants to fixed-point */

/* silk_min() versions with typecast in the function call */
int silk_min_int(int a, int b);

opus_int16 silk_min_16(opus_int16 a, opus_int16 b);

opus_int32 silk_min_32(opus_int32 a, opus_int32 b);

int8 silk_min_64(int8 a, int8 b);

/* silk_min() versions with typecast in the function call */
int silk_max_int(int a, int b);

opus_int16 silk_max_16(opus_int16 a, opus_int16 b);

opus_int32 silk_max_32(opus_int32 a, opus_int32 b);

int8 silk_max_64(int8 a, int8 b);

#end unsafe
