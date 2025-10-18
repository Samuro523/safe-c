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


/* Residual energy: nrg = wxx - 2 * wXx * c + c' * wXX * c */
float silk_residual_energy_covar_FLP(                              /* O    Weighted residual energy                    */
          float                *c,                                 /* I    Filter coefficients                         */
    float                      *wXX,                               /* I/O  Weighted correlation matrix, reg. out       */
          float                *wXx,                               /* I    Weighted correlation vector                 */
          float                wxx,                                /* I    Weighted correlation value                  */
          int                  D                                   /* I    Dimension                                   */
);

/* Calculates residual energies of input subframes where all subframes have LPC_order   */
/* of preceeding samples                                                                */
void silk_residual_energy_FLP(
    float                      nrgs*, //[ 4 ],               /* O    Residual energy per subframe                */
          float                x*,                                /* I    Input signal                                */
    float                      a*[ 16 ],            /* I    AR coefs for each frame half                */
          float                gains*,                            /* I    Quantization gains                          */
          int                  subfr_length,                       /* I    Subframe length                             */
          int                  nb_subfr,                           /* I    number of subframes                         */
          int                  LPC_order                           /* I    LPC order                                   */
);

#end unsafe
