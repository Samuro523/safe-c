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

use opus_types;
use entcode, tables, entenc, entdec;


/*#define silk_enc_map(a)                ((a) > 0 ? 1 : 0)*/
/*#define silk_dec_map(a)                ((a) > 0 ? 1 : -1)*/
/* shifting avoids if-statement */

/* Encodes signs of excitation */
public
void silk_encode_signs(
    ec_enc                      *psRangeEnc,                        /* I/O  Compressor data structure                   */
          int1             pulses*,                           /* I    pulse signal                                */
    int                    length,                             /* I    length of input                             */
          int              signalType,                         /* I    Signal type                                 */
          int              quantOffsetType,                    /* I    Quantization offset type                    */
          int              sum_pulses[ ( ( ( 5 * 4 ) * 16 ) / 16 ) ]   /* I    Sum of absolute pulses per block            */
)
{
    int         i, j, p;
    byte       icdf[ 2 ];
          int1  *q_ptr;
          byte *icdf_ptr;

    icdf[ 1 ] = 0;
    q_ptr = pulses;
    i = ((opus_int32)((opus_int16)(7)) * (opus_int32)((opus_int16)(((quantOffsetType) + ((opus_int32)((opus_uint32)((signalType))<<((1))))))));
    icdf_ptr = &silk_sign_iCDF[ i ];
    *&length = ((length + 16/2)>>(4));
    for( i = 0; i < length; i++ ) {
        p = sum_pulses[ i ];
        if( p > 0 ) {
            icdf[ 0 ] = icdf_ptr[ (((p & 0x1F) < (6)) ? (p & 0x1F) : (6)) ];
            for( j = 0; j < 16; j++ ) {
                if( q_ptr[ j ] != 0 ) {
                    ec_enc_icdf( psRangeEnc, ( (((q_ptr[ j ]))>>(15)) + 1 ), &icdf, 8 );
                }
            }
        }
        q_ptr += 16;
    }
}

/* Decodes signs of excitation */
public
void silk_decode_signs(
    ec_dec                      *psRangeDec,                        /* I/O  Compressor data structure                   */
    int                    pulses*,                           /* I/O  pulse signal                                */
    int                    length,                             /* I    length of input                             */
          int              signalType,                         /* I    Signal type                                 */
          int              quantOffsetType,                    /* I    Quantization offset type                    */
          int              sum_pulses[ ( ( ( 5 * 4 ) * 16 ) / 16 ) ]   /* I    Sum of absolute pulses per block            */
)
{
    int         i, j, p;
    byte       icdf[ 2 ];
    int         *q_ptr;
          byte *icdf_ptr;

    icdf[ 1 ] = 0;
    q_ptr = pulses;
    i = ((opus_int32)((opus_int16)(7)) * (opus_int32)((opus_int16)(((quantOffsetType) + ((opus_int32)((opus_uint32)((signalType))<<((1))))))));
    icdf_ptr = &silk_sign_iCDF[ i ];
    *&length = ((length + 16/2)>>(4));
    for( i = 0; i < length; i++ ) {
        p = sum_pulses[ i ];
        if( p > 0 ) {
            icdf[ 0 ] = icdf_ptr[ (((p & 0x1F) < (6)) ? (p & 0x1F) : (6)) ];
            for( j = 0; j < 16; j++ ) {
                if( q_ptr[ j ] > 0 ) {
                    /* attach sign */

                    /* implementation with shift, subtraction, multiplication */
                    q_ptr[ j ] *= ( ((opus_int32)((opus_uint32)((ec_dec_icdf( psRangeDec, &icdf, 8 )))<<(1))) - 1 );
                }
            }
        }
        q_ptr += 16;
    }
}
#end unsafe
