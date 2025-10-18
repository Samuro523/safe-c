
// gui.c

use arithm, strings, edline, edtext, text;

#if WINDOWS
  use win/windows;
  use gui/guihash;
#elif ANDROID
  use logging;
  use gui/guiandroid, gui/guigdi;
  use draw3d, image;
#else
  bad
#endif

use gui/guitree, gui/guiutil, gui/guicb, gui/guitreatevent;

//--------------------------------------------------------------------

typedef int MY_COMPARE (wstring^ data1, wstring^ data2);

//--------------------------------------------------------------------
//---------------  SCREEN  -------------------------------------------
//--------------------------------------------------------------------

// returns best UI scale (1 to 4)

public int advised_ui_scale()
{
  int x_size, y_size, scale;

  gui.get_desktop_resolution (out x_size, out y_size);
  _unused y_size;

#if WINDOWS
  scale = x_size / 1250;   // scale 2 starts at X >= 2500
#elif ANDROID
  scale = x_size / 1000;   // scale 2 starts at X >= 2000
#else
  bad
#endif
  
  if (scale < 1)
    scale = 1;
  if (scale > 4)
    scale = 4;

  return scale;
}

//--------------------------------------------------------------------

public void init_ui_scale (int scale)
{
  assert scale >= 1 && scale <= 4;

  g_scale = scale;

  {
    const UNSCALE G_UNSCALE[4] = {{factor => 1, shift => 0},        // x1
                                  {factor => 1, shift => 1},        // x2
                                  {factor => 10923, shift => 15},   // x3
                                  {factor => 1, shift => 2}};       // x4

    g_unscale = G_UNSCALE[g_scale-1];
  }
}

//-------------------------------------------------------------------------------------------

public int ui_scale (int size)
{
  return size * g_scale;
}

//-------------------------------------------------------------------------------------------

public int ui_unscale (int size)
{
  if (size >= 0)
    return (size * g_unscale.factor) >> g_unscale.shift;
  else
    return -(((-size) * g_unscale.factor) >> g_unscale.shift);
}

//-------------------------------------------------------------------------------------------

// example: (800 x 600), (1024 x 768), (1280 x 1024), ...

public void get_desktop_resolution (out int x_size, out int y_size)
{
#if WINDOWS
#begin unsafe
  RECT        rect;
  HMONITOR    h;
  MONITORINFO info;

  clear rect;
  if (main_hWnd != 0)
    GetClientRect (main_hWnd, &rect);

  h = MonitorFromRect (rect    => &rect,
                       dwFlags => MONITOR_DEFAULTTONEAREST);

  clear info;
  info.cbSize = info'size;
  GetMonitorInfoA (h, &info);
  rect = info.rcMonitor;

  x_size = rect.right  - rect.left;
  y_size = rect.bottom - rect.top;
#end unsafe

#elif ANDROID
  draw3d.get_desktop_resolution (out x_size, out y_size);

/*
  if (x_size > y_size)  // if paysage, convert to portrait
  {
    int t = x_size;
    x_size = y_size;
    y_size = t;
  }

  if (x_size < y_size)  // if portrait, convert to paysage
  {
    int t = x_size;
    x_size = y_size;
    y_size = t;
  }
*/

#else
  bad
#endif
}

//--------------------------------------------------------------------

// get active area of desktop, excluding task bar.

public void get_desktop_active_area (out int x, out int y, out int x_size, out int y_size)
{
#if WINDOWS
#begin unsafe
  RECT        rect;
  HMONITOR    h;
  MONITORINFO info;

  clear rect;
  if (main_hWnd != 0)
    GetClientRect (main_hWnd, &rect);

  h = MonitorFromRect (rect    => &rect,
                       dwFlags => MONITOR_DEFAULTTONEAREST);

  clear info;
  info.cbSize = info'size;
  GetMonitorInfoA (h, &info);
  rect = info.rcWork;

  x = rect.left;
  y = rect.top;

  x_size = rect.right  - rect.left;
  y_size = rect.bottom - rect.top;
#end unsafe

#elif ANDROID
  x = 0;
  y = 0;
  get_desktop_resolution (out x_size, out y_size);

#else
  bad
#endif
}

//--------------------------------------------------------------------
//---------------  FONT  ---------------------------------------------
//--------------------------------------------------------------------

public int text_width_of2 (string text, FONT font)
{
  FONT f;
  f = font;
  f.height = ui_scale (f.height);
  return ui_unscale (intern_text_width_of2 (text, f));
}

//--------------------------------------------------------------------

public int wtext_width_of2 (wstring text, FONT font)
{
  FONT f;
  f = font;
  f.height = ui_scale (f.height);
  return ui_unscale (intern_wtext_width_of2 (text, f));
}

//--------------------------------------------------------------------------
// --------------------------- DIALOGS -------------------------------------
//--------------------------------------------------------------------------

#begin unsafe

public void create_dialog (DIALOG_ID d, DIALOG_HANDLER dialog_handler)
{
#if WINDOWS
  guihash . treat_create_dialog (d);

  // pass this task to the windows thread, so that the thread
  // that created the window also treats its messages (more efficient and no sync needed).
  SendMessageA (main_hWnd, WM_USER + 20, d, *(int*)&dialog_handler);

#elif ANDROID
  guiandroid . create_dialog (d, dialog_handler);

#else
  bad

#endif
}

//--------------------------------------------------------------------------

public void close_dialog (DIALOG_ID d)
{
#if WINDOWS
  HWND h = guihash . treat_delete_dialog (d);
  if (h != 0)
    SendMessageA (h, WM_USER + 22, 0, 0);

#elif ANDROID
  guiandroid . close_dialog (d);

#else
  bad

#endif
}

//--------------------------------------------------------------------

// make dialog d visible/invisible

public void show_dialog (bool visible, DIALOG_ID d)
{
#if WINDOWS
  HWND h = find_dialog_window (d);
  if (h != 0)
  {
    uint         ShowWindowArg = visible ? SW_SHOW : SW_HIDE;
    DIALOG_INFO* p             = (DIALOG_INFO*)GetWindowLongPtrA (h, 0);
    if (p != null)
      p->ShowWindowArg = ShowWindowArg;
    ShowWindow (h, (int)ShowWindowArg);
  }

#elif ANDROID
  // android to do $
  _unused visible, d;

#else
  bad

#endif
}

//--------------------------------------------------------------------------

// put dialog in foreground (like clicking on it) (call with d == 0 for deactivating all dialogs)

public void activate_dialog (DIALOG_ID d)
{
#if WINDOWS
  HWND h;

  if (d == 0)
    h = main_hWnd;
  else
    h = find_dialog_window (d);

  SetActiveWindow (h);

#elif ANDROID
  // android to do $
  _unused d;

#else
  bad

#endif
}

//--------------------------------------------------------------------------

// allow dialog to receive keyboard/mouse events, or not

public void enable_dialog (bool enabled, DIALOG_ID d)
{
#if WINDOWS
  HWND h = find_dialog_window (d);
  if (h != 0)
    EnableWindow (h, (BOOL)enabled);

#elif ANDROID
  // android to do $
  _unused enabled, d;

#else
  bad

#endif
}

//--------------------------------------------------------------------------

// returns windows handle of dialog

public long hwnd_of (DIALOG_ID d)
{
  return find_dialog_window (d);
}

//--------------------------------------------------------------------------

void fatal_error (string message, object[] args)
{
#if WINDOWS
  char buffer[1024];
  const string title = "Gui - Fatal Error\0";
  sprintf (out buffer, message, args);
  buffer[buffer'length-1] = nul;
  MessageBoxA (main_hWnd, &buffer, &title, MB_OK);

#elif ANDROID
  log ("Gui - Fatal Error");
  log (message, args);

#else
  bad

#endif

  abort;  // to make error point known to the user
}

//---------------------------------------------------------------------------------
// --- all functions below can only be used WITHIN a dialog handler function ------
//---------------------------------------------------------------------------------

void check_in_dialog_handler ()
{
  if (g_dialog_ptr == null)
    fatal_error ("this function must be called within a dialog handler");
}

//---------------------------------------------------------------------------------

public void close_my_dialog ()
{
#if WINDOWS
  HWND hwnd;
  check_in_dialog_handler ();
  hwnd = guihash . treat_delete_dialog (id => g_dialog_ptr->d);
  if (hwnd != 0)
    SendMessageA (hwnd, WM_USER + 22, 0, 0);

#elif ANDROID
  guiandroid . close_dialog (g_dialog_ptr->d);

#else
  bad

#endif
}

//--------------------------------------------------------------------------

void repaint_dialog ()
{
#if WINDOWS
  InvalidateRect (g_dialog_ptr->hwnd, null, FALSE);

#elif ANDROID
  g_dialog_ptr->needs_repaint = true;
  g_dialog_ptr->full_repaint = true;
  g_dialogs_needs_repaint = true;

#else
  bad

#endif
}

//--------------------------------------------------------------------------

wstring^ zwstr (wstring str)
{
  int len = wstrlen(str);
  wstring^ p = new wchar[len+1];
  p^[0 : len] = str[0 : len];
  p^[len] = Lnul;
  return p;
}

//--------------------------------------------------------------------------

wstring^ zstr (string str)
{
  int len = strlen(str);
  int i;
  wstring^ p = new wchar[len+1];
  for (i=0; i<len; i++)
    p^[i] = (wchar)(int)str[i];
  p^[len] = Lnul;
  return p;
}

//--------------------------------------------------------------------------

public void set_dialog_title (string title)
{
  check_in_dialog_handler ();

#if WINDOWS
  {
    wstring^ p = zstr (title);
    SendMessagePtrW (g_dialog_ptr->hwnd, WM_SETTEXT, 0, &p^);
    free p;
  }

#elif ANDROID
  guiandroid.set_dialog_title (title);

#else
  bad

#endif

  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void wset_dialog_title (wstring title)
{
  check_in_dialog_handler ();

#if WINDOWS
  {
    wstring^ p = zwstr (title);
    SendMessagePtrW (g_dialog_ptr->hwnd, WM_SETTEXT, 0, &p^);
    free p;
  }

#elif ANDROID
  guiandroid.wset_dialog_title (title);

#else
  bad

#endif

  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_position (int x, int y)
{
  int scaled_x = ui_scale(x);
  int scaled_y = ui_scale(y);

  check_in_dialog_handler ();

  if (sub_window_windowpos != null)   // we're in WM_WINDOWPOSCHANGING
  {
    sub_window_windowpos->x = scaled_x + (main_win_rect.left + x_border);
    sub_window_windowpos->y = scaled_y + (main_win_rect.top + y_border + y_title);
  }
  else
  {
    int dx = scaled_x - (g_dialog_ptr->rect.left - (main_win_rect.left + x_border));
    int dy = scaled_y - (g_dialog_ptr->rect.top - (main_win_rect.top + y_border + y_title));

    g_dialog_ptr->rect.left   += dx;
    g_dialog_ptr->rect.top    += dy;
    g_dialog_ptr->rect.bottom += dy;
    g_dialog_ptr->rect.right  += dx;

#if WINDOWS
    SetWindowPos (g_dialog_ptr->hwnd,
                  0,
                  g_dialog_ptr->rect.left,
                  g_dialog_ptr->rect.top,
                  0,
                  0,
                  SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
#elif ANDROID
  g_dialog_ptr->needs_repaint = true;
  g_dialogs_needs_repaint = true;

#else
  bad

#endif
  }
}

//--------------------------------------------------------------------------

// extern size (border and title included)

public void set_dialog_size (int x_size, int y_size)
{
  int scaled_x_size = ui_scale(x_size);
  int scaled_y_size = ui_scale(y_size);

  check_in_dialog_handler ();

  if (sub_window_windowpos != null)   // we're in WM_WINDOWPOSCHANGING
  {
    sub_window_windowpos->cx = scaled_x_size;
    sub_window_windowpos->cy = scaled_y_size;
  }
  else
  {
    g_dialog_ptr->rect.right  = g_dialog_ptr->rect.left + scaled_x_size;
    g_dialog_ptr->rect.bottom = g_dialog_ptr->rect.top + scaled_y_size;

#if WINDOWS
    SetWindowPos (g_dialog_ptr->hwnd,
                  0,
                  0,
                  0,
                  scaled_x_size,
                  scaled_y_size,
                  SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);

#elif ANDROID
  g_dialog_ptr->needs_repaint = true;
  g_dialogs_needs_repaint = true;

#else
  bad

#endif
  }

  repaint_dialog ();
}

//--------------------------------------------------------------------------

// set border size that can be clicked to resize window using EVENT_RESIZE (default = 0)

public void set_dialog_border_drag_size (int border_drag_size)
{
  check_in_dialog_handler ();
  g_dialog_ptr->border_drag_size = ui_scale(border_drag_size);
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_border_visual_size (int border_visual_size)
{
  check_in_dialog_handler ();
  g_dialog_ptr->border_size = ui_scale(border_visual_size);
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_border_colors (uint[4] colors)
{
  int i;
  check_in_dialog_handler ();
  g_dialog_ptr->dialog_colors[0:2] = colors[0:2];
  g_dialog_ptr->dialog_colors[3:2] = colors[2:2];
  for (i=0; i<g_dialog_ptr->dialog_colors'length; i++)
    g_dialog_ptr->dialog_colors[i] &= 0xFFFFFF;
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_title_height (int height)  // default = 20
{
  check_in_dialog_handler ();
  g_dialog_ptr->title_height = ui_scale(height);
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_title_font (FONT font)   // includes color
{
  check_in_dialog_handler ();
  g_dialog_ptr->title_font = font;
  g_dialog_ptr->title_font.height = ui_scale(font.height);
  g_dialog_ptr->title_font.color &= 0xFFFFFF;
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_title_background_color (uint color)
{
  check_in_dialog_handler ();
  g_dialog_ptr->dialog_colors[2] = color & 0xFFFFFF;
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_close_button (bool enabled)
{
  check_in_dialog_handler ();
  g_dialog_ptr->has_close_button = enabled;
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_background_color (uint color)
{
  check_in_dialog_handler ();
  g_dialog_ptr->dialog_background_color = color & 0xFFFFFF;
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_transparency (int focus, int non_focus)   // default = (255, 165)
{
  check_in_dialog_handler ();
  g_dialog_ptr->dialog_transparency_focus = focus;
  g_dialog_ptr->dialog_transparency_non_focus = non_focus;
  repaint_dialog ();
}

//--------------------------------------------------------------------------

public void set_dialog_default_font (FONT default_font)   // set new default font for all new controls of this dialog.
{
  check_in_dialog_handler ();
  if (default_font.height < 1)
    fatal_error ("default_font.height must be >= 1");
  g_dialog_ptr->default_font = default_font;
  g_dialog_ptr->default_font.height = ui_scale(default_font.height);
  g_dialog_ptr->default_font.color &= 0xFFFFFF;
}

//--------------------------------------------------------------------------

public void set_dialog_default_control_colors (GUI_COLORS default_colors)   // set new default colors for all new controls of this dialog.
{
  int i;
  check_in_dialog_handler ();
  g_dialog_ptr->default_colors = default_colors;
  for (i=0; i<g_dialog_ptr->default_colors'length; i++)
    g_dialog_ptr->default_colors[i] &= 0xFFFFFF;
}

//--------------------------------------------------------------------------

public void set_dialog_default_insert_mode (bool insert)
{
  check_in_dialog_handler ();
  g_dialog_ptr->default_insert_mode = insert;
}

//--------------------------------------------------------------------------

public void redirect_keyboard_to_main_window (bool redirect)
{
  check_in_dialog_handler ();
  g_dialog_ptr->redirect_keyboard_to_main_window = redirect;
}

//--------------------------------------------------------------------------
// ---------- GENERAL SETTINGS APPLYING ON CONTROLS ------------------------
//--------------------------------------------------------------------------

CONTROL_INFO^ load_control (CONTROL_ID id)
{
  CONTROL_INFO^ l;

  check_in_dialog_handler ();

  l = g_dialog_ptr->list;
  if (l != null)
  {
    for (;;)
    {
      if (l^.id == id)
        return l;

      l = l^.next;
      if (l == g_dialog_ptr->list)
        break;
    }
  }

  fatal_error ("control id %d was not found", id);
  abort;
}

//--------------------------------------------------------------------

CONTROL_INFO^ load_control_of_typ (CONTROL_ID id, CONTROL_TYP typ)
{
  CONTROL_INFO^ l = load_control (id);

  if (l^.typ != typ)
    fatal_error ("control id %d must denote %s", id, typ_name[(int)typ]);

  return l;
}

//--------------------------------------------------------------------

CONTROL_INFO^ load_control_of_typ2 (CONTROL_ID id, CONTROL_TYP typ1, CONTROL_TYP typ2)
{
  CONTROL_INFO^ l = load_control (id);

  if (l^.typ != typ1 && l^.typ != typ2)
    fatal_error ("control id %d must denote %s or %s", id, typ_name[(int)typ1], typ_name[(int)typ2]);

  return l;
}

//--------------------------------------------------------------------

CONTROL_INFO^ load_control_of_typ3 (CONTROL_ID id, CONTROL_TYP typ1, CONTROL_TYP typ2, CONTROL_TYP typ3)
{
  CONTROL_INFO^ l = load_control (id);

  if (l^.typ != typ1 && l^.typ != typ2 && l^.typ != typ3)
    fatal_error ("control id %d must denote %s, %s or %s", id, typ_name[(int)typ1], typ_name[(int)typ2], typ_name[(int)typ3]);

  return l;
}

//--------------------------------------------------------------------

CONTROL_INFO^ load_control_of_typ4 (CONTROL_ID id, CONTROL_TYP typ1, CONTROL_TYP typ2, CONTROL_TYP typ3, CONTROL_TYP typ4)
{
  CONTROL_INFO^ l = load_control (id);

  if (l^.typ != typ1 && l^.typ != typ2 && l^.typ != typ3 && l^.typ != typ4)
    fatal_error ("control id %d must denote %s, %s, %s or %s", id, typ_name[(int)typ1], typ_name[(int)typ2], typ_name[(int)typ3], typ_name[(int)typ4]);

  return l;
}

//--------------------------------------------------------------------

CONTROL_INFO^ load_control_of_typ5 (CONTROL_ID id, CONTROL_TYP typ1, CONTROL_TYP typ2, CONTROL_TYP typ3, CONTROL_TYP typ4, CONTROL_TYP typ5)
{
  CONTROL_INFO^ l = load_control (id);

  if (l^.typ != typ1 && l^.typ != typ2 && l^.typ != typ3 && l^.typ != typ4 && l^.typ != typ5)
    fatal_error ("control id %d must denote %s, %s, %s, %s or %s", id, typ_name[(int)typ1], typ_name[(int)typ2], typ_name[(int)typ3], typ_name[(int)typ4], typ_name[(int)typ5]);

  return l;
}

//--------------------------------------------------------------------

public void set_focus (CONTROL_ID id)
{
  CONTROL_INFO^ p = load_control (id);

  if (!tabable[(int)p^.typ])
    fatal_error ("set_focus on control %d is not allowed", id);

  if (g_dialog_ptr->focus == p)
    return;

  if (g_dialog_ptr->focus != null)
    repaint_control (g_dialog_ptr->focus^);

  g_dialog_ptr->focus = p;

  repaint_control (p^);
}

//--------------------------------------------------------------------

public void get_position (out int x, out int y, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;
  x = ui_unscale(o.x);
  y = ui_unscale(o.y);
}

//--------------------------------------------------------------------

public void set_position (int x, int y, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;
  int scaled_x = ui_scale(x);
  int scaled_y = ui_scale(y);

  if (o.x == scaled_x && o.y == scaled_y)
    return;

  repaint_control (o);   // redraw old control space

  o.x = scaled_x;
  o.y = scaled_y;

  repaint_control (o);   // redraw old control space
}

//--------------------------------------------------------------------

public void get_size (out int x_size, out int y_size, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;
  x_size = ui_unscale(o.x_size);
  y_size = ui_unscale(o.y_size);
}

//--------------------------------------------------------------------

public void set_size (int x_size, int y_size, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;
  int scaled_x_size = ui_scale(x_size);
  int scaled_y_size = ui_scale(y_size);

  if (o.x_size == scaled_x_size && o.y_size == scaled_y_size)
    return;

  repaint_control (o);   // redraw old control space

  o.x_size = scaled_x_size;
  o.y_size = scaled_y_size;

  switch (o.typ)
  {
    case TYP_LISTBOX:
      o.listbox.nb_screen_lines = (o.y_size - ui_scale(4)) / o.font.height;
      break;

    case TYP_TREE:
      o.tree.listbox.nb_screen_lines = (o.y_size - ui_scale(4)) / o.font.height;
      break;

    case TYP_WINDOW:
      {
#if WINDOWS
        int    common_x_size, common_y_size;
        HANDLE hdc, hbitmap, hdcMemory, old_hbitmap;

        if (scaled_x_size < 1 || scaled_y_size < 1)
          fatal_error ("x_size/y_size is too small");

        common_x_size = min (o.x_size, scaled_x_size);
        common_y_size = min (o.y_size, scaled_y_size);

        hdc = GetDC (g_dialog_ptr->hwnd);
        hbitmap = CreateCompatibleBitmap (hdc, scaled_x_size, scaled_y_size);
        assert hbitmap != 0;
        hdcMemory = CreateCompatibleDC (hdc);
        assert hdcMemory != 0;
        ReleaseDC (g_dialog_ptr->hwnd, hdc);

        old_hbitmap = SelectObject (hdcMemory, hbitmap);

        // copy old to new bitmap
        BitBlt (hdcDest => hdcMemory,
                nXDest  => 0,
                nYDest  => 0,
                nWidth  => common_x_size,
                nHeight => common_y_size,
                hdcSrc  => o.window.hdcMemory,
                nXSrc   => 0,
                nYSrc   => 0,
                dwRop   => SRCCOPY);

        // free old bitmap
        SelectObject (o.window.hdcMemory, o.window.old_hbitmap);
        assert DeleteObject (o.window.hbitmap) != 0;
        assert DeleteDC (o.window.hdcMemory) != 0;

        // assign new bitmap
        o.window.hbitmap   = hbitmap;
        o.window.hdcMemory = hdcMemory;
        o.window.old_hbitmap = old_hbitmap;

#elif ANDROID
        free o.window.hdcMemory.image;
        clear o.window.hdcMemory;
        o.window.hdcMemory.image  = new byte[4 * o.x_size * o.y_size];
        o.window.hdcMemory.width  = (uint)o.x_size;
        o.window.hdcMemory.height = (uint)o.y_size;
        o.window.hdcMemory.rect   = {left   => 0,
                                     top    => 0,
                                     right  => o.x_size,
                                     bottom => o.y_size};
#endif
      }
      break;

    case TYP_EDITBOX:
      compute_editbox_scroll_field (ref o);
      compute_editbox_page (ref o);
      edit_text_set_redraw_screen_needed (ref o.editbox.cur.text);
      break;

    default:
      break;
  }

  repaint_control (o);  // redraw new control
}

//--------------------------------------------------------------------

public void get_visible (out bool visible, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;
  visible = !o.hide;
}

//--------------------------------------------------------------------

public void set_visible (bool visible, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;

  if (o.hide == !visible)
    return;

  o.hide = !visible;
  repaint_control (o);
}

//--------------------------------------------------------------------

public void set_visibles (bool visible, CONTROL_ID id1, CONTROL_ID id2)
{
  CONTROL_INFO^ l;

  check_in_dialog_handler ();

  l = g_dialog_ptr->list;
  if (l == null)
    return;

  for (;;)
  {
    if (l^.id >= id1 && l^.id <= id2)
      l^.hide = !visible;

    l = l^.next;
    if (l == g_dialog_ptr->list)
      break;
  }

  repaint_dialog();
}

//--------------------------------------------------------------------

public void set_font (FONT font, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;
  FONT scaled_font = font;
  scaled_font.height = ui_scale (font.height);

  if (o.typ == TYP_TEXT)
  {
    assert scaled_font.height <= o.y_size;
  }
  else if (o.typ == TYP_EDIT || o.typ == TYP_CHECKBOX || o.typ == TYP_RADIOBUTTON || o.typ == TYP_BUTTON || o.typ == TYP_EDITBOX)
  {
    assert scaled_font.height <= o.y_size-ui_scale(4);
    if (o.typ == TYP_EDITBOX)
    {
      recompute_font_width (ref o);
      compute_editbox_scroll_field (ref o);
      compute_editbox_page (ref o);
    }
  }
  else if (o.typ == TYP_WINDOW)
  {
  }
  else
  {
    fatal_error ("set_font() is only allowed on text, edit, checkbox, radiobutton and buttons\n");
  }

  if (scaled_font.height < 1)
    fatal_error ("font.height must be >= 1");

  o.font = scaled_font;
  o.font.color &= 0xFFFFFF;

  if (o.typ != TYP_WINDOW)
    repaint_control (o);
}

//--------------------------------------------------------------------

public void get_font (out FONT font, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control (id) ^;
  font = o.font;
  font.height = ui_unscale (font.height);
}

//--------------------------------------------------------------------

public void set_colors (GUI_COLORS colors, CONTROL_ID id)
{
  CONTROL_INFO^ p = load_control (id);
  int i;
  p^.colors = colors;
  for (i=0; i<p^.colors'length; i++)
    p^.colors[i] &= 0xFFFFFF;
  repaint_control (p^);
}

//--------------------------------------------------------------------

public void get_colors (out GUI_COLORS colors, CONTROL_ID id)
{
  CONTROL_INFO^ p = load_control (id);
  colors = p^.colors;
}

//--------------------------------------------------------------------

EDIT_INFO* fetch_edit_or_combo (CONTROL_INFO o)
{
  if (o.typ == TYP_EDIT)
    return &o.edit;
  else
    return &o.combo.edit;
}

//--------------------------------------------------------------------

public void set_insert_mode (bool insert, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ3 (id, TYP_EDIT, TYP_COMBO, TYP_EDITBOX) ^;
  if (o.typ == TYP_EDITBOX)
  {
    edit_text_set_insert_mode (ref o.editbox.cur.text, insert_mode => insert);
  }
  else
  {
    ref EDIT_INFO edit = *fetch_edit_or_combo (o);
    edit_line_set_insert_mode (ref edit.cr.edit_line, insert_mode => insert);
  }
}

//--------------------------------------------------------------------

public bool get_insert_mode (CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ3 (id, TYP_EDIT, TYP_COMBO, TYP_EDITBOX) ^;
  if (o.typ == TYP_EDITBOX)
  {
    return edit_text_get_insert_mode (o.editbox.cur.text);
  }
  else
  {
    ref EDIT_INFO edit = *fetch_edit_or_combo (o);
    return edit_line_get_insert_mode (edit.cr.edit_line);
  }
}

//--------------------------------------------------------------------

public void set_modify_mode (bool modify_allowed, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ5 (id,
                   TYP_EDIT, TYP_COMBO, TYP_EDITBOX, TYP_CHECKBOX, TYP_RADIOBUTTON) ^;

  if (o.typ == TYP_EDITBOX)
  {
    edit_text_set_modify_allowed (ref o.editbox.cur.text, modify_allowed => modify_allowed);
  }
  else if (o.typ == TYP_CHECKBOX)
  {
    o.checkbox.modify_allowed = modify_allowed;
  }
  else if (o.typ == TYP_RADIOBUTTON)
  {
    o.radiobutton.modify_allowed = modify_allowed;
  }
  else
  {
    ref EDIT_INFO edit = *fetch_edit_or_combo (o);
    edit_line_set_modify_allowed (ref edit.cr.edit_line, modify_allowed => modify_allowed);
  }
}

//--------------------------------------------------------------------

public bool get_modify_mode (CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ3 (id, TYP_EDIT, TYP_COMBO, TYP_EDITBOX) ^;
  if (o.typ == TYP_EDITBOX)
  {
    return edit_text_get_modify_allowed (o.editbox.cur.text);
  }
  else
  {
    ref EDIT_INFO edit = *fetch_edit_or_combo (o);
    return edit_line_get_modify_allowed (edit.cr.edit_line);
  }
}

//--------------------------------------------------------------------

void check_control_id (CONTROL_ID id)
{
  CONTROL_INFO^ l;

  if (id < 1)
    fatal_error ("control id %d must be >= 1", id);

  l = g_dialog_ptr->list;
  if (l == null)
    return;

  for (;;)
  {
    if (l^.id == id)
      fatal_error ("control id %d is not unique", id);

    l = l^.next;
    if (l == g_dialog_ptr->list)
      break;
  }
}

//--------------------------------------------------------------------

void check_control_position_and_size (CONTROL_INFO o, int min_x_size, int min_y_size)
{
  int x_size = g_dialog_ptr->rect.right - g_dialog_ptr->rect.left - 2*g_dialog_ptr->border_size;
  int y_size = g_dialog_ptr->rect.bottom - g_dialog_ptr->rect.top - 2*g_dialog_ptr->border_size - g_dialog_ptr->title_height;

  if (o.x < 0 || o.x >= x_size)
    fatal_error ("control %d : illegal x coordinate %d  (must be in %d .. %d)", o.id, ui_unscale(o.x), 0, ui_unscale(x_size)-1);

  if (o.y < 0 || o.y >= y_size)
    fatal_error ("control %d : illegal y coordinate %d  (must be in %d .. %d)", o.id, ui_unscale(o.y), 0, ui_unscale(y_size)-1);

  if (o.x_size < min_x_size || o.x+o.x_size > x_size)
    fatal_error ("control %d : illegal x_size parameter %d  (must be in %d .. %d)", o.id, ui_unscale(o.x_size), ui_unscale(min_x_size), ui_unscale(x_size-o.x));

  if (o.y_size < min_y_size || o.y+o.y_size > y_size)
    fatal_error ("control %d : illegal y_size parameter %d  (must be in %d .. %d)", o.id, ui_unscale(o.y_size), ui_unscale(min_y_size), ui_unscale(y_size-o.y));
}

//--------------------------------------------------------------------

void check_control_collision (CONTROL_INFO o)
{
  CONTROL_INFO^ l;

  l = g_dialog_ptr->list;
  if (l == null)
    return;

  for (;;)
  {
    if (l^.hide)
      ;
    else if (o.x >= l^.x + l^.x_size || o.y >= l^.y + l^.y_size || l^.x >= o.x + o.x_size || l^.y >= o.y + o.y_size)
      ;
    else
    {
      fatal_error ("control %d (x=%d,y=%d,dx=%d,dy=%d) collides with control %d (x=%d,y=%d,dx=%d,dy=%d)",
                   o.id, ui_unscale(o.x), ui_unscale(o.y), ui_unscale(o.x_size), ui_unscale(o.y_size), ui_unscale(l^.id), ui_unscale(l^.x), ui_unscale(l^.y), ui_unscale(l^.x_size), ui_unscale(l^.y_size));
    }

    l = l^.next;
    if (l == g_dialog_ptr->list)
      break;
  }
}

//--------------------------------------------------------------------

void append_control (CONTROL_INFO^ p)
{
  CONTROL_INFO^ last;

  if (g_dialog_ptr->list == null)    // this is the first control
  {
    p^.prev = p;
    p^.next = p;
    g_dialog_ptr->list = p;
  }
  else      // there is already a list
  {
    last = g_dialog_ptr->list^.prev;
    last^.next = p;
    p^.prev = last;
    p^.next = g_dialog_ptr->list;
    g_dialog_ptr->list^.prev = p;
  }
}

//--------------------------------------------------------------------

void check_append_and_repaint_control (CONTROL_INFO^ p, int min_x_size, int min_y_size)
{
  check_control_id (p^.id);
  check_control_position_and_size (p^, min_x_size, min_y_size);
  check_control_collision (p^);
  append_control (p);
  repaint_control (p^);
}

//--------------------------------------------------------------------
//-----------------  TEXT : simple text fields -----------------------
//--------------------------------------------------------------------

public void create_text (int x, int y, int x_size, int y_size, CONTROL_ID id)
{
  CONTROL_INFO^               p = new CONTROL_INFO (TYP_TEXT);
  ref CONTROL_INFO (TYP_TEXT) o = p^;

  check_in_dialog_handler ();

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.text = zstr ("");
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;

  check_append_and_repaint_control (
      p,
      min_x_size => 0,
      min_y_size => o.font.height);
}

//--------------------------------------------------------------------

public void wtext_put (wstring text, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_TEXT) ^;

  free o.text;
  o.text = zwstr (text);

  repaint_control (o);
}

//--------------------------------------------------------------------

public void text_put (string text, CONTROL_ID id)
{
  wstring^ w = zstr (text);
  wtext_put (w^, id);
  free w;
}

//--------------------------------------------------------------------

// examine 'text' and extract 'hotkey' after '&'

void extract_hotkey (wstring text, out wstring^ text2, out wchar hotkey)
{
  int i, j, length;

  length = wstrlen(text);
  j = 0;
  hotkey = Lnul;

  for (i=0; i<length; i++)
  {
    if (text[i] == L'&' && i+1<length && text[i+1] != L'&')
    {
      hotkey = text[i+1];
      i++;
    }

    j++;
  }

  text2 = new wchar[j];
  j = 0;

  for (i=0; i<length; i++)
  {
    if (text[i] == L'&' && i+1<length && text[i+1] != L'&')
      i++;

    text2^[j++] = text[i];
  }
}

//--------------------------------------------------------------------

public void wcreate_button (int x, int y, int x_size, int y_size, wstring text, CONTROL_ID id)
{
  CONTROL_INFO^                 p = new CONTROL_INFO (TYP_BUTTON);
  ref CONTROL_INFO (TYP_BUTTON) o = p^;
  wstring^                      ptext2;
  wchar                         hotkey;

  check_in_dialog_handler ();

  extract_hotkey (text, out ptext2, out hotkey);

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.text   = zwstr (ptext2^);
  o.hotkey = hotkey;
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4),
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

public void create_button (int x, int y, int x_size, int y_size, string text, CONTROL_ID id)
{
  wstring^ s = zstr (text);
  wcreate_button (x, y, x_size, y_size, s^, id);
  free s;
}

//--------------------------------------------------------------------

// modify text of a button

public void wbutton_set_text (wstring text, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_BUTTON) ^;
  wstring^         ptext2;
  wchar            hotkey;

  extract_hotkey (text, out ptext2, out hotkey);

  free o.text;
  o.text = zwstr (ptext2^);
  o.hotkey = hotkey;

  repaint_control (o);
}

//--------------------------------------------------------------------

public void button_set_text (string text, CONTROL_ID id)
{
  wstring^ s = zstr (text);
  wbutton_set_text (s^, id);
  free s;
}

//--------------------------------------------------------------------

public void wcreate_checkbox (int x, int y, int x_size, int y_size, int box_width, wstring text, CONTROL_ID id)
{
  CONTROL_INFO^                   p = new CONTROL_INFO (TYP_CHECKBOX);
  ref CONTROL_INFO (TYP_CHECKBOX) o = p^;

  assert box_width >= 0;

  check_in_dialog_handler ();

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.text   = zwstr (text);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;
  o.checkbox.box_width = ui_scale(box_width);
  o.checkbox.modify_allowed = true;

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4) + o.checkbox.box_width,
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

public void create_checkbox (int x, int y, int x_size, int y_size, int box_width, string text, CONTROL_ID id)
{
  wstring^ s = zstr (text);
  wcreate_checkbox (x, y, x_size, y_size, box_width, s^, id);
  free s;
}

//--------------------------------------------------------------------

public void checkbox_get (out bool setting, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_CHECKBOX) ^;
  setting = o.checkbox.setting;
}

//--------------------------------------------------------------------

public void checkbox_set (bool setting, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_CHECKBOX) ^;
  o.checkbox.setting = setting;
  repaint_control (o);
}

//--------------------------------------------------------------------

public void wcreate_radiobutton (int x, int y, int x_size, int y_size, int box_width, wstring text, RADIOBUTTON_GROUP_ID gid, CONTROL_ID id)
{
  CONTROL_INFO^                      p = new CONTROL_INFO (TYP_RADIOBUTTON);
  ref CONTROL_INFO (TYP_RADIOBUTTON) o = p^;

  assert box_width >= 0;

  check_in_dialog_handler ();

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.text   = zwstr (text);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;
  o.radiobutton.box_width = ui_scale(box_width);
  o.radiobutton.gid = gid;
  o.radiobutton.modify_allowed = true;

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4) + o.radiobutton.box_width,
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

public void create_radiobutton (int x, int y, int x_size, int y_size, int box_width, string text, RADIOBUTTON_GROUP_ID gid, CONTROL_ID id)
{
  wstring^ s = zstr (text);
  wcreate_radiobutton (x, y, x_size, y_size, box_width, s^, gid, id);
  free s;
}

//--------------------------------------------------------------------

public CONTROL_ID radiobutton_selected (RADIOBUTTON_GROUP_ID gid)
{
  CONTROL_INFO^ l;

  check_in_dialog_handler ();

  l = g_dialog_ptr->list;
  if (l != null)
  {
    for (;;)
    {
      if (l^.typ == TYP_RADIOBUTTON && l^.radiobutton.gid == gid && l^.radiobutton.setting)
        return l^.id;

      l = l^.next;
      if (l == g_dialog_ptr->list)
        break;
    }
  }

  return 0;
}

//--------------------------------------------------------------------

public void radiobutton_set (CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_RADIOBUTTON) ^;
  CONTROL_INFO^ l;

  o.radiobutton.setting = true;
  repaint_control (o);

  l = g_dialog_ptr->list;
  if (l != null)
  {
    for (;;)
    {
      if (l^.typ == TYP_RADIOBUTTON && l^.radiobutton.gid == o.radiobutton.gid &&
          l^.radiobutton.setting && l^.id != id)
      {
        l^.radiobutton.setting = false;
        repaint_control (l^);
      }

      l = l^.next;
      if (l == g_dialog_ptr->list)
        break;
    }
  }
}

//--------------------------------------------------------------------

// (x,y) are coordinates relative to the last created screen;
//   for example (0,0) is the upper left screen corner.
// 'x_size' should be = 8+text_width_of(text,..) + arrow_box_width
// 'y_size' must be >= (font_height + 4)
// 'length' denotes the max number of editable characters.

public
void create_combo (int x, int y, int x_size, int y_size,
                   int arrow_box_width, int arrow_box_height,
                   int length,
                   int max_listbox_lines,  // >= 1
                   CONTROL_ID id)
{
  CONTROL_INFO^                p = new CONTROL_INFO (TYP_COMBO);
  ref CONTROL_INFO (TYP_COMBO) o = p^;

  check_in_dialog_handler ();

  if (arrow_box_width < 4 || arrow_box_width >= x_size-4)
    fatal_error ("arrow_box_width (=%d) must be in range %d .. %d", arrow_box_width, 4, x_size-5);

  if (arrow_box_height < 4 || arrow_box_height >= y_size-4)
    fatal_error ("arrow_box_height (=%d) must be in range %d .. %d", arrow_box_height, 4, y_size-5);

  if (length < 1 || length > 512)
    fatal_error ("'length' must be in 1 .. 512");

  if (max_listbox_lines < 1)
    fatal_error ("'max_listbox_lines' must be >= 1");

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;

  edit_line_allocate (out o.combo.edit.cr.edit_line,
                          line                => new wchar[length],     // preallocated current line
                          length              => 0,                     // active length of line, has no trailing spaces
                          col                 => 0,                     // in range 0 ..  line^'length-1, or 0 .. line^.length if extra_col.
                          extra_col           => true,                  // can cursor go past last col
                          modify_allowed      => true,                  // false = line may not change
                          line_modified       => false,                 // true = line was changed
                          insert_mode         => g_dialog_ptr->default_insert_mode,     // false = delete, true = insert
                          switch_mode_allowed => true,                  // false = no switch allowed
                          set_marks_allowed   => true,                  // true = marks can be set
                          marks_modified      => false,                 // true = marks were modified
                          first               => {active => false, col => 0},       // .col included
                          last                => {active => false, col => 0});      // .col excluded

  {
    ref LISTBOX_INFO l = o.combo.listbox;

    wcreat_text (out l.wtext);
    l.page                 = 1;
//    l.ln                   = 0;
//    l.nb_screen_lines      =      // will be filled later
    l.line_length          = length;
    l.line_height          = o.font.height;
    l.scrollbar            = 1;
    l.arrow_box_width      = ui_scale(arrow_box_width);
    l.arrow_box_height     = ui_scale(arrow_box_height);
    l.allow_user_selection = true;   // user can select lines
    l.allow_hotkey_search  = true;   // user can use hotkey
    l.multiple_mode        = false;  // single mode
    l.selected_line        = 0;      // no line selected
    l.show_focus           = false;
  }

  {
    int d_y_size = g_dialog_ptr->rect.bottom - g_dialog_ptr->rect.top - 2*g_dialog_ptr->border_size - g_dialog_ptr->title_height;
    ref LISTBOX_EXTRA l = o.combo.lb;
    int y_space_bottom, y_space_top, y_space, nb_lines;

    l.listbox_shown = false;

    y_space_top    = o.y;
    y_space_bottom = d_y_size - (o.y + o.y_size);
    y_space = max (y_space_top, y_space_bottom);

    nb_lines = (y_space-ui_scale(4)) / o.font.height;
    if (nb_lines < 1)
      fatal_error ("no space for displaying combo's listbox");

    if (nb_lines > max_listbox_lines)
      nb_lines = max_listbox_lines;

    o.combo.listbox.nb_screen_lines = nb_lines;

    l.y_size = ui_scale(4) + nb_lines * o.font.height;

    if (y_space_top > y_space_bottom)   // listbox above edit
    {
      l.y = o.y - l.y_size;
    }
    else  // listbox below edit
    {
      l.y = o.y + o.y_size;
    }
  }

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4) + ui_scale(arrow_box_width),
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

public void create_edit (int x, int y, int x_size, int y_size, int length, CONTROL_ID id)
{
  CONTROL_INFO^               p = new CONTROL_INFO (TYP_EDIT);
  ref CONTROL_INFO (TYP_EDIT) o = p^;

  if (length < 1 || length > 512)
    fatal_error ("'length' must be in 1 .. 512");

  check_in_dialog_handler ();

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;

  edit_line_allocate (out o.edit.cr.edit_line,
                          line                => new wchar[length],     // preallocated current line
                          length              => 0,                     // active length of line, has no trailing spaces
                          col                 => 0,                     // in range 0 ..  line^'length-1, or 0 .. line^.length if extra_col.
                          extra_col           => true,                  // can cursor go past last col
                          modify_allowed      => true,                  // false = line may not change
                          line_modified       => false,                 // true = line was changed
                          insert_mode         => g_dialog_ptr->default_insert_mode,     // false = delete, true = insert
                          switch_mode_allowed => true,                  // false = no switch allowed
                          set_marks_allowed   => true,                  // true = marks can be set
                          marks_modified      => false,                 // true = marks were modified
                          first               => {active => false, col => 0},       // .col included
                          last                => {active => false, col => 0});      // .col excluded

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4),
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

public void wedit_get (out wstring buffer, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO edit = *fetch_edit_or_combo (o);
  edit_line_get_line (edit.cr.edit_line, out line => buffer, filler => Lnul);
}

//--------------------------------------------------------------------

// assertion: strings must have same length

void copy_wstring_string (wstring w, out string s)
{
  int i;
  assert w'length == s'length;
  clear s;
  for (i=0; i<w'length; i++)
  {
    if (w[i] <= (wchar)255)
      s[i] = (char)(int)w[i];
    else
      s[i] = '?';
  }
}

//--------------------------------------------------------------------

public void edit_get (out string buffer, CONTROL_ID id)
{
  wstring^ w = new wstring (buffer'length);
  wedit_get (out w^, id);
  copy_wstring_string (w^, out buffer);
  free w;
}

//--------------------------------------------------------------------

public void wedit_get2 (out wstring buffer, CONTROL_ID id)
{
  int i, j, len;

  wedit_get (out buffer, id);

  i = 0;
  while (i < buffer'length && buffer[i] == L' ')
    i++;

  j = wstrlen(buffer);
  while (j > i && buffer[j-1] == L' ')
    j--;

  len = j-i;

  buffer[0:len] = buffer[i:len];
  buffer[len:buffer'length-len] = {all => Lnul};
}

//--------------------------------------------------------------------

public void edit_get2 (out string buffer, CONTROL_ID id)
{
  wstring^ w = new wstring (buffer'length);
  wedit_get2 (out w^, id);
  copy_wstring_string (w^, out buffer);
  free w;
}

//--------------------------------------------------------------------

public void wedit_put (wstring text, CONTROL_ID id)
{
  ref CONTROL_INFO o    = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO    edit = *fetch_edit_or_combo (o);
  ref EDIT_LINE    e    = edit.cr.edit_line;

  edit_line_set_line (ref e, text[0:wstrlen(text)]);
  edit_line_set_col  (ref e, col => 0);

  {
    LINE_MARK m;
    clear m;
    edit_line_set_first_mark (ref e, m);
    edit_line_set_last_mark (ref e, m);
  }
  edit.cr.scroll_x_offset = 0;
  edit.line_marking_in_progress = false;
  edit.line_marking_start_col = 0;

  repaint_control (o);
}

//--------------------------------------------------------------------

public void edit_put (string text, CONTROL_ID id)
{
  wstring^ w = zstr (text);
  wedit_put (w^, id);
  free w;
}

//--------------------------------------------------------------------

// for edit or combo

public void edit_reset_undo (CONTROL_ID id)
{
  ref CONTROL_INFO o    = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO    edit = *fetch_edit_or_combo (o);
  int              i;

  for (i=0; i<edit.undo_slots_filled; i++)
    edit_line_dispose (ref edit.undo[i].edit_line);

  edit.next_undo_slot = 0;
  edit.undo_slots_filled = 0;
  clear edit.undo;
}

//--------------------------------------------------------------------

// for edit or combo, to be called after edit_put

public void edit_save_value_for_undo (CONTROL_ID id)
{
  ref CONTROL_INFO o    = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO    edit = *fetch_edit_or_combo (o);
  guitreatevent.save_edit_undo_context (ref edit);
}

//--------------------------------------------------------------------

public void wedit_insert_text (wstring text, CONTROL_ID id)
{
  ref CONTROL_INFO o    = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO    edit = *fetch_edit_or_combo (o);
  ref EDIT_LINE    e    = edit.cr.edit_line;

  // insert text at cursor col, set marks around inserted text, put cursor at end of insertion
  // returns 0 if OK, 1 if text too large
  edit_line_insert_text_block (ref e, text[0 : wstrlen(text)]);

  {
    LINE_MARK m;
    clear m;
    edit_line_set_first_mark (ref e, m);
    edit_line_set_last_mark (ref e, m);
  }

  edit.line_marking_in_progress = false;
  edit.line_marking_start_col = 0;

  update_scroll_x_edit_or_combo (ref o, ref edit, o.x_size);
  repaint_control (o);
}

//--------------------------------------------------------------------

public void edit_insert_text (string text, CONTROL_ID id)
{
  wstring^ w = zstr (text);
  wedit_insert_text (w^, id);
  free w;
}

//--------------------------------------------------------------------

public void edit_set_cursor (int cursor_x, CONTROL_ID id)
{
  ref CONTROL_INFO o    = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO    edit = *fetch_edit_or_combo (o);
  ref EDIT_LINE    e    = edit.cr.edit_line;
  int              limit = edit_line_get_max_length(e) + 1;

  if (cursor_x < 1 || cursor_x > limit)
    fatal_error ("cursor_x %d must be in range %d .. %d", cursor_x, 1, limit);

  edit_line_set_col (ref e, col => cursor_x - 1);

  repaint_control (o);
}

//--------------------------------------------------------------------

// returns cursor position within edit field (range 1 to length+1)

public int edit_get_cursor (CONTROL_ID id)
{
  ref CONTROL_INFO o    = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO    edit = *fetch_edit_or_combo (o);
  ref EDIT_LINE    e    = edit.cr.edit_line;

  return 1 + edit_line_get_col (e);
}

//--------------------------------------------------------------------

public void edit_set_password_mode (CONTROL_ID id)
{
  ref CONTROL_INFO o    = load_control_of_typ2 (id, TYP_EDIT, TYP_COMBO) ^;
  ref EDIT_INFO    edit = *fetch_edit_or_combo (o);

  edit.password_mode = true;
  repaint_control (o);
}

//--------------------------------------------------------------------

public void create_listbox (int x, int y,
                            int x_size, int y_size,
                            int arrow_box_width, int arrow_box_height,
                            int length,
                            CONTROL_ID id)
{
  CONTROL_INFO^ p = new CONTROL_INFO (TYP_LISTBOX);
  ref CONTROL_INFO o = p^;

  check_in_dialog_handler ();

  if (y_size < 4 + ui_unscale(g_dialog_ptr->default_font.height))
    fatal_error ("y_size (=%d) must >= 4 + font.height (=%d)", y_size, 4+ui_unscale(g_dialog_ptr->default_font.height));

  if (arrow_box_width < 4 || arrow_box_width >= x_size-4)
    fatal_error ("arrow_box_width (=%d) must be in range %d .. %d", arrow_box_width, 4, x_size-5);

  if (arrow_box_height < 4 || arrow_box_height >= y_size-4)
    fatal_error ("arrow_box_height (=%d) must be in range %d .. %d", arrow_box_height, 4, y_size-5);

  if (length < 1 || length > 512)
    fatal_error ("length (=%d) must be in range 1 .. 512", length);

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;


  {
    ref LISTBOX_INFO l = o.listbox;

    wcreat_text (out l.wtext);
    l.page                 = 1;
//    l.ln                   = 0;
    l.nb_screen_lines      = (o.y_size - ui_scale(4)) / o.font.height;
    l.line_length          = length;
    l.line_height          = o.font.height;
    l.scrollbar            = 1;
    l.arrow_box_width      = ui_scale(arrow_box_width);
    l.arrow_box_height     = ui_scale(arrow_box_height);
    l.allow_user_selection = true;   // user can select lines
    l.allow_hotkey_search  = true;   // user can use hotkey
    l.multiple_mode        = false;  // single mode
    l.selected_line        = 0;      // no line selected
    l.show_focus           = false;
  }

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4) + ui_scale(arrow_box_width),
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

public void create_tree (int x, int y, int x_size, int y_size,
                         int arrow_box_width, int arrow_box_height,
                         int length, CONTROL_ID id)
{
  CONTROL_INFO^ p = new CONTROL_INFO (TYP_TREE);
  ref CONTROL_INFO o = p^;

  check_in_dialog_handler ();

  if (y_size < 4 + ui_unscale(g_dialog_ptr->default_font.height))
    fatal_error ("y_size (=%d) must >= 4 + font.height (=%d)", y_size, 4+ui_unscale(g_dialog_ptr->default_font.height));

  if (arrow_box_width < 4 || arrow_box_width >= x_size-4)
    fatal_error ("arrow_box_width (=%d) must be in range %d .. %d", arrow_box_width, 4, x_size-5);

  if (arrow_box_height < 4 || arrow_box_height >= y_size-4)
    fatal_error ("arrow_box_height (=%d) must be in range %d .. %d", arrow_box_height, 4, y_size-5);

  if (length < 1 || length > 512)
    fatal_error ("length (=%d) must be in range 1 .. 512", length);

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;


  {
    ref LISTBOX_INFO l = o.tree.listbox;

    wcreat_text (out l.wtext);
    l.page                 = 1;
//    l.ln                   = 0;
    l.nb_screen_lines      = (o.y_size - ui_scale(4)) / o.font.height;
    l.line_length          = length;
    l.line_height          = o.font.height;
    l.scrollbar            = 1;
    l.arrow_box_width      = ui_scale(arrow_box_width);
    l.arrow_box_height     = ui_scale(arrow_box_height);
    l.allow_user_selection = true;   // user can select lines
    l.allow_hotkey_search  = true;   // user can use hotkey
    l.multiple_mode        = false;  // single mode
    l.selected_line        = 0;      // no line selected
    l.show_focus           = false;
  }

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4) + ui_scale(arrow_box_width),
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

LISTBOX_INFO* fetch_listbox_tree_or_combo (CONTROL_INFO o)
{
  if (o.typ == TYP_LISTBOX)
    return &o.listbox;
  else if (o.typ == TYP_TREE)
    return &o.tree.listbox;
  else
    return &o.combo.listbox;
}

//--------------------------------------------------------------------

public void wlistbox_insert (int             index,    // 0 = append to end
                             wstring         text,
                             CONTROL_ID      id,
                             ITEM_ATTRIBUTES attr = DEFAULT_ATTRIBUTES)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);
  int      length;
  wstring^ line;
  int      index2;

  if (index == 0)
    index2 = wnb_text_lines (listbox.wtext) + 1;
  else
    index2 = index;

  if (index2 < 1 || index2 > wnb_text_lines (listbox.wtext) + 1)
    fatal_error ("index is out of range");

  length = wstrlen(text);
  if (length > listbox.line_length)
    fatal_error ("text is too long");

  // build listbox line
  line = new wchar [PREFIX_WLEN + length];

  ((PREFIX*)&line^)->reference = attr.reference;
  ((PREFIX*)&line^)->is_folder = attr.is_folder;
  ((PREFIX*)&line^)->level     = attr.level;
  ((PREFIX*)&line^)->type      = attr.type;

  line^[PREFIX_WLEN:length] = text[0:length];

  winsert_text_line (ref listbox.wtext, index2, line^);


  // update the control

  if (index2 <= listbox.ln)
  {
//    listbox.page++;
    listbox.ln++;
  }

  if (listbox.ln == 0)
    listbox.ln = 1;       // current position is now defined

  if (listbox.selected_line != 0)
  {
    if (index2 <= listbox.selected_line)
      listbox.selected_line++;
  }

  free line;

  repaint_control (o);
}

//--------------------------------------------------------------------

public void listbox_insert (int             index,    // 0 = append to end
                            string          text,
                            CONTROL_ID      id,
                            ITEM_ATTRIBUTES attr = DEFAULT_ATTRIBUTES)
{
  wstring^ w = zstr (text);
  wlistbox_insert (index, w^, id, attr);
  free w;
}

//--------------------------------------------------------------------

void _listbox_update (int          index,
                      wstring      text,
                      bool         keep_old_reference,
                      int8         reference,
                      CONTROL_ID   id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);
  int       length, dummy;
  wstring^  line;

  if (index < 1 || index > wnb_text_lines (listbox.wtext))
    fatal_error ("index is out of range");

  length = wstrlen(text);
  if (length > listbox.line_length)
    fatal_error ("text is too long");

  // build listbox line
  line = new wchar [PREFIX_WLEN + length];

  wretrieve_text_line (ref listbox.wtext, index, out line^[0 : PREFIX_WLEN], out dummy);
  _unused dummy;

  if (!keep_old_reference)
    ((PREFIX*)&line^)->reference = reference;  // store reference
  line^[PREFIX_WLEN:length] = text[0:length];

  wupdate_text_line (ref listbox.wtext, index, line^[0 : PREFIX_WLEN + length]);

  free line;

  repaint_control (o);
}

//--------------------------------------------------------------------

public void wlistbox_update2 (int index, wstring text, int8 reference, CONTROL_ID id)
{
  _listbox_update (index, text, false, reference, id);
}

//--------------------------------------------------------------------

public void listbox_update2 (int index, string text, int8 reference, CONTROL_ID id)
{
  wstring^ w = zstr (text);
  wlistbox_update2 (index, w^, reference, id);
  free w;
}

//--------------------------------------------------------------------

public void wlistbox_update (int index, wstring text, CONTROL_ID id)
{
  _listbox_update (index, text, true, 0, id);
}

//--------------------------------------------------------------------

public void listbox_update (int index, string text, CONTROL_ID id)
{
  wstring^ w = zstr (text);
  wlistbox_update (index, w^, id);
  free w;
}

//--------------------------------------------------------------------

public void wlistbox_retrieve2 (    int             index,
                                out wstring         text,
                                out ITEM_ATTRIBUTES attr,
                                    CONTROL_ID      id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);
  int       length;
  wstring^  line;

  if (text'length < listbox.line_length)
    fatal_error ("text buffer is too small");

  if (index < 1 || index > wnb_text_lines (listbox.wtext))
    fatal_error ("index is out of range");

  // build listbox line
  line = new wchar [PREFIX_WLEN + listbox.line_length];

  wretrieve_text_line (ref listbox.wtext, index, out line^, out length);

  clear attr;
  attr.reference = ((PREFIX*)&line^)->reference;
  attr.is_folder = ((PREFIX*)&line^)->is_folder;
  attr.level     = ((PREFIX*)&line^)->level;
  attr.type      = ((PREFIX*)&line^)->type;

  wstrcpy (out text, line^[PREFIX_WLEN : length-PREFIX_WLEN]);

  free line;
}

//--------------------------------------------------------------------

public void listbox_retrieve2 (    int             index,
                               out string          text,
                               out ITEM_ATTRIBUTES attr,
                                   CONTROL_ID      id)
{
  wstring^ w = new wstring (text'length);
  wlistbox_retrieve2 (index, out w^, out attr, id);
  copy_wstring_string (w^, out text);
  free w;
}

//--------------------------------------------------------------------

public void wlistbox_retrieve (int index, out wstring text, CONTROL_ID id)
{
  ITEM_ATTRIBUTES attr;
  wlistbox_retrieve2 (index, out text, out attr, id);
  _unused attr;
}

//--------------------------------------------------------------------

public void listbox_retrieve (int index, out string text, CONTROL_ID id)
{
  ITEM_ATTRIBUTES attr;
  listbox_retrieve2 (index, out text, out attr, id);
  _unused attr;
}

//--------------------------------------------------------------------

public void listbox_delete (int index, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  if (index < 1 || index > wnb_text_lines (listbox.wtext))
    fatal_error ("index is out of range");

  wdelete_text_line (ref listbox.wtext, index);


  // update the control

  if (listbox.ln > index ||
      listbox.ln > wnb_text_lines (listbox.wtext))
  {
    listbox.ln--;    // can become zero
  }

  if (listbox.page > 1 &&
      index < listbox.page + listbox.nb_screen_lines)
  {
    listbox.page--;
  }

  if (listbox.selected_line != 0)
  {
    if (index == listbox.selected_line)
      listbox.selected_line = 0;
    else if (index < listbox.selected_line)
      listbox.selected_line--;
  }

  repaint_control (o);
}

//--------------------------------------------------------------------

public int listbox_count (CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  return wnb_text_lines (listbox.wtext);
}

//--------------------------------------------------------------------

public int listbox_cursor (CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  return listbox.ln;
}

//--------------------------------------------------------------------

public void listbox_set_cursor (int index, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);
  int new_page;

  if (index < 1 || index > wnb_text_lines (listbox.wtext))
    fatal_error ("index is out of range");

  // update the control

  if (index == listbox.ln)    // nothing to do
    return;

  listbox.ln = index;

  new_page = listbox.page;
  if (index < new_page)
    new_page = index;
  else if (index > new_page + (listbox.nb_screen_lines - 1))
  {
    new_page = index - (listbox.nb_screen_lines - 1);
    if (new_page < 1)
      new_page = 1;
  }

  listbox.page = new_page;

  repaint_control (o);
}

//--------------------------------------------------------------------

public void listbox_allow_user_selection (bool allow, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  listbox.allow_user_selection = allow;
}

//--------------------------------------------------------------------

public void listbox_allow_hotkey_search (bool allow, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  listbox.allow_hotkey_search = allow;
}

//--------------------------------------------------------------------

public void listbox_set_multiselection (CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ2 (id, TYP_LISTBOX, TYP_TREE) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  if (listbox.multiple_mode)
    fatal_error ("listbox is already in multiselection mode");

  listbox.multiple_mode = true;
  listbox.selected_line = 0;

  repaint_control (o);
}

//--------------------------------------------------------------------

public int listbox_selected_line (CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  if (listbox.multiple_mode)
    fatal_error ("this function is not allowed in multiselection mode");

  return listbox.selected_line;
}

//--------------------------------------------------------------------

public bool listbox_line_is_selected (int index, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  if (index < 1 || index > wnb_text_lines (listbox.wtext))
    fatal_error ("index is out of range");

  if (!listbox.multiple_mode)
  {
    // single mode
    return (index == listbox.selected_line);
  }
  else
  {
    // multiple mode
    wchar line[PREFIX_WLEN];
    int   dummy;

    wretrieve_text_line (ref listbox.wtext, index, out line, out dummy);
    _unused dummy;

    return ((PREFIX*)&line)->selected;
  }
}

//--------------------------------------------------------------------

public void listbox_select_line (int index, bool select, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);

  if (index < 1 || index > wnb_text_lines (listbox.wtext))
    fatal_error ("index is out of range");

  if (!listbox.multiple_mode)      // single mode
  {
    if (select)
      listbox.selected_line = index;
    else if (listbox.selected_line == index)
      listbox.selected_line = 0;
  }
  else           // multiple mode
  {
    int      length;
    wstring^ line;

    line = new wchar [PREFIX_WLEN + listbox.line_length];

    wretrieve_text_line (ref listbox.wtext, index, out line^, out length);

    ((PREFIX*)&line^)->selected = select;

    wupdate_text_line (ref listbox.wtext, index, line^[0:length]);

    free line;
  }

  repaint_control (o);
}

//--------------------------------------------------------------------

public int listbox_top_displayed_line (CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);
  return listbox.page;
}

//--------------------------------------------------------------------

package K = new WUSER_SORT_TEXT_LINES (USER_INFO => COMPARE_LISTBOX_LINES);

//--------------------------------------------------------------------

int intern_compare (wstring^ data1, wstring^ data2, ref COMPARE_LISTBOX_LINES compare_listbox_lines)
{
  ref wstring a = data1^;
  ref wstring b = data2^;

  return compare_listbox_lines
      (((PREFIX*)&a)->reference, ((PREFIX*)&a)->selected, a[PREFIX_WLEN : a'length-PREFIX_WLEN],
       ((PREFIX*)&b)->reference, ((PREFIX*)&b)->selected, b[PREFIX_WLEN : b'length-PREFIX_WLEN]);
}

//------------------------------------------------------------------

// sort listbox lines using user-defined comparison function
public void listbox_sort_user (CONTROL_ID id, COMPARE_LISTBOX_LINES user_compare)
{
  ref CONTROL_INFO  o = load_control_of_typ3 (id, TYP_LISTBOX, TYP_TREE, TYP_COMBO) ^;
  ref LISTBOX_INFO listbox = *fetch_listbox_tree_or_combo (o);
  COMPARE_LISTBOX_LINES ref_user_compare = user_compare;

  // for single-line selection, store flag in selected line.
  if ((!listbox.multiple_mode) && listbox.selected_line != 0)
  {
    wstring^ line;
    int      length;

    // build listbox line
    line = new wchar [PREFIX_WLEN + listbox.line_length];

    wretrieve_text_line (ref listbox.wtext, listbox.selected_line, out line^, out length);

    ((PREFIX*)&line^)->selected = true;   // line is selected

    wupdate_text_line (ref listbox.wtext, listbox.selected_line, line^[0:length]);
    free line;
  }

  K.sort (ref listbox.wtext, intern_compare, ref ref_user_compare);

  // for single-line selection, restore selected line flag.
  if ((!listbox.multiple_mode) && listbox.selected_line != 0)
  {
    wstring^ line;
    int      count, length, ln;

    // build listbox line
    line = new wchar [PREFIX_WLEN + listbox.line_length];

    count = wnb_text_lines (listbox.wtext);

    for (ln=1; ln<=count; ln++)
    {
      wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

      if (((PREFIX*)&line^)->selected)   // was selected
      {
        ((PREFIX*)&line^)->selected = false;   // delete again flag
        wupdate_text_line (ref listbox.wtext, ln, line^[0:length]);
        listbox.selected_line = ln;
        break;
      }
    }

    free line;
  }

  repaint_control (o);
}

//--------------------------------------------------------------------

int uppercase_extended_compare
               (int8 reference1, bool selected1, wstring line1,
                int8 reference2, bool selected2, wstring line2)
{
  wstring^ t1, t2;
  int      rc;

  _unused reference1;
  _unused reference2;
  _unused selected1;
  _unused selected2;

  t1 = new wchar [wstrlen(line1)];
  t2 = new wchar [wstrlen(line2)];

  wnormalize_extended_characters (line1, out t1^);
  wnormalize_extended_characters (line2, out t2^);

  rc = wstricmp (t1^, t2^);

  free t1;
  free t2;

  return rc;
}

//--------------------------------------------------------------------

public void listbox_sort (CONTROL_ID id)
{
  listbox_sort_user (id, uppercase_extended_compare);
}

//--------------------------------------------------------------------

public void signal_drop_allowed ()
{
  g_user_drop_allowed = true;
}

//--------------------------------------------------------------------

public void create_horizontal_scrollbar (int x, int y, int x_size, int y_size,
                                         int arrow_box_width, CONTROL_ID id)
{
  CONTROL_INFO^ p = new CONTROL_INFO (TYP_SCROLL);
  ref CONTROL_INFO o = p^;

  check_in_dialog_handler ();

  if (arrow_box_width < 4 || arrow_box_width >= x_size/2)
    fatal_error ("arrow_box_width (=%d) must be in range %d .. %d", arrow_box_width, 4, x_size/2-1);

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;

  {
    ref SCROLL_INFO l = o.scroll;

    l.tscroll = HSCROLL;
    l.page  = 1;
    l.range = 100;
    l.shown = 10;
    l.arrow_box_width_or_height = ui_scale(arrow_box_width);
    l.small_y_increment = 1;
  }

  check_append_and_repaint_control (
      p,
      min_x_size => 0,
      min_y_size => 0);
}

//--------------------------------------------------------------------

public void create_vertical_scrollbar (int x, int y, int x_size, int y_size,
                                       int arrow_box_height, CONTROL_ID id)
{
  CONTROL_INFO^ p = new CONTROL_INFO (TYP_SCROLL);
  ref CONTROL_INFO o = p^;

  check_in_dialog_handler ();

  if (arrow_box_height < 4 || arrow_box_height >= y_size/2)
    fatal_error ("arrow_box_height (=%d) must be in range %d .. %d", arrow_box_height, 4, y_size/2-1);

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;

  {
    ref SCROLL_INFO l = o.scroll;

    l.tscroll = VSCROLL;
    l.page  = 1;
    l.range = 100;
    l.shown = 10;
    l.arrow_box_width_or_height = ui_scale(arrow_box_height);
    l.small_y_increment = 1;
  }

  check_append_and_repaint_control (
      p,
      min_x_size => 0,
      min_y_size => 0);
}

//--------------------------------------------------------------------

public void horizontal_scrollbar_set_arrow_box_width (int arrow_box_width, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ (id, TYP_SCROLL) ^;
  repaint_control (o);
  assert o.scroll.tscroll == HSCROLL;
  o.scroll.arrow_box_width_or_height = ui_scale(arrow_box_width);
  repaint_control (o);
}

//--------------------------------------------------------------------

public void vertical_scrollbar_set_arrow_box_height (int arrow_box_height, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ (id, TYP_SCROLL) ^;
  repaint_control (o);
  assert o.scroll.tscroll == VSCROLL;
  o.scroll.arrow_box_width_or_height = ui_scale(arrow_box_height);
  repaint_control (o);
}

//--------------------------------------------------------------------

public void scrollbar_set_range (int full_size, int shown_size, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ (id, TYP_SCROLL) ^;

  o.scroll.range = full_size;
  o.scroll.shown = shown_size;

  repaint_control (o);
}

//--------------------------------------------------------------------

public void scrollbar_set_position (int pos, CONTROL_ID id)  // set top shown position
{
  ref CONTROL_INFO o        = load_control_of_typ (id, TYP_SCROLL) ^;
  ref SCROLL_INFO  scroll   = o.scroll;
  int              position = pos;

  if (position > scroll.range - scroll.shown)
    position = scroll.range - scroll.shown;
  if (position < 0)
    position = 0;

  o.scroll.page = 1 + pos;

  repaint_control (o);
}

//--------------------------------------------------------------------

public int scrollbar_position (CONTROL_ID id)  // get top shown position
{
  ref CONTROL_INFO o        = load_control_of_typ (id, TYP_SCROLL) ^;
  ref SCROLL_INFO  scroll   = o.scroll;
  int              position = scroll.page - 1;

  if (position > scroll.range - scroll.shown)
    position = scroll.range - scroll.shown;
  if (position < 0)
    position = 0;

  return position;
}

//--------------------------------------------------------------------

public void scrollbar_set_line_position_increment (int small_y_increment, CONTROL_ID id)
{
  ref CONTROL_INFO  o = load_control_of_typ (id, TYP_SCROLL) ^;
  o.scroll.small_y_increment = small_y_increment;
}

//--------------------------------------------------------------------

public void create_window (int x, int y, int x_size, int y_size, CONTROL_ID id)
{
  CONTROL_INFO^                 p = new CONTROL_INFO (TYP_WINDOW);
  ref CONTROL_INFO (TYP_WINDOW) o = p^;

  check_in_dialog_handler ();
  if (x_size < 1 || y_size < 1)
    fatal_error ("x_size/y_size is too small");

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;
  o.window.refresh = true;

#if WINDOWS
  {
    HANDLE hdc, hbitmap, hdcMemory;
    hdc = GetDC (g_dialog_ptr->hwnd);
    hbitmap = CreateCompatibleBitmap (hdc, o.x_size, o.y_size);
    assert hbitmap != 0;
    hdcMemory = CreateCompatibleDC (hdc);
    assert hdcMemory != 0;
    ReleaseDC (g_dialog_ptr->hwnd, hdc);

    o.window.hbitmap   = hbitmap;
    o.window.hdcMemory = hdcMemory;
    o.window.old_hbitmap = SelectObject (hdcMemory, hbitmap);
  }

#elif ANDROID
  {
    int   i, count;
    uint* ptr;
    
    o.window.hdcMemory.image  = new byte[4 * o.x_size * o.y_size];
    
    count = o.x_size * o.y_size;
    ptr = (uint*)&o.window.hdcMemory.image^;
    for (i=0; i<count; i++)
      *ptr++ = 0xFF000000;
  
    o.window.hdcMemory.width  = (uint)o.x_size;
    o.window.hdcMemory.height = (uint)o.y_size;
    o.window.hdcMemory.rect   = {left   => 0,
                                 top    => 0,
                                 right  => o.x_size,
                                 bottom => o.y_size};
  }

#else
  bad
#endif

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(1),
      min_y_size => ui_scale(1));
}

//--------------------------------------------------------------------

public void window_set_refresh_mode (bool flag, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;
  ref WINDOW_INFO w = o.window;
  w.refresh = flag;
}

//--------------------------------------------------------------------

public void window_set_color (uint rgb, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;
  o.font.color = rgb & 0xFFFFFF;
}

//--------------------------------------------------------------------

public void window_pixel (int x, int y, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;

#if WINDOWS
  ref WINDOW_INFO w = o.window;

  if (g_scale == 1)
    SetPixelV (w.hdcMemory, x, y, o.font.color);
  else
  {
    int scaled_x = ui_scale (x);
    int scaled_y = ui_scale (y);
    RECT   rect = {left   => scaled_x,
                   top    => scaled_y,
                   right  => scaled_x + g_scale,
                   bottom => scaled_y + g_scale};
    HBRUSH hbrush = CreateSolidBrush (o.font.color);
    FillRect (w.hdcMemory, &rect, hbrush);
    DeleteObject (hbrush);
  }

#elif ANDROID
  ref WINDOW_INFO w = o.window;
  int scaled_x = ui_scale (x);
  int scaled_y = ui_scale (y);
  RECT   rect = {left   => scaled_x,
                 top    => scaled_y,
                 right  => scaled_x + g_scale,
                 bottom => scaled_y + g_scale};
  HBRUSH hbrush = CreateSolidBrush (o.font.color);

  FillRect (w.hdcMemory, &rect, hbrush);

#else
  bad
#endif

  repaint_control (o);
}

//--------------------------------------------------------------------

public void window_line (int x1, int y1, int x2, int y2, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;

#if WINDOWS
  ref WINDOW_INFO w = o.window;
  HANDLE hpen, oldhpen;
  POINT pt[2];

  int scaled_x1 = ui_scale (x1);
  int scaled_y1 = ui_scale (y1);
  int scaled_x2 = ui_scale (x2);
  int scaled_y2 = ui_scale (y2);
  int x, y;

  hpen = CreatePen (PS_SOLID, g_scale, o.font.color);
  oldhpen = SelectObject (w.hdcMemory, hpen);

  for (y=0; y<g_scale; y++)
  {
    for (x=0; x<g_scale; x++)
    {
      pt[0].x = scaled_x1 + x;
      pt[0].y = scaled_y1 + y;
      pt[1].x = scaled_x2 + x;
      pt[1].y = scaled_y2 + y;
      Polyline (w.hdcMemory, &pt, 2);
    }
  }

  if (g_scale == 1)
    SetPixelV (w.hdcMemory, scaled_x2, scaled_y2, o.font.color);
  else
  {
    RECT rect = {left   => scaled_x2,
                 top    => scaled_y2,
                 right  => scaled_x2 + g_scale,
                 bottom => scaled_y2 + g_scale};
    HBRUSH hbrush = CreateSolidBrush (o.font.color);
    FillRect (w.hdcMemory, &rect, hbrush);
    DeleteObject (hbrush);
  }

  SelectObject (w.hdcMemory, oldhpen);
  DeleteObject (hpen);

#elif ANDROID
  // $
  _unused x1, y1, x2, y2;
#else
  bad
#endif

  repaint_control (o);
}

//--------------------------------------------------------------------

public void window_box (int x1, int y1, int x2, int y2, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;

  int scaled_x1 = ui_scale (x1);
  int scaled_y1 = ui_scale (y1);
  int scaled_x2 = ui_scale (x2);
  int scaled_y2 = ui_scale (y2);

  RECT rect = {left   => scaled_x1,
               top    => scaled_y1,
               right  => scaled_x2 + g_scale,
               bottom => scaled_y2 + g_scale};

  ref WINDOW_INFO w = o.window;
  HBRUSH hbrush = CreateSolidBrush (o.font.color);

#if WINDOWS
  FillRect (w.hdcMemory, &rect, hbrush);
  DeleteObject (hbrush);

#elif ANDROID
  FillRect (w.hdcMemory, &rect, hbrush);

#else
  bad

#endif

  repaint_control_rect (o, rect);
}

//--------------------------------------------------------------------

// x,y is bottom left corner

public void window_wtext (int x, int y, wstring text, CONTROL_ID id)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;

#if WINDOWS
  ref WINDOW_INFO w = o.window;
  HFONT    old_font, new_font;
  COLORREF old_color;
  int      old_back_mode;

  new_font = get_cached_font (o.font);
  old_font = SelectObject (w.hdcMemory, new_font);
  old_color = SetTextColor (w.hdcMemory, o.font.color);
  old_back_mode = SetBkMode (w.hdcMemory, TRANSPARENT);

  TextOutW (w.hdcMemory, ui_scale(x), ui_scale(y), &text, wstrlen(text));

  SetBkMode    (w.hdcMemory, old_back_mode);
  SetTextColor (w.hdcMemory, old_color);
  SelectObject (w.hdcMemory, old_font);

#elif ANDROID
  int scaled_x      = ui_scale (x);
  int scaled_y      = ui_scale (y);
  ref WINDOW_INFO w = o.window;
  COLORREF old_color;

  HFONT newfont = gdi_make_font (o.font);
  HFONT oldfont = SelectFont (w.hdcMemory, newfont);
  old_color = SetTextColor (w.hdcMemory, o.font.color);

  ExtTextOutW  (hdc    => w.hdcMemory,
                x_left => scaled_x,
                y_top  => scaled_y - o.font.height,
                flags  => 0,
                rect   => null,
                text   => &text,
                text_length => (uint)wstrlen(text),
                ptr    => null);

  SetTextColor (w.hdcMemory, old_color);
  SelectFont (w.hdcMemory, oldfont);

#else
  bad

#endif

  repaint_control (o);
}

//--------------------------------------------------------------------

public void window_text (int x, int y, string text, CONTROL_ID id)
{
  wstring^ w = zstr (text);
  window_wtext (x, y, w^, id);
  free w;
}

//--------------------------------------------------------------------

public void window_wtext_opaque (int x, int y, int width, int height, wstring text,
                                 uint background_color, CONTROL_ID id, int offset_x = 0, int offset_y = 0)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;

  int scaled_x      = ui_scale (x);
  int scaled_y      = ui_scale (y);
  int scaled_width  = ui_scale (width);
  int scaled_height = ui_scale (height);

  RECT rect = {scaled_x, scaled_y, scaled_x+scaled_width, scaled_y+scaled_height};

#if WINDOWS
  {
    ref WINDOW_INFO w = o.window;
    HFONT    old_font, new_font;
    COLORREF old_color, old_back_color;

    new_font = get_cached_font (o.font);
    old_font = SelectObject (w.hdcMemory, new_font);
    old_color = SetTextColor (w.hdcMemory, o.font.color);
    old_back_color = SetBkColor (w.hdcMemory, background_color & 0xFFFFFF);

    ExtTextOutW (w.hdcMemory,
                 scaled_x + ui_scale(offset_x),
                 scaled_y + (scaled_height - o.font.height) + ui_scale(offset_y),
                 ETO_CLIPPED | ETO_OPAQUE,
                 &rect,
                 &text,
                 (uint)wstrlen(text),
                 null);

    SetBkColor   (w.hdcMemory, old_back_color);
    SetTextColor (w.hdcMemory, old_color);
    SelectObject (w.hdcMemory, old_font);
  }

#elif ANDROID
  ref WINDOW_INFO w = o.window;
  COLORREF old_color, old_back_color;

  HFONT newfont = gdi_make_font (o.font);
  HFONT oldfont = SelectFont (w.hdcMemory, newfont);

  old_color = SetTextColor (w.hdcMemory, o.font.color);
  old_back_color = SetBkColor (w.hdcMemory, background_color & 0xFFFFFF);

  ExtTextOutW  (w.hdcMemory,
                 scaled_x + ui_scale(offset_x),
                 scaled_y + (scaled_height - o.font.height) + ui_scale(offset_y),
                 ETO_CLIPPED | ETO_OPAQUE,
                 &rect,
                 &text,
                 (uint)wstrlen(text),
                 null);

    SetBkColor   (w.hdcMemory, old_back_color);
    SetTextColor (w.hdcMemory, old_color);
    SelectFont (w.hdcMemory, oldfont);

#else
  bad

#endif

  repaint_control_rect (o, rect);
}

//--------------------------------------------------------------------

public void window_text_opaque (int x, int y, int width, int height, string text,
                                 uint background_color, CONTROL_ID id, int offset_x = 0, int offset_y = 0)
{
  wstring^ w = zstr (text);
  window_wtext_opaque (x, y, width, height, w^, background_color, id, offset_x, offset_y);
  free w;
}

//--------------------------------------------------------------------

#if ANDROID
// replace clipped rectangle in target with new source image

int replace_image_rectangle2 (byte[]     source_image_pixel,
                              uint       source_image_width,
                              uint       source_image_height,
                              CLIP_INFO  source_clip,
                              IMAGE_INFO target_image,
                              CLIP_INFO  target_clip)
{                              
  uint y, ofs1, ofs2, len, stride1, stride2;
  int  tofs_x, tofs_y, tsize_x, tsize_y, sofs_x, sofs_y;

  if (source_image_pixel'size != 4 * source_image_width * source_image_height ||
      target_image.pixel^'size != 4 * target_image.width * target_image.height)
  {
    return -1;
  }

  if (target_clip.size_x != source_clip.size_x ||
      target_clip.size_y != source_clip.size_y)
  {
    return -1;
  }

  if (source_clip.size_x   > source_image_width ||
      source_clip.offset_x < 0 ||
      source_clip.offset_x > (int)(source_image_width - source_clip.size_x) ||
      source_clip.size_y   > source_image_height ||
      source_clip.offset_y < 0 ||
      source_clip.offset_y > (int)(source_image_height - source_clip.size_y))
  {
    return -1;
  }

  if (target_clip.offset_x + (int)target_clip.size_x <= 0 ||
      target_clip.offset_y + (int)target_clip.size_y <= 0 ||
      target_clip.offset_x >= (int)target_image.width ||
      target_clip.offset_y >= (int)target_image.height)
  {
    return 0;   // target clip is fully outside target image : nothing to do
  }


  if (target_clip.offset_x >= 0)  // in range
  {
    sofs_x  = source_clip.offset_x;
    tofs_x  = target_clip.offset_x;
    tsize_x = (int)target_clip.size_x;
  }
  else    // x cut at start
  {
    sofs_x  = source_clip.offset_x - target_clip.offset_x;
    tofs_x  = 0;
    tsize_x = (int)target_clip.size_x + target_clip.offset_x;
  }

  if (tofs_x + tsize_x > (int)target_image.width)   // x cut at end
  {
    tsize_x = (int)target_image.width - tofs_x;
  }


  if (target_clip.offset_y >= 0)  // in range
  {
    sofs_y  = source_clip.offset_y;
    tofs_y  = target_clip.offset_y;
    tsize_y = (int)target_clip.size_y;
  }
  else    // y cut at start
  {
    sofs_y  = source_clip.offset_y - target_clip.offset_y;
    tofs_y = 0;
    tsize_y = (int)target_clip.size_y + target_clip.offset_y;
  }

  if (tofs_y + tsize_y > (int)target_image.height)   // y cut at end
  {
    tsize_y = (int)target_image.height - tofs_y;
  }


  ofs1 = 4 * ((uint)sofs_y * source_image_width + (uint)sofs_x);
  ofs2 = 4 * ((uint)tofs_y * target_image.width + (uint)tofs_x);
  len  = 4 * (uint)tsize_x;
  stride1 = 4 * source_image_width;
  stride2 = 4 * target_image.width;

  for (y=0; y<(uint)tsize_y; y++)
  {
    target_image.pixel^[ofs2:len] = source_image_pixel[ofs1:len];
    ofs1 += stride1;
    ofs2 += stride2;
  }

  return 0;
}
#endif

/**************************************************************************/

public void window_raster (int x, int y, int size_x, int size_y, byte[] image, CONTROL_ID id,
                           bool immediate = false, bool scaled = true)
{
  ref CONTROL_INFO o = load_control_of_typ (id, TYP_WINDOW) ^;
  int              scaled_x, scaled_y, scaled_size_x, scaled_size_y;

  if (size_x <= 0 || size_y <= 0)
    return;

  if (scaled)   // we need to apply a scale to the coordinates and the size
  {
    scaled_x      = ui_scale (x);
    scaled_y      = ui_scale (y);
    scaled_size_x = ui_scale (size_x);
    scaled_size_y = ui_scale (size_y);
  }
  else    // we use the raw parameters
  {
    scaled_x      = x;
    scaled_y      = y;
    scaled_size_x = size_x;
    scaled_size_y = size_y;
  }

#if WINDOWS
  {
    HANDLE hbitmapPict;

    {
      packed struct INFO
      {
        BITMAPINFOHEADER bmiHeader;
        DWORD            bmiColors[3];
      }

      INFO info;
      byte *bits;

      clear info;
      info.bmiHeader.biSize        = BITMAPINFOHEADER'size;
      info.bmiHeader.biWidth       = size_x;
      info.bmiHeader.biHeight      = -size_y;
      info.bmiHeader.biPlanes      = 1;
      info.bmiHeader.biBitCount    = 32;
      info.bmiHeader.biCompression = BI_BITFIELDS;
      info.bmiHeader.biSizeImage   = (uint)(4 * size_x * size_y);
      info.bmiColors = {0xFF, 0xFF00, 0xFF0000};

      hbitmapPict = CreateDIBSection (0, (byte *)&info, DIB_RGB_COLORS, &bits, 0, 0);
      assert hbitmapPict != 0;

      bits[0:info.bmiHeader.biSizeImage] = image;
    }

    {
      ref WINDOW_INFO  w = o.window;
      HANDLE           hdc, hbitmapPictOld;

      hdc = get_cached_hdc();

      hbitmapPictOld = SelectObject (hdc, hbitmapPict);

      if (scaled_size_x == size_x)
      {
        BitBlt (hdcDest => w.hdcMemory,
                nXDest  => scaled_x,
                nYDest  => scaled_y,
                nWidth  => scaled_size_x,
                nHeight => scaled_size_y,
                hdcSrc  => hdc,
                nXSrc   => 0,
                nYSrc   => 0,
                dwRop   => SRCCOPY);
      }
      else
      {
        StretchBlt (hdcDest => w.hdcMemory,
                    xDest   => scaled_x,
                    yDest   => scaled_y,
                    wDest   => scaled_size_x,
                    hDest   => scaled_size_y,
                    hdcSrc  => hdc,
                    xSrc    => 0,
                    ySrc    => 0,
                    wSrc    => size_x,
                    hSrc    => size_y,
                    rop     => SRCCOPY);
      }

      SelectObject (hdc, hbitmapPictOld);
      assert DeleteObject (hbitmapPict) != 0;
    }
  }

  if (immediate)
  {
    ref DIALOG_INFO d = *(DIALOG_INFO*)g_dialog_ptr;
    HDC             dhdc = GetDC (d.hwnd);
    int             dx, dy;
    int             tx, ty;

    dx = d.border_size;
    dy = d.border_size + d.title_height;

    tx  = dx + o.x;
    ty  = dy + o.y;

    BitBlt (hdcDest => dhdc,
            nXDest  => tx + scaled_x,   // inside window control of dialog
            nYDest  => ty + scaled_y,
            nWidth  => scaled_size_x,
            nHeight => scaled_size_y,
            hdcSrc  => o.window.hdcMemory,
            nXSrc   => scaled_x,
            nYSrc   => scaled_y,
            dwRop   => SRCCOPY);

    ReleaseDC (d.hwnd, dhdc);
  }
  else
  {
    repaint_control_rect (o, RECT ' {left   => scaled_x,
                                     top    => scaled_y,
                                     right  => scaled_x + scaled_size_x,
                                     bottom => scaled_y + scaled_size_y});
  }

#elif ANDROID
  {
    ref WINDOW_INFO  w = o.window;

    _unused immediate;

    if (scaled_size_x != size_x)   // scaling
    {
      IMAGE_INFO img = {pixel => new byte[] ' (image), width => (uint)size_x, height => (uint)size_y};
      CLIP_INFO  source_clip, target_clip;
      int        rc;

      rc = resize_image (ref info              => img,
                             width             => (uint)scaled_size_x,
                             height            => (uint)scaled_size_y,
                             border_color      => 0xFF000000,  // opaque black
                             bestfit           => false,
                             use_linear_colors => true);
      assert rc == 0;

      clear source_clip, target_clip;
      source_clip.size_x = (uint)scaled_size_x;
      source_clip.size_y = (uint)scaled_size_y;
      target_clip.offset_x = scaled_x;         // can be negative or larger than size
      target_clip.offset_y = scaled_y;         // can be negative or larger than size
      target_clip.size_x = (uint)scaled_size_x;
      target_clip.size_y = (uint)scaled_size_y;

      rc = replace_image_rectangle (source_image => img,
                                    source_clip  => source_clip,
                                    target_image => {w.hdcMemory.image, (uint)w.hdcMemory.width, (uint)w.hdcMemory.height},
                                    target_clip  => target_clip);
      assert rc == 0;

      free_image (ref img);
    }
    else   // no scaling
    {
      CLIP_INFO  source_clip, target_clip;
      int        rc;

      clear source_clip, target_clip;
      source_clip.size_x = (uint)size_x;
      source_clip.size_y = (uint)size_y;
      target_clip.offset_x = x;         // can be negative or larger than size
      target_clip.offset_y = y;         // can be negative or larger than size
      target_clip.size_x = (uint)size_x;
      target_clip.size_y = (uint)size_y;

      rc = replace_image_rectangle2 (source_image_pixel  => image,
                                     source_image_width  => (uint)size_x,
                                     source_image_height => (uint)size_y,
                                     source_clip         => source_clip,
                                     target_image        => {w.hdcMemory.image, (uint)w.hdcMemory.width, (uint)w.hdcMemory.height},
                                     target_clip         => target_clip);
      assert rc == 0;
    }


    repaint_control_rect (o, RECT ' {left   => scaled_x,
                                     top    => scaled_y,
                                     right  => scaled_x + scaled_size_x,
                                     bottom => scaled_y + scaled_size_y});
  }

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void create_editbox (int x, int y, int x_size, int y_size, int max_line_length, int max_text_lines, CONTROL_ID id)
{
  CONTROL_INFO^                  p = new CONTROL_INFO (TYP_EDITBOX);
  ref CONTROL_INFO (TYP_EDITBOX) o = p^;

  check_in_dialog_handler ();

  if (max_line_length < 1)
    fatal_error ("max_line_length must be >= 1");
  if (max_text_lines < 1)
    fatal_error ("max_text_lines must be >= 1");

  o.id     = id;
  o.x      = ui_scale(x);
  o.y      = ui_scale(y);
  o.x_size = ui_scale(x_size);
  o.y_size = ui_scale(y_size);
  o.font   = g_dialog_ptr->default_font;
  o.colors = g_dialog_ptr->default_colors;

  recompute_font_width (ref o);

  {
    ref EDITBOX_INFO ed = o.editbox;
    edit_text_allocate (out edit_text       => ed.cur.text,
                            max_line_length => max_line_length,
                            max_text_lines  => max_text_lines,
                            insert_mode     => g_dialog_ptr->default_insert_mode);

    ed.cur.page = 1;
    compute_editbox_page (ref o);
    ed.cur.scroll = 0;
  }

  check_append_and_repaint_control (
      p,
      min_x_size => ui_scale(4),
      min_y_size => ui_scale(4) + o.font.height);
}

//--------------------------------------------------------------------

public void editbox_set_text_was_modified_flag (bool text_was_modified, CONTROL_ID id)
{
  edit_text_set_text_modified (ref load_control_of_typ (id, TYP_EDITBOX) ^. editbox.cur.text, text_was_modified);
}

//--------------------------------------------------------------------

public bool editbox_text_was_modified (CONTROL_ID id)
{
  return edit_text_get_text_modified (load_control_of_typ (id, TYP_EDITBOX) ^. editbox.cur.text);
}

//--------------------------------------------------------------------

public int editbox_text_count_lines (CONTROL_ID id)
{
  return edit_text_count_lines (load_control_of_typ (id, TYP_EDITBOX) ^. editbox.cur.text);
}

//--------------------------------------------------------------------

public void editbox_get_cursor (out int col, out int line, CONTROL_ID id)
{
  ref EDIT_TEXT we = load_control_of_typ (id, TYP_EDITBOX) ^. editbox . cur . text;

  col = edit_text_get_col (we) + 1;
  line = edit_text_get_ln (we);
}

//--------------------------------------------------------------------

public void editbox_set_cursor (int col, int line, int page, CONTROL_ID id)
{
  ref CONTROL_INFO (TYP_EDITBOX) o = load_control_of_typ (id, TYP_EDITBOX) ^;
  ref EDITBOX_INFO               ed = o . editbox;
  ref EDIT_TEXT                  we = ed . cur . text;

  int cl = min(max(0,col-1), edit_text_get_max_line_length(we)-1);
  int ln = min(max(1,line), edit_text_count_lines(we));

  int lines_per_page;

  if (cl == edit_text_get_col(we) && ln == edit_text_get_ln(we))
    return;

  edit_text_set_col (ref we, col => cl);
  edit_text_set_ln (ref we, ln => ln);

  lines_per_page = (o.y_size - ui_scale(4)) / o.font.height;

  ed.cur.page = page;
  if (ed.cur.page < ln - lines_per_page + 1)   // line number of first screen line
    ed.cur.page = ln - lines_per_page + 1;
  if (ed.cur.page < 1)
    ed.cur.page = 1;
  if (ed.cur.page > ln)
    ed.cur.page = ln;

  edit_text_set_redraw_screen_needed (ref we);

  compute_editbox_scroll_field (ref o);
  compute_editbox_page (ref o);

  repaint_control (o);
}

//--------------------------------------------------------------------

public void editbox_clear_text (CONTROL_ID id)
{
  ref CONTROL_INFO (TYP_EDITBOX) o = load_control_of_typ (id, TYP_EDITBOX) ^;
  ref EDITBOX_INFO               ed = o . editbox;
  ref EDIT_TEXT                  we = ed . cur . text;

  edit_text_clear_text (ref we);

  ed.cur.page = 1;
  ed.cur.scroll = 0;

  repaint_control (o);
}

//--------------------------------------------------------------------

// retrieve the full text
// note: the text_was_modified flag is set to false (see editbox_text_was_modified())

public wstring^ editbox_retrieve_full_text (CONTROL_ID id)
{
  ref CONTROL_INFO (TYP_EDITBOX) o = load_control_of_typ (id, TYP_EDITBOX) ^;
  ref EDITBOX_INFO               ed = o . editbox;
  ref EDIT_TEXT                  we = ed . cur . text;

  edit_text_set_text_modified (ref we, text_modified => false);

  return edit_text_get_full_text (ref we);
}

//--------------------------------------------------------------------

// store the full text
// note: the text_was_modified flag is set to false (see editbox_text_was_modified())

public void editbox_store_text (wstring text, CONTROL_ID id)
{
  ref CONTROL_INFO (TYP_EDITBOX) o = load_control_of_typ (id, TYP_EDITBOX) ^;
  ref EDITBOX_INFO               ed = o . editbox;
  ref EDIT_TEXT                  we = ed . cur . text;

  edit_text_set_full_text (ref we, text);

  repaint_control (o);
}

//--------------------------------------------------------------------

public void editbox_get_info (out EDITBOX_INFORMATION editbox_information, CONTROL_ID id)
{
  ref CONTROL_INFO (TYP_EDITBOX) o = load_control_of_typ (id, TYP_EDITBOX) ^;
  ref EDITBOX_INFO               edit = o . editbox;
  ref EDIT_TEXT                  edit_text = edit . cur . text;

  editbox_information = {col    => edit_text_get_col (edit_text),
                         ln     => edit_text_get_ln (edit_text),
                         insert => edit_text_get_insert_mode (edit_text),
                         page   => edit.cur.page,
                         scroll => edit.cur.scroll};
}

//--------------------------------------------------------------------

public void editbox_press_key (int key, CONTROL_ID id)
{
  ref CONTROL_INFO (TYP_EDITBOX) o = load_control_of_typ (id, TYP_EDITBOX) ^;
  ref EDITBOX_INFO               edit = o . editbox;
  bool                           key_processed;

  guitreatevent . process_key_editbox (ref o, ref edit, key, out key_processed);

  _unused key_processed;
}

//--------------------------------------------------------------------

public void create_main_window (APPLICATION_EVENT_HANDLER application_event_handler)
{
#if WINDOWS
  guicb.create_main_window (application_event_handler);
#elif ANDROID
  guiandroid.create_main_window (application_event_handler);
#else
  bad

#endif
}

//---------------------------------------------------------------------

public void close_main_window ()
{
#if WINDOWS
  PostMessageA (main_hWnd, WM_DESTROY, 0, 0);

#elif ANDROID
  {
    APPLICATION_EVENT e;

    clear e;
    e.type = EVENT_END_APPLICATION;
    app_handler (e);
  }

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void show_window ()
{
#if WINDOWS
  const string          dll_name  = "Dwmapi.dll\0";
  const string          func_name = "DwmSetWindowAttribute\0";
  HMODULE               hinstLib;
  LPVOID                func;
  DWMSETWINDOWATTRIBUTE DwmSetWindowAttribute = null;

  hinstLib = LoadLibraryA (&dll_name);
  if (hinstLib != 0)
  {
    func = GetProcAddress (hinstLib, &func_name);
    *(LPVOID*)&DwmSetWindowAttribute = *(LPVOID*)&func;
  }

  // this trick avoids a white flash when displaying the window the first time

  if (DwmSetWindowAttribute != null)
  {
    BOOL cloak = TRUE;    // don't draw window on desktop
    DwmSetWindowAttribute (windows.main_hWnd, DWMWA_CLOAK, (byte*)&cloak, cloak'size);
  }

  ShowWindow (windows.main_hWnd, SW_SHOWDEFAULT);

  sleep 1.0 / 60.0;   // wait til wm_paint is done

  if (DwmSetWindowAttribute != null)
  {
    BOOL cloak = FALSE;   // make it appear on desktop
    DwmSetWindowAttribute (windows.main_hWnd, DWMWA_CLOAK, (byte*)&cloak, cloak'size);
  }

#elif ANDROID
  // $

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void set_main_window_position (int x, int y)
{
  if (main_window_windowpos != null)   // inside WM_WINDOWPOSCHANGING
  {
    main_window_windowpos->x = x;
    main_window_windowpos->y = y;
  }
  else
  {
#if WINDOWS
    RECT r;
    SetWindowPos (main_hWnd,
                  0,
                  x,
                  y,
                  0,
                  0,
                  SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);

    GetWindowRect (main_hWnd, &r);
    move_child_windows (r);
#endif

  }
}

//--------------------------------------------------------------------

public void get_main_window_position (out int x, out int y)
{
  if (main_window_windowpos != null)    // inside WM_WINDOWPOSCHANGING
  {
    x = main_window_windowpos->x;
    y = main_window_windowpos->y;
  }
  else
  {
#if WINDOWS
    RECT r;

    GetWindowRect (main_hWnd, &r);
    x = r.left;
    y = r.top;

#elif ANDROID
    x = 0;
    y = 0;

#else
  bad

#endif
  }
}

//--------------------------------------------------------------------

public void set_main_window_size (int x_size, int y_size)
{
  if (main_window_windowpos != null)    // inside WM_WINDOWPOSCHANGING
  {
    main_window_windowpos->cx = x_size;
    main_window_windowpos->cy = y_size;
  }
  else
  {
#if WINDOWS
    RECT r;
    SetWindowPos (main_hWnd,
                  0,
                  0,
                  0,
                  x_size,
                  y_size,
                  SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
    GetWindowRect (main_hWnd, &r);
    move_child_windows (r);

#elif ANDROID

#else
  bad

#endif
  }
}

//--------------------------------------------------------------------

public void get_main_window_size (out int x_size, out int y_size)
{
  if (main_window_windowpos != null)    // inside WM_WINDOWPOSCHANGING
  {
    x_size = main_window_windowpos->cx;
    y_size = main_window_windowpos->cy;
  }
  else
  {
#if WINDOWS
    RECT r;
    GetWindowRect (main_hWnd, &r);
    x_size = r.right - r.left;
    y_size = r.bottom - r.top;

#elif ANDROID
   get_desktop_resolution (out x_size, out y_size);

#else
  bad

#endif
  }
}

//--------------------------------------------------------------------

// get actual intern size (maybe user resized the application window or added a menu)

public void get_main_window_intern_size (out int width, out int height, bool scaled = true)
{
  if (main_window_windowpos != null)   // we're inside WM_WINDOWPOSCHANGING : fix with current size
  {
    int left, top, right, bottom;
    get_main_window_borders (out left, out top, out right, out bottom);
    width = main_window_windowpos->cx - left - right;
    height = main_window_windowpos->cy - top - bottom;
  }
  else
  {
#if WINDOWS
    RECT wrect;

    GetClientRect (main_hWnd, &wrect);

    width = wrect.right;
    height = wrect.bottom;

#elif ANDROID
   get_desktop_resolution (out width, out height);

#else
  bad

#endif
  }

  if (scaled)
  {
    width  = ui_unscale(width);
    height = ui_unscale(height);
  }
}

//---------------------------------------------------------------------

// get size of the 4 small borders of a main window.
//  (the difference between window size and client area)

public void get_main_window_borders (out int left, out int top, out int right, out int bottom)
{
#if WINDOWS
  int pad = GetSystemMetrics (SM_CXPADDEDBORDER);
  bottom = pad + GetSystemMetrics (SM_CYFRAME);
  top    = bottom + GetSystemMetrics (SM_CYCAPTION);
  right  = pad + GetSystemMetrics (SM_CXFRAME);
  left   = right;

#elif ANDROID
  left = 0;
  top = 0;
  right = 0;
  bottom = 0;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void wappend_menu (wstring name)
{
#if WINDOWS
  wstring^ s = zwstr (name);
  if (GetMenu(main_hWnd) == 0)
    SetMenu (main_hWnd, CreateMenu());
  AppendMenuW (GetMenu(main_hWnd), MF_POPUP, CreateMenu(), &s^);
  free s;
  DrawMenuBar(main_hWnd);

#elif ANDROID
  // $
  _unused name;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void append_menu (string name)
{
#if WINDOWS
  wstring^ s = zstr (name);
  wappend_menu (s^);
  free s;

#elif ANDROID
  // $
  _unused name;

#else
  bad

#endif
}

//--------------------------------------------------------------------

// assertion: id in 1 .. 32767

public void wappend_menu_item (wstring name, MENU_ID id)
{
#if WINDOWS
  wstring^ s = zwstr (name);

  assert id >= 1;

  if (GetMenu(main_hWnd) == 0)
    SetMenu (main_hWnd, CreateMenu());

  {
    HMENU hmenu = GetMenu(main_hWnd);
    int menu_pos = GetMenuItemCount(hmenu) - 1;
    AppendMenuW (GetSubMenu (hmenu, menu_pos), MF_STRING, id, &s^);
  }

  free s;
  DrawMenuBar(main_hWnd);

#elif ANDROID
  // $
  _unused name, id;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void append_menu_item (string name, MENU_ID id)
{
#if WINDOWS
  wstring^ s = zstr (name);
  wappend_menu_item (s^, id);
  free s;

#elif ANDROID
  // $
  _unused name, id;

#else
  bad

#endif
}

//--------------------------------------------------------------------

// assertion: id in 1 .. 32767

public void enable_menu_item (bool enable, MENU_ID id)
{
#if WINDOWS
  HMENU hmenu = GetMenu(main_hWnd);
  int   lx = GetMenuItemCount(hmenu);
  int   x, y;

  assert id >= 1;

  for (x=0; x<lx; x++)
  {
    HMENU hsub = GetSubMenu (hmenu, x);
    int count = GetMenuItemCount(hsub);
    for (y=0; y<count; y++)
    {
      if ((uint)id == GetMenuItemID (hsub, y))
        EnableMenuItem (hsub, (uint)id, enable ? MF_ENABLED : MF_GRAYED);
    }
  }

#elif ANDROID
  // $
  _unused enable, id;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void remove_menu_bar ()
{
#if WINDOWS
  HMENU hmenu;
  hmenu = GetMenu(main_hWnd);
  SetMenu (main_hWnd, 0);
  DestroyMenu (hmenu);

#elif ANDROID
  // $

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void post_menu_event (MENU_ID id)
{
#if WINDOWS
  PostMessageA (main_hWnd, WM_USER + 24, (uint)id, 0);

#elif ANDROID
  guiandroid.post_menu_event (id);

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void start_timer (uint msecs)
{
#if WINDOWS
  SetTimer (main_hWnd, nIDEvent => 1, uElapse => msecs, lpTimerFunc => null);

#elif ANDROID
  guiandroid.set_timer (msecs);

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void stop_timer ()
{
#if WINDOWS
  KillTimer (main_hWnd, 1);

#elif ANDROID
  guiandroid.stop_timer ();

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void set_main_window_title (string title)
{
#if WINDOWS
  wstring^ p  = zstr (title);
  SendMessagePtrW (main_hWnd, WM_SETTEXT, 0, &p^);
  free p;

#elif ANDROID
  // $
  _unused title;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void wset_main_window_title (wstring title)
{
#if WINDOWS
  wstring^ p  = zwstr (title);
  SendMessagePtrW (main_hWnd, WM_SETTEXT, 0, &p^);
  free p;

#elif ANDROID
  // $
  _unused title;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void post_dialog_message (DIALOG_ID d, int message)
{
#if WINDOWS
  HWND h = find_dialog_window (d);
  if (h != 0)
    PostMessageA (h, WM_USER+24, 0, message);

#elif ANDROID
  guiandroid.post_dialog_message (d, message);

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void set_mouse_cursor_shape (MOUSE_SHAPE shape)
{
#if WINDOWS
  g_mouse_shape = shape;
  set_cursor_shape ();

#elif ANDROID
  // $
  _unused shape;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void set_main_window_background_color (BACKGROUND_COLOR color)
{
  g_background_color = color;
}

//--------------------------------------------------------------------

public void set_relative_mouse_mode (bool on)
{
#if WINDOWS
  register_relative_mouse (on);

#elif ANDROID
  // $
  _unused on;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public void set_wallpaper (int size_x, int size_y, byte[] image)
{
#if WINDOWS
  packed struct INFO
  {
    BITMAPINFOHEADER bmiHeader;
    DWORD            bmiColors[3];
  }

  INFO info;
  byte *bits;

  clear info;
  info.bmiHeader.biSize        = BITMAPINFOHEADER'size;
  info.bmiHeader.biWidth       = size_x;
  info.bmiHeader.biHeight      = -size_y;
  info.bmiHeader.biPlanes      = 1;
  info.bmiHeader.biBitCount    = 32;
  info.bmiHeader.biCompression = BI_BITFIELDS;
  info.bmiHeader.biSizeImage   = (uint)(4 * size_x * size_y);
  info.bmiColors = {0xFF, 0xFF00, 0xFF0000};

  if (g_wallpaper_bitmap != 0)
    DeleteObject (g_wallpaper_bitmap);

  g_wallpaper_bitmap = CreateDIBSection (0, (byte *)&info, DIB_RGB_COLORS, &bits, 0, 0);
  assert g_wallpaper_bitmap != 0;

  bits[0:info.bmiHeader.biSizeImage] = image;

  InvalidateRect (main_hWnd, null, FALSE);

#elif ANDROID
  // $
  _unused size_x, size_y, image;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public
void fit_on_monitor (ref int x, ref int y, ref int x_size, ref int y_size)
{
#if WINDOWS
  int pad = GetSystemMetrics (SM_CXPADDEDBORDER);
  int pad_dx = pad + GetSystemMetrics (SM_CXFRAME);
  int pad_dy = pad + GetSystemMetrics (SM_CYFRAME);
  int monitor_dx, monitor_dy;

  RECT        r;
  HMONITOR    h;
  MONITORINFO monitor;

  r = {left   => x,
       top    => y,
       right  => x + x_size,
       bottom => y + y_size};

  h = MonitorFromRect (rect => &r, dwFlags => MONITOR_DEFAULTTONEAREST);

  clear monitor;
  monitor.cbSize = monitor'size;
  GetMonitorInfoA (hMonitor => h, lpmi => &monitor);

  monitor.rcWork.left   -= pad_dx;
  monitor.rcWork.right  += pad_dx;
  monitor.rcWork.top    -= pad_dy;
  monitor.rcWork.bottom += pad_dy;

  monitor_dx = monitor.rcWork.right  - monitor.rcWork.left;
  monitor_dy = monitor.rcWork.bottom - monitor.rcWork.top;

  // shrink if too large for monitor
  if (x_size > monitor_dx)
    x_size = monitor_dx;
  if (y_size > monitor_dy)
    y_size = monitor_dy;

  // move to center
  if (x + x_size > monitor.rcWork.right)
    x = monitor.rcWork.right - x_size;
  if (y + y_size > monitor.rcWork.bottom)
    y = monitor.rcWork.bottom - y_size;
  if (x < monitor.rcWork.left)
    x = monitor.rcWork.left;
  if (y < monitor.rcWork.top)
    y = monitor.rcWork.top;

#elif ANDROID
  // $
  _unused x, y, x_size, y_size;

#else
  bad

#endif
}

//--------------------------------------------------------------------

public
void start_dialog_timer (uint msecs)
{
#if WINDOWS
  SetTimer (g_dialog_ptr->hwnd, nIDEvent => 100, uElapse => msecs, lpTimerFunc => null);

#elif ANDROID
  guiandroid.set_dialog_timer (msecs);

#else
  bad
#endif
}

//--------------------------------------------------------------------

// returns drive letter of removeable drive when user inserts it, or nul if none.

#if WINDOWS
public char detect_removeable_drive ()
{
  return guicb.detect_removeable_drive();
}
#endif

//--------------------------------------------------------------------
#end unsafe
//--------------------------------------------------------------------
