#begin unsafe
/* Copyright (c) 2011 Xiph.Org Foundation
   Written by Jean-Marc Valin */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file opus_multistream.h
 * @brief Opus reference implementation multistream API
 */


use opus_types;

struct ChannelLayout {
   int nb_channels;
   int nb_streams;
   int nb_coupled_streams;
   byte      mapping[256];
}

struct OpusMSEncoder
{
   ChannelLayout layout;
   int bitrate;
   /* Encoder states go here */
}

struct OpusMSDecoder
{
   ChannelLayout layout;
   /* Decoder states go here */
}


/** Allocate and initialize a multistream encoder state object.
 *  Call opus_multistream_encoder_destroy() to release
 *  this object when finished. */
  OpusMSEncoder *opus_multistream_encoder_create(
      opus_int32 Fs,            /**< Sampling rate of input signal (Hz) */
      int channels,             /**< Number of channels in the input signal */
      int streams,              /**< Total number of streams to encode from the input */
      int coupled_streams,      /**< Number of coupled (stereo) streams to encode */
                byte      *mapping, /**< Encoded mapping between channels and streams */
      int application,          /**< Coding mode (OPUS_APPLICATION_VOIP/OPUS_APPLICATION_AUDIO) */
      int *error                /**< Error code */
) ;

/** Initialize an already allocated multistream encoder state. */
 int opus_multistream_encoder_init(
      OpusMSEncoder *st,        /**< Encoder state */
      opus_int32 Fs,            /**< Sampling rate of input signal (Hz) */
      int channels,             /**< Number of channels in the input signal */
      int streams,              /**< Total number of streams to encode from the input */
      int coupled_streams,      /**< Number of coupled (stereo) streams to encode */
                byte      *mapping, /**< Encoded mapping between channels and streams */
      int application           /**< Coding mode (OPUS_APPLICATION_VOIP/OPUS_APPLICATION_AUDIO) */
)  ;

/** Returns length of the data payload (in bytes) or a negative error code */
  int opus_multistream_encode(
    OpusMSEncoder *st,          /**< Encoder state */
          opus_int16 *pcm,      /**< Input signal as interleaved samples. Length is frame_size*channels */
    int frame_size,             /**< Number of samples per frame of input signal */
        byte      *data,        /**< Output buffer for the compressed payload (no more than max_data_bytes long) */
    opus_int32 max_data_bytes   /**< Allocated memory for payload; don't use for controlling bitrate */
)   ;

/** Returns length of the data payload (in bytes) or a negative error code. */
  int opus_multistream_encode_float(
      OpusMSEncoder *st,        /**< Encoder state */
            float *pcm,         /**< Input signal interleaved in channel order. length is frame_size*channels */
      int frame_size,           /**< Number of samples per frame of input signal */
          byte      *data,      /**< Output buffer for the compressed payload (no more than max_data_bytes long) */
      opus_int32 max_data_bytes /**< Allocated memory for payload; don't use for controlling bitrate */
)   ;

/** Gets the size of an OpusMSEncoder structure.
  * @returns size
  */
  opus_int32 opus_multistream_encoder_get_size(
      int nb_streams,              /**< Total number of coded streams */
      int nb_coupled_streams       /**< Number of coupled (stereo) streams */
);


/** Deallocate a multstream encoder state */
 void opus_multistream_encoder_destroy(OpusMSEncoder *st);

/** Get or set options on a multistream encoder state */
 int opus_multistream_encoder_ctl(OpusMSEncoder *st, int request, byte[] value0);

/** Allocate and initialize a multistream decoder state object.
 *  Call opus_multistream_decoder_destroy() to release
 *  this object when finished. */
  OpusMSDecoder *opus_multistream_decoder_create(
      opus_int32 Fs,            /**< Sampling rate to decode at (Hz) */
      int channels,             /**< Number of channels to decode */
      int streams,              /**< Total number of coded streams in the multistream */
      int coupled_streams,      /**< Number of coupled (stereo) streams in the multistream */
                byte      *mapping, /**< Stream to channel mapping table */
      int *error                /**< Error code */
) ;

/** Intialize a previously allocated decoder state object. */
 int opus_multistream_decoder_init(
      OpusMSDecoder *st,        /**< Encoder state */
      opus_int32 Fs,            /**< Sample rate of input signal (Hz) */
      int channels,             /**< Number of channels in the input signal */
      int streams,              /**< Total number of coded streams */
      int coupled_streams,      /**< Number of coupled (stereo) streams */
                byte      *mapping  /**< Stream to channel mapping table */
)  ;

/** Returns the number of samples decoded or a negative error code */
  int opus_multistream_decode(
    OpusMSDecoder *st,          /**< Decoder state */
              byte      *data,  /**< Input payload. Use a NULL pointer to indicate packet loss */
    opus_int32 len,             /**< Number of bytes in payload */
    opus_int16 *pcm,            /**< Output signal, samples interleaved in channel order . length is frame_size*channels */
    int frame_size,             /**< Number of samples per frame of input signal */
    int decode_fec              /**< Flag (0/1) to request that any in-band forward error correction data be */
                                /**< decoded. If no such data is available the frame is decoded as if it were lost. */
)  ;

/** Returns the number of samples decoded or a negative error code */
  int opus_multistream_decode_float(
    OpusMSDecoder *st,          /**< Decoder state */
              byte      *data,  /**< Input payload buffer. Use a NULL pointer to indicate packet loss */
    opus_int32 len,             /**< Number of payload bytes in data */
    float *pcm,                 /**< Buffer for the output signal (interleaved iin channel order). length is frame_size*channels */
    int frame_size,             /**< Number of samples per frame of input signal */
    int decode_fec              /**< Flag (0/1) to request that any in-band forward error correction data be */
                                /**< decoded. If no such data is available the frame is decoded as if it were lost. */
)  ;

/** Gets the size of an OpusMSDecoder structure.
  * @returns size
  */
  opus_int32 opus_multistream_decoder_get_size(
      int nb_streams,              /**< Total number of coded streams */
      int nb_coupled_streams       /**< Number of coupled (stereo) streams */
);


/** Get or set options on a multistream decoder state */
 int opus_multistream_decoder_ctl(OpusMSDecoder *st, int request, byte[] value0) ;

/** Deallocate a multistream decoder state object */
 void opus_multistream_decoder_destroy(OpusMSDecoder *st);


#end unsafe
