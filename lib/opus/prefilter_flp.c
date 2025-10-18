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

use structs_flp;

public
void silk_warped_LPC_analysis_filter_FLP(
          float                 state*,            /* I/O  State [order + 1]                       */
          float                 res*,              /* O    Residual signal [length]                */
          float                 coef*,             /* I    Coefficients [order]                    */
          float                 input*,            /* I    Input signal [length]                   */
          float                 lambda,             /* I    Warping factor                          */
          int                   length,             /* I    Length of input signal                  */
          int                   order               /* I    Filter order (even)                     */
)
{
    int     n, i;
    float   acc, tmp1, tmp2;

    /* Order must be even */
    ;

    for( n = 0; n < length; n++ ) {
        /* Output of lowpass section */
        tmp2 = state[ 0 ] + lambda * state[ 1 ];
        state[ 0 ] = input[ n ];
        /* Output of allpass section */
        tmp1 = state[ 1 ] + lambda * ( state[ 2 ] - tmp2 );
        state[ 1 ] = tmp2;
        acc = coef[ 0 ] * tmp2;
        /* Loop over allpass sections */
        for( i = 2; i < order; i += 2 ) {
            /* Output of allpass section */
            tmp2 = state[ i ] + lambda * ( state[ i + 1 ] - tmp1 );
            state[ i ] = tmp1;
            acc += coef[ i - 1 ] * tmp1;
            /* Output of allpass section */
            tmp1 = state[ i + 1 ] + lambda * ( state[ i + 2 ] - tmp2 );
            state[ i + 1 ] = tmp2;
            acc += coef[ i ] * tmp2;
        }
        state[ order ] = tmp1;
        acc += coef[ order - 1 ] * tmp1;
        res[ n ] = input[ n ] - acc;
    }
}


/*
* silk_prefilter. Main prefilter function
*/
public
void silk_prefilter_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
          silk_encoder_control_FLP  *psEncCtrl,                         /* I    Encoder control FLP                         */
    float                      xw*,                               /* O    Weighted signal                             */
          float                x*                                 /* I    Speech signal                               */
)
{
    silk_prefilter_state_FLP *P = &psEnc->sPrefilt;
    int   j, k, lag;
    float HarmShapeGain, Tilt, LF_MA_shp, LF_AR_shp;
    float B[ 2 ];
          float *AR1_shp;
          float *px;
    float *pxw;
    float HarmShapeFIR[ 3 ];
    float st_res[ ( 5 * 16 ) + 16 ];

	clear st_res, B, HarmShapeFIR;
	
    /* Set up pointers */
    px  = x;
    pxw = xw;
    lag = P->lagPrev;
    for( k = 0; k < psEnc->sCmn.nb_subfr; k++ ) {
        /* Update Variables that change per sub frame */
        if( psEnc->sCmn.indices.signalType == 2 ) {
            lag = psEncCtrl->pitchL[ k ];
        }

        /* Noise shape parameters */
        HarmShapeGain = psEncCtrl->HarmShapeGain[ k ] * ( 1.0f - psEncCtrl->HarmBoost[ k ] );
        HarmShapeFIR[ 0 ] = 0.25f               * HarmShapeGain;
        HarmShapeFIR[ 1 ] = 32767.0f / 65536.0f * HarmShapeGain;
        HarmShapeFIR[ 2 ] = 0.25f               * HarmShapeGain;
        Tilt      =  psEncCtrl->Tilt[ k ];
        LF_MA_shp =  psEncCtrl->LF_MA_shp[ k ];
        LF_AR_shp =  psEncCtrl->LF_AR_shp[ k ];
        AR1_shp   = &psEncCtrl->AR1[ k * 16 ];

        /* Short term FIR filtering */
        silk_warped_LPC_analysis_filter_FLP( &P->sAR_shp, &st_res, AR1_shp, px,
            (float)psEnc->sCmn.warping_Q16 / 65536.0f, psEnc->sCmn.subfr_length, psEnc->sCmn.shapingLPCOrder );

        /* Reduce (mainly) low frequencies during harmonic emphasis */
        B[ 0 ] =  psEncCtrl->GainsPre[ k ];
        B[ 1 ] = -psEncCtrl->GainsPre[ k ] *
            ( psEncCtrl->HarmBoost[ k ] * HarmShapeGain + 0.05f + psEncCtrl->coding_quality * 0.1f );
        pxw[ 0 ] = B[ 0 ] * st_res[ 0 ] + B[ 1 ] * P->sHarmHP;
        for( j = 1; j < psEnc->sCmn.subfr_length; j++ ) {
            pxw[ j ] = B[ 0 ] * st_res[ j ] + B[ 1 ] * st_res[ j - 1 ];
        }
        P->sHarmHP = st_res[ psEnc->sCmn.subfr_length - 1 ];

        silk_prefilt_FLP( P, pxw, pxw, &HarmShapeFIR, Tilt, LF_MA_shp, LF_AR_shp, lag, psEnc->sCmn.subfr_length );

        px  += psEnc->sCmn.subfr_length;
        pxw += psEnc->sCmn.subfr_length;
    }
    P->lagPrev = psEncCtrl->pitchL[ psEnc->sCmn.nb_subfr - 1 ];
}

/*
* Prefilter for finding Quantizer input signal
*/
public
void silk_prefilt_FLP(
    silk_prefilter_state_FLP    *P,                 /* I/O state */
    float                  st_res*,           /* I */
    float                  xw*,               /* O */
    float                  *HarmShapeFIR,      /* I */
    float                  Tilt,               /* I */
    float                  LF_MA_shp,          /* I */
    float                  LF_AR_shp,          /* I */
    int                    lag,                /* I */
    int                    length              /* I */
)
{
    int   i;
    int   idx, LTP_shp_buf_idx;
    float n_Tilt, n_LF, n_LTP;
    float sLF_AR_shp, sLF_MA_shp;
    float *LTP_shp_buf;

    /* To speed up use temp variables instead of using the struct */
    LTP_shp_buf     = &P->sLTP_shp;
    LTP_shp_buf_idx = P->sLTP_shp_buf_idx;
    sLF_AR_shp      = P->sLF_AR_shp;
    sLF_MA_shp      = P->sLF_MA_shp;

    for( i = 0; i < length; i++ ) {
        if( lag > 0 ) {
            ;
            idx = lag + LTP_shp_buf_idx;
            n_LTP  = LTP_shp_buf[ ( idx - 3 / 2 - 1) & ( 512 - 1 ) ] * HarmShapeFIR[ 0 ];
            n_LTP += LTP_shp_buf[ ( idx - 3 / 2    ) & ( 512 - 1 ) ] * HarmShapeFIR[ 1 ];
            n_LTP += LTP_shp_buf[ ( idx - 3 / 2 + 1) & ( 512 - 1 ) ] * HarmShapeFIR[ 2 ];
        } else {
            n_LTP = 0.0;
        }

        n_Tilt = sLF_AR_shp * Tilt;
        n_LF   = sLF_AR_shp * LF_AR_shp + sLF_MA_shp * LF_MA_shp;

        sLF_AR_shp = st_res[ i ] - n_Tilt;
        sLF_MA_shp = sLF_AR_shp - n_LF;

        LTP_shp_buf_idx = ( LTP_shp_buf_idx - 1 ) & ( 512 - 1 );
        LTP_shp_buf[ LTP_shp_buf_idx ] = sLF_MA_shp;

        xw[ i ] = sLF_MA_shp - n_LTP;
    }
    /* Copy temp variable back to state */
    P->sLF_AR_shp       = sLF_AR_shp;
    P->sLF_MA_shp       = sLF_MA_shp;
    P->sLTP_shp_buf_idx = LTP_shp_buf_idx;
}

#end unsafe
