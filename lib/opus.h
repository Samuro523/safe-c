
// opus.h : opus audio codec  --  more info on http://www.opus-codec.org/

//======================================================================================
//====================================== ENCODE ========================================
//======================================================================================

struct AUDIO_ENCODER;

enum AUDIO_TYPE {SPEECH, MUSIC};

//--------------------------------------------------------------------------------------

void open_audio_encoder (out AUDIO_ENCODER enc,
                             int           sampling_rate,  // 48000, 24000, 16000, 12000 or 8000.
                             int           nb_channels,    // 1 or 2
                             AUDIO_TYPE    type);          // above 20_000 bit/s, use MUSIC

//--------------------------------------------------------------------------------------

void set_bitrate (AUDIO_ENCODER enc,
                  int           bitrate);  // quality, between 8_000 to 192_000 bit/s

//--------------------------------------------------------------------------------------

// for sampling rate 48000, 16-bit, stereo (187,5 Kbytes/s),
// pcm must have one of the following sizes : 480, 960, 1920, 3840, 7680 or 11520 bytes.
// a pcm of   480 bytes will require 400 calls/sec.
// a pcm of   960 bytes will require 200 calls/sec.
// a pcm of  1920 bytes will require 100 calls/sec.
// a pcm of  3840 bytes will require  50 calls/sec.
// a pcm of  7680 bytes will require  25 calls/sec.
// a pcm of 11520 bytes will require  16,666 calls/sec.
// returns nb of compressed bytes in output, or a negative value if error.

int encode_audio (    AUDIO_ENCODER enc,
                      byte[]        pcm,
                  out byte[]        output);

//--------------------------------------------------------------------------------------

void close_audio_encoder (AUDIO_ENCODER enc);

//--------------------------------------------------------------------------------------

//======================================================================================
//====================================== DECODE ========================================
//======================================================================================

struct AUDIO_DECODER;

//--------------------------------------------------------------------------------------

void open_audio_decoder (out AUDIO_DECODER dec,
                             int           sampling_rate,  // 48000, 24000, 16000, 12000 or 8000.
                             int           nb_channels);   // 1 or 2

//--------------------------------------------------------------------------------------

// an input of 0 bytes can be used to indicate a lost frame.
// for sampling rate 48000, 16-bit, stereo (187,5 Kbytes/s),
// pcm must have one of the following sizes : 480, 960, 1920, 3840, 7680 or 11520 bytes.
// a pcm of   480 bytes will require 400 calls/sec.
// a pcm of   960 bytes will require 200 calls/sec.
// a pcm of  1920 bytes will require 100 calls/sec.
// a pcm of  3840 bytes will require  50 calls/sec.
// a pcm of  7680 bytes will require  25 calls/sec.
// a pcm of 11520 bytes will require  16,666 calls/sec.
// pcm will contain decoded 48 kHz, 16-bit pcm audio data (187,5 Kbytes/s).
// returns nb of pcm output bytes, or a negative value if error.

int decode_audio (    AUDIO_DECODER dec,
                      byte[]        input,
                  out byte[]        pcm);

//--------------------------------------------------------------------------------------

void close_audio_decoder (AUDIO_DECODER dec);

//--------------------------------------------------------------------------------------
