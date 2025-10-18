
// printer.c

use strings, strformat, memory, win/windows;

#define debug 0

#if debug
 from std use tracing, console;
#endif

//--------------------------------------------------------------------------

#begin unsafe

HDC     hdc;
byte[]^ pdevmode;

bool printer_is_open;

int  max_x, max_y;                 // paper size in pixels
int  margin_top, margin_bottom;    // non-printable margin in pixels
int  margin_left, margin_right;    // non-printable margin in pixels

// a page must be started using StartPage() before using hdc
bool page_started;

// variables for terminal routines :
int   terminal_col, terminal_line;
int   terminal_max_cols, terminal_max_lines;
HFONT terminal_hfont;      // 0 if not active

//--------------------------------------------------------------------------

const int NB_SHADES = 26;

byte matrix[NB_SHADES][8] =
   {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},     /*  0 */
    {0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x04, 0x00},     /*  2 */
    {0x00, 0x04, 0x40, 0x00, 0x00, 0x04, 0x20, 0x00},     /*  4 */
    {0x00, 0x08, 0x41, 0x10, 0x00, 0x02, 0x20, 0x00},     /*  6 */
    {0x00, 0x14, 0x41, 0x00, 0x00, 0x41, 0x14, 0x00},     /*  8 */
    {0x00, 0x14, 0x41, 0x08, 0x20, 0x82, 0x20, 0x08},     /* 10 */
    {0x22, 0x80, 0x09, 0x20, 0x02, 0x90, 0x04, 0x21},     /* 12 */
    {0x24, 0x82, 0x10, 0x49, 0x00, 0x82, 0x10, 0x45},     /* 14 */
    {0x24, 0x81, 0x14, 0x41, 0x14, 0x81, 0x28, 0x82},     /* 16 */
    {0x29, 0x80, 0x29, 0x82, 0x08, 0x22, 0x89, 0x54},     /* 18 */
    {0x29, 0x80, 0x29, 0x92, 0x48, 0x22, 0x89, 0x54},     /* 20 */
    {0x29, 0x84, 0x29, 0x92, 0x48, 0x32, 0x89, 0x54},     /* 22 */
    {0x2A, 0xC8, 0x23, 0x94, 0x25, 0xA8, 0x13, 0xC4},     /* 24 */
    {0x2A, 0xC8, 0x23, 0x94, 0x67, 0xA8, 0x13, 0xC4},     /* 26 */
    {0x2A, 0xCC, 0x23, 0x94, 0x25, 0xA8, 0x13, 0xD4},     /* 28 */
    {0x35, 0x8A, 0x53, 0xAA, 0x25, 0xCC, 0x55, 0xAA},     /* 30 */
    {0x35, 0xAA, 0x53, 0xAA, 0x65, 0xCC, 0x55, 0xAA},     /* 32 */
    {0x75, 0xAA, 0x53, 0xAA, 0x65, 0xCC, 0x55, 0xAB},     /* 34 */
    {0x75, 0xAA, 0x57, 0xAA, 0x75, 0xCE, 0x55, 0xAA},     /* 36 */
    {0xB6, 0x4B, 0xB5, 0xDE, 0x65, 0xBB, 0xD6, 0xAB},     /* 40 */
    {0xB6, 0x6B, 0xB5, 0xDE, 0x6D, 0xBB, 0xD6, 0xAB},     /* 42 */
    {0xB6, 0xEB, 0xB5, 0xDE, 0x6D, 0xBB, 0xD6, 0xBB},     /* 44 */
    {0xB6, 0xEB, 0xBD, 0xDE, 0x6D, 0xFB, 0xD6, 0xBB},     /* 46 */
    {0xB6, 0xFB, 0xBD, 0xDE, 0x6F, 0xFB, 0xD6, 0xBB},     /* 48 */
    {0xB7, 0xFB, 0xBD, 0xDE, 0x6F, 0xFB, 0xD7, 0xBB},     /* 50 */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};    /* 64 */

bool copy_done;

//--------------------------------------------------------------------------

// called when opening printer or after changing orientation

void compute_bounds ()
{
  max_x = GetDeviceCaps (hdc, HORZRES);
  max_y = GetDeviceCaps (hdc, VERTRES);

  margin_top  = GetDeviceCaps (hdc, PHYSICALOFFSETY);
  margin_left = GetDeviceCaps (hdc, PHYSICALOFFSETX);

  margin_bottom = GetDeviceCaps (hdc, PHYSICALHEIGHT) - margin_top - max_y;
  margin_right  = GetDeviceCaps (hdc, PHYSICALWIDTH) - margin_left - max_x;

  // terminal variables
  terminal_col       = 1;
  terminal_line      = 1;
  terminal_max_cols  = DEFAULT_PRINTER_COLUMNS;
  terminal_max_lines = DEFAULT_PRINTER_LINES;

  if (pdevmode != null && ((DEVMODE*)&pdevmode^)->dmOrientation == 2) // LANDSCAPE
  {
    terminal_max_cols  = terminal_max_cols  * 297 / 210;
    terminal_max_lines = terminal_max_lines * 210 / 297;
  }
}

//--------------------------------------------------------------------------

string^ zstr (string str)
{
  int len = strlen(str);
  string^ p = new char [len+1];
  p^[0 : len] = str[0 : len];
  return p;
}

//--------------------------------------------------------------------------

public int open_printer (string printer_name = "", string document_name = "Document")
{
  char     szPrinter[260];
  DEVMODE* ptrdevmode;
  uint     size;

  assert !printer_is_open;

  free pdevmode;
  pdevmode = null;

  if (strlen(printer_name) == 0)  // empty string : default local printer
  {
    DWORD siz = szPrinter'size;
    if (GetDefaultPrinterA (&szPrinter, &siz) == 0)
    {
#if debug
      console.printf ("GetDefaultPrinterA() failed\n");
#endif    
      return -1;
    }
  }
  else
  {
    strcpy (out szPrinter, printer_name);
  }

  szPrinter[szPrinter'size-1] = nul;
  
  {
    HANDLE  h;
    byte[]^ pinfo;
    DWORD   needed;

    if (OpenPrinterA (&szPrinter, &h, null) == FALSE)
      return -1;

    needed = 0;
    GetPrinterA (h, 2, (byte*)&pinfo, 0, &needed);

    pinfo = new byte[needed];

    if (GetPrinterA (h, 2, &pinfo^, needed, &needed) == FALSE)
    {
      free pinfo;
      ClosePrinter (h);
      return -1;
    }

    // save DEVMODE
    ptrdevmode = ((PRINTER_INFO_2 *)&pinfo^)->pDevMode;
    if (ptrdevmode != null)
    {
      size = ptrdevmode->dmSize + ptrdevmode->dmDriverExtra;
      pdevmode = new byte[] ' (((byte*)ptrdevmode)[0:size]);
    }

#if debug
  trace ("printer: '%s'\n", szPrinter);
  trace ("driver : '%s'\n", ((PRINTER_INFO_2 *)&pinfo^)->pDriverName[0 : 1024]);
  trace ("device : '%s'\n", ((PRINTER_INFO_2 *)&pinfo^)->pDevMode->dmDeviceName);
  trace ("share  : '%s'\n", ((PRINTER_INFO_2 *)&pinfo^)->pShareName[0 : 1024]);
#endif

    hdc = CreateDCA (((PRINTER_INFO_2 *)&pinfo^)->pDriverName, &szPrinter, null, null);

    free pinfo;

    if (hdc == 0)
    {
      ClosePrinter (h);
      return -1;
    }

    if (ClosePrinter (h) == FALSE)
    {
      DeleteDC (hdc);
      return -1;
    }
  }


  // start a new document

  {
    string^ pdocument_name = zstr (document_name);
    DOCINFO lpdi;

    clear lpdi;
    lpdi.cbSize = lpdi'size;
    lpdi.lpszDocName = &pdocument_name^;
    lpdi.lpszOutput = null;

    if (StartDocA (hdc, &lpdi) == SP_ERROR)
    {
#if debug
      console.printf ("StartDocA() failed\n");
#endif    
      free pdocument_name;
      DeleteDC (hdc);
      return -1;
    }

    free pdocument_name;
  }

  if (pdevmode != null)
  {
// trace ("was set : %d\n", ((DEVMODE*)&pdevmode^)->dmFields & DM_TTOPTION);

    ((DEVMODE*)&pdevmode^)->dmFields |= DM_TTOPTION;
    ((DEVMODE*)&pdevmode^)->dmTTOption = DMTT_BITMAP;       // force mode bitmap

//    ((DEVMODE*)&pdevmode^)->dmTTOption = DMTT_DOWNLOAD;     // download softfonts

    if (ResetDCA (hdc, (DEVMODE*)&pdevmode^) == 0)
    {
#if debug
      console.printf ("ResetDCA() failed\n");
#endif    
      DeleteDC (hdc);
      return -1;
    }
  }

  compute_bounds ();

  printer_is_open = true;
  page_started = false;

  return 0;
}

//--------------------------------------------------------------------------

public void get_printer_resolution (out int x_res, out int y_res)
{
  assert printer_is_open;
  x_res = max_x;
  y_res = max_y;
}

//--------------------------------------------------------------------------

public void get_printer_margins (out int top, out int bottom, out int left, out int right)
{
  assert printer_is_open;
  top    = margin_top;
  bottom = margin_bottom;
  left   = margin_left;
  right  = margin_right;
}

//--------------------------------------------------------------------------

package FONT_CACHE
  HFONT get_cached_font (FONT font);
  void discard_font();
end FONT_CACHE;

//--------------------------------------------------------------------

package body FONT_CACHE

  FONT  cached_font;
  HFONT hcached_font;

  bool same_font (FONT a, FONT b)
  {
    return strcmp (a.name, b.name) == 0 && a.height == b.height && a.style == b.style;
  }

  HFONT create_font (FONT font)
  {
    char name[MAX_FONT_NAME_LENGTH];

    strcpy (out name, font.name);
    name[MAX_FONT_NAME_LENGTH-1] = nul;   // font name must never exceed 31 chars + nul

    return CreateFontA (font.height,
                        0,             // use matching width
                        0, 0,
                        (font.style & BOLD) != 0 ? FW_BOLD : FW_DONTCARE,
                        (DWORD)((font.style & ITALIC) != 0),
                        (DWORD)((font.style & UNDERLINED) != 0),
                        (DWORD)FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, DRAFT_QUALITY,
                        DEFAULT_PITCH,
                        &name);
  }


  public HFONT get_cached_font (FONT font)
  {
    if (!same_font (font, cached_font))
    {
      if (hcached_font != 0)
        assert DeleteObject (hcached_font) != 0;

      cached_font = font;
      hcached_font = create_font (font);
      assert hcached_font != 0;
    }

    return hcached_font;
  }

  public void discard_font()
  {
    if (hcached_font != 0)
      assert DeleteObject (hcached_font) != 0;
    clear cached_font, hcached_font;
  }
  
end FONT_CACHE;

//--------------------------------------------------------------------

void close_terminal_font ()
{
  if (terminal_hfont != 0)
  {
    DeleteObject (terminal_hfont);
    terminal_hfont = 0;
  }
}

//--------------------------------------------------------------------------

void intern_close_printer (bool aborting)
{
  assert printer_is_open;

  close_terminal_font ();

  discard_font();
  
  if (aborting)
    AbortDoc (hdc);
  else
    EndDoc (hdc);

  DeleteDC (hdc);

  free pdevmode;
  pdevmode = null;

  printer_is_open = false;
  clear max_x, max_y, margin_top, margin_bottom, margin_left, margin_right;
}

//--------------------------------------------------------------------------

public void close_printer ()
{
  intern_close_printer (false);
}

//--------------------------------------------------------------------------

public void close_printer_and_cancel_job ()
{
  intern_close_printer (true);
}

//--------------------------------------------------------------------------

void make_sure_page_is_started ()
{
  if (!page_started)
  {
    StartPage (hdc);
    page_started = true;
  }
}

//--------------------------------------------------------------------------

public void printer_new_page ()
{
  assert printer_is_open;

  make_sure_page_is_started ();

  EndPage (hdc);

  page_started = false;
}

//--------------------------------------------------------------------------

public int printer_set_orientation (ORIENTATION orientation)
{
  assert printer_is_open;

  assert !page_started;

  assert orientation == PORTRAIT || orientation == LANDSCAPE;

  assert pdevmode != null;   // no DEVMODE available

  ((DEVMODE*)&pdevmode^)->dmOrientation = (short)(1+(int)orientation);   // pdevmode is global

  if (ResetDCA (hdc, (DEVMODE*)&pdevmode^) == 0)
    return -1;

  compute_bounds ();

  return 0;
}

//--------------------------------------------------------------------------

public void printer_set_text_size (int max_cols, int max_lines)
{
  const string font = "Courier New\0";

  assert printer_is_open;

  assert (max_cols >= 1 && max_cols <= 4096 && max_lines >= 1 && max_lines <= 4096);

  // close old font, if any
  close_terminal_font ();

  // try to create a terminal font
  terminal_hfont = CreateFontA (max_y / max_lines,
                                max_x / max_cols,
                                0, 0, FW_BOLD,
                                0, 0, 0,
                                ANSI_CHARSET,
                                OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS,
                                DRAFT_QUALITY,
                                DEFAULT_PITCH | FF_MODERN,
                                &font);

  assert (terminal_hfont != 0);

  if (terminal_col > max_cols)
    terminal_col = 1;

  if (terminal_line > max_lines)
    terminal_line = 1;

  terminal_max_cols  = max_cols;
  terminal_max_lines = max_lines;
}

//--------------------------------------------------------------------------

public void printer_set_column (int col)     // starting at 1
{
  assert printer_is_open;
  assert (col >= 1 && col <= terminal_max_cols);

  terminal_col = col;
}

//--------------------------------------------------------------------------

public void printer_set_line (int line)       // starting at 1
{
  assert printer_is_open;
  assert (line >= 1 && line <= terminal_max_lines);

  terminal_line = line;
}

//--------------------------------------------------------------------------

public int printer_column ()              // returns current column
{
  assert printer_is_open;
  return terminal_col;
}

//--------------------------------------------------------------------------

public int printer_line ()                // returns current line
{
  assert printer_is_open;
  return terminal_line;
}

//--------------------------------------------------------------------------

package WriteToPrinter
  int Put (WPUT_CONTEXT context, wstring buffer);
end WriteToPrinter;

//----------------------------------------------------------------------------

package body WriteToPrinter

  public int Put (WPUT_CONTEXT context, wstring buffer)
  {
    int   i;
    wchar c;

    _unused context;

    for (i=0; i<buffer'length; i++)
    {
      c = buffer[i];
      if (c == L'\n')
      {
        if (terminal_line > terminal_max_lines)
        {
          terminal_line = 1;
          printer_new_page ();
          make_sure_page_is_started ();
        }

        terminal_col = 1;
        terminal_line++;
      }
      else if (c == (wchar)12)
      {
        terminal_col = 1;
        terminal_line = 1;
        printer_new_page ();
        make_sure_page_is_started ();
      }
      else
      {
        if (terminal_col > terminal_max_cols)
        {
          terminal_col = 1;
          terminal_line++;
        }

        if (terminal_line > terminal_max_lines)
        {
          terminal_line = 1;
          printer_new_page ();
          make_sure_page_is_started ();
        }

        TextOutW (hdc,
                  (terminal_col - 1) * (max_x / terminal_max_cols),
                  (terminal_line - 1) * (max_y / terminal_max_lines),
                  &c,
                  1);

        terminal_col++;
      }
    }

    return 0;
  }

end WriteToPrinter;

//----------------------------------------------------------------------------

// print text, starts automatically a new page when needed.

public void wprinterf (wstring format, object[] arg)
{
  HFONT hOldFont;

  assert printer_is_open;

  make_sure_page_is_started ();

  if (terminal_hfont == 0)     // no font : create one
  {
    printer_set_text_size (terminal_max_cols, terminal_max_lines);
  }

  hOldFont = SelectObject (hdc, terminal_hfont);
  assert (hOldFont != 0);

  wformat_string (null, WriteToPrinter.Put, format, arg);

  SelectObject (hdc, hOldFont);
}

//--------------------------------------------------------------------------

wstring^ wide (string s)
{
  int len = s'length;
  wstring^ t = new wstring (len);
  ref wstring tt = t^;
  int i;
  for (i=0; i<len; i++)
    tt[i] = (wchar)(uint)s[i];
  return t;
}

//----------------------------------------------------------------------------

public void printerf (string format, object[] arg)
{
  wstring^ wformat = wide (format);
  wprinterf (wformat^, arg);
  free wformat;
}

//--------------------------------------------------------------------------

// compute width of a text, in pixels, using specified font.

public int text_width_of2 (string text, FONT font)
{
  int   length;
  HFONT old_font, new_font;
  SIZE  size;

  assert printer_is_open;

  length = strlen(text);

  new_font = get_cached_font (font);
  old_font = SelectObject (hdc, new_font);

  if (GetTextExtentPoint32A (hdc, &text, length, &size) == FALSE)
  {
#if debug
    trace ("error: text_width_of2() : GetTextExtentPoint32A() failed\n");
#endif
    size.cx = font.height * length;
  }

  if ((font.style & ITALIC) != 0)
  {
    if (size.cx != 0)
      size.cx += (size.cy >> 3);
  }

  SelectObject (hdc, old_font);

  return size.cx;
}

//--------------------------------------------------------------------

public int wtext_width_of2 (wstring text, FONT font)
{
  int    length;
  HFONT  old_font, new_font;
  SIZE   size;

  assert printer_is_open;

  length = wstrlen(text);

  new_font = get_cached_font (font);
  old_font = SelectObject (hdc, new_font);

  if (GetTextExtentPoint32W (hdc, &text, length, &size) == FALSE)
  {
#if debug
    trace ("error: text_width_of2() : GetTextExtentPoint32A() failed\n");
#endif
    size.cx = font.height * length;
  }

  if ((font.style & ITALIC) != 0)
  {
    if (size.cx != 0)
      size.cx += (size.cy >> 3);
  }

  SelectObject (hdc, old_font);

  return size.cx;
}

//---------------------------------------------------------------------------------

package WriteToPrinter2
  struct COORD
  {
    int x, y;
  }
  int Put (WPUT_CONTEXT context, wstring buffer);
end WriteToPrinter2;

//----------------------------------------------------------------------------

package body WriteToPrinter2

  public int Put (WPUT_CONTEXT context, wstring buffer)
  {
    COORD* coord = (COORD*)context;
    SIZE   size;
    TextOutW (hdc, coord->x, coord->y, &buffer[0], wstrlen(buffer));
    if (GetTextExtentPoint32W (hdc, &buffer[0], wstrlen(buffer), &size) != 0)
      coord->x += size.cx;
    return 0;
  }

end WriteToPrinter2;

//----------------------------------------------------------------------------

public
void ext_wprinterf (int      x,   // top/left coordinates
                    int      y,
                    FONT     font,
                    wstring  format,
                    object[] arg)
{
  HFONT  old_font, new_font;
  uint   old_color;
  COORD  coord = {x, y};

  assert printer_is_open;

  new_font = get_cached_font (font);
  old_font = SelectObject (hdc, new_font);
  old_color = SetTextColor (hdc, font.color);

  wformat_string ((WPUT_CONTEXT)&coord, WriteToPrinter2.Put, format, arg);

  SetTextColor (hdc, old_color);
  SelectObject (hdc, old_font);
}

//---------------------------------------------------------------------------------

public
void ext_printerf (int      x,   // top/left coordinates
                   int      y,
                   FONT     font,
                   string   format,
                   object[] arg)
{
  wstring^ wformat = wide (format);
  ext_wprinterf (x, y, font, wformat^, arg);
  free wformat;
}

//---------------------------------------------------------------------------------

// round up to a multiple of 4
int ALIGN4 (int x)
{
  return ((((x) + 3) / 4) * 4);
}

// round up to a multiple of 32
int ALIGN32 (int x)
{
  return ((((x) + 31) / 32) * 32);
}

//--------------------------------------------------------------------------

void reverse_matrix ()
{
  int  y, x;
  byte temp;

  if (copy_done)
    return;
    
  for (y=0; y<NB_SHADES; y++)
  {
    for (x=0; x<8; x++)
      matrix[y][x] = (byte)(0xFF - matrix[y][x]);
  }

  for (y=0; y<NB_SHADES/2; y++)
  {
    for (x=0; x<8; x++)
    {
      temp                     = matrix[y][x];
      matrix[y][x]             = matrix[NB_SHADES-y-1][x];
      matrix[NB_SHADES-y-1][x] = temp;
    }
  }

  copy_done = true;
}

//--------------------------------------------------------------------------

public
void printer_put_raster (int    x,
                         int    y,
                         int    size_x,
                         int    size_y,
                         byte[] raster)
{
  byte* bits;
  bool  is_color;
  int   size_per_line;

  packed struct INFO
  {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[2];
  }

  INFO info;

  assert printer_is_open;
  reverse_matrix ();
  
#if debug
  trace ("info: printer_put_raster (x=%d, y=%d, size_x=%d, size_y=%d)\n", x, y, size_x, size_y);
#endif

  assert (x <= max_x && y <= max_y && size_x <= max_x - x && size_y <= max_y - y);

  make_sure_page_is_started ();

  if (size_x == 0 || size_y == 0)    // windows 2000 correction
    return;

  is_color = (pdevmode != null) &&
             (((DEVMODE*)&pdevmode^)->dmFields & DM_COLOR) != 0 &&
             (((DEVMODE*)&pdevmode^)->dmColor == DMCOLOR_COLOR);

#if debug
  trace ("info: printer_put_raster() : is_color = %u\n", is_color);
#endif

  // windows bitmap storage size for 1 line
  if (is_color)
    size_per_line = ALIGN4(size_x * 3);
  else
    size_per_line = ALIGN32(size_x) / 8;

  clear info;
  info.bmiHeader.biSize          = BITMAPINFOHEADER'size;
  info.bmiHeader.biWidth         = size_x;
  info.bmiHeader.biHeight        = size_y;
  info.bmiHeader.biPlanes        = 1;
  info.bmiHeader.biBitCount      = (byte)(is_color ? 24 : 1);
  info.bmiHeader.biCompression   = BI_RGB;
  info.bmiHeader.biSizeImage     = (uint)(size_per_line * size_y);
  info.bmiHeader.biXPelsPerMeter = 1024;
  info.bmiHeader.biYPelsPerMeter = 1024;
  info.bmiHeader.biClrUsed       = 0;
  info.bmiHeader.biClrImportant  = 0;
  info.bmiColors[0].rgbRed   = 0;
  info.bmiColors[0].rgbGreen = 0;
  info.bmiColors[0].rgbBlue  = 0;
  info.bmiColors[1].rgbRed   = 255;
  info.bmiColors[1].rgbGreen = 255;
  info.bmiColors[1].rgbBlue  = 255;

  bits = malloc (info.bmiHeader.biSizeImage);
  if (bits == null)
  {
#if debug
  trace ("error: printer_put_raster() : malloc(%u MBytes) failed\n", info.bmiHeader.biSizeImage / 1024 / 1024);
#endif
    return;
  }

  if (is_color)
  {
    byte* s, t;
    uint  line, col;

    s = &raster;   // source
    t = bits + size_per_line * size_y;     // target

    for (line=0; line<(uint)size_y; line++)
    {
      t -= size_per_line;

      for (col=0; col<(uint)size_x; col++)
      {
        t[col*3+0] = s[col*4+2];
        t[col*3+1] = s[col*4+1];
        t[col*3+2] = s[col*4+0];
      }

      s += 4 * size_x;
    }
  }
  else   // 1 bit per pixel
  {
    uint  line;
    uint  i;
    byte* t, s;
    uint  sum, bit;
    uint  shade, mask;

    bits[0 : info.bmiHeader.biSizeImage] = {all => 0xFF};

    s = &raster;   // source
    t = bits + size_per_line * size_y;    // target

    for (line=0; line<(uint)size_y; line++)
    {
      t -= size_per_line;

      for (i=0; i<(uint)size_x; i++)
      {
        sum = (uint)(30*(uint)s[i*4]
                   + 59*(uint)s[i*4+1]
                   + 11*(uint)s[i*4+2]) / 100;
        if (sum > 255)
          sum = 255;

        shade = sum * (uint)NB_SHADES / 256;  // 0 .. NB_SHADES-1

        mask = (1 << (7 - (((uint)x + i) & 7)));

        bit = (uint)((matrix[shade][((uint)y + line) & 7] & mask) != 0);

        if (bit == 0)
          t[i / 8] &= (byte)(255 - ((1 << (7-(i & 7)))));
      }

      s += 4 * size_x;
    }
  }


  StretchDIBits (hdc, x, y, size_x, size_y,
                 0, 0, size_x, size_y, bits,
                 (BITMAPINFO *)&info, DIB_RGB_COLORS, SRCCOPY);

  freem (bits);
}

//--------------------------------------------------------------------------

public
int nb_jobs_in_spool (string printer_name = "")
{
  char    szPrinter[260];
  HANDLE  h;
  byte[]^ pinfo;
  int     count;
  DWORD   needed;

  if (strlen(printer_name) == 0)      // empty string
  {
    DWORD size = szPrinter'size;
    if (GetDefaultPrinterA (&szPrinter, &size) == 0)
      return -1;
  }
  else
  {
    strcpy (out szPrinter, printer_name);
  }

  szPrinter[szPrinter'size-1] = nul;
  
  if (OpenPrinterA (&szPrinter, &h, null) == FALSE)
    return -1;

  needed = 0;
  GetPrinterA (h, 2, (byte*)&pinfo, 0, &needed);

  pinfo = new byte[needed];

  if (GetPrinterA (h, 2, &pinfo^, needed, &needed) == FALSE)
  {
    free pinfo;
    ClosePrinter (h);
    return -1;
  }

  count = (int)((PRINTER_INFO_2 *)&pinfo^)->cJobs;
  
  free pinfo;

  if (ClosePrinter (h) == FALSE)
    return -1;

  return count;
}

//--------------------------------------------------------------------------
#end unsafe
