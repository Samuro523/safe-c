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

use opus_types, structs;


void silk_NSQ(
          silk_encoder_state    *psEncC,                                    /* I/O  Encoder State                   */
    silk_nsq_state              *NSQ,                                       /* I/O  NSQ state                       */
    SideInfoIndices             *psIndices,                                 /* I/O  Quantization Indices            */
          opus_int32            x_Q3*,                                     /* I    Prefiltered input signal        */
    int1                   pulses*,                                   /* O    Quantized pulse signal          */
          opus_int16            PredCoef_Q12*, // [ 2 * 16 ],          /* I    Short term prediction coefs     */
          opus_int16            LTPCoef_Q14*, // [ 5 * 4 ],    /* I    Long term prediction coefs      */
          opus_int16            AR2_Q13*, // [ 4 * 16 ], /* I Noise shaping coefs             */
          int              HarmShapeGain_Q14*, // [ 4 ],          /* I    Long term shaping coefs         */
          int              Tilt_Q14*, // [ 4 ],                   /* I    Spectral tilt                   */
          opus_int32            LF_shp_Q14*, // [ 4 ],                 /* I    Low frequency shaping coefs     */
          opus_int32            Gains_Q16*, // [ 4 ],                  /* I    Quantization step sizes         */
          int              pitchL*, // [ 4 ],                     /* I    Pitch lags                      */
          int              Lambda_Q10,                                 /* I    Rate/distortion tradeoff        */
          int              LTP_scale_Q14                               /* I    LTP state scaling               */
);

/***********************************/
/* silk_noise_shape_quantizer  */
/***********************************/
void silk_noise_shape_quantizer(
    silk_nsq_state      *NSQ,                   /* I/O  NSQ state                       */
    int            signalType,             /* I    Signal type                     */
          opus_int32    x_sc_Q10*,             /* I                                    */
    int1           pulses*,               /* O                                    */
    opus_int16          xq*,                   /* O                                    */
    opus_int32          sLTP_Q15*,             /* I/O  LTP state                       */
          opus_int16    a_Q12*,                /* I    Short term prediction coefs     */
          opus_int16    b_Q14*,                /* I    Long term prediction coefs      */
          opus_int16    AR_shp_Q13*,           /* I    Noise shaping AR coefs          */
    int            lag,                    /* I    Pitch lag                       */
    opus_int32          HarmShapeFIRPacked_Q14, /* I                                    */
    int            Tilt_Q14,               /* I    Spectral tilt                   */
    opus_int32          LF_shp_Q14,             /* I                                    */
    opus_int32          Gain_Q16,               /* I                                    */
    int            Lambda_Q10,             /* I                                    */
    int            offset_Q10,             /* I                                    */
    int            length,                 /* I    Input length                    */
    int            shapingLPCOrder,        /* I    Noise shaping AR filter order   */
    int            predictLPCOrder         /* I    Prediction filter order         */
);

void silk_nsq_scale_states(
          silk_encoder_state *psEncC,           /* I    Encoder State                   */
    silk_nsq_state      *NSQ,                   /* I/O  NSQ state                       */
          opus_int32    x_Q3*,                 /* I    input in Q3                     */
    opus_int32          x_sc_Q10*,             /* O    input scaled with 1/Gain        */
          opus_int16    sLTP*,                 /* I    re-whitened LTP state in Q0     */
    opus_int32          sLTP_Q15*,             /* O    LTP state matching scaled input */
    int            subfr,                  /* I    subframe number                 */
          int      LTP_scale_Q14,          /* I                                    */
          opus_int32    Gains_Q16*, // [ 4 ], /* I                                 */
          int      pitchL*, // [ 4 ], /* I    Pitch lag                       */
          int      signal_type             /* I    Signal type                     */
);
#end unsafe
