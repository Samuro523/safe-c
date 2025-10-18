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
use structs;

/********************************/
/* Noise shaping analysis state */
/********************************/
struct  silk_shape_state_FLP
{
    int1                   LastGainIndex;
    float                  HarmBoost_smth;
    float                  HarmShapeGain_smth;
    float                  Tilt_smth;
}

/********************************/
/* Prefilter state              */
/********************************/
 struct  silk_prefilter_state_FLP
 {
    float                  sLTP_shp[ 512 ];
    float                  sAR_shp[ 16 + 1 ];
    int                    sLTP_shp_buf_idx;
    float                  sLF_AR_shp;
    float                  sLF_MA_shp;
    float                  sHarmHP;
    opus_int32                  rand_seed;
    int                    lagPrev;
}

/********************************/
/* Encoder state FLP            */
/********************************/
struct  silk_encoder_state_FLP
{
    silk_encoder_state          sCmn;                               /* Common struct, shared with fixed-point code */
    silk_shape_state_FLP        sShape;                             /* Noise shaping state */
    silk_prefilter_state_FLP    sPrefilt;                           /* Prefilter State */

    /* Buffer for find pitch and noise shape analysis */
    float                  x_buf[ 2 * ( ( 5 * 4 ) * 16 ) + ( 5 * 16 ) ];/* Buffer for find pitch and noise shape analysis */
    float                  LTPCorr;                            /* Normalized correlation from pitch lag estimator */
}

/************************/
/* Encoder control FLP  */
/************************/
struct  silk_encoder_control_FLP
{
    /* Prediction and coding parameters */
    float                  Gains[ 4 ];
    float                  PredCoef[ 2 ][ 16 ];     /* holds interpolated and final coefficients */
    float                  LTPCoef[5 * 4];
    float                  LTP_scale;
    int                    pitchL[ 4 ];

    /* Noise shaping parameters */
    float                  AR1[ 4 * 16 ];
    float                  AR2[ 4 * 16 ];
    float                  LF_MA_shp[     4 ];
    float                  LF_AR_shp[     4 ];
    float                  GainsPre[      4 ];
    float                  HarmBoost[     4 ];
    float                  Tilt[          4 ];
    float                  HarmShapeGain[ 4 ];
    float                  Lambda;
    float                  input_quality;
    float                  coding_quality;

    /* Measures */
    float                  sparseness;
    float                  predGain;
    float                  LTPredCodGain;
    float                  ResNrg[ 4 ];             /* Residual energy per subframe */

    /* Parameters for CBR mode */
    opus_int32                  GainsUnq_Q16[ 4 ];
    int1                   lastGainIndexPrev;
}

/************************/
/* Encoder Super Struct */
/************************/
struct  silk_encoder
{
    silk_encoder_state_FLP      state_Fxx[ 2 ];
    stereo_enc_state            sStereo;
    opus_int32                  nBitsExceeded;
    int                    nChannelsAPI;
    int                    nChannelsInternal;
    int                    nPrevChannelsInternal;
    int                    timeSinceSwitchAllowed_ms;
    int                    allowBandwidthSwitch;
    int                    prev_decode_only_middle;
}

#end unsafe
