
/* wdraw.c : win drawing primitives */

use ../strings;
use windows, gi;

/**************************************************************************/
#begin unsafe
/**************************************************************************/

/* Basic Colors */

const uint COLOR_BLACK       = 0x000000;
const uint COLOR_MIDDLE_GREY = 0xC0C0C0;
const uint COLOR_WHITE       = 0xFFFFFF;
const uint COLOR_DARK_BLUE   = 0x800000;

const uint COLOR_SCREEN_CAPTION_BACKGROUND  = COLOR_DARK_BLUE;
const uint COLOR_SCREEN_CAPTION_TEXT        = COLOR_WHITE;
const uint COLOR_MENU_BACKGROUND            = COLOR_MIDDLE_GREY;
const uint COLOR_MENU_TEXT                  = COLOR_BLACK;
const uint COLOR_MENU_REV_BACKGROUND        = COLOR_DARK_BLUE;
const uint COLOR_MENU_REV_TEXT              = COLOR_WHITE;

/**************************************************************************/

// can be overridden by user's color theme
uint color_black  = 0x000000;
uint color_dark   = 0x808080;
uint color_middle = 0xC0C0C0;
uint color_light  = 0xE0E0E0;
uint color_white  = 0xFFFFFF;

uint field_background_color = COLOR_WHITE;

/**************************************************************************/

const int PEN_BLACK  =  0;
const int PEN_DARK   =  1;
const int PEN_MIDDLE =  2;
const int PEN_LIGHT  =  3;
const int PEN_WHITE  =  4;

const int MAX_PENS = 5;

HPEN hpen[MAX_PENS];

/**************************************************************************/

public void create_gi_objects ()
{
  uint pen_color[MAX_PENS];
  int  i;

  pen_color = {color_black,
               color_dark,
               color_middle,
               color_light,
               color_white};

  /* create PENs for drawing graphic objects */
  for (i=0; i<MAX_PENS; i++)
    hpen[i] = CreatePen (PS_SOLID, 1, pen_color[i]);
}

/**************************************************************************/

public void destroy_gi_objects ()
{
  int i;
  for (i=0; i<MAX_PENS; i++)
    DeleteObject (hpen[i]);
}

/**************************************************************************/

uint limit (int value)
{
  if (value < 0)
    return 0;

  if (value > 255)
    return 255;

  return (uint)value;
}

/**************************************************************************/

public void _win_set_background_colors (uint screen, uint field)
{
  int r = (int)(screen & 255);
  int g = (int)((screen >> 8) & 255);
  int b = (int)((screen >> 16) & 255);

  color_black = limit (r-192)
                  + (limit (g-192) << 8)
                  + (limit (b-192) << 16);

  color_dark = limit (r-64)
                + (limit (g-64) << 8)
                + (limit (b-64) << 16);

  color_middle = screen;

  color_light  = limit (r+32)
                  + (limit (g+32) << 8)
                  + (limit (b+32) << 16);

  color_white  = limit (r+64)
                  + (limit (g+64) << 8)
                  + (limit (b+64) << 16);

  field_background_color = field;

  colors_changed = true;
}

/**************************************************************************/

public void _win_get_background_colors (out uint screen, out uint field)
{
  screen = color_middle;
  field  = field_background_color;
}

/**************************************************************************/

package PKG_FIGURES

  struct RCOORD   /* relative coordinates (add box size if negative) */
  {
    short x, y;
  }

  struct BLINE    /* broken line (joins 3 points) */
  {
    int    pen;
    RCOORD coord[3];
  }

  const int MAX_BLINES = 4;

  struct FIGURE
  {
    BLINE bline[];
  }

  const FIGURE draw_button_data[2] =   /* NORMAL/PRESSED */
  { /* NORMAL  */ {   {{PEN_WHITE,   {{0,-2},  {0, 0}, {-2,0}}},
                       {PEN_DARK,    {{1,-2}, {-2,-2}, {-2,1}}},
                       {PEN_BLACK,   {{0,-1}, {-1,-1}, {-1,0}}}}},
    /* PRESSED */ {   {{PEN_BLACK,   {{0,-2},  {0, 0}, {-2,0}}},
                       {PEN_DARK,    {{1,-2},  {1, 1}, {-2,1}}},
                       {PEN_WHITE,   {{0,-1}, {-1,-1}, {-1,0}}}}}
  };


  const int HOLLOW   = 0;
  const int MOUNTAIN = 1;

  const FIGURE draw_box_data[2] =    /* HOLLOW / MOUNTAIN */
  { /* HOLLOW  */ {   {{PEN_DARK,   {{0,-2}, { 0, 0}, {-2,0}}},
                       {PEN_BLACK,  {{1,-3}, { 1, 1}, {-3,1}}},
                       {PEN_LIGHT,  {{1,-2}, {-2,-2}, {-2,1}}},
                       {PEN_WHITE,  {{0,-1}, {-1,-1}, {-1,0}}}}},
    /* MOUNTAIN*/ {   {{PEN_LIGHT,  {{0,-2}, { 0, 0}, {-2,0}}},
                       {PEN_WHITE,  {{1,-3}, { 1, 1}, {-3,1}}},
                       {PEN_DARK,   {{1,-2}, {-2,-2}, {-2,1}}},
                       {PEN_BLACK,  {{0,-1}, {-1,-1}, {-1,0}}}}}
  };

  const FIGURE draw_menu_data[2] =   /* NORMAL/PRESSED */
  { /* NORMAL  */ {   {{PEN_WHITE,  {{0,-2}, { 0, 0}, {-2,0}}},
                       {PEN_DARK,   {{0,-1}, {-1,-1}, {-1,0}}}}},
    /* PRESSED */ {   {{PEN_DARK,   {{0,-2}, { 0, 0}, {-2,0}}},
                       {PEN_WHITE,  {{0,-1}, {-1,-1}, {-1,0}}}}}};

  /* additional inner border around menu item box */
  const FIGURE draw_menu_item_inner_data =
   {   {{PEN_MIDDLE,  {{2,-3},  {2, 2}, {-3,2}}},
        {PEN_MIDDLE,  {{3,-3}, {-3,-3}, {-3,3}}}}};

end PKG_FIGURES;

/**************************************************************************/

void draw_figure (HDC    hdc,
                  int    x, int y, int width, int height,
                  FIGURE i)
{
  int   count, p;
  POINT pts[4];
  HPEN  holdpen;

  for (count=0; count<i.bline'length; count++)
  {
    clear pts;

    for (p=0; p<3; p++)
    {
      pts[p].x = i.bline[count].coord[p].x;
      if (pts[p].x < 0)
        pts[p].x += width;
      pts[p].x += x;

      pts[p].y = i.bline[count].coord[p].y;
      if (pts[p].y < 0)
        pts[p].y += height;
      pts[p].y += y;
    }

    /* Polyline does not draw the end-point, so we append a dummy line */
    pts[3].x = pts[2].x + 1;
    pts[3].y = pts[2].y;

    holdpen = SelectObject (hdc, hpen[i.bline[count].pen]);

    Polyline (hdc, &pts, 4);

    SelectObject (hdc, holdpen);
  }
}

/**************************************************************************/

void fill_rectangle (HDC      hdc,
                     int      x, int y, int width, int height,
                     COLORREF color)
{
  RECT   rect;
  HBRUSH hbrush;

  rect = {left   => x,
          top    => y,
          right  => x + width,
          bottom => y + height};

  hbrush = CreateSolidBrush (color);
  FillRect (hdc, &rect, hbrush);
  DeleteObject (hbrush);
}

/**************************************************************************/

void display_dotted_inner_border (HDC hdc,
                                  int x,
                                  int y,
                                  int width,
                                  int height,
                                  int pen_index)
{
  HPEN  holdpen;
  POINT pts[2];
  int   px, py, dot;

  holdpen = SelectObject (hdc, hpen[pen_index]);

  dot = 0;
  clear pts;

  /* horizontal line */
  py = y + 2;
  for (px=x+2; px<x+width-2; px++)
  {
    dot ^= 0x01;
    if (dot != 0)
    {
      pts[0].x = px;
      pts[0].y = py;
      pts[1].y = py;
      pts[1].x = pts[0].x + 1;

      Polyline (hdc, &pts, 2);
    }
  }

  /* vertical line */
  for (; py<y+height-2; py++)
  {
    dot ^= 0x01;
    if (dot != 0)
    {
      pts[0].x = px;
      pts[0].y = py;
      pts[1].y = py;
      pts[1].x = pts[0].x + 1;

      Polyline (hdc, &pts, 2);
    }
  }

  /* horizontal line */
  for (; px>x+2; px--)
  {
    dot ^= 0x01;
    if (dot != 0)
    {
      pts[0].x = px;
      pts[0].y = py;
      pts[1].y = py;
      pts[1].x = pts[0].x + 1;

      Polyline (hdc, &pts, 2);
    }
  }

  /* vertical line */
  for (; py>y+2; py--)
  {
    dot ^= 0x01;
    if (dot != 0)
    {
      pts[0].x = px;
      pts[0].y = py;
      pts[1].y = py;
      pts[1].x = pts[0].x + 1;

      Polyline (hdc, &pts, 2);
    }
  }

  SelectObject (hdc, holdpen);
}

/**************************************************************************/

public void display_button (HDC    hdc,
                            int    x,
                            int    y,
                            int    width,
                            int    height,   /* should be >= 4 + text_height */
                            string text,
                            bool   pressed,
                            bool   has_focus,
                            uint   text_color)
{
  RECT     rect;
  SIZE     size;
  int      length = strlen(text);
  COLORREF old_tx_color /*,old_bk_color*/ ;


  /* text rectangle */

  rect = {left   => x + 1 + (int)pressed,
          top    => y + 1 + (int)pressed,
          right  => x + width  - 2 + (int)pressed,
          bottom => y + height - 2 + (int)pressed};

  GetTextExtentPoint32A (hdc, &text, length, &size);

  old_tx_color = SetTextColor (hdc, text_color);
/*  old_bk_color = SetBkColor   (hdc, color_middle); */

  ExtTextOutA (hdc,
               x + (width >> 1)  - (size.cx >> 1) + (int)pressed,
               y + (height >> 1) - (size.cy >> 1) + (int)pressed,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               &text,
               (uint)length,
               null);

  SetTextColor (hdc, old_tx_color);
/*  SetBkColor (hdc, old_bk_color); */

  draw_figure (hdc, x, y, width, height, draw_button_data[(int)pressed]);

  if (has_focus)
  {
    display_dotted_inner_border (hdc,
                                 x + (int)pressed,
                                 y + (int)pressed,
                                 width - 2,
                                 height - 2,
                                 PEN_BLACK);
  }
}

/**************************************************************************/

/* align top & left */

public void display_static_text (HDC    hdc,
                                 int    x,
                                 int    y,
                                 int    width,
                                 int    height,  /* should be 4 + text_height */
                                 string text,
                                 uint   text_color)
{
  RECT     rect;
  int      length = strlen(text);
  COLORREF old_tx_color;

  old_tx_color = SetTextColor (hdc, text_color);

  /* text rectangle */

  rect = {left   => x,
          top    => y,
          right  => x + width,
          bottom => y + height};

  ExtTextOutA (hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &rect, &text, (uint)length, null);

  SetTextColor (hdc, old_tx_color);
}

/**************************************************************************/

public void display_edit (HDC    hdc,
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
                          uint   field_color)
{
  int      text_x, text_y, length;
  RECT     rect;
  SIZE     size2;
  COLORREF old_bk_color, old_tx_color;
  const char sample = 'A';

  _unused cursor_x;
  _unused begin_mark;
  _unused end_mark;

  /* text rectangle */

  rect = {left   => x + 2,
          top    => y + 2,
          right  => x + width  - 2,
          bottom => y + height - 2};

  GetTextExtentPoint32A (hdc, &sample, 1, &size2);

  text_x = x + 2 + 2;
  text_y = y + height / 2 - size2.cy / 2;

  old_tx_color = SetTextColor (hdc, text_color);
  old_bk_color = SetBkColor   (hdc, explicit_field_color ? field_color : field_background_color);

  length = strlen(text) - offset_x;
  if (length < 0)
    length = 0;
  
  ExtTextOutA (hdc,
               text_x,
               text_y,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               (&text)+offset_x,
               (uint)length,
               null);

  SetTextColor (hdc, old_tx_color);
  SetBkColor (hdc, old_bk_color);

  draw_figure (hdc, x, y, width, height, draw_box_data[HOLLOW]);
}

/**************************************************************************/

public void display_edit_caret (HWND   hwnd,
                                HDC    hdc,
                                int    x,
                                int    y,
                                int    width,
                                int    height,      /* should be 4 + text_height */
                                string text,
                                int    offset_x,    /* start display at text[offset_x]   */
                                int    cursor_x)
{
  int  text_x, text_y, length;
  SIZE size, size2;
  const char sample = 'A';

  _unused hwnd;
  _unused width;

  GetTextExtentPoint32A (hdc, &sample, 1, &size);   /* get font height */

  text_x = x + 2 + 2;
  text_y = y + height / 2 - size.cy / 2;

  length = cursor_x - offset_x;
  if (length < 0)
    length = 0;
  
  GetTextExtentPoint32A (hdc, (&text)+offset_x, length, &size2);

  gi_enable_caret (text_x + size2.cx, text_y, size.cy);
}

/**************************************************************************/

public void display_checkbox (HDC    hdc,
                              int    x,
                              int    y,
                              int    width,
                              int    height,  /* should be 4 + text_height */
                              int    box_width,
                              bool   setting,
                              string text,
                              bool   has_focus)
{
  int      length = strlen(text);
  int      text_x, text_y;
  RECT     rect;
  SIZE     size2;
  COLORREF old_bk_color, old_tx_color;
  const char sample = 'A';

  /* part 1 : checkbox 'v' sign */

  draw_figure (hdc, x, y, box_width, height, draw_box_data[HOLLOW]);
  fill_rectangle (hdc, x+2, y+2, box_width-4, height-4, field_background_color);

  if (setting)
  {
    HPEN  holdpen;
    POINT pts[3];
    int   dy;

    holdpen = SelectObject (hdc, hpen[PEN_BLACK]);

    for (dy=0; dy<height/4; dy++)
    {
      pts[0].x = x + 3;
      pts[0].y = y + height / 2 + dy - height/8;
      pts[1].x = x + box_width * 2 / 5;
      pts[1].y = y + height - 2 + (dy - (height/4 - 1));
      pts[2].x = x + box_width - 2;
      pts[2].y = y + 2 + dy;

      Polyline (hdc, &pts, 3);
    }

    SelectObject (hdc, holdpen);
  }


  /* part 2 : text rectangle */

  rect = {left   => x + box_width,
          top    => y,
          right  => x + width,
          bottom => y + height};

  GetTextExtentPoint32A (hdc, &sample, 1, &size2);

  text_x = x + box_width + 2 + 2;
  text_y = y + height / 2 - size2.cy / 2;

  old_tx_color = SetTextColor (hdc, color_black);
  old_bk_color = SetBkColor   (hdc, color_middle);

  ExtTextOutA (hdc,
               text_x,
               text_y,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               &text,
               (uint)length,
               null);

  SetTextColor (hdc, old_tx_color);
  SetBkColor (hdc, old_bk_color);

  if (has_focus)
  {
    display_dotted_inner_border (hdc,
                                 rect.left - 1,
                                 rect.top  - 1,
                                 rect.right - rect.left + 2,
                                 rect.bottom - rect.top + 2,
                                 PEN_BLACK);
  }
}

/**************************************************************************/

public void display_set_default_colors (HDC hdc)
{
  SetTextColor (hdc, color_black);
  SetBkColor (hdc, color_middle);
}

/**************************************************************************/

public void display_screen (HDC    hdc,
                            int    x,
                            int    y,
                            int    width,
                            int    height,
                            string title,
                            int    title_height)
{
  RECT     rect;
  int      length = strlen(title);
  SIZE     size;
  COLORREF old_bk_color, old_tx_color;

  if (title_height > 0)   /* we want a title */
  {
    /* text rectangle */

    clear rect;
    rect.left   = x + 2;
    rect.top    = y + 2;
    rect.right  = x + width - 2;
    rect.bottom = rect.top + title_height;

    GetTextExtentPoint32A (hdc, &title, length, &size);

    old_bk_color = SetBkColor   (hdc, COLOR_SCREEN_CAPTION_BACKGROUND);
    old_tx_color = SetTextColor (hdc, COLOR_SCREEN_CAPTION_TEXT);

    ExtTextOutA (hdc,
                 rect.left + ((rect.right - rect.left) >> 1) - (size.cx >> 1),
                 rect.top + ((rect.bottom - rect.top) >> 1) - (size.cy >> 1),
                 ETO_CLIPPED | ETO_OPAQUE,
                 &rect,
                 &title,
                 (uint)length,
                 null);

    SetBkColor (hdc, old_bk_color);
    SetTextColor (hdc, old_tx_color);
  }

  draw_figure (hdc, x, y, width, height, draw_box_data[MOUNTAIN]);

  /* fill screen area with middle grey color */
  fill_rectangle (hdc, x+2, y+2+title_height, width-4, height-4-title_height, color_middle);
}

/**************************************************************************/

public void display_main_screen (HDC hdc, int x, int y, int width, int height)
{
  /* fill screen area with middle grey color */
  fill_rectangle (hdc, x, y, width, height, color_middle);
}

/**************************************************************************/

void draw_black_arrow
               (HDC hdc,
                int x, int y, int width, int height,
                int direction)  /* +1 = down arrow, -1 = up arrow */

{
  HPEN   holdpen;
  POINT  pts[2];
  int    mx, my, d;

  holdpen = SelectObject (hdc, hpen[PEN_BLACK]);

  mx = x + (width >> 1);
  my = y + (height >> 1);

  for (d=3; d>=0; d--)
  {
    clear pts;

    pts[0].x = mx - d;
    pts[1].x = mx + d + 1;

    if (pts[0].x < x)
      pts[0].x = x;

    if (pts[1].x >= x + width)
      pts[1].x = x + width - 1;

    pts[1].y = my - direction * (d - 2);
    pts[0].y = pts[1].y;

    if (pts[0].y >= y + height)
    {
      pts[1].y = y + height - 1;
      pts[0].y = pts[1].y;
    }

    Polyline (hdc, &pts, 2);
  }

  SelectObject (hdc, holdpen);
}

/**************************************************************************/

public void compute_scrollbox_data
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
   out int scrollbox_height)
{
  int  scrollbox_position_num;
  int  scrollbox_position_den;
  int  scrollbox_height_num;
  int  scrollbox_height_den;
  int  move_height;

  sx = x + width - 2 - scrollbar_width;
  sy = y + 2 + scrollbar_arrow_box_height;
  sw = scrollbar_width;
  sh = height - 4 - 2 * scrollbar_arrow_box_height;


  scrollbox_height_num = nb_screen_lines;
  scrollbox_height_den = nb_listbox_lines;

  if (scrollbox_height_den > 0)
    scrollbox_height = sh * scrollbox_height_num / scrollbox_height_den;
  else
    scrollbox_height = sh;

  if (scrollbox_height < 4)
    scrollbox_height = 4;
  if (scrollbox_height > sh)
    scrollbox_height = sh;

  /* compute sum of free area above & below box */
  move_height = sh - scrollbox_height;


  scrollbox_position_num = current_page_nr - 1;    /* [0..max_page[ */
  scrollbox_position_den = nb_listbox_lines - nb_screen_lines;

  if (scrollbox_position_den > 0)
    scrollbox_y_offset = move_height * scrollbox_position_num / scrollbox_position_den;
  else
    scrollbox_y_offset = 0;

  if (scrollbox_y_offset < 0)
    scrollbox_y_offset = 0;
  if (scrollbox_y_offset > move_height)
    scrollbox_y_offset = move_height;
}

/**************************************************************************/

public void display_listbox (HDC hdc,
                             int x,
                             int y,
                             int width,
                             int height,   /* must be (4+nb_lines*text_height) */
                             int scrollbar_width,
                             int scrollbar_arrow_box_height,
                             int current_page_nr,
                             int nb_listbox_lines,
                             int nb_screen_lines)
{
  int sx, sy, sw, sh, scrollbox_y_offset, scrollbox_height, ty;

  draw_figure (hdc, x, y, width, height, draw_box_data[HOLLOW]);

  if (scrollbar_width < 4)
    return;

  /* upper arrow box */

  draw_figure (hdc,
               x+width-2-scrollbar_width,
               y+2,
               scrollbar_width,
               scrollbar_arrow_box_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+width-2-scrollbar_width+2,
                  y+4,
                  scrollbar_width-4,
                  scrollbar_arrow_box_height-4,
                  color_middle);

  draw_black_arrow (hdc,
                    x+width-2-scrollbar_width+2,
                    y+4,
                    scrollbar_width-4,
                    scrollbar_arrow_box_height-4,
                    -1);

  /* lower arrow box */

  draw_figure (hdc,
               x+width-2-scrollbar_width,
               y+height-2-scrollbar_arrow_box_height,
               scrollbar_width,
               scrollbar_arrow_box_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+width-2-scrollbar_width+2,
                  y+height-2-scrollbar_arrow_box_height+2,
                  scrollbar_width-4,
                  scrollbar_arrow_box_height-4,
                  color_middle);

  draw_black_arrow (hdc,
                    x+width-2-scrollbar_width+2,
                    y+height-2-scrollbar_arrow_box_height+2,
                    scrollbar_width-4,
                    scrollbar_arrow_box_height-4,
                    1);

  /* moveable scroll box */

  compute_scrollbox_data (x, y, width, height,
                          scrollbar_width, scrollbar_arrow_box_height,
                          current_page_nr, nb_listbox_lines, nb_screen_lines,
                          out sx, out sy, out sw, out sh,
                          out scrollbox_y_offset, out scrollbox_height);

  _unused sh;

  draw_figure (hdc, sx, sy + scrollbox_y_offset,
               sw, scrollbox_height, draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  sx + 2,
                  sy + scrollbox_y_offset + 2,
                  sw - 4,
                  scrollbox_height - 4,
                  color_middle);

  /* grey zones */

  ty = y + 2 + scrollbar_arrow_box_height;
  fill_rectangle (hdc,
                  x+width-2-scrollbar_width,
                  ty,
                  scrollbar_width,
                  sy + scrollbox_y_offset - ty,
                  color_light);

  ty = sy + scrollbox_y_offset + scrollbox_height;
  fill_rectangle (hdc,
                  x+width-2-scrollbar_width,
                  ty,
                  scrollbar_width,
                  y + height - 2 - scrollbar_arrow_box_height - ty,
                  color_light);
}

/**************************************************************************/

public void display_listbox_line (HDC    hdc,
                                  int    x,
                                  int    y,
                                  int    width,
                                  int    height,
                                  string text,
                                  bool   is_selected,
                                  bool   has_focus)
{
  uint  ink, paper, old_bk_color, old_tx_color;
  int   length = strlen(text);
  RECT  rect;

  if (is_selected)
  {
    ink   = field_background_color;
    paper = color_black;
  }
  else
  {
    ink   = color_black;
    paper = field_background_color;
  }

  old_tx_color = SetTextColor (hdc, ink);
  old_bk_color = SetBkColor   (hdc, paper);

  rect = {left   => x,
          top    => y,
          right  => x + width,
          bottom => y + height};

  ExtTextOutA (hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &rect, &text, (uint)length, null);

  SetTextColor (hdc, old_tx_color);
  SetBkColor   (hdc, old_bk_color);

  if (has_focus)
    display_dotted_inner_border (hdc, x-2, y-2, width+3, height+3, PEN_BLACK);
}

/**************************************************************************/

public
void compute_scrollbox_data2
      (    int height,           // must be (nb_lines*text_height)
           int scrollbar_arrow_box_height,
           int current_page_nr,
           int nb_listbox_lines,
           int nb_screen_lines,
       out int scrollbox_y_offset,
       out int scrollbox_height)
{
  int  scrollbox_position_num;
  int  scrollbox_position_den;
  int  scrollbox_height_num;
  int  scrollbox_height_den;
  int  move_height;
  int  sh;

  sh = height - 2 * scrollbar_arrow_box_height;

  scrollbox_height_num = nb_screen_lines;
  scrollbox_height_den = nb_listbox_lines;

  if (scrollbox_height_den > 0)
    scrollbox_height = sh * scrollbox_height_num / scrollbox_height_den;
  else
    scrollbox_height = sh;

  if (scrollbox_height < 4)
    scrollbox_height = 4;
  if (scrollbox_height > sh)
    scrollbox_height = sh;

  // compute sum of free area above & below box
  move_height = sh - scrollbox_height;


  scrollbox_position_num = current_page_nr - 1;    // [0..max_page[
  scrollbox_position_den = nb_listbox_lines - nb_screen_lines;

  if (scrollbox_position_den > 0)
    scrollbox_y_offset = move_height * scrollbox_position_num / scrollbox_position_den;
  else
    scrollbox_y_offset = 0;

  if (scrollbox_y_offset < 0)
    scrollbox_y_offset = 0;
  if (scrollbox_y_offset > move_height)
    scrollbox_y_offset = move_height;
}

//--------------------------------------------------------------------------

public
void display_vertical_scrollbar (
        HDC hdc,
        int x,
        int y,
        int scrollbar_width,
        int height,   // must be (nb_lines*text_height)
        int scrollbar_arrow_box_height,
        int current_page_nr,
        int nb_listbox_lines,
        int nb_screen_lines)
{
  int sx, sy, scrollbox_y_offset, scrollbox_height, ty;

  if (scrollbar_width < 4)
    return;

  // upper arrow box

  draw_figure (hdc,
               x,
               y,
               scrollbar_width,
               scrollbar_arrow_box_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+2,
                  y+2,
                  scrollbar_width-4,
                  scrollbar_arrow_box_height-4,
                  color_middle);

  draw_black_arrow (hdc,
                    x+2,
                    y+2,
                    scrollbar_width-4,
                    scrollbar_arrow_box_height-4,
                    -1);

  // lower arrow box

  draw_figure (hdc,
               x,
               y+height-scrollbar_arrow_box_height,
               scrollbar_width,
               scrollbar_arrow_box_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+2,
                  y+height-scrollbar_arrow_box_height+2,
                  scrollbar_width-4,
                  scrollbar_arrow_box_height-4,
                  color_middle);

  draw_black_arrow (hdc,
                    x+2,
                    y+height-scrollbar_arrow_box_height+2,
                    scrollbar_width-4,
                    scrollbar_arrow_box_height-4,
                    1);

  compute_scrollbox_data2 (    height,    // scroll area height (minus frame borders)
                               scrollbar_arrow_box_height,
                               current_page_nr,
                               nb_listbox_lines,
                               nb_screen_lines,
                           out scrollbox_y_offset,
                           out scrollbox_height);

  sx = x;
  sy = y + scrollbar_arrow_box_height;

  draw_figure (hdc, sx, sy + scrollbox_y_offset,
               scrollbar_width, scrollbox_height, draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  sx + 2,
                  sy + scrollbox_y_offset + 2,
                  scrollbar_width - 4,
                  scrollbox_height - 4,
                  color_middle);

  // grey zones

  ty = y + scrollbar_arrow_box_height;
  fill_rectangle (hdc,
                  x,
                  ty,
                  scrollbar_width,
                  sy + scrollbox_y_offset - ty,
                  color_light);

  ty = sy + scrollbox_y_offset + scrollbox_height;
  fill_rectangle (hdc,
                  x,
                  ty,
                  scrollbar_width,
                  y + height - scrollbar_arrow_box_height - ty,
                  color_light);
}

//--------------------------------------------------------------------------

void draw_horizontal_line (HDC hdc,
                           int x, int y, int width,
                           int pen_index)
{
  POINT pts[2];
  HPEN  holdpen;

  pts[0].x = x;
  pts[0].y = y;
  pts[1].x = x + width;
  pts[1].y = y;

  holdpen = SelectObject (hdc, hpen[pen_index]);

  Polyline (hdc, &pts, 2);

  SelectObject (hdc, holdpen);
}

/**************************************************************************/

public void display_menu (HDC    hdc,
                          int    x,
                          int    y,
                          int    width,
                          int    height,   /* must be (2+text_height) */
                          string text,
                          bool   has_focus,
                          bool   pressed,
                          int    hotkey_index)  /* index within 'text' or -1 */
{
  RECT     rect;
  int  length = strlen(text);
  SIZE     size;
  int      text_x, text_y;
  COLORREF old_bk_color, old_tx_color;


  /* text rectangle */

  GetTextExtentPoint32A (hdc, &text, length, &size);

  old_bk_color = SetBkColor   (hdc, COLOR_MENU_BACKGROUND);
  old_tx_color = SetTextColor (hdc, COLOR_MENU_TEXT);

  rect = {left   => x,
          top    => y,
          right  => x + width,
          bottom => y + height};

  text_x = x + (width >> 1)  - (size.cx >> 1) + (int)pressed;
  text_y = y + (height >> 1) - (size.cy >> 1) + (int)pressed;

  ExtTextOutA (hdc,
               text_x,
               text_y,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               &text,
               (uint)length,
               null);

  if (hotkey_index >= 0)
  {
    int px, py, pwidth;

    GetTextExtentPoint32A (hdc, &text, hotkey_index, &size);

    px = text_x + size.cx;

    GetTextExtentPoint32A (hdc, (&text)+hotkey_index, 1, &size);

    pwidth = size.cx;
    py = text_y + size.cy - 2;

    draw_horizontal_line (hdc, px, py, pwidth, PEN_BLACK);
  }

  SetBkColor (hdc, old_bk_color);
  SetTextColor (hdc, old_tx_color);

  if (has_focus)
    draw_figure (hdc, x, y, width, height, draw_menu_data[(int)pressed]);
}

/**************************************************************************/

public void display_menu_item_box (HDC  hdc,
                                   int  x,
                                   int  y,
                                   int  width,
                                   int  height)   /* must be (6+N*text_height) */
{
  draw_figure (hdc, x, y, width, height, draw_box_data[MOUNTAIN]);
  draw_figure (hdc, x, y, width, height, draw_menu_item_inner_data);
}

/**************************************************************************/

public void display_menu_item
  (HDC    hdc,
   int    x,             /* use 'x' of menu_item_box */
   int    y,             /* 'y' of menu_item_box + N * text_height */
   int    width,         /* use width of menu_item_box */
   int    height,        /* must be text_height */
   string text,
   int    hotkey_index,  /* index within 'text' or -1 */
   bool   has_focus)
{
  uint     cbk, ctx;
  int      pen_index;
  int  length = strlen(text);
  RECT     rect;
  SIZE     size;
  int      text_x, text_y;
  COLORREF old_bk_color, old_tx_color;

  if (has_focus)
  {
    cbk = COLOR_MENU_REV_BACKGROUND;
    ctx = COLOR_MENU_REV_TEXT;
    pen_index = PEN_WHITE;
  }
  else
  {
    cbk = COLOR_MENU_BACKGROUND;
    ctx = COLOR_MENU_TEXT;
    pen_index = PEN_BLACK;
  }


  /* text rectangle */

  GetTextExtentPoint32A (hdc, &text, length, &size);

  old_bk_color = SetBkColor   (hdc, cbk);
  old_tx_color = SetTextColor (hdc, ctx);

  rect = {left   => x + 3,
          top    => y + 3,
          right  => x + 3 + width  - 6,
          bottom => y + 3 + height};

  text_x = rect.left + 2;
  text_y = rect.top + (height >> 1) - (size.cy >> 1);

  ExtTextOutA (hdc,
               text_x,
               text_y,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               &text,
               (uint)length,
               null);

  if (hotkey_index >= 0)
  {
    int px, py, pwidth;

    GetTextExtentPoint32A (hdc, &text, hotkey_index, &size);

    px = text_x + size.cx;

    GetTextExtentPoint32A (hdc, (&text)+hotkey_index, 1, &size);

    pwidth = size.cx;
    py = text_y + size.cy - 2;

    draw_horizontal_line (hdc, px, py, pwidth, pen_index);
  }

  SetBkColor (hdc, old_bk_color);
  SetTextColor (hdc, old_tx_color);
}

/**************************************************************************/
#end unsafe
/**************************************************************************/
