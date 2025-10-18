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
use control;
use entcode;

/****************************************/
/* Encoder functions                    */
/****************************************/

int silk_Get_Encoder_Size(                         /* O    Returns error code                              */
    int                        *encSizeBytes       /* O    Number of bytes in SILK encoder state           */
);

/*************************/
/* Init or Reset encoder */
/*************************/
int silk_InitEncoder(                              /* O    Returns error code                              */
    byte                            *encState,          /* I/O  State                                           */
    silk_EncControlStruct           *encStatus          /* O    Encoder Status                                  */
);

/***************************************/
/* Read control structure from encoder */
/***************************************/
int silk_QueryEncoder(                             /* O    Returns error code                              */
          byte                      *encState,          /* I    State                                           */
    silk_EncControlStruct           *encStatus          /* O    Encoder Status                                  */
);

/**************************/
/* Encode frame with Silk */
/**************************/
/* Note: if prefillFlag is set, the input must contain 10 ms of audio, irrespective of what                     */
/* encControl->payloadSize_ms is set to                                                                         */
int silk_Encode(                                   /* O    Returns error code                              */
    byte                            *encState,          /* I/O  State                                           */
    silk_EncControlStruct           *encControl,        /* I    Control status                                  */
          opus_int16                *samplesIn,         /* I    Speech sample input vector                      */
    int                        nSamplesIn,         /* I    Number of samples in input vector               */
    ec_enc                          *psRangeEnc,        /* I/O  Compressor data structure                       */
    opus_int32                      *nBytesOut,         /* I/O  Number of bytes in payload (input: Max bytes)   */
          int                  prefillFlag         /* I    Flag to indicate prefilling buffers no coding   */
);

#end unsafe
