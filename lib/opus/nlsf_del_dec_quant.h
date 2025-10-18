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

/* Delayed-decision quantizer for NLSF residuals */
opus_int32 silk_NLSF_del_dec_quant(                             /* O    Returns RD value in Q25                     */
    int1                   indices*,                      /* O    Quantization indices [ order ]              */
          opus_int16            x_Q10*,                        /* I    Input [ order ]                             */
          opus_int16            w_Q5*,                         /* I    Weights [ order ]                           */
          byte            pred_coef_Q8*,                 /* I    Backward predictor coefs [ order ]          */
          opus_int16            ec_ix*,                        /* I    Indices to entropy coding tables [ order ]  */
          byte            ec_rates_Q5*,                  /* I    Rates []                                    */
          int              quant_step_size_Q16,            /* I    Quantization step size                      */
          opus_int16            inv_quant_step_size_Q6,         /* I    Inverse quantization step size              */
          opus_int32            mu_Q20,                         /* I    R/D tradeoff                                */
          opus_int16            order                           /* I    Number of input values                      */
);

#end unsafe
