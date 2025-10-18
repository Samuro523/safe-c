
// gi.h : low-level user interface

use ../thread;
use windows;

//------------------------------------------------------------------------------

typedef void GI_CREATE_OBJECTS ();
typedef void GI_PAINT_SCREEN (HDC hdc, PAINTSTRUCT ps);
typedef void GI_DESTROY_OBJECTS ();

GI_CREATE_OBJECTS  gi_create_objects;
GI_PAINT_SCREEN    gi_paint_screen;
GI_DESTROY_OBJECTS gi_destroy_objects;

//------------------------------------------------------------------------------

struct GDI
{
  SHARED_OBJECT shared;   // serializes all GDI calls (even for printer)
}

GDI gdi;

//------------------------------------------------------------------------------

// returns 0 if OK, -1 if error
int gi_init ();

//------------------------------------------------------------------------------

// closes main window
void gi_end ();

//------------------------------------------------------------------------------

// window

void gi_move (int x, int y, int width, int height);

void gi_set_title (string title);

// make main window visible/invisible
void gi_set_visible (bool visible);

void gi_set_resize_range (int min_width, int max_width, int min_height, int max_height);
void gi_set_resize_off ();

// query new window size after EVENT_RESIZE (note: this is a window size, not a user area size)
// (after the call the new size applies)
void gi_get_new_size (out int width, out int height);


enum CLOSE_BUTTON_STATE {
  _ACTIVE,              // closing window allowed (default)
  _INACTIVE,            // closing window does not work
  _CONVERT_TO_ESCAPE};  // closing window is converted into 10 ESCAPE keys

void gi_set_close_button (CLOSE_BUTTON_STATE state);
CLOSE_BUTTON_STATE gi_get_close_button_state ();

// must return true if message treated, false if windows should do default action
typedef bool TREAT_QUEUED_WINDOW_MESSAGE (HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

// returns hook function, or null
TREAT_QUEUED_WINDOW_MESSAGE gi_get_queued_window_message_hook ();

void gi_set_queued_window_message_hook (TREAT_QUEUED_WINDOW_MESSAGE func);


// must return true if message treated, false if windows should do default action
typedef bool TREAT_MAIN_WINDOW_MESSAGE (HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, out LRESULT result);

// returns hook function, or null
TREAT_MAIN_WINDOW_MESSAGE gi_get_main_window_message_hook ();

// returns previous hook function, or null
void gi_set_main_window_message_hook (TREAT_MAIN_WINDOW_MESSAGE func);


typedef void RANDOM_ADD_SEED (uint seed);
void gi_set_random_add_seed (RANDOM_ADD_SEED func);

// when user minimizes application, it appears in systray (this uses a message hook).
void minimize_in_systray ();

//------------------------------------------------------------------------------

// keyboard

// const int DEPRESSED_KEY_FLAG = -0x80000000;   // flag to OR to keycode value when key is depressed

void gi_put_key (int key);
bool gi_is_key_available ();
int gi_get_key ();

const int C_KEY_F1             =     -59;
const int C_KEY_F2             =     -60;
const int C_KEY_F3             =     -61;
const int C_KEY_F4             =     -62;
const int C_KEY_F5             =     -63;
const int C_KEY_F6             =     -64;
const int C_KEY_F7             =     -65;
const int C_KEY_F8             =     -66;
const int C_KEY_F9             =     -67;
const int C_KEY_F10            =     -68;
const int C_KEY_F11            =    -133;
const int C_KEY_F12            =    -134;

const int C_KEY_SHIFT_F1       =     -84;
const int C_KEY_SHIFT_F2       =     -85;
const int C_KEY_SHIFT_F3       =     -86;
const int C_KEY_SHIFT_F4       =     -87;
const int C_KEY_SHIFT_F5       =     -88;
const int C_KEY_SHIFT_F6       =     -89;
const int C_KEY_SHIFT_F7       =     -90;
const int C_KEY_SHIFT_F8       =     -91;
const int C_KEY_SHIFT_F9       =     -92;
const int C_KEY_SHIFT_F10      =     -93;
const int C_KEY_SHIFT_F11      =    -135;
const int C_KEY_SHIFT_F12      =    -136;

const int C_KEY_CTRL_F1        =     -94;
const int C_KEY_CTRL_F2        =     -95;
const int C_KEY_CTRL_F3        =     -96;
const int C_KEY_CTRL_F4        =     -97;
const int C_KEY_CTRL_F5        =     -98;
const int C_KEY_CTRL_F6        =     -99;
const int C_KEY_CTRL_F7        =    -100;
const int C_KEY_CTRL_F8        =    -101;
const int C_KEY_CTRL_F9        =    -102;
const int C_KEY_CTRL_F10       =    -103;
const int C_KEY_CTRL_F11       =    -137;
const int C_KEY_CTRL_F12       =    -138;

const int C_KEY_ALT_F1         =    -104;
const int C_KEY_ALT_F2         =    -105;
const int C_KEY_ALT_F3         =    -106;
const int C_KEY_ALT_F4         =    -107;
const int C_KEY_ALT_F5         =    -108;
const int C_KEY_ALT_F6         =    -109;
const int C_KEY_ALT_F7         =    -110;
const int C_KEY_ALT_F8         =    -111;
const int C_KEY_ALT_F9         =    -112;
const int C_KEY_ALT_F10        =    -113;
const int C_KEY_ALT_F11        =    -139;
const int C_KEY_ALT_F12        =    -140;

const int C_KEY_ESCAPE         =      27;
const int C_KEY_TAB            =       9;
const int C_KEY_SHIFT_TAB      =     -15;
const int C_KEY_ENTER          =      13;
const int C_KEY_BACKSPACE      =       8;

const int C_KEY_INSERT         =     -82;
const int C_KEY_DELETE         =     -83;
const int C_KEY_HOME           =     -71;
const int C_KEY_END            =     -79;
const int C_KEY_PAGE_UP        =     -73;
const int C_KEY_PAGE_DOWN      =     -81;

const int C_KEY_CURSOR_UP      =     -72;
const int C_KEY_CURSOR_DOWN    =     -80;
const int C_KEY_CURSOR_LEFT    =     -75;
const int C_KEY_CURSOR_RIGHT   =     -77;

const int C_KEY_CTRL_TAB          = -148;
const int C_KEY_CTRL_ENTER        =   10;
const int C_KEY_CTRL_BACKSPACE    =  127;
const int C_KEY_CTRL_CURSOR_LEFT  = -115;
const int C_KEY_CTRL_CURSOR_RIGHT = -116;

const int C_KEY_CTRL_INSERT     =   -146;
const int C_KEY_CTRL_DELETE     =   -147;
const int C_KEY_CTRL_HOME       =   -119;
const int C_KEY_CTRL_END        =   -117;
const int C_KEY_CTRL_PAGE_UP    =   -132;
const int C_KEY_CTRL_PAGE_DOWN  =   -118;

const int C_KEY_ALT_INSERT      =   -162;
const int C_KEY_ALT_DELETE      =   -163;
const int C_KEY_ALT_HOME        =   -151;
const int C_KEY_ALT_END         =   -159;
const int C_KEY_ALT_PAGE_UP     =   -153;
const int C_KEY_ALT_PAGE_DOWN   =   -161;

const int C_KEY_ALT_A           =   -(256+(int)'A');
const int C_KEY_ALT_Z           =   -(256+(int)'Z');

//------------------------------------------------------------------------------

// signal

const int MAX_SIGNALS = 64;

void gi_set_signal (int signal_nr);
bool gi_is_signal_available ();
int gi_get_signal ();

//------------------------------------------------------------------------------

// mouse

void gi_get_mouse_coordinates (out int x, out int y);
void gi_get_mouse_buttons (out bool left, out bool middle, out bool right);
void gi_get_mouse_wheel (out int wheel);   /* relative wheel move (+ or -) */

//------------------------------------------------------------------------------

// hourglass

/* enables/disables hourglass */
void gi_set_mouse_arrow (bool use_hourglass);

/* true = set_mouse_arrow() works (default) */
/* false = set_mouse_arrow() always sets normal shape */
void gi_set_hourglass_when_busy (bool enable);

// compute cursor shape as cursor moves
// x,y is the client area position
// must return 1 for arrow, 2 for cross, 3 for hand
typedef int CALLBACK_MOUSE_SHAPE (int x, int y);

// set a user-defined function, or null to cancel
void gi_set_callback_mouse_shape (CALLBACK_MOUSE_SHAPE fcallback_mouse_shape);

//------------------------------------------------------------------------------

// caret

void gi_enable_caret (int x, int y, int height);
void gi_disable_caret ();

//------------------------------------------------------------------------------

// events

const int WAIT_FOR_KEYBOARD = 0x0001;
const int WAIT_FOR_SIGNAL   = 0x0002;
const int WAIT_FOR_MOUSE    = 0x0004;
const int WAIT_FOR_RESIZE   = 0x0008;
const int WAIT_FOR_TIMEOUT  = 0x8000;

// wait until some event occurs or until timeout.
// (this function is used to avoid polling until an event occurs)
// 'wait'   : ORed events we want to wait for.
// 'timeout': the timeout in 1/1000 secs (if WAIT_FOR_TIMEOUT).
// returns: mask of events that occured.
int gi_wait (int wait, uint timeout);

/* wait : WAIT_FOR_KEYBOARD | WAIT_FOR_SIGNAL | WAIT_FOR_MOUSE | WAIT_FOR_RESIZE */
bool gi_are_events_pending (int wait);

// produce a dummy mouse event to wake up gi_wait()
void tingle_mouse_event ();

//------------------------------------------------------------------------------
