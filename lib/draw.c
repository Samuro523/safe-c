
// draw.c : draw in a memory buffer

use thread;

#if WINDOWS
  use strings;
  use win/windows;
#elif ANDROID
  use image, strings;
  use font/fonts;
#endif

/**************************************************************************/
#begin unsafe
/**************************************************************************/

struct CLIP_AREA
{
  int  min_x;   /* normalized coordinates, i.e. after origin conversion */
  int  max_x;
  int  min_y;
  int  max_y;
}

struct ORIGIN
{
  int  x;
  int  y;
  bool inv_x;   /* if true increase to the left */
  bool inv_y;   /* if true increase to the top */
}

struct DRAW_CONTEXT
{
  byte[]^   image;  /* points to a buffer of (4*width*height) bytes */
  int       width;
  int       height;
  CLIP_AREA clip;
  ORIGIN    origin;
}

/**************************************************************************/

#if WINDOWS
  char  g_cached_font_name[32];
  int   g_cached_font_height;
  uint  g_cached_font_style;

  HFONT g_hcached_font;
  HDC   g_cached_hdc;
#endif

SHARED_OBJECT g_o;

//--------------------------------------------------------------------

public uint rgb (byte r, byte g, byte b)
{
  return (((((uint)b) << 8) + ((uint)g)) << 8) + (uint)r;
}

/**************************************************************************/

public void init_draw (out DRAW_CONTEXT dc,
                       byte[]^          buffer,     /* -> (4*width*height) bytes */
                       uint             width,
                       uint             height)
{
  assert buffer^'size == 4*width*height;
  dc = {image  => buffer,
        width  => (int)width,
        height => (int)height,
        clip   => {min_x => 0, max_x => (int)width-1, min_y => 0, max_y => (int)height-1},
        origin => {x => 0, y => 0, inv_x => false, inv_y => false}};
}

/**************************************************************************/

byte mask_of (CLIP_AREA clip, int x, int y)
{
  byte mask;

  mask = 0;

  if (x < clip.min_x)
    mask |= 1;
  else if (x > clip.max_x)
    mask |= 2;

  if (y < clip.min_y)
    mask |= 4;
  else if (y > clip.max_y)
    mask |= 8;

  return mask;
}

/**************************************************************************/

int clip_line (CLIP_AREA clip, ref int x1, ref int y1, ref int x2, ref int y2)
{
  byte mask1, mask2, mask;
  int  ox1, oy1, ox2, oy2, px1, py1, px2, py2, px, py;


  /* save source points because they will be reused later */

  ox1 = x1;
  oy1 = y1;

  ox2 = x2;
  oy2 = y2;


  /* compute mask of source points */

  mask1 = mask_of (clip, ox1, oy1);
  mask2 = mask_of (clip, ox2, oy2);

  if ((mask1 & mask2) != 0)    /* line not visible */
    return -1;


  if (mask1 != 0)   /* point 1 is not visible : compute a new one */
  {
    /* reload source points */
    px1 = ox1;  py1 = oy1;   px2 = ox2;   py2 = oy2;

    for (;;)
    {
      px = px1 + ((px2 - px1) >> 1);
      py = py1 + ((py2 - py1) >> 1);

      if (px == px1 && py == py1)
        break;

      if (px == px2 && py == py2)
        break;

      mask = mask_of (clip, px, py);

      if ((mask & mask1) != 0)     /* new point is in area of point 1 */
      {
        if ((mask & mask2) != 0)     /* new point is also in area of point 2 */
          return -1;

        px1 = px;
        py1 = py;
      }
      else  /* new point is visible or in area of point 2 */
      {
        px2 = px;
        py2 = py;
      }
    }

    if (mask_of (clip, px2, py2) != 0)   /* point 2 not visible */
      return -1;

    x1 = px2;
    y1 = py2;
  }


  if (mask2 != 0)   /* point 2 is not visible : compute a new one */
  {
    /* reload source points */
    px1 = ox1;  py1 = oy1;   px2 = ox2;   py2 = oy2;

    for (;;)
    {
      px = px1 + ((px2 - px1) >> 1);
      py = py1 + ((py2 - py1) >> 1);

      if (px == px1 && py == py1)
        break;

      if (px == px2 && py == py2)
        break;

      mask = mask_of (clip, px, py);

      if ((mask & mask2) != 0)   /* new point is in area of point 2 */
      {
        if ((mask & mask1) != 0)    /* new point is also in area of point 1 */
          return -1;

        px2 = px;
        py2 = py;
      }
      else  /* new point is visible or in area of point 1 */
      {
        px1 = px;
        py1 = py;
      }
    }

    if (mask_of (clip, px1, py1) != 0)   /* point 1 not visible */
      return -1;

    x2 = px1;
    y2 = py1;
  }

  return 0;
}

/**************************************************************************/

/* uses normalized coordinates */
/* assertion: x1 <= x2 */

void hor_line (ref DRAW_CONTEXT c, int x1, int x2, int y, uint color)
{
  int xa, xb;

  if (y < c.clip.min_y || y > c.clip.max_y)
    return;

  xa = x1;
  xb = x2;

  if (xa < c.clip.min_x)
    xa = c.clip.min_x;

  if (xb > c.clip.max_x)
    xb = c.clip.max_x;

  if (xa <= xb)
  {
    uint *image = (uint *)&c.image^[4*(y*c.width+xa)];
    image[0:xb - xa + 1] = {all => color};
  }
}

/**************************************************************************/

void full_circle (ref DRAW_CONTEXT c, int x, int y, int radius, uint color)
{
  int r, p, dx, dy;

  if (radius < 1)
    return;

  r = radius - 1;

  dx = 0;
  dy = r;
  p  = (5 - (r << 2)) >> 2;

  for (;;)
  {
    hor_line (ref c, x-dx, x+dx, y-dy, color);    /* top    */
    hor_line (ref c, x-dx, x+dx, y+dy, color);    /* bottom */
    hor_line (ref c, x-dy, x+dy, y-dx, color);    /* upper  */
    hor_line (ref c, x-dy, x+dy, y+dx, color);    /* lower  */

    if (dx >= dy)
      break;

    dx++;
    if (p < 0)
      p += ((dx<<1) + 1);
    else
    {
      dy--;
      p += (((dx - dy) << 1) + 1);
    }
  }
}

/**************************************************************************/

void a_circle (ref DRAW_CONTEXT c, int x, int y, int radius, uint color, int thick)
{
  int r, p, dx, dy, th;

  if (radius < 1 || thick < 1)
    return;

  th = thick >> 1;
  if (th == 0)
    th = 1;

  r = radius - 1;

  dx = 0;
  dy = r;
  p  = (5 - (r << 2)) >> 2;

  for (;;)
  {
    full_circle (ref c, x-dx, y-dy, th, color);
    full_circle (ref c, x+dx, y-dy, th, color);

    full_circle (ref c, x-dx, y+dy, th, color);
    full_circle (ref c, x+dx, y+dy, th, color);

    full_circle (ref c, x-dy, y-dx, th, color);
    full_circle (ref c, x+dy, y-dx, th, color);

    full_circle (ref c, x-dy, y+dx, th, color);
    full_circle (ref c, x+dy, y+dx, th, color);

    if (dx >= dy)
      break;

    dx++;
    if (p < 0)
      p += ((dx<<1) + 1);
    else
    {
      dy--;
      p += (((dx - dy) << 1) + 1);
    }
  }
}

/**************************************************************************/

public void draw_circle (ref DRAW_CONTEXT dc,
                         int              x,
                         int              y,
                         int              radius,
                         uint             color,
                         int              thick)
{
  int x0, y0;

  x0 = x;
  y0 = y;


  /* convert to origin coordinates */

  if (dc.origin.inv_x)
    x0 = -x0;

  if (dc.origin.inv_y)
    y0 = -y0;

  x0 += dc.origin.x;
  y0 += dc.origin.y;


  a_circle (ref dc, x0, y0, radius, color, thick);
}

/**************************************************************************/

public void draw_solid_circle (ref DRAW_CONTEXT dc,
                               int              x,
                               int              y,
                               int              radius,
                               uint             color)
{
  int x0, y0;

  x0 = x;
  y0 = y;


  /* convert to origin coordinates */

  if (dc.origin.inv_x)
    x0 = -x0;

  if (dc.origin.inv_y)
    y0 = -y0;

  x0 += dc.origin.x;
  y0 += dc.origin.y;


  full_circle (ref dc, x0, y0, radius, color);
}

/**************************************************************************/

public void draw_line (ref DRAW_CONTEXT dc,
                       int              x1,
                       int              y1,
                       int              x2,
                       int              y2,
                       uint             color,    /* = rgb(red,green,blue) */
                       int              thick,    /* >= 1                  */
                       bool             draw_last_pixel = true)
{
  int   xa, ya, xb, yb;
  int   dx, dy, x, y, step_x, step_y, e, width;
  int   abs_dx, abs_dy, ofs_image_y, max_thick1, max_thick2, thick0, t;
  int   min_y, max_y, min_x, max_x, old_min_y, old_min_x;

  if (thick < 1)
    return;

  xa = x1;
  ya = y1;
  xb = x2;
  yb = y2;


  /* convert to origin coordinates */

  if (dc.origin.inv_x)
  {
    xa = -xa;
    xb = -xb;
  }

  if (dc.origin.inv_y)
  {
    ya = -ya;
    yb = -yb;
  }

  xa += dc.origin.x;
  ya += dc.origin.y;
  xb += dc.origin.x;
  yb += dc.origin.y;

  if (clip_line (dc.clip, ref xa, ref ya, ref xb, ref yb) < 0)
    return;


  {
    ref byte[] pimage = dc.image^;    // will lock heap object
    uint* image, p;

    dx = xb - xa;
    dy = yb - ya;

    if (dx < 0)
    {
      abs_dx = -dx;
      step_x = -1;
    }
    else
    {
      abs_dx = dx;
      step_x = +1;
    }

    if (dy < 0)
    {
      abs_dy = -dy;
      step_y = -1;
    }
    else
    {
      abs_dy = dy;
      step_y = +1;
    }

    abs_dx++;
    abs_dy++;

    x = xa;
    y = ya;

    width = dc.width;
    ofs_image_y = step_y * width;

    e = 0;

    /* always a central line, so always thick >= 1 */

    max_thick1 = ((thick-1) >> 1);          /* nb pixels on side 1 */
    max_thick2 = (thick-1) - max_thick1;    /* nb pixels on side 2 */

    full_circle (ref dc, x, y, max_thick1, color);

    if (abs_dx > abs_dy)   /* horizontal-like line */
    {
      min_y = y - max_thick1;
      if (min_y < dc.clip.min_y)
        min_y = dc.clip.min_y;

      max_y = y + max_thick2;
      if (max_y > dc.clip.max_y)
        max_y = dc.clip.max_y;

      image = (uint *)&pimage[4*(x + width*min_y)];

      thick0 = max_y - min_y + 1;         /* thickness of vertical stripe */

      for (;;)
      {
        if (!draw_last_pixel && x == xb)   /* done */
          break;

        /* plot (x, y) */

        if (thick == 1)
          *image = color;
        else
        {
          p = image;
          for (t=0; t<thick0; t++)
          {
            *p = color;
            p += width;
          }
        }

        if (x == xb)   /* done */
          break;

        x += step_x;
        image += step_x;
        e += abs_dy;

        if (e >= abs_dx)     /* 'y' changes */
        {
          e -= abs_dx;
          y += step_y;

          old_min_y = min_y;

          min_y = y - max_thick1;
          if (min_y < dc.clip.min_y)
            min_y = dc.clip.min_y;

          max_y = y + max_thick2;
          if (max_y > dc.clip.max_y)
            max_y = dc.clip.max_y;

          thick0 = max_y - min_y + 1;

          if (min_y < old_min_y)
            image -= width;
          else if (min_y > old_min_y)
            image += width;
        }
      }
    }
    else    /* vertical-like line (abs_dx <= abs_dy) */
    {
      min_x = x - max_thick1;
      if (min_x < dc.clip.min_x)
        min_x = dc.clip.min_x;

      max_x = x + max_thick2;
      if (max_x > dc.clip.max_x)
        max_x = dc.clip.max_x;

      image = (uint *)&pimage[4*(min_x + width*y)];

      thick0 = max_x - min_x + 1;         /* thickness of horizontal stripe */

      for (;;)
      {
        if (!draw_last_pixel && y == yb)   /* done */
          break;

        /* plot (x, y) */

        if (thick == 1)
          *image = color;
        else
        {
          p = image;
          for (t=0; t<thick0; t++)
            *p++ = color;
        }

        if (y == yb)   /* done */
         break;

        y += step_y;
        image += ofs_image_y;
        e += abs_dx;

        if (e >= abs_dy)     /* 'x' changes */
        {
          e -= abs_dy;
          x += step_x;

          old_min_x = min_x;

          min_x = x - max_thick1;
          if (min_x < dc.clip.min_x)
            min_x = dc.clip.min_x;

          max_x = x + max_thick2;
          if (max_x > dc.clip.max_x)
            max_x = dc.clip.max_x;

          thick0 = max_x - min_x + 1;

          if (min_x < old_min_x)
            image--;
          else if (min_x > old_min_x)
            image++;
        }
      }
    }

    full_circle (ref dc, x, y, max_thick1, color);
  }
}

/**************************************************************************/

public void set_draw_origin (ref DRAW_CONTEXT dc,
                             int              ox,
                             int              oy,
                             int              dx,   /* direction of x-axis */
                             int              dy)   /* direction of y-axis */
{
  dc.origin.inv_x = (dx < 0);
  dc.origin.inv_y = (dy < 0);

  dc.origin.x = ox;
  dc.origin.y = oy;
}

/**************************************************************************/

public void set_draw_clip (ref DRAW_CONTEXT dc,
                           int              min_x,
                           int              min_y,
                           int              max_x,
                           int              max_y)
{
  int min_x0, min_y0, max_x0, max_y0, t;

  min_x0 = min_x;
  min_y0 = min_y;
  max_x0 = max_x;
  max_y0 = max_y;

  /* convert to origin coordinates */

  if (dc.origin.inv_x)
  {
    min_x0 = -min_x0;
    max_x0 = -max_x0;
  }

  if (dc.origin.inv_y)
  {
    min_y0 = -min_y0;
    max_y0 = -max_y0;
  }

  min_x0 += dc.origin.x;
  min_y0 += dc.origin.y;
  max_x0 += dc.origin.x;
  max_y0 += dc.origin.y;


  /* constrain the clip area with the actual image size */

  if (min_x0 < 0)
    min_x0 = 0;

  if (min_y0 < 0)
    min_y0 = 0;

  if (max_x0 > dc.width-1)
    max_x0 = dc.width-1;

  if (max_y0 > dc.height-1)
    max_y0 = dc.height-1;

  if (min_x0 > max_x0)
  {
    t      = min_x0;
    min_x0 = max_x0;
    max_x0 = t;
  }

  if (min_y0 > max_y0)
  {
    t      = min_y0;
    min_y0 = max_y0;
    max_y0 = t;
  }

  dc.clip.min_x = min_x0;
  dc.clip.min_y = min_y0;
  dc.clip.max_x = max_x0;
  dc.clip.max_y = max_y0;
}

/**************************************************************************/

#if WINDOWS

HFONT intern_create_font (string  name,  int height,  uint style)
{
  char fname[32];

  strncpy (out fname, name, fname'length);
  fname[fname'length-1] = nul;    // font name must never exceed 31 chars + nul

  return CreateFontA (height,
                      0,             /* use matching width */
                      0, 0,
                      (style & _STYLE_BOLD) != 0 ? FW_BOLD : FW_DONTCARE,
                      (DWORD)((style & _STYLE_ITALIC) != 0),
                      (DWORD)((style & _STYLE_UNDERLINED) != 0),
                      (DWORD)FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DRAFT_QUALITY,
                      DEFAULT_PITCH,
                      &fname);
}

#endif

//--------------------------------------------------------------------

#if WINDOWS

HFONT get_cached_font (string  name,  int height,  uint style)
{
  if (strcmp (name, g_cached_font_name) != 0 || height != g_cached_font_height || style != g_cached_font_style)
  {
    if (g_hcached_font != 0)
      assert DeleteObject (g_hcached_font) != 0;

    strncpy (out g_cached_font_name, name, g_cached_font_name'length);
    g_cached_font_height = height;
    g_cached_font_style  = style;

    g_hcached_font = intern_create_font (name, height, style);
    assert g_hcached_font != 0;
  }

  return g_hcached_font;
}

#endif

//--------------------------------------------------------------------

public int draw_text (ref DRAW_CONTEXT  dc,
                          string        text,           /* string to display */
                          string        font_name,      /* ex: "Times New Roman" */
                          int           font_height,    /* ex: 12 */
                          int           x,              /* lower left corner */
                          int           y,
                          uint          color,
                          uint          style)          /* = 0 */
{
  int     x0, y0, length;

#if WINDOWS
  HFONT   new_font, old_font;
  SIZE    size;
  HBITMAP hbitmapPict, hbitmapPictOld;
  int     min_x, max_x, min_y, max_y;
  uint    color_BGR;

  packed struct INFO
  {
    BITMAPINFOHEADER bmiHeader;
    DWORD            bmiColors[3];
  }

  INFO info;
  byte*   bits;
#endif

  x0 = x;
  y0 = y;


  /* convert to origin coordinates */

  if (dc.origin.inv_x)
    x0 = -x0;

  if (dc.origin.inv_y)
    y0 = -y0;

  x0 += dc.origin.x;
  y0 += dc.origin.y;



  length = strlen(text);



  /* check constraints */

  if (font_height < 1 || length < 1)
    return 0;


  enter_shared_object (ref g_o);


#if WINDOWS
  /* create DC */

  if (g_cached_hdc == 0)
  {
    HDC hdc = GetDC (0);
    g_cached_hdc = CreateCompatibleDC (hdc);
    ReleaseDC (0, hdc);
    assert g_cached_hdc != 0;
  }


  /* create font */

  new_font = get_cached_font (font_name, font_height, style);
  old_font = SelectObject (g_cached_hdc, new_font);
  GetTextExtentPoint32A (g_cached_hdc, &text, length, &size);

  if ((style & _STYLE_ITALIC) != 0)
  {
    if (size.cx != 0)
      size.cx += (size.cy >> 3);
  }


  /* create bitmap */

  info.bmiHeader.biSize          = BITMAPINFOHEADER'size;
  info.bmiHeader.biWidth         = size.cx;
  info.bmiHeader.biHeight        = -font_height;
  info.bmiHeader.biPlanes        = 1;
  info.bmiHeader.biBitCount      = 32;
  info.bmiHeader.biCompression   = BI_RGB;
  info.bmiHeader.biSizeImage     = (uint)(4 * size.cx * font_height);
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
    SelectObject (g_cached_hdc, old_font);
    leave_shared_object (ref g_o);
    return -1;
  }

  hbitmapPictOld = SelectObject (g_cached_hdc, hbitmapPict);


  /* set y = above the text */
  y0 -= (font_height - 1);


  /* compute copy limits */

  min_x = 0;
  max_x = size.cx - 1;
  min_y = 0;
  max_y = font_height - 1;

  if (min_x+x0 < dc.clip.min_x)
    min_x = dc.clip.min_x - x0;

  if (max_x+x0 > dc.clip.max_x)
    max_x = dc.clip.max_x - x0;

  if (min_y+y0 < dc.clip.min_y)
    min_y = dc.clip.min_y - y0;

  if (max_y+y0 > dc.clip.max_y)
    max_y = dc.clip.max_y - y0;


  /* copy-in the bitmap */

  if (min_x <= max_x && min_y <= max_y)
  {
    uint*  win, img;
    int    len, j;

    img = (uint*)&dc.image^[4*((min_y+y0)*dc.width + (min_x+x0))];
    win = (uint*)&bits[4*(min_y*size.cx + min_x)];
    len = max_x - min_x + 1;

    for (j=min_y; j<=max_y; j++)
    {
      win[0:len] = img[0:len];
      img += dc.width;
      win += size.cx;
    }
  }


  /* display the text */

  color_BGR = ((color & 0xFF) << 16) + (color & 0xFF00) + ((color >> 16) & 0xFF);
  SetTextColor (g_cached_hdc, color_BGR);
  SetBkMode (g_cached_hdc, TRANSPARENT);
  ExtTextOutA (g_cached_hdc, 0, 0, 0, null, &text, (uint)length, null);

  /* copy-back the bitmap */

  if (min_x <= max_x && min_y <= max_y)
  {
    uint* win, img;
    int   len, i, j;

    img = (uint*)&dc.image^[4*((min_y+y0)*dc.width + (min_x+x0))];
    win = (uint*)&bits[4*(min_y*size.cx + min_x)];
    len = max_x - min_x + 1;

    for (j=min_y; j<=max_y; j++)
    {
      img[0:len] = win[0:len];
      for (i=0; i<len; i++)   // force transparency byte to FF for opaque
       img[i] |= 0xFF000000;
      img += dc.width;
      win += size.cx;
    }
  }

  SelectObject (g_cached_hdc, hbitmapPictOld);
  SelectObject (g_cached_hdc, old_font);
  DeleteObject (hbitmapPict);

  leave_shared_object (ref g_o);

  return size.cx;

#elif ANDROID

  /* set y = above the text */
  y0 -= (font_height - 1);

  {
    int font_index = create_font (font_name, font_height, style);
    int width = draw_font_text 
           (image => IMAGE_INFO'{pixel => dc.image,
                                 width => (uint)dc.width,
                                 height => (uint)dc.height},
            clip  => CLIP_INFO ' {offset_x => 0,        // don't draw outside clipping rectangle
                                  offset_y => 0,        // don't draw outside clipping rectangle
                                  size_x   => (uint)dc.width,
                                  size_y   => (uint)dc.height},
            text       => text,
            font_index => font_index,  // from create_font (string font_name, int height, uint style)
            left_x     => x0,      // lower left corner (0,0 is top left)
            top_y      => y0,
            color      => color);      // RGB text
  
    leave_shared_object (ref g_o);

    return width;
  }
#endif
}

/**************************************************************************/

public int draw_wtext (ref DRAW_CONTEXT  dc,
                           wstring       text,           /* string to display */
                           string        font_name,      /* ex: "Times New Roman" */
                           int           font_height,    /* ex: 12 */
                           int           x,              /* lower left corner */
                           int           y,
                           uint          color,
                           uint          style)          /* = 0 */
{
  int     x0, y0, length;

#if WINDOWS
  HFONT   new_font, old_font;
  SIZE    size;
  HBITMAP hbitmapPict, hbitmapPictOld;
  int     min_x, max_x, min_y, max_y;
  uint    color_BGR;

  packed struct INFO
  {
    BITMAPINFOHEADER bmiHeader;
    DWORD            bmiColors[3];
  }

  INFO info;
  byte*   bits;
#endif

  x0 = x;
  y0 = y;


  /* convert to origin coordinates */

  if (dc.origin.inv_x)
    x0 = -x0;

  if (dc.origin.inv_y)
    y0 = -y0;

  x0 += dc.origin.x;
  y0 += dc.origin.y;



  length = wstrlen(text);



  /* check constraints */

  if (font_height < 1 || length < 1)
    return 0;


  enter_shared_object (ref g_o);


#if WINDOWS
  /* create DC */

  if (g_cached_hdc == 0)
  {
    HDC hdc = GetDC (0);
    g_cached_hdc = CreateCompatibleDC (hdc);
    ReleaseDC (0, hdc);
    assert g_cached_hdc != 0;
  }


  /* create font */

  new_font = get_cached_font (font_name, font_height, style);
  old_font = SelectObject (g_cached_hdc, new_font);
  GetTextExtentPoint32W (g_cached_hdc, &text, length, &size);

  if ((style & _STYLE_ITALIC) != 0)
  {
    if (size.cx != 0)
      size.cx += (size.cy >> 3);
  }


  /* create bitmap */

  info.bmiHeader.biSize          = BITMAPINFOHEADER'size;
  info.bmiHeader.biWidth         = size.cx;
  info.bmiHeader.biHeight        = -font_height;
  info.bmiHeader.biPlanes        = 1;
  info.bmiHeader.biBitCount      = 32;
  info.bmiHeader.biCompression   = BI_RGB;
  info.bmiHeader.biSizeImage     = (uint)(4 * size.cx * font_height);
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
    SelectObject (g_cached_hdc, old_font);
    leave_shared_object (ref g_o);
    return -1;
  }

  hbitmapPictOld = SelectObject (g_cached_hdc, hbitmapPict);


  /* set y = above the text */
  y0 -= (font_height - 1);


  /* compute copy limits */

  min_x = 0;
  max_x = size.cx - 1;
  min_y = 0;
  max_y = font_height - 1;

  if (min_x+x0 < dc.clip.min_x)
    min_x = dc.clip.min_x - x0;

  if (max_x+x0 > dc.clip.max_x)
    max_x = dc.clip.max_x - x0;

  if (min_y+y0 < dc.clip.min_y)
    min_y = dc.clip.min_y - y0;

  if (max_y+y0 > dc.clip.max_y)
    max_y = dc.clip.max_y - y0;


  /* copy-in the bitmap */

  if (min_x <= max_x && min_y <= max_y)
  {
    uint*  win, img;
    int    len, j;

    img = (uint*)&dc.image^[4*((min_y+y0)*dc.width + (min_x+x0))];
    win = (uint*)&bits[4*(min_y*size.cx + min_x)];
    len = max_x - min_x + 1;

    for (j=min_y; j<=max_y; j++)
    {
      win[0:len] = img[0:len];
      img += dc.width;
      win += size.cx;
    }
  }


  /* display the text */

  color_BGR = ((color & 0xFF) << 16) + (color & 0xFF00) + ((color >> 16) & 0xFF);
  SetTextColor (g_cached_hdc, color_BGR);
  SetBkMode (g_cached_hdc, TRANSPARENT);
  ExtTextOutW (g_cached_hdc, 0, 0, 0, null, &text, (uint)length, null);

  /* copy-back the bitmap */

  if (min_x <= max_x && min_y <= max_y)
  {
    uint* win, img;
    int   len, i, j;

    img = (uint*)&dc.image^[4*((min_y+y0)*dc.width + (min_x+x0))];
    win = (uint*)&bits[4*(min_y*size.cx + min_x)];
    len = max_x - min_x + 1;

    for (j=min_y; j<=max_y; j++)
    {
      img[0:len] = win[0:len];
      for (i=0; i<len; i++)   // force transparency byte to FF for opaque
       img[i] |= 0xFF000000;
      img += dc.width;
      win += size.cx;
    }
  }

  SelectObject (g_cached_hdc, hbitmapPictOld);
  SelectObject (g_cached_hdc, old_font);
  DeleteObject (hbitmapPict);

  leave_shared_object (ref g_o);

  return size.cx;

#elif ANDROID

  /* set y = above the text */
  y0 -= (font_height - 1);

  {
    int font_index = create_font (font_name, font_height, style);
    int width = draw_font_wtext 
           (image => IMAGE_INFO'{pixel => dc.image,
                                 width => (uint)dc.width,
                                 height => (uint)dc.height},
            clip  => CLIP_INFO ' {offset_x => 0,        // don't draw outside clipping rectangle
                                  offset_y => 0,        // don't draw outside clipping rectangle
                                  size_x   => (uint)dc.width,
                                  size_y   => (uint)dc.height},
            text       => text,
            font_index => font_index,  // from create_font (string font_name, int height, uint style)
            left_x     => x0,      // lower left corner (0,0 is top left)
            top_y      => y0,
            color      => color);      // RGB text
  
    leave_shared_object (ref g_o);

    return width;
  }
#endif
}

/**************************************************************************/

public void draw_image (ref DRAW_CONTEXT  dc,
                        int               x,            /* target (upper-left corner) */
                        int               y,
                        byte[]            image,
                        uint              width,
                        uint              height,
                        uint              clip_x,       /* =0 for all image */
                        uint              clip_y,       /* =0 for all image */
                        uint              clip_size_x,  /* =width for all image */
                        uint              clip_size_y,  /* =height for all image */
                        uint              flags = 0)
{
  int  x0, y0, ofs, dy;
  uint clip_x0, clip_y0, clip_size_x0, clip_size_y0;

  if (flags != 0)
    return;


  x0 = x;
  y0 = y;

  clip_x0 = clip_x;
  clip_y0 = clip_y;
  clip_size_x0 = clip_size_x;
  clip_size_y0 = clip_size_y;


  /* convert to origin coordinates */

  if (dc.origin.inv_x)
    x0 = -x0;

  if (dc.origin.inv_y)
    y0 = -y0;

  x0 += dc.origin.x;
  y0 += dc.origin.y;


  /* constrain clip limits to source clip */

  if (x0 < dc.clip.min_x)
  {
    ofs = dc.clip.min_x - x0;
    if (ofs >= (int)clip_size_x0)
      return;
    clip_x0      += (uint)ofs;
    clip_size_x0 -= (uint)ofs;
    x0           += ofs;
  }

  if (y0 < dc.clip.min_y)
  {
    ofs = dc.clip.min_y - y0;
    if (ofs >= (int)clip_size_y0)
      return;
    clip_y0      += (uint)ofs;
    clip_size_y0 -= (uint)ofs;
    y0           += ofs;
  }

  if (x0 > dc.clip.max_x || y0 > dc.clip.max_y)
    return;


  /* constrain clip limits to target */

  if (clip_x0 > width)
    clip_x0 = width;
  if (clip_y0 > height)
    clip_y0 = height;

  if (clip_size_x0 > width - clip_x0)
    clip_size_x0 = width - clip_x0;
  if (clip_size_y0 > height - clip_y0)
    clip_size_y0 = height - clip_y0;


  /* clip sizes to source sizes */

  if ((int)clip_size_x0 > dc.clip.max_x+1 - x0)
    clip_size_x0 = (uint)(dc.clip.max_x+1 - x);
  if ((int)clip_size_y0 > dc.clip.max_y+1 - y0)
    clip_size_y0 = (uint)(dc.clip.max_y+1 - y);

  {
    uint*  source, target;

    source = (uint *)&image[4*(clip_x0 + clip_y0*width)];
    target = (uint *)&dc.image^[4*(x0 + y0*dc.width)];

    for (dy=0; dy<(int)clip_size_y0; dy++)
    {
      target[0:clip_size_x0] = source[0:clip_size_x0];
      source += width;
      target += dc.width;
    }
  }
}

/**************************************************************************/
#end unsafe
/**************************************************************************/
