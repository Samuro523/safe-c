
// guigdi.c

use ../gui, ../image, ../draw, guitree;
use ../font/fonts;

//-------------------------------------------------------------------------------------
#begin unsafe
//-------------------------------------------------------------------------------------

public HBRUSH CreateSolidBrush (COLORREF color)
{
  return (HBRUSH)(color | 0xFF000000);
}

//-------------------------------------------------------------------------------------

public HFONT gdi_make_font (FONT font)
{
  return (HFONT)create_font (font.name, font.height, font.style);
}

//-------------------------------------------------------------------------------------

// returns previous one
public HFONT SelectFont (HDC hdc, HFONT h)
{
  HFONT old = hdc.font;
  *&hdc.font = h;
  return old;
}

//-------------------------------------------------------------------------------------

public HPEN SelectPen (HDC hdc, HPEN h)
{
  HPEN old = hdc.pen;
  *&hdc.pen = h;
  return old;
}

//-------------------------------------------------------------------------------------

public HBRUSH SelectBrush (HDC hdc, HBRUSH h)
{
  HBRUSH old = hdc.brush;
  *&hdc.brush = h;
  return old;
}

//-------------------------------------------------------------------------------------

public COLORREF SetTextColor (HDC hdc, COLORREF col)
{
  COLORREF old = hdc.textcolor;
  *&hdc.textcolor = col | 0xFF000000;
  return old;
}

//-------------------------------------------------------------------------------------

public COLORREF SetBkColor (HDC hdc, COLORREF col)
{
  COLORREF old = hdc.backcolor;
  *&hdc.backcolor = col | 0xFF000000;
  return old;
}

//-------------------------------------------------------------------------------------

public void Polyline (HDC hdc, POINT* pts, int count)
{
  DRAW_CONTEXT dc;
  int          i;

  init_draw (out dc, hdc.image, hdc.width, hdc.height);

  for (i=1; i<count; i++)
  {
    draw_line (ref dc, pts[i-1].x, pts[i-1].y, pts[i].x, pts[i].y, color => hdc.pen, thick => 1, draw_last_pixel => false);
  }
}

//-------------------------------------------------------------------------------------

void intersect_rect (ref RECT r, RECT n)
{
  if (r.left < n.left)
    r.left = n.left;
  if (r.top < n.top)
    r.top = n.top;
  if (r.right > n.right)
    r.right = n.right;
  if (r.bottom > n.bottom)
    r.bottom = n.bottom;
}

//--------------------------------------------------------------------

public void FillRect (HDC hdc, RECT* rect, HBRUSH brush_color)
{
  RECT r = hdc.rect;

  intersect_rect (ref r, *rect);

//  r.right--;    // don't draw last col nor last line
//  r.bottom--;

  if (r.top < 0)
    r.top = 0;

  if (r.bottom > (int)hdc.height)
    r.bottom = (int)hdc.height;

  if (r.left < 0)
    r.left = 0;

  if (r.right > (int)hdc.width)
    r.right = (int)hdc.width;

  {
    uint* image = (uint*)&hdc.image^;
    uint* image_row, image_col;
    int   x, y;

    image_row = image + ((uint)r.top*hdc.width + (uint)r.left);

    for (y=r.top; y<r.bottom; y++)
    {
      image_col = image_row;

      for (x=r.left; x<r.right; x++)
      {
        *image_col = brush_color;
        image_col++;
      }

      image_row += hdc.width;
    }
  }
}

//-------------------------------------------------------------------------------------

public void Ellipse (HDC hdc, 
                     int nLeftRect, 
                     int nTopRect, 
                     int nRightRect, 
                     int nBottomRect)
{
  // $
}

//-------------------------------------------------------------------------------------

public void Arc (HDC hdc,
                 int nLeftRect, 
                 int nTopRect,
                 int nRightRect, 
                 int nBottomRect,
                 int nXStartArc, 
                 int nYStartArc,
                 int nXEndArc, 
                 int nYEndArc)
{
  // $
}

//-------------------------------------------------------------------------------------

public void GetTextExtentPoint32W (HDC hdc, wchar* text, int length, SIZE* size)
{
  size->cx = fonts.width_of_font_wtext (text[0:length], (int)hdc.font);
  size->cy = fonts.height_of_font ((int)hdc.font);
}

//-------------------------------------------------------------------------------------

public void ExtTextOutW  (HDC    hdc,
                          int    x_left,
                          int    y_top,
                          uint   flags,
                          RECT*  rect,
                          wchar* text,
                          uint   text_length,
                          byte*  ptr)
{
  RECT r = hdc.rect;

  if ((flags & ETO_CLIPPED) != 0)
    intersect_rect (ref r, *rect);

  if (r.left < r.right && r.top < r.bottom)
  {
    if ((flags & ETO_OPAQUE) != 0)
      FillRect (hdc, &r, brush_color => hdc.backcolor);

    draw_font_wtext
       (image => IMAGE_INFO'{pixel  => hdc.image,
                             width  => (uint)hdc.width,
                             height => (uint)hdc.height},
        clip  => CLIP_INFO ' {offset_x => r.left,       // don't draw outside clipping rectangle
                              offset_y => r.top,        // don't draw outside clipping rectangle
                              size_x   => (uint)(r.right - r.left),
                              size_y   => (uint)(r.bottom - r.top)},
        text       => text[0:text_length],
        font_index => (int)hdc.font,   // from create_font (string font_name, int height, uint style)
        left_x     => x_left,          // lower left corner (0,0 is top left)
        top_y      => y_top,
        color      => hdc.textcolor);  // RGB text
  }
}

//-------------------------------------------------------------------------------------

public void BitBlt (HDC        hdcDest, 
                    int        nXDest, 
                    int        nYDest, 
                    int        nWidth, 
                    int        nHeight,
                    HDC        hdcSrc, 
                    int        nXSrc, 
                    int        nYSrc, 
                    uint4      dwRop)
{
  IMAGE_INFO target_image;
  CLIP_INFO  source_clip, target_clip;
  int        rc;

  clear source_clip, target_clip;
  
  source_clip.offset_x = nXSrc;
  source_clip.offset_y = nYSrc;
  source_clip.size_x = (uint)nWidth;
  source_clip.size_y = (uint)nHeight;
  
  target_clip.offset_x = nXDest;
  target_clip.offset_y = nYDest;
  target_clip.size_x = (uint)nWidth;
  target_clip.size_y = (uint)nHeight;


  target_image = {pixel => hdcDest.image, width => hdcDest.width, height => hdcDest.height};
    
  rc = replace_image_rectangle (source_image => {hdcSrc.image, (uint)hdcSrc.width, (uint)hdcSrc.height},
                                source_clip  => source_clip,
                                target_image => target_image,
                                target_clip  => target_clip);
  assert rc == 0;
}

//-------------------------------------------------------------------------------------

// return NULLREGION (=1) if no more to draw

public int ExcludeClipRect (HDC hdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect)
{
  _unused hdc, nLeftRect, nTopRect, nRightRect, nBottomRect;
  return 0;
}

//-------------------------------------------------------------------------------------
#end unsafe
//-------------------------------------------------------------------------------------
