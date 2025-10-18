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

/* Gain scalar quantization with hysteresis, uniform on log scale */
void silk_gains_quant(
    int1                   ind*, //[ 4 ],            /* O    gain indices                                */
    opus_int32                  gain_Q16*, //[ 4 ],       /* I/O  gains (quantized out)                       */
    int1                   *prev_ind,                      /* I/O  last index in previous frame                */
          int              conditional,                    /* I    first gain is delta coded if 1              */
          int              nb_subfr                        /* I    number of subframes                         */
);

/* Gains scalar dequantization, uniform on log scale */
void silk_gains_dequant(
    opus_int32                  gain_Q16*, //[ 4 ],       /* O    quantized gains                             */
          int1             ind*,   // [ 4 ],            /* I    gain indices                                */
    int1                   *prev_ind,                      /* I/O  last index in previous frame                */
          int              conditional,                    /* I    first gain is delta coded if 1              */
          int              nb_subfr                        /* I    number of subframes                          */
);

/* Compute unique identifier of gain indices vector */
opus_int32 silk_gains_ID(                                       /* O    returns unique identifier of gains          */
          int1             ind*, // [ 4 ],            /* I    gain indices                                */
          int              nb_subfr                        /* I    number of subframes                         */
);

#end unsafe
