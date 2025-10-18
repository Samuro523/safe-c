#begin unsafe
/* Copyright (c) 2010-2011 Xiph.Org Foundation, Skype Limited
   Written by Jean-Marc Valin and Koen Vos */
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
 * @file opus.h
 * @brief Opus reference implementation API
 */

use opus_types, arch;

/**
 * @mainpage Opus
 *
 * The Opus codec is designed for interactive speech and audio transmission over the Internet.
 * It is designed by the IETF Codec Working Group and incorporates technology from
 * Skype's SILK codec and Xiph.Org's CELT codec.
 *
 * The Opus codec is designed to handle a wide range of interactive audio applications,
 * including Voice over IP, videoconferencing, in-game chat, and even remote live music
 * performances. It can scale from low bit-rate narrowband speech to very high quality
 * stereo music. Its main features are:

 * @li Sampling rates from 8 to 48 kHz
 * @li Bit-rates from 6 kb/s to 510 kb/s
 * @li Support for both      ant bit-rate (CBR) and variable bit-rate (VBR)
 * @li Audio bandwidth from narrowband to full-band
 * @li Support for speech and music
 * @li Support for mono and stereo
 * @li Support for multichannel (up to 255 channels)
 * @li Frame sizes from 2.5 ms to 60 ms
 * @li Good loss robustness and packet loss concealment (PLC)
 * @li Floating point and fixed-point implementation
 *
 * Documentation sections:
 * @li @ref opus_encoder
 * @li @ref opus_decoder
 * @li @ref opus_repacketizer
 * @li @ref opus_libinfo
 * @li @ref opus_custom
 */

struct OpusEncoder;

int encoder_channels (OpusEncoder e);

/** @defgroup opus_encoder Opus Encoder
  * @{
  *
  * @brief This page describes the process and functions used to encode Opus.
  *
  * Since Opus is a stateful codec, the encoding process starts with creating an encoder
  * state. This can be done with:
  *
  * @code
  * int          error;
  * OpusEncoder *enc;
  * enc = opus_encoder_create(Fs, channels, application, &error);
  * @endcode
  *
  * From this point, @c enc can be used for encoding an audio stream. An encoder state
  * @b must @b not be used for more than one stream at the same time. Similarly, the encoder
  * state @b must @b not be re-initialized for each frame.
  *
  * While opus_encoder_create() allocates memory for the state, it's also possible
  * to initialize pre-allocated memory:
  *
  * @code
  * int          size;
  * int          error;
  * OpusEncoder *enc;
  * size = opus_encoder_get_size(channels);
  * enc = malloc(size);
  * error = opus_encoder_init(enc, Fs, channels, application);
  * @endcode
  *
  * where opus_encoder_get_size() returns the required size for the encoder state. Note that
  * future versions of this code may change the size, so no assuptions should be made about it.
  *
  * The encoder state is always continuous in memory and only a shallow copy is sufficient
  * to copy it (e.g. memcpy())
  *
  * It is possible to change some of the encoder's settings using the opus_encoder_ctl()
  * interface. All these settings already default to the recommended value, so they should
  * only be changed when necessary. The most common settings one may want to change are:
  *
  * @code
  * opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate));
  * opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
  * opus_encoder_ctl(enc, OPUS_SET_SIGNAL(signal_type));
  * @endcode
  *
  * where
  *
  * @arg bitrate is in bits per second (b/s)
  * @arg complexity is a value from 1 to 10, where 1 is the lowest complexity and 10 is the highest
  * @arg signal_type is either OPUS_AUTO (default), OPUS_SIGNAL_VOICE, or OPUS_SIGNAL_MUSIC
  *
  * See @ref opus_encoderctls and @ref opus_genericctls for a complete list of parameters that can be set or queried. Most parameters can be set or changed at any time during a stream.
  *
  * To encode a frame, opus_encode() or opus_encode_float() must be called with exactly one frame (2.5, 5, 10, 20, 40 or 60 ms) of audio data:
  * @code
  * len = opus_encode(enc, audio_frame, frame_size, packet, max_packet);
  * @endcode
  *
  * where
  * <ul>
  * <li>audio_frame is the audio data in opus_int16 (or float for opus_encode_float())</li>
  * <li>frame_size is the duration of the frame in samples (per channel)</li>
  * <li>packet is the byte array to which the compressed data is written</li>
  * <li>max_packet is the maximum number of bytes that can be written in the packet (4000 bytes is recommended)</li>
  * </ul>
  *
  * opus_encode() and opus_encode_frame() return the number of bytes actually written to the packet.
  * The return value <b>can be negative</b>, which indicates that an error has occurred. If the return value
  * is 1 byte, then the packet does not need to be transmitted (DTX).
  *
  * Once the encoder state if no longer needed, it can be destroyed with
  *
  * @code
  * opus_encoder_destroy(enc);
  * @endcode
  *
  * If the encoder was created with opus_encoder_init() rather than opus_encoder_create(),
  * then no action is required aside from potentially freeing the memory that was manually
  * allocated for it (calling free(enc) for the example above)
  *
  */

/** Opus encoder state.
  * This contains the complete state of an Opus encoder.
  * It is position independent and can be freely copied.
  * @see opus_encoder_create,opus_encoder_init
  */

  int opus_encoder_get_size(int channels);

/**
 */

/** Allocates and initializes an encoder state.
 * There are three coding modes:
 *
 * @ref OPUS_APPLICATION_VOIP gives best quality at a given bitrate for voice
 *    signals. It enhances the  input signal by high-pass filtering and
 *    emphasizing formants and harmonics. Optionally  it includes in-band
 *    forward error correction to protect against packet loss. Use this
 *    mode for typical VoIP applications. Because of the enhancement,
 *    even at high bitrates the output may sound different from the input.
 *
 * @ref OPUS_APPLICATION_AUDIO gives best quality at a given bitrate for most
 *    non-voice signals like music. Use this mode for music and mixed
 *    (music/voice) content, broadcast, and applications requiring less
 *    than 15 ms of coding delay.
 *
 * @ref OPUS_APPLICATION_RESTRICTED_LOWDELAY configures low-delay mode that
 *    disables the speech-optimized mode in exchange for slightly reduced delay.
 *    This mode can only be set on an newly initialized or freshly reset encoder
 *    because it changes the codec delay.
 *
 * This is useful when the caller knows that the speech-optimized modes will not be needed (use with caution).
 * @param [in] Fs <tt>opus_int32</tt>: Sampling rate of input signal (Hz)
 * @param [in] channels <tt>int</tt>: Number of channels (1/2) in input signal
 * @param [in] application <tt>int</tt>: Coding mode (@ref OPUS_APPLICATION_VOIP/@ref OPUS_APPLICATION_AUDIO/@ref OPUS_APPLICATION_RESTRICTED_LOWDELAY)
 * @param [out] error <tt>int*</tt>: @ref opus_errorcodes
 * @note Regardless of the sampling rate and number channels selected, the Opus encoder
 * can switch to a lower audio audio bandwidth or number of channels if the bitrate
 * selected is too low. This also means that it is safe to always use 48 kHz stereo input
 * and let the encoder optimize the encoding.
 */
  OpusEncoder *opus_encoder_create(
    opus_int32 Fs,
    int channels,
    int application,
    int *error
);

/** Initializes a previously allocated encoder state
  * The memory pointed to by st must be the size returned by opus_encoder_get_size.
  * This is intended for applications which use their own allocator instead of malloc.
  * @see opus_encoder_create(),opus_encoder_get_size()
  * To reset a previously initialized state use the OPUS_RESET_STATE CTL.
  * @param [in] st <tt>OpusEncoder*</tt>: Encoder state
  * @param [in] Fs <tt>opus_int32</tt>: Sampling rate of input signal (Hz)
  * @param [in] channels <tt>int</tt>: Number of channels (1/2) in input signal
  * @param [in] application <tt>int</tt>: Coding mode (OPUS_APPLICATION_VOIP/OPUS_APPLICATION_AUDIO/OPUS_APPLICATION_RESTRICTED_LOWDELAY)
  * @retval OPUS_OK Success or @ref opus_errorcodes
  */
 int opus_encoder_init(
    OpusEncoder *st,
    opus_int32 Fs,
    int channels,
    int application
) ;

/** Encodes an Opus frame.
  * The passed frame_size must an opus frame size for the encoder's sampling rate.
  * For example, at 48kHz the permitted values are 120, 240, 480, 960, 1920, and 2880.
  * Passing in a duration of less than 10ms (480 samples at 48kHz) will
  * prevent the encoder from using the LPC or hybrid modes.
  * @param [in] st <tt>OpusEncoder*</tt>: Encoder state
  * @param [in] pcm <tt>opus_int16*</tt>: Input signal (interleaved if 2 channels). length is frame_size*channels*sizeof(opus_int16)
  * @param [in] frame_size <tt>int</tt>: Number of samples per frame of input signal
  * @param [out] data <tt>char*</tt>: Output payload (at least max_data_bytes long)
  * @param [in] max_data_bytes <tt>opus_int32</tt>: Allocated memory for payload; don't use for controlling bitrate
  * @returns length of the data payload (in bytes) or @ref opus_errorcodes
  */
  opus_int32 opus_encode(
    OpusEncoder *st,
          opus_int16 *pcm,
    int frame_size,
        byte      *data,
    opus_int32 max_data_bytes
)   ;

/** Encodes an Opus frame from floating point input.
  * The passed frame_size must an opus frame size for the encoder's sampling rate.
  * For example, at 48kHz the permitted values are 120, 240, 480, 960, 1920, and 2880.
  * Passing in a duration of less than 10ms (480 samples at 48kHz) will
  * prevent the encoder from using the LPC or hybrid modes.
  * @param [in] st <tt>OpusEncoder*</tt>: Encoder state
  * @param [in] pcm <tt>float*</tt>: Input in float format (interleaved if 2 channels), with a normal range of +/-1.0.
  *          Samples with a range beyond +/-1.0 are supported but will
  *          be clipped by decoders using the integer API and should
  *          only be used if it is known that the far end supports
  *          extended dynamic range.
  *          length is frame_size*channels*sizeof(float)
  * @param [in] frame_size <tt>int</tt>: Number of samples per frame of input signal
  * @param [out] data <tt>char*</tt>: Output payload (at least max_data_bytes long)
  * @param [in] max_data_bytes <tt>opus_int32</tt>: Allocated memory for payload; don't use for controlling bitrate
  * @returns length of the data payload (in bytes) or @ref opus_errorcodes
  */
  
opus_int32 opus_encode_float (OpusEncoder *st,       opus_val16 *pcm, int frame_size,
                               byte      *data, opus_int32 out_data_bytes);

/** Frees an OpusEncoder allocated by opus_encoder_create.
  * @param[in] st <tt>OpusEncoder*</tt>: State to be freed.
  */
 void opus_encoder_destroy(OpusEncoder *st);

/** Perform a CTL function on an Opus encoder.
  *
  * Generally the request and subsequent arguments are generated
  * by a convenience macro.
  * @see opus_encoderctls
  */
 int opus_encoder_ctl(OpusEncoder *st, int request, byte[] value0);
/**@}*/


#end unsafe
