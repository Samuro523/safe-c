
// sound : microphone and speaker

//-----------------------------------------------------------------------------------

typedef uint DEVICE;
const DEVICE DEFAULT_DEVICE = uint'max;

struct FORMAT_SOUND  // size of PCM sound data per second = sample_size * channels * frequency
{
  int sample_size;   // 1 = 8-bit (0 to 255), 2 = 16-bit (-32768 to 32767)
  int channels;      // 1 = mono, 2 = stereo
  int frequency;     // nb samples per second (8000, 11025, 22050, 44100, 48000)
}

typedef wstring(32) AUDIO_NAME;

//-----------------------------------------------------------------------------------

package Microphone

  struct MICROPHONE;

  // get list of all microphones, from 0 to number_of_microphone_devices-1
  // returns number of microphone devices.
  int get_microphone_list (out AUDIO_NAME[] list);

  // open microphone, set recording format and preallocate enough intern buffers
  // for 'buffer_seconds' of data.
  // the function aborts in case of illegal values or if micro is already open.
  void open_micro (out MICROPHONE microphone, FORMAT_SOUND format, DEVICE device = DEFAULT_DEVICE, int buffer_seconds = 1);

  // returns 0 if OK, -1 if error (no microphone available)
  // the function aborts if micro is not open, or if recording is already in progress.
  int micro_start_recording (ref MICROPHONE microphone);

  // the function aborts if micro is not open or if recording was not started.
  void micro_stop_recording (ref MICROPHONE microphone);

  // buffer'length must be a multiple of 4.
  // returns nb of bytes recorded in buffer, or 0 if no further data is available.
  int micro_get_buffer (ref MICROPHONE microphone, out byte[] buffer);

  // stop recording, deallocate all intern buffers, and close microphone.
  // the function aborts if micro is not open.
  void close_micro (ref MICROPHONE microphone);

end Microphone;

//-----------------------------------------------------------------------------------

package Speaker

  struct SPEAKER;

  // get list of all speakers, from 0 to number_of_speaker_devices-1
  // returns number of speaker devices.
  int get_speaker_list (out AUDIO_NAME[] list);

  // open speaker, set format and preallocate enough intern buffers
  // for 'buffer_seconds' of data.
  // the function aborts in case of illegal values or if speaker is already open.
  void open_speaker (out SPEAKER speaker, FORMAT_SOUND format, DEVICE device = DEFAULT_DEVICE, int buffer_seconds = 1);

  // buffer'length must be a multiple of 4.
  // returns nb of buffer bytes sent, this can be less than buffer'length
  // when intern buffers are full or an i/o error occured.
  // the function aborts if speaker is not open.
  int speaker_put_buffer (ref SPEAKER speaker, byte[] buffer);

  // pause the speaker (sent buffers will be buffered)
  // the function aborts if speaker is not open.
  void speaker_pause (ref SPEAKER speaker, bool pause);       // true = pause, false = continue

  // stop speaker, deallocate all intern buffers and close speaker.
  // the function aborts if speaker is not open.
  void close_speaker (ref SPEAKER speaker);

end Speaker;

//-----------------------------------------------------------------------------------
