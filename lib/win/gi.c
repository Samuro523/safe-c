
// gi.c

use ../thread;
use ../tracing;
use windows;

const bool debug = false;

//------------------------------------------------------------------------------
#begin unsafe
//------------------------------------------------------------------------------

volatile int status;     // 0 = not initialized, +1 = initialization success, -1 = initialization failure

//------------------------------------------------------------------------------

const uint MY_EVENT_ASK_MOUSE_SHAPE_CHANGE = 35603;


struct WINDOW
{
  bool visible;
  int  width;       // size of logical window stored by GI layer (actual window can have different size)
  int  height;

  bool resize_allowed;
  int  min_width;
  int  max_width;
  int  min_height;
  int  max_height;

  int  new_width;   // for resize (will be copied into width/height before application receives event)
  int  new_height;

  CLOSE_BUTTON_STATE          state;

  TREAT_MAIN_WINDOW_MESSAGE   treat_main_window_message;
  TREAT_QUEUED_WINDOW_MESSAGE treat_queued_window_message;
  RANDOM_ADD_SEED             random_add_seed;
}


// event masks
const int EVENT_KEYBOARD =  1;   // can be virtual user-event
const int EVENT_SIGNAL   =  2;   // user-defined application event (ex: for repainting screen)
const int EVENT_MOUSE    =  4;
const int EVENT_RESIZE   =  8;

struct EVENT
{
  HWND id;
  int  mask;
}


const int KEYBOARD_BUFFER_SIZE = 64;

struct KEYBOARD
{
  int  buffer[KEYBOARD_BUFFER_SIZE];
  int  put;     /* next free entry */
  int  get;     /* next key to remove */

  bool shift_pressed;
  bool control_pressed;
  bool alt_pressed;
}


struct SIGNAL
{
  int buffer[MAX_SIGNALS];   // each signal nr is only present once in the buffer
                             // they are treated in FIFO order
  int  put;     /* next free entry */
  int  get;     /* next key to remove */
}


struct MOUSE
{
  short x;
  short y;
  bool  left;
  bool  middle;
  bool  right;
  short wheel;
  bool  mouse_tracking_active;

  CALLBACK_MOUSE_SHAPE callback_mouse_shape;
}


struct GI
{
  SHARED_OBJECT shared;
  WINDOW        window;
  EVENT         event;
  KEYBOARD      keyboard;
  MOUSE         mouse;
  SIGNAL        signal;
}

GI gi;

//------------------------------------------------------------------------------

public void gi_move (int x, int y, int width, int height)
{
  enter_shared_object (ref gi.shared);
  gi.window.width = width;
  gi.window.height = height;
  leave_shared_object (ref gi.shared);

  MoveWindow (main_hWnd, x, y, width, height, TRUE);
}

//------------------------------------------------------------------------------

public void gi_set_title (string title)
{
  string^ p = new string (title'length+1);
  p^[0:title'length] = title;
  p^[title'length] = nul;
  SetWindowTextA (main_hWnd, &p^[0]);
  free p;
}

//------------------------------------------------------------------------------

// make main window visible/invisible

public void gi_set_visible (bool visible)
{
  if (visible == gi.window.visible)
    return;

  if (visible)
  {
    ShowWindow (main_hWnd, SW_SHOW);   // start WM_PAINT messages
    SetForegroundWindow (main_hWnd);   // put main window in foreground
  }
  else
  {
    ShowWindow (main_hWnd, SW_HIDE);
  }

  gi.window.visible = visible;
}

//------------------------------------------------------------------------------

public void gi_set_resize_range (int min_width, int max_width, int min_height, int max_height)
{
  enter_shared_object (ref gi.shared);

  gi.window.resize_allowed = true;
  gi.window.min_width  = min_width;
  gi.window.max_width  = max_width;
  gi.window.min_height = min_height;
  gi.window.max_height = max_height;

  leave_shared_object (ref gi.shared);
}

//------------------------------------------------------------------------------

public void gi_set_resize_off ()
{
  gi.window.resize_allowed = false;
}

//------------------------------------------------------------------------------

// query new window size after EVENT_RESIZE (note: this is a window size, not a user area size)
// (after the call the new size applies)

public void gi_get_new_size (out int width, out int height)
{
  width = gi.window.new_width;
  height = gi.window.new_height;

  gi.window.width = width;
  gi.window.height = height;
}

//------------------------------------------------------------------------------

public void gi_set_close_button (CLOSE_BUTTON_STATE state)
{
  gi.window.state = state;
}

//------------------------------------------------------------------------------

public CLOSE_BUTTON_STATE gi_get_close_button_state ()
{
  return gi.window.state;
}

//------------------------------------------------------------------------------

public TREAT_QUEUED_WINDOW_MESSAGE gi_get_queued_window_message_hook ()
{
  return gi.window.treat_queued_window_message;
}

//------------------------------------------------------------------------------

public void gi_set_queued_window_message_hook (TREAT_QUEUED_WINDOW_MESSAGE func)
{
  gi.window.treat_queued_window_message = func;
}

//------------------------------------------------------------------------------

public TREAT_MAIN_WINDOW_MESSAGE gi_get_main_window_message_hook ()
{
  return gi.window.treat_main_window_message;
}

//------------------------------------------------------------------------------

public void gi_set_main_window_message_hook (TREAT_MAIN_WINDOW_MESSAGE func)
{
  gi.window.treat_main_window_message = func;
}

//------------------------------------------------------------------------------

public void gi_set_random_add_seed (RANDOM_ADD_SEED func)
{
  gi.window.random_add_seed = func;
}

//------------------------------------------------------------------------------


/****************/
/**  KEYBOARD  **/
/****************/

public void gi_put_key (int key)
{
  int put9;

  enter_shared_object (ref gi.shared);

  put9 = gi.keyboard.put + 1;
  if (put9 == KEYBOARD_BUFFER_SIZE)
    put9 = 0;

  if (gi.keyboard.get == put9)    /* buffer is full */
  {
    leave_shared_object (ref gi.shared);
    return;
  }

  gi.keyboard.buffer[gi.keyboard.put] = key;
  gi.keyboard.put = put9;

  gi.event.mask |= EVENT_KEYBOARD;

  leave_shared_object (ref gi.shared);

  SetEvent (gi.event.id);
}

/************************************************************************/

/* note: this function is called from another thread */

public bool gi_is_key_available ()
{
  return gi.keyboard.put != gi.keyboard.get;
}

/************************************************************************/

/* read a key from the keyboard */
/* note: this function is called from another thread */

public int gi_get_key ()
{
  int get9, ch;

  for (;;)
  {
    if (gi.keyboard.put != gi.keyboard.get)
    {
      enter_shared_object (ref gi.shared);

      ch = gi.keyboard.buffer[gi.keyboard.get];
      get9 = gi.keyboard.get + 1;
      if (get9 == KEYBOARD_BUFFER_SIZE)
        get9 = 0;
      gi.keyboard.get = get9;

      if (gi.keyboard.get == gi.keyboard.put)
        gi.event.mask &= (~EVENT_KEYBOARD);

      leave_shared_object (ref gi.shared);

      break;
    }

    gi_wait (WAIT_FOR_KEYBOARD, 0);
  }

  return ch;
}

/************************************************************************/

/*************/
/** SIGNALS **/
/*************/

public void gi_set_signal (int signal_nr)
{
  int put9, index;

  enter_shared_object (ref gi.shared);


  /* test if signal already present in buffer */

  index = gi.signal.get;

  while (index != gi.signal.put)
  {
    if (gi.signal.buffer[index] == signal_nr)   /* found */
    {
      leave_shared_object (ref gi.shared);   // already present : do nothing
      return;
    }

    index++;
    if (index == MAX_SIGNALS)
      index = 0;
  }


  /* add event at end-of-buffer */

  put9 = gi.signal.put + 1;
  if (put9 == MAX_SIGNALS)
    put9 = 0;

  if (gi.signal.get == put9)    /* buffer is full */
  {
    leave_shared_object (ref gi.shared);
    return;
  }

  gi.signal.buffer[gi.signal.put] = signal_nr;
  gi.signal.put = put9;

  gi.event.mask |= EVENT_SIGNAL;

  leave_shared_object (ref gi.shared);

  SetEvent (gi.event.id);
}

/************************************************************************/

/* note: this function is called from another thread */

public bool gi_is_signal_available ()
{
  return gi.signal.put != gi.signal.get;
}

/************************************************************************/

/* read a signal */
/* note: this function is called from another thread */

public int gi_get_signal ()
{
  int get9, ch;

  for (;;)
  {
    if (gi.signal.put != gi.signal.get)
    {
      enter_shared_object (ref gi.shared);

      ch = gi.signal.buffer[gi.signal.get];
      get9 = gi.signal.get + 1;
      if (get9 == MAX_SIGNALS)
        get9 = 0;
      gi.signal.get = get9;

      if (gi.signal.get == gi.signal.put)
        gi.event.mask &= (~EVENT_SIGNAL);

      leave_shared_object (ref gi.shared);
      break;
    }

    gi_wait (WAIT_FOR_SIGNAL, 0);
  }

  return ch;
}


/************************************************************************/

/***********/
/** MOUSE **/
/***********/

package Mouse

  HCURSOR mouse_cursor_normal, mouse_cursor_wait, mouse_cursor_cross, mouse_cursor_hand;
  HCURSOR current_mouse_cursor;   // either normal, cross or hand

end Mouse;

/**********************************************************************/

public void gi_get_mouse_coordinates (out int x, out int y)
{
  enter_shared_object (ref gi.shared);
  x = gi.mouse.x;
  y = gi.mouse.y;
  leave_shared_object (ref gi.shared);
}

/************************************************************************/

public void gi_get_mouse_buttons (out bool left, out bool middle, out bool right)
{
  enter_shared_object (ref gi.shared);
  left   = gi.mouse.left;
  middle = gi.mouse.middle;
  right  = gi.mouse.right;
  leave_shared_object (ref gi.shared);
}

/************************************************************************/

public void gi_get_mouse_wheel (out int wheel)   /* relative wheel move (+ or -) */
{
  enter_shared_object (ref gi.shared);
  wheel = gi.mouse.wheel;
  gi.mouse.wheel = 0;
  leave_shared_object (ref gi.shared);
}

/************************************************************************/
/************************************************************************/

// HOURGLASS

package HOURGLASS_DATA

  bool hourglass_active;     /* true = hourglass active, false = normal cursor */
  bool hourglass_disabled;   /* master switch, overrides all */

  HCURSOR old_cursor = -1;

end HOURGLASS_DATA;

/************************************************************************/

// returns non-zero if shape was changed

HCURSOR intern_update_cursor_shape ()
{
  HCURSOR new_cursor;

  new_cursor = hourglass_active ? mouse_cursor_wait : current_mouse_cursor;

  if (new_cursor == old_cursor)   // no change needed
    return 0;

  old_cursor = new_cursor;

  SetClassLongA (main_hWnd, GCL_HCURSOR, (int)new_cursor);

  return new_cursor;
}

/************************************************************************/

/* use_hourglass: false = normal, true = hourglass */
/* called by win.c layer to enable/disable hourglass */

public void gi_set_mouse_arrow (bool use_hourglass)
{
  HCURSOR new_cursor;
  bool f = use_hourglass;

  if (hourglass_disabled)
    f = false;

  hourglass_active = f;

  new_cursor = intern_update_cursor_shape();
  if (new_cursor != 0)
    PostMessageW (main_hWnd, MY_EVENT_ASK_MOUSE_SHAPE_CHANGE, (uint)new_cursor, 0);
}

/************************************************************************/

/* true = set_mouse_arrow() works (default) */
/* false = set_mouse_arrow() always sets normal shape */

public void gi_set_hourglass_when_busy (bool enable)
{
  hourglass_disabled = !enable;
}

/************************************************************************/

public void gi_set_callback_mouse_shape (CALLBACK_MOUSE_SHAPE fcallback_mouse_shape)
{
  if (fcallback_mouse_shape == null)
    current_mouse_cursor = mouse_cursor_normal;

  gi.mouse.callback_mouse_shape = fcallback_mouse_shape;

  gi_set_mouse_arrow (hourglass_active);
}

/************************************************************************/
/************************************************************************/

// CARET

package CARET_DATA

  bool win_caret;        /* true=setfocus, false=killfocus */
  bool gi_caret;         /* true=gi_enable_caret, false=gi_disable_caret */
  int  caret_height;     /* current caret height (0 = not created) */
  bool caret_visible;    /* controls ShowCaret() / HideCaret() */
  int  caret_new_x;      /* stored */
  int  caret_new_y;      /* stored */
  int  caret_new_height; /* stored */

end CARET_DATA;

/************************************************************************/

void activate_caret ()
{
  if (win_caret & gi_caret)   /* both windows and the application want a caret */
  {
    if (caret_new_height != caret_height)   /* current caret_height is zero (no caret) or different */
    {
      CreateCaret (main_hWnd, 0, 2*GetSystemMetrics(SM_CXBORDER), caret_new_height);
      caret_height = caret_new_height;
      caret_visible = false;             /* a new caret is always invisible */
    }

    SetCaretPos (caret_new_x, caret_new_y);

    if (!caret_visible)
    {
      ShowCaret (main_hWnd);
      caret_visible = true;
    }
  }
}

/* these functions are called during a repaint */

public void gi_enable_caret (int x, int y, int height)
{
  caret_new_x      = x;
  caret_new_y      = y;
  caret_new_height = height;

  gi_caret = true;
  activate_caret();
}

public void gi_disable_caret ()
{
  gi_caret = false;

  if (caret_visible)
  {
    HideCaret (main_hWnd);
    caret_visible = false;
  }
}

/************************************************************************/

/* wait until some event occurs on a set of devices or until timeout.     */
/* (this function is used to avoid taking too much CPU time with polling) */
/* 'wait'   : ORed events we want to wait for.                            */
/* 'timeout': the timeout in 1/1000 secs (if WAIT_FOR_TIMEOUT).           */
/* returns: mask of events that occured. */

public int gi_wait (int wait, uint timeout)
{
  const DWORD INFINITE = 0xFFFFFFFF;
  const int   CANCEL   = ~(EVENT_KEYBOARD | EVENT_SIGNAL);

  uint  start, elapsed;
  DWORD tm, rc;
  int   occured;

  occured = wait & gi.event.mask;
  if (occured != 0)    // some of these events occured
  {
enter_shared_object (ref gi.shared);
    gi.event.mask &= (~(occured & CANCEL));   // cancel all events we waited for (except keyboard & signals)
leave_shared_object (ref gi.shared);
    return occured;
  }

  if ((wait & WAIT_FOR_TIMEOUT) == 0)   // no timeout
  {
    for (;;)
    {
      rc = WaitForSingleObject (gi.event.id, INFINITE);
      if (rc == WAIT_TIMEOUT)
        return WAIT_FOR_TIMEOUT;

      if (rc != 0)   // some error
        abort;

      occured = wait & gi.event.mask;
      if (occured != 0)    // some of these events occured
      {
enter_shared_object (ref gi.shared);
        gi.event.mask &= (~(occured & CANCEL));   // cancel all events we waited for (except keyboard & signals)
leave_shared_object (ref gi.shared);
        return occured;
      }
    }
  }
  else    // with timeout
  {
    start = GetTickCount();

    rc = WaitForSingleObject (gi.event.id, timeout);
    if (rc == WAIT_TIMEOUT)
      return WAIT_FOR_TIMEOUT;

    if (rc != 0)   // some error
      abort;

    for (;;)
    {
      occured = wait & gi.event.mask;
      if (occured != 0)    // some of these events occured
      {
enter_shared_object (ref gi.shared);
        gi.event.mask &= (~(occured & CANCEL));   // cancel all events we waited for (except keyboard & signals)
leave_shared_object (ref gi.shared);
        return occured;
      }

      elapsed = GetTickCount() - start;
      if (elapsed >= timeout)    // time elapsed
        return WAIT_FOR_TIMEOUT;
      tm = timeout - elapsed;

      rc = WaitForSingleObject (gi.event.id, tm);
      if (rc == WAIT_TIMEOUT)
        return WAIT_FOR_TIMEOUT;

      if (rc != 0)   // some error
        abort;
    }
  }

  abort;
}

/************************************************************************/

/* wait : WAIT_FOR_KEYBOARD | WAIT_FOR_SIGNAL | WAIT_FOR_MOUSE | WAIT_FOR_RESIZE */

public bool gi_are_events_pending (int wait)
{
  return (wait & gi.event.mask) != 0;
}

/************************************************************************/

public void tingle_mouse_event ()
{
  gi.event.mask |= EVENT_MOUSE;
  SetEvent (gi.event.id);
}

/************************************************************************/

[callback]
LRESULT MainWndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

if (debug) trace ("gi.c : MainWndProc() received wnd=%x message=%x wparam=%x lparam=%x\n", hWnd, message, wParam, lParam);

  {
    TREAT_MAIN_WINDOW_MESSAGE f;

    f = gi.window.treat_main_window_message;

    if (f != null)
    {
      LRESULT result;

      if (f (hWnd, message, wParam, lParam, out result))
        return result;
    }
  }


  switch (message)
  {
    //==============================================

    case WM_CREATE:
      enter_shared_object (ref gdi.shared);

      gi_create_objects ();

      leave_shared_object (ref gdi.shared);
      break;

    //==============================================

    case WM_GETMINMAXINFO:
      {
        MINMAXINFO *p;

        p'byte = lParam'byte;

        enter_shared_object (ref gi.shared);

        if (gi.window.resize_allowed)
        {
          p->ptMinTrackSize.x = gi.window.min_width;
          p->ptMaxTrackSize.x = gi.window.max_width;
          p->ptMinTrackSize.y = gi.window.min_height;
          p->ptMaxTrackSize.y = gi.window.max_height;
        }
        else
        {
          p->ptMinTrackSize.x = gi.window.width;
          p->ptMaxTrackSize.x = gi.window.width;
          p->ptMinTrackSize.y = gi.window.height;
          p->ptMaxTrackSize.y = gi.window.height;
        }

        leave_shared_object (ref gi.shared);
      }
      break;

    //==============================================

    case WM_SIZE:
      if (IsIconic (main_hWnd) == 0)   // not small icon
      {
        RECT r;

        GetWindowRect (hWnd, &r);

        enter_shared_object (ref gi.shared);
        gi.window.new_width  = r.right - r.left;
        gi.window.new_height = r.bottom - r.top;
        gi.event.mask |= EVENT_RESIZE;
        leave_shared_object (ref gi.shared);

        SetEvent (gi.event.id);
      }
      break;

    //==============================================

    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC         hdc;

        enter_shared_object (ref gdi.shared);

        hdc = BeginPaint (hWnd, &ps);
        gi_paint_screen (hdc, ps);
        EndPaint (hWnd, &ps);

        leave_shared_object (ref gdi.shared);
      }
      break;

    //==============================================

    case WM_KILLFOCUS:

      enter_shared_object (ref gdi.shared);

      win_caret = false;
      caret_visible = false;
      if (caret_height != 0)   /* caret was created */
      {
        DestroyCaret();
        caret_height = 0;
      }

      leave_shared_object (ref gdi.shared);

      return DefWindowProcW (hWnd, message, wParam, lParam);

    //==============================================

    case WM_SETFOCUS:     /* windows has again input focus */

      enter_shared_object (ref gdi.shared);

      win_caret = true;
      activate_caret();

      leave_shared_object (ref gdi.shared);

      gi.keyboard.shift_pressed   = false;
      gi.keyboard.control_pressed = false;
      gi.keyboard.alt_pressed     = false;

      return DefWindowProcW (hWnd, message, wParam, lParam);

    //==============================================

    case WM_SYSCOMMAND:
      switch (wParam)
      {
        case SC_CLOSE:
          switch (gi.window.state)
          {
            case _INACTIVE:  // closing window does not work
              break;

            case _CONVERT_TO_ESCAPE:  // closing window is converted into 10 ESCAPE keys
              {
                int i;
                for (i=0; i<10; i++)
                  gi_put_key (C_KEY_ESCAPE);
              }
              break;

            default:
              return DefWindowProcW (hWnd, message, wParam, lParam);
          }
          break;

        case SC_MAXIMIZE:
        case SC_SIZE:
          if (!gi.window.resize_allowed)
            break;   /* ignore */
          return DefWindowProcW (hWnd, message, wParam, lParam);

        default:
          return DefWindowProcW (hWnd, message, wParam, lParam);
      }
      break;

    //==============================================

    case WM_DESTROY:
      enter_shared_object (ref gdi.shared);

      gi_disable_caret ();

      gi_destroy_objects ();

      leave_shared_object (ref gdi.shared);

      PostQuitMessage (0);
      break;

    //==============================================

    default:
if (debug) trace ("gi.c : -> to be treated by DefWindowProc()\n");
      return DefWindowProcW (hWnd, message, wParam, lParam);
  }

  return 0;
}

/************************************************************************/

// this function returns only to terminate the process

void process_windows_messages ()
{
  int  ret;
  MSG  msg;
  bool done;

  for (;;)
  {
    ret = GetMessageW (&msg, 0, 0, 0);
    if (ret <= 0)  // WM_QUIT(0) or error(-1)
    {
if (debug) trace ("gi.c : GetMessage() returned %d\n", ret);
      break;
    }

if (debug) trace ("gi.c : GetMessage() message wnd=%x message=%x wparam=%x lparam=%x\n", msg.hwnd, msg.message, msg.wParam, msg.lParam);

    done = false;


    {
      RANDOM_ADD_SEED f;

      f = gi.window.random_add_seed;

      if (f != null)
        f ((uint)main_hWnd ^ (uint)msg.hwnd ^ (uint)msg.message ^ (uint)msg.wParam ^ (uint)msg.lParam ^ (uint)msg.time);
    }


    {
      TREAT_QUEUED_WINDOW_MESSAGE f;

      f = gi.window.treat_queued_window_message;

      if (f != null)
        done = f (msg.hwnd, msg.message, msg.wParam, (int)msg.lParam);
    }


    if (!done)
    {
      switch (msg.message)
      {

#if 0
 $$$        case WM_KEYUP:
 $$$        case WM_SYSKEYUP:
 what if user activates another window ?  will the key never be depressed again ?
 what if he presses shift and unpresses shift before releasing the key ?
#endif

        case WM_KEYUP:
        case WM_SYSKEYUP:
          switch (msg.wParam)
          {
            case VK_SHIFT:
              gi.keyboard.shift_pressed = false;
              break;

            case VK_CONTROL:
              gi.keyboard.control_pressed = false;
              break;

            case VK_MENU:
              gi.keyboard.alt_pressed = false;
              break;

            default:
              break;
          }
          break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
          {
            int count, i;

            count = (int)msg.lParam & 32767;

            for (i=0; i<count; i++)
            {
              switch (msg.wParam)
              {
                case VK_SHIFT:
                  gi.keyboard.shift_pressed = true;
                  break;

                case VK_CONTROL:
                  gi.keyboard.control_pressed = true;
                  break;

                case VK_MENU:
                  gi.keyboard.alt_pressed = true;
                  break;

                case VK_TAB:
                  if (gi.keyboard.shift_pressed)
                    gi_put_key (C_KEY_SHIFT_TAB);
                  else if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_TAB);
                  else
                    gi_put_key (C_KEY_TAB);
                  done = true;
                  break;

                case VK_RETURN:
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_ENTER);
                  else
                    gi_put_key (C_KEY_ENTER);
                  done = true;
                  break;

                case VK_BACK:
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_BACKSPACE);
                  else
                    gi_put_key (C_KEY_BACKSPACE);
                  done = true;
                  break;

                case VK_ESCAPE:
                  gi_put_key (C_KEY_ESCAPE);
                  done = true;
                  break;

                case VK_PRIOR:   /* page up   */
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_PAGE_UP);
                  else if (gi.keyboard.alt_pressed)
                    gi_put_key (C_KEY_ALT_PAGE_UP);
                  else
                    gi_put_key (C_KEY_PAGE_UP);
                  done = true;
                  break;

                case VK_NEXT:    /* page down */
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_PAGE_DOWN);
                  else if (gi.keyboard.alt_pressed)
                    gi_put_key (C_KEY_ALT_PAGE_DOWN);
                  else
                    gi_put_key (C_KEY_PAGE_DOWN);
                  done = true;
                  break;

                case VK_HOME:
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_HOME);
                  else if (gi.keyboard.alt_pressed)
                    gi_put_key (C_KEY_ALT_HOME);
                  else
                    gi_put_key (C_KEY_HOME);
                  done = true;
                  break;

                case VK_END:
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_END);
                  else if (gi.keyboard.alt_pressed)
                    gi_put_key (C_KEY_ALT_END);
                  else
                    gi_put_key (C_KEY_END);
                  done = true;
                  break;

                case VK_UP:      /* cursor up    */
                  gi_put_key (C_KEY_CURSOR_UP);
                  done = true;
                  break;

                case VK_DOWN:    /* cursor down  */
                  gi_put_key (C_KEY_CURSOR_DOWN);
                  done = true;
                  break;

                case VK_LEFT:    /* cursor left  */
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_CURSOR_LEFT);
                  else
                    gi_put_key (C_KEY_CURSOR_LEFT);
                  done = true;
                  break;

                case VK_RIGHT:   /* cursor right */
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_CURSOR_RIGHT);
                  else
                    gi_put_key (C_KEY_CURSOR_RIGHT);
                  done = true;
                  break;

                case VK_INSERT:
                  if (gi.keyboard.shift_pressed)
                    gi_put_key ((int)'V'-(int)'A'+1);     /* CTRL-V : paste */
                  else if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_INSERT);
                  else if (gi.keyboard.alt_pressed)
                    gi_put_key (C_KEY_ALT_INSERT);
                  else
                    gi_put_key (C_KEY_INSERT);
                  done = true;
                  break;

                case VK_DELETE:
                  if (gi.keyboard.control_pressed)
                    gi_put_key (C_KEY_CTRL_DELETE);
                  else if (gi.keyboard.alt_pressed)
                    gi_put_key (C_KEY_ALT_DELETE);
                  else
                    gi_put_key (C_KEY_DELETE);
                  done = true;
                  break;

                case VK_F1:
                case VK_F2:
                case VK_F3:
                case VK_F4:
                case VK_F5:
                case VK_F6:
                case VK_F7:
                case VK_F8:
                case VK_F9:
                case VK_F10:
                  if ((int)msg.wParam == VK_F4 && gi.keyboard.alt_pressed)
                  {
                    /* VK_F4 will cause message (WM_SYSCOMMAND, SC_CLOSE, 0) */
                    /* to be posted.                                          */
                  }
                  else
                  {
                    if (gi.keyboard.shift_pressed)
                      gi_put_key (((int)C_KEY_SHIFT_F10) + VK_F10 - (int)msg.wParam);
                    else if (gi.keyboard.control_pressed)
                      gi_put_key (((int)C_KEY_CTRL_F10) + VK_F10 - (int)msg.wParam);
                    else if (gi.keyboard.alt_pressed)
                      gi_put_key (((int)C_KEY_ALT_F10) + VK_F10 - (int)msg.wParam);
                    else
                      gi_put_key (((int)C_KEY_F10) + VK_F10 - (int)msg.wParam);
                    done = true;
                  }
                  break;

                case VK_F11:
                case VK_F12:
                  if (gi.keyboard.shift_pressed)
                    gi_put_key (((int)C_KEY_SHIFT_F12) + VK_F12 - (int)msg.wParam);
                  else if (gi.keyboard.control_pressed)
                    gi_put_key (((int)C_KEY_CTRL_F12) + VK_F12 - (int)msg.wParam);
                  else if (gi.keyboard.alt_pressed)
                    gi_put_key (((int)C_KEY_ALT_F12) + VK_F12 - (int)msg.wParam);
                  else
                    gi_put_key (((int)C_KEY_F12) + VK_F12 - (int)msg.wParam);
                  done = true;
                  break;

                default:
                  if (gi.keyboard.alt_pressed)
                  {
                    if (msg.wParam >= (uint)'A' && msg.wParam <= (uint)'Z')
                    {
                      gi_put_key (C_KEY_ALT_A - ((int)msg.wParam - (int)'A'));
                      done = true;
                    }
                  }
                  break;
              }  /* end switch */
            }  /* for */
          }
          break;

        case WM_CHAR:
          {
            gi_put_key ((int)msg.wParam);
          }
          break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MOUSEMOVE:

          enter_shared_object (ref gi.shared);

          gi.mouse.x         = (short)(msg.lParam & 0xFFFF);
          gi.mouse.y         = (short)(msg.lParam >> 16);
          gi.mouse.left      = ((int)msg.wParam & MK_LBUTTON) != 0;
          gi.mouse.middle    = ((int)msg.wParam & MK_MBUTTON) != 0;
          gi.mouse.right     = ((int)msg.wParam & MK_RBUTTON) != 0;

          gi.event.mask |= EVENT_MOUSE;

          leave_shared_object (ref gi.shared);


          SetEvent (gi.event.id);


          if (!gi.mouse.mouse_tracking_active)
          {
            TRACKMOUSEEVENT EventTrack;

            gi.mouse.mouse_tracking_active = true;

            clear EventTrack;
            EventTrack.cbSize = EventTrack'size;
            EventTrack.dwFlags = TME_LEAVE;
            EventTrack.hwndTrack = main_hWnd;
            TrackMouseEvent (&EventTrack);
          }

          if (msg.message == WM_MOUSEMOVE)
          {
            CALLBACK_MOUSE_SHAPE p = gi.mouse.callback_mouse_shape;
            if (p != null)
            {
              switch ((p)(gi.mouse.x, gi.mouse.y))
              {
                case 1:
                  current_mouse_cursor = mouse_cursor_normal;
                  break;
                case 2:
                  current_mouse_cursor = mouse_cursor_cross;
                  break;
                case 3:
                  current_mouse_cursor = mouse_cursor_hand;
                  break;
                default:
                  abort;
              }

              intern_update_cursor_shape();
            }
          }

          break;


        case WM_MOUSELEAVE:

          enter_shared_object (ref gi.shared);

          gi.mouse.x         = -1;
          gi.mouse.y         = -1;
          gi.mouse.left      = false;
          gi.mouse.middle    = false;
          gi.mouse.right     = false;

          gi.event.mask |= EVENT_MOUSE;

          leave_shared_object (ref gi.shared);

          gi.mouse.mouse_tracking_active = false;


          SetEvent (gi.event.id);

          break;


        case WM_MOUSEWHEEL:
          {
            const int WHEEL_DELTA = 120;

            if (((int)msg.wParam & (MK_SHIFT | MK_CONTROL)) != 0)
              break;

            enter_shared_object (ref gi.shared);
            gi.mouse.wheel += (short)(((int)msg.wParam >> 16) / WHEEL_DELTA);
            gi.event.mask |= EVENT_MOUSE;
            leave_shared_object (ref gi.shared);

            SetEvent (gi.event.id);
          }
          break;

        case MY_EVENT_ASK_MOUSE_SHAPE_CHANGE:
          SetCursor ((HCURSOR)msg.wParam);
          break;        
          
        default:
          break;

      }  // end switch
    }

    if (!done)
    {
      TranslateMessage (&msg);
      DispatchMessageW (&msg);
    }
  }  // end while
}

/************************************************************************/

int init_win ()
{
  HINSTANCE    hInst;
  WNDCLASSEXW  wc;
  const wstring classname = L"C\0";
  const wstring title = L"\0";

  hInst = GetModuleHandleW (null);
  assert hInst != 0;
  
  // to avoid error in RegisterClass() below  
  {
    INITCOMMONCONTROLSEX controls;
    clear controls;
    controls.dwSize = controls'size;
    controls.dwICC = ICC_WIN95_CLASSES;
    assert InitCommonControlsEx(&controls) == TRUE;
  }

  // create event

  gi.event.id = CreateEventA (null, FALSE, FALSE, null);
  if (gi.event.id == 0)
    return -1;


  // initialize mouse

  gi.mouse.x = -1;
  gi.mouse.y = -1;

  mouse_cursor_normal = LoadCursorA (0, IDC_ARROW);
  mouse_cursor_wait   = LoadCursorA (0, IDC_WAIT);
  mouse_cursor_cross  = LoadCursorA (0, IDC_CROSS);
  mouse_cursor_hand   = LoadCursorA (0, 32649);

  current_mouse_cursor = mouse_cursor_normal;


  // register window class

  clear wc;
  wc.cbSize = wc'size;
  wc.style = CS_HREDRAW | CS_VREDRAW;  // ensure all client area is redrawn if client area size changes
  wc.lpfnWndProc     = MainWndProc;
  wc.cbClsExtra      = 8;
  wc.cbWndExtra      = 8;
  wc.hInstance       = hInst;

  wc.hIcon = LoadIconA (hInst, 1000);  // try to load icon from resource file item 1000
  if (wc.hIcon == 0)
    wc.hIcon = LoadIconA (0, IDI_APPLICATION);

  wc.hCursor         = mouse_cursor_normal;
  wc.hbrBackground   = 0;    // no default background !
  wc.lpszMenuName    = null;
  wc.lpszClassName   = &classname;    // some window class

  if (RegisterClassExW (&wc) == 0)
    return -1;


  // create main window

  main_hWnd = CreateWindowExW
       (0,
        &classname,
        &title,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        x => CW_USEDEFAULT,
        y => (int)SW_HIDE,         // window is initially hidden
        width => 0,
        height => 0,
        parent => 0,
        0,
        hInst,
        null);

  if (main_hWnd == 0)
    return -1;

  ShowWindow (main_hWnd, SW_HIDE);    /* SW_HIDE is ignored by Windows on first call */

  return 0;
}

//------------------------------------------------------------------------------

void main_window_thread ()
{
  if (init_win () < 0)
  {
    status = -1;
    return;
  }

  status = 1;
  process_windows_messages ();  // returns when user closes window.

  ExitProcess (0);
}

//------------------------------------------------------------------------------

public int gi_init ()
{
  if (status == 0)     // has never been called
  {
    if (run main_window_thread() < 0)
      status = -1;     // starting thread failed

    while (status == 0)   // wait until thread has started
      sleep 0.05;
  }

  if (status < 0)
    return -1;

  return 0;
}

//------------------------------------------------------------------------------

// closes the application

public void gi_end ()
{
  if (status > 0)
  {
    /* tell the main window it can destroy itself */
    while (PostMessageW (main_hWnd, WM_DESTROY, 0, 0) == 0)
      sleep 0.25;
  }
}

//------------------------------------------------------------------------------

// additional systray function

package SYSTRAY

  UINT                      s_uTaskbarRestart;
  TREAT_MAIN_WINDOW_MESSAGE previous;

  bool message_hook (HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, out LRESULT result);

end SYSTRAY;


package body SYSTRAY

  bool icon_visible;

  const uint WM_TRAY_ICONE = WM_USER + 31400;
  const uint TRAY_UID      = 100;

  int create_tray_icon ()
  {
    NOTIFYICONDATA tnid;
    int            b;
    char           title[128+1];

    clear tnid;
    tnid.cbSize = (DWORD)tnid'size;
    tnid.hWnd = main_hWnd;
    tnid.uID = TRAY_UID;
    tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tnid.uCallbackMessage = WM_TRAY_ICONE;
    tnid.hIcon = LoadIconA (GetModuleHandleA(null), 1000);

    GetWindowTextA (main_hWnd, &title, title'size);
    tnid.szTip = title[0:128];

    b = Shell_NotifyIconA (NIM_ADD, &tnid);
    return b != 0 ? 0 : -1;
  }

  int remove_tray_icon ()
  {
    NOTIFYICONDATA tnid;
    int            b;

    clear tnid;
    tnid.cbSize = (DWORD)tnid'size;
    tnid.hWnd = main_hWnd;
    tnid.uID = TRAY_UID;

    b = Shell_NotifyIconA (NIM_DELETE, &tnid);
    return b != 0 ? 0 : -1;
  }


  public bool message_hook (HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, out LRESULT result)
  {
    if (hwnd == main_hWnd)
    {
      if (message == s_uTaskbarRestart && icon_visible)      /* taskbar was restarted */
      {
        create_tray_icon();
      }

      if (message == WM_SYSCOMMAND && (wparam & 0xFFF0) == SC_MINIMIZE)
      {
        if (create_tray_icon() == 0)
        {
          ShowWindow (main_hWnd, SW_HIDE);
          result = 0;
          icon_visible = true;
          return true;   // event treated
        }
      }

      if (message == WM_TRAY_ICONE && wparam == TRAY_UID && ((uint)lparam == WM_LBUTTONUP || (uint)lparam == WM_RBUTTONUP || (uint)lparam == WM_LBUTTONDBLCLK))
      {
        ShowWindow (main_hWnd, SW_SHOW);
        result = 0;
        remove_tray_icon ();
        icon_visible = false;
        return true;   // event treated
      }
    }

    if (previous != null)
      return (previous) (hwnd, message, wparam, lparam, out result);

    result = 0;
    return false;  // event not treated
  }

end SYSTRAY;


public void minimize_in_systray ()
{
  const string str = "TaskbarCreated\0";

  if (s_uTaskbarRestart == 0)
    s_uTaskbarRestart = RegisterWindowMessageA (&str);

  previous = gi_get_main_window_message_hook ();
  gi_set_main_window_message_hook (message_hook);
}

//------------------------------------------------------------------------------
#end unsafe
//------------------------------------------------------------------------------
