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

use opus_types, os_support;
use structs, tables, inlines, lpc_analysis_filter;

/**********************************************************/
/* Core decoder. Performs inverse NSQ operation LTP + LPC */
/**********************************************************/
public
void silk_decode_core(
    silk_decoder_state          *psDec,                         /* I/O  Decoder state                               */
    silk_decoder_control        *psDecCtrl,                     /* I    Decoder control                             */
    opus_int16                  xq*,                           /* O    Decoded speech                              */
          int              pulses* //[ ( ( 5 * 4 ) * 16 ) ]      /* I    Pulse signal                                */
)
{
    int   i, k, lag = 0, start_idx, sLTP_buf_idx, NLSF_interpolation_flag, signalType;
    opus_int16 A_Q12*, B_Q14*, pxq*, A_Q12_tmp[ 16 ];
    opus_int16 sLTP[ ( ( 5 * 4 ) * 16 ) ];
    opus_int32 sLTP_Q15[ 2 * ( ( 5 * 4 ) * 16 ) ];
    opus_int32 LTP_pred_Q13, LPC_pred_Q10, Gain_Q10, inv_gain_Q31, gain_adj_Q16, rand_seed, offset_Q10;
    opus_int32 * pred_lag_ptr, pexc_Q14, pres_Q14;
    opus_int32 res_Q14[ ( 5 * 16 ) ];
    opus_int32 sLPC_Q14[ ( 5 * 16 ) + 16 ];

    clear sLTP, sLTP_Q15;

    offset_Q10 = silk_Quantization_Offsets_Q10[ psDec->indices.signalType >> 1 ][ psDec->indices.quantOffsetType ];

    if( psDec->indices.NLSFInterpCoef_Q2 < 1 << 2 ) {
        NLSF_interpolation_flag = 1;
    } else {
        NLSF_interpolation_flag = 0;
    }

    /* Decode excitation */
    rand_seed = psDec->indices.Seed;
    for( i = 0; i < psDec->frame_length; i++ ) {
        rand_seed = (((opus_int32)((opus_uint32)((907633515)) + (opus_uint32)((opus_uint32)((rand_seed)) * (opus_uint32)(196314165)))));
        psDec->exc_Q14[ i ] = ((opus_int32)((opus_uint32)((opus_int32)pulses[ i ])<<(14)));
        if( psDec->exc_Q14[ i ] > 0 ) {
            psDec->exc_Q14[ i ] -= 80 << 4;
        } else
        if( psDec->exc_Q14[ i ] < 0 ) {
            psDec->exc_Q14[ i ] += 80 << 4;
        }
        psDec->exc_Q14[ i ] += offset_Q10 << 4;
        if( rand_seed < 0 ) {
           psDec->exc_Q14[ i ] = -psDec->exc_Q14[ i ];
        }

        rand_seed = ((opus_int32)((opus_uint32)(rand_seed) + (opus_uint32)(pulses[ i ])));
    }

    /* Copy LPC state */
    memcpy((byte*)(&sLPC_Q14), (byte*)&(psDec->sLPC_Q14_buf), (16 * ((int)(opus_int32 ' size  ))));

    pexc_Q14 = &psDec->exc_Q14;
    pxq      = xq;
    sLTP_buf_idx = psDec->ltp_mem_length;
    /* Loop over subframes */
    for( k = 0; k < psDec->nb_subfr; k++ ) {
        pres_Q14 = &res_Q14;
        A_Q12 = &psDecCtrl->PredCoef_Q12[ k >> 1 ];

        /* Preload LPC coeficients to array on stack. Gives small performance gain */
        memcpy((byte*)&(A_Q12_tmp), (byte*)(A_Q12), (psDec->LPC_order * ((int)(opus_int16 ' size  ))));
        B_Q14        = &psDecCtrl->LTPCoef_Q14[ k * 5 ];
        signalType   = psDec->indices.signalType;

        Gain_Q10     = ((psDecCtrl->Gains_Q16[ k ])>>(6));
        inv_gain_Q31 = silk_INVERSE32_varQ( psDecCtrl->Gains_Q16[ k ], 47 );

        /* Calculate gain adjustment factor */
        if( psDecCtrl->Gains_Q16[ k ] != psDec->prev_gain_Q16 ) {
            gain_adj_Q16 =  silk_DIV32_varQ( psDec->prev_gain_Q16, psDecCtrl->Gains_Q16[ k ], 16 );

            /* Scale short term state */
            for( i = 0; i < 16; i++ ) {
                sLPC_Q14[ i ] = ((((((((gain_adj_Q16)) >> 16) * (opus_int32)((opus_int16)((sLPC_Q14[ i ])))) + (((((gain_adj_Q16)) & 0x0000FFFF) * (opus_int32)((opus_int16)((sLPC_Q14[ i ])))) >> 16)))) + ((((gain_adj_Q16)) * (((16) == 1 ? (((sLPC_Q14[ i ])) >> 1) + (((sLPC_Q14[ i ])) & 1) : ((((sLPC_Q14[ i ])) >> ((16) - 1)) + 1) >> 1)))));
            }
        } else {
            gain_adj_Q16 = (opus_int32)1 << 16;
        }

        /* Save inv_gain */
        ;
        psDec->prev_gain_Q16 = psDecCtrl->Gains_Q16[ k ];

        /* Avoid abrupt transition from voiced PLC to unvoiced normal decoding */
        if( psDec->lossCnt!=0 && psDec->prevSignalType == 2 &&
            psDec->indices.signalType != 2 && k < 4/2 ) {

            memset((char*)(B_Q14), (0), (5 * ((int)(opus_int16 ' size  ))));
            B_Q14[ 5/2 ] = (opus_int16)(((opus_int32)((0.25) * (float)((int8)1 << (14)) + 0.5)));

            signalType = 2;
            psDecCtrl->pitchL[ k ] = psDec->lagPrev;
        }

        if( signalType == 2 ) {
            /* Voiced */
            lag = psDecCtrl->pitchL[ k ];

            /* Re-whitening */
            if( k == 0 || ( k == 2 && NLSF_interpolation_flag!=0 ) ) {
                /* Rewhiten with new A coefs */
                start_idx = psDec->ltp_mem_length - lag - psDec->LPC_order - 5 / 2;
                ;

                if( k == 2 ) {
                    memcpy((byte*)(&psDec->outBuf[ psDec->ltp_mem_length ]), (byte*)(xq), (2 * psDec->subfr_length * ((int)(opus_int16 ' size  ))));
                }

                silk_LPC_analysis_filter( &sLTP[ start_idx ], &psDec->outBuf[ start_idx + k * psDec->subfr_length ],
                    A_Q12, psDec->ltp_mem_length - start_idx, psDec->LPC_order );

                /* After rewhitening the LTP state is unscaled */
                if( k == 0 ) {
                    /* Do LTP downscaling to reduce inter-packet dependency */
                    inv_gain_Q31 = ((opus_int32)((opus_uint32)(((((inv_gain_Q31) >> 16) * (opus_int32)((opus_int16)(psDecCtrl->LTP_scale_Q14))) + ((((inv_gain_Q31) & 0x0000FFFF) * (opus_int32)((opus_int16)(psDecCtrl->LTP_scale_Q14))) >> 16)))<<(2)));
                }
                for( i = 0; i < lag + 5/2; i++ ) {
                    sLTP_Q15[ sLTP_buf_idx - i - 1 ] = ((((inv_gain_Q31) >> 16) * (opus_int32)((opus_int16)(sLTP[ psDec->ltp_mem_length - i - 1 ]))) + ((((inv_gain_Q31) & 0x0000FFFF) * (opus_int32)((opus_int16)(sLTP[ psDec->ltp_mem_length - i - 1 ]))) >> 16));
                }
            } else {
                /* Update LTP state when Gain changes */
                if( gain_adj_Q16 != (opus_int32)1 << 16 ) {
                    for( i = 0; i < lag + 5/2; i++ ) {
                        sLTP_Q15[ sLTP_buf_idx - i - 1 ] = ((((((((gain_adj_Q16)) >> 16) * (opus_int32)((opus_int16)((sLTP_Q15[ sLTP_buf_idx - i - 1 ])))) + (((((gain_adj_Q16)) & 0x0000FFFF) * (opus_int32)((opus_int16)((sLTP_Q15[ sLTP_buf_idx - i - 1 ])))) >> 16)))) + ((((gain_adj_Q16)) * (((16) == 1 ? (((sLTP_Q15[ sLTP_buf_idx - i - 1 ])) >> 1) + (((sLTP_Q15[ sLTP_buf_idx - i - 1 ])) & 1) : ((((sLTP_Q15[ sLTP_buf_idx - i - 1 ])) >> ((16) - 1)) + 1) >> 1)))));
                    }
                }
            }
        }

        /* Long-term prediction */
        if( signalType == 2 ) {
            /* Set up pointer */
            pred_lag_ptr = &sLTP_Q15[ sLTP_buf_idx - lag + 5 / 2 ];
            for( i = 0; i < psDec->subfr_length; i++ ) {
                /* Unrolled loop */
                /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
                LTP_pred_Q13 = 2;
                LTP_pred_Q13 = ((LTP_pred_Q13) + ((((pred_lag_ptr[ 0 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 0 ]))) + ((((pred_lag_ptr[ 0 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 0 ]))) >> 16)));
                LTP_pred_Q13 = ((LTP_pred_Q13) + ((((pred_lag_ptr[ -1 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 1 ]))) + ((((pred_lag_ptr[ -1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 1 ]))) >> 16)));
                LTP_pred_Q13 = ((LTP_pred_Q13) + ((((pred_lag_ptr[ -2 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 2 ]))) + ((((pred_lag_ptr[ -2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 2 ]))) >> 16)));
                LTP_pred_Q13 = ((LTP_pred_Q13) + ((((pred_lag_ptr[ -3 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 3 ]))) + ((((pred_lag_ptr[ -3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 3 ]))) >> 16)));
                LTP_pred_Q13 = ((LTP_pred_Q13) + ((((pred_lag_ptr[ -4 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 4 ]))) + ((((pred_lag_ptr[ -4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 4 ]))) >> 16)));
                pred_lag_ptr++;

                /* Generate LPC excitation */
                pres_Q14[ i ] = (((pexc_Q14[ i ])) + (((opus_int32)((opus_uint32)((LTP_pred_Q13))<<((1))))));

                /* Update states */
                sLTP_Q15[ sLTP_buf_idx ] = ((opus_int32)((opus_uint32)(pres_Q14[ i ])<<(1)));
                sLTP_buf_idx++;
            }
        } else {
            pres_Q14 = pexc_Q14;
        }

        for( i = 0; i < psDec->subfr_length; i++ ) {
            /* Short-term prediction */
            ;
            /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
            LPC_pred_Q10 = ((psDec->LPC_order)>>(1));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 1 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 0 ]))) + ((((sLPC_Q14[ 16 + i - 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 0 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 2 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 1 ]))) + ((((sLPC_Q14[ 16 + i - 2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 1 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 3 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 2 ]))) + ((((sLPC_Q14[ 16 + i - 3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 2 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 4 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 3 ]))) + ((((sLPC_Q14[ 16 + i - 4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 3 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 5 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 4 ]))) + ((((sLPC_Q14[ 16 + i - 5 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 4 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 6 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 5 ]))) + ((((sLPC_Q14[ 16 + i - 6 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 5 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 7 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 6 ]))) + ((((sLPC_Q14[ 16 + i - 7 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 6 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 8 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 7 ]))) + ((((sLPC_Q14[ 16 + i - 8 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 7 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 9 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 8 ]))) + ((((sLPC_Q14[ 16 + i - 9 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 8 ]))) >> 16)));
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 10 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 9 ]))) + ((((sLPC_Q14[ 16 + i - 10 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 9 ]))) >> 16)));
            if( psDec->LPC_order == 16 ) {
                LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 11 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 10 ]))) + ((((sLPC_Q14[ 16 + i - 11 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 10 ]))) >> 16)));
                LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 12 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 11 ]))) + ((((sLPC_Q14[ 16 + i - 12 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 11 ]))) >> 16)));
                LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 13 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 12 ]))) + ((((sLPC_Q14[ 16 + i - 13 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 12 ]))) >> 16)));
                LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 14 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 13 ]))) + ((((sLPC_Q14[ 16 + i - 14 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 13 ]))) >> 16)));
                LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 15 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 14 ]))) + ((((sLPC_Q14[ 16 + i - 15 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 14 ]))) >> 16)));
                LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14[ 16 + i - 16 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12_tmp[ 15 ]))) + ((((sLPC_Q14[ 16 + i - 16 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12_tmp[ 15 ]))) >> 16)));
            }

            /* Add prediction to LPC excitation */
            sLPC_Q14[ 16 + i ] = (((pres_Q14[ i ])) + (((opus_int32)((opus_uint32)((LPC_pred_Q10))<<((4))))));

            /* Scale with gain */
            pxq[ i ] = (opus_int16)((((8) == 1 ? ((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((Gain_Q10)))) + (((((sLPC_Q14[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q10)))) >> 16)))) + ((((sLPC_Q14[ 16 + i ])) * (((16) == 1 ? (((Gain_Q10)) >> 1) + (((Gain_Q10)) & 1) : ((((Gain_Q10)) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1))));
        }

        /* DEBUG_STORE_DATA( dec.pcm, pxq, psDec->subfr_length * sizeof( opus_int16 ) ) */

        /* Update LPC filter state */
        memcpy((byte*)&(sLPC_Q14), (byte*)(&sLPC_Q14[ psDec->subfr_length ]), (16 * ((int)(opus_int32 ' size  ))));
        pexc_Q14 += psDec->subfr_length;
        pxq      += psDec->subfr_length;
    }

    /* Save LPC state */
    memcpy((byte*)(&psDec->sLPC_Q14_buf), (byte*)(&sLPC_Q14), (16 * ((int)(opus_int32 ' size  ))));
}
#end unsafe
