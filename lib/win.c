
// win.c

/*************************************************************************/

use clipboard, edline, thread, strings, tracing, text;
use win/gi, win/winstr, win/wdraw, win/windows;

/*************************************************************************/
#begin unsafe
/*************************************************************************/

const char SPACE = ' ';

SCREEN_INFO^  pscreen;   // main tree of screen and objects

FONT_STYLE    current_font = {name   => "Microsoft Sans Serif\0           ",
                              height => 12,
                              style  => 0,
                              color  => 0x000000};

enum FUNCNAME {
  F, Fset_font, Fwin_set_char, Freset_win, Frefresh_win, Fcreate_screen, Fget_screen_resolution, Fcreate_button, Fset_screen_title,
  Fcreate_text, Ftext_put, Fcreate_edit, Fedit_get, Fedit_put, Fedit_set_cursor, Fedit_get_cursor,
  Fedit_set_insert_mode, Fedit_get_insert_mode, Fedit_set_password_mode, Fcreate_window, Fwindow_get_resolution,
  Fwindow_set_refresh_mode, Fwindow_set_color, Fwindow_pixel, Fwindow_line, Fwindow_box, Fwindow_text, Fwindow_raster,
  Fclose_screen, Fdelete_all_screen_objects, Fedit_set_default_insert_mode, Fset_visible, Fget_visible,
  Fset_object_font_style, Fget_object_font_style, Fcreate_checkbox, Fcheckbox_get, Fcheckbox_set, Fcreate_listbox,
  Flistbox_insert, Flistbox_update, Flistbox_retrieve, Flistbox_delete, Flistbox_count, Flistbox_cursor,
  Flistbox_set_cursor, Flistbox_allow_user_selection, Flistbox_allow_hotkey_search, Flistbox_set_multiselection,
  Flistbox_selected_line, Flistbox_line_is_selected, Flistbox_select_line, Flistbox_sort, Fget_event, Fset_focus,
  Fedit_set_scroll_offset, Fedit_get_scroll_offset, Fedit_set_modifiable, Fedit_is_modifiable, Fset_field_color,
  Fget_field_color, Fset_position, Fset_size, Fcreate_vertical_scrollbar, Fscrollbar_set_arrow_box_height,
  Fscrollbar_set_range, Fscrollbar_set_position, Fscrollbar_set_line_position_increment};
FUNCNAME current_function;

int           max_client_x, max_client_y;
bool          global_edit_insert_mode    = true;
bool          global_editbox_insert_mode = true;
bool          repaint_done;
volatile bool wm_paint_ignored;
bool          arrow_keys_reserved;   // arrow keys reserved for user actions

/*************************************************************************/

// intern events used only by WIN layer
const int _EVENT_MOUSE_CLICK_LEFT        = -0x30000;
const int _EVENT_MOUSE_CLICK_RIGHT       = -0x30001;
const int _EVENT_MENU_SELECTED           = -0x30002;
const int _EVENT_EDIT_CHANGED            = -0x30003;
const int _EVENT_CHECKBOX_CHANGED        = -0x30004;
const int _EVENT_LISTBOX_LINE_SELECTED   = -0x30005;
const int _EVENT_LISTBOX_LINE_DESELECTED = -0x30006;
const int _EVENT_BUTTON_PRESSED          = -0x30007;
const int _EVENT_MOUSE_DRAG              = -0x30008;
const int _EVENT_MOUSE_DROP              = -0x30009;
const int _EVENT_MOUSE_WHEEL             = -0x30010;
const int _EVENT_REDRAW_MAIN_WINDOW      = -0x30011;
const int _EVENT_SIGNAL                  = -0x30012;
const int _EVENT_KEYBOARD                = -0x30013;

const int C_KEY_USER_EVENT1  = 0x100001;
const int C_KEY_USER_EVENT2  = 0x100002;
const int C_KEY_USER_EVENT3  = 0x100003;

/*************************************************************************/

// critical section to protect GDI calls and also to avoid modifications of the 'pscreen' tree during drawing.

void ENTER_GI ()
{
  enter_shared_object (ref gdi.shared);
}

void LEAVE_GI ()
{
  leave_shared_object (ref gdi.shared);
}

/*************************************************************************/

void repaint_gi_screen (HDC hdc, PAINTSTRUCT ps);

/************************************************************************/

void init_gi_layer ()
{
  gi_create_objects  = create_gi_objects;
  gi_paint_screen    = repaint_gi_screen;
  gi_destroy_objects = destroy_gi_objects;

  assert (gi_init() == 0);
}

/************************************************************************/

package P_PAINT

  const int MAX_REPAINT_RECTS  = 32;

  RECT repaint_rect[MAX_REPAINT_RECTS];
  int  nb_repaint_rect = 0;

end P_PAINT;

/*************************************************************************/

/* accumulate redraw information in a buffer for later redrawing */

void add_repaint_rect (RECT r)
{
  int  i, j, count, best_count;

  best_count = -1;
  j = -1;

  for (i=0; i<nb_repaint_rect; i++)
  {
    ref RECT p = repaint_rect[i];

    count = (int)(r.top  >= p.top)  + (int)(r.bottom <= p.bottom)
          + (int)(r.left >= p.left) + (int)(r.right  <= p.right);

    if (count == 4)  /* already contained in existing rectangle */
      return;

    if (count > best_count)
    {
      j = i;
      best_count = count;
    }
  }


  /* append a new repaint rectangle, if possible */

  if (nb_repaint_rect < MAX_REPAINT_RECTS)
  {
    repaint_rect[nb_repaint_rect++] = r;
    return;
  }


  /* expand rectangle 'j' to include the new rectangle */

  {
    ref RECT p = repaint_rect[j];

    if (r.top    < p.top)     p.top    = r.top;
    if (r.left   < p.left)    p.left   = r.left;
    if (r.bottom > p.bottom)  p.bottom = r.bottom;
    if (r.right  > p.right)   p.right  = r.right;
  }
}

/*************************************************************************/

/* produce WM_PAINT messages to redraw the window */
/* set 'repaint_done' to false if there is something to redraw. */

void flush_repaint_rects ()
{
  int i;

  wm_paint_ignored = false;

  repaint_done = (nb_repaint_rect == 0);

  for (i=0; i<nb_repaint_rect; i++)
    InvalidateRect (main_hWnd, &repaint_rect[i], FALSE);

//  UpdateWindow (main_hWnd);

  nb_repaint_rect = 0;
}

/*************************************************************************/

/* assertion: the global variable 'current_function' is initialized */

void fatal_error (string message)
{
  int          size;
  string^      p;
  const string title = "FATAL ERROR\0";
  ref string   func = current_function'string;

  init_gi_layer ();

  /* make sure all earlier objects have been drawn */
  gi_set_visible (true);
  flush_repaint_rects ();

  size = 20 + func'length + strlen(message);  // enough for a trailing nul

  p = new char[size];

  strcpy (out p^, "function ");
  strcat (ref p^, func[1:func'length-1]);

  strcat (ref p^, "() :\n");
  strcat (ref p^, message);

  trace ("fatal error in win layer : %s\n", p^);

  MessageBoxA (main_hWnd, &p^, &title, MB_ICONEXCLAMATION | MB_OK);

  abort;   // program author will be notified with this
}

/*************************************************************************/

HFONT create_font (string  font_name,
                   int     font_height,
                   uint    style)
{
  char name[MAX_FONT_NAME_LENGTH];

  strcpy (out name, font_name);
  name[MAX_FONT_NAME_LENGTH-1] = nul;   // font name must never exceed 31 chars + nul

  return CreateFontA (font_height,
                      0,             /* use matching width */
                      0, 0,
                      (style & _FONT_STYLE_BOLD) != 0 ? FW_BOLD : FW_DONTCARE,
                      (DWORD)((style & _FONT_STYLE_ITALIC) != 0),
                      (DWORD)((style & _FONT_STYLE_UNDERLINED) != 0),
                      (DWORD)FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DRAFT_QUALITY,
                      DEFAULT_PITCH,
                      &name);
}

/*************************************************************************/

public int text_width_of2 (string     text,
                           FONT_STYLE font)
{
  int        length;
  char       font_name[MAX_FONT_NAME_LENGTH];
  int        font_height;
  HDC        hdc;
  HFONT      new_font, old_font;
  SIZE       size;

  init_gi_layer ();

  if (font.name[0] == nul)
    strcpy (out font_name, current_font.name);
  else
    strcpy (out font_name, font.name);

  if (font.height == 0)
    font_height = current_font.height;
  else
    font_height = font.height;

  length = strlen(text);

ENTER_GI();

  new_font = create_font (font_name, font_height, font.style);
  if (new_font == 0)
  {
    trace ("error: text_width_of2() : create_font() failed\n");
    LEAVE_GI ();
    return font_height * length;
  }

  hdc = GetDC (main_hWnd);

  old_font = SelectObject (hdc, new_font);
  if (old_font == 0)
  {
    trace ("error: text_width_of2() : SelectObject() failed\n");
    size = {cx => font_height * length,
            cy => 0};
  }
  else
  {
    if (GetTextExtentPoint32A (hdc, &text, length, &size) == FALSE)
    {
      trace ("error: text_width_of2() : GetTextExtentPoint32() failed\n");
      size.cx = font_height * length;
    }

    SelectObject (hdc, old_font);
  }

  DeleteObject (new_font);
  ReleaseDC (main_hWnd, hdc);

LEAVE_GI ();

  return size.cx;
}

/*************************************************************************/

public int text_width_of (string  text,
                          string  font_name   = "",
                          int     font_height = 0)
{
  FONT_STYLE font;

  clear font;

  strcpy (out font.name, font_name);
  font.height = font_height;
  font.style  = current_font.style;
  font.color  = current_font.color;

  return text_width_of2 (text, font);
}

/*************************************************************************/

string^ new_zstring (string s)
{
  string^ p = new string (strlen(s) + 1);
  strcpy (out p^, s);
  return p;
}

/*************************************************************************/

void window_message_box (string title, string message)
{
  string^  title2   = new_zstring (title);
  string^  message2 = new_zstring (message);

  refresh_win ();

  MessageBoxA (main_hWnd, &message2^, &title2^, MB_OK);

  free title2;
  free message2;

  return;
}

/*************************************************************************/

public void message_box (string title, string message, string button)
{
  int  s, s0, nb_lines, max_width, i, x_res, y_res;
  int  title_width, button_width, width, max_height;
  bool quit;

  if (pscreen == null)     /* no screen exists yet */
  {
    window_message_box (title, message);
    return;
  }


  /* compute nb_lines & (max_width, max_height) of box */

  nb_lines  = 0;
  max_width = 0;

  s = 0;
  s0 = 0;   // index into message

  while (s < message'length && message[s] != '\0')
  {
    if (message[s] == '\n')
    {
      width = text_width_of (message[s0:s-s0]);
      if (width > max_width)
        max_width = width;
      nb_lines++;
      s0 = s+1;
    }
    s++;
  }

  width = text_width_of (message[s0:s-s0]);
  if (width > max_width)
    max_width = width;
  nb_lines++;


  /* extend max_width with width of title */

  title_width = text_width_of (title) + 4 + 8;
  if (title_width > max_width)
    max_width = title_width;


  /* extend max_width with width of button */

  button_width = text_width_of (button) + 4 + 8;
  if (button_width > max_width)
    max_width = button_width;

  max_width += 4;       /* screen border */
  max_width += 4 + 4;   /* 4 pixels for left, 4 pixels for right */

  max_height = 4                                      /* screen border */
             + (current_font.height + 4)              /* title   */
             + current_font.height
             + current_font.height * nb_lines         /* message */
             + current_font.height
             + (current_font.height + 4 + 5)          /* button  */
             + current_font.height;


  /* check available screen size */

  if (max_width > max_client_x || max_height > max_client_y)
  {
    window_message_box (title, message);
    return;
  }

  create_screen (max_client_x/2 - max_width/2, max_client_y/2 - max_height/2,
                 max_width, max_height, title);

  get_screen_resolution (out x_res, out y_res);

  _unused y_res;

  s = 0;
  s0 = 0;   // index into message

  for (i=0; i<nb_lines; i++)
  {
    create_text (4, current_font.height*(i+1),
                 x_res-4, current_font.height, 100+i);
    s = strchr (message[s0:message'length-s0], '\n');
    if (s == -1)
      s = message'length-s0;
    text_put (message[s0:s], 100+i);
    s0 += s+1;
  }

  create_button (x_res/2 - button_width/2,
                 current_font.height * (nb_lines + 2),
                 button_width,
                 current_font.height+4 + 5,
                 button,
                 10);
  quit = false;
  while (!quit)
  {
    EVENT e;
    get_event (out e);
    switch (e.typ)
    {
      case EVENT_BUTTON_PRESSED:
        quit = true;
        break;

      case EVENT_KEY_PRESSED:
        if (e.key == KEY_ESCAPE)
          quit = true;
        break;

      default:
        break;
    }
  }

  close_screen ();
}

/*************************************************************************/

public void get_desktop_resolution (out int x_res, out int y_res)
{
  x_res = GetSystemMetrics (SM_CXSCREEN);
  y_res = GetSystemMetrics (SM_CYSCREEN);
}

/*************************************************************************/

public void get_screen_borders (out int left, out int top, out int right, out int bottom)
{
  top    = GetSystemMetrics (SM_CXPADDEDBORDER) + GetSystemMetrics (SM_CYFRAME) + GetSystemMetrics (SM_CYCAPTION);
  bottom = GetSystemMetrics (SM_CXPADDEDBORDER) + GetSystemMetrics (SM_CYFRAME);
  left   = GetSystemMetrics (SM_CXPADDEDBORDER) + GetSystemMetrics (SM_CXFRAME);
  right  = GetSystemMetrics (SM_CXPADDEDBORDER) + GetSystemMetrics (SM_CXFRAME);
}

/*************************************************************************/

public void get_desktop_active_area (out int x, out int y, out int x_size, out int y_size)
{
  RECT rect;

  if (SystemParametersInfoA (SPI_GETWORKAREA, 0, (byte *)&rect, 0) == FALSE)
  {
    rect.top = 0;
    rect.left = 0;
    get_desktop_resolution (out rect.right, out rect.bottom);
  }

  x = rect.left;
  y = rect.top;

  x_size = rect.right  - rect.left;
  y_size = rect.bottom - rect.top;
}

/*************************************************************************/

void check_screen_open ()
{
  if (pscreen == null)
    fatal_error ("no previous create_screen() call");
}

/*************************************************************************/

void check_box_parameters (int x, int y, int x_size, int y_size,
                           int min_x_size, int min_y_size, int id)
{
  char str[128];

  if (x < 0 || x >= pscreen^.ox_size)
  {
    sprintf (out str, "object %d : illegal x coordinate %d  (%d .. %d)", id, x, 0, pscreen^.ox_size-1);
    fatal_error (str);
  }

  if (y < 0 || y >= pscreen^.oy_size)
  {
    sprintf (out str, "object %d : illegal y coordinate %d  (%d .. %d)", id, y, 0, pscreen^.oy_size-1);
    fatal_error (str);
  }

  if (x_size < min_x_size || x+x_size > pscreen^.ox_size)
  {
    sprintf (out str, "object %d : illegal x_size parameter %d  (%d .. %d)", id, x_size, min_x_size, pscreen^.ox_size-x);
    fatal_error (str);
  }

  if (y_size < min_y_size || y+y_size > pscreen^.oy_size)
  {
    sprintf (out str, "object %d : illegal y_size parameter %d  (%d .. %d)", id, y_size, min_y_size, pscreen^.oy_size-y);
    fatal_error (str);
  }
}

/************************************************************************/

void check_object_id (OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          i;
  char         str[128];

  if (id < 1)
    fatal_error ("object id must be >= 1");

  o = pscreen^.list;
  for (i=0; i<pscreen^.nb_objects; i++)
  {
    if (o^.id == id)
    {
      sprintf (out str, "object id %d is not unique", id);
      fatal_error (str);
    }

    o = o^.next;
  }
}

/************************************************************************/

/* examine 'text' and extract 'hotkey' after '&' */
/* create 'text2'.                               */

void extract_hotkey (string text, out string text2, out char hotkey)
{
  int i, j, length;

  length = strlen(text);

  clear text2;

  hotkey = nul;
  j      = 0;

  for (i=0; i<length; i++)
  {
    if (text[i] == '&' && i+1<length && text[i+1] != '&')
    {
      hotkey = text[i+1];
      i++;
    }

    text2[j++] = text[i];
  }
}

/**********************************************************************/

void check_object_collision (OBJECT_INFO^ obj)
{
  OBJECT_INFO^ o;
  int          i;
  int          x, y, x_size, y_size;

  x      = obj^.x;
  y      = obj^.y;
  x_size = obj^.x_size;
  y_size = obj^.y_size;

  o = pscreen^.list;
  for (i=0; i<pscreen^.nb_objects; i++)
  {
    if ((o^.typ != TYP_MENU_ITEM) && (!o^.hide))
    {
      if (x >= o^.x + o^.x_size ||    /* to the right */
          y >= o^.y + o^.y_size ||    /* under it     */
          o^.x >= x + x_size    ||    /* to the left  */
          o^.y >= y + y_size)         /* above it     */
      {
        /* ok */;
      }
      else
      {
        char str[256];
        sprintf (out str, "object %d (x=%d,y=%d,dx=%d,dy=%d) collides with object %d (x=%d,y=%d,dx=%d,dy=%d)",
                 obj^.id, x, y, x_size, y_size, o^.id, o^.x, o^.y, o^.x_size, o^.y_size);
        fatal_error (str);
      }
    }
    o = o^.next;
  }
}

/************************************************************************/

void allocate_and_append_new_object (OBJECT_INFO^ obj)
{
  OBJECT_INFO^ last;

ENTER_GI ();      /* we're about to modify the 'pscreen' tree ... */

  if (pscreen^.nb_objects == 0)    /* this is the first object */
  {
    obj^.prev = obj;
    obj^.next = obj;
    pscreen^.list = obj;
  }
  else      /* there is already a list */
  {
    last = pscreen^.list^.prev;
    last^.next = obj;
    obj^.prev = last;
    obj^.next = pscreen^.list;
    pscreen^.list^.prev = obj;
  }

  pscreen^.nb_objects++;

  if (obj^.typ != TYP_TEXT && obj^.typ != TYP_WINDOW)
  {
    pscreen^.nb_activable_objects++;
    if (pscreen^.focus == null)      /* set focus on first TABable object */
      pscreen^.focus = obj;
  }

LEAVE_GI ();
}

/************************************************************************/

OBJECT_INFO^ load_object (OBJECT_ID id)
{
  int          i;
  OBJECT_INFO^ obj;
  char         str[128];

  if (id < 1)
  {
    sprintf (out str, "object id %d must be >= 1", id);
    fatal_error (str);
  }

  obj = pscreen^.list;
  for (i=0; i<pscreen^.nb_objects; i++)
  {
    if (obj^.id == id)     /* found */
      return obj;
    obj = obj^.next;
  }

  sprintf (out str, "object id %d was not found", id);
  fatal_error (str);
  return null;  /* avoid warning */
}

/************************************************************************/

public void set_font (string font_name, int font_height)
{
  current_function = Fset_font;

  if (font_height < 1)
  {
    char str[128];
    sprintf (out str, "font_height %d must be >= 1", font_height);
    fatal_error (str);
  }

  strcpy (out current_font.name, font_name);
  current_font.name[current_font.name'length-1] = nul;  // always nul-terminated
  current_font.height = font_height;
}

/********************************************************************/

public void get_font (out string(MAX_FONT_NAME_LENGTH) font_name,
                      out int                          font_height)
{
  font_name = current_font.name;
  font_height = current_font.height;
}

/********************************************************************/

public void set_font_style (FONT_STYLE style)
{
  current_font = style;
  current_font.name[current_font.name'length-1] = nul;  // always nul-terminated
}

/********************************************************************/

public void get_font_style (out FONT_STYLE style)
{
  style = current_font;
}

/********************************************************************/

string^ allocate_string (string s)
{
  int len = strlen(s);
  return new string ' (s[0:len]);
}

/********************************************************************/

public void reset_win ()
{
  RECT rect;

  current_function = Freset_win;

  init_gi_layer ();

  gi_set_visible (visible => true);

  GetClientRect (main_hWnd, &rect);    /* query size of client area */
  add_repaint_rect (rect);             /* accumulate redraw information */

  flush_repaint_rects ();
}

/************************************************************************/

public void refresh_win ()
{
  current_function = Frefresh_win;

  init_gi_layer ();

  gi_set_visible (visible => true);
  flush_repaint_rects ();
}

/************************************************************************/

int min (int a, int b)
{
  return a < b ? a : b;
}

/************************************************************************/

int max (int a, int b)
{
  return a > b ? a : b;
}

/************************************************************************/

// used for main create_screen and after screen resizing

void set_new_screen_size (ref SCREEN_INFO s, int x_size, int y_size)
{
  int  left_border, top_border, right_border, bottom_border;
  RECT crect;

  get_screen_borders (out left_border, out top_border, out right_border, out bottom_border);

  crect = {left   => 0,
           top    => 0,
           right  => x_size - left_border - right_border,
           bottom => y_size - top_border - bottom_border};

  s.ox_size   = crect.right;
  s.x_size    = crect.right;
  max_client_x = crect.right;

  s.oy_size   = crect.bottom;
  s.y_size    = crect.bottom;
  max_client_y = crect.bottom;

  add_repaint_rect (crect);  /* accumulate redraw information */
}

/************************************************************************/

public void create_screen (int x, int y, int x_size, int y_size, string title)
{
  SCREEN_INFO^ ps = new SCREEN_INFO;
  ref SCREEN_INFO s = ps^;

  current_function = Fcreate_screen;

  s.title = allocate_string (title);

  if (pscreen == null)    /* it's the very first screen */
  {
    char old_title[260];
    int  x2      = x;
    int  y2      = y;
    int  x2_size = x_size;
    int  y2_size = y_size;

    init_gi_layer ();

    if (x2_size <= 0 || y2_size <= 0)   /* ask for maximum screen size */
      get_desktop_active_area (out x2, out y2, out x2_size, out y2_size);

    GetWindowTextA (main_hWnd, &old_title, old_title'size);
    if (strcmp (old_title, s.title^) != 0)
      gi_set_title (s.title^);

    /* set minimum size */
    x2_size = max (x2_size, GetSystemMetrics (SM_CXMIN));
    y2_size = max (y2_size, GetSystemMetrics (SM_CYMIN));

    /* setup size, position and title of main window */
    gi_move (x2, y2, x2_size, y2_size);

    set_new_screen_size (ref s, x2_size, y2_size);
  }
  else     /* it's a child screen */
  {
    RECT crect;

    if (x < 0 || y < 0 || x_size < 0 || y_size < 0 ||
        x + x_size > max_client_x || y + y_size > max_client_y)
      fatal_error ("bad coordinates");

    s.x            = x;
    s.y            = y;
    s.x_size       = x_size;
    s.y_size       = y_size;
    s.title_height = current_font.height + 4;

    s.ox           = x + 2;
    s.oy           = y + 2 + s.title_height;
    s.ox_size      = x_size - 4;
    s.oy_size      = y_size - 4 - s.title_height;

    crect = {left   => x,
             top    => y,
             right  => x + x_size,
             bottom => y + y_size};

    add_repaint_rect (crect);    /* accumulate redraw information */
  }


  /* save font for title */

  s.font = current_font;


  /* save old settings */

  s.old_font                = current_font;
  s.old_edit_insert_mode    = global_edit_insert_mode;
  s.old_editbox_insert_mode = global_editbox_insert_mode;


  /* link in tree */

ENTER_GI ();
  ps^.parent_screen = pscreen;
  pscreen = ps;
LEAVE_GI ();
}

/*************************************************************************/

public void set_screen_title (string title)
{
  string^ s;
  RECT    rect;

  current_function = Fset_screen_title;

  check_screen_open ();

  s = allocate_string (title);

ENTER_GI ();
  free (pscreen^.title);
  pscreen^.title = s;
LEAVE_GI ();

  if (pscreen^.parent_screen == null)   /* main window */
  {
    gi_set_title (title);
  }
  else
  {
    GetClientRect (main_hWnd, &rect);
    add_repaint_rect (rect);  /* accumulate redraw information */
  }
}

/*************************************************************************/

public void get_screen_resolution (out int x_res, out int y_res)
{
  current_function = Fget_screen_resolution;

  check_screen_open ();

  x_res = pscreen^.ox_size;
  y_res = pscreen^.oy_size;
}

/*************************************************************************/

/* repaint object of current screen */

void repaint_object (OBJECT_INFO o)
{
  RECT rect;

  rect = {left   => pscreen^.ox + o.x,
          top    => pscreen^.oy + o.y,
          right  => pscreen^.ox + o.x + o.x_size,
          bottom => pscreen^.oy + o.y + o.y_size};

  add_repaint_rect (rect);         /* accumulate redraw information */
}

/*************************************************************************/

public void create_button (int x, int y, int x_size, int y_size, string text, OBJECT_ID id)
{
  int          length;
  OBJECT_INFO^ po = new OBJECT_INFO (TYP_BUTTON);
  ref OBJECT_INFO (TYP_BUTTON) o = po^;
  string^      text2;
  char         hotkey;

  current_function = Fcreate_button;
  check_screen_open ();
  check_box_parameters (x, y, x_size, y_size, 5, 5, id);

  if (y_size < 4 + current_font.height)
    fatal_error ("y_size is too small");

  check_object_id (id);

  length = strlen(text);
  text2 = new char[length];

  extract_hotkey (text, out text2^, out hotkey);


  /* initialize object */
  o.id     = id;
  o.x      = x;
  o.y      = y;
  o.x_size = x_size;
  o.y_size = y_size;

//  o.button.pressed = false;

  o.font   = current_font;
  o.text   = text2;
  o.hotkey = hotkey;
//  o.hide   = false;

  check_object_collision (po);
  allocate_and_append_new_object (po);
  repaint_object (o);
}

/*************************************************************************/

public void create_text (int x, int y, int x_size, int y_size, OBJECT_ID id)
{
  OBJECT_INFO^ po = new OBJECT_INFO (TYP_TEXT);
  ref OBJECT_INFO (TYP_TEXT) o = po^;

  current_function = Fcreate_text;
  check_screen_open ();
  check_box_parameters (x, y, x_size, y_size, 1, 1, id);

  if (y_size < current_font.height)
    fatal_error ("y_size is too small");

  check_object_id (id);

  /* initialize object */
  o.id     = id;
  o.x      = x;
  o.y      = y;
  o.x_size = x_size;
  o.y_size = y_size;

  o.font  = current_font;
//  o.text  = null;
//  o.hotkey = nul;
//  o.hide   = false;

  check_object_collision (po);
  allocate_and_append_new_object (po);
  repaint_object (o);
}

/*************************************************************************/

public void text_put (string text, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  char         str[128];

  current_function = Ftext_put;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_TEXT)
  {
    sprintf (out str, "object id %d does not denote a text", id);
    fatal_error (str);
  }

ENTER_GI ();
  free (o^.text);      /* free previous text, if any */
  o^.text = allocate_string (text);
LEAVE_GI ();

  repaint_object (o^);
}

/*************************************************************************/

public void create_edit (int x, int y, int x_size, int y_size, int length, OBJECT_ID id)
{
  OBJECT_INFO^               po = new OBJECT_INFO (TYP_EDIT);
  ref OBJECT_INFO (TYP_EDIT) o = po^;

  current_function = Fcreate_edit;
  check_screen_open ();
  check_box_parameters (x, y, x_size, y_size, 5, 5, id);

  if (y_size < current_font.height + 4)
    fatal_error ("y_size is too small");

  if (length < 1 || length > 512)
    fatal_error ("'length' must be in 1 .. 512");

  check_object_id (id);

  /* initialize object */
  o.id     = id;
  o.x      = x;
  o.y      = y;
  o.x_size = x_size;
  o.y_size = y_size;
  o.font   = current_font;
//  o.text   = null;
//  o.hotkey = nul;
//  o.hide   = false;

  check_object_collision (po);

//  o.edit.scroll_x_offset = 0;
//  o.edit.password_mode   = false;

  edit_line_allocate (out o.edit.edit_line,
                          line                => new wchar[length], // preallocated current line
                          length              => 0,                 // active length of line, has no trailing spaces
                          col                 => 0,                 // in range 0 ..  line^'length-1, or 0 .. line^.length if extra_col.
                          extra_col           => true,              // can cursor go past last col
                          modify_allowed      => true,              // false = line may not change
                          line_modified       => false,             // true = line was changed
                          insert_mode         => global_edit_insert_mode,         // false = delete, true = insert
                          switch_mode_allowed => true,              // false = no switch allowed
                          set_marks_allowed   => false,             // true = marks can be set
                          marks_modified      => false,             // true = marks were modified
                          first               => {active => false, col => 0},               // .col included
                          last                => {active => false, col => 0});

  allocate_and_append_new_object (po);
  repaint_object (o);
}

/**********************************************************************/

void intern_edit_line_get_line (EDIT_LINE edit_line, out string line, char filler = ' ')
{
  wstring^ s = new wchar[edit_line_get_max_length(edit_line)];
  int      i;

  edit_line_get_line (edit_line, out s^, filler => (wchar)(uint)filler);

  clear line;
  for (i=0; i<s^'length; i++)
    line[i] = (char)(uint)s^[i];

  free s;
}

/**********************************************************************/

void intern_edit_line_set_line (ref EDIT_LINE edit_line, string line)
{
  wstring^ s = new wchar[edit_line_get_max_length(edit_line)];
  int      i;

  for (i=0; i<line'length; i++)
    s^[i] = (wchar)(uint)line[i];

  edit_line_set_line (ref edit_line, s^);

  free s;
}

/**********************************************************************/

public void edit_get (out string buffer, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_get;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  {
    ref EDIT_LINE e = o^.edit.edit_line;

    if (buffer'length < edit_line_get_max_length(e))
      fatal_error ("buffer_size is too small");

    intern_edit_line_get_line (e, out buffer, filler => nul);
  }
}

/**********************************************************************/

public void edit_get2 (out string buffer, OBJECT_ID id)
{
  int i, j, len;

  edit_get (out buffer, id);

  i = 0;
  while (i < buffer'length && buffer[i] == ' ')
    i++;

  j = strlen(buffer);
  while (j > i && buffer[j-1] == ' ')
    j--;

  len = j-i;

  buffer[0:len] = buffer[i:len];
  buffer[len:buffer'length-len] = {all => nul};
}

/**********************************************************************/

public void edit_put (string text, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          length;

  current_function = Fedit_put;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  length = strlen(text);

  {
    ref EDIT_LINE e = o^.edit.edit_line;

    if (length > edit_line_get_max_length(e))
      fatal_error ("text is too long");

ENTER_GI ();
    o^.edit.scroll_x_offset = 0;

    intern_edit_line_set_line (ref e, text[0:length]);
    edit_line_set_col (ref e, col => 0);
LEAVE_GI ();
  }

  repaint_object (o^);
}

/**********************************************************************/

/* redraw an empty slice that will cause a WM_PAINT message */
/* effect: the caret position will be updated */

void redraw_caret (OBJECT_INFO o)
{
  RECT rect;

  clear rect;

  rect.left   = pscreen^.ox + o.x;
  rect.top    = pscreen^.oy + o.y;
  rect.right  = rect.left + 1;
  rect.bottom = rect.top  + o.y_size;

  add_repaint_rect (rect);
}

/**********************************************************************/

// called:
// - after treating a key,
// - after clicking on an edit field,
// - after changing the cursor position.

/* effect: adapt scroll_x_offset and redraw the edit field if needed */

/* this function must be called within critical section, as it modifies the edit object and calls GDI functions.   */

void update_scroll_x (ref OBJECT_INFO o)
{
  ref EDIT_LINE e = o.edit.edit_line;
  string^       line = new char[edit_line_get_max_length(e)];
  int           i;
  HDC           hdc;
  SIZE          size;
  HFONT         new_font, old_font;

  /* fill buffer with line + trailing spaces */
  intern_edit_line_get_line (e, out line^, filler => ' ');

  if (o.edit.password_mode)
  {
    ref string ligne = line^;
    for (i=0; i<line^'length; i++)
      if (ligne[i] != ' ')
        ligne[i] = '*';
  }

  if (edit_line_get_col(e) == o.edit.scroll_x_offset)   /* no scroll */
  {
    redraw_caret (o);
    free line;
    return;
  }

  if (edit_line_get_col(e) < o.edit.scroll_x_offset)    /* scroll left */
  {
    o.edit.scroll_x_offset = edit_line_get_col(e);
    repaint_object (o);
    free line;
    return;
  }

  // assert: e->col > o->var.edit.scroll_x_offset

  /* test if we need to scroll to the right */

  hdc = GetDC (main_hWnd);

  new_font = create_font (o.font.name, o.font.height, o.font.style);

  old_font = SelectObject (hdc, new_font);

  GetTextExtentPoint32A (hdc, &line^[o.edit.scroll_x_offset],
                         edit_line_get_col(e) - o.edit.scroll_x_offset, &size);

  if (size.cx < o.x_size - 4 - 2)   /* no scroll needed */
  {
    SelectObject (hdc, old_font);
    DeleteObject (new_font);
    ReleaseDC (main_hWnd, hdc);

    redraw_caret (o);

    free line;
    return;
  }

  for (;;)
  {
    o.edit.scroll_x_offset++;

    if (o.edit.scroll_x_offset == edit_line_get_col(e))
      break;

    GetTextExtentPoint32A (hdc, &line^[o.edit.scroll_x_offset],
                           edit_line_get_col(e) - o.edit.scroll_x_offset, &size);

    if (size.cx < o.x_size - 4 - 2)   /* all contained within field */
      break;
  }

  SelectObject (hdc, old_font);
  DeleteObject (new_font);
  ReleaseDC (main_hWnd, hdc);

  repaint_object (o);

  free line;
}

/**********************************************************************/

public void edit_set_cursor (int cursor_x, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          limit;

  current_function = Fedit_set_cursor;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  {
    ref EDIT_LINE e = o^.edit.edit_line;

    limit = edit_line_get_max_length(e) + 1;

    if (cursor_x < 1 || cursor_x > limit)
      fatal_error ("illegal value for cursor_x");

ENTER_GI ();
    edit_line_set_col (ref e, col => cursor_x - 1);
    update_scroll_x (ref o^);
LEAVE_GI ();
  }
}

/**********************************************************************/

/* returns cursor position within edit field (range 1 to length+1) */

public int edit_get_cursor (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_get_cursor;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  return 1 + edit_line_get_col (o^.edit.edit_line);
}

/**********************************************************************/

/* 'scroll_x' must be in range 0 to length, it indicates the nb of chars hidden on the left of the edit field */
/* this function should be called after setting the cursor position with edit_set_cursor(), */
/* it will adapt scroll_x in case the cursor is not visible */

public void edit_set_scroll_offset (int scroll_x, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_set_scroll_offset;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

ENTER_GI ();
  o^.edit.scroll_x_offset = max (0, scroll_x);
  update_scroll_x (ref o^);
LEAVE_GI ();

  repaint_object (o^);
}

/**********************************************************************/

/* returns scroll_x of edit field, it indicates the nb of chars hidden on the left of the edit field */

public int edit_get_scroll_offset (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_get_scroll_offset;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  return o^.edit.scroll_x_offset;
}

/**********************************************************************/

public void edit_set_insert_mode (bool      insert,    /* true=insert, false=delete */
                                  OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_set_insert_mode;
  check_screen_open ();
  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  edit_line_set_insert_mode (ref o^.edit.edit_line, insert_mode => insert);
}

/**********************************************************************/

public bool edit_get_insert_mode (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_get_insert_mode;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  return edit_line_get_insert_mode (o^.edit.edit_line);
}

/**********************************************************************/

public void edit_set_password_mode (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_set_password_mode;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  o^.edit.password_mode = true;

  repaint_object (o^);
}

//--------------------------------------------------------------------

public
void edit_set_modifiable (bool      boolean,  /* true=modify allowed, false=not allowed */
                          OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_set_modifiable;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  edit_line_set_modify_allowed (ref o^.edit.edit_line, modify_allowed => boolean);
}

/**********************************************************************/

// returns: true=modify allowed, false=not allowed

public bool edit_is_modifiable (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fedit_is_modifiable;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote an edit field", id);
    fatal_error (str);
  }

  return edit_line_get_modify_allowed (o^.edit.edit_line);
}

/**********************************************************************/

public void set_field_color (uint field_color, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fset_field_color;
  check_screen_open ();

  o = load_object (id);

  o^.field_color = field_color;
  o^.explicit_field_color = true;

  repaint_object (o^);
}

/**********************************************************************/

public uint get_field_color (OBJECT_ID id)
{
  OBJECT_INFO^ o;
  uint         screen, field;

  current_function = Fget_field_color;
  check_screen_open ();

  o = load_object (id);

  _win_get_background_colors (out screen, out field);
  _unused screen;

  return o^.explicit_field_color ? o^.field_color : field;
}

/**********************************************************************/

public void set_position (int x, int y, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fset_position;
  check_screen_open ();

  o = load_object (id);

  o^.x = x;
  o^.y = y;

  repaint_object (o^);
}

/**********************************************************************/

public void set_size (int x_size, int y_size, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fset_size;
  check_screen_open ();

  o = load_object (id);

  repaint_object (o^);

  if (o^.x_size == x_size && o^.y_size == y_size)
    return;

  o^.x_size = x_size;
  o^.y_size = y_size;

  switch (o^.typ)
  {
    case TYP_LISTBOX:
      o^.listbox.nb_screen_lines = (o^.y_size - 4) / o^.font.height;
      break;

    case TYP_WINDOW:
      {
        HDC  hdc;
        HBITMAP hbitmap;

ENTER_GI ();
        SelectObject (o^.window.hdcMemory, o^.window.old_hbitmap);
        DeleteObject (o^.window.hbitmap);

        if (x_size < 1 || y_size < 1)
          fatal_error ("x_size/y_size is too small");

        hdc = GetDC (main_hWnd);
        hbitmap = CreateCompatibleBitmap (hdc, x_size, y_size);
        ReleaseDC (main_hWnd, hdc);

        o^.window.hbitmap = hbitmap;
        o^.window.old_hbitmap = SelectObject (o^.window.hdcMemory, hbitmap);
LEAVE_GI ();
      }
      break;

    default:
      break;
  }

  repaint_object (o^);
}

/**********************************************************************/

public void create_window (int x, int y, int x_size, int y_size, OBJECT_ID id)
{
  OBJECT_INFO^                 po = new OBJECT_INFO (TYP_WINDOW);
  ref OBJECT_INFO (TYP_WINDOW) o = po^;
  HDC          hdc, hdcMemory;
  HBITMAP      hbitmap;

  current_function = Fcreate_window;
  check_screen_open ();
  check_box_parameters (x, y, x_size, y_size, 1, 1, id);

  check_object_id (id);

  /* initialize object */
  o.id     = id;
  o.x      = x;
  o.y      = y;
  o.x_size = x_size;
  o.y_size = y_size;
  o.font   = current_font;
//  o.text   = null;
//  o.hotkey = nul;
//  o.hide   = false;

  o.window.x_size = x_size;
  o.window.y_size = y_size;
  o.window.ofs_x  = 0;
  o.window.ofs_y  = 0;
  o.window.rgb_color = 0x8080FF;  /* blue */
  o.window.refresh = true;

  check_object_collision (po);

ENTER_GI ();

  hdc = GetDC (main_hWnd);
  hbitmap = CreateCompatibleBitmap (hdc,
                                    o.window.x_size,
                                    o.window.y_size);
  if (hbitmap == 0)
  {
    ReleaseDC (main_hWnd, hdc);
LEAVE_GI ();
    fatal_error ("CreateCompatibleBitmap() failed");
  }

  hdcMemory = CreateCompatibleDC (hdc);

  ReleaseDC (main_hWnd, hdc);

  if (hdcMemory == 0)
  {
LEAVE_GI ();
    fatal_error ("CreateCompatibleDC() failed");
  }

  o.window.old_hbitmap = SelectObject (hdcMemory, hbitmap);

LEAVE_GI ();

  o.window.hbitmap   = hbitmap;
  o.window.hdcMemory = hdcMemory;

  allocate_and_append_new_object (po);
}

/**********************************************************************/

public void window_get_resolution (out int x, out int y, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fwindow_get_resolution;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
  {
    char str[128];
    sprintf (out str, "object id %d does not denote a window", id);
    fatal_error (str);
  }

  x = o^.window.x_size;
  y = o^.window.y_size;
}

/**********************************************************************/

public void window_set_refresh_mode (bool flag, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fwindow_set_refresh_mode;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
    fatal_error ("object id does not denote a window");

  o^.window.refresh = flag;
}

/**********************************************************************/

public void window_set_color (byte[3] rgb, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fwindow_set_color;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
    fatal_error ("object id does not denote a window");

  o^.window.rgb_color'byte[0:3] = rgb;
}

/**********************************************************************/

public void window_pixel (int x, int y, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          xs, ys;
  RECT         rect;

  current_function = Fwindow_pixel;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
    fatal_error ("object id does not denote a window");

  xs = o^.window.x_size;
  ys = o^.window.y_size;

  if (x < 0 || y < 0 || x >= xs || y >= ys)
    fatal_error ("invalid coordinates");

ENTER_GI ();
  SetPixelV (o^.window.hdcMemory, x, y, o^.window.rgb_color);
LEAVE_GI ();

  clear rect;
  rect.left   = pscreen^.ox + o^.x + o^.window.ofs_x + x;
  rect.top    = pscreen^.oy + o^.y + o^.window.ofs_y + y;
  rect.right  = rect.left+1;
  rect.bottom = rect.top+1;

  add_repaint_rect (rect);         /* accumulate redraw information */
}

/**********************************************************************/

public void window_line (int x1, int y1, int x2, int y2, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          xs, ys, dx, dy;
  HPEN         hpen, oldhpen;
  POINT        pt[2];
  RECT         rect;

  current_function = Fwindow_line;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
    fatal_error ("object id does not denote a window");

  xs = o^.window.x_size;
  ys = o^.window.y_size;

  if (x1 < 0 || y1 < 0 || x1 >= xs || y1 >= ys ||
      x2 < 0 || y2 < 0 || x2 >= xs || y2 >= ys)
    fatal_error ("invalid coordinates");

ENTER_GI ();

  hpen = CreatePen (PS_SOLID, 1, o^.window.rgb_color);
  oldhpen = SelectObject (o^.window.hdcMemory, hpen);

  pt[0].x = x1;
  pt[0].y = y1;
  pt[1].x = x2;
  pt[1].y = y2;

  Polyline (o^.window.hdcMemory, &pt, 2);

  SetPixelV (o^.window.hdcMemory, x2, y2, o^.window.rgb_color);

  SelectObject (o^.window.hdcMemory, oldhpen);
  DeleteObject (hpen);

LEAVE_GI ();

  dx = pscreen^.ox + o^.x + o^.window.ofs_x;
  dy = pscreen^.oy + o^.y + o^.window.ofs_y;

  clear rect;
  rect.left   = dx + min(x1,x2);
  rect.right  = dx + max(x1,x2)+1;
  rect.top    = dy + min(y1,y2);
  rect.bottom = dy + max(y1,y2)+1;

  add_repaint_rect (rect);         /* accumulate redraw information */
}

/**********************************************************************/

public void window_box (int x1, int y1, int x2, int y2, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          xs, ys, dx, dy, ya, yb, yy;
  HPEN         hpen, oldhpen;
  POINT        pt[2];
  RECT         rect;

  current_function = Fwindow_box;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
    fatal_error ("object id does not denote a window");

  xs = o^.window.x_size;
  ys = o^.window.y_size;

  if (x1 < 0 || y1 < 0 || x1 >= xs || y1 >= ys ||
      x2 < 0 || y2 < 0 || x2 >= xs || y2 >= ys)
    fatal_error ("invalid coordinates");

  if (y2 < y1)
  {
    ya = y2;
    yb = y1;
  }
  else
  {
    ya = y1;
    yb = y2;
  }

ENTER_GI ();

  hpen = CreatePen (PS_SOLID, 1, o^.window.rgb_color);
  oldhpen = SelectObject (o^.window.hdcMemory, hpen);

  for (yy=ya; yy<=yb; yy++)
  {
    pt[0].x = x1;
    pt[0].y = yy;
    pt[1].x = x2;
    pt[1].y = yy;

    Polyline (o^.window.hdcMemory, &pt, 2);

    SetPixelV (o^.window.hdcMemory, x2, yy, o^.window.rgb_color);
  }

  SelectObject (o^.window.hdcMemory, oldhpen);
  DeleteObject (hpen);

LEAVE_GI ();

  dx = pscreen^.ox + o^.x + o^.window.ofs_x;
  dy = pscreen^.oy + o^.y + o^.window.ofs_y;

  clear rect;
  rect.left   = dx + min(x1,x2);
  rect.right  = dx + max(x1,x2)+1;
  rect.top    = dy + ya;
  rect.bottom = dy + yb+1;

  add_repaint_rect (rect);         /* accumulate redraw information */
}

/**********************************************************************/

public void window_text (int x, int y, string text, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          xs, ys, dx, dy, length;
  HFONT        new_font, old_font;
  SIZE         size;
  RECT         rect;
  COLORREF     old_color;
  int          old_back_mode;

  current_function = Fwindow_text;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
    fatal_error ("object id does not denote a window");

  xs = o^.window.x_size;
  ys = o^.window.y_size;

  if (x < 0 || y < 0 || x >= xs || y >= ys || y + current_font.height > ys)
    fatal_error ("invalid coordinates");

  length = strlen(text);

ENTER_GI ();

  new_font = create_font (current_font.name,
                          current_font.height,
                          current_font.style);

  old_font = SelectObject (o^.window.hdcMemory, new_font);

  GetTextExtentPoint32A (o^.window.hdcMemory, &text, length, &size);

  old_color = SetTextColor (o^.window.hdcMemory, o^.window.rgb_color);
  old_back_mode = SetBkMode (o^.window.hdcMemory, TRANSPARENT);

  TextOutA (o^.window.hdcMemory, x, y, &text, length);

  SetBkMode    (o^.window.hdcMemory, old_back_mode);
  SetTextColor (o^.window.hdcMemory, old_color);
  SelectObject (o^.window.hdcMemory, old_font);

  DeleteObject (new_font);

LEAVE_GI ();

  dx = pscreen^.ox + o^.x + o^.window.ofs_x;
  dy = pscreen^.oy + o^.y + o^.window.ofs_y;

  clear rect;
  rect.left   = dx + x;
  rect.top    = dy + y;
  rect.right  = rect.left + size.cx;
  rect.bottom = rect.top  + size.cy;

  add_repaint_rect (rect);         /* accumulate redraw information */
}

/**********************************************************************/

public void window_raster (int x, int y, int size_x, int size_y, byte[] raster, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          xs, ys, dx, dy;
  RECT         rect;
  HDC          hdc, hdcPicture;
  HBITMAP      hbitmapPict, hbitmapPictOld;
  byte         *bits;

  packed struct INFO
  {
    BITMAPINFOHEADER bmiHeader;
    DWORD            bmiColors[3];
  }

  INFO info;

  current_function = Fwindow_raster;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_WINDOW)
    fatal_error ("object id does not denote a window");

  xs = o^.window.x_size;
  ys = o^.window.y_size;

  if (x < 0 || y < 0 || x >= xs || y >= ys)
    fatal_error ("invalid coordinates");

  if (x + size_x > xs || y + size_y > ys)
    fatal_error ("x_size, y_size is too large");

  if (size_x <= 0 || size_y <= 0)
    return;

ENTER_GI ();

  hdc = GetDC (main_hWnd);
  hdcPicture = CreateCompatibleDC (hdc);
  ReleaseDC (main_hWnd, hdc);
  if (hdcPicture == 0)
  {
LEAVE_GI ();
    {
      const string title = "window_raster()\0";
      const string msg = "CreateCompatibleDC() failed\0";

      MessageBoxA (main_hWnd, &msg, &title, MB_ICONEXCLAMATION|MB_OK);

      trace ("error: %s : %s\n", title, msg);
    }
    return;
  }

  info.bmiHeader.biSize          = BITMAPINFOHEADER'size;
  info.bmiHeader.biWidth         = size_x;
  info.bmiHeader.biHeight        = -size_y;
  info.bmiHeader.biPlanes        = 1;
  info.bmiHeader.biBitCount      = 32;
  info.bmiHeader.biCompression   = BI_RGB;  /* BI_BITFIELDS; */
  info.bmiHeader.biSizeImage     = (uint)(4 * size_x * size_y);
  info.bmiHeader.biXPelsPerMeter = 1024;
  info.bmiHeader.biYPelsPerMeter = 1024;
  info.bmiHeader.biClrUsed       = 0;
  info.bmiHeader.biClrImportant  = 0;

  hbitmapPict = CreateDIBSection (0,
                                  (byte *)&info,
                                  DIB_RGB_COLORS,
                                  &bits,
                                  0,
                                  0);
  if (hbitmapPict == 0)
  {
    DeleteDC (hdcPicture);
LEAVE_GI ();
    {
      const string title = "window_raster()\0";
      const string msg = "CreateDIBSection() failed\0";

      MessageBoxA (main_hWnd, &msg, &title, MB_ICONEXCLAMATION|MB_OK);

      trace ("error: %s : %s\n", title, msg);
    }
    return;
  }

  bits[0:info.bmiHeader.biSizeImage] = raster;

  /* swap the red and blue bytes of each int4 */

  {
    int  nb_pixels, i;
    byte temp, p*;

    nb_pixels = size_x * size_y;
    p = bits;
    for (i=0; i<nb_pixels; i++)
    {
      temp     = p[i*4];
      p[i*4]   = p[i*4+2];
      p[i*4+2] = temp;
    }
  }

  hbitmapPictOld = SelectObject (hdcPicture, hbitmapPict);

  BitBlt (o^.window.hdcMemory, x, y, size_x, size_y, hdcPicture, 0, 0, SRCCOPY);

  SelectObject (hdcPicture, hbitmapPictOld);

  DeleteDC     (hdcPicture);
  DeleteObject (hbitmapPict);

LEAVE_GI ();

  dx = pscreen^.ox + o^.x + o^.window.ofs_x;
  dy = pscreen^.oy + o^.y + o^.window.ofs_y;

  clear rect;
  rect.left   = dx + x;
  rect.top    = dy + y;
  rect.right  = rect.left + size_x;
  rect.bottom = rect.top  + size_y;

  add_repaint_rect (rect);         /* accumulate redraw information */
}

/**********************************************************************/

public void set_signal (int signal_nr)
{
  gi_set_signal (signal_nr);
}

/**********************************************************************/

void unpress_all_buttons ()
{
  OBJECT_INFO^ o;
  int          i;
  bool         wait, left, middle, right;

  wait = false;
  o = pscreen^.list;
  for (i=0; i<pscreen^.nb_objects; i++)
  {
    if (o^.typ == TYP_BUTTON && o^.button.pressed)
    {
      wait = true;
      break;
    }
    o = o^.next;
  }

  if (wait)    /* wait until mouse buttons are released */
  {
    for (;;)
    {
      gi_get_mouse_buttons (out left, out middle, out right);

      _unused middle;

      if (!(left || right))             /* all buttons released */
        break;
      gi_wait (WAIT_FOR_MOUSE, 0);   /* blocking */
    }

    o = pscreen^.list;
    for (i=0; i<pscreen^.nb_objects; i++)
    {
      if (o^.typ == TYP_BUTTON && o^.button.pressed)
      {
        o^.button.pressed = false;
        repaint_object (o^);
      }
      o = o^.next;
    }

    /* produce WM_REPAINT messages to redraw the window */
    flush_repaint_rects ();
  }
}

/**********************************************************************/

package LOCAL_GET_EVENT

  int g_repeat_click;      // is incremented when clicking on listbox scrollbar
  int g_mouse_wheel_y;     // wheel position (after _EVENT_MOUSE_WHEEL)
  int g_local_key;         // key pressed (after _EVENT_KEYBOARD)
  int g_local_signal_nr;   // signal triggered (after _EVENT_SIGNAL)

  WIN_IDLE_CALLBACK_ROUTINE  intern_win_idle_callback_func;
  uint                       intern_idle_timeout;

  int local_get_event ();

end LOCAL_GET_EVENT;

/**********************************************************************/

package body LOCAL_GET_EVENT

  bool old_buttons, old_left;
  int  last_mouse_x, last_mouse_y;
  int  active_events;     // mask of not yet treated events

public int local_get_event ()
{
  bool  left, middle, right, new_buttons, click, drag, drop;
  int   mouse_x, mouse_y, mouse_wheel;
  uint  trigger_count = 0;

  init_gi_layer ();

  gi_set_mouse_arrow (false);      /* normal */

  /* produce WM_PAINT messages to redraw the window */
  flush_repaint_rects ();

  unpress_all_buttons ();


  // compute trigger_count (timeout when to generate new mouse click)
  if (g_repeat_click > 0)
  {
    trigger_count = GetTickCount();

    if (g_repeat_click == 1)
      trigger_count += 500;
    else
      trigger_count += 50;
  }

  for (;;)
  {
    if (active_events == 0)
    {
      int  wait;
      uint timeout;

      wait    = WAIT_FOR_KEYBOARD | WAIT_FOR_SIGNAL | WAIT_FOR_MOUSE | WAIT_FOR_RESIZE;
      timeout = 0;

      if (g_repeat_click > 0)      // add a timeout for repeat click
      {
        wait |= WAIT_FOR_TIMEOUT;
        timeout = trigger_count - GetTickCount();
        if (timeout > 86400)
          timeout = 0;
      }

      if (intern_win_idle_callback_func != null)
      {
        uint mdelay;

        mdelay = intern_idle_timeout - GetTickCount();

        if (mdelay > 86400)  /* timer elapsed */
        {
          mdelay = (intern_win_idle_callback_func) ();   // calls user idle function

          if (mdelay != 0)
            intern_idle_timeout = GetTickCount() + mdelay;
          else
            intern_win_idle_callback_func = null;
        }

        if (intern_win_idle_callback_func != null)
        {
          if ((wait & WAIT_FOR_TIMEOUT) == 0 || mdelay < timeout)
            timeout = mdelay;
          wait |= WAIT_FOR_TIMEOUT;
        }
      }

      active_events = gi_wait (wait, timeout);
    }


    if ((active_events & WAIT_FOR_TIMEOUT) != 0)
    {
      active_events &= (~WAIT_FOR_TIMEOUT);

      if (g_repeat_click > 0 && GetTickCount() > trigger_count)
      {
        gi_set_mouse_arrow (true);      /* hourglass */
        return old_left ? _EVENT_MOUSE_CLICK_LEFT : _EVENT_MOUSE_CLICK_RIGHT;
      }
    }


    if ((active_events & WAIT_FOR_KEYBOARD) != 0)
    {
      active_events &= (~WAIT_FOR_KEYBOARD);

      if (gi_is_key_available ())
      {
        g_local_key = gi_get_key ();
        gi_set_mouse_arrow (true);      /* hourglass */
        return _EVENT_KEYBOARD;
      }
    }


    if ((active_events & WAIT_FOR_SIGNAL) != 0)
    {
      active_events &= (~WAIT_FOR_SIGNAL);

      if (gi_is_signal_available ())
      {
        g_local_signal_nr = gi_get_signal ();
        gi_set_mouse_arrow (true);      /* hourglass */
        return _EVENT_SIGNAL;
      }
    }


    if ((active_events & WAIT_FOR_MOUSE) != 0)
    {
      active_events &= (~WAIT_FOR_MOUSE);

      gi_get_mouse_buttons (out left, out middle, out right);
      _unused middle;
      new_buttons = (left || right);

      click = (!old_buttons) && new_buttons;
      drag  = old_buttons    && new_buttons;
      drop  = old_buttons    && (!new_buttons);

      if (!new_buttons)
        g_repeat_click = 0;

      old_buttons = new_buttons;
      old_left = left;

      gi_get_mouse_coordinates (out mouse_x, out mouse_y);
      if (mouse_x != last_mouse_x || mouse_y != last_mouse_y)
      {
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        if (drag)
        {
          gi_set_mouse_arrow (true);      /* hourglass */
          return _EVENT_MOUSE_DRAG;
        }
      }

      if (click)
      {
        gi_set_mouse_arrow (true);      /* hourglass */
        return left ? _EVENT_MOUSE_CLICK_LEFT : _EVENT_MOUSE_CLICK_RIGHT;
      }

      if (drop)
      {
        gi_set_mouse_arrow (true);      /* hourglass */
        return _EVENT_MOUSE_DROP;
      }

      gi_get_mouse_wheel (out mouse_wheel);
      if (mouse_wheel != 0)
      {
        g_mouse_wheel_y = mouse_wheel;  /* save in global variable */
        gi_set_mouse_arrow (true);      /* hourglass */
        return _EVENT_MOUSE_WHEEL;
      }
    }


    if ((active_events & WAIT_FOR_RESIZE) != 0)
    {
      active_events &= (~WAIT_FOR_RESIZE);

      gi_set_mouse_arrow (true);      /* hourglass */
      return _EVENT_REDRAW_MAIN_WINDOW;
    }
  }
}

end LOCAL_GET_EVENT;

/************************************************************************/

public void win_set_idle_callback_routine (WIN_IDLE_CALLBACK_ROUTINE func)
{
  init_gi_layer ();

  intern_win_idle_callback_func = func;
  intern_idle_timeout = GetTickCount();
  tingle_mouse_event ();
}

/************************************************************************/

void deallocate_objects (OBJECT_INFO^ obj,
                         int          nb_objects)
{
  int          i;
  OBJECT_INFO^ old, o;
  BOOL         b;

  o = obj;
  for (i=0; i<nb_objects; i++)
  {
    switch (o^.typ)
    {
#if 0
      case TYP_MENU:
        break;
      case TYP_MENU_ITEM:
        break;
#endif

      case TYP_TEXT:
        break;

      case TYP_EDIT:
        edit_line_dispose (ref o^.edit.edit_line);
        break;

      case TYP_CHECKBOX:
        break;

#if 0
      case TYP_EDITBOX:
        break;
#endif

      case TYP_LISTBOX:
        close_text (ref o^.listbox.text);
        break;

      case TYP_BUTTON:
        break;

      case TYP_WINDOW:
ENTER_GI ();
        SelectObject (o^.window.hdcMemory, o^.window.old_hbitmap);
        DeleteDC (o^.window.hdcMemory);
        b = DeleteObject (o^.window.hbitmap);
LEAVE_GI ();
        if (b == FALSE)
        {
          const string title = "deallocate_objects()\0";
          const string msg = "unable to free bitmap !\0";

          MessageBoxA (main_hWnd, &msg, &title, MB_ICONEXCLAMATION|MB_OK);
        }
        break;

      case TYP_SCROLL:
        break;

      default:
        abort;
    }

    free o^.text;

    old = o;
    o = o^.next;
    free old;
  }
}

/************************************************************************/

public void close_screen ()
{
  SCREEN_INFO^ s;
  RECT         rect;

  current_function = Fclose_screen;
  check_screen_open ();

  wm_paint_ignored = true;

ENTER_GI ();
  s = pscreen;
  pscreen = pscreen^.parent_screen;
LEAVE_GI ();

  /* restore the old settings */

  current_font               = s^.old_font;
  global_edit_insert_mode    = s^.old_edit_insert_mode;
  global_editbox_insert_mode = s^.old_editbox_insert_mode;


  /* deallocate all objects of this screen */
  deallocate_objects (s^.list, s^.nb_objects);

  /* refresh full screen */
  rect = {left   => 0,
          top    => 0,
          right  => max_client_x,
          bottom => max_client_y};
  add_repaint_rect (rect);  /* accumulate redraw information */

  free s^.title;
  free s;
}

/**********************************************************************/

public void delete_all_screen_objects ()
{
  RECT rect;

  current_function = Fdelete_all_screen_objects;
  check_screen_open ();

  wm_paint_ignored = true;

ENTER_GI ();

  /* deallocate all objects of this screen */
  deallocate_objects (pscreen^.list, pscreen^.nb_objects);

  pscreen^.nb_objects           = 0;
  pscreen^.nb_activable_objects = 0;

  pscreen^.list        = null;
  pscreen^.focus       = null;
  pscreen^.clicked_obj = null;

LEAVE_GI ();

  /* refresh full screen */
  rect = {left   => 0,
          top    => 0,
          right  => max_client_x,
          bottom => max_client_y};
  add_repaint_rect (rect);  /* accumulate redraw information */
}

/**********************************************************************/

public void edit_set_default_insert_mode (bool insert)
{
  current_function = Fedit_set_default_insert_mode;

  global_edit_insert_mode = insert;
}

/**********************************************************************/

void transfer_focus (OBJECT_INFO^ old, OBJECT_INFO^ newo);

/**********************************************************************/

public void set_visible (OBJECT_ID id, bool visible)
{
  OBJECT_INFO^ o, obj;

  current_function = Fset_visible;
  check_screen_open ();

  obj = load_object (id);
  o = obj;

  if (o^.hide == !visible)    // nothing to do
    return;

  o^.hide = !visible;

  if (o^.hide)   /* hide an object */
  {
    if (o^.typ != TYP_TEXT && o^.typ != TYP_WINDOW)
    {
      pscreen^.nb_activable_objects--;

      if (pscreen^.nb_activable_objects == 0)
      {
ENTER_GI ();
        pscreen^.focus = null;
LEAVE_GI ();
      }
      else
      {
        if (pscreen^.focus == o)
        {
          o = o^.next;
          while (o^.typ == TYP_MENU_ITEM || o^.typ == TYP_TEXT || o^.typ == TYP_WINDOW || o^.hide)
            o = o^.next;

          transfer_focus (pscreen^.focus, o);
ENTER_GI ();
          pscreen^.focus = o;
LEAVE_GI ();
        }
      }
    }
  }
  else    /* make new object visible */
  {
    if (o^.typ != TYP_TEXT && o^.typ != TYP_WINDOW)
    {
      if (pscreen^.nb_activable_objects == 0)
      {
ENTER_GI ();
        pscreen^.focus = o;
LEAVE_GI ();
        transfer_focus (null, o);
      }

      pscreen^.nb_activable_objects++;
    }
  }

  repaint_object (obj^);
}

/**********************************************************************/

public void get_visible (OBJECT_ID id, out bool visible)
{
  OBJECT_INFO^ o;

  current_function = Fget_visible;
  check_screen_open ();

  o = load_object (id);

  visible = !o^.hide;
}

/**********************************************************************/

public void set_object_font_style (FONT_STYLE style, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fset_object_font_style;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ == TYP_TEXT)
  {
    if (style.height > o^.y_size)
      fatal_error ("style .height must not exceed size_y");
  }
  else if (o^.typ == TYP_EDIT ||
           o^.typ == TYP_CHECKBOX ||
           o^.typ == TYP_BUTTON)
  {
    if (style.height > o^.y_size-4)
      fatal_error ("style .height must not exceed size_y-4");
  }
  else
  {
    fatal_error ("object id does not denote text, edit, checkbox or button");
  }


ENTER_GI ();
  o^.font = style;
LEAVE_GI ();

  repaint_object (o^);
}

/**********************************************************************/

public void get_object_font_style (out FONT_STYLE style, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fget_object_font_style;
  check_screen_open ();

  o = load_object (id);

ENTER_GI ();
  style = o^.font;
LEAVE_GI ();
}

/**********************************************************************/

// key can be KEY_USER_EVENT1, KEY_USER_EVENT2 or KEY_USER_EVENT3.

void trigger_user_event (int key)
{
  if (key < KEY_USER_EVENT1 || key > KEY_USER_EVENT3)
    fatal_error ("trigger_user_event(): bad key");

  gi_put_key (C_KEY_USER_EVENT1 + (key - KEY_USER_EVENT1));
}

/**********************************************************************/
/* checkbox */
/**********************************************************************/

public void create_checkbox (int x, int y, int x_size, int y_size,
                             int box_width, string text, OBJECT_ID id)
{
  OBJECT_INFO^ po = new OBJECT_INFO (TYP_CHECKBOX);
  ref OBJECT_INFO (TYP_CHECKBOX) o = po^;

  current_function = Fcreate_checkbox;
  check_screen_open ();

  check_box_parameters (x, y, x_size, y_size, 5, 5, id);

  if (y_size < current_font.height + 4)
    fatal_error ("y_size is too small");

  if (box_width + 4 > x_size)
    fatal_error ("box_width is too large");

  check_object_id (id);

  /* initialize object */
  o.id     = id;
  o.x      = x;
  o.y      = y;
  o.x_size = x_size;
  o.y_size = y_size;

  o.font = current_font;
  o.text = allocate_string (text);
//  o.hotkey = '\0';
//  o.hide   = false;

  check_object_collision (po);

//  o.checkbox.setting = false;
  o.checkbox.box_width = box_width;

  allocate_and_append_new_object (po);
  repaint_object (o);
}

/**********************************************************************/

public void checkbox_get (out bool setting, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fcheckbox_get;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_CHECKBOX)
    fatal_error ("object id does not denote a checkbox field");

  setting = o^.checkbox.setting;
}

/**********************************************************************/

public void checkbox_set (bool setting, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fcheckbox_set;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_CHECKBOX)
    fatal_error ("object id does not denote a checkbox field");

  o^.checkbox.setting = setting;

  repaint_object (o^);
}

/**********************************************************************/
/* listbox functions */
/**********************************************************************/

void redraw_listbox_lines (HDC          hdc,
                           SCREEN_INFO^ p,
                           OBJECT_INFO^ o,
                           int          first,
                           int          last)
{
  int     width, size, y, length, ln;
  string^ line;

  width = o^.x_size - 4
        - o^.listbox.arrow_box_width;   /* width of line */

  /* format: reference:4, selected(0/1):1, line */
  size = o^.listbox.line_length + 5;
  line = new char [size];

  for (y=0; y<o^.listbox.nb_screen_lines; y++)
  {
    ln = o^.listbox.page + (int)y;    /* text line nr to redraw */
    if (y >= first && y <= last)
    {
      /* redraw the text line 'ln' on screen line 'y' */
      if (ln >= 1 && ln <= nb_text_lines (o^.listbox.text))
      {
        retrieve_text_line (ref o^.listbox.text, ln, out line^, out length);
        line^[length:size-length] = {all => SPACE};
        if (!o^.listbox.multiple_mode)
          line^[4] = (char)(int)(o^.listbox.selected_line == ln);
      }
      else
      {
        line^ = {all => SPACE};
        line^[4] = nul;          /* not selected */
      }

      display_listbox_line
        (hdc,
         p^.ox + o^.x + 2,
         p^.oy + o^.y + 2 + y * o^.listbox.line_height,
         width,
         o^.listbox.line_height,
         line^[5:o^.listbox.line_length],
         is_selected => (!o^.listbox.multiple_mode && ln == o^.listbox.selected_line)
                        || (o^.listbox.multiple_mode && line^[4]!=nul),
         has_focus   => p^.focus == o && ln == o^.listbox.ln &&
                        o^.listbox.show_focus);
    }
  }

  free line;
}

/**********************************************************************/

// returns either 0 or an event

int select_deselect_listbox_line (OBJECT_INFO^ o)
{
  if (o^.listbox.allow_user_selection && o^.listbox.ln > 0)
  {
    if (!o^.listbox.multiple_mode)    /* SINGLE LINE */
    {
      /* single selection mode */
      if (o^.listbox.ln == o^.listbox.selected_line)
      {
        /* deselect current line */
        o^.listbox.selected_line = 0;
        repaint_object (o^);
        return _EVENT_LISTBOX_LINE_DESELECTED;
      }
      else   /* deselect old line & select new line */
      {
        int old;

        /* deselect old line */
        old = o^.listbox.selected_line;
        if (old > 0)
          o^.listbox.selected_line = 0;

        /* select new line */
        o^.listbox.selected_line = o^.listbox.ln;
        repaint_object (o^);
        return _EVENT_LISTBOX_LINE_SELECTED;
      }
    }
    else            /* MULTIPLE LINES */
    {
      string^  line;
      int      length;
      bool     b;

      /* reserve stack space for a buffer */
      line = new char[o^.listbox.line_length + 5];

ENTER_GI ();
      retrieve_text_line (ref o^.listbox.text,
                          o^.listbox.ln,
                          out line^,
                          out length);

      line^[4] = (char)(int)!(bool)(int)line^[4];

      update_text_line (ref o^.listbox.text,
                        o^.listbox.ln,
                        line^[0:length]);
LEAVE_GI ();

      repaint_object (o^);

      b = ((bool)(int)line^[4]);

      free line;

      if (b)
        return _EVENT_LISTBOX_LINE_SELECTED;
      else
        return _EVENT_LISTBOX_LINE_DESELECTED;
    }
  }

  return 0;
}

/**********************************************************************/

// returns either 0 or an event

int select_deselect_all_lb_lines (OBJECT_INFO^ o, bool select)
{
  if (o^.listbox.allow_user_selection &&
      o^.listbox.ln > 0 &&
      o^.listbox.multiple_mode)
  {
    string^  line;
    int      length, ln;

    /* reserve stack space for a buffer */
    line = new char[o^.listbox.line_length + 5];

ENTER_GI ();

    for (ln=1; ln<=nb_text_lines(o^.listbox.text); ln++)
    {
      retrieve_text_line (ref o^.listbox.text,
                          ln,
                          out line^,
                          out length);

      line^[4] = (char)(int)select;

      update_text_line (ref o^.listbox.text,
                        ln,
                        line^[0:length]);
    }

LEAVE_GI ();

    free line;

    repaint_object (o^);

    if (select)
      return _EVENT_LISTBOX_LINE_SELECTED;
    else
      return _EVENT_LISTBOX_LINE_DESELECTED;
  }

  return 0;
}

/**********************************************************************/

void listbox_up (OBJECT_INFO^ o, int nb_lines)
{
  if (o^.listbox.ln > 1)
  {
ENTER_GI ();

    o^.listbox.ln -= nb_lines;
    if (o^.listbox.ln < 1)
      o^.listbox.ln = 1;

    if (o^.listbox.ln < o^.listbox.page)
      o^.listbox.page = o^.listbox.ln;

LEAVE_GI ();

    repaint_object (o^);
  }
}

/**********************************************************************/

void listbox_page_up (OBJECT_INFO^ o)
{
  listbox_up (o, o^.listbox.nb_screen_lines);
}

/**********************************************************************/

void listbox_scroll_up (OBJECT_INFO^ o, int nb_lines)
{
  int page;

  page = o^.listbox.page - nb_lines;

  if (page < 1)
    page = 1;

  if (o^.listbox.page != page)
  {
ENTER_GI ();

    o^.listbox.page = page;

    if (o^.listbox.ln >
           o^.listbox.page + o^.listbox.nb_screen_lines - 1)
    {
      o^.listbox.ln =
            o^.listbox.page + o^.listbox.nb_screen_lines - 1;
    }

LEAVE_GI ();

    repaint_object (o^);
  }
}

/**********************************************************************/

void listbox_down (OBJECT_INFO^ o, int nb_lines)
{
  if (o^.listbox.ln > 0 &&
      o^.listbox.ln < nb_text_lines (o^.listbox.text))
  {
ENTER_GI ();

    o^.listbox.ln += nb_lines;
    if (o^.listbox.ln > nb_text_lines (o^.listbox.text))
      o^.listbox.ln = nb_text_lines (o^.listbox.text);

    if (o^.listbox.ln >
        o^.listbox.page + o^.listbox.nb_screen_lines - 1)
      o^.listbox.page = o^.listbox.ln - (o^.listbox.nb_screen_lines - 1);

    if (o^.listbox.page < 1)
      o^.listbox.page = 1;

LEAVE_GI ();

    repaint_object (o^);
  }
}

/**********************************************************************/

void listbox_page_down (OBJECT_INFO^ o)
{
  listbox_down (o, o^.listbox.nb_screen_lines);
}

/**********************************************************************/

void listbox_scroll_down (OBJECT_INFO^ o, int nb_lines)
{
  int page;

  page = o^.listbox.page + nb_lines;

  if (page + o^.listbox.nb_screen_lines - 1
        > nb_text_lines (o^.listbox.text))
  {
    page = nb_text_lines (o^.listbox.text)
            - o^.listbox.nb_screen_lines + 1;
  }

  if (page < 1)
    page = 1;

  if (o^.listbox.page != page)
  {
ENTER_GI ();

    o^.listbox.page = page;

    if (o^.listbox.ln < page)
      o^.listbox.ln = page;

LEAVE_GI ();

    repaint_object (o^);
  }
}

/**********************************************************************/

/* returns the target line, or 0 if there is none */

int search_listbox_line (ref      A_TEXT text,
                         int      starting_ln,
                         int      line_length,
                         int      hotkey_position,
                         int      hotkey,
                         out bool match)
{
  int     size, length, j;
  string^ line1, line2;
  int     ln, best;

  match = false;

  if (starting_ln > nb_text_lines (text))    /* text is empty */
    return 0;

  match = false;
  best = starting_ln;

  size = line_length + 5;

  line1 = new char[size];
  line2 = new char[size];

ENTER_GI ();

  retrieve_text_line (ref text,
                      starting_ln,
                      out line1^,
                      out length);

  line1^[length:size-length] = {all => SPACE};


  /* scan now within all lines starting with this one */

  for (ln=starting_ln; ln<=nb_text_lines (text); ln++)
  {
    retrieve_text_line (ref text,
                        ln,
                        out line2^,
                        out length);

    line2^[length:size-length] = {all => SPACE};

    /* check if the prefix is the same */
    for (j=0; j<hotkey_position; j++)
    {
      if (toupper (normalize_extended_character (line1^[5+j]))
              != toupper (normalize_extended_character (line2^[5+j])))
      {
LEAVE_GI ();
        free line1;
        free line2;
        return best;
      }
    }

    /* check if the hotkey is acceptable */
    if (toupper (normalize_extended_character (line2^[5+hotkey_position]))
        == normalize_extended_character ((char)hotkey))    /* match */
    {
      match = true;
LEAVE_GI ();
      free line1;
      free line2;
      return ln;
    }

    if (toupper (normalize_extended_character (line2^[5+hotkey_position]))
        > normalize_extended_character ((char)hotkey))     /* too far */
    {
LEAVE_GI ();
      free line1;
      free line2;
      return best;
    }

    best = ln;
  }

LEAVE_GI ();

  free line1;
  free line2;

  return best;
}

/**********************************************************************/

/* reserve arrow keys for move actions */

public void win_reserve_arrow_keys (bool reserve)
{
  arrow_keys_reserved = reserve;
}

/**********************************************************************/

// returns an event that could not be processed

int animate_listbox (OBJECT_INFO^ o)
{
  int  event, key, ln;
  int  hotkey_position = 0;
  bool match;

  for (;;)
  {
    event = local_get_event ();
    if (event != _EVENT_KEYBOARD)
      return event;

    key = g_local_key;

    if (key >= 32 && key <= 255)
    {
      o^.listbox.show_focus = true;

      if (o^.listbox.allow_hotkey_search)
      {
        if (hotkey_position > o^.listbox.line_length) /*past last column*/
          hotkey_position = 0;

        if (hotkey_position == 0)     /* search from start */
          ln = 1;
        else                          /* search from current line */
          ln = o^.listbox.ln;

        ln = search_listbox_line (ref o^.listbox.text,
                                  ln,
                                  o^.listbox.line_length,
                                  hotkey_position,
                                  (int) toupper ((char)key),
                                  out match);

        if (ln > 0)     /* some appropriate line was found */
        {
          if (match)
            hotkey_position++;
          else
            hotkey_position = 0;

          if (ln != o^.listbox.ln)
          {
ENTER_GI ();
            o^.listbox.ln = ln;
            if (ln < o^.listbox.page ||
                ln >= o^.listbox.page + o^.listbox.nb_screen_lines)
            {
              o^.listbox.page = ln;
              if (o^.listbox.page + o^.listbox.nb_screen_lines - 1
                   > nb_text_lines (o^.listbox.text))
              {
                o^.listbox.page = nb_text_lines (o^.listbox.text)
                                    - (o^.listbox.nb_screen_lines - 1);
                if (o^.listbox.page < 1)
                  o^.listbox.page = 1;
              }
            }
LEAVE_GI ();

            repaint_object (o^);
          }
        }
        else
        {
          hotkey_position = 0;
        }
      }
    }
    else
    {
      hotkey_position = 0;

      switch (key)
      {
        case C_KEY_HOME:
        case C_KEY_CTRL_PAGE_UP:
        case C_KEY_CTRL_HOME:
          o^.listbox.show_focus = true;
          if (o^.listbox.ln > 1)
          {
ENTER_GI ();
            o^.listbox.ln   = 1;
            o^.listbox.page = 1;
LEAVE_GI ();
            repaint_object (o^);
          }
          break;

        case C_KEY_END:
        case C_KEY_CTRL_PAGE_DOWN:
        case C_KEY_CTRL_END:
          o^.listbox.show_focus = true;
          if (o^.listbox.ln != nb_text_lines (o^.listbox.text))
          {
            int new_page;

ENTER_GI ();
            o^.listbox.ln = nb_text_lines (o^.listbox.text);
            if (o^.listbox.ln >= o^.listbox.page + o^.listbox.nb_screen_lines)
            {
              new_page = o^.listbox.ln - (o^.listbox.nb_screen_lines-1);
              if (new_page < 1)
                new_page = 1;

              o^.listbox.page = new_page;
            }
LEAVE_GI ();
            repaint_object (o^);
          }
          break;

        case C_KEY_CURSOR_UP:
          if (arrow_keys_reserved)
            return _EVENT_KEYBOARD;     /* return event to above layer */
          o^.listbox.show_focus = true;
          listbox_up (o, 1);
          break;

        case C_KEY_CURSOR_DOWN:
          if (arrow_keys_reserved)
            return _EVENT_KEYBOARD;     /* return event to above layer */
          o^.listbox.show_focus = true;
          listbox_down (o, 1);
          break;

        case C_KEY_PAGE_UP:
          o^.listbox.show_focus = true;
          listbox_page_up (o);
          break;

        case C_KEY_PAGE_DOWN:
          o^.listbox.show_focus = true;
          listbox_page_down (o);
          break;

        case C_KEY_ENTER:    /* select / deselect listbox line */
          o^.listbox.show_focus = true;
          event = select_deselect_listbox_line (o);
          if (event != 0)
            return event;
          break;

        case C_KEY_INSERT:
          o^.listbox.show_focus = true;
          event = select_deselect_all_lb_lines (o, true);
          if (event != 0)
            return event;
          break;

        case C_KEY_DELETE:
          o^.listbox.show_focus = true;
          event = select_deselect_all_lb_lines (o, false);
          if (event != 0)
            return event;
          break;

        default:
          return _EVENT_KEYBOARD;    /* return event to above layer */
      }
    }
  }
}

/**********************************************************************/

public void create_listbox (int x, int y, int x_size, int y_size,
                            int arrow_box_width, int arrow_box_height,
                            int length,          OBJECT_ID id)
{
  OBJECT_INFO^ po = new OBJECT_INFO (TYP_LISTBOX);
  ref OBJECT_INFO o = po^;

  current_function = Fcreate_listbox;
  check_screen_open ();

  if (x_size < 5)
    fatal_error ("size_x must be >= 5");

  if ((y_size - 4) % current_font.height != 0 ||
      (y_size - 4) / current_font.height < 1)
    fatal_error ("size_y must = 4 + font.height * nb_lines");

  if (arrow_box_width < 4 || arrow_box_width >= x_size-4)
    fatal_error ("illegal parameter 'arrow_box_width'");

  if (arrow_box_height < 4 || arrow_box_height >= y_size-4)
    fatal_error ("illegal parameter 'arrow_box_height'");

  if (length < 1 || length > 512)
    fatal_error ("illegal 'length'");

  check_box_parameters (x, y, x_size, y_size, 5, 5, id);
  check_object_id (id);

  /* initialize object */
  o.id     = id;
  o.x      = x;
  o.y      = y;
  o.x_size = x_size;
  o.y_size = y_size;
  o.font   = current_font;
//  o.text   = null;
//  o.hotkey = nul;
//  o.hide   = false;

  check_object_collision (po);

  {
    ref LISTBOX_INFO l = o.listbox;

    creat_text (out l.text);
    l.page                 = 1;
//    l.ln                   = 0;
    l.nb_screen_lines      = (y_size - 4) / current_font.height;
    l.line_length          = length;
    l.line_height          = current_font.height;
    l.scrollbar            = 1;
    l.arrow_box_width      = arrow_box_width;
    l.arrow_box_height     = arrow_box_height;
    l.allow_user_selection = true;   /* user can select lines */
    l.allow_hotkey_search  = true;   /* user can use hotkey   */
    l.multiple_mode        = false;  /* single mode           */
    l.selected_line        = 0;      /* no line selected      */
    l.show_focus           = false;

    allocate_and_append_new_object (po);
    repaint_object (o);
  }
}

/**********************************************************************/

public void listbox_insert2 (int       index,       /* 0 = append to end */
                             string    text,
                             int       reference,
                             OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          length;
  string^      line;
  int          index2;

  current_function = Flistbox_insert;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (index == 0)
    index2 = nb_text_lines (o^.listbox.text) + 1;
  else if (index < 1 || index > nb_text_lines (o^.listbox.text) + 1)
  {
    fatal_error ("index is out of range");
    abort;
  }
  else
    index2 = index;

  length = strlen(text);

  if (length > o^.listbox.line_length)
    fatal_error ("text is too long");

  /* build listbox line */
  line = new char [length + 5];

  line^[0:4]'byte = reference'byte;
//  line^[4] = nul;
  line^[5:length] = text[0:length];

ENTER_GI ();

  insert_text_line (ref o^.listbox.text, index2, line^);


  /* update the object */

  if (index2 <= o^.listbox.ln)
  {
    o^.listbox.page++;
    o^.listbox.ln++;
  }

  if (o^.listbox.ln == 0)
    o^.listbox.ln = 1;       /* current position is now defined */

  if (index2 <= o^.listbox.selected_line)
    o^.listbox.selected_line++;

LEAVE_GI ();

  free line;

  repaint_object (o^);
}

/**********************************************************************/

public void listbox_insert (int index, string text, OBJECT_ID id)
{
  listbox_insert2 (index, text, 0, id);
}

/**********************************************************************/

void intern_listbox_update (int       index,
                            string    text,
                            bool      keep_old_reference,
                            int       reference,
                            OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          length, dummy;
  string^      line;

  current_function = Flistbox_update;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (index < 1 || index > nb_text_lines (o^.listbox.text))
    fatal_error ("index is out of range");

  length = strlen(text);

  if (length > o^.listbox.line_length)
    fatal_error ("text is too long");

  /* build listbox line */
  line = new char [length + 5];

ENTER_GI ();

  retrieve_text_line (ref o^.listbox.text,
                      index,
                      out line^[0:5],  /* read only the 5 first bytes */
                      out dummy);
  _unused dummy;

  if (!keep_old_reference)
    line^[0:4]'byte = reference'byte;
  line^[5:length] = text[0:length];

  update_text_line (ref o^.listbox.text, index, line^[0:length + 5]);

LEAVE_GI ();

  free line;

  repaint_object (o^);
}

/**********************************************************************/

public void listbox_update (int index, string text, OBJECT_ID id)
{
  intern_listbox_update (index, text, true, 0, id);
}

/**********************************************************************/

public void listbox_update2 (int index, string text, int reference, OBJECT_ID id)
{
  intern_listbox_update (index, text, false, reference, id);
}

/**********************************************************************/

public void listbox_retrieve2 (int        index,
                               out string text,
                               out int    reference,
                               OBJECT_ID  id)
{
  OBJECT_INFO^ o;
  int          length;
  string^      line;

  current_function = Flistbox_retrieve;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (index < 1 || index > nb_text_lines (o^.listbox.text))
    fatal_error ("index is out of range");

  /* build listbox line */
  line = new char [o^.listbox.line_length + 5];

ENTER_GI ();

  retrieve_text_line (ref o^.listbox.text,
                      index,
                      out line^,
                      out length);
LEAVE_GI ();

  reference'byte = line^[0:4]'byte;
  strcpy (out text, line^[5:length-5]);

  free line;
}

/**********************************************************************/

public void listbox_retrieve (int index, out string text, OBJECT_ID id)
{
  int dummy;
  listbox_retrieve2 (index, out text, out dummy, id);
  _unused dummy;
}

/**********************************************************************/

public void listbox_delete (int       index,
                            OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_delete;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (index < 1 || index > nb_text_lines (o^.listbox.text))
    fatal_error ("index is out of range");

ENTER_GI ();

  delete_text_line (ref o^.listbox.text, index);


  /* update the object */

  if (o^.listbox.ln > index ||
      o^.listbox.ln > nb_text_lines (o^.listbox.text))
  {
    o^.listbox.ln--;    /* can become zero */
  }

  if (o^.listbox.page > 1 &&
      index < o^.listbox.page + o^.listbox.nb_screen_lines)
  {
    o^.listbox.page--;
  }

  if (!o^.listbox.multiple_mode)
  {
    if (o^.listbox.selected_line != 0)
    {
      if (index == o^.listbox.selected_line)
        o^.listbox.selected_line = 0;
      else if (index < o^.listbox.selected_line)
        o^.listbox.selected_line--;
    }
  }

LEAVE_GI ();

  repaint_object (o^);
}

/**********************************************************************/

public int listbox_count (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_count;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  return nb_text_lines (o^.listbox.text);
}

/**********************************************************************/

public int listbox_cursor (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_cursor;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  return o^.listbox.ln;
}

/**********************************************************************/

public void listbox_set_cursor (int index, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          new_page;

  current_function = Flistbox_set_cursor;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (index < 1 || index > nb_text_lines (o^.listbox.text))
    fatal_error ("index is out of range");

  /* update the object */

  if (index == o^.listbox.ln)    /* nothing to do */
    return;

ENTER_GI ();

  o^.listbox.ln = index;

  new_page = o^.listbox.page;
  if (index < new_page)
    new_page = index;
  else if (index > new_page + (o^.listbox.nb_screen_lines - 1))
  {
    new_page = index - (o^.listbox.nb_screen_lines - 1);
    if (new_page < 1)
      new_page = 1;
  }

  o^.listbox.page = new_page;

LEAVE_GI ();

  repaint_object (o^);
}

/**********************************************************************/

public void listbox_allow_user_selection (bool allow, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_allow_user_selection;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  /* update the object */
  o^.listbox.allow_user_selection = allow;
}

/**********************************************************************/

public void listbox_allow_hotkey_search (bool allow, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_allow_hotkey_search;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  /* update the object */
  o^.listbox.allow_hotkey_search = allow;
}

/**********************************************************************/

public void listbox_set_multiselection (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_set_multiselection;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (o^.listbox.multiple_mode)
    fatal_error ("listbox is already in multiselection mode");

  o^.listbox.multiple_mode = true;
  o^.listbox.selected_line = 0;

  repaint_object (o^);
}

/**********************************************************************/

public int listbox_selected_line (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_selected_line;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (o^.listbox.multiple_mode)
    fatal_error ("this function is not allowed in multiselection mode");

  return o^.listbox.selected_line;
}

/**********************************************************************/

public bool listbox_line_is_selected (int index, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Flistbox_line_is_selected;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (index < 1 || index > nb_text_lines (o^.listbox.text))
    fatal_error ("index is out of range");

  if (!o^.listbox.multiple_mode)
  {
    /* single mode */
    return (index == o^.listbox.selected_line);
  }
  else
  {
    /* multiple mode */
    char line[5];
    int  dummy;

ENTER_GI ();

    retrieve_text_line (ref o^.listbox.text,
                        index,
                        out line,    /* retrieve only 5 bytes */
                        out dummy);
    _unused dummy;

LEAVE_GI ();

    return (bool)(int)line[4];
  }
}

/**********************************************************************/

public void listbox_select_line (int index, bool select, OBJECT_ID id)
{
  OBJECT_INFO^ o;
  int          length;
  string^      line;

  current_function = Flistbox_select_line;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

  if (index < 1 || index > nb_text_lines (o^.listbox.text))
    fatal_error ("index is out of range");

ENTER_GI ();

  if (!o^.listbox.multiple_mode)      /* single mode */
  {
    if (select)
      o^.listbox.selected_line = index;
    else if (o^.listbox.selected_line == index)
      o^.listbox.selected_line = 0;
  }
  else           /* multiple mode */
  {
    /* build listbox line */
    line = new char [o^.listbox.line_length + 5];

    retrieve_text_line (ref o^.listbox.text,
                        index,
                        out line^,
                        out length);

    line^[4] = (char)(int)select;

    update_text_line (ref o^.listbox.text, index, line^[0:length]);

    free line;
  }

LEAVE_GI ();

  repaint_object (o^);
}

/**********************************************************************/

package K = new USER_SORT_TEXT_LINES (USER_INFO => COMPARE_LISTBOX_LINES);

//--------------------------------------------------------------------

int intern_compare (byte[]^ data1, byte[]^ data2, ref COMPARE_LISTBOX_LINES compare_listbox_lines)
{
  ref byte[] a = data1^;
  ref byte[] b = data2^;

  return compare_listbox_lines
                (a[0:4], a[4:1], a[5:a'length-5],
                 b[0:4], b[4:1], b[5:b'length-5]);
}

//------------------------------------------------------------------

// sort listbox lines using user-defined comparison function
public void listbox_sort_user (OBJECT_ID id, COMPARE_LISTBOX_LINES user_compare)
{
  OBJECT_INFO^ o;
  COMPARE_LISTBOX_LINES ref_user_compare = user_compare;

  current_function = Flistbox_sort;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_LISTBOX)
    fatal_error ("object id does not denote a listbox");

ENTER_GI ();

  // for single-line selection, store flag in selected line.
  if ((!o^.listbox.multiple_mode) && o^.listbox.selected_line != 0)
  {
    string^ line;
    int     length;

    /* build listbox line */
    line = new char [o^.listbox.line_length + 5];

    retrieve_text_line (ref o^.listbox.text,
                            o^.listbox.selected_line,
                        out line^,
                        out length);

    line^[4] = (char)1;

    update_text_line (ref o^.listbox.text,
                          o^.listbox.selected_line,
                          line^[0:length]);
    free line;
  }

  K.sort (ref o^.listbox.text, intern_compare, ref ref_user_compare);

  // for single-line selection, store flag in selected line.
  if ((!o^.listbox.multiple_mode) && o^.listbox.selected_line != 0)
  {
    string^ line;
    int     count, length, ln;

    /* build listbox line */
    line = new char [o^.listbox.line_length + 5];

    count = nb_text_lines (o^.listbox.text);

    for (ln=1; ln<=count; ln++)
    {
      retrieve_text_line (ref o^.listbox.text,
                              ln,
                          out line^,
                          out length);

      if (line^[4] != nul)   // was selected
      {
        line^[4] = nul;   // delete again flag
        update_text_line (ref o^.listbox.text, ln, line^[0:length]);
        o^.listbox.selected_line = ln;
        break;
      }
    }

    free line;
  }

LEAVE_GI ();

  repaint_object (o^);
}

//--------------------------------------------------------------------

int uppercase_extended_compare
               (int reference1, bool selected1, string line1,
                int reference2, bool selected2, string line2)
{
  string^ t1, t2;
  int     rc;

  _unused reference1;
  _unused reference2;
  _unused selected1;
  _unused selected2;

  t1 = new char [strlen(line1)];
  t2 = new char [strlen(line2)];

  normalize_extended_characters (line1, out t1^);
  normalize_extended_characters (line2, out t2^);

  rc = stricmp (t1^, t2^);

  free t1;
  free t2;

  return rc;
}

/**********************************************************************/

public void listbox_sort (OBJECT_ID id)
{
  listbox_sort_user (id, uppercase_extended_compare);
}

/**********************************************************************/

public
void create_vertical_scrollbar (int x, int y, int x_size, int y_size,
                                int arrow_box_height, OBJECT_ID id)
{
  OBJECT_INFO^ po = new OBJECT_INFO (TYP_SCROLL);
  ref OBJECT_INFO o = po^;

  current_function = Fcreate_vertical_scrollbar;
  check_screen_open ();

  if (arrow_box_height < 4 || arrow_box_height >= y_size/2)
  {
    char str[1000];
    sprintf (out str, "arrow_box_height (=%d) must be in range %d .. %d", arrow_box_height, 4, y_size/2);
    fatal_error (str);
  }

  check_box_parameters (x, y, x_size, y_size, 0, 0, id);
  check_object_id (id);

  /* initialize object */
  o.id     = id;
  o.x      = x;
  o.y      = y;
  o.x_size = x_size;
  o.y_size = y_size;
  o.font   = current_font;

  check_object_collision (po);

  {
    ref SCROLL_INFO l = o.scroll;

    l.page  = 1;
    l.range = 100;
    l.shown = 10;
    l.arrow_box_height = arrow_box_height;
    l.small_y_increment = 1;

    allocate_and_append_new_object (po);
    repaint_object (o);
  }
}

//--------------------------------------------------------------------

public void scrollbar_set_arrow_box_height (int arrow_box_height, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fscrollbar_set_arrow_box_height;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_SCROLL)
    fatal_error ("object id does not denote a scrollbar");

  repaint_object (o^);
  o^.scroll.arrow_box_height = arrow_box_height;
  repaint_object (o^);
}

//--------------------------------------------------------------------

public void scrollbar_set_range (int full_size, int shown_size, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fscrollbar_set_range;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_SCROLL)
    fatal_error ("object id does not denote a scrollbar");

  o^.scroll.range = full_size;
  o^.scroll.shown = shown_size;

  repaint_object (o^);
}

//--------------------------------------------------------------------

public void scrollbar_set_position (int pos, OBJECT_ID id)  // set top shown position
{
  OBJECT_INFO^ o;

  current_function = Fscrollbar_set_position;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_SCROLL)
    fatal_error ("object id does not denote a scrollbar");

  o^.scroll.page = 1 + pos;

  repaint_object (o^);
}

//--------------------------------------------------------------------

public void scrollbar_set_line_position_increment (int small_y_increment, OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fscrollbar_set_line_position_increment;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_SCROLL)
    fatal_error ("object id does not denote a scrollbar");

  o^.scroll.small_y_increment = small_y_increment;
}

//--------------------------------------------------------------------

// returns a key that could not be processed, or a pseudo key
// corresponding to an event to be returned to the application.

int animate_edit (OBJECT_INFO^ o)
{
  int  event, key;
  bool quit;

  // init the edit line as being not modified
  edit_line_set_line_modified (ref o^.edit.edit_line, line_modified => false);

  for (;;)
  {
    redraw_caret (o^);

    event = local_get_event ();
    if (event != _EVENT_KEYBOARD)
      return event;

    key = g_local_key;

    quit = false;

ENTER_GI ();
    if (key >= 32 && key <= 255)
      edit_line_achar (ref o^.edit.edit_line, (wchar)key);
    else
    {
      switch (key)
      {
        case C_KEY_INSERT:
          edit_line_switch_insert (ref o^.edit.edit_line);
          break;

        case C_KEY_HOME:
          edit_line_home (ref o^.edit.edit_line);
          break;

        case C_KEY_END:
          edit_line_end (ref o^.edit.edit_line);
          break;

        case C_KEY_CURSOR_LEFT:
          if (arrow_keys_reserved)
          {
            quit = true;
            break;
          }
          edit_line_cursor_left (ref o^.edit.edit_line);
          break;

        case C_KEY_CURSOR_RIGHT:
          if (arrow_keys_reserved)
          {
            quit = true;
            break;
          }
          edit_line_cursor_right (ref o^.edit.edit_line);
          break;

        case C_KEY_DELETE:
          edit_line_delete (ref o^.edit.edit_line);
          break;

        case (int)'T' - (int)'A' + 1:      // CONTROL-T
          edit_line_delete_word (ref o^.edit.edit_line);
          break;

        case C_KEY_BACKSPACE:
          edit_line_backspace (ref o^.edit.edit_line);
          break;

        case C_KEY_CTRL_CURSOR_LEFT:
          if (arrow_keys_reserved)
          {
            quit = true;
            break;
          }
          edit_line_previous_word (ref o^.edit.edit_line, begin_of_word => true);
          break;

        case C_KEY_CTRL_CURSOR_RIGHT:
          if (arrow_keys_reserved)
          {
            quit = true;
            break;
          }
          edit_line_next_word (ref o^.edit.edit_line, begin_of_word => true);
          break;

        case (int)'C' - (int)'A' + 1:      // CONTROL-C : copy to clipboard
          {
            string^ s = new char [edit_line_get_max_length (o^.edit.edit_line)];

            intern_edit_line_get_line (o^.edit.edit_line, out s^, filler => nul);

            save_data_to_clipboard (s^[0:strlen(s^)], CF_TEXT);

            free s;
          }
          break;

        case (int)'V' - (int)'A' + 1:      // CONTROL-V : paste to clipboard
          {
            byte[]^ p;
            string^ s       = new char [edit_line_get_max_length (o^.edit.edit_line)];
            string^ curline = new char [edit_line_get_max_length (o^.edit.edit_line)];
            int     i, len, tail_len, curlen, col;

            p = load_data_from_clipboard (CF_TEXT);
            if (p != null)
            {
              len = min (edit_line_get_max_length (o^.edit.edit_line), p^'length);
              s^[0 : len]'byte = p^[0 : len];
              len = strlen(s^);
              free p;

              if (edit_line_get_length (o^.edit.edit_line) + len > edit_line_get_max_length (o^.edit.edit_line))
              {
                len = edit_line_get_max_length (o^.edit.edit_line) - edit_line_get_length (o^.edit.edit_line);
              }

              for (i=0; i<len; i++)
              {
                if (s^[i] < ' ' || s^[i] == (char)127)
                  s^[i] = '?';
              }

              curlen = edit_line_get_length (o^.edit.edit_line);
              col = edit_line_get_col (o^.edit.edit_line);

              if (col > curlen)
                col = curlen;

              tail_len = curlen - col;

              intern_edit_line_get_line (o^.edit.edit_line, out curline^);

              curline^[col + len : tail_len] = curline^[col : tail_len];
              curline^[col : len] = s^[0:len];

              intern_edit_line_set_line (ref o^.edit.edit_line, curline^[0 : curlen+len]);
              edit_line_set_col (ref o^.edit.edit_line, col);

              edit_line_set_line_modified (ref o^.edit.edit_line, line_modified => true);
            }

            free s;
            free curline;
          }
          break;

        default:
          quit = true;
          break;
      }
    }

    if (quit)
    {
LEAVE_GI ();
      return _EVENT_KEYBOARD;     // return key to above layer
    }

    // possibly update scroll_x offset
    update_scroll_x (ref o^);

LEAVE_GI ();

    if (edit_line_get_line_modified (o^.edit.edit_line))
    {
      repaint_object (o^);
      return _EVENT_EDIT_CHANGED;
    }
  }
}

/**********************************************************************/

void wait_until_mouse_buttons_released ()
{
  bool left, middle, right;
  for (;;)
  {
    gi_get_mouse_buttons (out left, out middle, out right);
    _unused middle;
    if (!(left || right))             // all buttons released
      break;
    gi_wait (WAIT_FOR_MOUSE, 0);   // blocking
  }
}

/**********************************************************************/

// returns an event

int animate_button (OBJECT_INFO^ o)
{
  int event, key;

  _unused o;

  for (;;)
  {
    event = local_get_event ();
    if (event != _EVENT_KEYBOARD)
      return event;

    key = g_local_key;
    switch (key)
    {
      case C_KEY_ENTER:
        return _EVENT_BUTTON_PRESSED;

      default:
        return _EVENT_KEYBOARD;     /* return key to above layer */
    }
  }
}

/**********************************************************************/

/* returns a key that could not be processed, or a pseudo key   */
/* corresponding to an event to be returned to the application. */

int animate_checkbox (OBJECT_INFO^ o)
{
  int event, key;

  for (;;)
  {
    event = local_get_event ();
    if (event != _EVENT_KEYBOARD)
      return event;

    key = g_local_key;

    switch (key)
    {
      case 32:
        o^.checkbox.setting = !o^.checkbox.setting;
        repaint_object (o^);
        return _EVENT_CHECKBOX_CHANGED;

      default:
        return _EVENT_KEYBOARD;     /* return key to above layer */
    }
  }
}

/**********************************************************************/

void button_effect_3D (OBJECT_INFO^ o)
{
  o^.button.pressed = true;
  repaint_object (o^);

  /* produce WM_REPAINT messages to redraw the window */
  flush_repaint_rects ();

  while (!repaint_done)
    sleep 0.1;

  wait_until_mouse_buttons_released ();
}

/**********************************************************************/

void repaint_gi_screen (HDC hdc, PAINTSTRUCT ps)
{
  SCREEN_INFO^ p;
  OBJECT_INFO^ o;
  int          i, x, y, sx, sy;
  FONT_STYLE   font;
  HFONT        old_font, new_font;
  bool         done;

  if (wm_paint_ignored)
    return;

  if (colors_changed)
  {
    colors_changed = false;
    destroy_gi_objects();
    create_gi_objects ();
  }

  display_set_default_colors (hdc);

  clear font;

  old_font = 0;
  new_font = 0;

  done = false;

  p = pscreen;
  while (p != null)
  {
    /* redraw all objects of screen */

    o = p^.list;

    for (i=0; i<p^.nb_objects; i++)
    {
      if (o^.hide)
      {
        o = o^.next;
        continue;
      }

      x  = p^.ox + o^.x;
      y  = p^.oy + o^.y;
      sx = o^.x_size;
      sy = o^.y_size;

      if (x <= ps.rcPaint.right  && x + sx >= ps.rcPaint.left &&
          y <= ps.rcPaint.bottom && y + sy >= ps.rcPaint.top)
      {
        if (stricmp (o^.font.name, font.name) != 0 ||
            o^.font.height != font.height ||
            o^.font.style  != font.style)
        {
          /* restore old font */
          if (old_font != (HFONT)0)
            SelectObject (hdc, old_font);

          if (new_font != (HFONT)0)
            DeleteObject (new_font);

          new_font = create_font (o^.font.name, o^.font.height, o^.font.style);
          old_font = SelectObject (hdc, new_font);

          font = o^.font;
        }

        switch (o^.typ)
        {
          case TYP_TEXT:
            display_static_text (hdc, x, y, sx, sy,
                                 o^.text != null ? o^.text^ : "",
                                 o^.font.color);
            break;

          case TYP_EDIT:
            {
              string^ line = new char[edit_line_get_max_length (o^.edit.edit_line)];
              intern_edit_line_get_line (o^.edit.edit_line, out line^);

              if (o^.edit.password_mode)
              {
                int j;
                for (j=0; j<line^'length; j++)
                  if (line^[j] != ' ')
                    line^[j] = '*';
              }

              display_edit (hdc, x, y, sx, sy,
                            line^,
                            o^.edit.scroll_x_offset,
                            edit_line_get_col(o^.edit.edit_line),
                            -1,
                            -1,
                            o^.font.color,
                            o^.explicit_field_color,
                            o^.field_color);

              free line;
            }
            break;

          case TYP_CHECKBOX:
            display_checkbox (hdc, x, y, sx, sy,
                              o^.checkbox.box_width,
                              o^.checkbox.setting,
                              o^.text^,
                              (p^.focus == o));
            break;

          case TYP_LISTBOX:
            /* determine which visible line-range must be redrawn, if any */
            {
              int first, last;

              if (ps.rcPaint.top <= y+2)
                first = 0;     /* start at first line */
              else
              {
                first = (ps.rcPaint.top - (y+2)) / o^.listbox.line_height;
                if (first >= o^.listbox.nb_screen_lines)
                  first = o^.listbox.nb_screen_lines - 1;
              }

              if (ps.rcPaint.bottom >= y+sy-2)
                last = o^.listbox.nb_screen_lines - 1;  /* end at last line */
              else
              {
                last = (ps.rcPaint.bottom - (y+2)) / o^.listbox.line_height;
                if (last < 0)
                  last = 0;
              }

              redraw_listbox_lines (hdc, p, o, first, last);
            }

            display_listbox
              (hdc, x, y, sx, sy,
               o^.listbox.arrow_box_width,
               o^.listbox.arrow_box_height,
               o^.listbox.page,
               nb_text_lines (o^.listbox.text),
               o^.listbox.nb_screen_lines);

            break;

          case TYP_BUTTON:
            display_button (hdc, x, y, sx, sy, o^.text^,
                            o^.button.pressed,
                            (p^.focus == o),
                            o^.font.color);
            break;

          case TYP_WINDOW:
            if (o^.window.refresh)
            {
              BitBlt (ps.hdc, x, y, sx, sy,
                      o^.window.hdcMemory, 0, 0,
                      SRCCOPY);
            }
            break;

          case TYP_SCROLL:
            display_vertical_scrollbar (hdc, x, y, sx, sy, o^.scroll.arrow_box_height,
                                        o^.scroll.page, o^.scroll.range, o^.scroll.shown);
            break;

          default:
            break;
        }

        if (ExcludeClipRect (hdc, x, y, x+sx, y+sy) == NULLREGION)
        {
          done = true;
          break;
        }
      }

      o = o^.next;

    }  // end for


    if (done)
      break;


    /* redraw screen itself */

    if (p^.x <= ps.rcPaint.right  && p^.x + p^.x_size >= ps.rcPaint.left &&
        p^.y <= ps.rcPaint.bottom && p^.y + p^.y_size >= ps.rcPaint.top)
    {
      if (stricmp (p^.font.name, font.name) != 0 ||
          p^.font.height != font.height ||
          p^.font.style  != font.style)
      {
        /* restore old font */
        if (old_font != (HFONT)0)
          SelectObject (hdc, old_font);

        if (new_font != (HFONT)0)
          DeleteObject (new_font);

        new_font = create_font (p^.font.name, p^.font.height, p^.font.style);
        old_font = SelectObject (hdc, new_font);

        font = p^.font;
      }

      if (p^.parent_screen == null)   /* main screen */
      {
        display_main_screen (hdc, p^.x, p^.y, p^.x_size, p^.y_size);
      }
      else  /* child screen */
      {
        display_screen (hdc, p^.x, p^.y, p^.x_size, p^.y_size,
                        p^.title^,
                        p^.title_height);
      }

      if (ExcludeClipRect (hdc, p^.x, p^.y,
                           p^.x + p^.x_size, p^.y + p^.y_size) == NULLREGION)
      {
        break;
      }
    }

    p = p^.parent_screen;
  }


  /* redraw caret */
  if (pscreen != null && pscreen^.focus != null && pscreen^.focus^.typ == TYP_EDIT)
  {
    o = pscreen^.focus;

    x  = pscreen^.ox + o^.x;
    y  = pscreen^.oy + o^.y;
    sx = o^.x_size;
    sy = o^.y_size;

    if (stricmp (o^.font.name, font.name) != 0 ||
        o^.font.height != font.height ||
        o^.font.style  != font.style)
    {
      /* restore old font */
      if (old_font != (HFONT)0)
        SelectObject (hdc, old_font);

      if (new_font != (HFONT)0)
        DeleteObject (new_font);

      new_font = create_font (o^.font.name, o^.font.height, o^.font.style);
      old_font = SelectObject (hdc, new_font);

      font = o^.font;
    }

    {
      ref EDIT_LINE e    = o^.edit.edit_line;
      string^       line = new char [edit_line_get_max_length(e)];
      int           k;

      /* fill buffer with line + trailing spaces */
      intern_edit_line_get_line (e, out line^);

      if (o^.edit.password_mode)
      {
        for (k=0; k<line^'length; k++)
          if (line^[k] != ' ')
            line^[k] = '*';
      }

      display_edit_caret (main_hWnd,
                          hdc, x, y, sx, sy,
                          line^,
                          o^.edit.scroll_x_offset,
                          edit_line_get_col (o^.edit.edit_line));
      free line;
    }
  }
  else
  {
    gi_disable_caret ();
  }

  /* restore old font */
  if (old_font != (HFONT)0)
    SelectObject (hdc, old_font);

  if (new_font != (HFONT)0)
    DeleteObject (new_font);

  repaint_done = true;
}

/*************************************************************************/

void redraw_object_for_focus (OBJECT_INFO^ o)
{
  switch (o^.typ)
  {
    case TYP_MENU:
      break;

    case TYP_EDIT:
      redraw_caret (o^);
      break;

    case TYP_CHECKBOX:
    case TYP_EDITBOX:
    case TYP_LISTBOX:
    case TYP_BUTTON:
    case TYP_SCROLL:
      repaint_object (o^);
      break;

    default:
      abort;
  }
}

/*************************************************************************/

void transfer_focus (OBJECT_INFO^ old, OBJECT_INFO^ newo)
{
  if (old != newo)
  {
    if (old != null)
      redraw_object_for_focus (old);

    if (newo != null)
      redraw_object_for_focus (newo);
  }
}

/*************************************************************************/

/* assertion: the screen has at least 1 object */

int global_find_next_hotkey_object (out OBJECT_INFO^  obj,
                                    char              hotkey)
{
  int nb_objects, i;

  obj = pscreen^.focus;
  if (obj == null)
    return -1;

  nb_objects = pscreen^.nb_objects;
  for (i=0; i<nb_objects; i++)
  {
    obj = obj^.next;
    if ((!obj^.hide) && obj^.typ != TYP_MENU_ITEM &&
        toupper(obj^.hotkey) == toupper(hotkey))
    {
      return 0;
    }
  }

  return -1;
}

/************************************************************************/

/* attention: this function also returns objects of TYP_WINDOW */
/* although they can't have the input focus.                   */

int global_find_clicked_object (out OBJECT_INFO^ o,
                                out int relative_x, out int relative_y)
{
  int mouse_x, mouse_y, nb_objects, i;

  gi_get_mouse_coordinates (out mouse_x, out mouse_y);

  mouse_x -= pscreen^.ox;
  mouse_y -= pscreen^.oy;

  o = pscreen^.list;
  if (o == null)
  {
    relative_x = 0;
    relative_y = 0;
    return -1;
  }

  nb_objects = pscreen^.nb_objects;
  for (i=0; i<nb_objects; i++)
  {
    if (o^.typ != TYP_MENU_ITEM && o^.typ != TYP_TEXT && (!o^.hide))
    {
      if (mouse_x >= o^.x && mouse_x < o^.x + o^.x_size &&
          mouse_y >= o^.y && mouse_y < o^.y + o^.y_size)
      {
        relative_x = mouse_x - o^.x;            /* found */
        relative_y = mouse_y - o^.y;
        return 0;
      }
    }
    o = o^.next;
  }

  relative_x = 0;
  relative_y = 0;
  return -1;
}

/************************************************************************/

void set_clicked_edit_col (OBJECT_INFO^ o,
                           int         relative_x,
                           int         relative_y)
{
  ref EDIT_LINE e = o^.edit.edit_line;
  string^       line = new char [edit_line_get_max_length(e)];
  int           i, col, x0, x;
  HDC           hdc;
  SIZE          size;
  HFONT         new_font, old_font;

  _unused relative_y;


  /* fill buffer with line + trailing spaces */
  intern_edit_line_get_line (e, out line^);

  if (o^.edit.password_mode)
  {
    for (i=0; i<line^'length; i++)
      if (line^[i] != ' ')
        line^[i] = '*';
  }


ENTER_GI ();

  hdc = GetDC (main_hWnd);

  new_font = create_font (o^.font.name, o^.font.height, o^.font.style);

  old_font = SelectObject (hdc, new_font);

  x0 = 4;   /* left border */

  for (col=o^.edit.scroll_x_offset; col<edit_line_get_length(e); col++)
  {
    GetTextExtentPoint32A (hdc, &line^[col], 1, &size);
    x = x0 + size.cx;

    if (relative_x <= (x0+x) / 2)
      break;

    x0 = x;
  }

  edit_line_set_col (ref e, col);

  update_scroll_x (ref o^);

  SelectObject (hdc, old_font);
  DeleteObject (new_font);
  ReleaseDC (main_hWnd, hdc);

LEAVE_GI ();

  redraw_caret (o^);

  free line;
}

/************************************************************************/

void scroll_up (    OBJECT_INFO^  o,
                ref SCROLL_INFO   scroll,
                    int          nb_lines,
                out bool         event_occured,
                ref EVENT        event)
{
  int page = scroll.page - nb_lines;

  event_occured = false;

  if (page < 1)
    page = 1;

  if (scroll.page != page)
  {
    scroll.page = page;
    repaint_object (o^);

    clear event;
    event.y = page - 1;
    event.typ = EVENT_SCROLLBAR_MOVED;
    event.id = o^.id;
    event_occured = true;
  }
}

//--------------------------------------------------------------------

void scroll_page_up (    OBJECT_INFO^ o,
                     ref SCROLL_INFO  scroll,
                     out bool         event_occured,
                     ref EVENT        event)
{
  scroll_up (o, ref scroll, scroll.shown, out event_occured, ref event);
}

//--------------------------------------------------------------------

void scroll_down (    OBJECT_INFO^  o,
                  ref SCROLL_INFO   scroll,
                      int          nb_lines,
                  out bool         event_occured,
                  ref EVENT        event)
{
  int page = scroll.page + nb_lines;

  event_occured = false;

  if (page + scroll.shown - 1 > scroll.range)
  {
    page = scroll.range - scroll.shown + 1;
  }

  if (page < 1)
    page = 1;

  if (scroll.page != page)
  {
    scroll.page = page;
    repaint_object (o^);

    clear event;
    event.y = page - 1;
    event.typ = EVENT_SCROLLBAR_MOVED;
    event.id = o^.id;
    event_occured = true;
  }
}

//--------------------------------------------------------------------

void scroll_page_down (    OBJECT_INFO^ o,
                       ref SCROLL_INFO  scroll,
                       out bool         event_occured,
                       ref EVENT        event)
{
  scroll_down (o, ref scroll, scroll.shown, out event_occured, ref event);
}

//--------------------------------------------------------------------

void scroll_top (    OBJECT_INFO^ o,
                 ref SCROLL_INFO  scroll,
                 out bool         event_occured,
                 ref EVENT        event)
{
  event_occured = false;

  if (scroll.page > 1)
  {
    scroll.page = 1;
    repaint_object (o^);

    clear event;
    event.y = 0;
    event.typ = EVENT_SCROLLBAR_MOVED;
    event.id = o^.id;
    event_occured = true;
  }
}

//--------------------------------------------------------------------

void scroll_bottom (    OBJECT_INFO^ o,
                    ref SCROLL_INFO  scroll,
                    out bool         event_occured,
                    ref EVENT        event)
{
  int page = scroll.range - scroll.shown + 1;

  event_occured = false;

  if (page < 1)
    page = 1;

  if (scroll.page != page)
  {
    scroll.page = page;
    repaint_object (o^);

    clear event;
    event.y = page - 1;
    event.typ = EVENT_SCROLLBAR_MOVED;
    event.id = o^.id;
    event_occured = true;
  }
}

//--------------------------------------------------------------------

void click_scroll (    OBJECT_INFO^ o,
                       int          y,
                   ref SCROLL_INFO  scroll,
                   out bool         event_occured,
                   ref EVENT        event)
{
  event_occured = false;

  if (y < scroll.arrow_box_height) // upper arrow box
  {
    scroll_up (o, ref scroll, scroll.small_y_increment, out event_occured, ref event);
    g_repeat_click++;
 }
  else if (y >= o^.y_size - scroll.arrow_box_height)  // lower arrow box
  {
    scroll_down (o, ref scroll, scroll.small_y_increment, out event_occured, ref event);
    g_repeat_click++;
  }
  else   // click on scrollbar area
  {
    int sy, scrollbox_y_offset, scrollbox_height;

    compute_scrollbox_data2 (     o^.y_size,
                                  scroll.arrow_box_height,
                                  scroll.page,
                                  scroll.range,
                                  scroll.shown,
                              out scrollbox_y_offset,
                              out scrollbox_height);

    sy = scroll.arrow_box_height;

    if (y < sy + scrollbox_y_offset)
    {
      // free area above box
      scroll_up (o, ref scroll, scroll.shown, out event_occured, ref event);
      g_repeat_click++;
    }
    else if (y >= sy + scrollbox_y_offset + scrollbox_height)
    {
      // free area below box
      scroll_down (o, ref scroll, scroll.shown, out event_occured, ref event);
      g_repeat_click++;
    }
    else
    {
      scroll.clicked_scrollbox_y = y;
      scroll.clicked_page        = scroll.page;
    }
  }
}

//--------------------------------------------------------------------

/* the user clicked on an object */

int execute_object_click (    OBJECT_INFO^ o,
                              int          relative_x,
                              int          relative_y,
                          out bool         event_occured,
                          ref EVENT        event)
{
  event_occured = false;

  switch (o^.typ)
  {
    case TYP_MENU:
      break;

    case TYP_EDIT:
      set_clicked_edit_col (o, relative_x, relative_y);
      break;

    case TYP_CHECKBOX:
      o^.checkbox.setting = !o^.checkbox.setting;
      repaint_object (o^);
      break;

#if 0
    case TYP_EDITBOX:
      if (relative_x >= 1 && relative_x <= o^.x_size-2 &&
          relative_y >= 1 && relative_y <= o^.y_size-2)
      {
        int  col;
        int  ln;
        int  rc;

        rc = flush_editbox_text (o);
        if (rc)
        {
          message_box ("SYSTEM WARNING",
                       "SYSTEM IS OUT OF VIRTUAL MEMORY\nEDITBOX CONTENT IS CORRUPTED",
                       "CONTINUE");
        }
        o^.editbox.text.cache_state = _UNUSED;

        col = relative_x - 1 + o^.editbox.scroll;
        if (col >= o^.editbox.text.max_length)
          col = o^.editbox.text.max_length - 1;
        o^.editbox.text.col = col;

        ln = relative_y - 1 + o^.editbox.page;
        if (ln > nb_text_lines (o^.editbox.text.text) + 1)
          ln = nb_text_lines (o^.editbox.text.text) + 1;

        o^.editbox.text.ln = ln;

        set_video_cursor
          (active_screen.x + o^.x + 1
              + o^.editbox.text.col - o^.editbox.scroll,
           active_screen.y + o^.y + 1
              + (int)(o^.editbox.text.ln - o^.editbox.page));

        intern_update_block (handle, (char *)o, sizeof(OBJECT_INFO));
      }
      break;
#endif

    case TYP_LISTBOX:

      o^.listbox.show_focus = false;   /* do not show focus */

      if (relative_x >= 2 && relative_x < o^.x_size-2-o^.listbox.arrow_box_width &&
          relative_y >= 2 && relative_y < o^.y_size-2)
      {
        /* click on listbox text line */

        if (o^.listbox.page + (relative_y-2) / o^.listbox.line_height <=
            nb_text_lines (o^.listbox.text))
        {
          int key;
          o^.listbox.ln = o^.listbox.page + (relative_y-2) / o^.listbox.line_height;
          key = select_deselect_listbox_line (o);
          repaint_object (o^);
          return key;
        }
      }
      else if (relative_x >= o^.x_size-2-o^.listbox.arrow_box_width &&
               relative_x < o^.x_size-2 &&
               relative_y >= 2 && relative_y < o^.y_size-2)
      {
        if (relative_y < 2+o^.listbox.arrow_box_height) /* upper arrow box */
        {
          listbox_scroll_up (o, 1);
          g_repeat_click++;
        }
        else if (relative_y >= o^.y_size-2-o^.listbox.arrow_box_height) /* lower arrow box */
        {
          listbox_scroll_down (o, 1);
          g_repeat_click++;
        }
        else   /* click on scrollbar area */
        {
          int sx, sy, sw, sh, scrollbox_y_offset, scrollbox_height;

          compute_scrollbox_data (0, 0, o^.x_size, o^.y_size,
                                  o^.listbox.arrow_box_width,
                                  o^.listbox.arrow_box_height,
                                  o^.listbox.page,
                                  nb_text_lines (o^.listbox.text),
                                  o^.listbox.nb_screen_lines,
                                  out sx, out sy, out sw, out sh,
                                  out scrollbox_y_offset, out scrollbox_height);

          _unused sx;
          _unused sw;
          _unused sh;

          if (relative_y < sy + scrollbox_y_offset)
          {
            /* free area above box */
            listbox_scroll_up (o, o^.listbox.nb_screen_lines);
            g_repeat_click++;
          }
          else if (relative_y >= sy + scrollbox_y_offset + scrollbox_height)
          {
            /* free area below box */
            listbox_scroll_down (o, o^.listbox.nb_screen_lines);
            g_repeat_click++;
          }
          else
          {
            o^.listbox.clicked_scrollbox_y = relative_y;
            o^.listbox.clicked_page        = o^.listbox.page;
          }
        }
      }
      break;

    case TYP_BUTTON:
      break;

    case TYP_SCROLL:
      click_scroll (o, relative_y, ref o^.scroll, out event_occured, ref event);
      break;

    default:
      abort;
  }

  return 0;
}

/************************************************************************/

/* the user drags a listbox bar */

void execute_listbox_drag (OBJECT_INFO^ o, int rel_x, int rel_y)
{
  int relative_x = rel_x;
  int relative_y = rel_y;

  int sx, sy, sw, sh, scrollbox_y_offset, scrollbox_height, y0;
  int y_offset, move_height, line_offset, new_page;

  if (o^.listbox.clicked_scrollbox_y == 0)    /* no earlier click */
    return;

  if (relative_x >= o^.x_size-2-3*o^.listbox.arrow_box_width &&
      relative_x <= o^.x_size+2*o^.listbox.arrow_box_width)
  {
    y0 = 2+o^.listbox.arrow_box_height;
    if (relative_y < y0)
      relative_y = y0;

    y0 = o^.y_size-2-o^.listbox.arrow_box_height;
    if (relative_y > y0)
      relative_y = y0;

    compute_scrollbox_data (0, 0, o^.x_size, o^.y_size,
                            o^.listbox.arrow_box_width,
                            o^.listbox.arrow_box_height,
                            o^.listbox.page,
                            nb_text_lines (o^.listbox.text),
                            o^.listbox.nb_screen_lines,
                            out sx, out sy, out sw, out sh,
                            out scrollbox_y_offset, out scrollbox_height);
    _unused sx;
    _unused sy;
    _unused sw;
    _unused scrollbox_y_offset;

    /* compute y-offset in pixels */

    y_offset = relative_y - o^.listbox.clicked_scrollbox_y;

    move_height = sh - scrollbox_height;
    if (move_height < 1)
      move_height = 1;

    /* can become negative */
    line_offset = y_offset
                  * (nb_text_lines (o^.listbox.text)
                      - o^.listbox.nb_screen_lines)
                  / move_height;

    new_page = o^.listbox.clicked_page + line_offset;

    if (new_page > nb_text_lines (o^.listbox.text)
                    - o^.listbox.nb_screen_lines + 1)
      new_page = nb_text_lines (o^.listbox.text)
                  - o^.listbox.nb_screen_lines + 1;

    if (new_page < 1)
      new_page = 1;

    if (new_page == o^.listbox.page)   /* same page : nothing to do */
      return;

ENTER_GI ();

    o^.listbox.page = new_page;

    if (o^.listbox.ln < new_page)
      o^.listbox.ln = new_page;

    if (o^.listbox.ln > new_page + o^.listbox.nb_screen_lines - 1)
      o^.listbox.ln = new_page + o^.listbox.nb_screen_lines - 1;

LEAVE_GI ();

    repaint_object (o^);
  }
}

/************************************************************************/

int process_key_scroll (OBJECT_INFO^ o, ref SCROLL_INFO scroll, out bool event_occured, ref EVENT event)
{
  int eventnr, key;

  event_occured = false;

  eventnr = local_get_event ();
  if (eventnr != _EVENT_KEYBOARD)
    return eventnr;

  key = g_local_key;

  switch (key)
  {
    case C_KEY_HOME:
    case C_KEY_CTRL_HOME:
    case C_KEY_CTRL_PAGE_UP:  // CTRL + PAGE UP
      scroll_top (o, ref scroll, out event_occured, ref event);
      break;

    case C_KEY_END:
    case C_KEY_CTRL_END:
    case C_KEY_CTRL_PAGE_DOWN:  // CTRL + PAGE_DOWN
      scroll_bottom (o, ref scroll, out event_occured, ref event);
      break;

    case C_KEY_CURSOR_UP:
      scroll_up (o, ref scroll, scroll.small_y_increment, out event_occured, ref event);
      break;

    case C_KEY_CURSOR_DOWN:
      scroll_down (o, ref scroll, scroll.small_y_increment, out event_occured, ref event);
      break;

    case C_KEY_PAGE_UP:  // PAGE_UP
      scroll_page_up (o, ref scroll, out event_occured, ref event);
      break;

    case C_KEY_PAGE_DOWN:  // PAGE_DOWN
      scroll_page_down (o, ref scroll, out event_occured, ref event);
      break;

    default:
      return eventnr;
  }

  return 0;
}

//--------------------------------------------------------------------

void mouse_wheel (OBJECT_INFO^ o, int delta, out bool event_occured, ref EVENT event)
{
  if (o != null && o^.typ == TYP_SCROLL)
  {
    if (delta > 0)
      scroll_up (o, ref o^.scroll, delta*o^.scroll.small_y_increment, out event_occured, ref event);
    else
      scroll_down (o, ref o^.scroll, -delta*o^.scroll.small_y_increment, out event_occured, ref event);
  }
  else
  {
    event.typ = EVENT_MOUSE_WHEEL;
    event.id  = 0;
    event.key = 0;
    event.x = 0;
    event.y = delta;
    event_occured = true;
  }
}

//--------------------------------------------------------------------

void execute_scroll_drag (OBJECT_INFO^ o, ref SCROLL_INFO scroll, int rel_y, out bool event_occured, ref EVENT event)
{
  int y = rel_y;
  int sh, scrollbox_y_offset, scrollbox_height, y0;
  int y_offset, move_height, line_offset, new_page;

  event_occured = false;

  if (scroll.clicked_scrollbox_y == 0)    // no earlier click
    return;

  y0 = scroll.arrow_box_height;
  if (y < y0)
    y = y0;

  y0 = o^.y_size - scroll.arrow_box_height;
  if (y > y0)
    y = y0;

  compute_scrollbox_data2 (    o^.y_size,
                               scroll.arrow_box_height,
                               scroll.page,
                               scroll.range,
                               scroll.shown,
                           out scrollbox_y_offset,
                           out scrollbox_height);
  _unused scrollbox_y_offset;

  sh = o^.y_size - 2 * scroll.arrow_box_height;


  // compute y-offset in pixels

  y_offset = y - scroll.clicked_scrollbox_y;

  move_height = sh - scrollbox_height;
  if (move_height < 1)
    move_height = 1;

  // can become negative
  line_offset = y_offset * (scroll.range - scroll.shown) / move_height;

  new_page = scroll.clicked_page + line_offset;

  if (new_page > scroll.range - scroll.shown + 1)
    new_page = scroll.range - scroll.shown + 1;

  if (new_page < 1)
    new_page = 1;

  if (new_page == scroll.page)   // same page : nothing to do
    return;

  scroll.page = new_page;

  repaint_object (o^);

  clear event;
  event.y = new_page - 1;
  event.typ = EVENT_SCROLLBAR_MOVED;
  event.id = o^.id;
  event_occured = true;
}

//--------------------------------------------------------------------

public void get_event (out EVENT event)
{
  OBJECT_INFO^ o, obj;
  int          eventnr;

#if 0
  int          menu_item_id;
  int          auto_open_menu = 0;
#endif

  clear event;

  current_function = Fget_event;
  check_screen_open ();

  gi_set_visible (true);

  /* produce WM_REPAINT messages to redraw the window */
  flush_repaint_rects ();

  if (pscreen^.nb_activable_objects == 0)   /* TEXT or WINDOW or keyboard */
  {
    o = null;
  }
  else
  {
    /* get first non-text & non-window & visible object starting at focus */
    for (;;)
    {
      /* retrieve current focus object */
      o = pscreen^.focus;
      if (o^.typ != TYP_TEXT && o^.typ != TYP_WINDOW && (!o^.hide))
        break;
ENTER_GI ();
      pscreen^.focus = o^.next;
LEAVE_GI ();
    }
  }

  for (;;)
  {
    if (o == null)   /* no activable objects */
    {
      eventnr = local_get_event ();
    }
    else
    {
      switch (o^.typ)
      {
#if 0
        case TYP_MENU:
          eventnr = animate_menu (o, &auto_open_menu, &menu_item_id);
          break;
#endif

        case TYP_EDIT:
          eventnr = animate_edit (o);
          break;

        case TYP_CHECKBOX:
          eventnr = animate_checkbox (o);
          break;

#if 0
        case TYP_EDITBOX:
          eventnr = animate_editbox (o);
          intern_update_block (&active_screen.focus, (char *)&o, sizeof(OBJECT_INFO));
          break;
#endif

        case TYP_LISTBOX:
          eventnr = animate_listbox (o);
          break;

        case TYP_BUTTON:
          eventnr = animate_button (o);
          break;

        case TYP_SCROLL:
          {
            bool event_occured;
            eventnr = process_key_scroll (o, ref o^.scroll, out event_occured, ref event);
            if (event_occured)
              return;
          }
          break;

        default:
          abort;
      }
    }


    if (eventnr == _EVENT_KEYBOARD)
    {
      int key = g_local_key;

      switch (key)
      {
        case C_KEY_ESCAPE:
          event.typ = EVENT_KEY_PRESSED;
          event.key = KEY_ESCAPE;
          return;

        case C_KEY_F1:
        case C_KEY_F2:
        case C_KEY_F3:
        case C_KEY_F4:
        case C_KEY_F5:
        case C_KEY_F6:
        case C_KEY_F7:
        case C_KEY_F8:
        case C_KEY_F9:
        case C_KEY_F10:
          event.typ = EVENT_KEY_PRESSED;
          event.key = (C_KEY_F1 - key) + KEY_F1;
          return;

        case C_KEY_F11:
        case C_KEY_F12:
          event.typ = EVENT_KEY_PRESSED;
          event.key = (C_KEY_F11 - key) + KEY_F11;
          return;

        case C_KEY_CTRL_ENTER:
          event.typ = EVENT_KEY_PRESSED;
          event.key = KEY_CTRL_ENTER;
          return;

        case C_KEY_CURSOR_LEFT:
        case C_KEY_CURSOR_UP:
        case C_KEY_SHIFT_TAB:     /* set focus to previous object in list */

          if (arrow_keys_reserved &&
               (key == C_KEY_CURSOR_LEFT || key == C_KEY_CURSOR_UP))
          {
            event.typ = EVENT_KEY_PRESSED;
            event.key = (key == C_KEY_CURSOR_LEFT) ? KEY_TURN_LEFT : KEY_MOVE_UP;
            return;
          }

          if (o == null)
            break;

          obj = o;   /* save previous focus object */

ENTER_GI ();
          o = o^.prev;
          pscreen^.focus = o;
LEAVE_GI ();

          while (o^.typ == TYP_MENU_ITEM || o^.typ == TYP_TEXT ||
                 o^.typ == TYP_WINDOW    || o^.hide)
          {
ENTER_GI ();
            o = o^.prev;
            pscreen^.focus = o;
LEAVE_GI ();
          }

          if (o^.typ == TYP_LISTBOX)
            o^.listbox.show_focus = true;

          transfer_focus (obj, o);

          if (o^.id != 0)
          {
            event.typ = EVENT_NEW_FOCUS;
            event.id  = o^.id;
            event.key = key;
            return;
          }
          break;

        case C_KEY_CURSOR_RIGHT:
        case C_KEY_CURSOR_DOWN:
        case C_KEY_ENTER:
        case C_KEY_TAB:
        case C_KEY_CTRL_TAB:      /* set focus to next object in list */

          if (arrow_keys_reserved &&
               (key == C_KEY_CURSOR_RIGHT || key == C_KEY_CURSOR_DOWN))
          {
            event.typ = EVENT_KEY_PRESSED;
            event.key = (key==C_KEY_CURSOR_RIGHT) ? KEY_TURN_RIGHT : KEY_MOVE_DOWN;
            return;
          }

          if (o == null)
            break;

          obj = o;   /* save previous focus object */

ENTER_GI ();
          o = o^.next;
          pscreen^.focus = o;
LEAVE_GI ();

          while (o^.typ == TYP_MENU_ITEM || o^.typ == TYP_TEXT ||
                 o^.typ == TYP_WINDOW    || o^.hide)
          {
ENTER_GI ();
            o = o^.next;
            pscreen^.focus = o;
LEAVE_GI ();
          }

          if (o^.typ == TYP_LISTBOX)
            o^.listbox.show_focus = true;

          transfer_focus (obj, o);

          if (o^.id != 0)
          {
            event.typ = EVENT_NEW_FOCUS;
            event.id  = o^.id;
            event.key = key;
            return;
          }
          break;

        case C_KEY_CTRL_CURSOR_LEFT:
        case C_KEY_CTRL_CURSOR_RIGHT:
          if (arrow_keys_reserved)
          {
            event.typ = EVENT_KEY_PRESSED;
            event.key = (key==C_KEY_CTRL_CURSOR_LEFT) ? KEY_MOVE_LEFT : KEY_MOVE_RIGHT;
            return;
          }
          break;

        case C_KEY_USER_EVENT1:
        case C_KEY_USER_EVENT2:
        case C_KEY_USER_EVENT3:
          event.typ = EVENT_KEY_PRESSED;
          event.key = KEY_USER_EVENT1 + (key - C_KEY_USER_EVENT1);
          return;

        default:
          if (key >= C_KEY_ALT_Z && key <= C_KEY_ALT_A)    /* ALT-letter */
          {
            /* set focus to next hotkey */
            if (global_find_next_hotkey_object (out obj, (char)(-key-256)) == 0)
            {
              transfer_focus (pscreen^.focus, obj);

ENTER_GI ();
              pscreen^.focus = obj;
LEAVE_GI ();

              if (obj^.typ == TYP_MENU)
              {
#if 0
 $
                o = obj;
                auto_open_menu = 1;     /* always open the menu on first item */
                /* continue (no break!) */
#endif
              }
              else
              {
                event.typ = EVENT_NEW_FOCUS;
                event.id  = obj^.id;
                event.key = key;
                return;
              }
            }
          }
          break;

      }  // end switch (key)
    }
    else
    {
      switch (eventnr)
      {
#if 0
        case _EVENT_MENU_SELECTED:
          event.typ = EVENT_MENU_SELECTED;
          event.id  = menu_item_id;
          return;
#endif

        case _EVENT_EDIT_CHANGED:
          event.typ = EVENT_EDIT_CHANGED;
          event.id  = o^.id;
          return;

        case _EVENT_CHECKBOX_CHANGED:
          event.typ = EVENT_CHECKBOX_CHANGED;
          event.id  = o^.id;
          event.key = (int)o^.checkbox.setting;
          return;

        case _EVENT_LISTBOX_LINE_SELECTED:
          event.typ = EVENT_LISTBOX_LINE_SELECTED;
          event.id  = o^.id;
          return;

        case _EVENT_LISTBOX_LINE_DESELECTED:
          event.typ = EVENT_LISTBOX_LINE_DESELECTED;
          event.id  = o^.id;
          return;

        case _EVENT_BUTTON_PRESSED:
          button_effect_3D (o);
          event.typ = EVENT_BUTTON_PRESSED;
          event.id  = o^.id;
          return;

        case _EVENT_MOUSE_CLICK_LEFT:
        case _EVENT_MOUSE_CLICK_RIGHT:
          {
            int  relative_x, relative_y;
            bool event_occured;

            pscreen^.clicked_obj = null;

            if (global_find_clicked_object (out obj, out relative_x, out relative_y) == 0)
            {
              pscreen^.clicked_obj = obj;   /* save last clicked object */

              if (obj^.typ == TYP_WINDOW)   /* special case : a window */
              {
                int mouse_x, mouse_y, xs, ys;

                /* note: a window cannot receive the input focus */

                mouse_x = relative_x - obj^.window.ofs_x;
                mouse_y = relative_y - obj^.window.ofs_y;

                xs = obj^.window.x_size;
                ys = obj^.window.y_size;

                if (mouse_x < 0 || mouse_x >= xs ||
                    mouse_y < 0 || mouse_y >= ys)
                  break;     /* click was outside active part of window */

                event.typ = EVENT_WINDOW_CLICKED;
                event.id  = obj^.id;
                event.key = (eventnr == _EVENT_MOUSE_CLICK_LEFT) ? 1 : 2;
                event.x   = mouse_x;
                event.y   = mouse_y;

                gi_set_mouse_arrow (false);      /* normal */

                return;
              }

              transfer_focus (pscreen^.focus, obj);
ENTER_GI ();
              pscreen^.focus = obj;
LEAVE_GI ();

              eventnr = execute_object_click (obj, relative_x, relative_y, out event_occured, ref event);
              if (event_occured)
                return;

              if (obj^.typ == TYP_MENU)
              {
#if 0
                o = obj;
                auto_open_menu = 1;  /* always open the menu on first item */
                break;
#endif
              }
              else if (obj^.typ == TYP_CHECKBOX)
              {
                event.typ = EVENT_CHECKBOX_CHANGED;
                event.id  = obj^.id;
                event.key = (int)obj^.checkbox.setting;
                return;
              }
              else if (obj^.typ == TYP_LISTBOX)
              {
                if (eventnr == _EVENT_LISTBOX_LINE_SELECTED)
                  event.typ = EVENT_LISTBOX_LINE_SELECTED;
                else if (eventnr == _EVENT_LISTBOX_LINE_DESELECTED)
                  event.typ = EVENT_LISTBOX_LINE_DESELECTED;
                else
                  event.typ = EVENT_NEW_FOCUS;
                event.id  = obj^.id;
                return;
              }
              else if (obj^.typ == TYP_BUTTON)
              {
                button_effect_3D (obj);
                event.typ = EVENT_BUTTON_PRESSED;
                event.id  = obj^.id;
                return;
              }
              else
              {
                event.typ = EVENT_NEW_FOCUS;
                event.id  = obj^.id;
                return;
              }
            }
          }
          break;

        case _EVENT_MOUSE_DRAG:
        case _EVENT_MOUSE_DROP:
          {
            int relative_x, relative_y;

            obj = pscreen^.clicked_obj;
            if (obj != null &&
                obj^.typ == TYP_LISTBOX)   /* drag listbox scrollbox */
            {
              if (eventnr == _EVENT_MOUSE_DRAG)
              {
                gi_get_mouse_coordinates (out relative_x, out relative_y);

                relative_x -= (pscreen^.ox + obj^.x);
                relative_y -= (pscreen^.oy + obj^.y);

                execute_listbox_drag (obj, relative_x, relative_y);
              }
              else
                obj^.listbox.clicked_scrollbox_y = 0;
            }
            else if (obj != null && obj^.typ == TYP_SCROLL)   /* drag scrollbar */
            {
              if (eventnr == _EVENT_MOUSE_DRAG)
              {
                bool event_occured;

                gi_get_mouse_coordinates (out relative_x, out relative_y);

                relative_x -= (pscreen^.ox + obj^.x);
                relative_y -= (pscreen^.oy + obj^.y);

                execute_scroll_drag (obj, ref obj^.scroll, relative_y, out event_occured, ref event);

                if (event_occured)
                  return;
              }
              else
              {
                obj^.scroll.clicked_scrollbox_y = 0;
                obj^.scroll.clicked_page = 0;
              }
            }
            else if (global_find_clicked_object (out obj, out relative_x, out relative_y) == 0)
            {
              if (obj^.typ == TYP_WINDOW)   /* special case : a window */
              {
                int mouse_x, mouse_y, xs, ys;

                /* note: a window cannot receive the input focus */

                mouse_x = relative_x - obj^.window.ofs_x;
                mouse_y = relative_y - obj^.window.ofs_y;

                xs = obj^.window.x_size;
                ys = obj^.window.y_size;

                if (mouse_x < 0 || mouse_x >= xs ||
                    mouse_y < 0 || mouse_y >= ys)
                  break;     /* click was outside active part of window */

                if (eventnr == _EVENT_MOUSE_DRAG)
                  event.typ = EVENT_WINDOW_DRAG;
                else
                  event.typ = EVENT_WINDOW_DROP;

                event.id  = obj^.id;
                event.x   = mouse_x;
                event.y   = mouse_y;

                gi_set_mouse_arrow (false);      /* normal */

                return;
              }
            }
          }
          break;

        case _EVENT_MOUSE_WHEEL:
          {
            bool event_occured;
            mouse_wheel (o, g_mouse_wheel_y, out event_occured, ref event);
            if (event_occured)
              return;
          }
          break;

        case _EVENT_REDRAW_MAIN_WINDOW:
          {
            SCREEN_INFO^ p = pscreen;

            event.typ = EVENT_MAIN_SCREEN_RESIZED;
            gi_get_new_size (out event.x_size, out event.y_size);

            while (p^.parent_screen != null)
              p = p^.parent_screen;

            set_new_screen_size (ref p^, event.x_size, event.y_size);
          }
          return;

        case _EVENT_SIGNAL:
          event.typ = EVENT_SIGNAL;
          event.key = g_local_signal_nr;
          return;

        default:      /* unknown key has no effect */
          break;
      }
    }
  }
}

/*************************************************************************/

public void set_focus (OBJECT_ID id)
{
  OBJECT_INFO^ o;

  current_function = Fset_focus;
  check_screen_open ();

  o = load_object (id);

  if (o^.typ != TYP_EDIT     &&
      o^.typ != TYP_CHECKBOX &&
      o^.typ != TYP_LISTBOX  &&
      o^.typ != TYP_EDITBOX  &&
      o^.typ != TYP_SCROLL   &&
      o^.typ != TYP_BUTTON)
    fatal_error ("cannot set focus on this typ of object");

  if (o^.hide)
    fatal_error ("cannot set focus on invisible object");

ENTER_GI ();
  transfer_focus (pscreen^.focus, o);
  pscreen^.focus = o;
LEAVE_GI ();
}

/**********************************************************************/

/* set all values to -1 to fix screen size */

public void win_set_min_max_main_screen_size (int min_x, int max_x, int min_y, int max_y)
{
  init_gi_layer ();

  if (min_x < 0)
    gi_set_resize_off ();
  else
    gi_set_resize_range (min_x, max_x, min_y, max_y);
}

/**********************************************************************/

public void win_set_background_colors (uint screen, uint field)
{
  _win_set_background_colors (screen, field);
}

/**********************************************************************/

public void win_get_background_colors (out uint screen, out uint field)
{
  _win_get_background_colors (out screen, out field);
}

/**********************************************************************/
#end unsafe
/*************************************************************************/
