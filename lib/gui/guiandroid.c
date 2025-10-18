
// guiandroid.c : gui layer for android

use ../android/android;
use ../arraye, ../gui, ../image, ../queue, ../strings, ../thread;
use ../edline, ../edtext;
use guitree, guicb, guidraw, guitreatevent;
use guiandroidrender, guiandroidkeyboard;

//----------------------------------------------------------------------------------------
#begin unsafe
//----------------------------------------------------------------------------------------

package Q1 = new SAFE_QUEUE (INFO => APPLICATION_EVENT);
package Q2 = new SAFE_QUEUE (INFO => EVENT);

Q1.QUEUE g_msg_queue;
Q2.QUEUE g_msg;

int  g_signal;

bool g_main_timer_set;
uint g_main_timer_duration;
uint g_main_timer_timeout;

int  g_dialog_timer_count;
uint g_dialog_timer_id[4];
uint g_dialog_timer_timeout[4];

uint g_unique_seqnr;

ONPAUSE g_parent_onPause;
bool    g_secondary_button_pressed;

//----------------------------------------------------------------------------------------

struct INFO
{
  DIALOG_INFO  dialog_info;  // !! MUST BE FIRST FIELD OF THIS STRUCTURE SINCE WE CAST BETWEEN INFO AND DIALOG_INFO
  wstring^     title;
  uint         seqnr;
  IMAGE_INFO   image;
}

typedef INFO^ PINFO;

package AR = new ARRAY_EXTENDER (ELEMENT => PINFO);

PINFO[]^ g_dialogs;   // ordered from top to bottom

int g_last_click_x, g_last_click_y;

INFO^         g_lgrabbed_dialog;      // dialog between click, drag and release (left button)
CONTROL_INFO^ g_lgrabbed_control;     // control between click, drag and release (left button)

INFO^         g_rgrabbed_dialog;      // dialog between click and release (right button)
CONTROL_INFO^ g_rgrabbed_control;     // control between click and release (right button)

INFO^         g_active_dialog;       // currently active dialog (that also receives keyboard input)

bool g_need_keyboard;

//--------------------------------------------------------------------

PINFO touched_dialog (int x, int y)
{
  int i;
  for (i=0; i<g_dialogs^'length; i++)
  {
    ref INFO info = g_dialogs^[i]^;
    if (x >= info.dialog_info.rect.left && x < info.dialog_info.rect.right &&
        y >= info.dialog_info.rect.top && y < info.dialog_info.rect.bottom)
    {
      return g_dialogs^[i];
    }
  }
  return null;
}

//--------------------------------------------------------------------

int index_of (DIALOG_ID d)
{
  int i;

  for (i=0; i<g_dialogs^'length; i++)
  {
    if (d == g_dialogs^[i]^.dialog_info.d)
      return i;
  }

  return -1;
}

//----------------------------------------------------------------------------------------

bool find (DIALOG_ID d, out PINFO value)
{
  int i = index_of (d);
  if (i != -1)
  {
    value = g_dialogs^[i];
    return true;
  }
  else
  {
    clear value;
    return false;
  }
}

//----------------------------------------------------------------------------------------

void deallocate_dialog (DIALOG_ID d)
{
  int i = index_of (d);
  if (i != -1)
  {
    {
      ref INFO^ p = g_dialogs^[i];

      if (p == g_lgrabbed_dialog)
      {
        g_lgrabbed_dialog = null;
        g_lgrabbed_control = null;
      }

      if (p == g_rgrabbed_dialog)
      {
        g_rgrabbed_dialog = null;
        g_rgrabbed_control = null;
      }

      guiandroidrender.insert_dialog_job (DIALOG_JOB(JOB_DELETE) ' {seqnr => p^.seqnr});

      deallocate_controls (g_dialogs^[i]^.dialog_info.list);
      free g_dialogs^[i]^.title;
      free_image (ref g_dialogs^[i]^.image);

      if (g_active_dialog == p)
        g_active_dialog = null;
    }
    free g_dialogs^[i];
    AR.remove (ref g_dialogs, i);
  }
}

//----------------------------------------------------------------------------------------

void insert_dialog (DIALOG_ID d, PINFO p)
{
  deallocate_dialog (d);
  AR.insert (ref g_dialogs, index => 0, element => p);
}

//----------------------------------------------------------------------------------------

void move_dialog_to_front (DIALOG_ID d, PINFO p)
{
  int i = index_of (d);
  assert (i != -1);
  assert g_dialogs^[i] == p;
  AR.remove (ref g_dialogs, i);
  AR.insert (ref g_dialogs, index => 0, element => p);
}

//----------------------------------------------------------------------------------------

void click_on_world_unfocus_all_dialogs ()
{
  int i;
  for (i=0; i<g_dialogs^'length; i++)
  {
    ref DIALOG_INFO dp = g_dialogs^[i]^.dialog_info;
    bool needs_update = (dp.current_transparency != dp.dialog_transparency_non_focus)
                     || dp.has_keyboard_focus;

    dp.has_focus = false;           // dialog has non-focus transparency (several dialogs can have focus)
    dp.has_keyboard_focus = false;  // only 1 dialog in the system has keyboard focus

    if (needs_update)
    {
      dp.current_transparency = dp.dialog_transparency_non_focus;
      dp.needs_move = true;
    }
  }

  if (g_active_dialog != null)
  {
    EVENT e;

    g_dialog_ptr = &g_active_dialog^.dialog_info;

    clear e;
    e.d = g_active_dialog^.dialog_info.d;
    e.type = EVENT_DIALOG_DEACTIVATED;
    e.other_d = 0;
    g_active_dialog^.dialog_info.handler(e);

    // hide dash points on focus control and caret
    if (g_active_dialog^.dialog_info.focus != null)
    {
      repaint_control (g_active_dialog^.dialog_info.focus^);

      // close open combobox that has focus
      if (g_active_dialog^.dialog_info.focus^.typ == TYP_COMBO)
        g_active_dialog^.dialog_info.focus^.combo.lb.listbox_shown = false;
    }

    g_dialog_ptr = null;
    g_active_dialog = null;

    g_need_keyboard = false;
  }
}

//----------------------------------------------------------------------------------------

void activate_dialog (PINFO new_dialog)
{
  if (new_dialog^.dialog_info.d == 1_500_000_000)   // keyboard
    return;

  if (new_dialog == g_active_dialog)   // is already active dialog
    return;

  if (g_dialogs^[0] != new_dialog)  // currently not in front
  {
    move_dialog_to_front (new_dialog^.dialog_info.d, new_dialog);
    new_dialog^.dialog_info.needs_set_top = true;
  }

  if (g_active_dialog != null)
  {
    EVENT e;

    g_dialog_ptr = &g_active_dialog^.dialog_info;

    g_dialog_ptr->has_focus = false;
    g_dialog_ptr->has_keyboard_focus = false;

    clear e;
    e.d = g_dialog_ptr->d;
    e.type = EVENT_DIALOG_DEACTIVATED;
    e.other_d = new_dialog^.dialog_info.d;
    g_dialog_ptr->handler(e);

    // hide dash points on focus control and caret
    if (g_dialog_ptr->focus != null)
    {
      repaint_control (g_dialog_ptr->focus^);

      // close open combobox that has focus
      if (g_dialog_ptr->focus^.typ == TYP_COMBO)
        g_dialog_ptr->focus^.combo.lb.listbox_shown = false;
    }

    g_dialog_ptr = null;
  }

  {
    EVENT e;

    g_dialog_ptr = &new_dialog^.dialog_info;

    g_dialog_ptr->has_focus = true;
    g_dialog_ptr->has_keyboard_focus = true;

    g_dialog_ptr->current_transparency = g_dialog_ptr->dialog_transparency_focus;
    g_dialog_ptr->needs_move = true;

    clear e;
    e.d = g_dialog_ptr->d;
    e.type = EVENT_DIALOG_ACTIVATED;
    e.other_d = (g_active_dialog != null) ? g_active_dialog^.dialog_info.d : 0;
    g_dialog_ptr->handler(e);

    if (g_dialog_ptr->focus != null)
      repaint_control (g_dialog_ptr->focus^);

    g_dialog_ptr = null;
  }

  if (new_dialog^.dialog_info.d != 30)    // W_EMOTICONS (has never focus)
    g_active_dialog = new_dialog;
}

//----------------------------------------------------------------------------------------

// called in windows thread

void treat_android_gui_event ()
{
  uint4             now = thread.ticks();
  APPLICATION_EVENT ev;
  EVENT             ev2;

  if (g_main_timer_set && now - g_main_timer_timeout < uint'max/2)  // elapsed
  {
    clear ev;
    ev.type = EVENT_TIMER;
    Q1.enqueue (ref g_msg_queue, ev);

    g_main_timer_timeout = now + g_main_timer_duration;
  }

  while (Q1.dequeue (ref g_msg_queue, out ev) == 0)
  {
//    log ("GUI EVENT %s", ev.type'string);
    app_handler (ev);
  }

  {
    int i;
    for (i=0; i<g_dialog_timer_count; i++)
    {
      if (now - g_dialog_timer_timeout[i] < uint'max/2)  // elapsed
      {
        clear ev2;
        ev2.d = g_dialog_timer_id[i];
        ev2.type = EVENT_DIALOG_TIMER;
        Q2.enqueue (ref g_msg, ev2);
        
        g_dialog_timer_count--;

        g_dialog_timer_id     [i : g_dialog_timer_count-i] = g_dialog_timer_id     [i+1 : g_dialog_timer_count-i];
        g_dialog_timer_timeout[i : g_dialog_timer_count-i] = g_dialog_timer_timeout[i+1 : g_dialog_timer_count-i];

        i--;
      }
    }
  }

  while (Q2.dequeue (ref g_msg, out ev2) == 0)
  {
    INFO^ p;

    if (ev2.d < uint'max/2)
    {
      if (find (d => ev2.d, out value => p))
      {
        g_dialog_ptr = &p^.dialog_info;

        // treat no events before EVENT_NEW_DIALOG, that can cause crashes
        if (ev2.type == EVENT_NEW_DIALOG || g_dialog_ptr->dialog_initialized_done)
        {
          if (ev2.type == EVENT_NEW_DIALOG)
          {
            g_dialog_ptr->dialog_initialized_done = true;
          }

          p^.dialog_info.handler (ev2);

          if (ev2.type == EVENT_NEW_DIALOG)
          {
            // make sure title bar is visible on screen
            int x_size, y_size, delta;

            get_desktop_resolution (out x_size, out y_size);

            if (g_dialog_ptr->rect.bottom > y_size)
            {
              delta = g_dialog_ptr->rect.bottom - y_size;
              g_dialog_ptr->rect.top -= delta;
              g_dialog_ptr->rect.bottom = y_size;
            }

            if (g_dialog_ptr->rect.right > x_size)
            {
              delta = g_dialog_ptr->rect.right - x_size;
              g_dialog_ptr->rect.left -= delta;
              g_dialog_ptr->rect.right = x_size;
            }

            if (g_dialog_ptr->rect.top < 0)
            {
              g_dialog_ptr->rect.bottom -= g_dialog_ptr->rect.top;
              g_dialog_ptr->rect.top = 0;
            }

            if (g_dialog_ptr->rect.left < 0)
            {
              g_dialog_ptr->rect.right -= g_dialog_ptr->rect.left;
              g_dialog_ptr->rect.left = 0;
            }

            // new dialogs must be activated, otherwise EVENT_DIALOG_DEACTIVATED is not sent to remove the menu !
            activate_dialog (p);
          }

          if (ev2.type == EVENT_CLOSE_DIALOG)
          {
            deallocate_dialog (ev2.d);
            if (ev2.d == 1_500_000_000)     // QWERTY KEYBOARD
            {
              g_need_keyboard = false;
            }
          }
        }

        g_dialog_ptr = null;
      }
    }
    else   // special device commands (mouse, keyboard)
    {
      switch ((int)ev2.d)
      {
        case -1:   // press finger

          ev2.x = ui_scale (ui_unscale (ev2.x));  // reduce precision
          ev2.y = ui_scale (ui_unscale (ev2.y));

          g_last_click_x = ev2.x;
          g_last_click_y = ev2.y;

          {
            PINFO pd = touched_dialog (ev2.x, ev2.y);

            if (pd == null)    // touch on world
            {
              g_lgrabbed_dialog = null;
              click_on_world_unfocus_all_dialogs ();

              {
                APPLICATION_EVENT e;
                clear e;
                e.x = ev2.x;
                e.y = ev2.y;
                e.type = EVENT_MOUSE_CLICKED;
                e.key = 1;   // key=1 for click, 2=second of doubleclick $
                Q1.enqueue (ref g_msg_queue, e);
              }
            }
            else   // some dialog was clicked
            {
              ref DIALOG_INFO dp = pd^.dialog_info;

              activate_dialog (pd);

              g_dialog_ptr = &dp;
              g_lgrabbed_dialog = pd;

              {
                int           x = ev2.x;
                int           y = ev2.y;

                CONTROL_INFO^ c;

                x -= dp.rect.left;
                y -= dp.rect.top;

                c = touched_control (&dp, x, y);

                if (c != null && !tabable[(int)c^.typ])  // we clicked on a non-tabbable control
                  c = null;

                g_lgrabbed_control = c;

                {
                  int ofs_x = dp.border_size;
                  int ofs_y = dp.border_size + dp.title_height;

                  if (pd^.dialog_info.d != 1_500_000_000 &&  // QWERTY KEYBOARD
                      pd^.dialog_info.d != 30)               // W_EMOTICONS
                  {
                    g_need_keyboard = false;
                    if (c != null && (c^.typ == TYP_EDIT || c^.typ == TYP_COMBO || c^.typ == TYP_EDITBOX))
                    {
                      if (c^.typ == TYP_EDIT)
                        g_need_keyboard = edit_line_get_modify_allowed (c^.edit.cr.edit_line);
                      else if (c^.typ == TYP_COMBO)
                        g_need_keyboard = edit_line_get_modify_allowed (c^.combo.edit.cr.edit_line);
                      else if (c^.typ == TYP_EDITBOX)
                        g_need_keyboard = edit_text_get_modify_allowed (c^.editbox.cur.text);
                    }
                  }

/*
          if (y < p->border_drag_size + p->title_height &&
              x >= p->rect.right - p->rect.left - p->border_drag_size - p->title_height)
            rc = HTCLIENT;    // close button - in client area (generates WM_MOUSEMOVE)
*/

                  g_dialog_ptr->focus = c;

                  mouse_click_left (c,
                                    d_x             => x - ofs_x,
                                    d_y             => y - ofs_y,
                                    is_double_click => false,
                                    dialog_x        => x,
                                    dialog_y        => y);

                  mouse_hover (c, x-ofs_x, y-ofs_y);
                }
              }

              g_dialog_ptr = null;
            }
          }
          break;

        case -2:   // drag finger

          ev2.x = ui_scale (ui_unscale (ev2.x));  // reduce precision
          ev2.y = ui_scale (ui_unscale (ev2.y));

          if (g_lgrabbed_dialog == null)    // drag point on world
          {
            // no effect
          }
          else   // drag a dialog/control
          {
            int dx, dy;

            dx = ev2.x - g_last_click_x;
            dy = ev2.y - g_last_click_y;

            if (dx == 0 && dy == 0)
              break;

            g_last_click_x = ev2.x;
            g_last_click_y = ev2.y;

            g_dialog_ptr = &g_lgrabbed_dialog^.dialog_info;

            {
              CONTROL_INFO^ c = g_lgrabbed_control;

              if (c == null)   // move dialog
              {
                int x_size, y_size, delta;

                get_desktop_resolution (out x_size, out y_size);

                delta = g_dialog_ptr->title_height >> 1;

                if (ev2.x >= delta && ev2.x < x_size - delta &&
                    ev2.y >= delta && ev2.y < y_size - delta)
                {
                  WINDOWPOS sub;

                  // if click is within .border_drag_size, resize it, otherwise move it
                  // $

                  // old position in wpos will be updated by user through this global pointer
                  sub_window_windowpos = &sub;

                  sub_window_windowpos->x = g_dialog_ptr->rect.left;
                  sub_window_windowpos->y = g_dialog_ptr->rect.top;
                  sub_window_windowpos->cx = g_dialog_ptr->rect.right - g_dialog_ptr->rect.left;
                  sub_window_windowpos->cy = g_dialog_ptr->rect.bottom - g_dialog_ptr->rect.top;

                  {
                    EVENT e;

                    clear e;
                    e.d = g_dialog_ptr->d;
                    e.type = EVENT_MOVE;

                    e.x = ui_unscale (g_dialog_ptr->rect.left + dx);
                    e.y = ui_unscale (g_dialog_ptr->rect.top + dy);

                    g_dialog_ptr->handler(e);  // can change all 4 coord (move) or right/bottom (size) in any order
                  }

                  // we need some boundaries to avoid that a dialog becomes unclickable
                  // y : title bar must remains in screen
                  // x : finger-wide space must remains in screen

                  g_dialog_ptr->rect.left = sub.x;
                  g_dialog_ptr->rect.top = sub.y;
                  g_dialog_ptr->rect.right = sub.x + sub.cx;
                  g_dialog_ptr->rect.bottom = sub.y + sub.cy;

                  sub_window_windowpos = null;


/*
                  {
                    // old position in wpos will be updated by user through this global pointer
                    sub_window_windowpos = wpos;

            //        if (newpos.x != wpos->x || newpos.y != wpos->y)    // x, y changed
                    {
                      clear e;
                      e.d = p->d;
                      e.type = EVENT_MOVE;

                      e.x = ui_unscale (newpos.x);
                      e.y = ui_unscale (newpos.y);
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
*/

/*
                  g_dialog_ptr->rect.left   += dx;
                  g_dialog_ptr->rect.top    += dy;
                  g_dialog_ptr->rect.right  += dx;
                  g_dialog_ptr->rect.bottom += dy;
*/
                  g_dialog_ptr->needs_move = true;
                }
              }
              else
              {
                int x = ev2.x;
                int y = ev2.y;

                int ofs_x = g_dialog_ptr->border_size;
                int ofs_y = g_dialog_ptr->border_size + g_dialog_ptr->title_height;

                x -= g_dialog_ptr->rect.left;
                y -= g_dialog_ptr->rect.top;

                g_dialog_ptr->focus = c;

                mouse_drag (c, x-ofs_x, y-ofs_y);
                mouse_hover (c, x-ofs_x, y-ofs_y);
              }
            }

            g_dialog_ptr = null;
          }
          break;

        case -3:   // release left finger

          ev2.x = ui_scale (ui_unscale (ev2.x));  // reduce precision
          ev2.y = ui_scale (ui_unscale (ev2.y));

          {
            PINFO pd = touched_dialog (ev2.x, ev2.y);

            g_lgrabbed_dialog = pd;

            if (pd != null)
            {
              int           x = ev2.x;
              int           y = ev2.y;

              CONTROL_INFO^ c;

              x -= pd^.dialog_info.rect.left;
              y -= pd^.dialog_info.rect.top;

              c = touched_control (&pd^.dialog_info, x, y);

              if (c != null && !tabable[(int)c^.typ])  // we clicked on a non-tabbable control
                c = null;

              g_lgrabbed_control = c;
            }
          }

          if (g_lgrabbed_dialog == null)
          {
            int x = ev2.x;
            int y = ev2.y;
            APPLICATION_EVENT e;

            clear e;
            e.x = x;
            e.y = y;
            e.type = EVENT_MOUSE_DROP;
            Q1.enqueue (ref g_msg_queue, e);
          }
          else
          {
            g_dialog_ptr = &g_lgrabbed_dialog^.dialog_info;

            {
              int x = ev2.x;
              int y = ev2.y;
              int ofs_x = g_dialog_ptr->border_size;
              int ofs_y = g_dialog_ptr->border_size + g_dialog_ptr->title_height;

              CONTROL_INFO^ c = g_lgrabbed_control;

              x -= g_dialog_ptr->rect.left;
              y -= g_dialog_ptr->rect.top;

/*
if (c != null)
  log ("RELEASE DIALOG %u CONTROL %d", g_dialog_ptr->d, c^.id);
*/
              mouse_click_left_released (c, x-ofs_x, y-ofs_y);
            }

            g_dialog_ptr = null;
            g_lgrabbed_dialog = null;
            g_lgrabbed_control = null;
          }
          break;

        case -4:  // press right finger

          ev2.x = ui_scale (ui_unscale (ev2.x));  // reduce precision
          ev2.y = ui_scale (ui_unscale (ev2.y));

          g_last_click_x = ev2.x;
          g_last_click_y = ev2.y;

          {
            PINFO pd = touched_dialog (ev2.x, ev2.y);
            if (pd == null)        // right click on world
            {
              g_rgrabbed_dialog = null;
              click_on_world_unfocus_all_dialogs ();

              {
                APPLICATION_EVENT e;
                clear e;
                e.x = ev2.x;
                e.y = ev2.y;
                e.type = EVENT_MOUSE_MENU;
                Q1.enqueue (ref g_msg_queue, e);
              }
              
            }
            else   // some dialog was clicked
            {
              ref DIALOG_INFO dp = pd^.dialog_info;

              activate_dialog (pd);

              g_dialog_ptr = &dp;
              g_rgrabbed_dialog = pd;

              {
                int           x = ev2.x;
                int           y = ev2.y;

                CONTROL_INFO^ c;

                x -= dp.rect.left;
                y -= dp.rect.top;

                c = touched_control (&dp, x, y);

                if (c != null && !tabable[(int)c^.typ])  // we clicked on a non-tabbable control
                  c = null;

                g_rgrabbed_control = c;

                {
                  int ofs_x = dp.border_size;
                  int ofs_y = dp.border_size + dp.title_height;

/*
if (c != null)
  log ("RIGHT-CLICK DIALOG %u CONTROL %d", pd^.dialog_info.d, c^.id);
*/

/*
          if (y < p->border_drag_size + p->title_height &&
              x >= p->rect.right - p->rect.left - p->border_drag_size - p->title_height)
            rc = HTCLIENT;    // close button - in client area (generates WM_MOUSEMOVE)
*/

                  if (c != null)
                    mouse_click_right (c,
                                       d_x             => x - ofs_x,
                                       d_y             => y - ofs_y,
                                       dialog_x        => x,
                                       dialog_y        => y);
                }
              }

              g_dialog_ptr = null;
            }
          }
          break;

        case -6:   // release right finger

          ev2.x = ui_scale (ui_unscale (ev2.x));  // reduce precision
          ev2.y = ui_scale (ui_unscale (ev2.y));

          if (g_rgrabbed_dialog == null)    // release right click on world
          {
/*          
            int x = ev2.x;
            int y = ev2.y;
            APPLICATION_EVENT e;
            clear e;
            e.x = x;
            e.y = y;
            e.type = EVENT_MOUSE_MENU;
            Q1.enqueue (ref g_msg_queue, e);
*/            
          }
          else    // release right click on dialog
          {
            g_dialog_ptr = &g_rgrabbed_dialog^.dialog_info;

            {
              int x = ev2.x;
              int y = ev2.y;
              int ofs_x = g_dialog_ptr->border_size;
              int ofs_y = g_dialog_ptr->border_size + g_dialog_ptr->title_height;

              CONTROL_INFO^ c = g_rgrabbed_control;

              x -= g_dialog_ptr->rect.left;
              y -= g_dialog_ptr->rect.top;

/*
if (c != null)
  log ("RELEASE DIALOG %u CONTROL %d", g_dialog_ptr->d, c^.id);
*/
              mouse_click_right_released (c, x-ofs_x, y-ofs_y, x, y);
            }

            g_dialog_ptr = null;
            g_rgrabbed_dialog = null;
            g_rgrabbed_control = null;
          }
          break;

        case -10:   // key
          {
            g_dialog_ptr = null;

            if (g_active_dialog != null)
            {
              g_dialog_ptr = &g_active_dialog^.dialog_info;

              if (ev2.key == 27 ||   // escape
                   (ev2.key >= KEY_CMD+0x70 && ev2.key <= KEY_CMD+0x87) ||  // Fxx keys
                    ev2.key==KEY_CMD+VK_SHIFT ||    // shift key
                    ev2.key==KEY_CMD+VK_CONTROL ||  // control key
                    ev2.key==KEY_CMD+VK_MENU)       // alt key
              {
                g_dialog_ptr = null;
              }
            }

            if (g_dialog_ptr != null)
            {
              treat_keyboard_key (ev2.key);
              g_dialog_ptr = null;
            }
            else
            {
              clear ev;
              ev.type = EVENT_KEYBOARD;
              ev.key = ev2.key;
              Q1.enqueue (ref g_msg_queue, ev);
            }
          }
          break;

        default:
          break;
      }
    }
  }

  if (g_need_keyboard)
    guiandroidkeyboard.show_keyboard ();
  else
    guiandroidkeyboard.hide_keyboard ();
}

//----------------------------------------------------------------------------------------

void paint_dialog (ref INFO info,
                       RECT local_rect_portion)   // rectangle to redraw
{
  RECT rect = local_rect_portion;
  int dialog_size_x = info.dialog_info.rect.right  - info.dialog_info.rect.left;
  int dialog_size_y = info.dialog_info.rect.bottom - info.dialog_info.rect.top;
  HDC  hdc;

  if (info.image.width != (uint)dialog_size_x || info.image.height != (uint)dialog_size_y)
  {
    free info.image.pixel;
    info.image.width = (uint)dialog_size_x;
    info.image.height = (uint)dialog_size_y;
    info.image.pixel = new byte[4 * dialog_size_x * dialog_size_y];

    {
      ref uint image[] = ((uint *)&(info.image.pixel^))[0:dialog_size_x * dialog_size_y];
      image = {all => info.dialog_info.dialog_background_color | 0xFF000000};
    }

    rect = {left => 0, top => 0, right => (int)dialog_size_x, bottom => (int)dialog_size_y};
  }

  // make sure rect is never outside image
  if (rect.top < 0)
    rect.top = 0;
  if (rect.bottom > (int)info.image.height)
    rect.bottom = (int)info.image.height;
  if (rect.left < 0)
    rect.left = 0;
  if (rect.right > (int)info.image.width)
    rect.right = (int)info.image.width;


  if (rect.top >= rect.bottom || rect.left >= rect.right)  // empty slice
    ;  // nothing to draw
  else
  {
    clear hdc;
    hdc.image  = info.image.pixel;
    hdc.width  = info.image.width;
    hdc.height = info.image.height;
    hdc.rect   = rect;

    redraw_dialogs (hwnd  => 0,
                    hdc   => hdc,
                    title => info.title == null ? L"" : info.title^,
                    ps    => {rcPaint => rect},
                    d     => info.dialog_info);
  }
}

//----------------------------------------------------------------------------------------

void windows_thread ()
{
  int i;

  for (;;)
  {
    treat_android_gui_event ();


    // loop on all dialogs. when (needs_repaint), redraw dialog on image, adapt draw3d texture.

    for (i=g_dialogs^'length-1; i>=0; i--)   // reverse order, so JOB_MOVE_ON_TOP are sent in same order as g_dialogs
    {
      ref INFO info = g_dialogs^[i]^;


      if (info.dialog_info.needs_repaint)
      {
        if (info.dialog_info.full_repaint)
        {
          info.dialog_info.paint_rect = {left   => 0,
                                         top    => 0,
                                         right  => info.dialog_info.rect.right - info.dialog_info.rect.left,
                                         bottom => info.dialog_info.rect.bottom - info.dialog_info.rect.top};
        }

        paint_dialog (ref info               => g_dialogs^[i]^,
                          local_rect_portion => info.dialog_info.paint_rect);

        info.dialog_info.needs_repaint = false;
        info.dialog_info.full_repaint = false;
        info.dialog_info.paint_rect = {left => int'max, top => int'max, right => int'min, bottom => int'min};  // empty

        {
          IMAGE_INFO image;

          assert copy_image (info.image, out image) == 0;

          guiandroidrender.insert_dialog_job (DIALOG_JOB(JOB_UPDATE_IMAGE_RECT)
                  ' {seqnr => info.seqnr,
                     ofs_x => 0,
                     ofs_y => 0,
                     image => image});
        }

        info.dialog_info.needs_move = true;
      }


      if (info.dialog_info.needs_move)
      {
        info.dialog_info.needs_move = false;
        guiandroidrender.insert_dialog_job (DIALOG_JOB(JOB_UPDATE_ATTR)
                ' {seqnr  => info.seqnr,
                   x      => info.dialog_info.rect.left,
                   y      => info.dialog_info.rect.top,
                   size_x => info.dialog_info.rect.right - info.dialog_info.rect.left,
                   size_y => info.dialog_info.rect.bottom - info.dialog_info.rect.top,
                   transparency => info.dialog_info.current_transparency,
                   });
      }


      if (info.dialog_info.needs_set_top)
      {
        info.dialog_info.needs_set_top = false;
        guiandroidrender.insert_dialog_job (DIALOG_JOB(JOB_MOVE_ON_TOP)
                ' {seqnr => info.seqnr});
      }
    }


    if (Q2.count(g_msg) > 0)
      continue;

    if (Q1.count(g_msg_queue) > 0)
      continue;

    {
      uint rest = uint'max;

      if (g_main_timer_set)
      {
        rest = g_main_timer_timeout - thread.ticks();
      }

      if (g_dialog_timer_count > 0)
      {
        int t;
        for (t=0; t<g_dialog_timer_count; t++)
        {
          uint rest2 = g_dialog_timer_timeout[t] - thread.ticks();
          if ((int)rest2 < (int)rest)
            rest = rest2;
        }
      }
      
      if ((int)rest < 0)
        continue;

      // wakeup when global timer or messages present in queue
      wait_signal (g_signal, timeout_msecs => rest);
    }
  }
}

//----------------------------------------------------------------------------------------

public void set_timer (uint msecs)
{
  g_main_timer_set = true;
  g_main_timer_duration = msecs;
  g_main_timer_timeout = thread.ticks() + msecs;

  raise_signal (g_signal);
}

//----------------------------------------------------------------------------------------

public void stop_timer ()
{
  g_main_timer_set = false;
}

//----------------------------------------------------------------------------------------

public void set_dialog_timer (uint msec)
{
  int i;
  for (i=0; i<g_dialog_timer_count; i++)
  {
    if (g_dialog_ptr->d == g_dialog_timer_id[i])
      break;
  }
  g_dialog_timer_id[i] = g_dialog_ptr->d;
  g_dialog_timer_timeout[i] = thread.ticks() + msec;
  
  if (i == g_dialog_timer_count)  // new slot
    g_dialog_timer_count++;

  raise_signal (g_signal);
}

//----------------------------------------------------------------------------------------

// called in windows thread

public void create_dialog (DIALOG_ID d, DIALOG_HANDLER dialog_handler)
{
  INFO^ p;

  {
    DIALOG_INFO info;

    clear info;
    info.d                             = d;
    info.handler                       = dialog_handler;
//    info.ShowWindowArg                 = SW_SHOW;
    info.border_size                   = ui_scale(2);
    info.dialog_colors                 = {0x000000, 0x808080, 0xFF8080, 0xFF8080, 0xE0E0E0, 0xFFFFFF}; // {0x000000, 0x808080, 0x3E3E3E, 0xE0E0E0, 0xFFFFFF};
    info.dialog_background_color       = 0xC0C0C0;    // grey background color (0x3E3E3E = grey background color Firestorm)

    strcpy (out info.title_font.name, "Tahoma");
    info.title_font.height             = ui_scale(16);
//      info.title_font.color              = 0x000000;   // black

    info.title_height                  = ui_scale(20);
    info.dialog_transparency_non_focus = 165;
    info.dialog_transparency_focus     = 255;

    info.current_transparency = info.dialog_transparency_focus;

    strcpy (out info.default_font.name, "Microsoft Sans Serif");
    info.default_font.height           = ui_scale(12);

    info.default_colors                = {0x000000, 0x808080, 0xC0C0C0, 0xC0C0C0, 0xE0E0E0, 0xFFFFFF};
    info.default_insert_mode           = true;

    p = new INFO;
    p^.dialog_info = info;
    p^.seqnr = g_unique_seqnr++;

    insert_dialog (d => d, p => p);
    guiandroidrender.insert_dialog_job (DIALOG_JOB(JOB_ADD) ' {seqnr => p^.seqnr});
  }

  {
    EVENT ev;
    clear ev;
    ev.d = d;
    ev.type = EVENT_NEW_DIALOG;
    Q2.enqueue (ref g_msg, ev);
  }
}

//----------------------------------------------------------------------------------------

// called in windows thread

public void close_dialog (DIALOG_ID d)
{
  EVENT ev;
  clear ev;
  ev.d = d;
  ev.type = EVENT_CLOSE_DIALOG;
  Q2.enqueue (ref g_msg, ev);
}

//----------------------------------------------------------------------------------------

wstring^ new_wstring (wstring s)
{
  return new wstring ' (s[0 : wstrlen(s)]);
}

//----------------------------------------------------------------------------------------

wstring^ new_string (string s)
{
  int len = strlen(s);
  wstring^ ps = new wstring (len);
  int i;
  for (i=0; i<len; i++)
    ps^[i] = (wchar)(byte)s[i];
  return ps;
}

//----------------------------------------------------------------------------------------

// called in windows thread

public void set_dialog_title (string title)
{
  int i = index_of (g_dialog_ptr->d);
  if (i != -1)
  {
    ref INFO p = g_dialogs^[i]^;
    free p.title;
    p.title = new_string (title);
  }
}

//----------------------------------------------------------------------------------------

public void wset_dialog_title (wstring title)
{
  int i = index_of (g_dialog_ptr->d);
  if (i != -1)
  {
    ref INFO p = g_dialogs^[i]^;
    free p.title;
    p.title = new_wstring (title);
  }
}

//----------------------------------------------------------------------------------------

public void post_menu_event (MENU_ID id)
{
  APPLICATION_EVENT e;
  clear e;
  e.type = EVENT_MENU;
  e.id = id;
  Q1.enqueue (ref g_msg_queue, e);
}

//----------------------------------------------------------------------------------------

public void post_event (EVENT ev)
{
  Q2.enqueue (ref g_msg, ev);

  raise_signal (g_signal);
}

//----------------------------------------------------------------------------------------

public void post_dialog_message (DIALOG_ID d, int message)
{
  EVENT ev;
  clear ev;
  ev.d = d;
  ev.type = EVENT_MESSAGE;
  ev.message = message;
  Q2.enqueue (ref g_msg, ev);

  raise_signal (g_signal);
}

//----------------------------------------------------------------------------------------

package POINTERS

  int g_pointer_id;

end POINTERS;

//----------------------------------------------------------------------------------------

// called by draw3d thread

void device_event (int cmd, int x, int y)
{
  EVENT ev;

  clear ev;
  ev.d = (uint)cmd;   // negative
  ev.x = x;
  ev.y = y;

  Q2.enqueue (ref g_msg, ev);

  raise_signal (g_signal);
}

//----------------------------------------------------------------------------------------

[callback]
bool onTouchEvent (GameActivity* activity, GameActivityMotionEvent* motionEvent)
{
  const int AMOTION_EVENT_ACTION_POINTER_INDEX_MASK = 0xff00;
  const int AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT = 8;
  const int AMOTION_EVENT_AXIS_X = 0;
  const int AMOTION_EVENT_AXIS_Y = 1;
  const int AMOTION_EVENT_ACTION_MASK = 0xFF;
  const int AMOTION_EVENT_ACTION_DOWN = 0;

/*
  const int AMOTION_EVENT_AXIS_PRESSURE = 2;
  const int AMOTION_EVENT_AXIS_SIZE = 3;
*/
  const int AMOTION_EVENT_ACTION_POINTER_DOWN = 5;
  const int AMOTION_EVENT_ACTION_CANCEL = 3;
  const int AMOTION_EVENT_ACTION_UP = 1;
  const int AMOTION_EVENT_ACTION_POINTER_UP = 6;
  const int AMOTION_EVENT_ACTION_MOVE = 2;
/*
  const int AKEY_EVENT_ACTION_DOWN = 0;
  const int AKEY_EVENT_ACTION_UP = 1;
  const int AKEY_EVENT_ACTION_MULTIPLE = 2;
*/

  int32_t action = motionEvent->action;

  // Find the pointer index, mask and bitshift to turn it into a readable value.
  int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

  // get the x and y position of this event if it is not ACTION_MOVE.
  GameActivityPointerAxes* pointer = &motionEvent->pointers[pointerIndex];
  float x = pointer->axisValues[AMOTION_EVENT_AXIS_X];
  float y = pointer->axisValues[AMOTION_EVENT_AXIS_Y];

  _unused activity;

  // determine the action type and process the event accordingly.
  switch (action & AMOTION_EVENT_ACTION_MASK)
  {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      if (pointerIndex > 0)  // second pointer : right click
      {
        device_event (-3, -1, -1);    // close previous click
        device_event (-4, (int)x, (int)y);
      }
      else
      {
        if ((motionEvent->buttonState & 2) != 0)   // BUTTON_SECONDARY
        {
          device_event (-4, (int)x, (int)y);
          g_secondary_button_pressed = true;
        }
        else
          device_event (-1, (int)x, (int)y);
      }
      g_pointer_id = pointer->id;

      return true;

    case AMOTION_EVENT_ACTION_CANCEL:
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
      if (g_pointer_id == pointer->id)
      {
        if (pointerIndex > 0)
          device_event (-6, (int)x, (int)y);   // release right button
        else
        {
          if (g_secondary_button_pressed)
          {
            device_event (-6, (int)x, (int)y);   // release right button
            g_secondary_button_pressed = false;
          }
          else
            device_event (-3, (int)x, (int)y);   // release left button
        }
        g_pointer_id = -1;
      }

      return true;

    case AMOTION_EVENT_ACTION_MOVE:
      {
        uint index;
        // There is no pointer index for ACTION_MOVE, only a snapshot of
        // all active pointers; app needs to cache previous active pointers
        // to figure out which ones are actually moved.
        for (index = 0; index < motionEvent->pointerCount; index++)
        {
          pointer = &motionEvent->pointers[index];
          x = pointer->axisValues[AMOTION_EVENT_AXIS_X];
          y = pointer->axisValues[AMOTION_EVENT_AXIS_Y];
          if (g_pointer_id == pointer->id)
          {
            if (pointerIndex == 0)
              device_event (-2, (int)x, (int)y);
          }
        }
      }

      return true;

    default:
      break;
  }

  return false;
}

//----------------------------------------------------------------------------------------

[callback]
void onPause (GameActivity* activity)
{
  APPLICATION_EVENT e;

  clear e;
  e.type = EVENT_CLOSE_BUTTON;
  Q1.enqueue (ref g_msg_queue, e);

  if (g_parent_onPause != null)
    g_parent_onPause (activity);
}

//----------------------------------------------------------------------------------------

#if 0
$
static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
        AInputEvent_getSource(event) == AINPUT_SOURCE_MOUSE) {

        int32_t buttonState = AMotionEvent_getButtonState(event);


Use AMotionEvent_getX(event) and AMotionEvent_getY(event) to get cursor position.


left button

if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
    AInputEvent_getSource(event) == AINPUT_SOURCE_MOUSE) {

    int32_t action = AMotionEvent_getAction(event);
    int32_t buttonState = AMotionEvent_getButtonState(event);

    if ((action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_UP) &&
        (buttonState & AMOTION_EVENT_BUTTON_PRIMARY)) {
        // Handle left-click press or release
    }
}


right button

        if (buttonState & AMOTION_EVENT_BUTTON_SECONDARY) {
            // Right-click detected
            // Handle your logic here
AMOTION_EVENT_BUTTON_SECONDARY corresponds to the right mouse button.

You can also check for AMOTION_EVENT_ACTION_DOWN or ACTION_UP to detect press/release.


hover:
if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
    AInputEvent_getSource(event) == AINPUT_SOURCE_MOUSE) {

    int32_t action = AMotionEvent_getAction(event);

    if (action == AMOTION_EVENT_ACTION_HOVER_ENTER ||
        action == AMOTION_EVENT_ACTION_HOVER_MOVE ||
        action == AMOTION_EVENT_ACTION_HOVER_EXIT) {
        // Handle hover behavior

Dragging only works while the button is pressed and ACTION_MOVE is received.

Hover events stop when a button is pressed, so switch to move detection during drag.
    }
}


drag:
if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
    AInputEvent_getSource(event) == AINPUT_SOURCE_MOUSE) {

    int32_t action = AMotionEvent_getAction(event);
    int32_t buttonState = AMotionEvent_getButtonState(event);

    if ((buttonState & AMOTION_EVENT_BUTTON_PRIMARY) &&
        action == AMOTION_EVENT_ACTION_MOVE) {
        // Handle drag movement
    }
}

        }
    }
    return 0;
}

#endif

//----------------------------------------------------------------------------------------

// called by gui.c (draw3d thread)

public void create_main_window (APPLICATION_EVENT_HANDLER application_event_handler)
{
  int rc;

  app_handler = application_event_handler;

  g_signal = create_signal ();

  g_app->activity->callbacks->onTouchEvent = onTouchEvent;
  g_parent_onPause = g_app->activity->callbacks->onPause;
  g_app->activity->callbacks->onPause = onPause;

  {
    APPLICATION_EVENT ev;

    clear ev;
    ev.type = EVENT_NEW_APPLICATION;

    Q1.enqueue (ref g_msg_queue, ev);
  }

  g_dialogs = new PINFO[0];

  g_active_dialog = null;

  guiandroidrender.add_hook_in_draw3d_to_render_dialogs ();

  rc = run windows_thread ();
  assert rc == 0;
}

//----------------------------------------------------------------------------------------
#end unsafe
//----------------------------------------------------------------------------------------
