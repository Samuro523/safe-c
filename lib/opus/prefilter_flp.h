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


void silk_warped_LPC_analysis_filter_FLP(
          float                 state*,            /* I/O  State [order + 1]                       */
          float                 res*,              /* O    Residual signal [length]                */
          float                 coef*,             /* I    Coefficients [order]                    */
          float                 input*,            /* I    Input signal [length]                   */
          float                 lambda,             /* I    Warping factor                          */
          int                   length,             /* I    Length of input signal                  */
          int                   order               /* I    Filter order (even)                     */
);


/*
* silk_prefilter. Main prefilter function
*/
void silk_prefilter_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
          silk_encoder_control_FLP  *psEncCtrl,                         /* I    Encoder control FLP                         */
    float                      xw*,                               /* O    Weighted signal                             */
          float                x*                                 /* I    Speech signal                               */
);

/*
* Prefilter for finding Quantizer input signal
*/
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
);

#end unsafe
