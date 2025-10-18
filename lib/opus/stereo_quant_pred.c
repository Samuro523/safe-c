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

use opus_types, tables;

/* Quantize mid/side predictors */
public
void silk_stereo_quant_pred(
    opus_int32                  pred_Q13*,                     /* I/O  Predictors (out: quantized)                 */
    int1                   ix*[ 3 ]                    /* O    Quantization indices                        */
)
{
    int   i, j, n;
    opus_int32 low_Q13, step_Q13, lvl_Q13, err_min_Q13, err_Q13, quant_pred_Q13 = 0;
    bool done;
	
    /* Quantize */
    for( n = 0; n < 2; n++ ) {
        /* Brute-force search over quantization levels */
        err_min_Q13 = 0x7FFFFFFF;
        done = false;
		for( i = 0; (!done) && i < 16 - 1; i++ ) {
            low_Q13 = silk_stereo_pred_quant_Q13[ i ];
            step_Q13 = 
((((silk_stereo_pred_quant_Q13[ i + 1 ] - low_Q13) >> 16) * (opus_int32)((opus_int16)(((opus_int32)((0.5 / 5.0) * (float)((int8)1 << (16)) + 0.5))))) + ((((silk_stereo_pred_quant_Q13[ i + 1 ] - low_Q13) & 0x0000FFFF) * (opus_int32)((opus_int16)(((opus_int32)((0.5 / 5.0) * (float)((int8)1 << (16)) + 0.5))))) >> 16));
            for( j = 0; (!done) && j < 5; j++ ) {
                lvl_Q13 = ((low_Q13) + ((opus_int32)((opus_int16)(step_Q13))) * (opus_int32)((opus_int16)(2 * j + 1)));
                err_Q13 = (((pred_Q13[ n ] - lvl_Q13) > 0) ? (pred_Q13[ n ] - lvl_Q13) : -(pred_Q13[ n ] - lvl_Q13));
                if( err_Q13 < err_min_Q13 ) {
                    err_min_Q13 = err_Q13;
                    quant_pred_Q13 = lvl_Q13;
                    ix[ n ][ 0 ] = (int1)i;
                    ix[ n ][ 1 ] = (int1)j;
                } else {
                    /* Error increasing, so we're past the optimum */
                    done = true;  // goto done
                }
            }
        }
//        done:
        ix[ n ][ 2 ]  = (int1)((opus_int32)((ix[ n ][ 0 ]) / (3)));
        ix[ n ][ 0 ] -= (int1)(ix[ n ][ 2 ] * 3);
        pred_Q13[ n ] = quant_pred_Q13;
    }

    /* Subtract second from first predictor (helps when actually applying these) */
    pred_Q13[ 0 ] -= pred_Q13[ 1 ];
}
#end unsafe
