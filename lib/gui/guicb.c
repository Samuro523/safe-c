
// guicb.c

#if WINDOWS
  use ../win/windows;
  use ../arithm, ../strings, ../memory;
  use guidraw, guitreatevent;
  use ../edline;
#endif

use ../gui, guitree, guihash;

//--------------------------------------------------------------------------
#begin unsafe
//--------------------------------------------------------------------------

const wstring child_class = L"child\0";

bool g_is_minimized;
bool g_maximize_main_screen;
int  g_old_xPosRelative, g_old_yPosRelative;
bool g_relative_mouse_registered;
bool g_main_window_move_in_progress;
uint freeze_move_windows_til;
char g_removable_drive;

//--------------------------------------------------------------------

// returns drive letter of removeable drive when user inserts it, or nul if none.

public char detect_removeable_drive ()
{
  return g_removable_drive;
}

//--------------------------------------------------------------------

void merge_rect (ref RECT r, RECT n)
{
  if (r.left > n.left)
    r.left = n.left;
  if (r.top > n.top)
    r.top = n.top;
  if (r.right < n.right)
    r.right = n.right;
  if (r.bottom < n.bottom)
    r.bottom = n.bottom;
}

//--------------------------------------------------------------------

public void repaint_control (CONTROL_INFO o)
{
  RECT r;

  clear r;

  r.left   = g_dialog_ptr->border_size + o.x;
  r.right  = g_dialog_ptr->border_size + o.x + o.x_size;
  r.top    = g_dialog_ptr->border_size + g_dialog_ptr->title_height + o.y;
  r.bottom = g_dialog_ptr->border_size + g_dialog_ptr->title_height + o.y + o.y_size;

#if WINDOWS
  InvalidateRect (g_dialog_ptr->hwnd, &r, FALSE);

#elif ANDROID
  merge_rect (ref g_dialog_ptr->paint_rect, r);
  g_dialog_ptr->needs_repaint = true;
  g_dialogs_needs_repaint = true;

#else
  bad

#endif

  if (o.typ == TYP_COMBO && o.combo.lb.listbox_shown)
  {
    r.top    = g_dialog_ptr->border_size + g_dialog_ptr->title_height + o.combo.lb.y;
    r.bottom = g_dialog_ptr->border_size + g_dialog_ptr->title_height + o.combo.lb.y + o.combo.lb.y_size;

#if WINDOWS
    InvalidateRect (g_dialog_ptr->hwnd, &r, FALSE);

#elif ANDROID
    merge_rect (ref g_dialog_ptr->paint_rect, r);

#else
    bad

#endif
  }
}

//--------------------------------------------------------------------------

public void repaint_control_rect (CONTROL_INFO o, RECT rect)
{
  RECT r;

  clear r;

  r.left   = g_dialog_ptr->border_size + o.x + rect.left;
  r.right  = g_dialog_ptr->border_size + o.x + rect.right;
  r.top    = g_dialog_ptr->border_size + g_dialog_ptr->title_height + o.y + rect.top;
  r.bottom = g_dialog_ptr->border_size + g_dialog_ptr->title_height + o.y + rect.bottom;

#if WINDOWS
  InvalidateRect (g_dialog_ptr->hwnd, &r, FALSE);

#elif ANDROID
  merge_rect (ref g_dialog_ptr->paint_rect, r);
  g_dialog_ptr->needs_repaint = true;
  g_dialogs_needs_repaint = true;

#else
  bad

#endif
}

//--------------------------------------------------------------------------

int key_of (uint message, uint wParam)
{
  int key;

#if WINDOWS
  g_shift_pressed   = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
  g_control_pressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  g_alt_pressed     = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;


#if 0  // old method
  switch (message)
  {

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      switch (wParam)
      {
        case VK_SHIFT:
          g_shift_pressed = true;
          break;

        case VK_CONTROL:
          g_control_pressed = true;
          break;

        case VK_MENU:
          g_alt_pressed = true;
          break;

        default:
          break;
      }
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      switch (wParam)
      {
        case VK_SHIFT:
          g_shift_pressed = false;
          break;

        case VK_CONTROL:
          g_control_pressed = false;
          break;

        case VK_MENU:
          g_alt_pressed = false;
          break;

        default:
          break;
      }
      break;

    default:
      break;
  }
#endif  // old method

  switch (message)
  {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      key = KEY_CMD + (int)(wParam & 255);
      if (message == WM_KEYUP || message == WM_SYSKEYUP)
        key |= KEY_RELEASE;
      if (g_shift_pressed)
        key |= KEY_SHIFT;
      if (g_control_pressed)
        key |= KEY_CONTROL;
      if (g_alt_pressed)
        key |= KEY_ALT;
      break;

    case WM_CHAR:
      key = (int)(wParam & 65535);
      break;

    default:
      key = 0;
      break;
  }

  return key;

#elif ANDROID
  switch (message)
  {
    case 0:
      _unused wParam;
      key = 0;
      break;

    default:
      key = 0;
      break;
  }

  return key;
#else
    bad

#endif
}

//--------------------------------------------------------------------------

public void set_cursor_shape ()
{
#if WINDOWS
  INT_PTR shape;

  if (g_relative_mouse_registered)
    shape = 0;
  else if (g_mouse_shape != MOUSE_SHAPE_DEFAULT)
  {
    if (g_mouse_shape == MOUSE_SHAPE_HOURGLASS)
      shape = IDC_WAIT;
    else // if (g_mouse_shape == MOUSE_SHAPE_NONE)
      shape = 0;
  }
  else if (g_drag_selected && g_drag_moved && !g_user_drop_allowed)
    shape = IDC_NO;
  else if (g_drag_selected && g_drag_moved)
    shape = IDC_HAND;
  else
    shape = g_basic_mouse_shape;   // IDC_ARROW or IDC_BEAM

  if (g_current_mouse_shape != shape)
  {
    g_current_mouse_shape = shape;
    if (shape == 0)
      g_hcursor_shape = 0;
    else
      g_hcursor_shape = LoadCursorA (0, shape);
  }

  SetCursor (g_hcursor_shape);
#endif
}

//--------------------------------------------------------------------------

void set_middle_mouse_button (bool b)
{
  if (g_middle_mouse_button_pressed != b)
  {
    APPLICATION_EVENT e;
    clear e;
    e.type = EVENT_MOUSE_MIDDLE_BUTTON;
    e.key = (int)b;
    g_middle_mouse_button_pressed = b;
    app_handler (e);
  }
}

//--------------------------------------------------------------------------

#if WINDOWS

//-------------------------------------------

char FirstDriveFromMask (ULONG unitmask0)
{
  uint i;
  ULONG unitmask = unitmask0;

  for (i = 0; i < 26; i++)
  {
    if ((unitmask & 0x1) != 0)
      break;
    unitmask >>= 1;
  }

  return (char)((uint)'A' + i);
}

//-------------------------------------------

// child window procedure

[callback]
LRESULT ChildWindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  DIALOG_INFO* safeguarded_dialog_ptr = g_dialog_ptr;
  bool         call_default_handler = true;
  LRESULT      window_retcode = 0;

  g_dialog_ptr = (DIALOG_INFO*)GetWindowLongPtrA ((HWND)hwnd, 0);

  if (g_dialog_ptr == null)
  {
    g_dialog_ptr = g_new_dialog_ptr;
    g_new_dialog_ptr = null;
    SetWindowLongPtrA (hwnd, nIndex => 0, (byte*)g_dialog_ptr);    // stores g_dialog_ptr into the window data.
  }

  switch (message)
  {
    case WM_CREATE:
    {
      if (!treat_init_window (g_dialog_ptr->d, hwnd))
      {
        call_default_handler = false;
        window_retcode = -1;    // note that g_dialog_ptr->hwnd equals 0 here, so we won't execute termination in WM_DESTROY
        break;
      }

      // fill hwnd
      g_dialog_ptr->hwnd = hwnd;

      // send EVENT_NEW_DIALOG to application so it can init title, create controls, ..
      {
        EVENT e;
        clear e;
        e.d = g_dialog_ptr->d;
        e.type = EVENT_NEW_DIALOG;
        g_dialog_ptr->handler(e);
      }

      // if no focus was set by user, set focus on first visible control
      if (g_dialog_ptr->focus == null)
        set_tab_on_next_or_previous_object (ref *g_dialog_ptr, next => true);
    }
    break;

    case WM_NCHITTEST:  // allow dragging window by clicking on client area
      {
        LRESULT rc = DefWindowProcW (hwnd, message, wParam, lParam);
        int x = (short)lParam;
        int y = (int)(lParam >> 16);
        DIALOG_INFO* p = g_dialog_ptr;
        INT_PTR shape;

        if (y <= p->rect.top + p->border_drag_size - 1)
          rc = HTTOP;  // 12
        else if (y >= p->rect.bottom - p->border_drag_size)
          rc = HTBOTTOM;  // 15;
        else
          rc = 9;

        if (x <= p->rect.left + p->border_drag_size - 1)
          rc += 1;
        else if (x >= p->rect.right - p->border_drag_size)
          rc += 2;

        shape = IDC_ARROW;
        g_user_drop_allowed = false;

        if (rc == 9)   // inside window (not border)
        {
          x -= p->rect.left;
          y -= p->rect.top;

          if (y < p->border_drag_size + p->title_height &&
              x >= p->rect.right - p->rect.left - p->border_drag_size - p->title_height)
            rc = HTCLIENT;    // close button - in client area (generates WM_MOUSEMOVE)
          else
          {
            CONTROL_INFO^ c = touched_control (p, x, y);
            if (c == null || !tabable[(int)c^.typ])  // we didn't clicked on a tabbable control
              rc = HTCAPTION;   // move window
            else
            {
              rc = HTCLIENT;    // in client area (generates WM_MOUSEMOVE)
              if (c != null)
              {
                switch (c^.typ)
                {
                  case TYP_EDIT:
                    shape = IDC_IBEAM;
                    break;

                  case TYP_COMBO:
                    {
                      ref CONTROL_INFO o = c^;
                      int ofs_x = p->border_size;
                      int ofs_y = p->border_size + p->title_height;
                      int cx = x - ofs_x;
                      int cy = y - ofs_y;

                      if (cx >= o.x &&
                          cx < o.x+o.x_size-o.combo.listbox.arrow_box_width &&
                          cy >= o.y &&
                          cy < o.y+o.y_size &&
                          edit_line_get_modify_allowed (c^.combo.edit.cr.edit_line))
                      {
                        shape = IDC_IBEAM;
                      }
                    }
                    break;

                  default:
                    break;
                }


                if (g_drag_selected & g_drag_moved)
                {
                  switch (c^.typ)
                  {
                    case TYP_LISTBOX:
                    case TYP_TREE:
                      {
                        EVENT e;
                        clear e;
                        e.d = g_dialog_ptr->d;
                        e.id = c^.id;
                        e.type = EVENT_LISTBOX_TEST_DROP_ALLOWED;
                        g_dialog_ptr->handler(e);
                      }
                      break;

                    case TYP_EDIT:
                    case TYP_TEXT:
                    case TYP_WINDOW:
                      {
                        EVENT e;
                        clear e;
                        e.d = g_dialog_ptr->d;
                        e.id = c^.id;
                        e.type = EVENT_TEST_DROP_ON_CONTROL_ALLOWED;
                        g_dialog_ptr->handler(e);
                      }
                      break;

                    default:
                      break;
                  }
                }
              }
            }
          }
        }

        g_basic_mouse_shape = shape;  // IDC_ARROW or IDC_IBEAM

        call_default_handler = false;
        window_retcode = rc;
        break;
      }

    case WM_ACTIVATE:
      {
        DIALOG_INFO* p2 = (DIALOG_INFO*)GetWindowLongPtrA ((HWND)lParam, 0);
        EVENT e;

        clear e;
        e.d = g_dialog_ptr->d;
        e.type = (wParam & 0xFFFF) != 0 ? EVENT_DIALOG_ACTIVATED : EVENT_DIALOG_DEACTIVATED;
        e.other_d = p2 == null ||
                    GetWindow ((HWND)lParam, GW_OWNER) != main_hWnd ||
                    p2->hwnd != (HWND)lParam
                  ? 0 : p2->d;
        g_dialog_ptr->handler(e);

        if (e.other_d == 0 && (HWND)lParam != main_hWnd)
        {
          g_alt_pressed = false;  // when doing Alt-Tab and coming back, unpress Alt

          {
            APPLICATION_EVENT ae;
            clear ae;
            ae.key = (int)(e.type == EVENT_DIALOG_ACTIVATED);
            ae.type = EVENT_ACTIVATE;
            app_handler (ae);
          }
        }
      }
      break;

    case WM_DEVICECHANGE:
      switch (wParam)
      {
        case 7:  // DBT_DEVNODES_CHANGED
          freeze_move_windows_til = GetTickCount() + 3000;
          break;

        default:
          break;
      }
      break;

    case WM_WINDOWPOSCHANGING:
      {
        DIALOG_INFO* p = g_dialog_ptr;
        WINDOWPOS    wpos*, newpos;
        EVENT        e;

        wpos'byte = lParam'byte;

        newpos = *wpos;   // this is the new wanted pos/size

        // set old pos/size for now
        wpos->x = p->rect.left;
        wpos->y = p->rect.top;
        wpos->cx = p->rect.right - p->rect.left;
        wpos->cy = p->rect.bottom - p->rect.top;

        if (freeze_move_windows_til != 0)   // freeze any windows pos/size changes for 3 seconds after WM_DEVICECHANGE,
        {                                   // otherwise Windows moves WS_POPUP windows !
          if (freeze_move_windows_til - GetTickCount() <= 3000)
          {
            wpos->flags &= (UINT'max - SWP_NOMOVE - SWP_NOSIZE);   // always move & resize
            break;
          }
          else
            freeze_move_windows_til = 0;
        }

        if ((newpos.flags & SWP_NOMOVE) != 0)    // x, y not filled
        {
          newpos.x = wpos->x;
          newpos.y = wpos->y;
        }

        if ((newpos.flags & SWP_NOSIZE) != 0)    // cx, cy not filled
        {
          newpos.cx = wpos->cx;
          newpos.cy = wpos->cy;
        }

        if (!g_main_window_move_in_progress)
        {
          // old position in wpos will be updated by user through this global pointer
          sub_window_windowpos = wpos;

//        if (newpos.x != wpos->x || newpos.y != wpos->y)    // x, y changed
          {
            clear e;
            e.d = p->d;
            e.type = EVENT_MOVE;

            e.x = ui_unscale (newpos.x - (main_win_rect.left + x_border));
            e.y = ui_unscale (newpos.y - (main_win_rect.top + y_border + y_title));
            p->handler(e);  // can change all 4 coord (move) or right/bottom (size) in any order
          }

          if (newpos.cx != wpos->cx || newpos.cy != wpos->cy)    // cx, cy changed
          {
            clear e;
            e.d = p->d;
            e.type = EVENT_RESIZE;
            e.x_size = ui_unscale (newpos.cx);
            e.y_size = ui_unscale (newpos.cy);
            p->handler(e);
          }

          sub_window_windowpos = null;
        }

        p->rect = {left   => wpos->x,
                   top    => wpos->y,
                   right  => wpos->x + wpos->cx,
                   bottom => wpos->y + wpos->cy};

        wpos->flags &= (UINT'max - SWP_NOMOVE - SWP_NOSIZE);   // always move & resize
      }
      break;

    case WM_SETFOCUS:
      // make this window more opaque
      {
        DIALOG_INFO* p = g_dialog_ptr;

        int OPAQUE = p->dialog_transparency_focus;
        if (p->current_transparency != OPAQUE)
        {
          SetLayeredWindowAttributes (hwnd, 0, (byte)OPAQUE, LWA_ALPHA);
          p->current_transparency = OPAQUE;
        }

        // window has now keyboard focus
        p->has_keyboard_focus = true;

        // show dash points on focus control and caret
        if (p->focus != null)
          repaint_control (p->focus^);
      }
      break;

    case WM_KILLFOCUS:
      {
        DIALOG_INFO* p = g_dialog_ptr;

        p->has_keyboard_focus = false;

        // close caret in case the new window belongs to another application
        DestroyCaret();
        caret_visible = false;
        caret_new_height = 0;

        // hide dash points on focus control and caret
        if (p->focus != null)
        {
          repaint_control (p->focus^);

          // close open combobox that has focus
          if (p->focus^.typ == TYP_COMBO)
            p->focus^.combo.lb.listbox_shown = false;
        }
      }
      break;

    case WM_MOUSEMOVE:
      {
        DIALOG_INFO* p = g_dialog_ptr;
        int  ofs_x = p->border_size;
        int  ofs_y = p->border_size + p->title_height;
        int2 x, y;
        bool lbutton, mbutton;

        x = (short)(lParam & 0xFFFF);
        y = (short)(lParam >> 16);
        lbutton = (wParam & (uint)MK_LBUTTON) != 0;
        mbutton = (wParam & (uint)MK_MBUTTON) != 0;
        set_middle_mouse_button (mbutton);

        if (!p->mouse_tracking_active)
        {
          TRACKMOUSEEVENT EventTrack;

          p->mouse_tracking_active = true;

          clear EventTrack;
          EventTrack.cbSize = EventTrack'size;
          EventTrack.dwFlags = TME_LEAVE;
          EventTrack.hwndTrack = hwnd;
          TrackMouseEvent (&EventTrack);
        }

        if (!lbutton)
        {
          g_drag_selected = false;
          set_cursor_shape ();
        }

        if (x != p->previous_mouse_x || y != p->previous_mouse_y)  // Windows sends unecessary move messages
        {
          p->previous_mouse_x = x;
          p->previous_mouse_y = y;

          {
            CONTROL_INFO^ c = touched_control (p, x, y);
            if (lbutton)
              mouse_drag (c, x-ofs_x, y-ofs_y);
            else
              mouse_hover (c, x-ofs_x, y-ofs_y);
          }

          g_drag_moved = true;
          set_cursor_shape ();
        }
      }
      break;

    case WM_SETCURSOR:
      if ((lParam & 0xffff) == HTCLIENT || (lParam & 0xffff) == HTCAPTION)
      {
        set_cursor_shape ();
        SetClassLongA (hwnd, GCL_HCURSOR, (LONG)g_hcursor_shape);

        call_default_handler = false;
        window_retcode = 1;  // Processed
        break;
      }
      break;

    case WM_MOUSELEAVE:     // leave control
      {
        DIALOG_INFO* p = g_dialog_ptr;
        p->mouse_tracking_active = false;
        mouse_click_left_released (null, int'min, int'min);
      }
      break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
      {
        DIALOG_INFO* p = g_dialog_ptr;
        int  x = (short)(lParam & 0xFFFF);
        int  y = (short)(lParam >> 16);
        int ofs_x = p->border_size;
        int ofs_y = p->border_size + p->title_height;
        CONTROL_INFO^ c = touched_control (p, x, y);
        SetCapture (hwnd);
        mouse_click_left (c, x-ofs_x, y-ofs_y, message == WM_LBUTTONDBLCLK, x, y);
      }
      break;

    case WM_NCLBUTTONDOWN:
      mouse_click_left (null, 0, 0, false, 0, 0);  // will close combobox and remove text selection
      break;

    case WM_RBUTTONDOWN:    // right mouse button : just set focus and treat window control
      {
        DIALOG_INFO* p = g_dialog_ptr;
        int  x = (short)(lParam & 0xFFFF);
        int  y = (short)(lParam >> 16);
        int ofs_x = p->border_size;
        int ofs_y = p->border_size + p->title_height;
        CONTROL_INFO^ c = touched_control (p, x, y);
        if (c != null)
          mouse_click_right (c, x-ofs_x, y-ofs_y, x, y);
      }
      break;

    case WM_LBUTTONUP:
      {
        DIALOG_INFO* p = g_dialog_ptr;
        int  x = (short)(lParam & 0xFFFF);
        int  y = (short)(lParam >> 16);
        int ofs_x = p->border_size;
        int ofs_y = p->border_size + p->title_height;
        CONTROL_INFO^ c = touched_control (p, x, y);
        mouse_click_left_released (c, x-ofs_x, y-ofs_y);
        ReleaseCapture();
        g_drag_selected = false;
        set_cursor_shape ();
      }
      break;

    case WM_NCLBUTTONUP:
      g_drag_selected = false;
      set_cursor_shape ();
      break;

    case WM_RBUTTONUP:    // right mouse button : show context menu
      {
        DIALOG_INFO* p = g_dialog_ptr;
        int  x = (short)(lParam & 0xFFFF);
        int  y = (short)(lParam >> 16);
        int ofs_x = p->border_size;
        int ofs_y = p->border_size + p->title_height;
        CONTROL_INFO^ c = touched_control (p, x, y);
        mouse_click_right_released (c, x-ofs_x, y-ofs_y, x, y);
      }
      break;

    case WM_MBUTTONDOWN:
      set_middle_mouse_button (true);
      break;

    case WM_MBUTTONUP:
      set_middle_mouse_button (false);
      break;

    case WM_MOUSEWHEEL:
      {
        const int WHEEL_DELTA = 120;
        if (g_dialog_ptr->redirect_keyboard_to_main_window)
        {
          APPLICATION_EVENT e;
          clear e;
          e.x = 0;
          e.y = (((int)wParam >> 16) / WHEEL_DELTA);
          e.type = EVENT_MOUSE_WHEEL;
          app_handler (e);
        }
        else
        {
          mouse_wheel (((int)wParam >> 16) / WHEEL_DELTA);
        }
      }
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
      {
        int key, scan, count, i;
        key = key_of (message, (uint)wParam);
        scan = key & (KEY_CMD+0xFFFF);
        count = (int)(lParam & 32767);
        for (i=0; i<count; i++)
        {
          if (g_dialog_ptr->redirect_keyboard_to_main_window ||
              key == 27 ||   // escape
              (scan >= KEY_CMD+0x70 && scan <= KEY_CMD+0x87) ||  // Fxx keys
               scan==KEY_CMD+VK_SHIFT ||    // shift key
               scan==KEY_CMD+VK_CONTROL ||  // control key
               scan==KEY_CMD+VK_MENU)       // alt key
          {
            APPLICATION_EVENT e;
            clear e;
            e.key = key;
            e.scancode = (int)((lParam >> 16) & 255);
            e.type = EVENT_KEYBOARD;
            app_handler (e);
          }
          else
          {
            treat_keyboard_key (key);
          }
        }
      }
      break;

    case WM_ERASEBKGND:
      call_default_handler = false;
      window_retcode = 1;  // no default drawing of background
      break;

    case WM_PAINT:       // paint all controls
    {
      PAINTSTRUCT s;
      BeginPaint (hwnd, &s);
      redraw_dialogs (hwnd, s.hdc, L"", s, *g_dialog_ptr);
      EndPaint (hwnd, &s);
      update_caret (hwnd, *g_dialog_ptr);
    }
    break;

    case WM_CLOSE:
      PostMessageA (main_hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);      // post ALT-F4 to main window
      call_default_handler = false;
      window_retcode = 0;
      break;

    case WM_TIMER:
      KillTimer (hwnd, wParam);
      {
        EVENT e;
        clear e;
        e.d = g_dialog_ptr->d;
        e.type = EVENT_DIALOG_TIMER;
        g_dialog_ptr->handler(e);
      }
      break;

    case WM_USER + 24:  // received user message in lParam
      {
        EVENT e;
        clear e;
        e.d = g_dialog_ptr->d;
        e.type = EVENT_MESSAGE;
        e.message = (int)lParam;
        g_dialog_ptr->handler(e);
      }
      break;

    case WM_USER + 22:
      {
        if (g_dialog_ptr->hwnd != 0)  // initialization succeeded, so do also closing
        {
          EVENT e;

          // send EVENT_CLOSE_DIALOG to application
          clear e;
          e.d = g_dialog_ptr->d;
          e.type = EVENT_CLOSE_DIALOG;
          g_dialog_ptr->handler(e);
        }

        // destroy window later, when all the stacked calls were treated.
        PostMessageA (hwnd, WM_USER + 23, 0, 0);
      }
      break;

    case WM_USER + 23:
      DestroyWindow (hwnd);
      break;

    case WM_NCDESTROY:
      if (g_dialog_ptr != null)
      {
        // free all controls of this dialog
        deallocate_controls (g_dialog_ptr->list);

        // clear dialog structure
        clear *g_dialog_ptr;
        freem((byte*)g_dialog_ptr);
        g_dialog_ptr = null;

        SetWindowLongPtrA (hwnd, nIndex => 0, (byte*)null);
      }
      break;

    default:
      break;
  }


  if (call_default_handler)
    window_retcode = DefWindowProcW (hwnd, message, wParam, lParam);

  g_dialog_ptr = safeguarded_dialog_ptr;

  return window_retcode;
}

#endif

//--------------------------------------------------------------------------

package P
  typedef void TREAT (DIALOG_INFO* p, RECT* pdesktop, RECT* pchildrect);

  struct TREAT_INFO
  {
    TREAT treat;
    RECT* desktop;
    RECT* pchildrect;
  }
end P;

//--------------------------------------------------------------------------

void move_child_window (DIALOG_INFO* p, RECT* pdesktop, RECT* pchildrect)
{
#if WINDOWS
  int  diff1, diff2, diff3, diff4, offset, offset1, offset2;
  bool resizable;
  RECT border;

  resizable = (GetWindowLongW (p->hwnd, GWL_STYLE) & (LONG)WS_SIZEBOX) == (LONG)WS_SIZEBOX;

  clear border;
  border.bottom = y_border;
  border.top    = y_border + y_title;
  border.left   = x_border;
  border.right  = x_border;

  diff1 = abs (main_win_rect.top    + border.top    - p->rect.top);
  diff2 = abs (main_win_rect.bottom - border.bottom - p->rect.bottom);
  diff3 = abs (main_win_rect.top    + border.top    - p->rect.bottom);
  diff4 = abs (main_win_rect.bottom - border.bottom - p->rect.top);

  if (min(diff1,diff3) < min(diff2,diff4))
    offset = pchildrect->top - main_win_rect.top;
  else
    offset = pchildrect->bottom - main_win_rect.bottom;

  if (diff1 < 3 && resizable)
    offset1 = pchildrect->top - main_win_rect.top;
  else
    offset1 = offset;

  if (diff2 < 3 && resizable)
    offset2 = pchildrect->bottom - main_win_rect.bottom;
  else
    offset2 = offset;

  if (offset1 < 0)
    offset1 = max (offset1, min(0,pdesktop->top - p->rect.top));
  else
    offset1 = min (offset1, max(0,pdesktop->bottom - p->rect.bottom));

  if (offset2 < 0)
    offset2 = max (offset2, min(0,pdesktop->top - p->rect.top));
  else
    offset2 = min (offset2, max(0,pdesktop->bottom - p->rect.bottom));

  p->rect.top += offset1;
  p->rect.bottom += offset2;


  diff1 = abs (main_win_rect.left+border.left   - p->rect.left);
  diff2 = abs (main_win_rect.right-border.right - p->rect.right);
  diff3 = abs (main_win_rect.left+border.left   - p->rect.right);
  diff4 = abs (main_win_rect.right-border.right - p->rect.left);

  if (min(diff1,diff3) < min(diff2,diff4))
    offset = pchildrect->left - main_win_rect.left;
  else
    offset = pchildrect->right - main_win_rect.right;

  if (diff1 < 3 && resizable)
    offset1 = pchildrect->left - main_win_rect.left;
  else
    offset1 = offset;

  if (diff2 < 3 && resizable)
    offset2 = pchildrect->right - main_win_rect.right;
  else
    offset2 = offset;

  if (offset1 < 0)   // move left
    offset1 = max (offset1, min(0,pdesktop->left - p->rect.left));
  else               // move right
    offset1 = min (offset1, max(0,pdesktop->right - p->rect.right));

  if (offset2 < 0)
    offset2 = max (offset2, min(0,pdesktop->left - p->rect.left));
  else
    offset2 = min (offset2, max(0,pdesktop->right - p->rect.right));

  p->rect.left += offset1;
  p->rect.right += offset2;

  MoveWindow(p->hwnd,
             p->rect.left,
             p->rect.top,
             p->rect.right - p->rect.left,
             p->rect.bottom - p->rect.top,
             TRUE);    // must be TRUE for Windows Classic, can be FALSE for Aero.

#elif ANDROID
  _unused p, pdesktop, pchildrect;

#else
  bad

#endif
}

//--------------------------------------------------------------------------

#if WINDOWS

void make_half_transparent (DIALOG_INFO* p, RECT* pdesktop, RECT* pchildrect)
{
  int TRANSPARENT = p->dialog_transparency_non_focus;

  _unused pdesktop;
  _unused pchildrect;

  if (p->current_transparency != TRANSPARENT)
  {
    SetLayeredWindowAttributes (p->hwnd, 0, (byte)TRANSPARENT, LWA_ALPHA);
    p->current_transparency = TRANSPARENT;
  }
}

#endif

//--------------------------------------------------------------------------

#if WINDOWS

[callback]
BOOL treat_child_window (HWND hwnd, LPARAM lParam)
{
  DIALOG_INFO* p;
  TREAT_INFO* ptr;

  if (GetWindow (hwnd, GW_OWNER) != main_hWnd)
    return TRUE;   // continue with next window

  p = (DIALOG_INFO*)GetWindowLongPtrA (hwnd, 0);
  if (p == null)
    return TRUE;   // continue with next window

  ptr'byte = lParam'byte;

  ptr->treat (p, ptr->desktop, ptr->pchildrect);

  return TRUE;   // continue with next window
}

#endif

//--------------------------------------------------------------------------

#if WINDOWS

void treat_child_windows (TREAT treat, RECT* pdesktop, RECT* pchildrect)
{
  TREAT_INFO t = {treat, pdesktop, pchildrect};
  TREAT_INFO* pt = &t;
  LPARAM par;

  if (main_hWnd == 0)
    return;

  par'byte = pt'byte;

  EnumWindows (treat_child_window, par);

}

#endif

//--------------------------------------------------------------------------

#if WINDOWS

void get_work_area (RECT origin, out RECT rect)
{
  HMONITOR    h = MonitorFromRect (rect => &origin,
                                   dwFlags => MONITOR_DEFAULTTONEAREST);
  MONITORINFO info;

  clear info;
  info.cbSize = info'size;
  GetMonitorInfoA (h, &info);
  rect = info.rcWork;
}

#endif

//--------------------------------------------------------------------------

public
int register_relative_mouse (bool register)
{
#if WINDOWS
  RAWINPUTDEVICE Rid[1];
  BOOL           regDeviceDone;

  if (g_relative_mouse_registered == register)  // already
    return 0;

  clear Rid;
  Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
  Rid[0].usUsage     = HID_USAGE_GENERIC_MOUSE;

  if (register)
  {
    Rid[0].dwFlags = RIDEV_NOLEGACY;   //  | RIDEV_CAPTUREMOUSE; // RIDEV_INPUTSINK;
    Rid[0].hwndTarget = main_hWnd;
  }
  else
  {
    Rid[0].dwFlags = RIDEV_REMOVE;
  }

  regDeviceDone = RegisterRawInputDevices (&Rid, 1, RAWINPUTDEVICE'size);
  if (regDeviceDone == FALSE)
    return -1;  // failed

  g_relative_mouse_registered = register;

  set_cursor_shape ();

#elif ANDROID
  _unused register;

#else
  bad

#endif

  return 0;
}

//--------------------------------------------------------------------------

public void move_child_windows (RECT r)
{
#if WINDOWS
  RECT desktop;

  if (memcmp (r, main_win_rect) == 0)
    return;

  get_work_area (r, out desktop);

  desktop.top    = min (desktop.top,    r.top);
  desktop.left   = min (desktop.left,   r.left);
  desktop.right  = max (desktop.right,  r.right);
  desktop.bottom = max (desktop.bottom, r.bottom);

  g_main_window_move_in_progress = true;

  // move all sub windows
  treat_child_windows (move_child_window, &desktop, &r);

  g_main_window_move_in_progress = false;

  // store main window position
  main_win_rect = r;

#elif ANDROID
  _unused r;

#else
  bad

#endif
}


//--------------------------------------------------------------------------

public HWND find_dialog_window (DIALOG_ID d)
{
  return guihash . find_hwnd_of_dialog (d);
}

//--------------------------------------------------------------------------

#if WINDOWS

// this is the message handler for the main window

[callback]
LRESULT MainWindowProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  DIALOG_INFO* safeguarded_dialog_ptr = g_dialog_ptr;
  bool         call_default_handler = true;
  LRESULT      window_retcode = 0;

  g_dialog_ptr = null;

  switch (message)
  {
    case WM_CREATE:
      break;

    case WM_NCACTIVATE:
      if (wParam == 0)   // window becomes inactive
      {
        {
          APPLICATION_EVENT e;
          clear e;
          e.type = EVENT_KEYBOARD;  // key 0 = keyboard focus lost
          app_handler (e);
        }

        call_default_handler = false;
        window_retcode = TRUE;     // don't change title color
      }
      break;

    case WM_ERASEBKGND:
      if (g_wallpaper_bitmap != 0)
      {
        window_retcode = 0;  // further painting necessary
      }
      else
      {
        const int[1+(uint)BACKGROUND_COLOR'last] BRUSHES = {BLACK_BRUSH, WHITE_BRUSH, HOLLOW_BRUSH};
        int  brush = BRUSHES[(uint)g_background_color];
        HDC  hdc = (HDC) wParam;
        RECT rect;
        GetClientRect (hWnd, &rect);
        FillRect (hdc, &rect, (HBRUSH) GetStockObject (brush));
        window_retcode = 1;  // no further painting necessary
      }
      call_default_handler = false;
      break;

    case WM_PAINT:
      if (g_wallpaper_bitmap != 0)
      {
        PAINTSTRUCT  s;
        HDC          hdc;
        RECT         rect;

        hdc = BeginPaint (hWnd, &s);

        GetClientRect (hWnd, &rect);
        if (rect.right > 0 && rect.bottom > 0)
        {
          struct BITMAP
          {
            LONG   bmType;
            LONG   bmWidth;
            LONG   bmHeight;
            LONG   bmWidthBytes;
            WORD   bmPlanes;
            WORD   bmBitsPixel;
            LPVOID bmBits;
          }

          HDC          hdcMem;
          HGDIOBJ      oldBitmap;
          BITMAP       bitmap;
          int          ofs_x, ofs_y;

          hdcMem    = CreateCompatibleDC(hdc);
          oldBitmap = SelectObject(hdcMem, g_wallpaper_bitmap);

          GetObjectA (g_wallpaper_bitmap, bitmap'size, (LPVOID)&bitmap);

          ofs_x = (int)bitmap.bmWidth - (int)bitmap.bmHeight * (int)rect.right / (int)rect.bottom;
          ofs_y = (int)bitmap.bmHeight - (int)bitmap.bmWidth * (int)rect.bottom / (int)rect.right;
          if (ofs_x < 0)
            ofs_x = 0;
          if (ofs_y < 0)
            ofs_y = 0;
          if (ofs_x >= (int)bitmap.bmWidth)
            ofs_x = (int)bitmap.bmWidth - 1;
          if (ofs_y >= (int)bitmap.bmHeight)
            ofs_y = (int)bitmap.bmHeight - 1;

          SetStretchBltMode (hdc, HALFTONE);
          SetBrushOrgEx (hdc, 0, 0, null);
          StretchBlt (hdc,    0,       0,       rect.right,           rect.bottom,
                      hdcMem, ofs_x/2, ofs_y/2, bitmap.bmWidth-ofs_x, bitmap.bmHeight-ofs_y,
                      SRCCOPY);

          SelectObject(hdcMem, oldBitmap);
          DeleteDC(hdcMem);
        }

        EndPaint (hWnd, &s);

        call_default_handler = false;
        window_retcode = 1;  // no further painting necessary
      }
      break;

    case WM_WINDOWPOSCHANGING:
      {
        WINDOWPOS         wpos*, newpos;
        APPLICATION_EVENT e;

        wpos'byte = lParam'byte;

        newpos = *wpos;   // this is the new wanted pos/size

        if ((newpos.flags & (SWP_NOMOVE|SWP_NOSIZE)) == (SWP_NOMOVE|SWP_NOSIZE))
          break;  // none of x, y, cx, cy is filled

        if (newpos.x == -32000)  // minimize
        {
          g_is_minimized = true;
          clear e;   // send size 0 to app for minimize
          e.type = EVENT_MAIN_WINDOW_MINIMIZED;
          app_handler (e);
          break;
        }

        // set old pos/size for now
        wpos->x = main_win_rect.left;
        wpos->y = main_win_rect.top;
        wpos->cx = main_win_rect.right - main_win_rect.left;
        wpos->cy = main_win_rect.bottom - main_win_rect.top;

        if ((newpos.flags & SWP_NOMOVE) != 0)    // x, y not filled
        {
          newpos.x = wpos->x;
          newpos.y = wpos->y;
        }

        if ((newpos.flags & SWP_NOSIZE) != 0)    // cx, cy not filled
        {
          newpos.cx = wpos->cx;
          newpos.cy = wpos->cy;
        }

        if (g_maximize_main_screen)
        {
          // user wants fullscreen
          RECT rect;
          int pad = GetSystemMetrics (SM_CXPADDEDBORDER);
          int dx = GetSystemMetrics(SM_CXFRAME) + pad;
          int dy = GetSystemMetrics(SM_CYFRAME) + pad;

          get_work_area (RECT ' {left   => newpos.x,
                                 top    => newpos.y,
                                 right  => newpos.x + newpos.cx,
                                 bottom => newpos.y + newpos.cy},
                         out rect);

          newpos.x  = rect.left - dx;
          newpos.y  = rect.top  - dy;
          newpos.cx = rect.right  - rect.left + 2*dx;
          newpos.cy = rect.bottom - rect.top  + 2*dy;

          g_maximize_main_screen = false;
        }

        main_window_windowpos = wpos;  // this variable will be updated by user

        if (newpos.x != wpos->x || newpos.y != wpos->y || g_is_minimized)    // x, y changed
        {
          clear e;
          e.type = EVENT_MAIN_WINDOW_MOVE;
          e.x = newpos.x;
          e.y = newpos.y;
          app_handler (e);  // can change all 4 coord (move) or right/bottom (size) in any order
        }

        if (newpos.cx != wpos->cx || newpos.cy != wpos->cy || g_is_minimized)    // cx, cy changed
        {
          clear e;
          e.type = EVENT_MAIN_WINDOW_RESIZE;
          e.x_size = newpos.cx;
          e.y_size = newpos.cy;
          app_handler (e);  // can change all 4 coord (move) or right/bottom (size) in any order
        }

        g_is_minimized = false;   // no further forcing of pos/size messages

        main_window_windowpos = null;

        move_child_windows ( {left   => wpos->x,
                              top    => wpos->y,
                              right  => wpos->x + wpos->cx,
                              bottom => wpos->y + wpos->cy} );

        wpos->flags &= (UINT'max - SWP_NOMOVE - SWP_NOSIZE);   // always move & resize
      }

      if (g_wallpaper_bitmap != 0)
        InvalidateRect(hWnd, null, TRUE);
      break;

    case WM_ACTIVATE:
      {
        DIALOG_INFO* p2 = (DIALOG_INFO*)GetWindowLongPtrA ((HWND)lParam, 0);
        DIALOG_ID other_d = p2 == null ||
                            GetWindow ((HWND)lParam, GW_OWNER) != main_hWnd ||
                            p2->hwnd != (HWND)lParam
                            ? 0 : p2->d;
        if (other_d == 0)
        {
          g_alt_pressed = false; // when doing Alt-Tab and coming back, unpress Alt

          {
            APPLICATION_EVENT e;
            clear e;
            e.key = (int)((wParam & 0xFFFF) != 0);
            e.type = EVENT_ACTIVATE;
            app_handler (e);
          }
        }
      }
      break;

    case WM_SETFOCUS:
      // make all sub-windows half transparent
      treat_child_windows (make_half_transparent, null, null);
      break;

    case WM_MOUSEMOVE:
      {
        int2 x, y;
        bool lbutton, mbutton;

        x = (short)(lParam & 0xFFFF);
        y = (short)(lParam >> 16);
        lbutton = (wParam & (uint)MK_LBUTTON) != 0;
        mbutton = (wParam & (uint)MK_MBUTTON) != 0;
        set_middle_mouse_button (mbutton);

        if (!g_mouse_tracking_active)
        {
          TRACKMOUSEEVENT EventTrack;

          g_mouse_tracking_active = true;

          clear EventTrack;
          EventTrack.cbSize = EventTrack'size;
          EventTrack.dwFlags = TME_LEAVE;
          EventTrack.hwndTrack = hWnd;
          TrackMouseEvent (&EventTrack);
        }

        if (!lbutton)
        {
          g_drag_selected = false;
          set_cursor_shape ();
        }

        if (x != g_previous_mouse_x || y != g_previous_mouse_y)  // Windows sends unecessary move messages
        {
          APPLICATION_EVENT e;

          g_previous_mouse_x = x;
          g_previous_mouse_y = y;

          clear e;
          e.x = x;
          e.y = y;
          e.type = lbutton ? EVENT_MOUSE_DRAG : EVENT_MOUSE_MOVE;
          app_handler (e);
        }
      }
      break;

    case WM_NCHITTEST:
      g_basic_mouse_shape = IDC_ARROW;    // IDC_ARROW or IDC_BEAM

      if (g_drag_selected && g_drag_moved)
      {
        APPLICATION_EVENT e;

        g_user_drop_allowed = false;

        clear e;
        e.x = (short)lParam;
        e.y = (int)(lParam >> 16);
        e.type = EVENT_TEST_DROP_ALLOWED;
        app_handler (e);
      }
      break;

    case WM_SETCURSOR:
      if ((lParam & 0xffff) == HTCLIENT)
      {
        set_cursor_shape ();
        SetClassLongA (hWnd, GCL_HCURSOR, (LONG)g_hcursor_shape);

        call_default_handler = false;
        window_retcode = 1;  // Processed
      }
      break;

    case WM_MOUSELEAVE:     // leave window
      {
        APPLICATION_EVENT e;
        g_mouse_tracking_active = false;
        clear e;
        e.type = EVENT_MOUSE_CANCEL;
        app_handler (e);
      }
      break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
      {
        int  x = (short)(lParam & 0xFFFF);
        int  y = (short)(lParam >> 16);
        APPLICATION_EVENT e;
        clear e;
        e.x = x;
        e.y = y;
        e.type = EVENT_MOUSE_CLICKED;
        e.key = (message == WM_LBUTTONDOWN) ? 1 : 2;
        app_handler (e);
        SetCapture (hWnd);
      }
      break;

    case WM_LBUTTONUP:
      {
        int x = (short)(lParam & 0xFFFF);
        int y = (short)(lParam >> 16);
        APPLICATION_EVENT e;
        g_drag_selected = false;
        set_cursor_shape ();
        clear e;
        e.x = x;
        e.y = y;
        e.type = EVENT_MOUSE_DROP;
        app_handler (e);
        ReleaseCapture();
      }
      break;

    case WM_NCLBUTTONUP:
      g_drag_selected = false;
      set_cursor_shape ();
      break;

#if 0
    case WM_RBUTTONDOWN:
#endif

    case WM_RBUTTONUP:
      {
        int  x = (short)(lParam & 0xFFFF);
        int  y = (short)(lParam >> 16);
        APPLICATION_EVENT e;
        clear e;
        e.x = x;
        e.y = y;
        e.type = EVENT_MOUSE_MENU;
        app_handler (e);
      }
      break;

    case WM_MBUTTONDOWN:
      set_middle_mouse_button (true);
      break;

    case WM_MBUTTONUP:
      set_middle_mouse_button (false);
      break;

    case WM_MOUSEWHEEL:
      {
        const int WHEEL_DELTA = 120;
        APPLICATION_EVENT e;
        clear e;
        e.x = 0;
        e.y = (((int)wParam >> 16) / WHEEL_DELTA);
        e.type = EVENT_MOUSE_WHEEL;
        app_handler (e);
      }
      break;

    case WM_INPUT:
      {
        BYTE     lpb[48];
        UINT     dwSize = lpb'size;
        RAWINPUT raw;

        GetRawInputData ((HANDLE)lParam, RID_INPUT, &lpb, &dwSize, RAWINPUTHEADER'size);
        raw'byte = lpb[0 : raw'size];
        if (raw.header.dwType == RIM_TYPEMOUSE)
        {
          int xPosRelative = raw.data.mouse.lLastX;
          int yPosRelative = raw.data.mouse.lLastY;
          uint buttons = raw.data.mouse.u.s.usButtonFlags;

          if ((raw.data.mouse.usFlags & 1) == 1)       // absolute coords
          {
            xPosRelative -= g_old_xPosRelative;
            g_old_xPosRelative += xPosRelative;
            yPosRelative -= g_old_yPosRelative;
            g_old_yPosRelative += yPosRelative;
          }

          if ((xPosRelative | yPosRelative) != 0)
          {
            APPLICATION_EVENT e;
            clear e;
            e.type = EVENT_RELATIVE_MOUSE_MOVE;
            e.x = xPosRelative;
            e.y = yPosRelative;
            app_handler (e);
          }

          if ((buttons & 1) != 0)
          {
            APPLICATION_EVENT e;
            clear e;
            e.type = EVENT_RELATIVE_MOUSE_LEFT_CLICK;
            app_handler (e);
          }

          if ((buttons & 4) != 0)
          {
            APPLICATION_EVENT e;
            clear e;
            e.type = EVENT_RELATIVE_MOUSE_RIGHT_CLICK;
            app_handler (e);
          }

          if ((buttons & 0x0400) != 0)    // vertical mouse wheel
          {
            const int WHEEL_DELTA = 120;
            APPLICATION_EVENT e;
            clear e;
            e.x = 0;
            e.y = (SHORT)raw.data.mouse.u.s.usButtonData / WHEEL_DELTA;
            e.type = EVENT_MOUSE_WHEEL;
            app_handler (e);
          }

          {
            RECT r;
            GetWindowRect (main_hWnd, &r);
            SetCursorPos(r.left + ((r.right - r.left) >> 1), r.top + ((r.bottom - r.top) >> 1));
          }
        }
      }
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
      {
        APPLICATION_EVENT e;
        int count = (int)(lParam & 32767);
        int i;
        clear e;
        e.key = key_of (message, (uint)wParam);
        e.scancode = (int)((lParam >> 16) & 255);
        e.type = EVENT_KEYBOARD;
        for (i=0; i<count; i++)
          app_handler (e);
      }
      break;

    case WM_USER + 20:   // create dialog
    {
      DIALOG_INFO* ptr = (DIALOG_INFO*)malloc (DIALOG_INFO'size);

      clear *ptr;
      ptr->d                   = (uint)wParam;
      *(INT_PTR*)&ptr->handler = lParam;

      ptr->ShowWindowArg                = SW_SHOW;
//      ptr->border_drag_size              = ui_scale(0);
      ptr->border_size                   = ui_scale(2);
      ptr->dialog_colors                = {0x000000, 0x808080, 0xFF8080, 0xFF8080, 0xE0E0E0, 0xFFFFFF}; // {0x000000, 0x808080, 0x3E3E3E, 0xE0E0E0, 0xFFFFFF};
      ptr->dialog_background_color       = 0xC0C0C0;    // grey background color (0x3E3E3E = grey background color Firestorm)

      strcpy (out ptr->title_font.name, "Tahoma");
      ptr->title_font.height             = ui_scale(16);
//      ptr->title_font.color              = 0x000000;   // black

      ptr->title_height                  = ui_scale(20);
      ptr->dialog_transparency_non_focus = 165;
      ptr->dialog_transparency_focus     = 255;

      strcpy (out ptr->default_font.name, "Microsoft Sans Serif");
      ptr->default_font.height           = ui_scale(12);

      ptr->default_colors                = {0x000000, 0x808080, 0xC0C0C0, 0xC0C0C0, 0xE0E0E0, 0xFFFFFF};
      ptr->default_insert_mode           = true;

      {
        const wstring title = L"\0";
        HWND child_hwnd;

        // sends WM_CREATE which stores g_new_dialog_ptr into the window data.
        g_new_dialog_ptr = ptr;
        child_hwnd = CreateWindowExW (WS_EX_LAYERED,    // uses transparency
                                      &child_class, &title, WS_CLIPCHILDREN | WS_POPUP,
                                      0, 0, 0, 0, main_hWnd, 0, hInstance, null);

        if (child_hwnd != 0)  // WM_CREATE succeeded
        {
          GetWindowRect (child_hwnd, &ptr->rect);
          ShowWindow (child_hwnd, (int)ptr->ShowWindowArg);
        }
      }
    }
    break;

    case WM_USER + 24:   // post_menu_event
      {
        APPLICATION_EVENT e;
        clear e;
        e.type = EVENT_MENU;
        e.id = (short)wParam;
        app_handler (e);
      }
      break;

    case WM_TIMER:
      {
        APPLICATION_EVENT e;
        clear e;
        e.type = EVENT_TIMER;
        app_handler (e);
      }
      break;

    case WM_DEVICECHANGE:
//      trace ("WM_DEVICECHANGE %d %d\n", (int)wParam, (int)lParam);
      switch (wParam)
      {
        case DBT_DEVICEARRIVAL:
        case DBT_DEVICEREMOVECOMPLETE:
          {
            DEV_BROADCAST_HDR* lpdb;
            lpdb'byte = lParam'byte;

//            trace ("device type : %d\n", (int)lpdb->dbch_devicetype);
            
            if (lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
              DEV_BROADCAST_VOLUME* lpdbv;
              char drive;
              lpdbv'byte = lpdb'byte;
              drive = FirstDriveFromMask (lpdbv ->dbcv_unitmask);
//              trace ("drive : %c\n", drive);
              if (wParam == DBT_DEVICEARRIVAL)  // inserting drive
              {
                g_removable_drive = drive;
//                trace ("add drive : %c\n", drive);
              }
              else  // removing drive
              {
                if (g_removable_drive == drive)
                {
//                  trace ("remove drive : %c\n", drive);
                  g_removable_drive = nul;   // no longer exists
                }
              }
            }
          }
          break;

        default:
          break;
      }
      break;

    case WM_COMMAND:
      if ((wParam & 32767) == wParam)
      {
        APPLICATION_EVENT e;
        clear e;
        e.type = EVENT_MENU;
        e.id = (short)wParam;
        app_handler (e);
      }
      break;

    case WM_SYSCOMMAND:
      switch (wParam & 0xFFF0)
      {
        case SC_CLOSE:
          {
            APPLICATION_EVENT e;
            clear e;
            e.type = EVENT_CLOSE_BUTTON;
            app_handler (e);
          }

          call_default_handler = false;
          window_retcode = 0;
          break;

        case SC_MAXIMIZE:
          g_maximize_main_screen = true;
          break;

        case SC_SIZE:
          break;

        default:
          break;
      }
      break;

    case WM_DESTROY:
    {
      APPLICATION_EVENT e;
      clear e;
      e.type = EVENT_END_APPLICATION;
      app_handler (e);
    }
    PostQuitMessage (0);
    break;

    default:
      break;
  }


  if (call_default_handler)
    window_retcode = DefWindowProcW (hWnd, message, wParam, lParam);

  g_dialog_ptr = safeguarded_dialog_ptr;

  return window_retcode;
}

#endif

//--------------------------------------------------------------------------

public void create_main_window (APPLICATION_EVENT_HANDLER application_event_handler)
{
#if WINDOWS
  WNDCLASSEXW  wc;

  const wstring classname = L"C\0";
  const wstring title = L"\0";

  app_handler = application_event_handler;   // user function that handles main window events

  hInstance = GetModuleHandleA (null);
  assert hInstance != 0;

  // to avoid error in RegisterClass() below
  {
    INITCOMMONCONTROLSEX controls;
    controls = {dwSize => controls'size,
                dwICC  => ICC_WIN95_CLASSES};
    assert InitCommonControlsEx(&controls) == TRUE;
  }

  // ------------------------

  // class for main window

  clear wc;
  wc.cbSize = wc'size;
  wc.style = CS_DBLCLKS;    // allows WM_LBUTTONDBLCLK
//  wc.style |= CS_HREDRAW | CS_VREDRAW;  // ensure all client area is redrawn if client area size changes
                                          // don't use this : causes flicker
  wc.lpfnWndProc = MainWindowProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIconA (hInstance, 1000);  // try to load icon from resource file item 1000
  if (wc.hIcon == 0)
    wc.hIcon = LoadIconA (0, IDI_APPLICATION);
//  wc.hCursor       = 0;
//  wc.hbrBackground = GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = &classname;
  assert RegisterClassExW (&wc) != 0;  // fails when manifest is missing (see chapter 8 compiler)

  // ------------------------

  windows.main_hWnd = CreateWindowExW (0, &classname, &title, WS_OVERLAPPEDWINDOW /*|WS_VISIBLE*/,
                                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                       0, 0, hInstance, null);
  assert main_hWnd != 0;

  GetWindowRect (main_hWnd, &main_win_rect);

  // ------------------------

  // class for child window
  clear wc;
  wc.cbSize        = wc'size;
  wc.style         = CS_DBLCLKS;   // allows WM_LBUTTONDBLCLK
  wc.lpfnWndProc   = ChildWindowProc;
//  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 8;    // a pointer
  wc.hInstance     = hInstance;
//  wc.hIcon         = 0;
//  wc.hCursor       = 0;
//  wc.hbrBackground = 0;
//  wc.lpszMenuName  = null;
  wc.lpszClassName = &child_class;
//  wc.hIconSm       = 0;
  assert RegisterClassExW (&wc) != 0;

  // ------------------------

  y_title  = GetSystemMetrics (SM_CYCAPTION);
  x_border = GetSystemMetrics (SM_CXPADDEDBORDER) + GetSystemMetrics (SM_CXFRAME);
  y_border = GetSystemMetrics (SM_CXPADDEDBORDER) + GetSystemMetrics (SM_CYFRAME);


  {
    APPLICATION_EVENT e;
    clear e;
    e.type = EVENT_NEW_APPLICATION;
    app_handler (e);
  }

  // enter the main loop
  for (;;)
  {
    MSG msg;
    int bRet;

    bRet = GetMessageW (&msg, 0, 0, 0);
    if (bRet == 0 || bRet == -1)
      break;

    TranslateMessage (&msg);
    DispatchMessageW (&msg);
  }

#elif ANDROID
  _unused application_event_handler;

#else
  bad

#endif
}

//--------------------------------------------------------------------------
#end unsafe
//--------------------------------------------------------------------------
