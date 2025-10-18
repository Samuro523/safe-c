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

use opus_types, structs, control, entcode;

/************************/
/* Decoder Super Struct */
/************************/
struct  silk_decoder {
    silk_decoder_state          channel_state[ 2 ];
    stereo_dec_state                sStereo;
    int                         nChannelsAPI;
    int                         nChannelsInternal;
    int                         prev_decode_only_middle;
}

/*********************/
/* Decoder functions */
/*********************/

int silk_Get_Decoder_Size(                         /* O    Returns error code                              */
    int                        *decSizeBytes       /* O    Number of bytes in SILK decoder state           */
);


/* Reset decoder state */
int silk_InitDecoder(                              /* O    Returns error code                              */
    byte                            *decState           /* I/O  State                                           */
);

/* Decode a frame */
int silk_Decode(                                   /* O    Returns error code                              */
    byte*                           decState,           /* I/O  State                                           */
    silk_DecControlStruct*          decControl,         /* I/O  Control Structure                               */
    int                        lostFlag,           /* I    0: no loss, 1 loss, 2 decode fec                */
    int                        newPacketFlag,      /* I    Indicates first decoder call for this packet    */
    ec_dec                          *psRangeDec,        /* I/O  Compressor data structure                       */
    opus_int16                      *samplesOut,        /* O    Decoded output speech vector                    */
    opus_int32                      *nSamplesOut        /* O    Number of samples decoded                       */
);

#end unsafe
