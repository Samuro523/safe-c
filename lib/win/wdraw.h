
// wdraw.h : win drawing primitives

use windows;

/***********************************************************************/

void _win_get_background_colors (out uint screen, out uint field);
void _win_set_background_colors (uint screen, uint field);

bool colors_changed;   /* set to true if user changes theme */

/***********************************************************************/

void create_gi_objects ();
void destroy_gi_objects ();

/***********************************************************************/

void display_set_default_colors (HDC hdc);

/***********************************************************************/

void display_button (HDC    hdc,
                     int    x,
                     int    y,
                     int    width,
                     int    height,   /* should be >= 4 + text_height */
                     string text,
                     bool   pressed,
                     bool   has_focus,
                     uint   text_color);

/***********************************************************************/

void display_static_text (HDC    hdc,
                          int    x,
                          int    y,
                          int    width,
                          int    height,  /* should be 4 + text_height */
                          string text,
                          uint   text_color);

/***********************************************************************/

void display_edit (HDC    hdc,
                   int    x,
                   int    y,
                   int    width,
                   int    height,      /* should be 4 + text_height */
                   string text,
                   int    offset_x,    /* start display at text[offset_x]   */
                   int    cursor_x,    /* position of cursor, starting at 0 */
                   int    begin_mark,  /* selected piece of text, or -1     */
                   int    end_mark,
                   uint   text_color,
                   bool   explicit_field_color,
                   uint   field_color);

/***********************************************************************/

void display_edit_caret (HWND   hwnd,
                         HDC    hdc,
                         int    x,
                         int    y,
                         int    width,
                         int    height,      /* should be 4 + text_height */
                         string text,
                         int    offset_x,    /* start display at text[offset_x]   */
                         int    cursor_x);

/***********************************************************************/

void display_checkbox (HDC    hdc,
                       int    x,
                       int    y,
                       int    width,
                       int    height,  /* should be 4 + text_height */
                       int    box_width,
                       bool   setting,
                       string text,
                       bool   has_focus);

/***********************************************************************/

void display_screen (HDC    hdc,
                     int    x,
                     int    y,
                     int    width,
                     int    height,
                     string title,
                     int    title_height);

/***********************************************************************/

void display_main_screen (HDC hdc, int x, int y, int width, int height);

/***********************************************************************/

void compute_scrollbox_data
  (int x,
   int y,
   int width,
   int height,           /* must be (4+nb_lines*text_height) */
   int scrollbar_width,
   int scrollbar_arrow_box_height,
   int current_page_nr,
   int nb_listbox_lines,
   int nb_screen_lines,
   out int sx,      /* area where we can draw the moveable scroll box */
   out int sy,
   out int sw,
   out int sh,
   out int scrollbox_y_offset,
   out int scrollbox_height);

void display_listbox (HDC hdc,
                      int x,
                      int y,
                      int width,
                      int height,   /* must be (4+nb_lines*text_height) */
                      int scrollbar_width,
                      int scrollbar_arrow_box_height,
                      int current_page_nr,
                      int nb_listbox_lines,
                      int nb_screen_lines);

/***********************************************************************/

void display_listbox_line (HDC    hdc,
                           int    x,
                           int    y,
                           int    width,
                           int    height,
                           string text,
                           bool   is_selected,
                           bool   has_focus);

/***********************************************************************/

void compute_scrollbox_data2
      (    int height,           // must be (nb_lines*text_height)
           int scrollbar_arrow_box_height,
           int current_page_nr,
           int nb_listbox_lines,
           int nb_screen_lines,
       out int scrollbox_y_offset,
       out int scrollbox_height);

/***********************************************************************/

void display_vertical_scrollbar (
        HDC hdc,
        int x,
        int y,
        int scrollbar_width,
        int height,   // must be (nb_lines*text_height)
        int scrollbar_arrow_box_height,
        int current_page_nr,
        int nb_listbox_lines,
        int nb_screen_lines);

/***********************************************************************/

void display_menu (HDC    hdc,
                   int    x,
                   int    y,
                   int    width,
                   int    height,   /* must be (2+text_height) */
                   string text,
                   bool   has_focus,
                   bool   pressed,
                   int    hotkey_index);   /* index within 'text' or -1 */

/***********************************************************************/

void display_menu_item_box (HDC  hdc,
                            int  x,
                            int  y,
                            int  width,
                            int  height);   /* must be (6+N*text_height) */

/***********************************************************************/

void display_menu_item
  (HDC    hdc,
   int    x,             /* use 'x' of menu_item_box */
   int    y,             /* 'y' of menu_item_box + N * text_height */
   int    width,         /* use width of menu_item_box */
   int    height,        /* must be text_height */
   string text,
   int    hotkey_index,  /* index within 'text' or -1 */
   bool   has_focus);

/***********************************************************************/
