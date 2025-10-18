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


/* Entropy      rained matrix-weighted VQ, hard-coded to 5-element vectors, for a single input data vector */
public
void silk_VQ_WMat_EC(
    int1                   *ind,                           /* O    index of best codebook vector               */
    opus_int32                  *rate_dist_Q14,                 /* O    best weighted quant error + mu * rate       */
          opus_int16            *in_Q14,                        /* I    input vector to be quantized                */
          opus_int32            *W_Q18,                         /* I    weighting matrix                            */
          int1             *cb_Q7,                         /* I    codebook                                    */
          byte            *cl_Q5,                         /* I    code length for each codebook vector        */
          int              mu_Q9,                          /* I    tradeoff betw. weighted error and rate      */
    int                    L                               /* I    number of vectors in codebook               */
)
{
    int   k;
          int1 *cb_row_Q7;
    opus_int16 diff_Q14[ 5 ];
    opus_int32 sum1_Q14, sum2_Q16;

	clear diff_Q14;
	
    /* Loop over codebook */
    *rate_dist_Q14 = 0x7FFFFFFF;
    cb_row_Q7 = cb_Q7;
    for( k = 0; k < L; k++ ) {
        diff_Q14[ 0 ] = (opus_int16)(in_Q14[ 0 ] - ((opus_int32)((opus_uint32)(cb_row_Q7[ 0 ])<<(7))));
        diff_Q14[ 1 ] = (opus_int16)(in_Q14[ 1 ] - ((opus_int32)((opus_uint32)(cb_row_Q7[ 1 ])<<(7))));
        diff_Q14[ 2 ] = (opus_int16)(in_Q14[ 2 ] - ((opus_int32)((opus_uint32)(cb_row_Q7[ 2 ])<<(7))));
        diff_Q14[ 3 ] = (opus_int16)(in_Q14[ 3 ] - ((opus_int32)((opus_uint32)(cb_row_Q7[ 3 ])<<(7))));
        diff_Q14[ 4 ] = (opus_int16)(in_Q14[ 4 ] - ((opus_int32)((opus_uint32)(cb_row_Q7[ 4 ])<<(7))));

        /* Weighted rate */
        sum1_Q14 = ((opus_int32)((opus_int16)(mu_Q9)) * (opus_int32)((opus_int16)(cl_Q5[ k ])));

        ;

        /* first row of W_Q18 */
        sum2_Q16 = ((((W_Q18[ 1 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 1 ]))) + ((((W_Q18[ 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 1 ]))) >> 16));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 2 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) + ((((W_Q18[ 2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) >> 16)));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 3 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) + ((((W_Q18[ 3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) >> 16)));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 4 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) + ((((W_Q18[ 4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) >> 16)));
        sum2_Q16 = ((opus_int32)((opus_uint32)(sum2_Q16)<<(1)));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 0 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 0 ]))) + ((((W_Q18[ 0 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 0 ]))) >> 16)));
        sum1_Q14 = ((sum1_Q14) + ((((sum2_Q16) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 0 ]))) + ((((sum2_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 0 ]))) >> 16)));

        /* second row of W_Q18 */
        sum2_Q16 = ((((W_Q18[ 7 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) + ((((W_Q18[ 7 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) >> 16));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 8 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) + ((((W_Q18[ 8 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) >> 16)));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 9 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) + ((((W_Q18[ 9 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) >> 16)));
        sum2_Q16 = ((opus_int32)((opus_uint32)(sum2_Q16)<<(1)));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 6 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 1 ]))) + ((((W_Q18[ 6 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 1 ]))) >> 16)));
        sum1_Q14 = ((sum1_Q14) + ((((sum2_Q16) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 1 ]))) + ((((sum2_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 1 ]))) >> 16)));

        /* third row of W_Q18 */
        sum2_Q16 = ((((W_Q18[ 13 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) + ((((W_Q18[ 13 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) >> 16));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 14 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) + ((((W_Q18[ 14 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) >> 16)));
        sum2_Q16 = ((opus_int32)((opus_uint32)(sum2_Q16)<<(1)));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 12 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) + ((((W_Q18[ 12 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) >> 16)));
        sum1_Q14 = ((sum1_Q14) + ((((sum2_Q16) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) + ((((sum2_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 2 ]))) >> 16)));

        /* fourth row of W_Q18 */
        sum2_Q16 = ((((W_Q18[ 19 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) + ((((W_Q18[ 19 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) >> 16));
        sum2_Q16 = ((opus_int32)((opus_uint32)(sum2_Q16)<<(1)));
        sum2_Q16 = ((sum2_Q16) + ((((W_Q18[ 18 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) + ((((W_Q18[ 18 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) >> 16)));
        sum1_Q14 = ((sum1_Q14) + ((((sum2_Q16) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) + ((((sum2_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 3 ]))) >> 16)));

        /* last row of W_Q18 */
        sum2_Q16 = ((((W_Q18[ 24 ]) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) + ((((W_Q18[ 24 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) >> 16));
        sum1_Q14 = ((sum1_Q14) + ((((sum2_Q16) >> 16) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) + ((((sum2_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(diff_Q14[ 4 ]))) >> 16)));

        ;

        /* find best */
        if( sum1_Q14 < *rate_dist_Q14 ) {
            *rate_dist_Q14 = sum1_Q14;
            *ind = (int1)k;
        }

        /* Go to next cbk vector */
        cb_row_Q7 += 5;
    }
}
#end unsafe
