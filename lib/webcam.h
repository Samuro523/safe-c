
// webcam.h - webcam image capture

use image;

//------------------------------------------------------------------------------------

typedef wstring(64) WEBCAM_NAME;

//------------------------------------------------------------------------------------

// get list of all webcams;
// returns number of webcams available.

int get_webcam_list (out WEBCAM_NAME[] list);

//------------------------------------------------------------------------------------

struct WEBCAM_CAPTURE;   // opaque type

//------------------------------------------------------------------------------------

// returns 0 if OK, -1 if error.
int start_webcam (ref WEBCAM_CAPTURE info,
                      WEBCAM_NAME    source_name,
                      int            width  = 320,  // 1280 (most common)
                      int            height = 240); //  720

void stop_webcam (ref WEBCAM_CAPTURE info);

//------------------------------------------------------------------------------------

// to be called if webcam is no longer used
void deallocate_webcam (ref WEBCAM_CAPTURE info);

//------------------------------------------------------------------------------------

// display dialog box with capture settings.
// (works only for old DirectShow cams)
void display_webcam_dialog (ref WEBCAM_CAPTURE info, long ParentWhwnd, int x, int y);

//------------------------------------------------------------------------------------

// returns the most recent webcam image.
// returns -1 if there is no image available (especially just after starting webcam),
//   or returns an image number >= 0 that can be the same as for the previous call if no new image is available.
// The function manages 'image', don't ever allocate or deallocate it.
// You can use 'image' after the call until calling unlock_webcam_image().
// several threads can call this function.

int lock_and_get_webcam_image (ref WEBCAM_CAPTURE info,
                               out IMAGE_INFO     image,
                               out int            lock_number);

//------------------------------------------------------------------------------------

// releases intern buffer
// You must always call unlock_webcam_image() after each lock_and_get_lock_webcam_image() call !

void unlock_webcam_image (ref WEBCAM_CAPTURE info, int lock_number);

//------------------------------------------------------------------------------------
