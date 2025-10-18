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

/* Conversion between prediction filter coefficients and NLSFs  */
/* Requires the order to be an even number                      */
/* A piecewise linear approximation maps LSF <-> cos(LSF)       */
/* Therefore the result is not accurate NLSFs, but the two      */
/* functions are accurate inverses of each other                */

use opus_types;


/* Number of binary divisions, when not in low complexity mode */

/* Helper function for A2NLSF(..)                    */
/* Transforms polynomials from cos(n*f) to cos(f)^n  */

void silk_A2NLSF_trans_poly(
    opus_int32          *p,                     /* I/O    Polynomial                                */
    int      dd                      /* I      Polynomial order (= filter order / 2 )    */
);


/* Helper function for A2NLSF(..) */
/* Polynomial evaluation          */
opus_int32 silk_A2NLSF_eval_poly( /* return the polynomial evaluation, in Q16     */
    opus_int32          *p,                     /* I    Polynomial, Q16                         */
          opus_int32    x,                      /* I    Evaluation point, Q12                   */
          int      dd                      /* I    Order                                   */
);


void silk_A2NLSF_init(
           opus_int32    *a_Q16,
     opus_int32          *P,
     opus_int32          *Q,
           int      dd
);



/* Compute Normalized Line Spectral Frequencies (NLSFs) from whitening filter coefficients      */
/* If not all roots are found, the a_Q16 coefficients are bandwidth expanded until convergence. */
void silk_A2NLSF(
    opus_int16                  *NLSF,              /* O    Normalized Line Spectral Frequencies in Q15 (0..2^15-1) [d] */
    opus_int32                  *a_Q16,             /* I/O  Monic whitening filter coefficients in Q16 [d]              */
          int              d                   /* I    Filter order (must be even)                                 */
);

#end unsafe
