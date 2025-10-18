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

use opus_types, structs, control, entdec, entcode;
use init_decoder, decoder_set_fs, resampler_structs, tables, stereo_decode_pred;
use decode_indices, decode_frame, decode_pulses, stereo_ms_to_lr;
use resampler;
use os_support;

/*********************/
/* Decoder functions */
/*********************/

public
int silk_Get_Decoder_Size(                         /* O    Returns error code                              */
    int                        *decSizeBytes       /* O    Number of bytes in SILK decoder state           */
)
{
    int ret = 0;

    *decSizeBytes = ((int)(silk_decoder ' size  ));

    return ret;
}

/* Reset decoder state */
public
int silk_InitDecoder(                              /* O    Returns error code                              */
    byte                            *decState           /* I/O  State                                           */
)
{
    int n, ret = 0;
    silk_decoder_state *channel_state = &((silk_decoder *)decState)->channel_state;

    for( n = 0; n < 2; n++ ) {
        ret  = silk_init_decoder( &channel_state[ n ] );
    }

    return ret;
}

/* Decode a frame */
public
int silk_Decode(                                   /* O    Returns error code                              */
    byte*                           decState,           /* I/O  State                                           */
    silk_DecControlStruct*          decControl,         /* I/O  Control Structure                               */
    int                        lostFlag,           /* I    0: no loss, 1 loss, 2 decode fec                */
    int                        newPacketFlag,      /* I    Indicates first decoder call for this packet    */
    ec_dec                          *psRangeDec,        /* I/O  Compressor data structure                       */
    opus_int16                      *samplesOut,        /* O    Decoded output speech vector                    */
    opus_int32                      *nSamplesOut        /* O    Number of samples decoded                       */
)
{
    int   i, n, decode_only_middle = 0, ret = 0;
    opus_int32 nSamplesOutDec, LBRR_symbol;
    opus_int16 samplesOut1_tmp[ 2 ][ 16 * ( 5 * 4 ) + 2 ];
    opus_int16 samplesOut2_tmp[ 48 * ( 5 * 4 ) ];
    opus_int32 MS_pred_Q13[ 2 ] = { 0, 0 };
    opus_int16 *resample_out_ptr;
    silk_decoder *psDec = ( silk_decoder * )decState;
    silk_decoder_state *channel_state = &psDec->channel_state;
    int has_side;
    int stereo_to_mono;

	clear samplesOut1_tmp, nSamplesOutDec;
	
    /**********************************/
    /* Test if first frame in payload */
    /**********************************/
    if( newPacketFlag!=0 ) {
        for( n = 0; n < decControl->nChannelsInternal; n++ ) {
            channel_state[ n ].nFramesDecoded = 0;  /* Used to count frames in packet */
        }
    }

    /* If Mono -> Stereo transition in bitstream: init state of second channel */
    if( decControl->nChannelsInternal > psDec->nChannelsInternal ) {
        ret += silk_init_decoder( &channel_state[ 1 ] );
    }

    stereo_to_mono = (int)(decControl->nChannelsInternal == 1 && psDec->nChannelsInternal == 2 &&
                     ( decControl->internalSampleRate == 1000*channel_state[ 0 ].fs_kHz ));

    if( channel_state[ 0 ].nFramesDecoded == 0 ) {
        for( n = 0; n < decControl->nChannelsInternal; n++ ) {
            int fs_kHz_dec;
            if( decControl->payloadSize_ms == 0 ) {
                /* Assuming packet loss, use 10 ms */
                channel_state[ n ].nFramesPerPacket = 1;
                channel_state[ n ].nb_subfr = 2;
            } else if( decControl->payloadSize_ms == 10 ) {
                channel_state[ n ].nFramesPerPacket = 1;
                channel_state[ n ].nb_subfr = 2;
            } else if( decControl->payloadSize_ms == 20 ) {
                channel_state[ n ].nFramesPerPacket = 1;
                channel_state[ n ].nb_subfr = 4;
            } else if( decControl->payloadSize_ms == 40 ) {
                channel_state[ n ].nFramesPerPacket = 2;
                channel_state[ n ].nb_subfr = 4;
            } else if( decControl->payloadSize_ms == 60 ) {
                channel_state[ n ].nFramesPerPacket = 3;
                channel_state[ n ].nb_subfr = 4;
            } else {
                ;
                return -203;
            }
            fs_kHz_dec = ( decControl->internalSampleRate >> 10 ) + 1;
            if( fs_kHz_dec != 8 && fs_kHz_dec != 12 && fs_kHz_dec != 16 ) {
                ;
                return -200;
            }
            ret += silk_decoder_set_fs( &channel_state[ n ], fs_kHz_dec, decControl->API_sampleRate );
        }
    }

    if( decControl->nChannelsAPI == 2 && decControl->nChannelsInternal == 2 && ( psDec->nChannelsAPI == 1 || psDec->nChannelsInternal == 1 ) ) {
        memset((char*)(&psDec->sStereo.pred_prev_Q13), (0), (((int)((psDec->sStereo.pred_prev_Q13) ' size  ))));
        memset((char*)&(psDec->sStereo.sSide), (0), (((int)((psDec->sStereo.sSide) ' size  ))));
        memcpy((byte*)(&channel_state[ 1 ].resampler_state), (byte*)(&channel_state[ 0 ].resampler_state), (((int)(silk_resampler_state_struct ' size  ))));
    }
    psDec->nChannelsAPI      = decControl->nChannelsAPI;
    psDec->nChannelsInternal = decControl->nChannelsInternal;

    if( decControl->API_sampleRate > (opus_int32)48 * 1000 || decControl->API_sampleRate < 8000 ) {
        ret = -200;
        return( ret );
    }

    if( lostFlag != 1 && channel_state[ 0 ].nFramesDecoded == 0 ) {
        /* First decoder call for this payload */
        /* Decode VAD flags and LBRR flag */
        for( n = 0; n < decControl->nChannelsInternal; n++ ) {
            for( i = 0; i < channel_state[ n ].nFramesPerPacket; i++ ) {
                channel_state[ n ].VAD_flags[ i ] = ec_dec_bit_logp(psRangeDec, 1);
            }
            channel_state[ n ].LBRR_flag = ec_dec_bit_logp(psRangeDec, 1);
        }
        /* Decode LBRR flags */
        for( n = 0; n < decControl->nChannelsInternal; n++ ) {
            memset((char*)&(channel_state[ n ].LBRR_flags), (0), (((int)((channel_state[ n ].LBRR_flags) ' size  ))));
            if( channel_state[ n ].LBRR_flag != 0 ) {
                if( channel_state[ n ].nFramesPerPacket == 1 ) {
                    channel_state[ n ].LBRR_flags[ 0 ] = 1;
                } else {
                    LBRR_symbol = ec_dec_icdf( psRangeDec, &silk_LBRR_flags_iCDF_ptr[ channel_state[ n ].nFramesPerPacket - 2 ], 8 ) + 1;
                    for( i = 0; i < channel_state[ n ].nFramesPerPacket; i++ ) {
                        channel_state[ n ].LBRR_flags[ i ] = ((LBRR_symbol)>>(i)) & 1;
                    }
                }
            }
        }

        if( lostFlag == 0 ) {
            /* Regular decoding: skip all LBRR data */
            for( i = 0; i < channel_state[ 0 ].nFramesPerPacket; i++ ) {
                for( n = 0; n < decControl->nChannelsInternal; n++ ) {
                    if( channel_state[ n ].LBRR_flags[ i ] != 0 ) {
                        int pulses[ ( ( 5 * 4 ) * 16 ) ];
                        int condCoding;

						clear pulses;
						
                        if( decControl->nChannelsInternal == 2 && n == 0 ) {
                            silk_stereo_decode_pred( psRangeDec, MS_pred_Q13 );
                            if( channel_state[ 1 ].LBRR_flags[ i ] == 0 ) {
                                silk_stereo_decode_mid_only( psRangeDec, &decode_only_middle );
                            }
                        }
                        /* Use conditional coding if previous frame available */
                        if( i > 0 && channel_state[ n ].LBRR_flags[ i - 1 ] != 0 ) {
                            condCoding = 2;
                        } else {
                            condCoding = 0;
                        }
                        silk_decode_indices( &channel_state[ n ], psRangeDec, i, 1, condCoding );
                        silk_decode_pulses( psRangeDec, &pulses, channel_state[ n ].indices.signalType,
                            channel_state[ n ].indices.quantOffsetType, channel_state[ n ].frame_length );
                    }
                }
            }
        }
    }

    /* Get MS predictor index */
    if( decControl->nChannelsInternal == 2 ) {
        if(   lostFlag == 0 ||
            ( lostFlag == 2 && channel_state[ 0 ].LBRR_flags[ channel_state[ 0 ].nFramesDecoded ] == 1 ) )
        {
            silk_stereo_decode_pred( psRangeDec, MS_pred_Q13 );
            /* For LBRR data, decode mid-only flag only if side-channel's LBRR flag is false */
            if( ( lostFlag == 0 && channel_state[ 1 ].VAD_flags[ channel_state[ 0 ].nFramesDecoded ] == 0 ) ||
                ( lostFlag == 2 && channel_state[ 1 ].LBRR_flags[ channel_state[ 0 ].nFramesDecoded ] == 0 ) )
            {
                silk_stereo_decode_mid_only( psRangeDec, &decode_only_middle );
            } else {
                decode_only_middle = 0;
            }
        } else {
            for( n = 0; n < 2; n++ ) {
                MS_pred_Q13[ n ] = psDec->sStereo.pred_prev_Q13[ n ];
            }
        }
    }

    /* Reset side channel decoder prediction memory for first frame with side coding */
    if( decControl->nChannelsInternal == 2 && decode_only_middle == 0 && psDec->prev_decode_only_middle == 1 ) {
        memset((char*)&(psDec->channel_state[ 1 ].outBuf), (0), (((int)((psDec->channel_state[ 1 ].outBuf) ' size  ))));
        memset((char*)&(psDec->channel_state[ 1 ].sLPC_Q14_buf), (0), (((int)((psDec->channel_state[ 1 ].sLPC_Q14_buf) ' size  ))));
        psDec->channel_state[ 1 ].lagPrev        = 100;
        psDec->channel_state[ 1 ].LastGainIndex  = 10;
        psDec->channel_state[ 1 ].prevSignalType = 0;
        psDec->channel_state[ 1 ].first_frame_after_reset = 1;
    }

    if( lostFlag == 0 ) {
        has_side = (int)(decode_only_middle==0);
    } else {
        has_side = (int)((psDec->prev_decode_only_middle==0)
                      || (decControl->nChannelsInternal == 2 && lostFlag == 2 && channel_state[1].LBRR_flags[ channel_state[1].nFramesDecoded ] == 1 ));
    }
    /* Call decoder for one frame */
    for( n = 0; n < decControl->nChannelsInternal; n++ ) {
        if( n == 0 || has_side!=0 ) {
            int FrameIndex;
            int condCoding;

            FrameIndex = channel_state[ 0 ].nFramesDecoded - n;
            /* Use independent coding if no previous frame available */
            if( FrameIndex <= 0 ) {
                condCoding = 0;
            } else if( lostFlag == 2 ) {
                condCoding = channel_state[ n ].LBRR_flags[ FrameIndex - 1 ]!=0 ? 2 : 0;
            } else if( n > 0 && psDec->prev_decode_only_middle!=0 ) {
                /* If we skipped a side frame in this packet, we don't
                   need LTP scaling; the LTP state is well-defined. */
                condCoding = 1;
            } else {
                condCoding = 2;
            }
            ret += silk_decode_frame( &channel_state[ n ], psRangeDec, &samplesOut1_tmp[ n ][ 2 ], &nSamplesOutDec, lostFlag, condCoding);
        } else {
            memset((char*)(&samplesOut1_tmp[ n ][ 2 ]), (0), (nSamplesOutDec * ((int)(opus_int16 ' size  ))));
        }
        channel_state[ n ].nFramesDecoded++;
    }

    if( decControl->nChannelsAPI == 2 && decControl->nChannelsInternal == 2 ) {
        /* Convert Mid/Side to Left/Right */
        silk_stereo_MS_to_LR( &psDec->sStereo, &samplesOut1_tmp[ 0 ], &samplesOut1_tmp[ 1 ], &MS_pred_Q13, channel_state[ 0 ].fs_kHz, nSamplesOutDec );
    } else {
        /* Buffering */
        memcpy((byte*)&(samplesOut1_tmp[ 0 ]), (byte*)&(psDec->sStereo.sMid), (2 * ((int)(opus_int16 ' size  ))));
        memcpy((byte*)&(psDec->sStereo.sMid), (byte*)(&samplesOut1_tmp[ 0 ][ nSamplesOutDec ]), (2 * ((int)(opus_int16 ' size  ))));
    }

    /* Number of output samples */
    *nSamplesOut = ((opus_int32)((nSamplesOutDec * decControl->API_sampleRate) / (((opus_int32)((opus_int16)(channel_state[ 0 ].fs_kHz)) * (opus_int32)((opus_int16)(1000))))));

    /* Set up pointers to temp buffers */
    if( decControl->nChannelsAPI == 2 ) {
        resample_out_ptr = &samplesOut2_tmp;
    } else {
        resample_out_ptr = samplesOut;
    }

    for( n = 0; n < (((decControl->nChannelsAPI) < (decControl->nChannelsInternal)) ? (decControl->nChannelsAPI) : (decControl->nChannelsInternal)); n++ ) {

        /* Resample decoded signal to API_sampleRate */
        ret += silk_resampler( &channel_state[ n ].resampler_state, resample_out_ptr, &samplesOut1_tmp[ n ][ 1 ], nSamplesOutDec );

        /* Interleave if stereo output and stereo stream */
        if( decControl->nChannelsAPI == 2 ) {
            for( i = 0; i < *nSamplesOut; i++ ) {
                samplesOut[ n + 2 * i ] = resample_out_ptr[ i ];
            }
        }
    }

    /* Create two channel output from mono stream */
    if( decControl->nChannelsAPI == 2 && decControl->nChannelsInternal == 1 ) {
        if ( stereo_to_mono!=0 ){
            /* Resample right channel for newly collapsed stereo just in case
               we weren't doing collapsing when switching to mono */
            ret += silk_resampler( &channel_state[ 1 ].resampler_state, resample_out_ptr, &samplesOut1_tmp[ 0 ][ 1 ], nSamplesOutDec );

            for( i = 0; i < *nSamplesOut; i++ ) {
                samplesOut[ 1 + 2 * i ] = resample_out_ptr[ i ];
            }
        } else {
            for( i = 0; i < *nSamplesOut; i++ ) {
                samplesOut[ 1 + 2 * i ] = samplesOut[ 0 + 2 * i ];
            }
        }
    }

    /* Export pitch lag, measured at 48 kHz sampling rate */
    if( channel_state[ 0 ].prevSignalType == 2 ) {
        int mult_tab[ 3 ] = { 6, 4, 3 };
        decControl->prevPitchLag = channel_state[ 0 ].lagPrev * mult_tab[ ( channel_state[ 0 ].fs_kHz - 8 ) >> 2 ];
    } else {
        decControl->prevPitchLag = 0;
    }

    if( lostFlag == 1 ) {
       /* On packet loss, remove the gain clamping to prevent having the energy "bounce back"
          if we lose packets when the energy is going down */
       for ( i = 0; i < psDec->nChannelsInternal; i++ )
          psDec->channel_state[ i ].LastGainIndex = 10;
    } else {
       psDec->prev_decode_only_middle = decode_only_middle;
    }
    return ret;
}

#end unsafe
