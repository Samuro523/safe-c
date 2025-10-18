
// win.h : graphic user interface
// requires adding a Windows manifest for windows controls (see chapter 18 of compiler documentation)

//--------------------------------------------------------------------
//---------------  MESSAGE BOX  --------------------------------------
//--------------------------------------------------------------------

// display a simple message in a message box and wait for the user
// to click on a button.
// 'message' may contain several lines separated by a '\n' character.
// A screen need not be created before calling this function.

void message_box (string title, string message, string button);


//--------------------------------------------------------------------
//---------------  SCREEN  -------------------------------------------
//--------------------------------------------------------------------

// example: (800 x 600), (1024 x 768), (1280 x 1024), ...

void get_desktop_resolution (out int x_res, out int y_res);

//--------------------------------------------------------------------

// get active area of desktop, excluding task bar.

void get_desktop_active_area (out int x, out int y, out int x_size, out int y_size);

//--------------------------------------------------------------------

// get size of the 4 small borders of a window.
//  (the difference between screen size and client area)

void get_screen_borders (out int left, out int top, out int right, out int bottom);

//--------------------------------------------------------------------

// set default font for all new objects.
// default is (FONT_MICROSOFT_SANS_SERIF, 12).

const string FONT_ARIAL                = "Arial";
const string FONT_COURIER              = "Courier";
const string FONT_COURIER_NEW          = "Courier New";
const string FONT_MICROSOFT_SANS_SERIF = "Microsoft Sans Serif";
const string FONT_TIMES_NEW_ROMAN      = "Times New Roman";

const int MAX_FONT_NAME_LENGTH = 32;

void set_font (string font_name,     // see above constants (max MAX_FONT_NAME_LENGTH chars)
               int    font_height);  // in pixels

void get_font (out string(MAX_FONT_NAME_LENGTH) font_name,
               out int                          font_height);

//--------------------------------------------------------------------

// font style constants :
const uint _FONT_STYLE_UNDERLINED  = 0x0001;
const uint _FONT_STYLE_ITALIC      = 0x0002;
const uint _FONT_STYLE_BOLD        = 0x0004;

struct FONT_STYLE
{
  char name[MAX_FONT_NAME_LENGTH]; // font name
  int  height;                     // in pixels
  uint style;                      // see font-style constants above
  uint color;                      // rgb color of text
}

void set_font_style (FONT_STYLE style);
void get_font_style (out FONT_STYLE style);

//--------------------------------------------------------------------

// compute width of a text, in pixels.

// text        : input text.
// font_name   : can be empty string for default font name.
// font_height : can be 0 for default font height.

int text_width_of (string  text,
                   string  font_name   = "",
                   int     font_height = 0);

int text_width_of2 (string     text,
                    FONT_STYLE font);

//--------------------------------------------------------------------

// set basic background colors for screens and (edit/checkbox/listbox) fields
// (default: RGB(192,192,192) for screens, RGB(255,255,255) for fields)

void win_set_background_colors (uint screen, uint field);
void win_get_background_colors (out uint screen, out uint field);

//--------------------------------------------------------------------

// (x,y) denotes the upper left corner of the screen in pixels,
//   for example (0,0) for the upper left desktop corner.
// (x_size, y_size) denotes the screen size, including the border;
//   use (0,0) for a full screen.
// Once a screen is created, screen objects may be created (see below)

void create_screen (int x, int y, int x_size, int y_size, string title);

//--------------------------------------------------------------------

void close_screen ();

//--------------------------------------------------------------------

// change title of screen

void set_screen_title (string title);

//--------------------------------------------------------------------

// get resolution of client area of last created screen.
// the client area is where objects can be drawn.

void get_screen_resolution (out int x_res, out int y_res);

//--------------------------------------------------------------------

// user-defined identifier associated with every screen object.
// an object id must be positive (>=1) and unique within a screen.

typedef int OBJECT_ID;

//--------------------------------------------------------------------


#if 0  // menus are not supported at the moment

//--------------------------------------------------------------------
//-----------------  MENU --------------------------------------------
//--------------------------------------------------------------------

// 'text' : a shortkey can be created using a preceding '&' sign,
// for example the menu "&Files" will be accessible by typing ALT-F.
// the function returns 0 if OK, or (-1) if out of virtual storage.

int create_menu (int x_size, string text);

//--------------------------------------------------------------------

// append a menu item at the end of the last created menu (see above).
// 'text' : a shortkey can be created using a preceding '&' sign,
// for example the menu item "s&Ave" will be accessible by typing 'a'.
// the function returns 0 if OK, or (-1) if out of virtual storage.

int create_menu_item (int x_size, string text, OBJECT_ID id);

#endif




//--------------------------------------------------------------------
//-----------------  TEXT : simple text fields -----------------------
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen;
//   for example (0,0) is the upper left screen corner.
// y_size must be >= font_height.

void create_text (int x, int y, int x_size, int y_size, OBJECT_ID id);

//--------------------------------------------------------------------

// initialize the text field with a string

void text_put (string text, OBJECT_ID id);


//--------------------------------------------------------------------
//-----------------  EDIT : editable text lines ----------------------
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen;
//   for example (0,0) is the upper left screen corner.
// 'x_size' should be = 8+text_width_of(text,..)
// 'y_size' must be >= (font_height + 4)
// 'length' denotes the max number of editable characters.

void create_edit (int x, int y, int x_size, int y_size, int length, OBJECT_ID id);

//--------------------------------------------------------------------

// retrieve the contents of an edit field.
// buffer'size must be >= length bytes
//   (see function create_edit(), above).

void edit_get (out string buffer, OBJECT_ID id);

//--------------------------------------------------------------------

// same as above, but the leading and trailing spaces of
// the result string are removed.

void edit_get2 (out string buffer, OBJECT_ID id);

//--------------------------------------------------------------------

// initialize the edit field with a string and put cursor in column 1

void edit_put (string text, OBJECT_ID id);

//--------------------------------------------------------------------

// 'cursor_x' must be in range 1 to length+1 (see function create_edit())

void edit_set_cursor (int cursor_x, OBJECT_ID id);

//--------------------------------------------------------------------

// returns cursor position within edit field (range 1 to length+1)

int edit_get_cursor (OBJECT_ID id);

//--------------------------------------------------------------------

// 'scroll_x' must be in range 0 to length, it indicates the nb of chars hidden on the left of the edit field
// this function should be called after setting the cursor position with edit_set_cursor(),
// it will adapt scroll_x in case the cursor is not visible.

void edit_set_scroll_offset (int scroll_x, OBJECT_ID id);

//--------------------------------------------------------------------

/* returns scroll_x of edit field, it indicates the nb of chars hidden on the left of the edit field */

int edit_get_scroll_offset (OBJECT_ID id);

//--------------------------------------------------------------------

// set insert/delete mode of an edit field (default = insert)

void edit_set_insert_mode (bool      insert,    // true=insert, false=delete
                           OBJECT_ID id);

//--------------------------------------------------------------------

// get insert/delete mode of an edit field (true=insert, false=delete)

bool edit_get_insert_mode (OBJECT_ID id);

//--------------------------------------------------------------------

// set default insert/delete mode of all new edit fields (default = insert)

void edit_set_default_insert_mode (bool insert);  // true=insert, false=delete

//--------------------------------------------------------------------

// set password mode (all characters are displayed as '****')

void edit_set_password_mode (OBJECT_ID id);

//--------------------------------------------------------------------

void edit_set_modifiable (bool      boolean,  /* true=modify allowed, false=not allowed */
                          OBJECT_ID id);

//--------------------------------------------------------------------

/* returns: true=modify allowed, false=not allowed */
bool edit_is_modifiable (OBJECT_ID id);

//--------------------------------------------------------------------

//--------------------------------------------------------------------
//----  CHECKBOX : rectangle zone that can be checked 'v' + text -----
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen;
//   for example (0,0) is the upper left screen corner.
// 'x_size' must be >= (box_width + 4).
// 'y_size' must be >= (font_height + 4).
// 'box_width' : width of the box with the 'v' sign.

void create_checkbox (int x, int y, int x_size, int y_size,
                      int box_width, string text, OBJECT_ID id);

//--------------------------------------------------------------------

// retrieve the setting of a checkbox.
// 'setting' : true=enabled, false=disabled.

void checkbox_get (out bool setting, OBJECT_ID id);

//--------------------------------------------------------------------

// modify the setting of a checkbox.
// 'setting' : true=enabled, false=disabled.

void checkbox_set (bool setting, OBJECT_ID id);

//--------------------------------------------------------------------


#if 0   // editbox are not supported at the moment

//--------------------------------------------------------------------
//--------------  EDITBOX  -------------------------------------------
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen;
//   for example (1,1) is the upper left screen corner.
// (x_size, y_size) denotes the editbox size, including the border;
//   both sizes must at least be >= 3 for a 1x1 editbox.
// 'max_line_length' must not exceed 512.
// the function returns 0 if OK, or (-1) if out of virtual storage.

int create_editbox (int x, int y, int x_size, int y_size,
                    int max_line_length, OBJECT_ID id);

//--------------------------------------------------------------------

// set insert/delete mode of an editbox (default = insert)

void editbox_set_insert_mode (bool      insert, // true=insert, false=delete
                              OBJECT_ID id);

//--------------------------------------------------------------------

// set default insert/delete mode of all new edit boxes (default = insert)

void editbox_set_default_insert_mode (bool insert);  // true=insert, false=delete

//--------------------------------------------------------------------

// set read-only / read-write mode of an editbox (default = read-write)

void editbox_allow_modify_text (bool      allow, // true=read-write, false=read-only
                                OBJECT_ID id);

//--------------------------------------------------------------------

// show the header line "Col xx  Line yyy   Insert  *" (default), or not.

void editbox_display_header (bool display, OBJECT_ID id);

//--------------------------------------------------------------------

// sets the text modified flag. This flag can be examined using
// editbox_was_modified(), see below.

void editbox_set_text_modified_flag (bool modified, OBJECT_ID id);

//--------------------------------------------------------------------

// returns a flag indicating if the text was modified.
// The flag is set to true if the user modifies the text.
// It is set to false if a new text is loaded using editbox_load_text(),
//   if the text is saved using editbox_save_text(), or if lines are
//   inserted, updated or deleted.

bool editbox_was_modified (OBJECT_ID id);

//--------------------------------------------------------------------

// insert line 'index' in editbox 'id'.
// text'length must be <= max_line_length

void editbox_insert (int       index,    // 0 = append to end
                     string    text,
                     OBJECT_ID id);

//--------------------------------------------------------------------

// update line 'index' in editbox 'id'.
// text'length must be <= max_line_length.

void editbox_update (int       index,
                     string    text,
                     OBJECT_ID id);

//--------------------------------------------------------------------

// retrieve line 'index' from editbox 'id'.
// 'text' must be a buffer of at least max_line_length bytes.

void editbox_retrieve (    int       index,
                       out string    text,
                           OBJECT_ID id);

//--------------------------------------------------------------------

// deletes line 'index' in editbox 'id'

void editbox_delete (int       index,
                     OBJECT_ID id);

//--------------------------------------------------------------------

// returns the number of lines in the editbox

int editbox_count (OBJECT_ID id);

//--------------------------------------------------------------------

// 'col' should be in range 1 to max_line_length (see above)
// 'ln' should be in range 1 to editbox_count()+1 (see above)

int editbox_set_cursor (int col, int ln, OBJECT_ID id);

//--------------------------------------------------------------------

// returns the position of the marked text block.
// note that the first position is included and the last is excluded.

void editbox_get_marks (    OBJECT_ID id,
                        out bool block_is_marked,
                        out int first_col, out int first_ln,
                        out int last_col,  out int last_ln);

//--------------------------------------------------------------------

// search a text fragment within an editbox,
// start searching at the current cursor position.
// returns 0 if a fragment was found (the cursor is also set to
//   this position).
// returns (+1) if no further occurence of the fragment was found.
// returns -3, -4 or -5 in case of illegal values for the
//   parameters 'direction', 'match_case' and 'whole_word'.
// returns -6 if fragment_length is zero.
// if (replace),
//   returns (+2) if the text is read-only.
//   returns (-8) if the fragment cannot be contained in the line.
//   returns (-9) if the line becomes too long.

int editbox_search (OBJECT_ID id,
                    string    fragment,
                    int       direction,    // -1 = up, +1 = down
                    bool      match_case,   // true = case must match
                    bool      whole_word,   // true = delimited by non-alpha
                    bool      replace,      // false = find only, true = replace
                    string    replacer);

//--------------------------------------------------------------------

//  read_only: is an out parameter : false = file was opened in read-write,
//                                   true = file was opened in read-only mode.
//
//  read_final_crlf: false = treat a text-terminating CR LF as an extra line
//                   true = read terminating CR LF of last line (normal case)
//
//  return codes:     0 = OK
//                   -1 = cannot open file (not found)
//                   -2 = cannot read file (disk read error, ..)
//                   -3 = out of virtual memory (cannot insert text line)
//
//  notes:
//  . the text_modified flag is set to zero (see editbox_was_modified())
//  . the allow_modify_text is set to zero if the text was opened in
//    read-only access (see editbox_allow_modify_text ()),
//    it is set to one(!) if the text was opened in read/write access.

int editbox_load_text (    string    filename,
                       out bool      read_only,
                           bool      read_final_crlf,
                           OBJECT_ID id);

//--------------------------------------------------------------------

//  mode:             0 = do not overwrite old file of the same name,
//                    1 = overwrite old file,
//                    2 = rename old file into '.bak'
//
//  write_final_crlf: false = do not terminate last line with CR LF
//                    true  = terminate last line with CR LF (normal case)
//
//  return codes:     0 = OK
//                   -1 = cannot create file (bad format, file exists, ..)
//                   -2 = cannot write to file (disk full, ..)
//                   -3 = out of virtual memory (cannot flush text)
//
//  note: the text_modified flag is set to zero (see editbox_was_modified())

int editbox_save_text (string    filename,
                       int       mode,
                       bool      write_final_crlf,
                       OBJECT_ID id);

//--------------------------------------------------------------------

struct CONTEXT;

// exchanges the editbox's text. The context must be filled with zeroes
// or must represent a null pointer for a new text.
// 'old_context' and 'new_context' must be different.

int editbox_exchange_context (    OBJECT_ID id,
                              ref CONTEXT   old_context,
                              ref CONTEXT   new_context);

//--------------------------------------------------------------------
#endif


//--------------------------------------------------------------------
//---- LISTBOX : a list of selectable text lines, with scrollbar -----
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen;
//   for example (0,0) is the upper left screen corner.
// (x_size, y_size) denotes the listbox size, including the border;
//  'x_size' must be > 4.
//  'y_size' must = (4 + font_height * number_of_lines).
// 'arrow_box_width' and 'arrow_box_height' : 16 (must be >= 4).
// 'length' denotes the max number of characters of a line.

void create_listbox (int x, int y,        int x_size, int y_size,
                     int arrow_box_width, int arrow_box_height,
                     int length,          OBJECT_ID id);

//--------------------------------------------------------------------

// insert a line in listbox 'id' at 'index'.
// text'length must not exceed 'length'.

void listbox_insert (int       index,    // 0 = append to end
                     string    text,
                     OBJECT_ID id);

//--------------------------------------------------------------------

// same as above, with an additional user-defined reference value
// that can be retrieved using listbox_retrieve2().

void listbox_insert2 (int       index,       // 0 = append to end
                      string    text,
                      int       reference,
                      OBJECT_ID id);

//--------------------------------------------------------------------

// update a line in listbox 'id' at 'index'.
// text'length must not exceed 'length'.

void listbox_update (int       index,
                     string    text,
                     OBJECT_ID id);

//--------------------------------------------------------------------

// same as above, with an additional user-defined reference value
// that can be retrieved using listbox_retrieve2().

void listbox_update2 (int       index,
                      string    text,
                      int       reference,
                      OBJECT_ID id);

//--------------------------------------------------------------------

// retrieve a line from listbox 'id' at 'index'.
// 'text' must be a buffer of at least 'length' bytes.

void listbox_retrieve (    int       index,
                       out string    text,
                           OBJECT_ID id);

//--------------------------------------------------------------------

// same as above, with an additional reference value that was
//   inserted using listbox_insert2() or listbox_update2().

void listbox_retrieve2 (    int       index,
                        out string    text,
                        out int       reference,
                            OBJECT_ID id);

//--------------------------------------------------------------------

// deletes a line in listbox 'id' at 'index'

void listbox_delete (int       index,
                     OBJECT_ID id);

//--------------------------------------------------------------------

// counts the number of lines in the listbox 'id'

int listbox_count (OBJECT_ID id);

//--------------------------------------------------------------------

// returns the current cursor line, or 0 if the listbox is empty

int listbox_cursor (OBJECT_ID id);

//--------------------------------------------------------------------

// set the current cursor line (error if the listbox is empty).
// note: this function does not set the focus on the listbox.
//       it also does not select or deselect a line.

void listbox_set_cursor (int index, OBJECT_ID id);

//--------------------------------------------------------------------

// allow/disallow the selection of lines by the user

void listbox_allow_user_selection (bool allow, OBJECT_ID id);

//--------------------------------------------------------------------

// allow/disallow the searching of lines using a hotkey (default=allow)
//  (this makes only sense if the listbox lines are sorted)

void listbox_allow_hotkey_search (bool allow, OBJECT_ID id);

//--------------------------------------------------------------------

// permit the simultaneous selection of several lines. Setting this mode
// cancels any selection done in the earlier single selection mode.
// it is not possible to reset the listbox to single selection mode.

void listbox_set_multiselection (OBJECT_ID id);

//--------------------------------------------------------------------

// obtain the currently selected line, or 0 if no line was selected.
// this function is not allowed if multiselection was enabled (see above).

int listbox_selected_line (OBJECT_ID id);

//--------------------------------------------------------------------

// test if listbox line 'index' is selected.

bool listbox_line_is_selected (int index, OBJECT_ID id);

//--------------------------------------------------------------------

// select the listbox line 'index'.
// 'select': false = deselect, true = select.

void listbox_select_line (int index, bool select, OBJECT_ID id);

//--------------------------------------------------------------------

// sort listbox lines using uppercase comparison and extended characters.

void listbox_sort (OBJECT_ID id);

//--------------------------------------------------------------------

typedef int COMPARE_LISTBOX_LINES (int reference1, bool selected1, string line1,
                                   int reference2, bool selected2, string line2);

// sort listbox lines using user-defined comparison function
void listbox_sort_user (OBJECT_ID id, COMPARE_LISTBOX_LINES user_compare);


//--------------------------------------------------------------------
//---------  BUTTON : rectangle with text, can be clicked ------------
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen.
// (x_size,y_size) denote the button size, for example (74,23)
//  'y_size' must be >= (font_size + 4).
// 'text' : a shortkey can be created by a preceding '&' sign,
//  for example the button "&OK" will be accessible by typing 'ALT-o'.

void create_button (int x, int y, int x_size, int y_size, string text, OBJECT_ID id);


//--------------------------------------------------------------------
//---------  WINDOW : drawing area -----------------------------------
//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen.
// (x_size,y_size) denotes the window size.

void create_window (int x, int y, int x_size, int y_size, OBJECT_ID id);

//--------------------------------------------------------------------

// 'flag' = false : disable refresh.
void window_set_refresh_mode (bool flag, OBJECT_ID id);

//--------------------------------------------------------------------

// the following functions are available to draw in a window :

void window_get_resolution (out int x, out int y, OBJECT_ID id);
void window_set_color (byte[3] rgb, OBJECT_ID id);
void window_pixel (int x, int y, OBJECT_ID id);
void window_line  (int x1, int y1, int x2, int y2, OBJECT_ID id);
void window_box   (int x1, int y1, int x2, int y2, OBJECT_ID id);
void window_text  (int x, int y, string text, OBJECT_ID id);


// display an image in a window.
// 'raster' must be an array of (4 * size_x * size_y) bytes.
// Each pixel is stored as 4 bytes in format RGBS :
// the three first bytes indicate the Red, Green, Blue intensity of the pixel;
// 'S' is reserved for future use and should be zero.

void window_raster (int x, int y, int size_x, int size_y,
                    byte[] raster, OBJECT_ID id);


//--------------------------------------------------------------------
//--------------  SCROLLBAR  -----------------------------------------
//--------------------------------------------------------------------

void create_vertical_scrollbar (int x, int y, int x_size, int y_size,
                                int arrow_box_height, OBJECT_ID id);

//--------------------------------------------------------------------

void scrollbar_set_arrow_box_height (int arrow_box_height, OBJECT_ID id);

//--------------------------------------------------------------------

// full_size = total height of object to display, in lines or pixels.
// shown_size = height of object shown on screen, in lines or pixels.
void scrollbar_set_range (int full_size, int shown_size, OBJECT_ID id);

//--------------------------------------------------------------------

void scrollbar_set_position (int pos, OBJECT_ID id);  // set top shown position

//--------------------------------------------------------------------

// small_y_increment = nb lines or pixels to jump when clicking on arrow box.
void scrollbar_set_line_position_increment (int small_y_increment, OBJECT_ID id);


//--------------------------------------------------------------------
//-------- SIGNAL : an event signaling that something happened -------
//--------------------------------------------------------------------

// trigger a signal, i.e; the function get_event() below will return
//   an event of typ EVENT_SIGNAL.
// calling this function several times with the same signal_nr has no effect.
// calling get_event() to retrieve the signal clears it.

void set_signal (int signal_nr);

//--------------------------------------------------------------------
//--------- GET EVENT : obtain next user event -----------------------
//--------------------------------------------------------------------

// constants for EVENT_KEY_PRESSED :
const int KEY_ESCAPE     = 100;
const int KEY_F1         = 101;
const int KEY_F2         = 102;
const int KEY_F3         = 103;
const int KEY_F4         = 104;
const int KEY_F5         = 105;
const int KEY_F6         = 106;
const int KEY_F7         = 107;
const int KEY_F8         = 108;
const int KEY_F9         = 109;
const int KEY_F10        = 110;
const int KEY_F11        = 111;
const int KEY_F12        = 112;
const int KEY_CTRL_ENTER = 256;

const int KEY_USER_EVENT1 = 258;  // generated by trigger_user_event()
const int KEY_USER_EVENT2 = 259;
const int KEY_USER_EVENT3 = 260;

// if arrow keys are reserved (see win_reserve_arrow_keys) :
const int KEY_MOVE_UP    = 300;
const int KEY_MOVE_DOWN  = 301;
const int KEY_MOVE_LEFT  = 302;
const int KEY_MOVE_RIGHT = 303;
const int KEY_TURN_LEFT  = 304;
const int KEY_TURN_RIGHT = 305;

enum EVENT_TYP       // the user did something :
{
  EVENT_MENU_SELECTED,           // menu item was selected
  EVENT_EDIT_CHANGED,            // edit field was changed
  EVENT_CHECKBOX_CHANGED,        // checkbox setting was changed
  EVENT_LISTBOX_LINE_SELECTED,   // listbox line was selected
  EVENT_LISTBOX_LINE_DESELECTED, // listbox line was deselected
  EVENT_BUTTON_PRESSED,          // button was clicked
  EVENT_SCROLLBAR_MOVED,         // scrollbar was moved
  EVENT_KEY_PRESSED,             // keyboard control key was pressed (see KEY_xx)

  EVENT_WINDOW_CLICKED,  // user clicked in window draw area
  EVENT_WINDOW_DRAG,     // mouse moved in window with pressed mouse button
  EVENT_WINDOW_DROP,     // mouse button released in window
  EVENT_MOUSE_WHEEL,     // mouse wheel turned (up>0 or down<0)

  EVENT_NEW_FOCUS,       // cursor was moved (TABed or clicked) to a new object :
                         // (edit, editbox, checkbox, listbox or button).
                         // (key=last key pressed, or 0 if mouse click)

  EVENT_MAIN_SCREEN_RESIZED,  // main screen was resized
  EVENT_SIGNAL                // user triggered a signal (signal nr is in .key)
};

struct EVENT
{
  EVENT_TYP typ; // typ of the event
  OBJECT_ID id;  // denotes the object associated with the event

  int  key;   // for EVENT_KEY_PRESSED (KEY_xx constant)
              // for EVENT_WINDOW_CLICKED indicates mouse button 1 or 2.
              // for EVENT_SIGNAL indicates signal_nr

  int  x, y;  // for EVENT_WINDOW_xxx (window coordinates)
              // for EVENT_MOUSE_WHEEL (y up=+ or down=-)

  int  x_size, y_size;  // for EVENT_MAIN_SCREEN_RESIZED
}


// let the user control the screen and returns the next user event.

void get_event (out EVENT event);


//--------------------------------------------------------------------
//----------------  other special functions --------------------------
//--------------------------------------------------------------------

// place the cursor on the specified object
//  (edit, editbox, checkbox, listbox or button).
// This does not(!) generate an event EVENT_NEW_FOCUS.

void set_focus (OBJECT_ID id);

//--------------------------------------------------------------------

// make objects visible/invisible.
// 'visible' : false=invisible, true=visible.

void set_visible (OBJECT_ID id, bool visible);

//--------------------------------------------------------------------

// obtain an object's visibility state.
// 'visible' : false=invisible, true=visible.

void get_visible (OBJECT_ID id, out bool visible);

//--------------------------------------------------------------------

// change the style of a (text, edit, checkbox or button)
// note: font.height must not exceed object limits !

void set_object_font_style (FONT_STYLE style, OBJECT_ID id);

//--------------------------------------------------------------------

// get the current style of an object.

void get_object_font_style (out FONT_STYLE style, OBJECT_ID id);

//--------------------------------------------------------------------

// redraw the screen in case a long computation takes place
// without 'get_event' being called.

void refresh_win ();

//--------------------------------------------------------------------

// force a redraw of the entire screen.

void reset_win ();

//--------------------------------------------------------------------

// delete all objects that have been created within this screen.

void delete_all_screen_objects ();

//--------------------------------------------------------------------

// this user-defined function will be called when the win layer has nothing to do;
// it must return nb msecs when it wishes to be called again (0=never).
typedef uint WIN_IDLE_CALLBACK_ROUTINE ();

// set a function to be called when the user controls the screen
// 'func' must denote a user-defined function, or null to cancel.
void win_set_idle_callback_routine (WIN_IDLE_CALLBACK_ROUTINE func);

//--------------------------------------------------------------------

// make screen resizable (generates event EVENT_MAIN_SCREEN_RESIZED if user resizes it)
// set all values to -1 to disable resizing.

void win_set_min_max_main_screen_size (int min_x, int max_x, int min_y, int max_y);

//--------------------------------------------------------------------

void set_position (int x, int y, OBJECT_ID id);
void set_size (int x_size, int y_size, OBJECT_ID id);
void set_field_color (uint field_color, OBJECT_ID id);
uint get_field_color (OBJECT_ID id);

/**********************************************************************/

// reserve arrow keys for move actions.
// reserve: false = default, true = cursor up/down/left/right/ctrl-left/ctrl-right
//                                  produce events and do not change screen objects

void win_reserve_arrow_keys (bool reserve);

//--------------------------------------------------------------------
