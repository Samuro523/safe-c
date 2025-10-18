
// guitree.h

use ../edline, ../edtext, ../text;
use ../gui;

#if WINDOWS
  use ../win/windows;
#elif ANDROID
#else
  bad
#endif

//--------------------------------------------------------------------
#if ANDROID
  typedef uint    HWND;
  typedef uint    COLORREF;
  typedef uint    HPEN;
  typedef uint    HGDIOBJ;
  typedef uint    HBRUSH;
  typedef uint    HFONT;
  typedef byte[]^ HBITMAP;

  packed struct POINT
  {
    int x;
    int y;
  }

  packed struct SIZE
  {
    int cx;
    int cy;
  }

  packed struct RECT
  {
    int left;
    int top;
    int right;
    int bottom;
  }

  struct HDC
  {
    byte[]^   image;
    uint      width;
    uint      height;
    RECT      rect;
    HBRUSH    brush;
    HFONT     font;
    HPEN      pen;
    COLORREF  textcolor;
    COLORREF  backcolor;
  }

  typedef int8    INT_PTR;
  typedef INT_PTR HCURSOR;
  typedef byte[]^ HANDLE;

  struct WINDOWPOS
  {
    HWND hwnd;
    HWND hwndInsertAfter;
    int  x;
    int  y;
    int  cx;
    int  cy;
    uint flags;
  }

  struct PAINTSTRUCT
  {
    RECT rcPaint;
  }

const int IDC_ARROW = 32512;
const int IDC_IBEAM = 32513;
const int IDC_WAIT  = 32514;
const int IDC_NO    = 32648;
const int IDC_HAND  = 32649;

const int VK_BACK         = 0x08;
const int VK_TAB          = 0x09;
const int VK_RETURN       = 0x0D;
const int VK_SHIFT        = 0x10;
const int VK_CONTROL      = 0x11;
const int VK_MENU         = 0x12;
const int VK_INSERT       = 0x2D;
const int VK_DELETE       = 0x2E;
const int VK_PRIOR        = 0x21;
const int VK_NEXT         = 0x22;
const int VK_END          = 0x23;
const int VK_HOME         = 0x24;
const int VK_LEFT         = 0x25;
const int VK_UP           = 0x26;
const int VK_RIGHT        = 0x27;
const int VK_DOWN         = 0x28;

#endif
//--------------------------------------------------------------------

enum CONTROL_TYP {TYP_TEXT, TYP_EDIT, TYP_CHECKBOX, TYP_RADIOBUTTON, TYP_BUTTON, TYP_WINDOW, TYP_LISTBOX,
   TYP_SCROLL, TYP_TREE, TYP_COMBO, TYP_EDITBOX};

const string[] typ_name = {"a text", "an edit", "a checkbox", "a radiobutton", "a button", "a window", "a listbox",
   "a scroll", "a tree", "a combo", "an editbox"};

// control can be clicked and does not move the dialog window, can be reached with TAB
const bool tabable[1+(int)CONTROL_TYP'last] = {false, true, true, true, true, true, true, true, true, true, true};

//--------------------------------------------------------------------

struct SINGLE_EDIT_INFO
{
  EDIT_LINE    edit_line;
  int          scroll_x_offset;
}

const int MAX_EDIT_UNDOS = 32;

struct EDIT_INFO
{
  bool             password_mode;     // false = normal, true = display "***"
  bool             line_marking_in_progress;
  int              line_marking_start_col;
  SINGLE_EDIT_INFO cr;
  int              next_undo_slot;         // next undo slot to fill
  int              undo_slots_filled;
  SINGLE_EDIT_INFO undo[MAX_EDIT_UNDOS];
}

struct CHECKBOX_INFO
{
  int          box_width;
  bool         modify_allowed;
  bool         setting;
}

struct RADIOBUTTON_INFO
{
  int          box_width;
  bool         setting;
  bool         modify_allowed;
  RADIOBUTTON_GROUP_ID gid;
}

struct BUTTON_INFO
{
  bool         pressed;
}

struct WINDOW_INFO
{
#if WINDOWS
  HBITMAP       hbitmap;
  HDC           hdcMemory;
  HBITMAP       old_hbitmap;

#elif ANDROID
  HDC           hdcMemory;  

#else
  bad

#endif
  
  bool          refresh;         // false = no refresh (cam), true = refresh
}

struct LISTBOX_INFO
{
  W_TEXT       wtext;                // format: reference:4, selected(0/1):1, line
  int          page;                 // index of first visible listbox line (1 .. ?)
  int          ln;                   // index of current listbox line (0=none .. ?)
  int          nb_screen_lines;      // lines visible on screen
  int          line_length;          // max characters of a line
  int          line_height;          // height of a line, in pixels
  int          scrollbar;            // scrollbar position (1 .. y_size-1)
  int          arrow_box_width;
  int          arrow_box_height;
  bool         allow_user_selection;
  bool         allow_hotkey_search;
  int          hotkey_position;
  bool         multiple_mode;        // false = single mode, true = multiple mode
  int          selected_line;        // for single mode : the selected line, or 0 for none,
                                     // for multi mode : the start line of a range, or 0 for none.
  int          clicked_scrollbox_y;  // clicked y for dragging scrollbox
  int          clicked_page;         // current page nr at click
  bool         show_focus;           // only if working with keyboard
}

packed struct PREFIX      // line prefix (14 bytes) MUST BE MULTIPLE OF 2 (WCHAR)
{
  int8  reference;        // user reference
  bool  selected;         // line is selected by user (displayed in reverse)
  bool  is_folder;        // true = contains children, false = single item
  uint2 level;            // tree level (0 = root)
  uint2 type;             // type of icon (for future use)
}

const int PREFIX_WLEN = (int)PREFIX'size / 2;   // nb of wchars to store PREFIX

const int MAX_TREE_LEVELS = 32;   // must be power of 2

bool tree_line_is_expanded (ref W_TEXT wtext, int ln, uint2 level);

enum TSCROLL {HSCROLL, VSCROLL};

struct SCROLL_INFO
{
  TSCROLL      tscroll;
  int          page;                 // index of first visible col line (1 .. ?)
  int          range;                // extern range (#total)
  int          shown;                // intern range (#visible)
  int          arrow_box_width_or_height;  // width for HSCROLL, height for VSCROLL
  int          clicked_scrollbox_c;  // clicked x for dragging hscrollbox, y for dragging vscrollbox
  int          clicked_page;         // current page nr at click
  int          small_y_increment;
}

struct LISTBOX_EXTRA
{
  bool         listbox_shown;
  int          y;             // relative position and
  int          y_size;        // size of listbox
}

struct TREE_INFO
{
  LISTBOX_INFO  listbox;
}

struct COMBO_INFO
{
  EDIT_INFO     edit;
  LISTBOX_INFO  listbox;
  LISTBOX_EXTRA lb;
}

struct SINGLE_EDITBOX_INFO
{
  EDIT_TEXT    text;
  int          page;             // line number of first screen line (>= 1)
  int          scroll;           // column of first screen column (>= 0)
}

const int MAX_EDITBOX_UNDOS = 64;
struct EDITBOX_INFO
{
  int                  font_width;
  int                  nb_full_visible_lines;
  bool                 text_marking_in_progress;
  TEXT_MARK            text_marking_start;
  SINGLE_EDITBOX_INFO  cur;
  int                  next_undo_slot;         // next undo slot to fill
  int                  undo_slots_filled;
  SINGLE_EDITBOX_INFO  undo[MAX_EDITBOX_UNDOS];
}

struct CONTROL_INFO (CONTROL_TYP typ)
{
  CONTROL_ID    id;
  int           x, y;             // relative position and
  int           x_size, y_size;   // size of object
  FONT          font;
  wstring^      text;             // allocated on heap (can be null)
  wchar         hotkey;
  CONTROL_INFO^ prev, next;       // circular list (for TAB/UNTAB)
  bool          hide;             // true = invisible, false = visible
  GUI_COLORS    colors;

  switch (typ)
  {
    case TYP_TEXT:
      null;

    case TYP_EDIT:
      EDIT_INFO edit;

    case TYP_CHECKBOX:
      CHECKBOX_INFO checkbox;

    case TYP_RADIOBUTTON:
      RADIOBUTTON_INFO radiobutton;

    case TYP_BUTTON:
      BUTTON_INFO button;

    case TYP_WINDOW:
      WINDOW_INFO window;

    case TYP_LISTBOX:
      LISTBOX_INFO listbox;

    case TYP_SCROLL:
      SCROLL_INFO scroll;

    case TYP_TREE:
      TREE_INFO tree;

    case TYP_COMBO:
      COMBO_INFO combo;

    case TYP_EDITBOX:
      EDITBOX_INFO editbox;
  }
}

//--------------------------------------------------------------------

struct DIALOG_INFO
{
  DIALOG_ID      d;
  HWND           hwnd;
  DIALOG_HANDLER handler;
  uint           ShowWindowArg;
  int            border_drag_size; // size of border than can be dragged with the mouse to resize the window (default = 0)
  int            border_size;      // visual border size, 0, 1 or 2.
  bool           has_close_button;
  FONT           title_font;                  // font & color of title
  int            title_height;
  GUI_COLORS     dialog_colors;
  uint           dialog_background_color;
  int            dialog_transparency_focus;
  int            dialog_transparency_non_focus;
  FONT           default_font;               // current default font for controls
  GUI_COLORS     default_colors;             // current default colors for controls
  bool           default_insert_mode;        // true = insert, false = delete
  bool           redirect_keyboard_to_main_window;

#if ANDROID
  bool           dialog_initialized_done;
  bool           needs_repaint;
  bool           full_repaint;
  RECT           paint_rect;     // relative to dialog
  bool           needs_set_top;
  bool           needs_move;
  bool           has_focus;      // user clicked on it : dialog has focus transparency (several dialogs can have focus)
#endif

  // work variables
  RECT           rect;           // absolute screen position of dialog

  int            current_transparency;
  bool           has_keyboard_focus;    // only 1 dialog in the system has keyboard focus
  bool           mouse_tracking_active;
  int2           previous_mouse_x, previous_mouse_y;

  // fields for controls
  CONTROL_INFO^  list;           // circular list of controls or null
  CONTROL_INFO^  focus;          // control having the focus or null
}

//--------------------------------------------------------------------

HWND hInstance;

APPLICATION_EVENT_HANDLER app_handler;   // user function that handles main window events

RECT main_win_rect;                      // position and size of main window

int  y_title, x_border, y_border;        // size of borders of main window
bool caret_visible;
int  caret_x, caret_y, caret_height, caret_new_x, caret_new_y, caret_new_height;
bool g_shift_pressed, g_control_pressed, g_alt_pressed, g_middle_mouse_button_pressed;

#begin unsafe
WINDOWPOS*   main_window_windowpos;
WINDOWPOS*   sub_window_windowpos;
#end unsafe

#begin unsafe
DIALOG_INFO* g_dialog_ptr;       // current dialog (set during event call)
DIALOG_INFO* g_new_dialog_ptr;   // (used as global temporary when creating a new dialog)
#end unsafe

bool g_mouse_tracking_active;
int2 g_previous_mouse_x, g_previous_mouse_y;

INT_PTR g_basic_mouse_shape = IDC_ARROW;   // IDC_ARROW or IDC_BEAM
INT_PTR g_current_mouse_shape;             // IDC_ARROW, IDC_BEAM, IDC_NO, IDC_WAIT
HCURSOR g_hcursor_shape;

bool g_drag_selected;
bool g_drag_moved;
bool g_user_drop_allowed;

MOUSE_SHAPE g_mouse_shape;

HBITMAP g_wallpaper_bitmap;

#begin unsafe
CONTROL_INFO* g_last_control_left_clicked;
#end unsafe

#if ANDROID
  bool     g_dialogs_needs_repaint;
#endif

//--------------------------------------------------------------------

struct UNSCALE
{
  int factor;
  int shift;
}

int     g_scale = 1;
UNSCALE g_unscale = {1, 0};

//--------------------------------------------------------------------
