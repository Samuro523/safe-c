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

use opus_types, resampler_structs, os_support;
use resampler_private_ar2;

opus_int16 *silk_resampler_private_down_FIR_INTERPOL(
    opus_int16          *_out,
    opus_int32          *buf,
          opus_int16    *FIR_Coefs,
    int            FIR_Order,
    int            FIR_Fracs,
    opus_int32          max_index_Q16,
    opus_int32          index_increment_Q16
)
{
    opus_int32 index_Q16, res_Q6;
    opus_int32 *buf_ptr;
    opus_int32 interpol_ind;
          opus_int16 *interpol_ptr;

    switch( FIR_Order ) {
        case 18:
            for( index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16 ) {
                /* Integer part gives pointer to buffered input */
                buf_ptr = buf + ((index_Q16)>>(16));

                /* Fractional part gives interpolation coefficients */
                interpol_ind = ((((index_Q16 & 0xFFFF) >> 16) * (opus_int32)((opus_int16)(FIR_Fracs))) + ((((index_Q16 & 0xFFFF) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Fracs))) >> 16));

                /* Inner product */
                interpol_ptr = &FIR_Coefs[ 18 / 2 * interpol_ind ];
                res_Q6 = ((((buf_ptr[ 0 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 0 ]))) + ((((buf_ptr[ 0 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 0 ]))) >> 16));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 1 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 1 ]))) + ((((buf_ptr[ 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 1 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 2 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 2 ]))) + ((((buf_ptr[ 2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 2 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 3 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 3 ]))) + ((((buf_ptr[ 3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 3 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 4 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 4 ]))) + ((((buf_ptr[ 4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 4 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 5 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 5 ]))) + ((((buf_ptr[ 5 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 5 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 6 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 6 ]))) + ((((buf_ptr[ 6 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 6 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 7 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 7 ]))) + ((((buf_ptr[ 7 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 7 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 8 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 8 ]))) + ((((buf_ptr[ 8 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 8 ]))) >> 16)));
                interpol_ptr = &FIR_Coefs[ 18 / 2 * ( FIR_Fracs - 1 - interpol_ind ) ];
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 17 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 0 ]))) + ((((buf_ptr[ 17 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 0 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 16 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 1 ]))) + ((((buf_ptr[ 16 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 1 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 15 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 2 ]))) + ((((buf_ptr[ 15 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 2 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 14 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 3 ]))) + ((((buf_ptr[ 14 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 3 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 13 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 4 ]))) + ((((buf_ptr[ 13 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 4 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 12 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 5 ]))) + ((((buf_ptr[ 12 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 5 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 11 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 6 ]))) + ((((buf_ptr[ 11 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 6 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 10 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 7 ]))) + ((((buf_ptr[ 10 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 7 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((buf_ptr[ 9 ]) >> 16) * (opus_int32)((opus_int16)(interpol_ptr[ 8 ]))) + ((((buf_ptr[ 9 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(interpol_ptr[ 8 ]))) >> 16)));

                /* Scale down, saturate and store in output array */
                *(*&_out)++ = (opus_int16)((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1))));
            }
            break;
        case 24:
            for( index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16 ) {
                /* Integer part gives pointer to buffered input */
                buf_ptr = buf + ((index_Q16)>>(16));

                /* Inner product */
                res_Q6 = ((((((buf_ptr[ 0 ]) + (buf_ptr[ 23 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 0 ]))) + ((((((buf_ptr[ 0 ]) + (buf_ptr[ 23 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 0 ]))) >> 16));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 1 ]) + (buf_ptr[ 22 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 1 ]))) + ((((((buf_ptr[ 1 ]) + (buf_ptr[ 22 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 1 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 2 ]) + (buf_ptr[ 21 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 2 ]))) + ((((((buf_ptr[ 2 ]) + (buf_ptr[ 21 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 2 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 3 ]) + (buf_ptr[ 20 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 3 ]))) + ((((((buf_ptr[ 3 ]) + (buf_ptr[ 20 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 3 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 4 ]) + (buf_ptr[ 19 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 4 ]))) + ((((((buf_ptr[ 4 ]) + (buf_ptr[ 19 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 4 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 5 ]) + (buf_ptr[ 18 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 5 ]))) + ((((((buf_ptr[ 5 ]) + (buf_ptr[ 18 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 5 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 6 ]) + (buf_ptr[ 17 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 6 ]))) + ((((((buf_ptr[ 6 ]) + (buf_ptr[ 17 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 6 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 7 ]) + (buf_ptr[ 16 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 7 ]))) + ((((((buf_ptr[ 7 ]) + (buf_ptr[ 16 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 7 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 8 ]) + (buf_ptr[ 15 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 8 ]))) + ((((((buf_ptr[ 8 ]) + (buf_ptr[ 15 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 8 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 9 ]) + (buf_ptr[ 14 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 9 ]))) + ((((((buf_ptr[ 9 ]) + (buf_ptr[ 14 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 9 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 10 ]) + (buf_ptr[ 13 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 10 ]))) + ((((((buf_ptr[ 10 ]) + (buf_ptr[ 13 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 10 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 11 ]) + (buf_ptr[ 12 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 11 ]))) + ((((((buf_ptr[ 11 ]) + (buf_ptr[ 12 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 11 ]))) >> 16)));

                /* Scale down, saturate and store in output array */
                *(*&_out)++ = (opus_int16)((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1))));
            }
            break;
        case 36:
            for( index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16 ) {
                /* Integer part gives pointer to buffered input */
                buf_ptr = buf + ((index_Q16)>>(16));

                /* Inner product */
                res_Q6 = ((((((buf_ptr[ 0 ]) + (buf_ptr[ 35 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 0 ]))) + ((((((buf_ptr[ 0 ]) + (buf_ptr[ 35 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 0 ]))) >> 16));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 1 ]) + (buf_ptr[ 34 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 1 ]))) + ((((((buf_ptr[ 1 ]) + (buf_ptr[ 34 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 1 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 2 ]) + (buf_ptr[ 33 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 2 ]))) + ((((((buf_ptr[ 2 ]) + (buf_ptr[ 33 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 2 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 3 ]) + (buf_ptr[ 32 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 3 ]))) + ((((((buf_ptr[ 3 ]) + (buf_ptr[ 32 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 3 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 4 ]) + (buf_ptr[ 31 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 4 ]))) + ((((((buf_ptr[ 4 ]) + (buf_ptr[ 31 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 4 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 5 ]) + (buf_ptr[ 30 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 5 ]))) + ((((((buf_ptr[ 5 ]) + (buf_ptr[ 30 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 5 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 6 ]) + (buf_ptr[ 29 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 6 ]))) + ((((((buf_ptr[ 6 ]) + (buf_ptr[ 29 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 6 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 7 ]) + (buf_ptr[ 28 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 7 ]))) + ((((((buf_ptr[ 7 ]) + (buf_ptr[ 28 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 7 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 8 ]) + (buf_ptr[ 27 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 8 ]))) + ((((((buf_ptr[ 8 ]) + (buf_ptr[ 27 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 8 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 9 ]) + (buf_ptr[ 26 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 9 ]))) + ((((((buf_ptr[ 9 ]) + (buf_ptr[ 26 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 9 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 10 ]) + (buf_ptr[ 25 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 10 ]))) + ((((((buf_ptr[ 10 ]) + (buf_ptr[ 25 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 10 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 11 ]) + (buf_ptr[ 24 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 11 ]))) + ((((((buf_ptr[ 11 ]) + (buf_ptr[ 24 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 11 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 12 ]) + (buf_ptr[ 23 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 12 ]))) + ((((((buf_ptr[ 12 ]) + (buf_ptr[ 23 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 12 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 13 ]) + (buf_ptr[ 22 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 13 ]))) + ((((((buf_ptr[ 13 ]) + (buf_ptr[ 22 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 13 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 14 ]) + (buf_ptr[ 21 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 14 ]))) + ((((((buf_ptr[ 14 ]) + (buf_ptr[ 21 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 14 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 15 ]) + (buf_ptr[ 20 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 15 ]))) + ((((((buf_ptr[ 15 ]) + (buf_ptr[ 20 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 15 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 16 ]) + (buf_ptr[ 19 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 16 ]))) + ((((((buf_ptr[ 16 ]) + (buf_ptr[ 19 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 16 ]))) >> 16)));
                res_Q6 = ((res_Q6) + ((((((buf_ptr[ 17 ]) + (buf_ptr[ 18 ]))) >> 16) * (opus_int32)((opus_int16)(FIR_Coefs[ 17 ]))) + ((((((buf_ptr[ 17 ]) + (buf_ptr[ 18 ]))) & 0x0000FFFF) * (opus_int32)((opus_int16)(FIR_Coefs[ 17 ]))) >> 16)));

                /* Scale down, saturate and store in output array */
                *(*&_out)++ = (opus_int16)((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1))));
            }
            break;
        default:
            break;
    }
    return _out;
}

/* Resample with a 2nd order AR filter followed by FIR interpolation */
public
void silk_resampler_private_down_FIR(
    byte                            *SS,            /* I/O  Resampler state             */
    opus_int16                      _out*,          /* O    Output signal               */
          opus_int16                in*,           /* I    Input signal                */
    opus_int32                      inLen           /* I    Number of input samples     */
)
{
    silk_resampler_state_struct *S = (silk_resampler_state_struct *)SS;
    opus_int32 nSamplesIn;
    opus_int32 max_index_Q16, index_increment_Q16;
    opus_int32 buf[ ( 10 * 48 ) + 36 ];
          opus_int16 *FIR_Coefs;

    /* Copy buffered samples to start of buffer */
    memcpy((byte*)&(buf), (byte*)&(S->sFIR), (S->FIR_Order * ((int)(opus_int32 ' size  ))));

    FIR_Coefs = &S->Coefs[ 2 ];

    /* Iterate over blocks of frameSizeIn input samples */
    index_increment_Q16 = S->invRatio_Q16;
    while( true ) {
        nSamplesIn = (((inLen) < (S->batchSize)) ? (inLen) : (S->batchSize));

        /* Second-order AR filter (output in Q8) */
        silk_resampler_private_AR2( &S->sIIR, &buf[ S->FIR_Order ], in, S->Coefs, nSamplesIn );

        max_index_Q16 = ((opus_int32)((opus_uint32)(nSamplesIn)<<(16)));

        /* Interpolate filtered signal */
        *&_out = silk_resampler_private_down_FIR_INTERPOL( _out, &buf, FIR_Coefs, S->FIR_Order,
            S->FIR_Fracs, max_index_Q16, index_increment_Q16 );

        *&in += nSamplesIn;
        *&inLen -= nSamplesIn;

        if( inLen > 1 ) {
            /* More iterations to do; copy last part of filtered signal to beginning of buffer */
            memcpy((byte*)&(buf), (byte*)(&buf[ nSamplesIn ]), (S->FIR_Order * ((int)(opus_int32 ' size  ))));
        } else {
            break;
        }
    }

    /* Copy last part of filtered signal to the state for the next call */
    memcpy((byte*)&(S->sFIR), (byte*)(&buf[ nSamplesIn ]), (S->FIR_Order * ((int)(opus_int32 ' size  ))));
}
#end unsafe
