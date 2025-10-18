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

/**********************************************************************
 * Function to solve linear equation Ax = b, when A is a MxM
 * symmetric square matrix - using LDL factorisation
 **********************************************************************/
void silk_solve_LDL_FLP(
    float                      *A,                                 /* I/O  Symmetric square matrix, out: reg.          */
          int                  M,                                  /* I    Size of matrix                              */
          float                *b,                                 /* I    Pointer to b vector                         */
    float                      *x                                  /* O    Pointer to x solution vector                */
);


void silk_SolveWithUpperTriangularFromLowerWdiagOnes_FLP(
          float    *L,         /* I    Pointer to Lower Triangular Matrix                              */
    int            M,          /* I    Dim of Matrix equation                                          */
          float    *b,         /* I    b Vector                                                        */
    float          *x          /* O    x Vector                                                        */
);



void silk_SolveWithLowerTriangularWdiagOnes_FLP(
          float    *L,         /* I    Pointer to Lower Triangular Matrix                              */
    int            M,          /* I    Dim of Matrix equation                                          */
          float    *b,         /* I    b Vector                                                        */
    float          *x          /* O    x Vector                                                        */
);


void silk_LDL_FLP(
    float          *A,         /* I/O  Pointer to Symetric Square Matrix                               */
    int            M,          /* I    Size of Matrix                                                  */
    float          *L,         /* I/O  Pointer to Square Upper triangular Matrix                       */
    float          *Dinv       /* I/O  Pointer to vector holding the inverse diagonal elements of D    */
);

#end unsafe
