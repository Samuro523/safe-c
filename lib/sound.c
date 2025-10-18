
// sound.c

#if WINDOWS


// sound.c : microphone and speaker

use thread, tracing, win/windows;

const bool debug = false;

#begin unsafe

//------------------------------------------------------------------------------

bool is_valid (FORMAT_SOUND f)
{
  if (f.sample_size < 1 || f.sample_size > 2)
    return false;
  if (f.channels < 1 || f.channels > 2)
    return false;
  if (f.frequency < 8000 || f.frequency > 48000)
    return false;
  return true;
}

//------------------------------------------------------------------------------
// microphone
//------------------------------------------------------------------------------

package body Microphone

  const int BUFFERS_PER_SEC = 25;   // has an effect on latency

  SHARED_OBJECT g_so_micro;

  struct MICROPHONE
  {
    FORMAT_SOUND  f;
    HWAVEIN       wi;        // handle (0 = no micro active)
    DEVICE        device;
    int           single_buffer_size;
    int           nb_buffers;
    byte[]^       buffers;   // null if micro closed
    WAVEHDR[]^    hdr;       // null if micro closed
    bool          recording; // true = recording is in progress
    int           index;     // index of next buffer to read
    uint          offset;    // offset within buffer
  }

  // get list of all microphones, from 0 to number_of_microphone_devices-1
  // returns number of microphone devices.

  public int get_microphone_list (out AUDIO_NAME[] list)
  {
    int count = (int)waveInGetNumDevs();
    int i;

    clear list;
    if (count > list'length)
      count = list'length;

    for (i=0; i<count; i++)
    {
      WAVEINCAPSW cap;
      if (waveInGetDevCapsW (uDeviceID => (UINT)i, &cap, cap'size) == 0)
        list[i] = cap.szPname;
    }

    return count;
  }

  // open microphone, set recording format and preallocate enough intern buffers
  // for 'buffer_seconds' of data.
  // the function aborts in case of illegal values or if micro is already open.
  public void open_micro (out MICROPHONE microphone, FORMAT_SOUND format, DEVICE device = DEFAULT_DEVICE, int buffer_seconds = 1)
  {
    int bytes_per_sec;

    enter_shared_object (ref g_so_micro);

if (debug) trace ("BEGIN open_micro()\n");

    clear microphone;

    assert is_valid(format) && buffer_seconds > 0;

    microphone.f = format;
    microphone.device = device;

    bytes_per_sec = microphone.f.sample_size * microphone.f.channels * microphone.f.frequency;

    microphone.single_buffer_size = (bytes_per_sec / BUFFERS_PER_SEC) & (-64);   // make buffers mult. of 64
    microphone.nb_buffers = 1 + BUFFERS_PER_SEC * buffer_seconds;

    microphone.buffers = new byte [microphone.single_buffer_size * microphone.nb_buffers];
    microphone.hdr     = new WAVEHDR [microphone.nb_buffers];

if (debug) trace ("END open_micro()\n");

    leave_shared_object (ref g_so_micro);
  }

  //---------------------------------------------------------------------------------

  // returns 0 if OK, -1 if error (no microphone available)
  // the function aborts if micro is not open, or if recording is already in progress.

  public int micro_start_recording (ref MICROPHONE microphone)
  {
    WAVEFORMATEX format;
    uint         ret;
    int          i;

    enter_shared_object (ref g_so_micro);

if (debug) trace ("BEGIN micro_start_recording()\n");

    assert microphone.buffers != null && !microphone.recording;

    if (microphone.wi == 0)     // handle was closed
    {
      // open new handle

      clear format;
      format.wFormatTag      = WAVE_FORMAT_PCM;
      format.nChannels       = (WORD)microphone.f.channels;
      format.nSamplesPerSec  = (DWORD)microphone.f.frequency;
      format.nBlockAlign     = (WORD)(microphone.f.channels * microphone.f.sample_size);
      format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
      format.wBitsPerSample  = (WORD)(microphone.f.sample_size << 3);

if (debug) trace ("waveInOpen()\n");

      ret = waveInOpen (&microphone.wi, microphone.device, format, 0, 0, 0);
      if (ret != 0)
      {
        microphone.wi = 0;
        trace ("error: micro_start_recording() : waveInOpen() returned %u\n", ret);
        leave_shared_object (ref g_so_micro);
        return -1;
      }

      // prepare all buffers

if (debug) trace ("prepare all buffers\n");

      clear microphone.hdr^;

      for (i=0; i<microphone.nb_buffers; i++)
      {
        microphone.hdr^[i].lpData         = &microphone.buffers^[i*microphone.single_buffer_size];
        microphone.hdr^[i].dwBufferLength = (uint)microphone.single_buffer_size;

        assert waveInPrepareHeader (microphone.wi, &microphone.hdr^[i], WAVEHDR'size) == 0;
        assert waveInAddBuffer     (microphone.wi, &microphone.hdr^[i], WAVEHDR'size) == 0;
      }

      microphone.index = 0;
      microphone.offset = 0;
    }

if (debug) trace ("waveInStart()\n");

    ret = waveInStart (microphone.wi);
    if (ret != 0)
    {
      trace ("error: micro_start_recording() : waveInStart() returned %u\n", ret);
      waveInReset (microphone.wi);
      waveInClose (microphone.wi);
      microphone.wi = 0;
      leave_shared_object (ref g_so_micro);
      return -1;
    }

if (debug) trace ("recording = true\n");

    microphone.recording = true;

if (debug) trace ("END micro_start_recording()\n");

    leave_shared_object (ref g_so_micro);

    return 0;
  }


  public void micro_stop_recording (ref MICROPHONE microphone)
  {
    enter_shared_object (ref g_so_micro);

if (debug) trace ("BEGIN micro_stop_recording()\n");

    assert microphone.buffers != null && microphone.recording;

    waveInStop (microphone.wi);
    microphone.recording = false;

if (debug) trace ("END micro_stop_recording()\n");

    leave_shared_object (ref g_so_micro);
  }


  void close_wi (ref MICROPHONE microphone)
  {
    int  i;

    if (microphone.wi == 0)
      return;

if (debug) trace ("close_wi()\n");

    (void)waveInReset (microphone.wi);

    for (i=0; i<microphone.nb_buffers; i++)
      assert waveInUnprepareHeader (microphone.wi, &microphone.hdr^[i], WAVEHDR'size) == 0;

    (void)waveInClose (microphone.wi);

    microphone.wi = 0;
  }


  // buffer'length must be a multiple of 4.
  // returns nb of bytes recorded in buffer, or 0 if no further data is available.

  public int micro_get_buffer (ref MICROPHONE microphone, out byte[] buffer)
  {
    uint  size;
    int   bytes_read;
    byte* dummy = &buffer;  // suppose buffer is initialized

    _unused dummy;

    enter_shared_object (ref g_so_micro);

if (debug) trace ("BEGIN micro_get_buffer()\n");

    if (microphone.buffers == null || microphone.wi == 0)
    {
      leave_shared_object (ref g_so_micro);
      return 0;
    }

    bytes_read = 0;

    for (;;)
    {
      if ((microphone.hdr^[microphone.index].dwFlags & WHDR_DONE) == 0)   // buffer not done
      {
        if (!microphone.recording)  // no more buffers expected
          close_wi (ref microphone);   // close wi handle & set it to 0, unprepare all buffers.
        break;
      }


      if (bytes_read == buffer'length)   // user buffer is full
        break;


      // buffer is done, copy it, or continue to copy it (from offset) to caller

if (debug) trace ("dwBytesRecorded=%u, offset=%u\n", microphone.hdr^[microphone.index].dwBytesRecorded, microphone.offset);

      size = microphone.hdr^[microphone.index].dwBytesRecorded - microphone.offset;
      if (size > buffer'size - (uint)bytes_read)
        size = buffer'size - (uint)bytes_read;

if (debug) trace ("copy %u bytes to user buffer\n", size);

      buffer[bytes_read:size] = microphone.hdr^[microphone.index].lpData[microphone.offset:size];
      microphone.offset += size;
      bytes_read += (int)size;


      if (microphone.offset == microphone.hdr^[microphone.index].dwBytesRecorded)    // all buffer given to caller : we can free it
      {
        assert waveInUnprepareHeader (microphone.wi, &microphone.hdr^[microphone.index], WAVEHDR'size) == 0;

if (debug) trace ("free and reuse micro buffer %d\n", microphone.index);

        clear microphone.hdr^[microphone.index];
        microphone.hdr^[microphone.index].lpData         = &microphone.buffers^[microphone.index*microphone.single_buffer_size];
        microphone.hdr^[microphone.index].dwBufferLength = (uint)microphone.single_buffer_size;

        assert waveInPrepareHeader (microphone.wi, &microphone.hdr^[microphone.index], WAVEHDR'size) == 0;
        assert waveInAddBuffer (microphone.wi, &microphone.hdr^[microphone.index], WAVEHDR'size) == 0;

        // and move to next buffer ...

        microphone.index++;
        if (microphone.index == microphone.nb_buffers)
          microphone.index = 0;
        microphone.offset = 0;
      }

    } // for

if (debug) trace ("END micro_get_buffer : %d bytes read\n", bytes_read);
if (debug) trace ("\n");

    leave_shared_object (ref g_so_micro);

    return bytes_read;
  }


  // stop recording, deallocate all intern buffers, and close microphone.
  // the function aborts if micro is not open.

  public void close_micro (ref MICROPHONE microphone)
  {
    enter_shared_object (ref g_so_micro);

if (debug) trace ("BEGIN close_micro\n");

    assert microphone.buffers != null;

    close_wi (ref microphone);
    microphone.recording = false;

    free microphone.buffers;
    free microphone.hdr;

    clear microphone;

if (debug) trace ("END close_micro\n");

    leave_shared_object (ref g_so_micro);
  }

end Microphone;

/*************************************************************************/

package body Speaker

  typedef WAVEHDR^ PWAVEHDR;

  struct SPEAKER
  {
    FORMAT_SOUND format;
    HWAVEOUT     wo;          // device handle (0 = no speaker active)
    DEVICE       device;
    byte[]^      huge_buffer; // null if speaker closed
    PWAVEHDR[]^  hdr;         // null if speaker closed

    // indexes into hdr
    int    hlast;      // index of first buffer to cleanup after play
    int    hfirst;     // index of next buffer to fill with data and play
    int    hused;      // slots used

    // index into huge_buffer
    int    blast;      // first used byte within huge_buffer
    int    bfirst;     // first free byte within huge_buffer
    int    bused;      // bytes used

    bool   paused;
  }

  SHARED_OBJECT g_so_speaker;


  // get list of all speakers, from 0 to number_of_speaker_devices-1
  // returns number of speaker devices.

  public int get_speaker_list (out AUDIO_NAME[] list)
  {
    int count = (int)waveOutGetNumDevs();
    int i;

    clear list;
    if (count > list'length)
      count = list'length;

    for (i=0; i<count; i++)
    {
      WAVEOUTCAPSW cap;
      if (waveOutGetDevCapsW (uDeviceID => (UINT)i, &cap, cap'size) == 0)
        list[i] = cap.szPname;
    }

    return count;
  }

  // open speaker, set format and preallocate enough intern buffers
  // for 'buffer_seconds' of data.
  // the function aborts in case of illegal values or if speaker is already open.

  public void open_speaker (out SPEAKER speaker, FORMAT_SOUND format, DEVICE device = DEFAULT_DEVICE, int buffer_seconds = 1)
  {
    int bytes_per_sec;

    enter_shared_object (ref g_so_speaker);

    clear speaker;

    assert is_valid(format) && buffer_seconds > 0;

    speaker.format = format;
    speaker.device = device;

    bytes_per_sec = speaker.format.sample_size * speaker.format.channels * speaker.format.frequency;   // about 160 K per second
    speaker.huge_buffer = new byte [bytes_per_sec * buffer_seconds];
    speaker.hdr = new PWAVEHDR [0];

    // indexes into hdr
    speaker.hlast  = 0;
    speaker.hfirst = 0;
    speaker.hused  = 0;

    // index into huge_buffer
    speaker.blast  = 0;
    speaker.bfirst = 0;
    speaker.bused  = 0;

    speaker.paused = false;

    leave_shared_object (ref g_so_speaker);
  }



  void release_old_buffers (ref SPEAKER speaker)
  {
    while (speaker.hused > 0)
    {
      ref WAVEHDR h = speaker.hdr^[speaker.hlast]^;
      int         rc, size;

      if ((h.dwFlags & WHDR_DONE) == 0)   // not yet done
        break;

      size = (int)h.dwBufferLength;

      rc = (int)waveOutUnprepareHeader (speaker.wo, &h, WAVEHDR'size);
      if (rc != 0)
      {
        trace ("error: waveOutUnprepareHeader() returned %d\n", rc);
        waveOutReset (speaker.wo);
      }

      speaker.bused -= size;
      speaker.blast += size;
      if (speaker.blast == speaker.huge_buffer^'length)
        speaker.blast = 0;
      speaker.hused--;
      speaker.hlast++;
      if (speaker.hlast == speaker.hdr^'length)
        speaker.hlast = 0;
    }
  }


  // buffer'length must be a multiple of 4.
  // returns nb of buffer bytes sent, this can be less than buffer'length
  // when intern buffers are full or if an i/o error occured.
  // the function aborts if speaker is not open.

  public int speaker_put_buffer (ref SPEAKER speaker, byte[] buffer)
  {
    WAVEFORMATEX format;
    uint         ret;
    int          bytes_written, ofs, ofs9, size;

    bytes_written = 0;

    enter_shared_object (ref g_so_speaker);

    assert speaker.huge_buffer != null;


    /**** cleanup all buffers that have already been played ****/

    release_old_buffers (ref speaker);


    /**** make sure audio is open if we have something to send ******/

    if (speaker.wo == 0 && buffer'length > 0)  // handle closed and something to speak
    {
      // open new handle

      clear format;
      format.wFormatTag      = WAVE_FORMAT_PCM;
      format.nChannels       = (WORD)speaker.format.channels;
      format.nSamplesPerSec  = (DWORD)speaker.format.frequency;
      format.nBlockAlign     = (WORD)(speaker.format.channels * speaker.format.sample_size);
      format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
      format.wBitsPerSample  = (WORD)(speaker.format.sample_size << 3);

      ret = waveOutOpen (&speaker.wo, speaker.device, &format, 0, 0, 0);
      if (ret != 0)
      {
        speaker.wo = 0;
        trace ("error: speaker_put_buffer() : waveOutOpen() returned %u\n", ret);
        leave_shared_object (ref g_so_speaker);
        return bytes_written;
      }

      if (speaker.paused)
        waveOutPause (speaker.wo);
    }


    /**** fill buffer ****/

    // compute size = available bytes in buffer
    size = speaker.huge_buffer^'length - speaker.bused;

    ofs = 0;
    ofs9 = buffer'length;
    if (ofs9 > size)
      ofs9 = size;    // limit bytes to write so we're sure it will fit

    while (ofs < ofs9)     // more data to put in buffers
    {
      if (speaker.blast <= speaker.bfirst)    // normal case
        size = speaker.huge_buffer^'length - speaker.bfirst;   // chunk size that can be written
      else
        size = speaker.blast - speaker.bfirst;

      if (size > ofs9 - ofs)
        size = ofs9 - ofs;


      // prepare and play this chunk

      if (speaker.hused == speaker.hdr^'length)   // no hdr's available, so allocate some
      {
        const int HOW_MANY_NEW = 4;
        PWAVEHDR[]^  p;
        int          i, count, idx;

        assert speaker.hlast == speaker.hfirst;

        p = new PWAVEHDR [speaker.hdr^'length + HOW_MANY_NEW];

        count = speaker.hdr^'length;
        idx = speaker.hlast;
        for (i=0; i<count; i++)
        {
          p^[i] = speaker.hdr^[idx];
          idx++;
          if (idx == speaker.hdr^'length)
            idx = 0;
        }

        for (; i<count+HOW_MANY_NEW; i++)
          p^[i] = new WAVEHDR;

        speaker.hlast = 0;
        speaker.hfirst = count;

        free speaker.hdr;
        speaker.hdr = p;
      }

      // write into huge buffer
      speaker.huge_buffer^[speaker.bfirst : size] = buffer[ofs : size];

      // prepare and play hdr
      {
        ref WAVEHDR h = speaker.hdr^[speaker.hfirst]^;
        int         rc;

        clear h;
        h.lpData  = &speaker.huge_buffer^[speaker.bfirst];
        h.dwBufferLength = (uint)size;

        rc = (int)waveOutPrepareHeader (speaker.wo, &h, WAVEHDR'size);
        if (rc != 0)
        {
          trace ("error: waveOutPrepareHeader() returned %d\n", rc);
          h.dwFlags |= WHDR_DONE;
          ret = 1;
        }
        else
        {
          ret = waveOutWrite (speaker.wo, &h, WAVEHDR'size);
        }
      }

      speaker.hused++;
      speaker.hfirst++;
      if (speaker.hfirst == speaker.hdr^'length)
        speaker.hfirst = 0;


      speaker.bused += size;
      speaker.bfirst += size;
      if (speaker.bfirst == speaker.huge_buffer^'length)
        speaker.bfirst = 0;

      ofs += size;
      bytes_written += size;

      if (ret != 0)
      {
        trace ("error: speaker_put_buffer() : waveOutWrite() returned %u\n", ret);
        leave_shared_object (ref g_so_speaker);
        return bytes_written;
      }
    }


/* do not close handle as it causes over 1 second delay when opening it again !
    if (speaker.wo != 0 && speaker.hused == 0)       // nothing left to clean
    {
      waveOutClose (speaker.wo);
      speaker.wo = 0;
    }
*/

    leave_shared_object (ref g_so_speaker);

    return bytes_written;
  }


  // pause the speaker (sent buffers will be buffered)
  // the function aborts if speaker is not open.

  public void speaker_pause (ref SPEAKER speaker, bool pause)       // true = pause, false = continue
  {
    enter_shared_object (ref g_so_speaker);

if (debug) trace ("BEGIN speaker_pause\n");

    assert speaker.huge_buffer != null;

    if (speaker.wo != 0 && pause != speaker.paused)
    {
      if (pause)
        (void)waveOutPause (speaker.wo);
      else
        (void)waveOutRestart (speaker.wo);
    }

    speaker.paused = pause;

if (debug) trace ("END speaker_pause\n");

    leave_shared_object (ref g_so_speaker);
  }


  // stop speaker, deallocate all intern buffers and close speaker.
  // the function aborts if speaker is not open.

  public void close_speaker (ref SPEAKER speaker)
  {
    enter_shared_object (ref g_so_speaker);

    assert speaker.huge_buffer != null;

    if (speaker.wo != 0)
    {
      waveOutReset (speaker.wo);

      release_old_buffers (ref speaker);

      waveOutClose (speaker.wo);
      speaker.wo = 0;
    }

    free speaker.huge_buffer;

    {
      int i;
      for (i=0; i<speaker.hdr^'length; i++)
        free speaker.hdr^[i];
      free speaker.hdr;
      speaker.hdr = null;
    }

if (debug) trace ("END close_speaker\n");

    clear speaker;

    leave_shared_object (ref g_so_speaker);
  }

end Speaker;

/*************************************************************************/
#end unsafe

/*************************************************************************/
/*************************************************************************/
#elif ANDROID
/*************************************************************************/
/*************************************************************************/

//-----------------------------------------------------------------------------------

package body Microphone

  struct MICROPHONE
  {
  }
  
  // get list of all microphones, from 0 to number_of_microphone_devices-1
  // returns number of microphone devices.
  public int get_microphone_list (out AUDIO_NAME[] list)
  {
    clear list;
    return 0;
  }

  // open microphone, set recording format and preallocate enough intern buffers
  // for 'buffer_seconds' of data.
  // the function aborts in case of illegal values or if micro is already open.
  public void open_micro (out MICROPHONE microphone, FORMAT_SOUND format, DEVICE device = DEFAULT_DEVICE, int buffer_seconds = 1)
  {
    clear microphone;
    _unused format, device, buffer_seconds;
  }

  // returns 0 if OK, -1 if error (no microphone available)
  // the function aborts if micro is not open, or if recording is already in progress.
  public int micro_start_recording (ref MICROPHONE microphone)
  {
    _unused microphone;
    return -1;
  }

  // the function aborts if micro is not open or if recording was not started.
  public void micro_stop_recording (ref MICROPHONE microphone)
  {
    _unused microphone;
  }

  // buffer'length must be a multiple of 4.
  // returns nb of bytes recorded in buffer, or 0 if no further data is available.
  public int micro_get_buffer (ref MICROPHONE microphone, out byte[] buffer)
  {
    _unused microphone;
    clear buffer;
    return 0;
  }

  // stop recording, deallocate all intern buffers, and close microphone.
  // the function aborts if micro is not open.
  public void close_micro (ref MICROPHONE microphone)
  {
    _unused microphone;
  }

end Microphone;

//-----------------------------------------------------------------------------------

package body Speaker

  struct SPEAKER
  {
  }

  // get list of all speakers, from 0 to number_of_speaker_devices-1
  // returns number of speaker devices.
  public int get_speaker_list (out AUDIO_NAME[] list)
  {
    clear list;
    return 0;
  }

  // open speaker, set format and preallocate enough intern buffers
  // for 'buffer_seconds' of data.
  // the function aborts in case of illegal values or if speaker is already open.
  public void open_speaker (out SPEAKER speaker, FORMAT_SOUND format, DEVICE device = DEFAULT_DEVICE, int buffer_seconds = 1)
  {
    clear speaker;
    _unused format, device, buffer_seconds;
  }

  // buffer'length must be a multiple of 4.
  // returns nb of buffer bytes sent, this can be less than buffer'length
  // when intern buffers are full or an i/o error occured.
  // the function aborts if speaker is not open.
  public int speaker_put_buffer (ref SPEAKER speaker, byte[] buffer)
  {
    _unused speaker, buffer;
    return 0;
  }

  // pause the speaker (sent buffers will be buffered)
  // the function aborts if speaker is not open.
  public void speaker_pause (ref SPEAKER speaker, bool pause)       // true = pause, false = continue
  {
    _unused speaker, pause;
  }
  
  // stop speaker, deallocate all intern buffers and close speaker.
  // the function aborts if speaker is not open.
  public void close_speaker (ref SPEAKER speaker)
  {
    _unused speaker;
  }

end Speaker;

//-----------------------------------------------------------------------------------

#else
  bad
  
#endif
  