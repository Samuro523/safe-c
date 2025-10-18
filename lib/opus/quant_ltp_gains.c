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

use opus_types, tables, vq_wmat_ec, os_support;

public
void silk_quant_LTP_gains(
    opus_int16                  B_Q14*, // [ 4 * 5 ],          /* I/O  (un)quantized LTP gains         */
    int1                   cbk_index*,  // [ 4 ],                  /* O    Codebook Index                  */
    int1                   *periodicity_index,                         /* O    Periodicity Index               */
          opus_int32            W_Q18*,  // [ 4*5*5 ],  /* I    Error Weights in Q18            */
    int                    mu_Q9,                                      /* I    Mu value (R/D tradeoff)         */
    int                    lowComplexity,                              /* I    Flag for low complexity         */
          int              nb_subfr                                    /* I    number of subframes             */
)
{
    int             j, k, cbk_size;
    int1            temp_idx[ 4 ];
          byte     *cl_ptr_Q5;
          int1      *cbk_ptr_Q7;
          opus_int16     *b_Q14_ptr;
          opus_int32     *W_Q18_ptr;
    opus_int32           rate_dist_Q14_subfr, rate_dist_Q14, min_rate_dist_Q14;

	clear temp_idx;
    /***************************************************/
    /* iterate over different codebooks with different */
    /* rates/distortions, and choose best */
    /***************************************************/
    min_rate_dist_Q14 = 0x7FFFFFFF;
    for( k = 0; k < 3; k++ ) {
        cl_ptr_Q5  = &silk_LTP_gain_BITS_Q5_ptrs[ k ];
        cbk_ptr_Q7 = &silk_LTP_vq_ptrs_Q7[        k ][0];
        cbk_size   = silk_LTP_vq_sizes[          k ];

        /* Set up pointer to first subframe */
        W_Q18_ptr = W_Q18;
        b_Q14_ptr = B_Q14;

        rate_dist_Q14 = 0;
        for( j = 0; j < nb_subfr; j++ ) {
            silk_VQ_WMat_EC(
                &temp_idx[ j ],         /* O    index of best codebook vector                           */
                &rate_dist_Q14_subfr,   /* O    best weighted quantization error + mu * rate            */
                b_Q14_ptr,              /* I    input vector to be quantized                            */
                W_Q18_ptr,              /* I    weighting matrix                                        */
                cbk_ptr_Q7,             /* I    codebook                                                */
                cl_ptr_Q5,              /* I    code length for each codebook vector                    */
                mu_Q9,                  /* I    tradeoff between weighted error and rate                */
                cbk_size                /* I    number of vectors in codebook                           */
            );

            rate_dist_Q14 = ((((uint)((rate_dist_Q14)+(rate_dist_Q14_subfr)) & 0x80000000))!=0 ? 0x7FFFFFFF : ((rate_dist_Q14)+(rate_dist_Q14_subfr)));

            b_Q14_ptr += 5;
            W_Q18_ptr += 5 * 5;
        }

        /* Avoid never finding a codebook */
        rate_dist_Q14 = (((0x7FFFFFFF - 1) < (rate_dist_Q14)) ? (0x7FFFFFFF - 1) : (rate_dist_Q14));

        if( rate_dist_Q14 < min_rate_dist_Q14 ) {
            min_rate_dist_Q14 = rate_dist_Q14;
            *periodicity_index = (int1)k;
            memcpy((byte*)(cbk_index), (byte*)&(temp_idx), (nb_subfr * ((int)(int1 ' size  ))));
        }

        /* Break early in low-complexity mode if rate distortion is below threshold */
        if( lowComplexity!=0 && ( rate_dist_Q14 < silk_LTP_gain_middle_avg_RD_Q14 ) ) {
            break;
        }
    }

    cbk_ptr_Q7 = &silk_LTP_vq_ptrs_Q7[ *periodicity_index ][0];
    for( j = 0; j < nb_subfr; j++ ) {
        for( k = 0; k < 5; k++ ) {
            B_Q14[ j * 5 + k ] = (opus_int16)(((opus_int32)((opus_uint32)(cbk_ptr_Q7[ cbk_index[ j ] * 5 + k ])<<(7))));
        }
    }
}

#end unsafe
