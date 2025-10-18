
// guiutils.c

#if WINDOWS
  use ../win/windows;
#elif ANDROID
  use ../font/fonts;
#else
  bad  
#endif

use ../strings;
use ../gui, ../edtext;
use guitree;

//--------------------------------------------------------------------

FONT  g_cached_font;
HFONT g_hcached_font;
HDC   g_cached_hdc;

//--------------------------------------------------------------------

bool same_font (FONT a, FONT b)
{
  return strcmp (a.name, b.name) == 0 && a.height == b.height && a.style == b.style;
}

//--------------------------------------------------------------------

#if WINDOWS

HFONT create_font (FONT font)
{
  char name[MAX_FONT_NAME_LENGTH];
  strcpy (out name, font.name);
  name[MAX_FONT_NAME_LENGTH-1] = nul;   // font name must never exceed 31 chars + nul

#begin unsafe
  return CreateFontA (font.height,
                      0,             // use matching width
                      0, 0,
                      (font.style & BOLD) != 0 ? FW_BOLD : FW_DONTCARE,
                      (DWORD)((font.style & ITALIC) != 0),
                      (DWORD)((font.style & UNDERLINED) != 0),
                      (DWORD)FALSE, 
                      ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DRAFT_QUALITY,
                      DEFAULT_PITCH,
                      &name);
#end unsafe
}

#endif

//--------------------------------------------------------------------

#if WINDOWS

HFONT cached_font_of (FONT font)
{
#begin unsafe
  if (!same_font (font, g_cached_font))
  {
    if (g_hcached_font != 0)
      assert DeleteObject (g_hcached_font) != 0;

    g_cached_font = font;
    g_hcached_font = create_font (font);
    assert g_hcached_font != 0;
  }

  return g_hcached_font;
#end unsafe
}

#endif

//--------------------------------------------------------------------

#if WINDOWS

public HDC get_cached_hdc ()
{
#begin unsafe
  if (g_cached_hdc == 0)
  {
    HDC hdc = GetDC (0);
    g_cached_hdc = CreateCompatibleDC (hdc);
    assert g_cached_hdc != 0;
    ReleaseDC (0, hdc);
  }
  return g_cached_hdc;
#end unsafe
}

#endif

//--------------------------------------------------------------------

#if WINDOWS

int avg_font_size (FONT font)
{
#begin unsafe
  const wstring TEXT = L"X\0";
  HFONT  old_font, new_font;
  HDC    hdc;
  SIZE   size;

  hdc = get_cached_hdc();
  new_font = create_font (font);
  old_font = SelectObject (hdc, new_font);

  if (GetTextExtentPoint32W (hdc, &TEXT, 1, &size) == FALSE)
    size.cx = font.height;

  SelectObject (hdc, old_font);
  assert DeleteObject (new_font) != 0;

  return size.cx;
#end unsafe
}

#endif

//--------------------------------------------------------------------

#if WINDOWS

public HFONT get_cached_font (FONT font)
{
  if (g_scale == 1)
  {
    return cached_font_of (font);
  }
  else
  {
    FONT ft;
    int  max_width;
    
    ft = font;
    ft.height = ui_unscale (ft.height);
    max_width = g_scale * avg_font_size (ft);
    
    ft = font;
    while (ft.height >= 6)
    {
      if (avg_font_size (ft) <= max_width)
        break;
      ft.height--;
    }

    return cached_font_of (ft);
  }
}

#endif

//--------------------------------------------------------------------

public void compute_scrollbox_data
      (int height,           // must be (nb_lines*text_height)
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

  if (scrollbox_height < ui_scale(4))
    scrollbox_height = ui_scale(4);
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

public void compute_editbox_scroll_field (ref CONTROL_INFO o)
{
  ref EDITBOX_INFO editbox = o.editbox;
  int nb_visible_cols      = o.x_size / editbox.font_width;
  int col                  = edit_text_get_col (editbox.cur.text);

  if (editbox.cur.scroll >= col)
  {
    editbox.cur.scroll = col-1;
    if (editbox.cur.scroll < 0)
      editbox.cur.scroll = 0;
  }

  if (editbox.cur.scroll < col - nb_visible_cols + 1)
    editbox.cur.scroll = col - nb_visible_cols + 1;
}

//--------------------------------------------------------------------------

public void compute_editbox_page (ref CONTROL_INFO o)
{
  ref EDITBOX_INFO editbox = o . editbox;
  int ln                   = edit_text_get_ln (editbox.cur.text);

  editbox.nb_full_visible_lines = o.y_size / o.font.height;
  if (editbox.nb_full_visible_lines == 0)
    editbox.nb_full_visible_lines = 1;

  if (editbox.cur.page > edit_text_count_lines(editbox.cur.text) - editbox.nb_full_visible_lines + 1)  // page too large
    editbox.cur.page = edit_text_count_lines(editbox.cur.text) - editbox.nb_full_visible_lines + 1;

  if (editbox.cur.page < ln - (editbox.nb_full_visible_lines - 1))   // page too small
    editbox.cur.page = ln - (editbox.nb_full_visible_lines - 1);

  if (editbox.cur.page < 1)    // page too small
    editbox.cur.page = 1;
  
  if (editbox.cur.page > ln)   // page too large (must be <= ln)
    editbox.cur.page = ln;
}

//--------------------------------------------------------------------------

public int intern_text_width_of2 (string text, FONT font)
{
#if WINDOWS

#begin unsafe
  int   length;
  HFONT old_font, new_font;
  HDC   hdc;
  SIZE  size;

  length = strlen(text);

  hdc = get_cached_hdc();

  new_font = get_cached_font (font);
  old_font = SelectObject (hdc, new_font);

  if (GetTextExtentPoint32A (hdc, &text, length, &size) == FALSE)
    size.cx = font.height * length;

  if ((font.style & ITALIC) != 0)
  {
    if (size.cx != 0)
      size.cx += (size.cy >> 3);
  }

  SelectObject (hdc, old_font);

  return size.cx;
#end unsafe

#elif ANDROID
  return fonts.width_of_font_text (text, fonts.create_font (font.name, font.height, font.style));

#else
  bad
  
#endif  
}

//--------------------------------------------------------------------

public int intern_wtext_width_of2 (wstring text, FONT font)
{
#if WINDOWS

#begin unsafe
  int    length;
  HFONT  old_font, new_font;
  HDC    hdc;
  SIZE   size;

  length = wstrlen(text);

  hdc = get_cached_hdc();

  new_font = get_cached_font (font);
  old_font = SelectObject (hdc, new_font);

  if (GetTextExtentPoint32W (hdc, &text, length, &size) == FALSE)
    size.cx = font.height * length;

  if ((font.style & ITALIC) != 0)
  {
    if (size.cx != 0)
      size.cx += (size.cy >> 3);
  }

  SelectObject (hdc, old_font);

  return size.cx;
#end unsafe

#elif ANDROID
  return fonts.width_of_font_wtext (text, fonts.create_font (font.name, font.height, font.style));

#else
  bad
  
#endif  
}

//--------------------------------------------------------------------

public void recompute_font_width (ref CONTROL_INFO o)
{
  assert o.typ == TYP_EDITBOX;
  o.editbox.font_width = intern_text_width_of2 ("W", o.font);
}

//--------------------------------------------------------------------------
