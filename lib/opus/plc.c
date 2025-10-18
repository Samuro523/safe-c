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

use opus_types, structs, os_support, sum_sqr_shift, sigproc_fix, bwexpander;
use lpc_inv_pred_gain, lpc_analysis_filter, inlines, macros;

const        opus_int16 HARM_ATT_Q15[2]              = { 32440, 31130 }; /* 0.99, 0.95 */
const        opus_int16 PLC_RAND_ATTENUATE_V_Q15[2]  = { 31130, 26214 }; /* 0.95, 0.8 */
const        opus_int16 PLC_RAND_ATTENUATE_UV_Q15[2] = { 32440, 29491 }; /* 0.99, 0.9 */

void silk_PLC_update(
    silk_decoder_state                  *psDec,             /* I/O Decoder state        */
    silk_decoder_control                *psDecCtrl          /* I/O Decoder control      */
);

void silk_PLC_conceal(
    silk_decoder_state                  *psDec,             /* I/O Decoder state        */
    silk_decoder_control                *psDecCtrl,         /* I/O Decoder control      */
    opus_int16                          frame*             /* O LPC residual signal    */
);

public void silk_PLC_Reset(
    silk_decoder_state                  *psDec              /* I/O Decoder state        */
)
{
    psDec->sPLC.pitchL_Q8 = ((opus_int32)((opus_uint32)(psDec->frame_length)<<(8 - 1)));
    psDec->sPLC.prevGain_Q16[ 0 ] = ((opus_int32)((float)((1) * ((int8)1 << (16))) + 0.5));
    psDec->sPLC.prevGain_Q16[ 1 ] = ((opus_int32)((float)((1) * ((int8)1 << (16))) + 0.5));
    psDec->sPLC.subfr_length = 20;
    psDec->sPLC.nb_subfr = 2;
}

public void silk_PLC(
    silk_decoder_state                  *psDec,             /* I/O Decoder state        */
    silk_decoder_control                *psDecCtrl,         /* I/O Decoder control      */
    opus_int16                          frame*,            /* I/O  signal              */
    int                            lost                /* I Loss flag              */
)
{
    /* PLC control function */
    if( psDec->fs_kHz != psDec->sPLC.fs_kHz ) {
        silk_PLC_Reset( psDec );
        psDec->sPLC.fs_kHz = psDec->fs_kHz;
    }

    if( lost!=0 ) {
        /****************************/
        /* Generate Signal          */
        /****************************/
        silk_PLC_conceal( psDec, psDecCtrl, frame );

        psDec->lossCnt++;
    } else {
        /****************************/
        /* Update state             */
        /****************************/
        silk_PLC_update( psDec, psDecCtrl );
    }
}

/**************************************************/
/* Update state of PLC                            */
/**************************************************/
void silk_PLC_update(
    silk_decoder_state                  *psDec,             /* I/O Decoder state        */
    silk_decoder_control                *psDecCtrl          /* I/O Decoder control      */
)
{
    opus_int32 LTP_Gain_Q14, temp_LTP_Gain_Q14;
    int   i, j;
    silk_PLC_struct *psPLC;

    psPLC = &psDec->sPLC;

    /* Update parameters used in case of packet loss */
    psDec->prevSignalType = psDec->indices.signalType;
    LTP_Gain_Q14 = 0;
    if( psDec->indices.signalType == 2 ) {
        /* Find the parameters for the last subframe which contains a pitch pulse */
        for( j = 0; j * psDec->subfr_length < psDecCtrl->pitchL[ psDec->nb_subfr - 1 ]; j++ ) {
            if( j == psDec->nb_subfr ) {
                break;
            }
            temp_LTP_Gain_Q14 = 0;
            for( i = 0; i < 5; i++ ) {
                temp_LTP_Gain_Q14 += psDecCtrl->LTPCoef_Q14[ ( psDec->nb_subfr - 1 - j ) * 5  + i ];
            }
            if( temp_LTP_Gain_Q14 > LTP_Gain_Q14 ) {
                LTP_Gain_Q14 = temp_LTP_Gain_Q14;
                

memcpy((byte*)&(psPLC->LTPCoef_Q14), (byte*)(&psDecCtrl->LTPCoef_Q14[ ((opus_int32)((opus_int16)(psDec->nb_subfr - 1 - j)) * (opus_int32)((opus_int16)(5))) ]), (5 * ((int)(opus_int16 ' size  ))));

                psPLC->pitchL_Q8 = ((opus_int32)((opus_uint32)(psDecCtrl->pitchL[ psDec->nb_subfr - 1 - j ])<<(8)));
            }
        }

        memset((char*)(&psPLC->LTPCoef_Q14), (0), (5 * ((int)(opus_int16 ' size  ))));
        psPLC->LTPCoef_Q14[ 5 / 2 ] = (opus_int16)LTP_Gain_Q14;

        /* Limit LT coefs */
        if( LTP_Gain_Q14 < 11469 ) {
            int   scale_Q10;
            opus_int32 tmp;

            tmp = ((opus_int32)((opus_uint32)(11469)<<(10)));
            scale_Q10 = ((opus_int32)((tmp) / ((((LTP_Gain_Q14) > (1)) ? (LTP_Gain_Q14) : (1)))));
            for( i = 0; i < 5; i++ ) {
                psPLC->LTPCoef_Q14[ i ] = (opus_int16)(((((opus_int32)((opus_int16)(psPLC->LTPCoef_Q14[ i ])) * (opus_int32)((opus_int16)(scale_Q10))))>>(10)));
            }
        } else if( LTP_Gain_Q14 > 15565 ) {
            int   scale_Q14;
            opus_int32 tmp;

            tmp = ((opus_int32)((opus_uint32)(15565)<<(14)));
            scale_Q14 = ((opus_int32)((tmp) / ((((LTP_Gain_Q14) > (1)) ? (LTP_Gain_Q14) : (1)))));
            for( i = 0; i < 5; i++ ) {
                psPLC->LTPCoef_Q14[ i ] = (opus_int16)(((((opus_int32)((opus_int16)(psPLC->LTPCoef_Q14[ i ])) * (opus_int32)((opus_int16)(scale_Q14))))>>(14)));
            }
        }
    } else {
        psPLC->pitchL_Q8 = ((opus_int32)((opus_uint32)(((opus_int32)((opus_int16)(psDec->fs_kHz)) * (opus_int32)((opus_int16)(18))))<<(8)));
        memset((char*)&(psPLC->LTPCoef_Q14), (0), (5 * ((int)(opus_int16 ' size  ))));
    }

    /* Save LPC coeficients */
    memcpy((byte*)&(psPLC->prevLPC_Q12), (byte*)&(psDecCtrl->PredCoef_Q12[ 1 ]), (psDec->LPC_order * ((int)(opus_int16 ' size  ))));
    psPLC->prevLTP_scale_Q14 = (opus_int16)psDecCtrl->LTP_scale_Q14;

    /* Save last two gains */
    memcpy((byte*)&(psPLC->prevGain_Q16), (byte*)(&psDecCtrl->Gains_Q16[ psDec->nb_subfr - 2 ]), (2 * ((int)(opus_int32 ' size  ))));

    psPLC->subfr_length = psDec->subfr_length;
    psPLC->nb_subfr = psDec->nb_subfr;
}

void silk_PLC_conceal(
    silk_decoder_state                  *psDec,             /* I/O Decoder state        */
    silk_decoder_control                *psDecCtrl,         /* I/O Decoder control      */
    opus_int16                          frame*             /* O LPC residual signal    */
)
{
    int   i, j, k;
    int   lag, idx, sLTP_buf_idx, shift1, shift2;
    opus_int32 rand_seed, harm_Gain_Q15, rand_Gain_Q15, inv_gain_Q30;
    opus_int32 energy1, energy2, rand_ptr*, pred_lag_ptr*;
    opus_int32 LPC_pred_Q10, LTP_pred_Q12;
    opus_int16 rand_scale_Q14;
    opus_int16 * B_Q14, exc_buf_ptr;
    opus_int32 * sLPC_Q14_ptr;
    opus_int16 exc_buf[ 2 * ( 5 * 16 ) ];
    opus_int16 A_Q12[ 16 ];
    opus_int16 sLTP[ ( ( 5 * 4 ) * 16 ) ];
    opus_int32 sLTP_Q14[ 2 * ( ( 5 * 4 ) * 16 ) ];
    silk_PLC_struct *psPLC = &psDec->sPLC;
    opus_int32 prevGain_Q10[2];

	clear prevGain_Q10, sLTP, sLTP_Q14;
	
    prevGain_Q10[0] = ((psPLC->prevGain_Q16[ 0 ])>>(6));
    prevGain_Q10[1] = ((psPLC->prevGain_Q16[ 1 ])>>(6));

    if( psDec->first_frame_after_reset != 0 ) {
       memset((char*)&(psPLC->prevLPC_Q12), (0), (((int)((psPLC->prevLPC_Q12) ' size  ))));
    }

    /* Find random noise component */
    /* Scale previous excitation signal */
    exc_buf_ptr = &exc_buf;
    for( k = 0; k < 2; k++ ) {
        for( i = 0; i < psPLC->subfr_length; i++ ) {
            exc_buf_ptr[ i ] = (opus_int16)
((((((((((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ k ])))) + (((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ k ])))) >> 16)))) + ((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) * (((16) == 1 ? (((prevGain_Q10[ k ])) >> 1) + (((prevGain_Q10[ k ])) & 1) : ((((prevGain_Q10[ k ])) >> ((16) - 1)) + 1) >> 1))))))>>(8))) > 0x7FFF ? 0x7FFF : ((((((((((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ k ])))) + (((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ k ])))) >> 16)))) + ((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) * (((16) == 1 ? (((prevGain_Q10[ k ])) >> 1) + (((prevGain_Q10[ k ])) & 1) : ((((prevGain_Q10[ k ])) >> ((16) - 1)) + 1) >> 1))))))>>(8))) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((((((((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ k ])))) + (((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ k ])))) >> 16)))) + ((((psDec->exc_Q14[ i + ( k + psPLC->nb_subfr - 2 ) * psPLC->subfr_length ])) * (((16) == 1 ? (((prevGain_Q10[ k ])) >> 1) + (((prevGain_Q10[ k ])) & 1) : ((((prevGain_Q10[ k ])) >> ((16) - 1)) + 1) >> 1))))))>>(8)))));
        }
        exc_buf_ptr += psPLC->subfr_length;
    }
    /* Find the subframe with lowest energy of the last two and use that as random noise generator */
    silk_sum_sqr_shift( &energy1, &shift1, &exc_buf,                         psPLC->subfr_length );
    silk_sum_sqr_shift( &energy2, &shift2, &exc_buf[ psPLC->subfr_length ], psPLC->subfr_length );

    if( ((energy1)>>(shift2)) < ((energy2)>>(shift1)) ) {
        /* First sub-frame has lowest energy */
        rand_ptr = &psDec->exc_Q14[ silk_max_int( 0, ( psPLC->nb_subfr - 1 ) * psPLC->subfr_length - 128 ) ];
    } else {
        /* Second sub-frame has lowest energy */
        rand_ptr = &psDec->exc_Q14[ silk_max_int( 0, psPLC->nb_subfr * psPLC->subfr_length - 128 ) ];
    }

    /* Set up Gain to random noise component */
    B_Q14          = &psPLC->LTPCoef_Q14;
    rand_scale_Q14 = psPLC->randScale_Q14;

    /* Set up attenuation gains */
    harm_Gain_Q15 = HARM_ATT_Q15[ silk_min_int( 2 - 1, psDec->lossCnt ) ];
    if( psDec->prevSignalType == 2 ) {
        rand_Gain_Q15 = PLC_RAND_ATTENUATE_V_Q15[  silk_min_int( 2 - 1, psDec->lossCnt ) ];
    } else {
        rand_Gain_Q15 = PLC_RAND_ATTENUATE_UV_Q15[ silk_min_int( 2 - 1, psDec->lossCnt ) ];
    }

    /* LPC concealment. Apply BWE to previous LPC */
    silk_bwexpander( &psPLC->prevLPC_Q12, psDec->LPC_order, ((opus_int32)((0.99) * (float)((int8)1 << (16)) + 0.5)) );

    /* Preload LPC coeficients to array on stack. Gives small performance gain */
    memcpy((byte*)&(A_Q12), (byte*)&(psPLC->prevLPC_Q12), (psDec->LPC_order * ((int)(opus_int16 ' size  ))));

    /* First Lost frame */
    if( psDec->lossCnt == 0 ) {
        rand_scale_Q14 = 1 << 14;

        /* Reduce random noise Gain for voiced frames */
        if( psDec->prevSignalType == 2 ) {
            for( i = 0; i < 5; i++ ) {
                rand_scale_Q14 -= B_Q14[ i ];
            }
            rand_scale_Q14 = silk_max_16( 3277, rand_scale_Q14 ); /* 0.2 */
            rand_scale_Q14 = (opus_int16)((((opus_int32)((opus_int16)(rand_scale_Q14)) * (opus_int32)((opus_int16)(psPLC->prevLTP_scale_Q14))))>>(14));
        } else {
            /* Reduce random noise for unvoiced frames with high LPC gain */
            opus_int32 invGain_Q30, down_scale_Q30;

            invGain_Q30 = silk_LPC_inverse_pred_gain( &psPLC->prevLPC_Q12, psDec->LPC_order );

            down_scale_Q30 = silk_min_32( (((opus_int32)1 << 30)>>(3)), invGain_Q30 );
            down_scale_Q30 = silk_max_32( (((opus_int32)1 << 30)>>(8)), down_scale_Q30 );
            down_scale_Q30 = ((opus_int32)((opus_uint32)(down_scale_Q30)<<(3)));

            rand_Gain_Q15 = ((((((down_scale_Q30) >> 16) * (opus_int32)((opus_int16)(rand_Gain_Q15))) + ((((down_scale_Q30) & 0x0000FFFF) * (opus_int32)((opus_int16)(rand_Gain_Q15))) >> 16)))>>(14));
        }
    }

    rand_seed    = psPLC->rand_seed;
    lag          = ((8) == 1 ? ((psPLC->pitchL_Q8) >> 1) + ((psPLC->pitchL_Q8) & 1) : (((psPLC->pitchL_Q8) >> ((8) - 1)) + 1) >> 1);
    sLTP_buf_idx = psDec->ltp_mem_length;

    /* Rewhiten LTP state */
    idx = psDec->ltp_mem_length - lag - psDec->LPC_order - 5 / 2;
    ;
    silk_LPC_analysis_filter( &sLTP[ idx ], &psDec->outBuf[ idx ], &A_Q12, psDec->ltp_mem_length - idx, psDec->LPC_order );
    /* Scale LTP state */
    inv_gain_Q30 = silk_INVERSE32_varQ( psPLC->prevGain_Q16[ 1 ], 46 );
    inv_gain_Q30 = (((inv_gain_Q30) < (0x7FFFFFFF >> 1)) ? (inv_gain_Q30) : (0x7FFFFFFF >> 1));
    for( i = idx + psDec->LPC_order; i < psDec->ltp_mem_length; i++ ) {
        sLTP_Q14[ i ] = ((((inv_gain_Q30) >> 16) * (opus_int32)((opus_int16)(sLTP[ i ]))) + ((((inv_gain_Q30) & 0x0000FFFF) * (opus_int32)((opus_int16)(sLTP[ i ]))) >> 16));
    }

    /***************************/
    /* LTP synthesis filtering */
    /***************************/
    for( k = 0; k < psDec->nb_subfr; k++ ) {
        /* Set up pointer */
        pred_lag_ptr = &sLTP_Q14[ sLTP_buf_idx - lag + 5 / 2 ];
        for( i = 0; i < psDec->subfr_length; i++ ) {
            /* Unrolled loop */
            /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
            LTP_pred_Q12 = 2;
            LTP_pred_Q12 = ((LTP_pred_Q12) + ((((pred_lag_ptr[ 0 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 0 ]))) + ((((pred_lag_ptr[ 0 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 0 ]))) >> 16)));
            LTP_pred_Q12 = ((LTP_pred_Q12) + ((((pred_lag_ptr[ -1 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 1 ]))) + ((((pred_lag_ptr[ -1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 1 ]))) >> 16)));
            LTP_pred_Q12 = ((LTP_pred_Q12) + ((((pred_lag_ptr[ -2 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 2 ]))) + ((((pred_lag_ptr[ -2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 2 ]))) >> 16)));
            LTP_pred_Q12 = ((LTP_pred_Q12) + ((((pred_lag_ptr[ -3 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 3 ]))) + ((((pred_lag_ptr[ -3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 3 ]))) >> 16)));
            LTP_pred_Q12 = ((LTP_pred_Q12) + ((((pred_lag_ptr[ -4 ]) >> 16) * (opus_int32)((opus_int16)(B_Q14[ 4 ]))) + ((((pred_lag_ptr[ -4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(B_Q14[ 4 ]))) >> 16)));
            pred_lag_ptr++;

            /* Generate LPC excitation */
            rand_seed = (((opus_int32)((opus_uint32)((907633515)) + (opus_uint32)((opus_uint32)((rand_seed)) * (opus_uint32)(196314165)))));
            idx = ((rand_seed)>>(25)) & ( 128 - 1 );
            sLTP_Q14[ sLTP_buf_idx ] = ((opus_int32)((opus_uint32)(((LTP_pred_Q12) + ((((rand_ptr[ idx ]) >> 16) * (opus_int32)((opus_int16)(rand_scale_Q14))) + ((((rand_ptr[ idx ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(rand_scale_Q14))) >> 16))))<<(2)));
            sLTP_buf_idx++;
        }

        /* Gradually reduce LTP gain */
        for( j = 0; j < 5; j++ ) {
            B_Q14[ j ] = (opus_int16)(((((opus_int32)((opus_int16)(harm_Gain_Q15)) * (opus_int32)((opus_int16)(B_Q14[ j ]))))>>(15)));
        }
        /* Gradually reduce excitation gain */
        rand_scale_Q14 = (opus_int16)(((((opus_int32)((opus_int16)(rand_scale_Q14)) * (opus_int32)((opus_int16)(rand_Gain_Q15))))>>(15)));

        /* Slowly increase pitch lag */
        psPLC->pitchL_Q8 = ((psPLC->pitchL_Q8) + ((((psPLC->pitchL_Q8) >> 16) * (opus_int32)((opus_int16)(655))) + ((((psPLC->pitchL_Q8) & 0x0000FFFF) * (opus_int32)((opus_int16)(655))) >> 16)));
        psPLC->pitchL_Q8 = silk_min_32( psPLC->pitchL_Q8, ((opus_int32)((opus_uint32)(((opus_int32)((opus_int16)(18)) * (opus_int32)((opus_int16)(psDec->fs_kHz))))<<(8))) );
        lag = ((8) == 1 ? ((psPLC->pitchL_Q8) >> 1) + ((psPLC->pitchL_Q8) & 1) : (((psPLC->pitchL_Q8) >> ((8) - 1)) + 1) >> 1);
    }

    /***************************/
    /* LPC synthesis filtering */
    /***************************/
    sLPC_Q14_ptr = &sLTP_Q14[ psDec->ltp_mem_length - 16 ];

    /* Copy LPC state */
    memcpy((byte*)(sLPC_Q14_ptr), (byte*)&(psDec->sLPC_Q14_buf), (16 * ((int)(opus_int32 ' size  ))));

    ; /* check that unrolling works */
    for( i = 0; i < psDec->frame_length; i++ ) {
        /* partly unrolled */
        /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
        LPC_pred_Q10 = ((psDec->LPC_order)>>(1));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 1 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 0 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 0 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 2 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 1 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 1 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 3 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 2 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 2 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 4 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 3 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 3 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 5 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 4 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 5 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 4 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 6 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 5 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 6 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 5 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 7 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 6 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 7 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 6 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 8 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 7 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 8 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 7 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 9 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 8 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 9 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 8 ]))) >> 16)));
        LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - 10 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 9 ]))) + ((((sLPC_Q14_ptr[ 16 + i - 10 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 9 ]))) >> 16)));
        for( j = 10; j < psDec->LPC_order; j++ ) {
            LPC_pred_Q10 = ((LPC_pred_Q10) + ((((sLPC_Q14_ptr[ 16 + i - j - 1 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ j ]))) + ((((sLPC_Q14_ptr[ 16 + i - j - 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ j ]))) >> 16)));
        }

        /* Add prediction to LPC excitation */
        sLPC_Q14_ptr[ 16 + i ] = (((sLPC_Q14_ptr[ 16 + i ])) + (((opus_int32)((opus_uint32)((LPC_pred_Q10))<<((4))))));

        /* Scale with Gain */
        frame[ i ] = (opus_int16)((((((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1))))) > 0x7FFF ? 0x7FFF : ((((((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) *
		(opus_int32)
((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1))))) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((
prevGain_Q10[ 1 ]))
 >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> 1) + ((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) & 1) : (((((((((((sLPC_Q14_ptr[ 16 + i ])) >> 16) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) + (((((sLPC_Q14_ptr[ 16 + i ])) & 0x0000FFFF) * (opus_int32)((opus_int16)((prevGain_Q10[ 1 ])))) >> 16)))) + ((((sLPC_Q14_ptr[ 16 + i ])) * (((16) == 1 ? (((prevGain_Q10[ 1 ])) >> 1) + (((prevGain_Q10[ 1 ])) & 1) : ((((prevGain_Q10[ 1 ])) >> ((16) - 1)) + 1) >> 1)))))) >> ((8) - 1)) + 1) >> 1)))))));
    }

    /* Save LPC state */
    memcpy((byte*)&(psDec->sLPC_Q14_buf), (byte*)(&sLPC_Q14_ptr[ psDec->frame_length ]), (16 * ((int)(opus_int32 ' size  ))));

    /**************************************/
    /* Update states                      */
    /**************************************/
    psPLC->rand_seed     = rand_seed;
    psPLC->randScale_Q14 = rand_scale_Q14;
    for( i = 0; i < 4; i++ ) {
        psDecCtrl->pitchL[ i ] = lag;
    }
}

/* Glues concealed frames with new good recieved frames */
public void silk_PLC_glue_frames(
    silk_decoder_state                  *psDec,             /* I/O decoder state        */
    opus_int16                          frame*,            /* I/O signal               */
    int                            length              /* I length of signal       */
)
{
    int   i, energy_shift;
    opus_int32 energy;
    silk_PLC_struct *psPLC;
    psPLC = &psDec->sPLC;

    if( psDec->lossCnt != 0 ) {
        /* Calculate energy in concealed residual */
        silk_sum_sqr_shift( &psPLC->conc_energy, &psPLC->conc_energy_shift, frame, length );

        psPLC->last_frame_lost = 1;
    } else {
        if( psDec->sPLC.last_frame_lost != 0 ) {
            /* Calculate residual in decoded signal if last frame was lost */
            silk_sum_sqr_shift( &energy, &energy_shift, frame, length );

            /* Normalize energies */
            if( energy_shift > psPLC->conc_energy_shift ) {
                psPLC->conc_energy = ((psPLC->conc_energy)>>(energy_shift - psPLC->conc_energy_shift));
            } else if( energy_shift < psPLC->conc_energy_shift ) {
                energy = ((energy)>>(psPLC->conc_energy_shift - energy_shift));
            }

            /* Fade in the energy difference */
            if( energy > psPLC->conc_energy ) {
                opus_int32 frac_Q24, LZ;
                opus_int32 gain_Q16, slope_Q16;

                LZ = silk_CLZ32( psPLC->conc_energy );
                LZ = LZ - 1;
                psPLC->conc_energy = ((opus_int32)((opus_uint32)(psPLC->conc_energy)<<(uint)(LZ)));
                energy = ((energy)>>(silk_max_32( 24 - LZ, 0 )));

                frac_Q24 = ((opus_int32)((psPLC->conc_energy) / ((((energy) > (1)) ? (energy) : (1)))));

                gain_Q16 = ((opus_int32)((opus_uint32)(silk_SQRT_APPROX( frac_Q24 ))<<(4)));
                slope_Q16 = ((opus_int32)((( (opus_int32)1 << 16 ) - gain_Q16) / (length)));
                /* Make slope 4x steeper to avoid missing onsets after DTX */
                slope_Q16 = ((opus_int32)((opus_uint32)(slope_Q16)<<(2)));

                for( i = 0; i < length; i++ ) {
                    frame[ i ] = (opus_int16)(((((gain_Q16) >> 16) * (opus_int32)((opus_int16)(frame[ i ]))) + ((((gain_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(frame[ i ]))) >> 16)));
                    gain_Q16 += slope_Q16;
                    if( gain_Q16 > (opus_int32)1 << 16 ) {
                        break;
                    }
                }
            }
        }
        psPLC->last_frame_lost = 0;
    }
}
#end unsafe
