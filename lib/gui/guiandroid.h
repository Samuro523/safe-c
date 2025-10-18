
// guiandroid.h

use ../gui;

#begin unsafe
void create_main_window (APPLICATION_EVENT_HANDLER application_event_handler);
#end unsafe

void set_timer (uint msecs);
void stop_timer ();

void set_dialog_timer (uint msec);

void create_dialog (DIALOG_ID d, DIALOG_HANDLER dialog_handler);
void close_dialog (DIALOG_ID d);

void set_dialog_title (string title);
void wset_dialog_title (wstring title);

void post_dialog_message (DIALOG_ID d, int message);

// used by guiandroikeyboard.c
void post_event (EVENT ev);

void post_menu_event (MENU_ID id);


