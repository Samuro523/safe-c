
use opus_types;


#begin unsafe

struct OpusRepacketizer
{
   byte      toc;
   int       nb_frames;
   byte      *frames[48];
   short     len[48];
   int       framesize;
}

/** Configures the encoder's expected percentage of voice
  * opposed to music or other signals.
  *
  * @note This interface is currently more aspiration than actuality. It's
  * ultimately expected to bias an automatic signal classifier, but it currently
  * just shifts the static bitrate to mode mapping around a little bit.
  *
  * @param[in] x <tt>int</tt>:   Voice percentage in the range 0-100, inclusive.
  * @hideinitializer */

/** Gets the encoder's configured voice ratio value, @see OPUS_SET_VOICE_RATIO
  *
  * @param[out] x <tt>int*</tt>:  Voice percentage in the range 0-100, inclusive.
  * @hideinitializer */

int encode_size(int size,     byte      *data);


/* Make sure everything's aligned to sizeof(void *) bytes */
int align(int i);


opus_int32 opus_repacketizer_out_range_impl(OpusRepacketizer *rp, int begin, int _end,     byte      *data, opus_int32 maxlen, int self_delimited);



  int opus_repacketizer_get_size();

 OpusRepacketizer *opus_repacketizer_init(OpusRepacketizer *rp) ;

  OpusRepacketizer *opus_repacketizer_create();

 void opus_repacketizer_destroy(OpusRepacketizer *rp);

 int opus_repacketizer_cat(OpusRepacketizer *rp,           byte      *data, opus_int32 len)  ;

  opus_int32 opus_repacketizer_out_range(OpusRepacketizer *rp, int begin, int _end,     byte      *data, opus_int32 maxlen)  ;

  int opus_repacketizer_get_nb_frames(OpusRepacketizer *rp) ;

  opus_int32 opus_repacketizer_out(OpusRepacketizer *rp,     byte      *data, opus_int32 maxlen) ;


#end unsafe
