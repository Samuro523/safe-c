
/* winstr.h : basic objects of WIN layer */

use ../text;
use windows, ../edline, ../win;


/************************************************************************/

enum OBJECT_TYP {TYP_MENU, TYP_MENU_ITEM, TYP_TEXT, TYP_EDIT,
   TYP_CHECKBOX, TYP_EDITBOX, TYP_LISTBOX, TYP_BUTTON, TYP_WINDOW, TYP_SCROLL};

/************************************************************************/

struct EDIT_INFO {
  EDIT_LINE    edit_line;
  bool         password_mode;     /* false = normal, true = display "***" */
  int          scroll_x_offset;
}

struct CHECKBOX_INFO {
  int          box_width;
  bool         setting;
}


#if 0
struct EDITBOX_INFO {
  EDIT_TEXT    text;
  int          page;             /* line number of first screen line */
  int          scroll;           /* column of first screen column    */
  bool         show_col_line;    /* display 'Col xxx Line yyy'       */
  BLOCK_HANDLE current_line;     /* buffer for current line contents */
}
#endif


struct LISTBOX_INFO {
  A_TEXT       text;                 /* format: reference:4, selected(0/1):1, line   */
  int          page;                 /* index of first visible listbox line (1 .. ?) */
  int          ln;                   /* index of current listbox line (0=none .. ?)  */
  int          nb_screen_lines;      /* lines visible on screen                */
  int          line_length;          /* max characters of a line               */
  int          line_height;          /* height of a line, in pixels            */
  int          scrollbar;            /* scrollbar position (1 .. y_size-1)     */
  int          arrow_box_width;
  int          arrow_box_height;
  bool         allow_user_selection;
  bool         allow_hotkey_search;
  bool         multiple_mode;        /* false = single mode, true = multiple mode  */
  int          selected_line;        /* only for single mode (0 for none)   */
  int          clicked_scrollbox_y;  /* clicked y for dragging scrollbox    */
  int          clicked_page;         /* current page nr at click            */
  bool         show_focus;           /* only if working with keyboard       */
}

struct BUTTON_INFO {
  bool         pressed;
}

struct WINDOW_INFO {
  int           x_size;       /* high-resolution range */
  int           y_size;
  int           ofs_x;
  int           ofs_y;
  uint          rgb_color;    /* current RGB color for drawing */
  HBITMAP       hbitmap;
  HDC           hdcMemory;
  HBITMAP       old_hbitmap;
  bool          refresh;      /* false = no refresh (cam), true = refresh */
}

struct SCROLL_INFO {
  int          page;                 // index of first visible listbox line (1 .. ?)
  int          range;                // extern range (#total)
  int          shown;                // intern range (#visible)
  int          arrow_box_height;
  int          clicked_scrollbox_y;  // clicked y for dragging scrollbox
  int          clicked_page;         // current page nr at click
  int          small_y_increment;
}

struct OBJECT_INFO (OBJECT_TYP typ)
{
  int          id;
  int          x, y;             /* relative position and */
  int          x_size, y_size;   /* size of object */
  FONT_STYLE   font;
  string^      text;             /* allocated on heap (can be null) */
  char         hotkey;
  bool         explicit_field_color;
  uint         field_color;      /* background color of field */
  OBJECT_INFO^ prev, next;       /* circular list (for TAB/UNTAB) */
  bool         hide;             /* true = invisible, false = visible */

  switch (typ)
  {
    case TYP_TEXT:
      null;

    case TYP_EDIT:
      EDIT_INFO      edit;

    case TYP_CHECKBOX:
      CHECKBOX_INFO  checkbox;

#if 0
    case TYP_EDITBOX:
      EDITBOX_INFO   editbox;
#endif

    case TYP_LISTBOX:
      LISTBOX_INFO   listbox;

    case TYP_BUTTON:
      BUTTON_INFO    button;

    case TYP_WINDOW:
      WINDOW_INFO    window;
    
    case TYP_SCROLL:
      SCROLL_INFO    scroll;
 }
}

/************************************************************************/

struct SCREEN_INFO
{
  SCREEN_INFO^ parent_screen;
  int          x, y;           /* absolute position and */
  int          x_size, y_size; /* size of screen (incl. title & border, except for main window) */

  FONT_STYLE   font;           /* font for title */
  string^      title;          /* allocated on heap */
  int          title_height;   /* height of title bar (0 = none)  */

  /* absolute position and size of client area of screen */
  int          ox, oy, ox_size, oy_size;

  FONT_STYLE   old_font;
  bool         old_edit_insert_mode;
  bool         old_editbox_insert_mode;

  /* fields for objects */
  int          nb_objects;    /* number of objects in lists below */
  int          nb_activable_objects;   /* number of TABable objects */
  OBJECT_INFO^ list;         /* circular list of objects or null */
  OBJECT_INFO^ focus;        /* circular list of objects or null */
  OBJECT_INFO^ clicked_obj;  /* last clicked object or null */
}

/************************************************************************/
