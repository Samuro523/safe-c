
// guicb.h : private interface layer between gui and guicb

#if WINDOWS
  use ../win/windows;
#endif  

use ../gui;
use guitree;

//--------------------------------------------------------------------------

BACKGROUND_COLOR  g_background_color;

//--------------------------------------------------------------------------

void create_main_window (APPLICATION_EVENT_HANDLER application_event_handler);

//--------------------------------------------------------------------------

void move_child_windows (RECT r);

HWND find_dialog_window (DIALOG_ID d);

void repaint_control (CONTROL_INFO o);

void repaint_control_rect (CONTROL_INFO o, RECT rect);

void set_cursor_shape ();

int register_relative_mouse (bool register);

// returns drive letter of removeable drive when user inserts it, or nul if none.
char detect_removeable_drive ();

//--------------------------------------------------------------------------
