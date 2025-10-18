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


/************************************************/
/* LPC analysis filter                          */
/* NB! State is kept internally and the         */
/* filter always starts with zero state         */
/* first Order output samples are set to zero   */
/************************************************/

/* 16th order LPC analysis filter, does not write first 16 samples */
void silk_LPC_analysis_filter16_FLP(
          float                 r_LPC*,            /* O    LPC residual signal                     */
          float                 PredCoef*,         /* I    LPC coefficients                        */
          float                 s*,                /* I    Input signal                            */
          int                   length              /* I    Length of input signal                  */
);

/* 12th order LPC analysis filter, does not write first 12 samples */
void silk_LPC_analysis_filter12_FLP(
          float                 r_LPC*,            /* O    LPC residual signal                     */
          float                 PredCoef*,         /* I    LPC coefficients                        */
          float                 s*,                /* I    Input signal                            */
          int                   length              /* I    Length of input signal                  */
);


/* 10th order LPC analysis filter, does not write first 10 samples */
void silk_LPC_analysis_filter10_FLP(
          float                 r_LPC*,            /* O    LPC residual signal                     */
          float                 PredCoef*,         /* I    LPC coefficients                        */
          float                 s*,                /* I    Input signal                            */
          int                   length              /* I    Length of input signal                  */
);

/* 8th order LPC analysis filter, does not write first 8 samples */
void silk_LPC_analysis_filter8_FLP(
          float                 r_LPC*,            /* O    LPC residual signal                     */
          float                 PredCoef*,         /* I    LPC coefficients                        */
          float                 s*,                /* I    Input signal                            */
          int                   length              /* I    Length of input signal                  */
);


/* 6th order LPC analysis filter, does not write first 6 samples */
void silk_LPC_analysis_filter6_FLP(
          float                 r_LPC*,            /* O    LPC residual signal                     */
          float                 PredCoef*,         /* I    LPC coefficients                        */
          float                 s*,                /* I    Input signal                            */
          int                   length              /* I    Length of input signal                  */
);


/************************************************/
/* LPC analysis filter                          */
/* NB! State is kept internally and the         */
/* filter always starts with zero state         */
/* first Order output samples are set to zero   */
/************************************************/
void silk_LPC_analysis_filter_FLP(
    float                      r_LPC*,                            /* O    LPC residual signal                         */
          float                PredCoef*,                         /* I    LPC coefficients                            */
          float                s*,                                /* I    Input signal                                */
          int                  length,                             /* I    Length of input signal                      */
          int                  Order                               /* I    LPC order                                   */
);

#end unsafe
