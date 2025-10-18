
// guidraw.c

use ../arithm, ../strings, ../text, ../edline, ../edtext;

#if WINDOWS
  use ../win/windows;
#elif ANDROID
  use guigdi;
#else
  bad
#endif

use ../gui, guitree, guiutil;

//--------------------------------------------------------------------------
#begin unsafe
//--------------------------------------------------------------------------

// pen indexes
const short PEN_BLACK   =  0;
const short PEN_DARK    =  1;
const short PEN_MIDDLE  =  2;
const short PEN_MIDDLE2 =  3;
const short PEN_LIGHT   =  4;
const short PEN_WHITE   =  5;

const int MAX_PENS = GUI_COLORS'length;

HPEN hpen[MAX_PENS];
HBRUSH hbrush[MAX_PENS];
uint pen_color[MAX_PENS];

//--------------------------------------------------------------------------

package PACKAGE_FIGURES

  struct RCOORD   // relative coordinates (add box size if negative)
  {
    int1 x, y;
  }

  struct LINE   // horizontal or vertical line
  {
    short  pen;
    RCOORD coord[2];
  }

  struct FIGURE
  {
    LINE bline[];
  }

  const FIGURE draw_button_data[2] =   /* NORMAL/PRESSED */
  { /* NORMAL  */ {   {{PEN_WHITE,   {{ 0, 0}, {-2, 0}}},   // horizontal top
                       {PEN_WHITE,   {{ 0, 1}, { 0,-2}}},   // vertical left
                       {PEN_DARK,    {{ 1,-2}, {-2,-2}}},   // horizontal but-bottom
                       {PEN_DARK,    {{-2, 1}, {-2,-3}}},   // vertical but-right
                       {PEN_BLACK,   {{ 0,-1}, {-1,-1}}},   // horizontal bottom
                       {PEN_BLACK,   {{-1, 0}, {-1,-2}}}}}, // vertical right
    /* PRESSED */ {   {{PEN_BLACK,   {{ 0, 0}, {-2, 0}}},
                       {PEN_BLACK,   {{ 0, 1}, { 0,-2}}},
                       {PEN_DARK,    {{ 1, 1}, { 1,-2}}},   // vertical but-left
                       {PEN_DARK,    {{ 2, 1}, {-2, 1}}},   // horizontal but-top
                       {PEN_WHITE,   {{ 0,-1}, {-1,-1}}},   // horizontal bottom
                       {PEN_WHITE,   {{-1, 0}, {-1,-2}}}}}  // vertical right
  };

  const int HOLLOW   = 0;
  const int MOUNTAIN = 1;

  const FIGURE draw_box_data[2] =    /* HOLLOW / MOUNTAIN */
  { /* HOLLOW  */ {   {{PEN_DARK,   {{ 0, 0}, { 0,-2}}},   // vertical left
                       {PEN_DARK,   {{ 1, 0}, {-2, 0}}},   // horizontal top
                       {PEN_BLACK,  {{ 1, 1}, { 1,-3}}},   // vertical but-left
                       {PEN_BLACK,  {{ 2, 1}, {-3, 1}}},   // horizontal but-top
                       {PEN_LIGHT,  {{-2, 1}, {-2,-2}}},   // vertical right
                       {PEN_LIGHT,  {{ 1,-2}, {-3,-2}}},   // horizontal but-bottom
                       {PEN_WHITE,  {{ 0,-1}, {-1,-1}}},   // horizontal bottom
                       {PEN_WHITE,  {{-1, 0}, {-1,-2}}}}}, // vertical right

    /* MOUNTAIN*/ {   {{PEN_LIGHT,  {{ 0, 0}, { 0,-2}}},
                       {PEN_LIGHT,  {{ 1, 0}, {-2, 0}}},
                       {PEN_WHITE,  {{ 1, 1}, { 1,-3}}},
                       {PEN_WHITE,  {{ 2, 1}, {-3, 1}}},
                       {PEN_DARK,   {{-2, 1}, {-2,-2}}},
                       {PEN_DARK,   {{ 1,-2}, {-3,-2}}},
                       {PEN_BLACK,  {{ 0,-1}, {-1,-1}}},
                       {PEN_BLACK,  {{-1, 0}, {-1,-2}}}}}
  };

  const FIGURE draw_box_1pixel =
                  {   {{PEN_BLACK,  {{ 0, 0}, { 0,-1}}},   // vertical left
                       {PEN_BLACK,  {{ 1, 0}, {-1, 0}}},   // horizontal top
                       {PEN_BLACK,  {{ 1,-1}, {-1,-1}}},   // horizontal bottom
                       {PEN_BLACK,  {{-1, 1}, {-1,-2}}}}}; // vertical right

end PACKAGE_FIGURES;

//--------------------------------------------------------------------------

void init_pens (uint[MAX_PENS] colors)
{
  int i;

  for (i=0; i<MAX_PENS; i++)
  {
#if WINDOWS
    if (hpen[i] != 0 && pen_color[i] == colors[i])    // already OK
      continue;

    if (hpen[i] != 0)
    {
      DeleteObject (hpen[i]);
      DeleteObject (hbrush[i]);
    }

    hpen[i]      = CreatePen (PS_SOLID, 1, colors[i]);
    hbrush[i]    = CreateSolidBrush (colors[i]);
    pen_color[i] = colors[i];

#elif ANDROID
    {
      uint c = colors[i] | 0xFF000000;
      hpen[i]      = c;
      hbrush[i]    = c;
      pen_color[i] = c;
    }

#else
    bad
#endif

  }
}

//--------------------------------------------------------------------------

void free_pens ()
{
#if WINDOWS
  int i;
  for (i=0; i<MAX_PENS; i++)
  {
    if (hpen[i] != 0)
    {
      DeleteObject (hpen[i]);
      DeleteObject (hbrush[i]);
      hpen[i] = 0;
      hbrush[i] = 0;
    }
  }
#endif
}

//--------------------------------------------------------------------------

// draw figure using current pen colors

void draw_figure (HDC    hdc,
                  int    x, int y, int width, int height,
                  FIGURE i)
{
  bool colors_inversed = (pen_color[PEN_BLACK] > pen_color[PEN_WHITE]);

  if (g_scale == 1)
  {
    int   count, p, idx, fx, fy;
    POINT pts[2];
    HPEN  holdpen;

    clear pts;

    for (count=0; count<i.bline'length; count++)
    {
      for (p=0; p<2; p++)
      {
        fx = i.bline[count].coord[p].x;
        if (fx < 0)
          fx += width;
        pts[p].x = x + fx;

        fy = i.bline[count].coord[p].y;
        if (fy < 0)
          fy += height;
        pts[p].y = y + fy;
      }

      if (pts[0].x != pts[1].x)  // horizontal line
        pts[1].x++;  // Polyline does not draw the end-point, so we append one point
      else                       // vertical line
        pts[1].y++;  // same

      idx = i.bline[count].pen;
      if (colors_inversed)
        idx = MAX_PENS - 1 - idx;

#if WINDOWS
      holdpen = SelectObject (hdc, hpen[idx]);
#elif ANDROID
      holdpen = SelectPen (hdc, hpen[idx]);
#endif

      Polyline (hdc, &pts, 2);

#if WINDOWS
      SelectObject (hdc, holdpen);
#elif ANDROID
      SelectPen (hdc, holdpen);
#endif
    }
  }
  else   // g_scale > 1
  {
    int    count, idx, fx, fy;
    RECT   rect;

    clear rect;

    for (count=0; count<i.bline'length; count++)
    {
      fx = i.bline[count].coord[0].x * g_scale;
      if (fx < 0)
        fx += width;
      rect.left = x + fx;

      fy = i.bline[count].coord[0].y * g_scale;
      if (fy < 0)
        fy += height;
      rect.top = y + fy;

      fx = i.bline[count].coord[1].x * g_scale;
      if (fx < 0)
        fx += width;
      rect.right = x + fx;

      fy = i.bline[count].coord[1].y * g_scale;
      if (fy < 0)
        fy += height;
      rect.bottom = y + fy;

      rect.right += g_scale;  // FillRect does not draw the bottom/right corner, so we append one
      rect.bottom += g_scale;

      idx = i.bline[count].pen;
      if (colors_inversed)
        idx = MAX_PENS - 1 - idx;

      FillRect (hdc, &rect, hbrush[idx]);
    }
  }
}

//--------------------------------------------------------------------------

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

#if WINDOWS
  DeleteObject (hbrush);
#endif
}

//--------------------------------------------------------------------------

void display_dotted_inner_border (HDC hdc,
                                  int x,
                                  int y,
                                  int width,
                                  int height,
                                  HPEN hpen)
{
  HPEN  holdpen;
  POINT pts[2];
  int   px, py, dot;

#if WINDOWS
  holdpen = SelectObject (hdc, hpen);
#elif ANDROID
  holdpen = SelectPen (hdc, hpen);
#endif

  dot = 0;
  clear pts;

  // horizontal line
  py = y + ui_scale(2);
  for (px=x+ui_scale(2); px<x+width-ui_scale(2); px++)
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

  // vertical line
  for (; py<y+height-ui_scale(2); py++)
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

  // horizontal line
  for (; px>x+ui_scale(2); px--)
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

  // vertical line
  for (; py>y+ui_scale(2); py--)
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

#if WINDOWS
  SelectObject (hdc, holdpen);
#elif ANDROID
  SelectPen (hdc, holdpen);
#endif
}

//--------------------------------------------------------------------------

void display_dialog
 (HWND    hwnd,
  HDC     hdc,

#if ANDROID
  wstring title,
#endif

  int     x,
  int     y,
  int     width,
  int     height,
  int     title_height,
  int     border_size,
  bool    has_close_button)
{
  RECT     rect, full_rect;
  int      length;
  SIZE     size;
  COLORREF old_bk_color;

#if WINDOWS
  wchar    title[120];
#elif ANDROID
  _unused hwnd;
#else
  bad
#endif

  if (title_height > 0)   // we want a title
  {
    clear full_rect;
    full_rect.left   = x + border_size;
    full_rect.top    = y + border_size;
    full_rect.right  = x + width - border_size;
    full_rect.bottom = full_rect.top + title_height;

    rect = full_rect;
    if (has_close_button)
      rect.right -= title_height;

#if WINDOWS
    SendMessagePtrW (hwnd, WM_GETTEXT, (uint)title'length, &title);
    title[title'length-1] = Lnul;
#endif

    length = wstrlen(title);

    GetTextExtentPoint32W (hdc, &title, length, &size);

    old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE]);

    ExtTextOutW (hdc,
                 rect.left + 1,   // rect.left + ((rect.right - rect.left) >> 1) - (size.cx >> 1),
                 rect.top + ((rect.bottom - rect.top) >> 1) - (size.cy >> 1),
                 ETO_CLIPPED | ETO_OPAQUE,
                 &rect,
                 &title,
                 (uint)length,
                 null);

    if (has_close_button)
    {
      wchar X = L'X';
      rect = full_rect;
      rect.left = rect.right - title_height;

      GetTextExtentPoint32W (hdc, &X, 1, &size);

      ExtTextOutW (hdc,
                   rect.left + ((rect.right - rect.left) >> 1) - (size.cx >> 1),
                   rect.top + ((rect.bottom - rect.top) >> 1) - (size.cy >> 1),
                   ETO_CLIPPED | ETO_OPAQUE,
                   &rect,
                   &X,
                   1,
                   null);
    }

    SetBkColor (hdc, old_bk_color);
    ExcludeClipRect (hdc, full_rect.left, full_rect.top, full_rect.right, full_rect.bottom);
  }

  if (border_size >= 2)
    draw_figure (hdc, x, y, width, height, draw_box_data[MOUNTAIN]);
  else if (border_size == 1)
    draw_figure (hdc, x, y, width, height, draw_box_1pixel);

  ExcludeClipRect (hdc, x, y, x+width, y+border_size);                // top
  ExcludeClipRect (hdc, x, y, x+border_size, y+height);               // left
  ExcludeClipRect (hdc, x+width-border_size, y, x+width, y+height);   // right
  ExcludeClipRect (hdc, x, y+height-border_size, x+width, y+height);  // bottom
}

//--------------------------------------------------------------------------

// display button :
//   text (current text color)
//   grey background
//   2-pixel border around it
//   black dotted focus border

void display_button (HDC     hdc,
                     int     x,
                     int     y,
                     int     width,
                     int     height,   // should be >= 4 + text_height
                     wstring text,
                     bool    pressed,
                     bool    has_focus)
{
  RECT  rect;
  SIZE  size;
  int      length = wstrlen(text);
  COLORREF old_bk_color;

  rect = {left   => x + 1 + (int)pressed,
          top    => y + 1 + (int)pressed,
          right  => x + width  - 2 + (int)pressed,
          bottom => y + height - 2 + (int)pressed};

  GetTextExtentPoint32W (hdc, &text, length, &size);

  old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE]);

  ExtTextOutW (hdc,
               x + (width >> 1)  - (size.cx >> 1) + (int)pressed,
               y + (height >> 1) - (size.cy >> 1) + (int)pressed,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               &text,
               (uint)length,
               null);

  SetBkColor (hdc, old_bk_color);

  draw_figure (hdc, x, y, width, height, draw_button_data[(int)pressed]);

  if (has_focus)
  {
    display_dotted_inner_border (hdc,
                                 x + (int)pressed,
                                 y + (int)pressed,
                                 width - 2,
                                 height - 2,
                                 hpen[PEN_BLACK]);
  }
}

//--------------------------------------------------------------------------

// text (current text color)
// align top & left

void display_static_text (HDC     hdc,
                          int     x,
                          int     y,
                          int     width,
                          int     height,  // should be 4 + text_height
                          wstring text)
{
  RECT rect;
  int  length = wstrlen(text);
  uint old_bk_color;

  rect = {left   => x,
          top    => y,
          right  => x + width,
          bottom => y + height};

  old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE]);

  ExtTextOutW (hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &rect, &text, (uint)length, null);

  SetBkColor (hdc, old_bk_color);
}

//--------------------------------------------------------------------------

void draw_text (HDC     hdc,
                int     x,
                int     y,
                wstring text,
                bool    mark_reverse,
                RECT    rect,
                uint    text_color)
{
  uint old_bk_color, old_tx_color;
  int  length = wstrlen(text);

  if (mark_reverse)
  {
    old_tx_color = SetTextColor (hdc, pen_color[PEN_MIDDLE2]);
    old_bk_color = SetBkColor (hdc, text_color);
  }
  else
  {
    old_tx_color = 0;  // ignored, is already set to o.font.color
    old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE2]);
  }

  ExtTextOutW (hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &rect, &text, (uint)length, null);

  if (mark_reverse)
  {
    SetTextColor (hdc, old_tx_color);
    SetBkColor (hdc, old_bk_color);
  }
  else
  {
    SetBkColor (hdc, old_bk_color);
  }
}

//--------------------------------------------------------------------

// text (current color)
// white background
// hollow border

void display_edit (HDC       hdc,
                   int       x,
                   int       y,
                   int       width,
                   int       height,      // should be 4 + text_height
                   wstring   text,
                   int       offset_x,    // start display at text[offset_x]
                   LINE_MARK first,
                   LINE_MARK last,
                   uint      text_color)
{
  int   text_x, text_y, length, col1, col2, ex;
  RECT  rect;
  SIZE  size2;

  const wchar SAMPLE = L'A';

  length = wstrlen(text);

  GetTextExtentPoint32W (hdc, &SAMPLE, 1, &size2);

  text_x = x + 2 + 1;   // leave 1 pixel so the text does not stick to the border
  text_y = y + height / 2 - size2.cy / 2;

  // text zone inside edit frame
  ex = x + width - 2;
  rect = {left   => x + 2,
          top    => y + 2,
          right  => ex,
          bottom => y + height - 2};

  if (!first.active && !last.active)   // no marks
  {
    int len = max (0, length - offset_x);
    draw_text (hdc, text_x, text_y, (&text)[offset_x:len], mark_reverse => false, rect, text_color);
  }
  else
  {
    if (first.active)
      col1 = first.col;
    else
      col1 = 0;

    if (col1 < offset_x)
      col1 = offset_x;

    if (last.active)
      col2 = last.col;
    else
      col2 = length;

    if (col2 > length)
      col2 = length;

    if (col1 >= col2)
    {
      col1 = offset_x;
      col2 = offset_x;
    }

    // display a normal part from offset_x to col1
    if (col1 > offset_x)
    {
      GetTextExtentPoint32W (hdc, &text+offset_x, col1-offset_x, &size2);

      rect.right = min (ex, text_x + size2.cx);

      draw_text (hdc, text_x, text_y, (&text)[offset_x:col1-offset_x], mark_reverse => false, rect, text_color);

      text_x += size2.cx;
      rect.left = text_x;
      rect.right = ex;
    }

    // display a reverse part from col1 to col2
    if (col2 > col1 && rect.left < rect.right)
    {
      GetTextExtentPoint32W (hdc, &text+col1, col2-col1, &size2);

      rect.right = min (ex, text_x + size2.cx);

      draw_text (hdc, text_x, text_y, (&text)[col1:col2-col1], mark_reverse => true, rect, text_color);

      text_x += size2.cx;
      rect.left = text_x;
      rect.right = ex;
    }

    // display a normal part from col2 to length
    if (rect.left < rect.right)
    {
      draw_text (hdc, text_x, text_y, (&text)[col2:length-col2], mark_reverse => false, rect, text_color);
    }
  }

  draw_figure (hdc, x, y, width, height, draw_box_data[HOLLOW]);
}

//--------------------------------------------------------------------------

// black V
// text in current color
// grey background
// hollow border
// dotted border if focus

void display_checkbox (
      HDC     hdc,
      int     x,
      int     y,
      int     width,
      int     height,  // should be 4 + text_height
      int     box_width,
      bool    setting,
      wstring text,
      bool    has_focus)
{
  int      length = wstrlen(text);
  int      text_x, text_y;
  RECT     rect;
  SIZE     size2;
  COLORREF old_bk_color;
  const wchar sample = L'A';

  // part 1 : checkbox 'v' sign

  draw_figure (hdc, x, y, box_width, height, draw_box_data[HOLLOW]);
  fill_rectangle (hdc, x+ui_scale(2), y+ui_scale(2), box_width-ui_scale(4), height-ui_scale(4), pen_color[PEN_MIDDLE]);

  if (setting)
  {
    HPEN  holdpen;
    POINT pts[3];
    int   dy;

#if WINDOWS
    holdpen = SelectObject (hdc, hpen[PEN_BLACK]);
#elif ANDROID
    holdpen = SelectPen (hdc, hpen[PEN_BLACK]);
#endif

    for (dy=0; dy<height/4; dy++)
    {
      pts[0].x = x + ui_scale(3);
      pts[0].y = y + height / 2 + dy - height/8;
      pts[1].x = x + box_width * 2 / 5;
      pts[1].y = y + height - ui_scale(2) + (dy - (height/4 - ui_scale(1)));
      pts[2].x = x + box_width - ui_scale(2);
      pts[2].y = y + ui_scale(2) + dy;

      Polyline (hdc, &pts, 3);
    }

#if WINDOWS
    SelectObject (hdc, holdpen);
#elif ANDROID
    SelectPen (hdc, holdpen);
#endif
  }


  // part 2 : text rectangle

  rect = {left   => x + box_width,
          top    => y,
          right  => x + width,
          bottom => y + height};

  GetTextExtentPoint32W (hdc, &sample, 1, &size2);

  text_x = x + box_width + ui_scale(2) + ui_scale(2);
  text_y = y + height / 2 - size2.cy / 2;

  old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE]);

  ExtTextOutW (hdc,
               text_x,
               text_y,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               &text,
               (uint)length,
               null);

  SetBkColor (hdc, old_bk_color);

  if (has_focus)
  {
    display_dotted_inner_border (hdc,
                                 rect.left - 1,
                                 rect.top  - 1,
                                 rect.right - rect.left + 2,
                                 rect.bottom - rect.top + 2,
                                 hpen[PEN_BLACK]);
  }
}

//--------------------------------------------------------------------------

// black O
// text in current color
// grey background
// hollow border
// dotted border if focus

void display_radiobutton (
      HDC     hdc,
      int     x,
      int     y,
      int     width,
      int     height,  // should be 4 + text_height
      int     box_width,
      bool    setting,
      wstring text,
      bool    has_focus)
{
  int      length = wstrlen(text);
  HPEN     holdpen;
  HGDIOBJ  oldbrush;
  int      text_x, text_y;
  RECT     rect;
  SIZE     size2;
  COLORREF old_bk_color;
  const wchar sample = L'A';
  int OFS = ui_scale(3);
  int DY = ui_scale(2);

  // part 1 : 'O' sign

  fill_rectangle (hdc, x, y, box_width, height, pen_color[PEN_MIDDLE]);

#if WINDOWS
  oldbrush = SelectObject (hdc, hbrush[PEN_MIDDLE2]);
  holdpen = SelectObject (hdc, hpen[PEN_MIDDLE2]);
#elif ANDROID
  oldbrush = SelectBrush (hdc, hbrush[PEN_MIDDLE2]);
  holdpen = SelectPen (hdc, hpen[PEN_MIDDLE2]);
#endif

  Ellipse(hdc, x+(ui_scale(2)+OFS), y+(ui_scale(2)+OFS+DY), x+box_width+(-ui_scale(4)-OFS), y+height+(-ui_scale(4)-OFS+DY));

#if WINDOWS
  SelectObject (hdc, hpen[PEN_LIGHT]);
#elif ANDROID
  SelectPen (hdc, hpen[PEN_LIGHT]);
#endif

  Arc (hdc, x+(ui_scale(2)+OFS), y+(ui_scale(2)+OFS+DY), x+box_width+(-ui_scale(4)-OFS), y+height+(-ui_scale(4)-OFS+DY),
       nXStartArc => x+(ui_scale(2)+OFS), nYStartArc => y+height+(-ui_scale(4)-OFS+DY), nXEndArc => x+box_width+(-ui_scale(4)-OFS), nYEndArc => y+(ui_scale(2)+OFS+DY));

#if WINDOWS
  SelectObject (hdc, hpen[PEN_DARK]);
#elif ANDROID
  SelectPen (hdc, hpen[PEN_DARK]);
#endif

  Arc (hdc, x+(ui_scale(2)+OFS), y+(ui_scale(2)+OFS+DY), x+box_width+(-ui_scale(4)-OFS), y+height+(-ui_scale(4)-OFS+DY),
       nXStartArc => x+box_width+(-ui_scale(4)-OFS), nYStartArc => y+(ui_scale(2)+OFS+DY), nXEndArc => x+(ui_scale(2)+OFS), nYEndArc => y+height+(-ui_scale(4)-OFS+DY));

  if (setting)
  {
    int D = ui_scale(2);

#if WINDOWS
    SelectObject (hdc, hbrush[PEN_BLACK]);
    SelectObject (hdc, hpen[PEN_BLACK]);
#elif ANDROID
    SelectBrush (hdc, hbrush[PEN_BLACK]);
    SelectPen (hdc, hpen[PEN_BLACK]);
#endif

    Ellipse(hdc, x+(ui_scale(2)+OFS+D), y+(ui_scale(2)+OFS+DY+D), x+box_width+(-ui_scale(4)-OFS-D), y+height+(-ui_scale(4)-OFS+DY-D));
  }


#if WINDOWS
  SelectObject (hdc, holdpen);
  SelectObject (hdc, oldbrush);
#elif ANDROID
  SelectPen (hdc, holdpen);
  SelectBrush (hdc, oldbrush);
#endif


  // part 2 : text rectangle

  rect = {left   => x + box_width,
          top    => y,
          right  => x + width,
          bottom => y + height};

  GetTextExtentPoint32W (hdc, &sample, 1, &size2);

  text_x = x + box_width + ui_scale(2) + ui_scale(2);
  text_y = y + height / 2 - size2.cy / 2;

  old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE]);

  ExtTextOutW (hdc,
               text_x,
               text_y,
               ETO_CLIPPED | ETO_OPAQUE,
               &rect,
               &text,
               (uint)length,
               null);

  SetBkColor (hdc, old_bk_color);

  if (has_focus)
  {
    display_dotted_inner_border (hdc,
                                 rect.left - 1,
                                 rect.top  - 1,
                                 rect.right - rect.left + ui_scale(2),
                                 rect.bottom - rect.top + ui_scale(2),
                                 hpen[PEN_BLACK]);
  }
}

//--------------------------------------------------------------------------

void display_edit_caret (HDC     hdc,
                         int     x,
                         int     y,
                         int     height,      // should be 4 + text_height
                         wstring text,
                         int     offset_x,    // start display at text[offset_x]
                         int     cursor_x)
{
  int         text_x, text_y, length;
  SIZE        size, size2;
  const wchar sample = L'A';

  GetTextExtentPoint32W (hdc, &sample, 1, &size);   // get font height

  text_x = x + ui_scale(2);
  text_y = y + height / 2 - size.cy / 2;

  length = cursor_x - offset_x;
  if (length < 0)
    length = 0;

  GetTextExtentPoint32W (hdc, (&text)+offset_x, length, &size2);

  caret_new_x      = text_x + size2.cx;
  caret_new_y      = text_y;
  caret_new_height = size.cy;
}

//--------------------------------------------------------------------------

void display_edit_or_combo_edit (HDC          hdc,
                                 int          dx,
                                 int          dy,
                                 CONTROL_INFO o,       // edit or combo control
                                 int          x_size,
                                 EDIT_INFO    edit,
                                 bool         has_focus)
{
  wstring^  line;
  int       m, length;
  LINE_MARK first, last;

  assert o.typ == TYP_EDIT || o.typ == TYP_COMBO;

  line = new wchar [edit_line_get_max_length(edit.cr.edit_line)];
  edit_line_get_line (edit.cr.edit_line, out line^, filler => L' ');
  length = edit_line_get_length (edit.cr.edit_line);

  if (edit.password_mode)
  {
    ref wstring wstr = line^;
    for (m=0; m<length; m++)
      if (wstr[m] != L' ')
        wstr[m] = L'*';
  }

  edit_line_get_first_mark (edit.cr.edit_line, out first);
  edit_line_get_last_mark  (edit.cr.edit_line, out last);

  display_edit (hdc, dx+o.x, dy+o.y, x_size, o.y_size,
                text       => line^[0 : length],
                offset_x   => edit.cr.scroll_x_offset,
                first      => first,
                last       => last,
                text_color => o.font.color);

  if (has_focus)
    display_edit_caret (hdc, dx+o.x, dy+o.y, o.y_size, line^,
                        offset_x => edit.cr.scroll_x_offset,
                        cursor_x => edit_line_get_col(edit.cr.edit_line));
  free line;
}

//--------------------------------------------------------------------

void draw_black_harrow
             (HDC hdc,
              int x, int y, int width, int height,
              int direction)  // +1 = right arrow, -1 = left arrow

{
  HPEN   holdpen;
  POINT  pts[2];
  int    mx, my, d;

#if WINDOWS
  holdpen = SelectObject (hdc, hpen[PEN_BLACK]);
#elif ANDROID
  holdpen = SelectPen (hdc, hpen[PEN_BLACK]);
#endif

  mx = x + (width >> 1);
  my = y + (height >> 1);

  for (d=3; d>=0; d--)
  {
    clear pts;

    pts[0].y = my - d;
    pts[1].y = my + d + 1;

    if (pts[0].y < y)
      pts[0].y = y;

    if (pts[1].y >= y + height)
      pts[1].y = y + height - 1;

    pts[1].x = mx - direction * (d - 2);
    pts[0].x = pts[1].x;

    if (pts[0].x >= x + width)
    {
      pts[1].x = x + width - 1;
      pts[0].x = pts[1].y;
    }

    Polyline (hdc, &pts, 2);
  }

#if WINDOWS
  SelectObject (hdc, holdpen);
#elif ANDROID
  SelectPen (hdc, holdpen);
#endif
}

//--------------------------------------------------------------------------

void draw_black_varrow
             (HDC hdc,
              int x, int y, int width, int height,
              int direction)  // +1 = down arrow, -1 = up arrow
{
  HPEN   holdpen;
  POINT  pts[2];
  int    mx, my, d;

#if WINDOWS
  holdpen = SelectObject (hdc, hpen[PEN_BLACK]);
#elif ANDROID
  holdpen = SelectPen (hdc, hpen[PEN_BLACK]);
#endif

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

#if WINDOWS
  SelectObject (hdc, holdpen);
#elif ANDROID
  SelectPen (hdc, holdpen);
#endif
}

//--------------------------------------------------------------------------

void display_horizontal_scrollbar (
        HDC hdc,
        int x,
        int y,
        int width,
        int scrollbar_height,
        int scrollbar_arrow_box_width,
        int current_page_nr,
        int nb_listbox_lines,
        int nb_screen_lines)
{
  int sx, sy, scrollbox_x_offset, scrollbox_width, tx;

  if (scrollbar_height < ui_scale(4))
    return;

  // left arrow box

  draw_figure (hdc,
               x,
               y,
               scrollbar_arrow_box_width,
               scrollbar_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+ui_scale(2),
                  y+ui_scale(2),
                  scrollbar_arrow_box_width-ui_scale(4),
                  scrollbar_height-ui_scale(4),
                  pen_color[PEN_MIDDLE]);

  draw_black_harrow (hdc,
                     x+ui_scale(2),
                     y+ui_scale(2),
                     scrollbar_arrow_box_width-ui_scale(4),
                     scrollbar_height-ui_scale(4),
                     -1);

  // right arrow box

  draw_figure (hdc,
               x+width-scrollbar_arrow_box_width,
               y,
               scrollbar_arrow_box_width,
               scrollbar_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+width-scrollbar_arrow_box_width+ui_scale(2),
                  y+ui_scale(2),
                  scrollbar_arrow_box_width-ui_scale(4),
                  scrollbar_height-ui_scale(4),
                  pen_color[PEN_MIDDLE]);

  draw_black_harrow (hdc,
                     x+width-scrollbar_arrow_box_width+ui_scale(2),
                     y+ui_scale(2),
                     scrollbar_arrow_box_width-ui_scale(4),
                     scrollbar_height-ui_scale(4),
                     1);

  compute_scrollbox_data (    width,    // scroll area height (minus frame borders)
                              scrollbar_arrow_box_width,
                              current_page_nr,
                              nb_listbox_lines,
                              nb_screen_lines,
                          out scrollbox_x_offset,
                          out scrollbox_width);

  sx = x + scrollbar_arrow_box_width;
  sy = y;

  draw_figure (hdc, sx + scrollbox_x_offset, sy,
               scrollbox_width, scrollbar_height, draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  sx + scrollbox_x_offset + ui_scale(2),
                  sy + ui_scale(2),
                  scrollbox_width - ui_scale(4),
                  scrollbar_height - ui_scale(4),
                  pen_color[PEN_MIDDLE]);

  // 2 grey zones

  tx = x + scrollbar_arrow_box_width;
  fill_rectangle (hdc,
                  tx,
                  y,
                  sx + scrollbox_x_offset - tx,
                  scrollbar_height,
                  pen_color[PEN_LIGHT]);

  tx = sx + scrollbox_x_offset + scrollbox_width;
  fill_rectangle (hdc,
                  tx,
                  y,
                  x + width - scrollbar_arrow_box_width - tx,
                  scrollbar_height,
                  pen_color[PEN_LIGHT]);
}

//--------------------------------------------------------------------------

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

  if (scrollbar_width < ui_scale(4))
    return;

  // upper arrow box

  draw_figure (hdc,
               x,
               y,
               scrollbar_width,
               scrollbar_arrow_box_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+ui_scale(2),
                  y+ui_scale(2),
                  scrollbar_width-ui_scale(4),
                  scrollbar_arrow_box_height-ui_scale(4),
                  pen_color[PEN_MIDDLE]);

  draw_black_varrow (hdc,
                     x+ui_scale(2),
                     y+ui_scale(2),
                     scrollbar_width-ui_scale(4),
                     scrollbar_arrow_box_height-ui_scale(4),
                     -1);

  // lower arrow box

  draw_figure (hdc,
               x,
               y+height-scrollbar_arrow_box_height,
               scrollbar_width,
               scrollbar_arrow_box_height,
               draw_box_data[MOUNTAIN]);

  fill_rectangle (hdc,
                  x+ui_scale(2),
                  y+height-scrollbar_arrow_box_height+ui_scale(2),
                  scrollbar_width-ui_scale(4),
                  scrollbar_arrow_box_height-ui_scale(4),
                  pen_color[PEN_MIDDLE]);

  draw_black_varrow (hdc,
                     x+ui_scale(2),
                     y+height-scrollbar_arrow_box_height+ui_scale(2),
                     scrollbar_width-ui_scale(4),
                     scrollbar_arrow_box_height-ui_scale(4),
                     1);

  compute_scrollbox_data (    height,    // scroll area height, must be (nb_lines*text_height)
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
                  sx + ui_scale(2),
                  sy + scrollbox_y_offset + ui_scale(2),
                  scrollbar_width - ui_scale(4),
                  scrollbox_height - ui_scale(4),
                  pen_color[PEN_MIDDLE]);

  // 2 grey zones

  ty = y + scrollbar_arrow_box_height;
  fill_rectangle (hdc,
                  x,
                  ty,
                  scrollbar_width,
                  sy + scrollbox_y_offset - ty,
                  pen_color[PEN_LIGHT]);

  ty = sy + scrollbox_y_offset + scrollbox_height;
  fill_rectangle (hdc,
                  x,
                  ty,
                  scrollbar_width,
                  y + height - scrollbar_arrow_box_height - ty,
                  pen_color[PEN_LIGHT]);
}

//--------------------------------------------------------------------------

void display_listbox (
        CONTROL_INFO o,    // listbox or combo
    ref LISTBOX_INFO lb,
        HDC hdc,
        int x,
        int y,
        int width,
        int height,   // must be (4+nb_lines*text_height)
        int scrollbar_width,
        int scrollbar_arrow_box_height,
        int current_page_nr,
        int nb_listbox_lines,
        int nb_screen_lines)
{
  draw_figure (hdc, x, y, width, height, draw_box_data[HOLLOW]);

  if (o.typ != TYP_COMBO || wnb_text_lines (lb.wtext) > lb.nb_screen_lines)
  {
    display_vertical_scrollbar (
          hdc,
          x + width-ui_scale(2)-scrollbar_width,
          y + ui_scale(2),
          scrollbar_width,
          height - ui_scale(4),   // must be (nb_lines*text_height)
          scrollbar_arrow_box_height,
          current_page_nr,
          nb_listbox_lines,
          nb_screen_lines);
  }
}

//--------------------------------------------------------------------------

void display_listbox_line (
            HDC     hdc,
            int     x,
            int     y,
            int     width,
            int     height,
            wstring text,
            bool    is_selected,
            bool    has_focus)
{
  uint  old_bk_color, old_tx_color;
  int   length = wstrlen(text);
  RECT  rect;

  if (is_selected)
  {
    old_tx_color = SetTextColor (hdc, pen_color[PEN_MIDDLE2]);
    old_bk_color = SetBkColor (hdc, old_tx_color);
  }
  else
  {
    old_tx_color = 0;  // ignored, is already set to o.font.color
    old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE2]);
  }

  rect = {left   => x,
          top    => y,
          right  => x + width,
          bottom => y + height};

  ExtTextOutW (hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &rect, &text, (uint)length, null);

  if (is_selected)
  {
    SetTextColor (hdc, old_tx_color);
    SetBkColor (hdc, old_bk_color);
  }
  else
  {
    SetBkColor (hdc, old_bk_color);
  }

  if (has_focus)
    display_dotted_inner_border (hdc, x-ui_scale(2), y-ui_scale(2), width+ui_scale(3), height+ui_scale(3), hpen[PEN_BLACK]);
}

//--------------------------------------------------------------------------

void redraw_listbox_tree_or_combo_lines (
        HDC           hdc,
        int           dx,
        int           dy,
        CONTROL_INFO  o,   // listbox, tree or combo
    ref LISTBOX_INFO  lb,
        int           lb_y,
        int           lb_y_size,
        int           first,
        int           last,
        bool          has_focus)
{
  int      margin, width, len, y, length, ln;
  wstring^ line;

  assert o.typ == TYP_LISTBOX || o.typ == TYP_TREE || o.typ == TYP_COMBO;

  margin = (o.typ != TYP_COMBO || wnb_text_lines (lb.wtext) > lb.nb_screen_lines)
         ? lb.arrow_box_width : 0;

  width = o.x_size - ui_scale(4) - margin;   // width of line

  len = PREFIX_WLEN + lb.line_length + 1 + MAX_TREE_LEVELS * 3;
  line = new wchar [len];

  for (y=0; ; y++)
  {
    int height = lb_y_size - ui_scale(4) - y * lb.line_height;
    if (height > lb.line_height)
      height = lb.line_height;
    if (height <= 0)
      break;

    ln = lb.page + (int)y;    // text line nr to redraw

    if (y >= first && y <= last)
    {
      // redraw the text line 'ln' on screen line 'y'
      if (ln >= 1 && ln <= wnb_text_lines (lb.wtext))
      {
        wretrieve_text_line (ref lb.wtext, ln, out line^, out length);
        length -= PREFIX_WLEN;   // printable text

        if (o.typ == TYP_TREE)
        {
          int level = (int)((PREFIX*)&line^)->level & (MAX_TREE_LEVELS-1);
          line^[PREFIX_WLEN + (level+1)*3+1 : length] = line^[PREFIX_WLEN : length];
          line^[PREFIX_WLEN : (level+1)*3+1 ] = {all => L' '};

          if (((PREFIX*)&line^)->is_folder)
          {
            line^[PREFIX_WLEN + level*3 : 3] = tree_line_is_expanded (ref lb.wtext, ln, ((PREFIX*)&line^)->level)
                                             ? L"[-]" : L"[+]";
          }

          length += (level+1)*3+1;
        }

        if (!lb.multiple_mode)
          ((PREFIX*)&line^)->selected = (lb.selected_line == ln);
      }
      else
      {
        clear *((PREFIX*)&line^);
        length = 0;
      }

      display_listbox_line
        (hdc,
         dx + o.x + ui_scale(2),
         dy + lb_y + ui_scale(2) + y * lb.line_height,
         width,
         height,
         line^[PREFIX_WLEN:length],
         is_selected => (!lb.multiple_mode && ln == lb.selected_line)
                        || (lb.multiple_mode && ((PREFIX*)&line^)->selected),
         has_focus   => has_focus && ln == lb.ln && lb.show_focus);
    }
  }

  free line;
}

//--------------------------------------------------------------------

void display_listbox_tree_or_combo_object (     HDC          hdc,
                                                int          dx,
                                                int          dy,
                                                CONTROL_INFO o,    // tree, listbox or combo
                                            ref LISTBOX_INFO lb,
                                                int          lb_y,      // listbox y
                                                int          lb_y_size, // listbox y_size
                                                RECT         r,
                                                bool         has_focus)
{
  assert o.typ == TYP_LISTBOX || o.typ == TYP_TREE || o.typ == TYP_COMBO;

  // determine which visible line-range must be redrawn, if any
  {
    int first, last;

    if (r.top <= dy+lb_y+ui_scale(2))
      first = 0;            // start at first line
    else
    {
      first = (r.top - (dy+lb_y+ui_scale(2))) / lb.line_height;
      if (first >= lb.nb_screen_lines)
        first = lb.nb_screen_lines - 1;
    }

    if (r.bottom >= dy+lb_y+lb_y_size-ui_scale(2))
      last = lb.nb_screen_lines;  // end at last line
    else
    {
      last = 1 + (r.bottom - (dy+lb_y+ui_scale(2))) / lb.line_height;
      if (last < 0)
        last = 0;
    }

    redraw_listbox_tree_or_combo_lines (hdc, dx, dy, o, ref lb, lb_y, lb_y_size, first, last, has_focus);
  }

  display_listbox (o, ref lb, hdc,
                   x               => dx+o.x,
                   y               => dy+lb_y,
                   width           => o.x_size,
                   height          => lb_y_size,
                   scrollbar_width => lb.arrow_box_width,
                   scrollbar_arrow_box_height => lb.arrow_box_height,
                   current_page_nr  => lb.page,
                   nb_listbox_lines => wnb_text_lines (lb.wtext),
                   nb_screen_lines  => lb.nb_screen_lines);
}

//--------------------------------------------------------------------

void draw_black_arrow_button
               (HDC hdc,
                int x, int y, int width, int height,
                int direction)  /* +1 = down arrow, -1 = up arrow */
{
  draw_figure (hdc, x, y, width, height, draw_box_data[MOUNTAIN]);
  fill_rectangle (hdc, x+ui_scale(2), y+ui_scale(2), width-ui_scale(4), height-ui_scale(4), pen_color[PEN_MIDDLE]);
  draw_black_varrow (hdc, x, y, width, height, direction);
}

//--------------------------------------------------------------------

void draw_text_line (int x, int y, wstring text, bool mark_reverse, HDC hdc, int size_x, int size_y, uint text_color)
{
  RECT  rect;
  uint  old_bk_color, old_tx_color;
  int   length = wstrlen(text);

  if (mark_reverse)
  {
    old_tx_color = SetTextColor (hdc, pen_color[PEN_MIDDLE2]);
    old_bk_color = SetBkColor (hdc, text_color);
  }
  else
  {
    old_tx_color = 0;
    old_bk_color = SetBkColor (hdc, pen_color[PEN_MIDDLE2]);
  }

  rect = {left => x, top => y, right => x + size_x, bottom => y + size_y};
  ExtTextOutW (hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &rect, &text, (uint)length, null);

  if (mark_reverse)
  {
    SetTextColor (hdc, old_tx_color);
    SetBkColor (hdc, old_bk_color);
  }
  else
  {
    SetBkColor (hdc, old_bk_color);
  }
}

//--------------------------------------------------------------------

void editbox_redraw_line (ref CONTROL_INFO o,
                          ref EDITBOX_INFO editbox,
                              int          ln,
                              int          x0,
                              int          y0,
                              HDC          hdc,
                          ref wstring      bufline) // temporary buffer used for building line
{
  int length, eol;
  int max_cols = 1 + o.x_size / editbox.font_width;
  int y = y0 + (ln - editbox.cur.page) * o.font.height;
  TEXT_MARK first, last;

  if (ln <= edit_text_count_lines (editbox.cur.text))
    edit_text_get_line (ref editbox.cur.text, ln, out bufline, out actual_length => length);
  else
    length = 0;  // all lines below text

  // build screen line, marking reverse text
  eol = min (length, editbox.cur.scroll + max_cols);
  if (eol < editbox.cur.scroll)
    eol = editbox.cur.scroll;

  // now, let's take care of text portions possibly displayed in reverse
  edit_text_get_first_mark (editbox.cur.text, out first);
  edit_text_get_last_mark  (editbox.cur.text, out last);

  if (ln < first.ln || ln > last.ln)
  {
    // display in normal
    draw_text_line (x => x0,
                    y => y,
                    bufline[editbox.cur.scroll : eol - editbox.cur.scroll],
                    mark_reverse => false,
                    hdc,
                    o.x_size,
                    o.font.height,
                    o.font.color);
  }
  else if (ln > first.ln && ln < last.ln)
  {
    int size_x;

    // display in reverse
    size_x = (eol - editbox.cur.scroll) * editbox.font_width;
    if (size_x > 0)
    {
      draw_text_line (x => x0,
                      y => y,
                      bufline[editbox.cur.scroll : eol - editbox.cur.scroll],
                      mark_reverse => true,
                      hdc,
                      min (o.x_size, size_x),
                      o.font.height,
                      o.font.color);
    }

    // display in normal
    if (size_x < o.x_size)
    {
      draw_text_line (x => x0 + size_x,
                      y => y,
                      L"",
                      mark_reverse => false,
                      hdc,
                      o.x_size - size_x,
                      o.font.height,
                      o.font.color);
    }
  }
  else
  {
    int col1, col2, size_x, size_x2;

    // display with markers between col1 and col2
    if (ln == first.ln)
      col1 = first.col;
    else
      col1 = 0;
    if (col1 < editbox.cur.scroll)
      col1 = editbox.cur.scroll;

    if (ln == last.ln)
      col2 = last.col;
    else
      col2 = length;
    if (col2 > eol)
      col2 = eol;

    if (col1 >= col2)
    {
      col1 = editbox.cur.scroll;
      col2 = editbox.cur.scroll;
    }

    // display first a normal part from scroll to col1
    size_x = (col1 - editbox.cur.scroll) * editbox.font_width;
    if (size_x > 0)
    {
      draw_text_line (x => x0,
                      y => y,
                      bufline[editbox.cur.scroll : col1-editbox.cur.scroll],
                      mark_reverse => false,
                      hdc,
                      min (o.x_size, size_x),
                      o.font.height,
                      o.font.color);
    }

    // display then a reverse part from col1 to col2 excluded
    if (size_x < o.x_size)
    {
      size_x2 = (col2 - col1) * editbox.font_width;

      if (size_x2 > 0)
      {
        draw_text_line (x => x0 + size_x,
                        y => y,
                        bufline[col1 : col2 - col1],
                        mark_reverse => true,
                        hdc,
                        min (o.x_size - size_x, size_x2),
                        o.font.height,
                        o.font.color);
      }

      // finally display a normal part from col2 to eol
      if (size_x + size_x2 < o.x_size)
      {
        draw_text_line (x => x0 + size_x + size_x2,
                        y => y,
                        bufline[col2 : eol - col2],
                        mark_reverse => false,
                        hdc,
                        o.x_size - size_x - size_x2,
                        o.font.height,
                        o.font.color);
      }
    }
  }
}

//--------------------------------------------------------------------

void editbox_redraw_screen (ref CONTROL_INFO o,
                            ref EDITBOX_INFO editbox,
                                int          x,
                                int          y,
                                HDC          hdc,
                                bool         only_current_line)
{
  int count = 1 + o.y_size / o.font.height;
  int ln;
  wstring^ bufline = new wchar [edit_text_get_max_line_length(editbox.cur.text)];
  for (ln=editbox.cur.page; ln<editbox.cur.page + count; ln++)
  {
    if (!only_current_line || ln == edit_text_get_ln(editbox.cur.text))
      editbox_redraw_line (ref o, ref editbox, ln, x, y, hdc, ref bufline^);
  }
  free bufline;
}

//--------------------------------------------------------------------

void display_editbox_caret (int  x,
                            int  y,
                            int  dy)
{
  caret_new_x      = x;
  caret_new_y      = y;
  caret_new_height = dy;
}

//--------------------------------------------------------------------

void display_editbox_object (     HDC          hdc,
                                  int          dx,
                                  int          dy,
                              ref CONTROL_INFO o,
                              ref EDITBOX_INFO editbox,
                                  bool         has_focus)
{
  if (edit_text_get_redraw_screen_needed (editbox.cur.text))
  {
    editbox_redraw_screen (ref o, ref editbox, x => o.x + dx, y => o.y + dy, hdc, only_current_line => false);
    edit_text_set_redraw_line_needed (ref editbox.cur.text, false);
    edit_text_set_redraw_screen_needed (ref editbox.cur.text, false);
  }
  else if (edit_text_get_redraw_line_needed (editbox.cur.text))
  {
    editbox_redraw_screen (ref o, ref editbox, x => o.x + dx, y => o.y + dy, hdc, only_current_line => true);
    edit_text_set_redraw_line_needed (ref editbox.cur.text, false);
  }

  if (has_focus)
  {
    display_editbox_caret (o.x + dx + editbox.font_width * (edit_text_get_col(editbox.cur.text) - editbox.cur.scroll),
                           o.y + dy + o.font.height * (edit_text_get_ln(editbox.cur.text) - editbox.cur.page),
                           o.font.height);
  }
}

//--------------------------------------------------------------------

void draw_open_combobox (    HDC          hdc,
                             PAINTSTRUCT  ps,
                             DIALOG_INFO  d,
                         ref HFONT        old_font,
                         ref HFONT        new_font,
                             int          dx,
                             int          dy)
{
  // draw any open combobox (listbox)

  if (d.focus != null && d.focus^.typ == TYP_COMBO)
  {
    ref CONTROL_INFO(TYP_COMBO) o = d.focus^;
    int x, y, sx, sy;

    x  = dx + o.x;
    y  = dy + o.combo.lb.y;
    sx = o.x_size;
    sy = o.combo.lb.y_size;

    if (!o.hide && o.combo.lb.listbox_shown &&
        x <= ps.rcPaint.right && x + sx >= ps.rcPaint.left && y <= ps.rcPaint.bottom && y + sy >= ps.rcPaint.top)
    {
      init_pens (o.colors);

      SetTextColor (hdc, o.font.color);

#if WINDOWS
      new_font = get_cached_font (o.font);
      old_font = SelectObject (hdc, new_font);
#elif ANDROID
      new_font = gdi_make_font (o.font);
      old_font = SelectFont (hdc, new_font);
#endif

      display_listbox_tree_or_combo_object (hdc, dx, dy, o, ref o.combo.listbox, o.combo.lb.y, o.combo.lb.y_size,
                                            ps.rcPaint, d.has_keyboard_focus);

#if WINDOWS
      SelectObject (hdc, old_font);
#elif ANDROID
      SelectFont (hdc, old_font);
#endif

      ExcludeClipRect (hdc, x, y, x+sx, y+sy);
    }
  }
}

//--------------------------------------------------------------------

// note: BeginPaint hides the caret, EndPaint shows it again

public void redraw_dialogs (HWND         hwnd,
                            HDC          hdc,
                            wstring      title,    // for android only !
                            PAINTSTRUCT  ps,
                            DIALOG_INFO  d)
{
  CONTROL_INFO^ p;
  uint          old_tx_color;
  HFONT         old_font, new_font;
  int           dx, dy;

#if ANDROID
  // redraw rectangle portion in dialog background color
  fill_rectangle (hdc    => hdc,
                  x      => ps.rcPaint.left,
                  y      => ps.rcPaint.top,
                  width  => ps.rcPaint.right - ps.rcPaint.left,
                  height => ps.rcPaint.bottom - ps.rcPaint.top,
                  color  => d.dialog_background_color);
#endif


  // draw title and border

  init_pens (d.dialog_colors);

  old_tx_color = SetTextColor (hdc, d.title_font.color);

#if WINDOWS
  new_font = get_cached_font (d.title_font);
  old_font = SelectObject (hdc, new_font);
#elif ANDROID
  new_font = gdi_make_font (d.title_font);
  old_font = SelectFont (hdc, new_font);
#endif

#if WINDOWS
  _unused title;
#endif

  display_dialog (hwnd,
                  hdc,
#if ANDROID
                  title,
#endif
                  x                => 0,
                  y                => 0,
                  width            => d.rect.right  - d.rect.left,
                  height           => d.rect.bottom - d.rect.top,
                  title_height     => d.title_height,
                  border_size      => d.border_size,
                  has_close_button => d.has_close_button);


#if WINDOWS
  SelectObject (hdc, old_font);
#elif ANDROID
  SelectFont (hdc, old_font);
#endif

  dx = d.border_size;
  dy = d.border_size + d.title_height;


#if WINDOWS
  draw_open_combobox (hdc, ps, d, ref old_font, ref new_font, dx, dy);
#endif


  // draw regular controls

  p = d.list;
  if (p != null)
  {
    for (;;)
    {
      ref CONTROL_INFO o = p^;
      int x, y, sx, sy;

      x  = dx + o.x;
      y  = dy + o.y;
      sx = o.x_size;
      sy = o.y_size;

      if (!o.hide &&
          x      <= ps.rcPaint.right &&
          x + sx >= ps.rcPaint.left &&
          y      <= ps.rcPaint.bottom &&
          y + sy >= ps.rcPaint.top)
      {
        init_pens (o.colors);
        SetTextColor (hdc, o.font.color);

#if WINDOWS
        new_font = get_cached_font (o.font);
        old_font = SelectObject (hdc, new_font);
#elif ANDROID
        new_font = gdi_make_font (o.font);
        old_font = SelectFont (hdc, new_font);
#endif

        switch (p^.typ)
        {
          case TYP_TEXT:
            display_static_text (hdc, x, y, sx, sy,  text=> o.text^);
            break;

          case TYP_EDIT:
            display_edit_or_combo_edit (hdc, dx, dy, o, o.x_size,
                                        edit      => o.edit,
                                        has_focus => p == d.focus && d.has_keyboard_focus);
            break;

          case TYP_CHECKBOX:
            display_checkbox (hdc, x, y, sx, sy, box_width => o.checkbox.box_width,
                              setting   => o.checkbox.setting,
                              text      => o.text^,
                              has_focus => p == d.focus && d.has_keyboard_focus);
            break;

          case TYP_RADIOBUTTON:
            display_radiobutton (hdc, x, y, sx, sy,
                                 box_width => o.radiobutton.box_width,
                                 setting   => o.radiobutton.setting,
                                 text      => o.text^,
                                 has_focus => p == d.focus && d.has_keyboard_focus);
            break;

          case TYP_BUTTON:
            display_button (hdc, x, y, sx, sy,
                            text      => o.text^,
                            pressed   => o.button.pressed,
                            has_focus => p == d.focus && d.has_keyboard_focus);
            break;

          case TYP_WINDOW:
            if (o.window.refresh)
            {
              BitBlt (hdcDest => hdc,
                      nXDest  => x,
                      nYDest  => y,
                      nWidth  => sx,
                      nHeight => sy,
                      hdcSrc  => o.window.hdcMemory,
                      nXSrc   => 0,
                      nYSrc   => 0,
                      dwRop   => SRCCOPY);
            }
            break;

          case TYP_LISTBOX:
            display_listbox_tree_or_combo_object (hdc, dx, dy, o, ref o.listbox, o.y, o.y_size,
                                                  ps.rcPaint, p == d.focus && d.has_keyboard_focus);
            break;

          case TYP_TREE:
            display_listbox_tree_or_combo_object (hdc, dx, dy, o, ref o.tree.listbox, o.y, o.y_size,
                                                  ps.rcPaint, p == d.focus && d.has_keyboard_focus);
            break;

          case TYP_COMBO:
            display_edit_or_combo_edit (hdc, dx, dy, o, o.x_size - o.combo.listbox.arrow_box_width,
                                        edit      => o.combo.edit,
                                        has_focus => p == d.focus && d.has_keyboard_focus);

            draw_black_arrow_button (hdc, dx + o.x + o.x_size - o.combo.listbox.arrow_box_width - ui_scale(1), dy + o.y,
                                     o.combo.listbox.arrow_box_width + ui_scale(1), o.y_size,
                                     direction => o.combo.lb.y < o.y ? -1 : +1);
            break;

          case TYP_SCROLL:
            if (o.scroll.tscroll == HSCROLL)
              display_horizontal_scrollbar (hdc, x, y, sx, sy, o.scroll.arrow_box_width_or_height,
                                            o.scroll.page, o.scroll.range, o.scroll.shown);
            else
              display_vertical_scrollbar (hdc, x, y, sx, sy, o.scroll.arrow_box_width_or_height,
                                          o.scroll.page, o.scroll.range, o.scroll.shown);
            break;

          case TYP_EDITBOX:
            display_editbox_object (hdc, dx, dy,
                                    ref o, ref o.editbox, p == d.focus && d.has_keyboard_focus);
            break;

          default:
            break;
        }

#if WINDOWS
        SelectObject (hdc, old_font);
#elif ANDROID
        SelectFont (hdc, old_font);
#endif
        if (ExcludeClipRect (hdc, x, y, x+sx, y+sy) == NULLREGION)
          break;   // no more to draw
      }

      p = p^.next;
      if (p == d.list)
        break;
    }
  }

#if ANDROID
  draw_open_combobox (hdc, ps, d, ref old_font, ref new_font, dx, dy);
#endif


#if WINDOWS
  // redraw dialog background
  fill_rectangle (hdc    => hdc,
                  x      => 0,
                  y      => 0,
                  width  => d.rect.right  - d.rect.left,
                  height => d.rect.bottom - d.rect.top,
                  color  => d.dialog_background_color);
#endif


  free_pens();

  SetTextColor (hdc, old_tx_color);
}

//--------------------------------------------------------------------------

// called after paint

public void update_caret (HWND hwnd, DIALOG_INFO d)
{
#if WINDOWS
  if (!d.has_keyboard_focus)    // this window has not the keyboard focus
    return;

  if (d.focus != null && (d.focus^.typ == TYP_EDIT || d.focus^.typ == TYP_COMBO || d.focus^.typ == TYP_EDITBOX))
  {
    // make caret visible, possibly update position and caret height
    if (caret_visible && caret_height != caret_new_height)
    {
      DestroyCaret();
      caret_visible = false;
    }

    if (!caret_visible)
    {
      CreateCaret (hwnd, 0, ui_scale(2*GetSystemMetrics(SM_CXBORDER)), caret_new_height);
    }

    if (!caret_visible || caret_x != caret_new_x || caret_y != caret_new_y)
    {
      SetCaretPos (caret_new_x, caret_new_y);
      caret_x = caret_new_x;
      caret_y = caret_new_y;
    }

    if (!caret_visible)
    {
      ShowCaret (hwnd);
    }

    caret_visible = true;
    caret_height = caret_new_height;
  }
  else
  {
    // make caret invisible
    if (caret_visible)
    {
      DestroyCaret();
      caret_visible = false;
      caret_new_height = 0;
    }
  }
#elif ANDROID
  _unused hwnd, d;
  //$
#endif
}

//--------------------------------------------------------------------------
#end unsafe
//--------------------------------------------------------------------------
