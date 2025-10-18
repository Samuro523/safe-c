
// gui.h : graphical user interface with dialogs
// if you use Windows controls, add a Windows manifest (see chapter 18 of compiler documentation)

//--------------------------------------------------------------------------
//-------------------------  DESKTOP  --------------------------------------
//--------------------------------------------------------------------------


int advised_ui_scale();          // returns best UI scale (1 to 4)
void init_ui_scale (int scale);  // scaling (1, 2, 3, 4) is not applied to main screen, only to dialogs.
int ui_scale (int size);         // multiplies by scale
int ui_unscale (int size);       // divides by scale

//--------------------------------------------------------------------------

// example: (800 x 600), (1024 x 768), (1280 x 1024), ...
void get_desktop_resolution (out int x_size, out int y_size);

// get active area of desktop, excluding task bar and widgets.
void get_desktop_active_area (out int x, out int y, out int x_size, out int y_size);

// adapt so it fits on monitor
void fit_on_monitor (ref int x, ref int y, ref int x_size, ref int y_size);

//--------------------------------------------------------------------
//---------------  FONT  ---------------------------------------------
//--------------------------------------------------------------------

const string ARIAL                     = "Arial";
const string COURIER                   = "Courier";
const string COURIER_NEW               = "Courier New";
const string FONT_MICROSOFT_SANS_SERIF = "Microsoft Sans Serif";
const string TIMES_NEW_ROMAN           = "Times New Roman";

const int MAX_FONT_NAME_LENGTH = 32;

const uint UNDERLINED  = 1;
const uint ITALIC      = 2;
const uint BOLD        = 4;

packed struct FONT
{
  char name[MAX_FONT_NAME_LENGTH]; // ARIAL, COURIER, ..
  int  height;                     // in pixels
  uint style;                      // a combination of UNDERLINED, ITALIC, BOLD
  uint color;                      // rgb color of text
}

// compute width of a text, in pixels, using specified font.
// these functions are not multi-thread safe.
int text_width_of2 (string text, FONT font);
int wtext_width_of2 (wstring text, FONT font);

//--------------------------------------------------------------------------
// ------------------------ MAIN WINDOW ------------------------------------
//--------------------------------------------------------------------------

enum APPLICATION_EVENT_TYPE {
  EVENT_NEW_APPLICATION,
  EVENT_MAIN_WINDOW_MOVE,   // user must call set_main_window_position (e.x, e.y);
  EVENT_MAIN_WINDOW_RESIZE, // user must call set_main_window_size (e.x_size, e.y_size);
  EVENT_MAIN_WINDOW_MINIMIZED, // notify user that main window was minimized.
  EVENT_ACTIVATE,           // notify user that app was activated (e.key=1) or deactivated (e.key=0)
  EVENT_KEYBOARD,
  EVENT_MOUSE_MOVE,
  EVENT_MOUSE_CLICKED,    // left mouse button clicked, e.x,e.y contains position, key=1 for click, 2=second of doubleclick
  EVENT_MOUSE_DRAG,       // move+left mouse button clicked, e.x,e.y contains position
  EVENT_MOUSE_DROP,       // release left mouse button, e.x,e.y contains position
  EVENT_TEST_DROP_ALLOWED,// test if drop allowed, if yes call signal_drop_allowed.
  EVENT_MOUSE_CANCEL,     // mouse moved outside main window
  EVENT_MOUSE_WHEEL,      // .y
  EVENT_MOUSE_MIDDLE_BUTTON, // .key == 1 if pressed, 0 if unpressed or leaving window
  EVENT_MOUSE_MENU,       // right mouse button clicked, e.x,e.y contains position
  EVENT_TIMER,
  EVENT_MENU,             // menu was selected or post_menu_event() was called
  EVENT_CLOSE_BUTTON,     // user hit CLOSE button of main window (must call close_main_window())
  EVENT_END_APPLICATION,
  EVENT_RELATIVE_MOUSE_MOVE,
  EVENT_RELATIVE_MOUSE_LEFT_CLICK,
  EVENT_RELATIVE_MOUSE_RIGHT_CLICK,
};

typedef short MENU_ID;    // must be > 0

struct APPLICATION_EVENT
{
  APPLICATION_EVENT_TYPE type;
  int     key;              // for EVENT_KEYBOARD (0 = keyboard focus lost)
  int     scancode;         // for EVENT_KEYBOARD
  MENU_ID id;               // for EVENT_MENU
  int     x, y;             // for EVENT_MOUSE_xxx, EVENT_MAIN_WINDOW_MOVE
  int     x_size, y_size;   // for EVENT_MAIN_WINDOW_RESIZE : new size asked
}

const int KEY_SHIFT   = 0x01_0000;
const int KEY_CONTROL = 0x02_0000;
const int KEY_ALT     = 0x04_0000;
const int KEY_CMD     = 0x08_0000;  // non-printable
const int KEY_RELEASE = 0x10_0000;  // key released (always with KEY_CMD)

enum BACKGROUND_COLOR {BK_BLACK, BK_WHITE, BK_TRANSPARENT};
void set_main_window_background_color (BACKGROUND_COLOR color);

typedef void APPLICATION_EVENT_HANDLER (APPLICATION_EVENT e);

// this function does not return until the main window is closed.
void create_main_window (APPLICATION_EVENT_HANDLER application_event_handler);

// these functions are to be called within an event handler :

void close_main_window ();   // in response to EVENT_CLOSE_BUTTON.

// get size of the 4 small borders of a main window.
//  (the difference between window size and client area)
void get_main_window_borders (out int left, out int top, out int right, out int bottom);

void get_main_window_position (out int x, out int y);
void get_main_window_size (out int x_size, out int y_size);

void set_main_window_position (int x, int y);
void set_main_window_size (int x_size, int y_size);

// to be called after setting window position and size, otherwise nothing appears on screen !
void show_window ();

// get actual intern size (maybe user resized the application window or added a menu)
// scaled = false to obtain real screen coordinates.
void get_main_window_intern_size (out int width, out int height, bool scaled = true);

void set_main_window_title (string title);
void wset_main_window_title (wstring title);

// must be called by handler EVENT_TEST_DROP_ALLOWED if drop is allowed.
void signal_drop_allowed ();

// disables mouse cursor and generates only relative mouse moves EVENT_RELATIVE_MOUSE_MOVE,
// EVENT_RELATIVE_MOUSE_LEFT_CLICK and EVENT_RELATIVE_MOUSE_RIGHT_CLICK.
void set_relative_mouse_mode (bool on);

//--------------------------------------------------------------------

//--------------------------------------------------------------------------
// --------------------------- MENUS ---------------------------------------
//--------------------------------------------------------------------------

void append_menu (string name);
void wappend_menu (wstring name);

// assertion: id in 1 .. 32767
void append_menu_item (string name, MENU_ID id);
void wappend_menu_item (wstring name, MENU_ID id);

// assertion: id in 1 .. 32767
void enable_menu_item (bool enable, MENU_ID id);

void remove_menu_bar ();

// when a menu item is selected, an EVENT_MENU is generated with .id containing the MENU_ID

void post_menu_event (MENU_ID id);

//--------------------------------------------------------------------------
// ------------------------- TIMER -----------------------------------------
//--------------------------------------------------------------------------

// start repetitive timer generating EVENT_TIMER on main window
void start_timer (uint msecs);
void stop_timer ();

//--------------------------------------------------------------------------
// ---------------------- DIALOG EVENTS ------------------------------------
//--------------------------------------------------------------------------

enum EVENT_TYPE
{
  EVENT_NEW_DIALOG,              // first event after creating dialog :
                                 // event handler must set title, position, size and create controls.

  EVENT_CLOSE_DIALOG,            // last event before closing dialog.

  EVENT_MOVE,                    // user wants to move dialog :
                                 // - event handler must call set_dialog_position (e.x, e.y);

  EVENT_RESIZE,                  // user wants to resize dialog :
                                 // - at ini, call once set_dialog_border_drag_size(4);
                                 // - event handler must call set_dialog_size (e.x_size, e.y_size);

  EVENT_NEW_FOCUS,               // focus was set to a new control using TAB keys or mouse :
                                 // (button, edit, checkbox, listbox or window).
                                 // (.key contains the last key pressed, or 0 if it was a mouse click)

  EVENT_DIALOG_ACTIVATED,        // user clicked on dialog (.other_d is deactivated)
  EVENT_DIALOG_DEACTIVATED,      // user clicked on another dialog (.other_d)

  EVENT_BUTTON_PRESSED,          // button was pressed

  EVENT_CHECKBOX_CHANGED,        // checkbox setting was changed
  EVENT_RADIOBUTTON_CHANGED,     // radiobutton setting was changed

  EVENT_EDIT_CHANGED,            // edit/combo field was changed
  EVENT_EDITBOX_CHANGED,         // editbox was changed, use editbox_get_info() to obtain latest info.

  EVENT_TEST_DROP_ON_CONTROL_ALLOWED,  // test if drop on text/edit/window id allowed, if yes call signal_drop_allowed.
  EVENT_DROP_ON_CONTROL,               // drop on text/edit/window id

  EVENT_LISTBOX_LINE_SELECTED,   // listbox/combo/tree (for single selection, .line_nr indicates line nr)
  EVENT_LISTBOX_LINE_DESELECTED, // listbox line/tree .line_nr was deselected (event generated for single selection only)
  EVENT_LISTBOX_DRAG,              // listbox id line .line_nr dragged
  EVENT_LISTBOX_TEST_DROP_ALLOWED, // test if drop on listbox id allowed, if yes call signal_drop_allowed.
  EVENT_LISTBOX_DROP,              // drop on listbox id line .line_nr (.line_nr can be out of range !)

  EVENT_TREE_EXPANDED,           // tree line .line_nr was selected for expansion [+] -> [-] (user must insert child items of level+1)
  EVENT_TREE_COLLAPSED,          // tree line .line_nr was selected for collapse [-] -> [+] (user must delete child items of level+1)
  EVENT_TREE_DELETE,             // tree line .line_nr was selected for deletion (user must delete line and child items of level+1)

  EVENT_SCROLLBAR_MOVED,         // scrollbar moved by user (x or y = new shown top position)

  EVENT_WINDOW_CLICKED,          // user clicked in window draw area
  EVENT_WINDOW_DRAG,             // mouse moved in window with pressed mouse button (coordinates can be outside window)
  EVENT_WINDOW_DROP,             // mouse button released in window (coordinates can be int'min if outside window)
  EVENT_WINDOW_HOVER,            // user mouse moves over window without clicking
  EVENT_WINDOW_CLICKED_RIGHT,    // user clicked in window draw area with right mouse button
  EVENT_WINDOW_DROP_RIGHT,       // right mouse button released in window (coordinates can be outside window)

  EVENT_CONTEXT_MENU,            // right mouse click (.id, .x, .y)

  EVENT_MESSAGE,                 // .message
  
  EVENT_DIALOG_TIMER,
};

typedef int CONTROL_ID;
typedef uint DIALOG_ID;

struct EVENT
{
  DIALOG_ID   d;
  EVENT_TYPE  type;     // type of the event
  CONTROL_ID  id;       // denotes the control associated with the event, or 0 for EVENT_NEW_DIALOG/CLOSE_DIALOG/RESIZE

  int key;              // for EVENT_NEW_FOCUS (last key pressed)
                        // for EVENT_WINDOW_CLICKED, EVENT_LISTBOX_LINE_SELECTED, EVENT_LISTBOX_DRAG indicates mouse button 1 or 2.
  int message;          // for EVENT_USER_MESSAGE
  int line_nr;          // for EVENT_LISTBOX_xxx and EVENT_TREE_xxx : listbox or tree line number
  int x, y;             // for EVENT_MOVE, EVENT_SCROLLBAR_MOVED, EVENT_WINDOW_xxx (window coordinates or outside it)
  int x_size, y_size;   // for EVENT_RESIZE : new size asked
  DIALOG_ID  other_d;   // for EVENT_DIALOG_ACTIVATED, EVENT_DIALOG_DEACTIVATED : the other dialog, or 0.
}

//--------------------------------------------------------------------------
// --------------------------- DIALOGS -------------------------------------
//--------------------------------------------------------------------------

typedef void DIALOG_HANDLER (EVENT e);
void create_dialog (DIALOG_ID d, DIALOG_HANDLER dialog_handler);

void close_dialog (DIALOG_ID d);

// make dialog d visible/invisible
void show_dialog (bool visible, DIALOG_ID d);

// allow dialog to receive keyboard/mouse events, or not
void enable_dialog (bool enabled, DIALOG_ID d);

// posts event EVENT_MESSAGE to dialog d (for inter-dialog communication)
void post_dialog_message (DIALOG_ID d, int message);

// put dialog in foreground (like clicking on it) (call with d == 0 for deactivating all dialogs)
void activate_dialog (DIALOG_ID d);

// returns windows handle of dialog
long hwnd_of (DIALOG_ID d);

//---------------------------------------------------------------------------------
// --- all functions below can only be used WITHIN a dialog handler function ------
//---------------------------------------------------------------------------------

typedef uint[6] GUI_COLORS;

void close_my_dialog ();

void set_dialog_position (int x, int y);
void set_dialog_size (int x_size, int y_size);                // extern size (border and title included)

// set border size that can be clicked to resize window using EVENT_RESIZE
void set_dialog_border_visual_size (int border_visual_size);  // should be 2 for 3D effect, default is 2.
void set_dialog_border_colors (uint[4] colors);               // default is {0x000000, 0x808080, 0xE0E0E0, 0xFFFFFF}
void set_dialog_border_drag_size (int border_drag_size);      // virtual interior border for mouse drag - should be 4 for resizing, default is 0.

void set_dialog_title_height (int height);                    // default is 20
void set_dialog_title (string title);
void wset_dialog_title (wstring title);
void set_dialog_title_font (FONT font);                       // default is "Tahoma", 16, black.
void set_dialog_title_background_color (uint color);          // default is 0xFF8080 (blue)
void set_dialog_close_button (bool enabled);                  // show close button on top right of title

void set_dialog_background_color (uint color);                // default is 0xC0C0C0.
void set_dialog_transparency (int focus, int non_focus);      // default = (255, 165)
void set_dialog_default_font (FONT default_font);             // set new default font for all new controls of this dialog.

// set new default colors for all new controls of this dialog.
void set_dialog_default_control_colors (GUI_COLORS default_colors);  // default is {0x000000, 0x808080, 0xC0C0C0, 0xC0C0C0, 0xE0E0E0, 0xFFFFFF}

// set default insert/delete mode of all new edit/combo fields of this dialog
void set_dialog_default_insert_mode (bool insert);  // true=insert, false=delete  (default = insert)

// redirect keyboard input (and mouse wheel events) to main window events
void redirect_keyboard_to_main_window (bool redirect);

//--------------------------------------------------------------------------
// ---------- GENERAL SETTINGS APPLYING ON CONTROLS ------------------------
//--------------------------------------------------------------------------

// place the cursor on the specified control (this does not generate EVENT_NEW_FOCUS)
void set_focus (CONTROL_ID id);

void get_position (out int x, out int y, CONTROL_ID id);
void set_position (int x, int y, CONTROL_ID id);

void get_size (out int x_size, out int y_size, CONTROL_ID id);
void set_size (int x_size, int y_size, CONTROL_ID id);

void get_visible (out bool visible, CONTROL_ID id);
void set_visible (bool visible, CONTROL_ID id);

// set visibility of all controls with id between id1 and id2
void set_visibles (bool visible, CONTROL_ID id1, CONTROL_ID id2);

void set_font (FONT font, CONTROL_ID id);     // change font (not allowed for listbox, combo & window control)
void get_font (out FONT font, CONTROL_ID id);

void set_colors (GUI_COLORS colors, CONTROL_ID id);
void get_colors (out GUI_COLORS colors, CONTROL_ID id);

// set insert/delete mode of edit or combo or editbox control (default = insert)
void set_insert_mode (bool       insert,    // true=insert, false=delete
                      CONTROL_ID id);

// get insert/delete mode of edit/combo/editbox control (true=insert, false=delete)
bool get_insert_mode (CONTROL_ID id);

// decide if user can type in field (default = true), for edit, checkbox, combo, editbox and radiobutton.
void set_modify_mode (bool modify_allowed, CONTROL_ID id);

// get modify mode for edit, combo and editbox.
bool get_modify_mode (CONTROL_ID id);

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//-----------------  TEXT : simple text fields -----------------------
//--------------------------------------------------------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// y_size must be >= font_height.

void create_text (int x, int y, int x_size, int y_size, CONTROL_ID id);

//--------------------------------------------------------------------

// initialize the text field with a string

void text_put (string text, CONTROL_ID id);
void wtext_put (wstring text, CONTROL_ID id);

//--------------------------------------------------------------------
//---------  BUTTON : rectangle with text, can be clicked ------------
//--------------------------------------------------------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// (x_size,y_size) denote the button size, for example (74,23)
//  'x_size' must be >= 4.
//  'y_size' must be >= (font_size + 4).
// 'text' : a shortkey can be created by a preceding '&' sign,
//  for example the button "&OK" will be accessible by typing 'ALT-o'.

void create_button (int x, int y, int x_size, int y_size, string text, CONTROL_ID id);
void wcreate_button (int x, int y, int x_size, int y_size, wstring text, CONTROL_ID id);

//--------------------------------------------------------------------

// modify text of a button
void button_set_text (string text, CONTROL_ID id);
void wbutton_set_text (wstring text, CONTROL_ID id);

//--------------------------------------------------------------------
//----  CHECKBOX : rectangle zone that can be checked 'v' + text -----
//--------------------------------------------------------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// 'x_size' must be >= (box_width + 4).
// 'y_size' must be >= (font_height + 4).
// 'box_width' : width of the box with the 'v' sign.

void create_checkbox (int x, int y, int x_size, int y_size, int box_width, string text, CONTROL_ID id);
void wcreate_checkbox (int x, int y, int x_size, int y_size, int box_width, wstring text, CONTROL_ID id);

//--------------------------------------------------------------------

// retrieve the setting of a checkbox.
// 'setting' : true=enabled, false=disabled.

void checkbox_get (out bool setting, CONTROL_ID id);

//--------------------------------------------------------------------

// modify the setting of a checkbox.
// 'setting' : true=enabled, false=disabled.

void checkbox_set (bool setting, CONTROL_ID id);

//-----------------------
//----  RADIOBUTTON -----
//-----------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// 'x_size' must be >= (box_width + 4).
// 'y_size' must be >= (font_height + 4).
// 'box_width' : width of the box with the 'O' sign.

typedef uint RADIOBUTTON_GROUP_ID;   // unique ID per group of radiobuttons

void create_radiobutton (int x, int y, int x_size, int y_size, int box_width, string text, RADIOBUTTON_GROUP_ID gid, CONTROL_ID id);
void wcreate_radiobutton (int x, int y, int x_size, int y_size, int box_width, wstring text, RADIOBUTTON_GROUP_ID gid, CONTROL_ID id);

//--------------------------------------------------------------------

// retrieve the id of the selected radiobutton of the group, or 0 if none is selected.

CONTROL_ID radiobutton_selected (RADIOBUTTON_GROUP_ID gid);

//--------------------------------------------------------------------

// modify the setting of a radiobutton.
// enable button 'id' and disable all other buttons in the group.

void radiobutton_set (CONTROL_ID id);

//--------------------------------------------------------------------
//-----------------  COMBO : editable text line + listbox ------------
//--------------------------------------------------------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// 'x_size' must be >= (4 + arrow_box_width)
// 'y_size' must be >= (font_height + 4)
// 'length' denotes the max number of editable characters.
// All functions for EDIT and LISTBOX controls can be used on COMBO controls.

void create_combo (int x, int y, int x_size, int y_size,
                   int arrow_box_width, int arrow_box_height,
                   int length,
                   int max_listbox_lines,  // >= 1
                   CONTROL_ID id);

//--------------------------------------------------------------------
//-----------------  TREE : listbox with expandable lines ------------
//--------------------------------------------------------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// 'x_size' must be >= (4 + arrow_box_width)
// 'y_size' must be >= (4 + font_height)
// 'length' denotes the max number of editable characters.
// All functions for LISTBOX controls can be used on TREE controls.

void create_tree (int x, int y, int x_size, int y_size,
                  int arrow_box_width, int arrow_box_height,
                  int length, CONTROL_ID id);

//--------------------------------------------------------------------
//-----------------  EDIT : editable text lines ----------------------
//--------------------------------------------------------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// 'x_size' must be >= 4.
// 'y_size' must be >= (font_height + 4)
// 'length' denotes the max number of editable characters.

void create_edit (int x, int y, int x_size, int y_size, int length, CONTROL_ID id);

//--------------------------------------------------------------------

// retrieve the contents of an edit or combo control.
// buffer'size must be >= length bytes
//   (see function create_edit(), above).

void edit_get (out string buffer, CONTROL_ID id);
void wedit_get (out wstring buffer, CONTROL_ID id);

//--------------------------------------------------------------------

// same as above, but the leading and trailing spaces of the result are removed.

void edit_get2 (out string buffer, CONTROL_ID id);
void wedit_get2 (out wstring buffer, CONTROL_ID id);

//--------------------------------------------------------------------

// initialize the edit/combo control with a string, put cursor in column 1, remove marks.

void edit_put (string text, CONTROL_ID id);
void wedit_put (wstring text, CONTROL_ID id);

//--------------------------------------------------------------------

// for edit or combo, controls undo by ctrl-z and ctrl-y

void edit_reset_undo (CONTROL_ID id);
void edit_save_value_for_undo (CONTROL_ID id);  // to be called before and after edit_put

//--------------------------------------------------------------------

void edit_insert_text (string text, CONTROL_ID id);
void wedit_insert_text (wstring text, CONTROL_ID id);

//--------------------------------------------------------------------

// 'cursor_x' must be in range 1 to length+1 (see function create_edit())

void edit_set_cursor (int cursor_x, CONTROL_ID id);

//--------------------------------------------------------------------

// returns cursor position within edit field (range 1 to length+1)

int edit_get_cursor (CONTROL_ID id);

//--------------------------------------------------------------------

// set password mode (all characters are displayed as '****')

void edit_set_password_mode (CONTROL_ID id);

//--------------------------------------------------------------------
//---- LISTBOX : a list of selectable text lines, with scrollbar -----
//--------------------------------------------------------------------

// (x,y) are coordinates within the dialog client area;
//   for example (0,0) is the upper left dialog corner inside border and below title.
// (x_size, y_size) denotes the listbox size, including the border;
// 'x_size' must be >= 4 + arrow_box_width.
// 'y_size' must >= 4 + font_height.
// 'arrow_box_width' and 'arrow_box_height' : 16 (must be >= 4).
// 'length' denotes the max number of characters of a line.

void create_listbox (int x, int y,        int x_size, int y_size,
                     int arrow_box_width, int arrow_box_height,
                     int length,          CONTROL_ID id);

//--------------------------------------------------------------------

struct ITEM_ATTRIBUTES
{
  int8  reference;   // user reference
  uint2 type;        // for a tree : icon type (for future use)
  bool  is_folder;   // for a tree : item can contain other items
  uint2 level;       // for a tree : depth level (0 = root)
}

const ITEM_ATTRIBUTES DEFAULT_ATTRIBUTES = {reference => 0, type => 0, is_folder => false, level => 0};

//--------------------------------------------------------------------

// insert a line in listbox 'id' at 'index'.
// text'length must not exceed 'length'.

void listbox_insert (int             index,    // 0 = append to end
                     string          text,
                     CONTROL_ID      id,
                     ITEM_ATTRIBUTES attr = DEFAULT_ATTRIBUTES);

void wlistbox_insert (int             index,    // 0 = append to end
                      wstring         text,
                      CONTROL_ID      id,
                      ITEM_ATTRIBUTES attr = DEFAULT_ATTRIBUTES);

//--------------------------------------------------------------------

// update a line in listbox 'id' at 'index'.
// text'length must not exceed 'length'.

void listbox_update (int        index,
                     string     text,
                     CONTROL_ID id);

void wlistbox_update (int        index,
                      wstring    text,
                      CONTROL_ID id);

//--------------------------------------------------------------------

// same as above, with additional reference values.

void listbox_update2 (int        index,
                      string     text,
                      int8       reference,
                      CONTROL_ID id);

void wlistbox_update2 (int        index,
                       wstring    text,
                       int8       reference,
                       CONTROL_ID id);

//--------------------------------------------------------------------

// retrieve a line from listbox 'id' at 'index'.
// 'text' must be a buffer of at least 'length' bytes.

void listbox_retrieve (    int        index,
                       out string     text,
                           CONTROL_ID id);

void wlistbox_retrieve (    int        index,
                        out wstring    text,
                            CONTROL_ID id);

//--------------------------------------------------------------------

// same as above, with additional attribute values.

void listbox_retrieve2 (    int             index,
                        out string          text,
                        out ITEM_ATTRIBUTES attr,
                            CONTROL_ID      id);

void wlistbox_retrieve2 (    int             index,
                         out wstring         text,
                         out ITEM_ATTRIBUTES attr,
                             CONTROL_ID      id);

//--------------------------------------------------------------------

// deletes a line in listbox 'id' at 'index'

void listbox_delete (int        index,
                     CONTROL_ID id);

//--------------------------------------------------------------------

// counts the number of lines in the listbox 'id'

int listbox_count (CONTROL_ID id);

//--------------------------------------------------------------------

// returns the current cursor line, or 0 if the listbox is empty

int listbox_cursor (CONTROL_ID id);

//--------------------------------------------------------------------

// set the current cursor line (error if the listbox is empty).
// note: this function does not set the focus on the listbox.
//       it also does not select or deselect a line.

void listbox_set_cursor (int index, CONTROL_ID id);

//--------------------------------------------------------------------

// allow/disallow the selection of lines by the user

void listbox_allow_user_selection (bool allow, CONTROL_ID id);

//--------------------------------------------------------------------

// allow/disallow the searching of lines using a hotkey (default=allow)
//  (this makes only sense if the listbox lines are sorted)

void listbox_allow_hotkey_search (bool allow, CONTROL_ID id);

//--------------------------------------------------------------------

// permit the simultaneous selection of several lines. Setting this mode
// cancels any selection done in the earlier single selection mode.
// it is not possible to reset the listbox to single selection mode.
// allowed for listbox or tree, not for combobox.

void listbox_set_multiselection (CONTROL_ID id);

//--------------------------------------------------------------------

// obtain the currently selected line, or 0 if no line was selected.
// this function is not allowed if multiselection was enabled (see above).

int listbox_selected_line (CONTROL_ID id);

//--------------------------------------------------------------------

// test if listbox line 'index' is selected.

bool listbox_line_is_selected (int index, CONTROL_ID id);

//--------------------------------------------------------------------

// select the listbox line 'index'.
// 'select': false = deselect, true = select.

void listbox_select_line (int index, bool select, CONTROL_ID id);

//--------------------------------------------------------------------

// obtain the nr of the top line displayed in the listbox.

int listbox_top_displayed_line (CONTROL_ID id);

//--------------------------------------------------------------------

// sort listbox lines using uppercase comparison and extended characters.

void listbox_sort (CONTROL_ID id);

//--------------------------------------------------------------------

// user-defined function that must return
// -1 if line1 < line2, 0 if equal, 1 if line1 > line2
typedef int COMPARE_LISTBOX_LINES
              (    int8 reference1, bool selected1, wstring line1,
                   int8 reference2, bool selected2, wstring line2);

// sort listbox lines using user-defined comparison function
void listbox_sort_user (CONTROL_ID id, COMPARE_LISTBOX_LINES user_compare);

//--------------------------------------------------------------------
//------------- SCROLLBAR --------------------------------------------
//--------------------------------------------------------------------

void create_horizontal_scrollbar (int x, int y, int x_size, int y_size,
                                  int arrow_box_width, CONTROL_ID id);

//--------------------------------------------------------------------

void horizontal_scrollbar_set_arrow_box_width (int arrow_box_width, CONTROL_ID id);

//--------------------------------------------------------------------

void create_vertical_scrollbar (int x, int y, int x_size, int y_size,
                                int arrow_box_height, CONTROL_ID id);

//--------------------------------------------------------------------

void vertical_scrollbar_set_arrow_box_height (int arrow_box_height, CONTROL_ID id);

//--------------------------------------------------------------------

// full_size = total height of object to display, in lines or pixels.
// shown_size = height of object shown on screen, in lines or pixels.
void scrollbar_set_range (int full_size, int shown_size, CONTROL_ID id);

//--------------------------------------------------------------------

void scrollbar_set_position (int pos, CONTROL_ID id);  // set top shown position

//--------------------------------------------------------------------

int scrollbar_position (CONTROL_ID id);  // get top shown position

//--------------------------------------------------------------------

// small_y_increment = nb lines or pixels to jump when clicking on arrow box.
void scrollbar_set_line_position_increment (int small_y_increment, CONTROL_ID id);

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//---------  WINDOW : drawing area -----------------------------------
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen.
// (x_size,y_size) denotes the window size.

void create_window (int x, int y, int x_size, int y_size, CONTROL_ID id);

//--------------------------------------------------------------------

// 'flag' = false : disable refresh.
// is used in case an extern control, like a webcam, refreshes it.
void window_set_refresh_mode (bool flag, CONTROL_ID id);

//--------------------------------------------------------------------

// the following functions are available to draw in a window :

void window_set_color (uint rgb, CONTROL_ID id);
void window_pixel (int x, int y, CONTROL_ID id);
void window_line  (int x1, int y1, int x2, int y2, CONTROL_ID id);
void window_box   (int x1, int y1, int x2, int y2, CONTROL_ID id);
void window_text  (int x, int y, string text, CONTROL_ID id);
void window_wtext (int x, int y, wstring text, CONTROL_ID id);

// writes text in bottom left corner, use offset_x, offset_y to change that.
void window_text_opaque (int x, int y, int width, int height, string text, uint background_color, CONTROL_ID id, int offset_x = 0, int offset_y = 0);
void window_wtext_opaque (int x, int y, int width, int height, wstring text, uint background_color, CONTROL_ID id, int offset_x = 0, int offset_y = 0);

// display an image in a window.
// 'image' must be an array of (4 * size_x * size_y) bytes.
// Each pixel is stored as 4 bytes in format RGBS :
// the three first bytes indicate the red, green, blue intensity of the pixel.
// scaled = false to use real screen coordinates.

void window_raster (int x, int y, int size_x, int size_y, byte[] image, CONTROL_ID id,
                    bool immediate = false, bool scaled = true);

//--------------------------------------------------------------------
//---- EDITBOX -------------------------------------------------------
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen;
//   for example (1,1) is the upper left screen corner.
// (x_size, y_size) denotes the editbox size, including the border;
// a non-proportional font, like Courier New, must be used.
void create_editbox (int x, int y, int x_size, int y_size, int max_line_length, int max_text_lines, CONTROL_ID id);

//--------------------------------------------------------------------

// sets the text_was_modified flag.
// This flag can be examined using editbox_text_was_modified(),
void editbox_set_text_was_modified_flag (bool text_was_modified, CONTROL_ID id);

//--------------------------------------------------------------------

// returns a flag indicating if the text was modified.
// The flag is set to true if the user modifies the text.
// It is set to false if a new text is loaded using editbox_load_text(),
//   if the text is saved using editbox_save_text(), or if lines are
//   inserted, updated or deleted.
bool editbox_text_was_modified (CONTROL_ID id);

//--------------------------------------------------------------------

// returns the number of lines in the editbox (always >= 1)
int editbox_text_count_lines (CONTROL_ID id);

//--------------------------------------------------------------------

void editbox_get_cursor (out int col, out int line, CONTROL_ID id);

//--------------------------------------------------------------------

void editbox_set_cursor (int col, int line, int page, CONTROL_ID id);

//--------------------------------------------------------------------

// note: the text_was_modified flag is set to false (see editbox_text_was_modified())
void editbox_clear_text (CONTROL_ID id);

//--------------------------------------------------------------------

// store the full text
// note: the text_was_modified flag is set to false (see editbox_text_was_modified())
void editbox_store_text (wstring text, CONTROL_ID id);

//--------------------------------------------------------------------

// retrieve the full text
// note: the text_was_modified flag is set to false (see editbox_text_was_modified())
wstring^ editbox_retrieve_full_text (CONTROL_ID id);

//--------------------------------------------------------------------

struct EDITBOX_INFORMATION
{
  int  col;              // current column (>= 0)
  int  ln;               // current line (>=1)
  bool insert;           // true=insert mode, false=delete mode
  int  page;             // line number of first screen line (>= 1)
  int  scroll;           // column of first screen column (>= 0)
}

void editbox_get_info (out EDITBOX_INFORMATION editbox_information, CONTROL_ID id);

//--------------------------------------------------------------------

// key:     1 : CTRL-A : select all
//          3 : CTRL-C : copy
//         22 : CTRL-V : paste
//         24 : CTRL-X : cut
//  0x08_002E : Delete : delete selection

void editbox_press_key (int key, CONTROL_ID id);

//--------------------------------------------------------------------

enum MOUSE_SHAPE {MOUSE_SHAPE_DEFAULT, MOUSE_SHAPE_HOURGLASS, MOUSE_SHAPE_NONE};
void set_mouse_cursor_shape (MOUSE_SHAPE shape);

//--------------------------------------------------------------------

void set_wallpaper (int size_x, int size_y, byte[] image);

//--------------------------------------------------------------------

// one-shot timer, generates EVENT_DIALOG_TIMER
void start_dialog_timer (uint msecs);

//--------------------------------------------------------------------

#if WINDOWS
  // returns drive letter of removeable drive when user inserts it, or nul if none.
  char detect_removeable_drive ();
#endif

//--------------------------------------------------------------------
