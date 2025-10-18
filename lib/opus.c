
// opus.c

use opus/opus_encoder, opus/opus_decoder, opus/opus_defines;

#begin unsafe

//-------------------------------------------------------------------------------------

struct AUDIO_ENCODER
{
  OpusEncoder*  enc;
}

struct AUDIO_DECODER
{
  OpusDecoder*  dec;
}

//-------------------------------------------------------------------------------------

public
void open_audio_encoder (out AUDIO_ENCODER enc,
                             int           sampling_rate,  // 48000, 24000, 16000, 12000 or 8000.
                             int           nb_channels,    // 1 or 2
                             AUDIO_TYPE    type)           // above 20_000 bit/s, use MUSIC
{
  int error;
  clear enc;
  enc.enc = opus_encoder_create (sampling_rate,
                                 nb_channels,
                                 type==SPEECH ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO,
                                 &error);
  assert enc.enc != null;

  opus_encoder_ctl(enc.enc, OPUS_SET_COMPLEXITY_REQUEST, (10));   // all options
  opus_encoder_ctl(enc.enc, OPUS_SET_SIGNAL_REQUEST, OPUS_AUTO);
}

//-------------------------------------------------------------------------------------

public
void close_audio_encoder (AUDIO_ENCODER enc)
{
  opus_encoder_destroy (enc.enc);
}

//-------------------------------------------------------------------------------------

public
void set_bitrate (AUDIO_ENCODER enc,
                  int           bitrate)  // quality, between 8_000 to 192_000 bit/s
{
  opus_encoder_ctl(enc.enc, OPUS_SET_BITRATE_REQUEST, bitrate);
}

//-------------------------------------------------------------------------------------

public
int encode_audio (    AUDIO_ENCODER enc,
                      byte[]        pcm,
                  out byte[]        output)
{
  // returns the number of bytes actually written to the packet.
  // The return value can be negative, which indicates that an error has occurred.
  // If the return value is 1 byte, then the packet does not need to be transmitted (DTX).
  
  return opus_encode (enc.enc,
                      (short *)&pcm,
                      (int)pcm'size >> encoder_channels(*enc.enc),    // nb_samples of this opus frame
                      &output,
                      (int)output'size);
}

//-------------------------------------------------------------------------------------

public
void open_audio_decoder (out AUDIO_DECODER dec,
                             int           sampling_rate,  // 48000, 24000, 16000, 12000 or 8000.
                             int           nb_channels)    // 1 or 2
{
  int error;
  clear dec;
  dec.dec = opus_decoder_create (sampling_rate,
                                 nb_channels,       // 1=mono or 2=stereo
                                 &error);
  assert dec.dec != null;
}

//-------------------------------------------------------------------------------------

public
void close_audio_decoder (AUDIO_DECODER dec)
{
  opus_decoder_destroy (dec.dec);
}

//-------------------------------------------------------------------------------------

public
int decode_audio (    AUDIO_DECODER dec,
                      byte[]        input,
                  out byte[]        pcm)
{
  int rc;

  // Lost packets can be replaced with loss concealment by calling the decoder
  // with a null pointer and zero length for the missing packet.

  // return the number of samples (per channel) decoded from the packet.
  // if that value is negative, then an error has occured.
  // This can occur if the packet is corrupted or if the audio buffer is too small
  // to hold the decoded audio.

  rc = opus_decode (dec.dec,
                      &input,
                      input'length,       // nb bytes to decode
                      (short *)&pcm,
                      (int)pcm'size >> decoder_channels(*dec.dec),    // nb_samples of this opus frame
                      0);
  if (rc < 0)
    return -1;
  return rc << decoder_channels(*dec.dec);
}

//-------------------------------------------------------------------------------------
#end unsafe
//-------------------------------------------------------------------------------------
