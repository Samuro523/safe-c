
// media.h : multimedia for DirectX 11

use draw3d;

//--------------------------------------------------------------------------
struct Media;
//--------------------------------------------------------------------------

// needs prior initialization of socket and DirectX libraries
// 'use_media_engine' : false = use windows 7 media session, true = use window 8 media engine.
int start_media_player (out Media media, bool use_media_engine = true);

void stop_media_player (ref Media media);

//--------------------------------------------------------------------------

void media_play     (Media media, wstring s);
void media_sound    (Media media, float volume, bool mute);
void media_action   (Media media, int action);     // action:  0=stop 1=pause 2=start
void media_seek     (Media media, int mode, float seconds);  // mode 0=absolute, 1=relative, 2=from end

bool media_is_playing        (Media media);
bool media_is_paused         (Media media);

float media_total_duration   (Media media);  // returns media duration in seconds, or 0.0 if not yet playing

bool media_has_video_channel (Media media);
bool media_has_audio_channel (Media media);

// updates a DirectX 11 texture that can be immediately used in-world.
// texture_id must be initialized to 0
// the function will automatically allocate and free texture_id as needed.
void media_update_texture (Media media, ref TEXTURE_ID texture_id);

//--------------------------------------------------------------------------
