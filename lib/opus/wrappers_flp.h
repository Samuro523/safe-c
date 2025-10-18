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
use structs, structs_flp;

/* Wrappers. Calls flp / fix code */

/* Convert AR filter coefficients to NLSF parameters */
void silk_A2NLSF_FLP(
    opus_int16                      *NLSF_Q15,                          /* O    NLSF vector      [ LPC_order ]              */
          float                *pAR,                               /* I    LPC coefficients [ LPC_order ]              */
          int                  LPC_order                           /* I    LPC order                                   */
);



/* Convert LSF parameters to AR prediction filter coefficients */
void silk_NLSF2A_FLP(
    float                      *pAR,                               /* O    LPC coefficients [ LPC_order ]              */
          opus_int16                *NLSF_Q15,                          /* I    NLSF vector      [ LPC_order ]              */
          int                  LPC_order                           /* I    LPC order                                   */
);


/******************************************/
/* Floating-point NLSF processing wrapper */
/******************************************/
void silk_process_NLSFs_FLP(
    silk_encoder_state              *psEncC,                            /* I/O  Encoder state                               */
    float                      PredCoef*[ 16 ],     /* O    Prediction coefficients                     */
    opus_int16                      NLSF_Q15*,     /* I/O  Normalized LSFs (quant out) (0 - (2^15-1))  */
          opus_int16                prev_NLSF_Q15*      /* I    Previous Normalized LSFs (0 - (2^15-1))     */
);


/****************************************/
/* Floating-point Silk NSQ wrapper      */
/****************************************/
void silk_NSQ_wrapper_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
    silk_encoder_control_FLP        *psEncCtrl,                         /* I/O  Encoder control FLP                         */
    SideInfoIndices                 *psIndices,                         /* I/O  Quantization indices                        */
    silk_nsq_state                  *psNSQ,                             /* I/O  Noise Shaping Quantzation state             */
    int1                       pulses*,                           /* O    Quantized pulse signal                      */
          float                x*                                 /* I    Prefiltered input signal                    */
);

/***********************************************/
/* Floating-point Silk LTP quantiation wrapper */
/***********************************************/
void silk_quant_LTP_gains_FLP(
    float                      B*,  //[ 4 * 5 ],      /* I/O  (Un-)quantized LTP gains                    */
    int1                       cbk_index*, // [ 4 ],          /* O    Codebook index                              */
    int1                       *periodicity_index,                 /* O    Periodicity index                           */
          float                W*,  // [ 4 * 5 * 5 ], /* I    Error weights                        */
          int                  mu_Q10,                             /* I    Mu value (R/D tradeoff)                     */
          int                  lowComplexity,                      /* I    Flag for low complexity                     */
          int                  nb_subfr                            /* I    number of subframes                         */
);

#end unsafe
