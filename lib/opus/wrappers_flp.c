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
use structs, structs_flp, sigproc_flp, a2nlsf, nlsf2a, process_nlsfs;
use tables, nsq_del_dec, nsq, quant_ltp_gains;


/* Wrappers. Calls flp / fix code */

/* Convert AR filter coefficients to NLSF parameters */
public
void silk_A2NLSF_FLP(
    opus_int16                      *NLSF_Q15,                          /* O    NLSF vector      [ LPC_order ]              */
          float                *pAR,                               /* I    LPC coefficients [ LPC_order ]              */
          int                  LPC_order                           /* I    LPC order                                   */
)
{
    int   i;
    opus_int32 a_fix_Q16[ 16 ];

    for( i = 0; i < LPC_order; i++ ) {
        a_fix_Q16[ i ] = silk_float2int( pAR[ i ] * 65536.0f );
    }

    silk_A2NLSF( NLSF_Q15, &a_fix_Q16, LPC_order );
}

/* Convert LSF parameters to AR prediction filter coefficients */
public
void silk_NLSF2A_FLP(
    float                      *pAR,                               /* O    LPC coefficients [ LPC_order ]              */
          opus_int16                *NLSF_Q15,                          /* I    NLSF vector      [ LPC_order ]              */
          int                  LPC_order                           /* I    LPC order                                   */
)
{
    int   i;
    opus_int16 a_fix_Q12[ 16 ];

    silk_NLSF2A( &a_fix_Q12, NLSF_Q15, LPC_order );

    for( i = 0; i < LPC_order; i++ ) {
        pAR[ i ] = ( float )a_fix_Q12[ i ] * ( 1.0f / 4096.0f );
    }
}

/******************************************/
/* Floating-point NLSF processing wrapper */
/******************************************/
public
void silk_process_NLSFs_FLP(
    silk_encoder_state              *psEncC,                            /* I/O  Encoder state                               */
    float                      PredCoef*[ 16 ],     /* O    Prediction coefficients                     */
    opus_int16                      NLSF_Q15*,     /* I/O  Normalized LSFs (quant out) (0 - (2^15-1))  */
          opus_int16                prev_NLSF_Q15*      /* I    Previous Normalized LSFs (0 - (2^15-1))     */
)
{
    int     i, j;
    opus_int16   PredCoef_Q12[ 2 ][ 16 ];

	clear PredCoef_Q12;
    silk_process_NLSFs( psEncC, &PredCoef_Q12, NLSF_Q15, prev_NLSF_Q15);

    for( j = 0; j < 2; j++ ) {
        for( i = 0; i < psEncC->predictLPCOrder; i++ ) {
            PredCoef[ j ][ i ] = ( float )PredCoef_Q12[ j ][ i ] * ( 1.0f / 4096.0f );
        }
    }
}

/****************************************/
/* Floating-point Silk NSQ wrapper      */
/****************************************/
public
void silk_NSQ_wrapper_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
    silk_encoder_control_FLP        *psEncCtrl,                         /* I/O  Encoder control FLP                         */
    SideInfoIndices                 *psIndices,                         /* I/O  Quantization indices                        */
    silk_nsq_state                  *psNSQ,                             /* I/O  Noise Shaping Quantzation state             */
    int1                       pulses*,                           /* O    Quantized pulse signal                      */
          float                x*                                 /* I    Prefiltered input signal                    */
)
{
    int     i, j;
    opus_int32   x_Q3[ ( ( 5 * 4 ) * 16 ) ];
    opus_int32   Gains_Q16[ 4 ];
     opus_int16 PredCoef_Q12[ 2 ][ 16 ];
    opus_int16   LTPCoef_Q14[ 5 * 4 ];
    int     LTP_scale_Q14;

    /* Noise shaping parameters */
    opus_int16   AR2_Q13[ 4 * 16 ];
    opus_int32   LF_shp_Q14[ 4 ];         /* Packs two int16 coefficients per int32 value             */
    int     Lambda_Q10;
    int     Tilt_Q14[ 4 ];
    int     HarmShapeGain_Q14[ 4 ];

	clear PredCoef_Q12;
	
    /* Convert control struct to fix control struct */
    /* Noise shape parameters */
    for( i = 0; i < psEnc->sCmn.nb_subfr; i++ ) {
        for( j = 0; j < psEnc->sCmn.shapingLPCOrder; j++ ) {
            AR2_Q13[ i * 16 + j ] = (opus_int16)silk_float2int( psEncCtrl->AR2[ i * 16 + j ] * 8192.0f );
        }
    }

    for( i = 0; i < psEnc->sCmn.nb_subfr; i++ ) {
        LF_shp_Q14[ i ] =   ((opus_int32)((opus_uint32)(silk_float2int( psEncCtrl->LF_AR_shp[ i ] * 16384.0f ))<<(16))) |
                              (int)(opus_uint16)silk_float2int( psEncCtrl->LF_MA_shp[ i ]     * 16384.0f );
        Tilt_Q14[ i ]   =        (int)silk_float2int( psEncCtrl->Tilt[ i ]          * 16384.0f );
        HarmShapeGain_Q14[ i ] = (int)silk_float2int( psEncCtrl->HarmShapeGain[ i ] * 16384.0f );
    }
    Lambda_Q10 = ( int )silk_float2int( psEncCtrl->Lambda * 1024.0f );

    /* prediction and coding parameters */
    for( i = 0; i < psEnc->sCmn.nb_subfr * 5; i++ ) {
        LTPCoef_Q14[ i ] = (opus_int16)silk_float2int( psEncCtrl->LTPCoef[ i ] * 16384.0f );
    }

    for( j = 0; j < 2; j++ ) {
        for( i = 0; i < psEnc->sCmn.predictLPCOrder; i++ ) {
            PredCoef_Q12[ j ][ i ] = (opus_int16)silk_float2int( psEncCtrl->PredCoef[ j ][ i ] * 4096.0f );
        }
    }

    for( i = 0; i < psEnc->sCmn.nb_subfr; i++ ) {
        Gains_Q16[ i ] = silk_float2int( psEncCtrl->Gains[ i ] * 65536.0f );
        ;
    }

    if( psIndices->signalType == 2 ) {
        LTP_scale_Q14 = silk_LTPScales_table_Q14[ psIndices->LTP_scaleIndex ];
    } else {
        LTP_scale_Q14 = 0;
    }

    /* Convert input to fix */
    for( i = 0; i < psEnc->sCmn.frame_length; i++ ) {
        x_Q3[ i ] = silk_float2int( 8.0 * x[ i ] );
    }

    /* Call NSQ */
    if( psEnc->sCmn.nStatesDelayedDecision > 1 || psEnc->sCmn.warping_Q16 > 0 ) {
        silk_NSQ_del_dec( &psEnc->sCmn, psNSQ, psIndices, &x_Q3, pulses, &PredCoef_Q12[ 0 ], &LTPCoef_Q14,
            &AR2_Q13, &HarmShapeGain_Q14, &Tilt_Q14, &LF_shp_Q14, &Gains_Q16, &psEncCtrl->pitchL, Lambda_Q10, LTP_scale_Q14 );
    } else {
        silk_NSQ( &psEnc->sCmn, psNSQ, psIndices, &x_Q3, pulses, &PredCoef_Q12[ 0 ], &LTPCoef_Q14,
            &AR2_Q13, &HarmShapeGain_Q14, &Tilt_Q14, &LF_shp_Q14, &Gains_Q16, &psEncCtrl->pitchL, Lambda_Q10, LTP_scale_Q14 );
    }
}

/***********************************************/
/* Floating-point Silk LTP quantiation wrapper */
/***********************************************/
public
void silk_quant_LTP_gains_FLP(
    float                      B*,  //[ 4 * 5 ],      /* I/O  (Un-)quantized LTP gains                    */
    int1                       cbk_index*, // [ 4 ],          /* O    Codebook index                              */
    int1                       *periodicity_index,                 /* O    Periodicity index                           */
          float                W*,  // [ 4 * 5 * 5 ], /* I    Error weights                        */
          int                  mu_Q10,                             /* I    Mu value (R/D tradeoff)                     */
          int                  lowComplexity,                      /* I    Flag for low complexity                     */
          int                  nb_subfr                            /* I    number of subframes                         */
)
{
    int   i;
    opus_int16 B_Q14[ 4 * 5 ];
    opus_int32 W_Q18[ 4*5*5 ];

	clear B_Q14, W_Q18;
	
    for( i = 0; i < nb_subfr * 5; i++ ) {
        B_Q14[ i ] = (opus_int16)silk_float2int( B[ i ] * 16384.0f );
    }
    for( i = 0; i < nb_subfr * 5 * 5; i++ ) {
        W_Q18[ i ] = (opus_int32)silk_float2int( W[ i ] * 262144.0f );
    }

    silk_quant_LTP_gains( &B_Q14, cbk_index, periodicity_index, &W_Q18, mu_Q10, lowComplexity, nb_subfr );

    for( i = 0; i < nb_subfr * 5; i++ ) {
        B[ i ] = (float)B_Q14[ i ] * ( 1.0f / 16384.0f );
    }
}
#end unsafe
