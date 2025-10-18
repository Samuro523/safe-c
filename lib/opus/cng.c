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

use opus_types, structs, os_support, nlsf2a;

/* Generates excitation for CNG LPC synthesis */
public
void silk_CNG_exc(
    opus_int32                       residual_Q10*,     /* O    CNG residual signal Q10                     */
    opus_int32                       exc_buf_Q14*,      /* I    Random samples buffer Q10                   */
    opus_int32                       Gain_Q16,           /* I    Gain to apply                               */
    int                         length,             /* I    Length                                      */
    opus_int32                       *rand_seed          /* I/O  Seed to random index generator              */
)
{
    opus_int32 seed;
    int   i, idx, exc_mask;

    exc_mask = 255;
    while( exc_mask > length ) {
        exc_mask = ((exc_mask)>>(1));
    }

    seed = *rand_seed;
    for( i = 0; i < length; i++ ) {
        seed = (((opus_int32)((opus_uint32)((907633515)) + (opus_uint32)((opus_uint32)((seed)) * (opus_uint32)(196314165)))));
        idx = (int)( ((seed)>>(24)) & exc_mask );
        ;
        ;
        residual_Q10[ i ] = (opus_int16)((((((((((exc_buf_Q14[ idx ])) >> 16)
 * (opus_int32)((opus_int16)((Gain_Q16 >> 4)))) + (((((exc_buf_Q14[ idx ]))
 & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q16 >> 4)))) >> 16))))
 + ((((exc_buf_Q14[ idx ])) * (((16) == 1 ? (((Gain_Q16 >> 4)) >> 1)
 + (((Gain_Q16 >> 4)) & 1) : ((((Gain_Q16 >> 4)) >> ((16) - 1))
 + 1) >> 1)))))) > 0x7FFF ? 0x7FFF : ((((((((((exc_buf_Q14[ idx ])) >> 16)
 * (opus_int32)((opus_int16)((Gain_Q16 >> 4)))) + (((((exc_buf_Q14[ idx ]))
 & 0x0000FFFF) * (opus_int32)((opus_int16)((Gain_Q16 >> 4)))) >> 16))))
 + ((((exc_buf_Q14[ idx ])) * (((16) == 1 ? (((Gain_Q16 >> 4)) >> 1)
 + (((Gain_Q16 >> 4)) & 1) : ((((Gain_Q16 >> 4)) >> ((16) - 1)) + 1) >> 1))))))
 < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((((((((exc_buf_Q14[ idx ])) >> 16)
 * (opus_int32)((opus_int16)((Gain_Q16 >> 4)))) + (((((exc_buf_Q14[ idx ])) & 0x0000FFFF)
 * (opus_int32)((opus_int16)((Gain_Q16 >> 4)))) >> 16)))) + ((((exc_buf_Q14[ idx ]))
 * (((16) == 1 ? (((Gain_Q16 >> 4)) >> 1) + (((Gain_Q16 >> 4)) & 1)
 : ((((Gain_Q16 >> 4)) >> ((16) - 1)) + 1) >> 1))))))));
    }
    *rand_seed = seed;
}

public
void silk_CNG_Reset(
    silk_decoder_state          *psDec                          /* I/O  Decoder state                               */
)
{
    int i, NLSF_step_Q15, NLSF_acc_Q15;

    NLSF_step_Q15 = ((opus_int32)((0x7FFF) / (psDec->LPC_order + 1)));
    NLSF_acc_Q15 = 0;
    for( i = 0; i < psDec->LPC_order; i++ ) {
        NLSF_acc_Q15 += NLSF_step_Q15;
        psDec->sCNG.CNG_smth_NLSF_Q15[ i ] = (opus_int16)NLSF_acc_Q15;
    }
    psDec->sCNG.CNG_smth_Gain_Q16 = 0;
    psDec->sCNG.rand_seed = 3176576;
}


/* Updates CNG estimate, and applies the CNG when packet was lost   */
public
void silk_CNG(
    silk_decoder_state          *psDec,                         /* I/O  Decoder state                               */
    silk_decoder_control        *psDecCtrl,                     /* I/O  Decoder control                             */
    opus_int16                  frame*,                        /* I/O  Signal                                      */
    int                    length                          /* I    Length of residual                          */
)
{
    int   i, subfr;
    opus_int32 sum_Q6, max_Gain_Q16;
    opus_int16 A_Q12[ 16 ];
    opus_int32 CNG_sig_Q10[ ( ( 5 * 4 ) * 16 ) + 16 ];
    silk_CNG_struct *psCNG = &psDec->sCNG;

    if( psDec->fs_kHz != psCNG->fs_kHz ) {
        /* Reset state */
        silk_CNG_Reset( psDec );

        psCNG->fs_kHz = psDec->fs_kHz;
    }
    if( psDec->lossCnt == 0 && psDec->prevSignalType == 0 ) {
        /* Update CNG parameters */

        /* Smoothing of LSF's  */
        for( i = 0; i < psDec->LPC_order; i++ ) {
            psCNG->CNG_smth_NLSF_Q15[ i ] += (opus_int16)((((((opus_int32)psDec->prevNLSF_Q15[ i ] - (opus_int32)psCNG->CNG_smth_NLSF_Q15[ i ]) >> 16) * (opus_int32)((opus_int16)(16348))) + (((((opus_int32)psDec->prevNLSF_Q15[ i ] - (opus_int32)psCNG->CNG_smth_NLSF_Q15[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(16348))) >> 16)));
        }
        /* Find the subframe with the highest gain */
        max_Gain_Q16 = 0;
        subfr        = 0;
        for( i = 0; i < psDec->nb_subfr; i++ ) {
            if( psDecCtrl->Gains_Q16[ i ] > max_Gain_Q16 ) {
                max_Gain_Q16 = psDecCtrl->Gains_Q16[ i ];
                subfr        = i;
            }
        }
        /* Update CNG excitation buffer with excitation from this subframe */
        memmove((byte*)(&psCNG->CNG_exc_buf_Q14[ psDec->subfr_length ]), (byte*)&(psCNG->CNG_exc_buf_Q14), (( psDec->nb_subfr - 1 ) * psDec->subfr_length * ((int)(opus_int32 ' size  ))));
        memcpy((byte*)(&psCNG->CNG_exc_buf_Q14), (byte*)(&psDec->exc_Q14[ subfr * psDec->subfr_length ]), (psDec->subfr_length * ((int)(opus_int32 ' size  ))));

        /* Smooth gains */
        for( i = 0; i < psDec->nb_subfr; i++ ) {
            psCNG->CNG_smth_Gain_Q16 += ((((psDecCtrl->Gains_Q16[ i ] - psCNG->CNG_smth_Gain_Q16) >> 16) * (opus_int32)((opus_int16)(4634))) + ((((psDecCtrl->Gains_Q16[ i ] - psCNG->CNG_smth_Gain_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(4634))) >> 16));
        }
    }

    /* Add CNG when packet is lost or during DTX */
    if( psDec->lossCnt != 0 ) {

        /* Generate CNG excitation */
        silk_CNG_exc( &CNG_sig_Q10 + 16, &psCNG->CNG_exc_buf_Q14, psCNG->CNG_smth_Gain_Q16, length, &psCNG->rand_seed );

        /* Convert CNG NLSF to filter representation */
        silk_NLSF2A( &A_Q12, &psCNG->CNG_smth_NLSF_Q15, psDec->LPC_order );

        /* Generate CNG signal, by synthesis filtering */
        memcpy((byte*)(&CNG_sig_Q10), (byte*)&(psCNG->CNG_synth_state), (16 * ((int)(opus_int32 ' size  ))));
        for( i = 0; i < length; i++ ) {
            ;
            /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
            sum_Q6 = ((psDec->LPC_order)>>(1));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 1 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 0 ]))) + ((((CNG_sig_Q10[ 16 + i - 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 0 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 2 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 1 ]))) + ((((CNG_sig_Q10[ 16 + i - 2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 1 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 3 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 2 ]))) + ((((CNG_sig_Q10[ 16 + i - 3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 2 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 4 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 3 ]))) + ((((CNG_sig_Q10[ 16 + i - 4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 3 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 5 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 4 ]))) + ((((CNG_sig_Q10[ 16 + i - 5 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 4 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 6 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 5 ]))) + ((((CNG_sig_Q10[ 16 + i - 6 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 5 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 7 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 6 ]))) + ((((CNG_sig_Q10[ 16 + i - 7 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 6 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 8 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 7 ]))) + ((((CNG_sig_Q10[ 16 + i - 8 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 7 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 9 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 8 ]))) + ((((CNG_sig_Q10[ 16 + i - 9 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 8 ]))) >> 16)));
            sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 10 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 9 ]))) + ((((CNG_sig_Q10[ 16 + i - 10 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 9 ]))) >> 16)));
            if( psDec->LPC_order == 16 ) {
                sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 11 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 10 ]))) + ((((CNG_sig_Q10[ 16 + i - 11 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 10 ]))) >> 16)));
                sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 12 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 11 ]))) + ((((CNG_sig_Q10[ 16 + i - 12 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 11 ]))) >> 16)));
                sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 13 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 12 ]))) + ((((CNG_sig_Q10[ 16 + i - 13 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 12 ]))) >> 16)));
                sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 14 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 13 ]))) + ((((CNG_sig_Q10[ 16 + i - 14 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 13 ]))) >> 16)));
                sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 15 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 14 ]))) + ((((CNG_sig_Q10[ 16 + i - 15 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 14 ]))) >> 16)));
                sum_Q6 = ((sum_Q6) + ((((CNG_sig_Q10[ 16 + i - 16 ]) >> 16) * (opus_int32)((opus_int16)(A_Q12[ 15 ]))) + ((((CNG_sig_Q10[ 16 + i - 16 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(A_Q12[ 15 ]))) >> 16)));
            }

            /* Update states */
            CNG_sig_Q10[ 16 + i ] = ((CNG_sig_Q10[ 16 + i ]) + ((opus_int32)((opus_uint32)((sum_Q6))<<((4)))));

            frame[ i ] = (opus_int16)(((((opus_int32)(frame[ i ])) + ((((6) == 1 ? ((sum_Q6) >> 1) + ((sum_Q6) & 1) : (((sum_Q6) >> ((6) - 1)) + 1) >> 1))))) > 0x7FFF ? 0x7FFF : (((((opus_int32)(frame[ i ])) + ((((6) == 1 ? ((sum_Q6) >> 1) + ((sum_Q6) & 1) : (((sum_Q6) >> ((6) - 1)) + 1) >> 1))))) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : ((((opus_int32)(frame[ i ])) + ((((6) == 1 ? ((sum_Q6) >> 1) + ((sum_Q6) & 1) : (((sum_Q6) >> ((6) - 1)) + 1) >> 1)))))));
        }
        memcpy((byte*)&(psCNG->CNG_synth_state), (byte*)(&CNG_sig_Q10[ length ]), (16 * ((int)(opus_int32 ' size  ))));
    } else {
        memset((char*)&(psCNG->CNG_synth_state), (0), (psDec->LPC_order * ((int)(opus_int32 ' size  ))));
    }
}
#end unsafe
