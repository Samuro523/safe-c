
// guitreatevent.c

#if WINDOWS
  use ../win/windows, guihash;
#elif ANDROID
  use guiandroid;
#else
  bad
#endif

use ../arithm, ../edline, ../edtext, ../strings, ../text, ../clipboard;
use guitree, guicb, ../gui, guiutil;

//--------------------------------------------------------------------
#begin unsafe
//--------------------------------------------------------------------

void send_event (EVENT_TYPE typ, EVENT e, CONTROL_INFO o)
{
  EVENT e2 = e;
  e2.d = g_dialog_ptr->d;
  e2.type = typ;
  e2.id = o.id;
  g_dialog_ptr->handler (e2);
}

//--------------------------------------------------------------------

void send_event_new_focus (DIALOG_INFO d, int key)
{
  EVENT e;
  clear e;
  if (d.focus != null)
  {
    e.key = key;
    send_event (EVENT_NEW_FOCUS, e, d.focus^);
  }
}

//--------------------------------------------------------------------

// maybe we must modify scroll_x_offset and redraw the field

public
void update_scroll_x_edit_or_combo (ref CONTROL_INFO o, ref EDIT_INFO edit, int x_size)
{
  ref EDIT_LINE e = edit.cr.edit_line;
  wstring^      line = new wchar[edit_line_get_max_length(e)];
  int           len;

  assert o.typ == TYP_EDIT || o.typ == TYP_COMBO;

  len = edit_line_get_length(e);
  edit_line_get_line (e, out line^);   // load line with trailing spaces

  if (edit.password_mode)
  {
    ref wstring ligne = line^;
    int         i;
    for (i=0; i<len; i++)
      if (ligne[i] != L' ')
        ligne[i] = L'*';
  }

  if (edit_line_get_col(e) <= edit.cr.scroll_x_offset)    // scroll left
  {
    int ns = edit_line_get_col(e) - 1;
    if (ns < 0)
      ns = 0;
    if (edit.cr.scroll_x_offset != ns)
    {
      edit.cr.scroll_x_offset = ns;
      repaint_control (o);
    }
    free line;
    return;
  }


  // test if we need to scroll to the right

  len = intern_wtext_width_of2 (line^[edit.cr.scroll_x_offset : edit_line_get_col(e) - edit.cr.scroll_x_offset], o.font);

  if (len < x_size - ui_scale(4 - 2))   // no scroll needed
  {
    free line;
    return;
  }

  for (;;)
  {
    edit.cr.scroll_x_offset++;

    if (edit.cr.scroll_x_offset == edit_line_get_col(e))
      break;

    len = intern_wtext_width_of2 (line^[edit.cr.scroll_x_offset : edit_line_get_col(e) - edit.cr.scroll_x_offset], o.font);

    if (len < x_size - ui_scale(4 - 2))   // all contained within field
      break;
  }

  free line;
  repaint_control (o);
}

//--------------------------------------------------------------------

bool is_word (wchar c, wchar adj_c)
{
  return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
         (c >= L'0' && c <= L'9') || (c == L'_') || (c >= (wchar)128 ||
         (c == L'.' && adj_c >= L'0' && adj_c <= L'9'));  // accept dot of float number
}

/************************************************************************/

void surround_word (wstring line, int col, out int first_col, out int last_col)
{
  first_col = col;
  last_col = col;

  if (col >= line'length)
    return;

  if (!is_word (line[col], L'0'))
    return;

  while (first_col > 0 && is_word(line[first_col-1], adj_c => line[first_col]))
    first_col--;

  while (last_col < line'length && is_word(line[last_col], adj_c => (last_col-1 >= 0 ? line[last_col-1] : Lnul)))
    last_col++;
}

//--------------------------------------------------------------------

void edit_set_marking_start_for_edit_and_combo (ref EDIT_INFO edit, int col)
{
  edit.line_marking_in_progress = true;
  edit.line_marking_start_col = col;
  edit_line_set_first_mark (ref edit.cr.edit_line, {active => true, col => col});
  edit_line_set_last_mark (ref edit.cr.edit_line, {active => true, col => col});
}

//--------------------------------------------------------------------

void set_clicked_col_edit_or_combo (ref CONTROL_INFO o,
                                        int          relative_x,   // relative to edit frame
                                    ref EDIT_INFO    edit,
                                        int          x_size,
                                        bool         is_drag,
                                        bool         is_double_click)
{
  ref EDIT_LINE edit_line = edit.cr.edit_line;
  wstring^      line = new wchar [edit_line_get_max_length(edit_line)];
  int           len, i, col, x0, x;

  len = edit_line_get_length(edit_line);
  edit_line_get_line (edit_line, out line^);   // load line with trailing spaces

  if (edit.password_mode)
  {
    for (i=0; i<edit_line_get_max_length(edit_line); i++)
      if (line^[i] != L' ')
        line^[i] = L'*';
  }

  x0 = 2;   // left border

  for (col=edit.cr.scroll_x_offset; col<len; col++)
  {
    x = x0 + intern_wtext_width_of2 (line^[col:1], o.font);

    if (relative_x < (x0+x) / 2)
      break;

    x0 = x;
  }

  if ((!edit_line_get_extra_col(edit_line)) && (col == edit_line_get_max_length(edit_line)))
    col--;

  if (is_double_click)   // select word
  {
    int first_col, last_col;
    surround_word (line^[0:len], col, out first_col, out last_col);
    edit_set_marking_start_for_edit_and_combo (ref edit, first_col);
    edit_line_set_last_mark (ref edit_line, last => {active => true, col => last_col});
    edit_line_set_col (ref edit_line, last_col);
  }
  else if (is_drag && edit.line_marking_in_progress)   // drag mark
  {
    if (edit.line_marking_start_col < col)
    {
      edit_line_set_first_mark (ref edit.cr.edit_line, {active => true, col => edit.line_marking_start_col});
      edit_line_set_last_mark (ref edit.cr.edit_line, {active => true, col => col});
    }
    else
    {
      edit_line_set_first_mark (ref edit.cr.edit_line, {active => true, col => col});
      edit_line_set_last_mark (ref edit.cr.edit_line, {active => true, col => edit.line_marking_start_col});
    }
    edit_line_set_col (ref edit_line, col);
  }
  else  // set cursor position
  {
    edit_line_set_col (ref edit_line, col);
    edit_set_marking_start_for_edit_and_combo (ref edit, col);
  }

  update_scroll_x_edit_or_combo (ref o, ref edit, x_size);

  free line;

  {
    EVENT ev;
    clear ev;
    send_event (EVENT_EDIT_CHANGED, ev, o);
  }
}

//--------------------------------------------------------------------

void lb_select_all_in_same_folder (ref LISTBOX_INFO listbox, int current)
{
  int      count = wnb_text_lines (listbox.wtext);
  wstring^ line = new wchar[PREFIX_WLEN + listbox.line_length];
  int      length, ln;
  uint2    level_expected;

  wretrieve_text_line (ref listbox.wtext, current, out line^, out length);
  level_expected = ((PREFIX*)&line^)->level;

  for (ln=current; ln<=count; ln++)
  {
    wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

    if (((PREFIX*)&line^)->level != level_expected)
      break;

    ((PREFIX*)&line^)->selected = true;
    wupdate_text_line (ref listbox.wtext, ln, line^[0:length]);
  }

  for (ln=current; ln>=1; ln--)
  {
    wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

    if (((PREFIX*)&line^)->level != level_expected)
      break;

    ((PREFIX*)&line^)->selected = true;
    wupdate_text_line (ref listbox.wtext, ln, line^[0:length]);
  }

  free line;
}

//--------------------------------------------------------------------

bool in_same_folder (ref LISTBOX_INFO listbox, int first, int last)
{
  wstring^ line = new wchar[PREFIX_WLEN + listbox.line_length];
  int      length, ln;
  uint2    level_expected;
  bool     b = true;

  wretrieve_text_line (ref listbox.wtext, first, out line^, out length);
  level_expected = ((PREFIX*)&line^)->level;

  for (ln=first+1; ln<=last; ln++)
  {
    wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

    if (((PREFIX*)&line^)->level != level_expected)
    {
      b = false;
      break;
    }
  }

  _unused length;

  free line;

  return b;
}

//--------------------------------------------------------------------

void lb_deselect_all_lines (ref LISTBOX_INFO listbox)
{
  int      count = wnb_text_lines (listbox.wtext);
  wstring^ line = new wchar[PREFIX_WLEN + listbox.line_length];
  int      length, ln;

  for (ln=1; ln<=count; ln++)
  {
    wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

    {
      ref bool b = ((PREFIX*)&line^)->selected;
      if (b)
      {
        b = false;
        wupdate_text_line (ref listbox.wtext, ln, line^[0:length]);
      }
    }
  }

  free line;
}

//--------------------------------------------------------------------

void lb_select_deselect_line (ref LISTBOX_INFO listbox, int ln)
{
  wstring^ line = new wchar[PREFIX_WLEN + listbox.line_length];
  int      length;
  bool     b;

  wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

  b = ((PREFIX*)&line^)->selected;
  ((PREFIX*)&line^)->selected = !b;

  wupdate_text_line (ref listbox.wtext, ln, line^[0:length]);

  free line;
}

//--------------------------------------------------------------------

void lb_select_range (ref LISTBOX_INFO listbox, int first, int last)
{
  wstring^ line = new wchar[PREFIX_WLEN + listbox.line_length];
  int      length, ln;

  for (ln=first; ln<=last; ln++)
  {
    wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

    {
      ref bool b = ((PREFIX*)&line^)->selected;
      if (!b)
      {
        b = true;
        wupdate_text_line (ref listbox.wtext, ln, line^[0:length]);
      }
    }
  }

  free line;
}

//--------------------------------------------------------------------

bool is_lb_line_selected (ref LISTBOX_INFO listbox, int ln)
{
  wstring^ line = new wchar[PREFIX_WLEN + listbox.line_length];
  int      length;
  bool     b;

  wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);
  _unused length;

  b = ((PREFIX*)&line^)->selected;

  free line;
  return b;
}

//--------------------------------------------------------------------

void select_deselect_listbox_line (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, bool using_mouse, bool left_button,
                                   int dialog_x, int dialog_y)
{
  if (!listbox.allow_user_selection || listbox.ln == 0)
    return;

  if (!listbox.multiple_mode)    // SINGLE LINE SELECTION
  {
    // single selection mode
    if (listbox.ln == listbox.selected_line)
    {
      // deselect current line
      listbox.selected_line = 0;

      {
        EVENT e;
        clear e;
        e.line_nr = listbox.ln;
        e.x = ui_unscale(dialog_x);
        e.y = ui_unscale(dialog_y);
        send_event (EVENT_LISTBOX_LINE_DESELECTED, e, o);
      }
    }
    else   // deselect old line & select new line
    {
      // select new line
      listbox.selected_line = listbox.ln;

      {
        EVENT e;
        clear e;
        if (using_mouse)
          e.key = left_button ? 1 : 2;
        e.line_nr = listbox.ln;
        e.x = ui_unscale(dialog_x);
        e.y = ui_unscale(dialog_y);
        send_event (EVENT_LISTBOX_LINE_SELECTED, e, o);
      }
    }
  }
  else  // MULTI-LINE SELECTION
  {
    if (!(g_shift_pressed | g_control_pressed) ||
        listbox.selected_line == 0 ||
        !in_same_folder (ref listbox, min(listbox.selected_line, listbox.ln),
                                      max(listbox.selected_line, listbox.ln)))
    {
      if (!is_lb_line_selected (ref listbox, listbox.ln))
      {
        // deselect all lines except this one
        lb_deselect_all_lines (ref listbox);
        lb_select_deselect_line (ref listbox, listbox.ln);
      }

      listbox.selected_line = listbox.ln;
    }
    else
    {
      if (g_shift_pressed)
      {
        // select/deselect a range (from listbox.selected_line to listbox.ln) provided they are in same folder !
        lb_deselect_all_lines (ref listbox);
        lb_select_range (ref listbox,
                             min(listbox.selected_line, listbox.ln),
                             max(listbox.selected_line, listbox.ln));
      }
      else if (g_control_pressed)
      {
        // select/deselect listbox.ln provided it is in same folder as listbox.selected_line
        lb_select_deselect_line (ref listbox, listbox.ln);

        // set start of range
        listbox.selected_line = listbox.ln;
      }
    }

    {
      EVENT e;
      clear e;
      if (using_mouse)
        e.key = left_button ? 1 : 2;
      e.line_nr = listbox.ln;      // we need to know the line nr in case of right mouse button click
      e.x = ui_unscale(dialog_x);
      e.y = ui_unscale(dialog_y);
      send_event (EVENT_LISTBOX_LINE_SELECTED, e, o);
    }
  }

  if (using_mouse && left_button)
  {
    EVENT e;
    clear e;
    e.key = left_button ? 1 : 2;
    e.line_nr = listbox.ln;
    send_event (EVENT_LISTBOX_DRAG, e, o);

#if WINDOWS
    ReleaseCapture();    // don't keep mouse when dragging
#endif

    g_drag_selected = true;
    g_drag_moved = false;
    set_cursor_shape ();
  }

  repaint_control (o);
}

//--------------------------------------------------------------------

// called when user clicked below active lines

void unselect_all_listbox_lines (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, bool using_mouse, bool left_button,
                                     int dialog_x, int dialog_y)
{
  if (!listbox.allow_user_selection)
    return;

  if (!listbox.multiple_mode)    // SINGLE LINE SELECTION
  {
    if (listbox.selected_line > 0)   // some line is current selected
    {
      EVENT e;
      clear e;
      e.line_nr = listbox.selected_line;   // unselect it

      // deselect current line before sending event
      listbox.selected_line = 0;

      e.x = ui_unscale(dialog_x);
      e.y = ui_unscale(dialog_y);
      send_event (EVENT_LISTBOX_LINE_DESELECTED, e, o);
    }
  }
  else  // MULTI-LINE SELECTION
  {
    lb_deselect_all_lines (ref listbox);
    listbox.selected_line = 0;

    {
      EVENT e;
      clear e;
      if (using_mouse)
        e.key = left_button ? 1 : 2;
      e.x = ui_unscale(dialog_x);
      e.y = ui_unscale(dialog_y);
      send_event (EVENT_LISTBOX_LINE_SELECTED, e, o);
    }
  }

  if (using_mouse && left_button)
  {
    EVENT e;
    clear e;
    e.key = left_button ? 1 : 2;
    e.line_nr = 0;
    send_event (EVENT_LISTBOX_DRAG, e, o);

#if WINDOWS
    ReleaseCapture();    // don't keep mouse when dragging
#endif

    g_drag_selected = true;
    g_drag_moved = false;
    set_cursor_shape ();
  }

  repaint_control (o);
}

//--------------------------------------------------------------------

void listbox_up (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int nb_lines)
{
  if (listbox.ln > 1)
  {
    listbox.ln -= nb_lines;
    if (listbox.ln < 1)
      listbox.ln = 1;

    if (listbox.ln < listbox.page)
      listbox.page = listbox.ln;

    repaint_control (o);
  }
}

//--------------------------------------------------------------------

void listbox_page_up (ref CONTROL_INFO o, ref LISTBOX_INFO listbox)
{
  listbox_up (ref o, ref listbox, listbox.nb_screen_lines);
}

//--------------------------------------------------------------------

void listbox_scroll_up (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int nb_lines)
{
  int page = listbox.page - nb_lines;

  if (page < 1)
    page = 1;

  if (listbox.page != page)
  {
    listbox.page = page;

    if (listbox.ln > listbox.page + listbox.nb_screen_lines - 1)
    {
      listbox.ln = listbox.page + listbox.nb_screen_lines - 1;
    }

    repaint_control (o);
  }
}

//--------------------------------------------------------------------

void listbox_roll_up (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int nb_lines)
{
  int i;
  if (listbox.ln == 1)
    return;

  for (i=0; i<nb_lines; i++)
  {
    if (listbox.ln == 1)
      break;

    listbox.ln--;
    if (listbox.page > 1)
      listbox.page--;
  }

  repaint_control (o);
}

//--------------------------------------------------------------------

void listbox_down (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int nb_lines)
{
  int count = wnb_text_lines (listbox.wtext);

  if (listbox.ln > 0 && listbox.ln < count)
  {
    listbox.ln += nb_lines;
    if (listbox.ln > count)
      listbox.ln = count;

    if (listbox.ln > listbox.page + listbox.nb_screen_lines - 1)
      listbox.page = listbox.ln - (listbox.nb_screen_lines - 1);

    if (listbox.page < 1)
      listbox.page = 1;

    repaint_control (o);
  }
}

//--------------------------------------------------------------------

void listbox_page_down (ref CONTROL_INFO o, ref LISTBOX_INFO listbox)
{
  listbox_down (ref o, ref listbox, listbox.nb_screen_lines);
}

//--------------------------------------------------------------------

void listbox_scroll_down (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int nb_lines)
{
  int count = wnb_text_lines (listbox.wtext);
  int page = listbox.page + nb_lines;

  if (page + listbox.nb_screen_lines - 1 > count)
  {
    page = count - listbox.nb_screen_lines + 1;
  }

  if (page < 1)
    page = 1;

  if (listbox.page != page)
  {
    listbox.page = page;

    if (listbox.ln < page)
      listbox.ln = page;

    repaint_control (o);
  }
}

//--------------------------------------------------------------------

void listbox_roll_down (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int nb_lines)
{
  int count = wnb_text_lines (listbox.wtext);
  int i;

  if (listbox.ln == count)
    return;

  for (i=0; i<nb_lines; i++)
  {
    if (listbox.ln == count)
      break;

    listbox.ln++;
    listbox.page++;

    if (listbox.page + listbox.nb_screen_lines - 1 > count)
    {
      listbox.page = count - listbox.nb_screen_lines + 1;
      if (listbox.page < 1)
        listbox.page = 1;
    }
  }

  repaint_control (o);
}

//--------------------------------------------------------------------

void select_combo_listbox_line (ref CONTROL_INFO o,
                                ref EDIT_INFO    edit,
                                ref LISTBOX_INFO listbox)
{
  ref EDIT_LINE ed = edit.cr.edit_line;
  wstring^      line;
  int           length;
  EVENT e;

  if (listbox.ln == 0)
    return;

  line = new wchar [PREFIX_WLEN + listbox.line_length];

  edit.cr.scroll_x_offset = 0;

  wretrieve_text_line (ref listbox.wtext, listbox.ln, out line^, out length);

  length -= PREFIX_WLEN;

  edit_line_set_line (ref ed, line^[PREFIX_WLEN:length]);
  edit_line_set_col (ref ed, length);

  free line;

  // select new line
  listbox.selected_line = listbox.ln;

  clear e;
  e.line_nr = listbox.ln;
  send_event (EVENT_LISTBOX_LINE_SELECTED, e, o);

  repaint_control (o);   // repaint area below listbox
  o.combo.lb.listbox_shown = false;   // remove listbox

  send_event (EVENT_EDIT_CHANGED, e, o);
}

//--------------------------------------------------------------------

void click_listbox (ref CONTROL_INFO o,
                        int          x,
                        int          y,
                    ref LISTBOX_INFO listbox,
                        int          y_size,
                        bool         left_button,
                        int          dialog_x,
                        int          dialog_y)
{
  int ln, margin;

  assert o.typ == TYP_LISTBOX || o.typ == TYP_TREE || o.typ == TYP_COMBO;

  listbox.show_focus = false;   // do not show focus

  margin = (o.typ != TYP_COMBO || wnb_text_lines (listbox.wtext) > listbox.nb_screen_lines)
         ? listbox.arrow_box_width : 0;

  if (x >= 2 && x < o.x_size-2-margin && y >= 2 && y < y_size-2)    // click on listbox text line
  {
    if (left_button)
    {
      ln = listbox.page + (y-2)/listbox.line_height;

      if (ln <= wnb_text_lines (listbox.wtext))
      {
        listbox.ln = ln;

        if (o.typ == TYP_COMBO)
        {
          select_combo_listbox_line (ref o, ref o.combo.edit, ref o.combo.listbox);
        }
        else
        {
          if (o.typ == TYP_TREE)
          {
            PREFIX prefix;

            {
              wstring^ line = new wchar [PREFIX_WLEN];
              int      length;
              wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);
              _unused length;
              prefix = *(PREFIX*)&line^;
              free line;
            }

            if (prefix.is_folder)
            {
              int twidth = intern_wtext_width_of2 (L" ", o.font);
              int width1 = (int)((prefix.level & (uint2)(MAX_TREE_LEVELS-1)) * 3) * twidth;
              int width2 = width1 + 4 * twidth;

              if (x-2 >= width1 && x-2 < width2)   // expand/collapse line
              {
                EVENT e;
                clear e;
                e.line_nr = ln;
                send_event (tree_line_is_expanded (ref listbox.wtext, ln, prefix.level) ? EVENT_TREE_COLLAPSED : EVENT_TREE_EXPANDED, e, o);
                return;
              }
            }
          }

          select_deselect_listbox_line (ref o, ref listbox, using_mouse => true, left_button => left_button, dialog_x, dialog_y);
        }
      }
      else  // click below active lines
      {
        unselect_all_listbox_lines (ref o, ref listbox, using_mouse => true, left_button => left_button, dialog_x, dialog_y);
      }
    }
  }
  else if (x >= o.x_size-2-listbox.arrow_box_width &&
           x < o.x_size-2 &&
           y >= 2 && y < y_size-2)
  {
    if (y < 2+listbox.arrow_box_height) // upper arrow box
    {
      listbox_scroll_up (ref o, ref listbox, 1);
    }
    else if (y >= y_size-2-listbox.arrow_box_height)  // lower arrow box
    {
      listbox_scroll_down (ref o, ref listbox, 1);
    }
    else   // click on scrollbar area
    {
      int sy, scrollbox_y_offset, scrollbox_height;

      compute_scrollbox_data (    height                     => y_size-ui_scale(4),
                                  scrollbar_arrow_box_height => listbox.arrow_box_height,
                                  current_page_nr            => listbox.page,
                                  nb_listbox_lines           => wnb_text_lines (listbox.wtext),
                                  nb_screen_lines            => listbox.nb_screen_lines,
                              out scrollbox_y_offset         => scrollbox_y_offset,
                              out scrollbox_height           => scrollbox_height);

      sy = 2 + listbox.arrow_box_height;   // add top border

      if (y < sy + scrollbox_y_offset)
      {
        // free area above box
        listbox_scroll_up (ref o, ref listbox, listbox.nb_screen_lines);
      }
      else if (y >= sy + scrollbox_y_offset + scrollbox_height)
      {
        // free area below box
        listbox_scroll_down (ref o, ref listbox, listbox.nb_screen_lines);
      }
      else
      {
        listbox.clicked_scrollbox_y = y;
        listbox.clicked_page        = listbox.page;
      }
    }
  }
}

//--------------------------------------------------------------------

void drop_on_listbox (ref CONTROL_INFO o,
                          int          x,
                          int          y,
                      ref LISTBOX_INFO listbox,
                          int          y_size)
{
  int ln;

  assert o.typ == TYP_LISTBOX || o.typ == TYP_TREE;

  if (x >= 2 && x < o.x_size-2 && y >= 2 && y < y_size-2)    // click on listbox
  {
    ln = listbox.page + (y-2)/listbox.line_height;

    if (ln < 1)
      ln = 1;
    if (ln > wnb_text_lines (listbox.wtext))
      ln = wnb_text_lines (listbox.wtext) + 1;

    if (!g_control_pressed)
    {
      if (&o != g_last_control_left_clicked || (ln > wnb_text_lines (listbox.wtext) || !is_lb_line_selected (ref listbox, ln)))
      {
        EVENT e;
        clear e;
        e.line_nr = ln;  // can be out of range
        send_event (EVENT_LISTBOX_DROP, e, o);
      }
    }
  }
}

//--------------------------------------------------------------------

void drop_on_control (ref CONTROL_INFO o,
                       int          x,
                       int          y)
{
  EVENT e;
  _unused x;
  _unused y;
  clear e;
  send_event (EVENT_DROP_ON_CONTROL, e, o);
}

//--------------------------------------------------------------------

void scroll_up (ref CONTROL_INFO o, ref SCROLL_INFO scroll, int nb_lines)
{
  int page = scroll.page - nb_lines;

  if (page < 1)
    page = 1;

  if (scroll.page != page)
  {
    EVENT event;

    scroll.page = page;
    repaint_control (o);

    clear event;
    event.x = page - 1;
    event.y = page - 1;
    send_event (EVENT_SCROLLBAR_MOVED, event, o);
  }
}

//--------------------------------------------------------------------

void scroll_page_up (ref CONTROL_INFO o, ref SCROLL_INFO scroll)
{
  scroll_up (ref o, ref scroll, scroll.shown);
}

//--------------------------------------------------------------------

void scroll_down (ref CONTROL_INFO o, ref SCROLL_INFO scroll, int nb_lines)
{
  int page = scroll.page + nb_lines;

  if (page + scroll.shown - 1 > scroll.range)
  {
    page = scroll.range - scroll.shown + 1;
  }

  if (page < 1)
    page = 1;

  if (scroll.page != page)
  {
    EVENT event;

    scroll.page = page;
    repaint_control (o);

    clear event;
    event.x = page - 1;
    event.y = page - 1;
    send_event (EVENT_SCROLLBAR_MOVED, event, o);
  }
}

//--------------------------------------------------------------------

void scroll_page_down (ref CONTROL_INFO o, ref SCROLL_INFO scroll)
{
  scroll_down (ref o, ref scroll, scroll.shown);
}

//--------------------------------------------------------------------

void scroll_top (ref CONTROL_INFO o, ref SCROLL_INFO scroll)
{
  if (scroll.page > 1)
  {
    EVENT event;

    scroll.page = 1;
    repaint_control (o);

    clear event;
    event.x = 0;
    event.y = 0;
    send_event (EVENT_SCROLLBAR_MOVED, event, o);
  }
}

//--------------------------------------------------------------------

void scroll_bottom (ref CONTROL_INFO o, ref SCROLL_INFO scroll)
{
  int page = scroll.range - scroll.shown + 1;
  if (page < 1)
    page = 1;

  if (scroll.page != page)
  {
    EVENT event;

    scroll.page = page;
    repaint_control (o);

    clear event;
    event.x = page - 1;
    event.y = page - 1;
    send_event (EVENT_SCROLLBAR_MOVED, event, o);
  }
}

//--------------------------------------------------------------------

void click_hscroll (ref CONTROL_INFO o,
                        int          x,
                    ref SCROLL_INFO  scroll)
{
  if (x < scroll.arrow_box_width_or_height) // left arrow box
  {
    scroll_up (ref o, ref scroll, scroll.small_y_increment);
  }
  else if (x >= o.x_size-scroll.arrow_box_width_or_height)  // right arrow box
  {
    scroll_down (ref o, ref scroll, scroll.small_y_increment);
  }
  else   // click on scrollbar area
  {
    int sx, scrollbox_x_offset, scrollbox_width;

    compute_scrollbox_data (    height                     => o.x_size,
                                scrollbar_arrow_box_height => scroll.arrow_box_width_or_height,
                                current_page_nr            => scroll.page,
                                nb_listbox_lines           => scroll.range,
                                nb_screen_lines            => scroll.shown,
                            out scrollbox_y_offset         => scrollbox_x_offset,
                            out scrollbox_height           => scrollbox_width);

    sx = scroll.arrow_box_width_or_height;

    if (x < sx + scrollbox_x_offset)
    {
      // free area above box
      scroll_up (ref o, ref scroll, scroll.shown);
    }
    else if (x >= sx + scrollbox_x_offset + scrollbox_width)
    {
      // free area below box
      scroll_down (ref o, ref scroll, scroll.shown);
    }
    else
    {
      scroll.clicked_scrollbox_c = x;
      scroll.clicked_page        = scroll.page;
    }
  }
}

//--------------------------------------------------------------------

void click_vscroll (ref CONTROL_INFO o,
                        int          y,
                    ref SCROLL_INFO  scroll)
{
  if (y < scroll.arrow_box_width_or_height) // upper arrow box
  {
    scroll_up (ref o, ref scroll, scroll.small_y_increment);
  }
  else if (y >= o.y_size-scroll.arrow_box_width_or_height)  // lower arrow box
  {
    scroll_down (ref o, ref scroll, scroll.small_y_increment);
  }
  else   // click on scrollbar area
  {
    int sy, scrollbox_y_offset, scrollbox_height;

    compute_scrollbox_data (    height                     => o.y_size,
                                scrollbar_arrow_box_height => scroll.arrow_box_width_or_height,
                                current_page_nr            => scroll.page,
                                nb_listbox_lines           => scroll.range,
                                nb_screen_lines            => scroll.shown,
                            out scrollbox_y_offset         => scrollbox_y_offset,
                            out scrollbox_height           => scrollbox_height);

    sy = scroll.arrow_box_width_or_height;

    if (y < sy + scrollbox_y_offset)
    {
      // free area above box
      scroll_up (ref o, ref scroll, scroll.shown);
    }
    else if (y >= sy + scrollbox_y_offset + scrollbox_height)
    {
      // free area below box
      scroll_down (ref o, ref scroll, scroll.shown);
    }
    else
    {
      scroll.clicked_scrollbox_c = y;
      scroll.clicked_page        = scroll.page;
    }
  }
}

//--------------------------------------------------------------------

void editbox_set_marking_start (ref EDITBOX_INFO editbox, int col, int ln)
{
  editbox.text_marking_in_progress = true;
  editbox.text_marking_start = {ln => ln, col => col};
  edit_text_set_marks (ref editbox.cur.text, editbox.text_marking_start, editbox.text_marking_start);
}

//--------------------------------------------------------------------

void click_editbox (ref CONTROL_INFO o, int x, int y, bool is_drag, bool is_double_click)
{
  ref EDITBOX_INFO editbox = o.editbox;
  ref EDIT_TEXT we = editbox.cur.text;
  int col = editbox.cur.scroll + (x + (editbox.font_width>>1)) / editbox.font_width;
  int ln  = editbox.cur.page + y / o.font.height;

  if (col < 0)
    col = 0;
  if (col >= edit_text_get_max_line_length (we))
    col = edit_text_get_max_line_length (we)-1;

  if (ln < 1)
    ln = 1;
  if (ln > edit_text_count_lines (we))
    ln = edit_text_count_lines (we);

  edit_text_set_col (ref we, col => col);
  edit_text_set_ln (ref we, ln => ln);

  if (is_double_click)   // select word
  {
    wchar[]^ line = new wchar[edit_text_get_max_line_length(editbox.cur.text)];
    int actual_length;
    int first_col, last_col;

    edit_text_get_line (ref editbox.cur.text, ln, out line^, out actual_length);
    surround_word (line^[0:actual_length], col, out first_col, out last_col);
    free line;

    editbox.text_marking_in_progress = true;
    editbox.text_marking_start = {ln => ln, col => first_col};
    edit_text_set_marks (ref editbox.cur.text,
                             mark1 => editbox.text_marking_start,
                             mark2 => {ln => ln, col => last_col});
    edit_text_set_col (ref we, col => last_col);
  }
  else if (is_drag)   // drag mark
  {
    edit_text_set_marks (ref editbox.cur.text,
                             mark1 => editbox.text_marking_start,
                             mark2 => {ln => ln, col => col});
  }
  else   // set cursor position
  {
    editbox_set_marking_start (ref editbox, col => col, ln => ln);
  }

  edit_text_set_redraw_screen_needed (ref we);
  repaint_control (o);

  {
    EVENT event;
    clear event;
    send_event (EVENT_EDITBOX_CHANGED, event, o);
  }
}

//--------------------------------------------------------------------

void trigger_radiobutton (CONTROL_INFO^ pc)
{
  ref CONTROL_INFO o = pc^;

  if (!o.radiobutton.modify_allowed)
    return;

  o.radiobutton.setting = true;
  repaint_control (o);

  {
    CONTROL_INFO^ l = pc;

    if (l != null)
    {
      for (;;)
      {
        if (l^.typ == TYP_RADIOBUTTON && l^.radiobutton.gid == o.radiobutton.gid &&
            l^.radiobutton.setting && l^.id != o.id)
        {
          l^.radiobutton.setting = false;
          repaint_control (l^);
        }

        l = l^.next;
        if (l == pc)
          break;
      }
    }
  }

  {
    EVENT e;
    clear e;
    send_event (EVENT_RADIOBUTTON_CHANGED, e, o);
  }
}

//--------------------------------------------------------------------

void unselect_text (ref EDIT_INFO edit)
{
  LINE_MARK m;
  clear m;
  edit_line_set_first_mark (ref edit.cr.edit_line, m);
  edit_line_set_last_mark (ref edit.cr.edit_line, m);
  edit.line_marking_in_progress = false;
}

//--------------------------------------------------------------------

public void mouse_click_left (CONTROL_INFO^ pc, int d_x, int d_y, bool is_double_click, int dialog_x, int dialog_y)
{
  {
    CONTROL_INFO^ old_focus = g_dialog_ptr->focus;

    if (old_focus != null && old_focus != pc)
    {
      repaint_control (old_focus^);   // repaint old area including listbox shown
      if (old_focus^.typ == TYP_COMBO)
      {
        old_focus^.combo.lb.listbox_shown = false;  // close open combobox that had focus
        unselect_text (ref old_focus^.combo.edit);
      }

      if (old_focus^.typ == TYP_EDIT)
        unselect_text (ref old_focus^.edit);
    }
  }

  if (pc != null)
  {
    ref CONTROL_INFO o = pc^;
    int x = d_x - o.x;
    int y = d_y - o.y;

    if (o.typ != TYP_TEXT && g_dialog_ptr->focus != pc)
    {
      g_dialog_ptr->focus = pc;
      send_event_new_focus (*g_dialog_ptr, 0);
    }

    switch (o.typ)
    {
      case TYP_TEXT:   // should never occur as clicking on it drags the dialog
        break;

      case TYP_EDIT:

        set_clicked_col_edit_or_combo (ref o,
                                           x,
                                       ref o.edit,
                                           o.x_size,
                                           is_drag => false,
                                           is_double_click);

        repaint_control (o);

        break;

      case TYP_CHECKBOX:
        if (!o.checkbox.modify_allowed)
          break;
        o.checkbox.setting = !o.checkbox.setting;
        repaint_control (o);

        {
          EVENT e;
          clear e;
          send_event (EVENT_CHECKBOX_CHANGED, e, o);
        }
        break;

      case TYP_RADIOBUTTON:
        trigger_radiobutton (pc);
        break;

      case TYP_BUTTON:
        o.button.pressed = true;
        repaint_control (o);
        break;

      case TYP_WINDOW:
        {
          EVENT e;
          clear e;
          e.x = ui_unscale(x);
          e.y = ui_unscale(y);
          send_event (EVENT_WINDOW_CLICKED, e, o);
        }
        break;

      case TYP_LISTBOX:
        click_listbox (ref o, x, y, ref o.listbox, o.y_size, left_button => true, dialog_x, dialog_y);
        break;

      case TYP_TREE:
        click_listbox (ref o, x, y, ref o.tree.listbox, o.y_size, left_button => true, dialog_x, dialog_y);
        break;

      case TYP_SCROLL:
        if (o.scroll.tscroll == HSCROLL)
          click_hscroll (ref o, x, ref o.scroll);
        else
          click_vscroll (ref o, y, ref o.scroll);
        break;

      case TYP_COMBO:
        if (y >= 0 && y < o.y_size)   // click on edit or arrow box
        {
          if (x < o.x_size - o.combo.listbox.arrow_box_width)     // click on edit
          {
            repaint_control (o);  // repaint edit + maybe listbox area
            o.combo.lb.listbox_shown = false;
            set_clicked_col_edit_or_combo (ref o, x,
                                           ref o.combo.edit,
                                               o.x_size - o.combo.listbox.arrow_box_width,
                                               is_drag => false,
                                               is_double_click);
          }
          else   // click on arrow box
          {
            bool b = o.combo.lb.listbox_shown;

            o.combo.lb.listbox_shown = true;
            repaint_control (o);  // repaint listbox area

            o.combo.lb.listbox_shown = !b;   // reserve flag
          }
        }
        else if (o.combo.lb.listbox_shown)
        {
          click_listbox (ref o, x, y + o.y - o.combo.lb.y, ref o.combo.listbox, o.combo.lb.y_size, left_button => true, dialog_x, dialog_y);
        }
        break;

      case TYP_EDITBOX:
        click_editbox (ref o, x, y, is_drag => g_shift_pressed && o.editbox.text_marking_in_progress,
                       is_double_click => is_double_click);
        break;

      default:
        break;
    }

    g_last_control_left_clicked = &o;
  }
}

//--------------------------------------------------------------------

// sets focus on the item just before context menu is shown when the button is released
// sends also event for click right on window.

public void mouse_click_right (CONTROL_INFO^ pc, int d_x, int d_y, int dialog_x, int dialog_y)
{
  CONTROL_INFO^    old_focus = g_dialog_ptr->focus;
  ref CONTROL_INFO o = pc^;
  int x = d_x - o.x;
  int y = d_y - o.y;

  if (o.typ != TYP_TEXT && g_dialog_ptr->focus != pc)
  {
    g_dialog_ptr->focus = pc;
    send_event_new_focus (*g_dialog_ptr, 0);
  }

  switch (o.typ)
  {
    case TYP_TEXT:
      break;

    case TYP_EDIT:
      set_clicked_col_edit_or_combo (ref o, x, ref o.edit, o.x_size, is_drag => false, is_double_click => false);
      repaint_control (o);
      break;

    case TYP_CHECKBOX:
    case TYP_RADIOBUTTON:
    case TYP_BUTTON:
      repaint_control (o);
      break;

    case TYP_WINDOW:
      {
        EVENT e;
        clear e;
        e.x = ui_unscale(x);
        e.y = ui_unscale(y);
        send_event (EVENT_WINDOW_CLICKED_RIGHT, e, o);
      }
      break;

    case TYP_LISTBOX:
      click_listbox (ref o, x, y, ref o.listbox, o.y_size, left_button => false, dialog_x, dialog_y);
      break;

    case TYP_TREE:
      click_listbox (ref o, x, y, ref o.tree.listbox, o.y_size, left_button => false, dialog_x, dialog_y);
      break;

    case TYP_SCROLL:
      break;

    case TYP_COMBO:
      if (y >= 0 && y < o.y_size)   // click on edit or arrow box
      {
        if (x < o.x_size - o.combo.listbox.arrow_box_width)     // click on edit
        {
          repaint_control (o);  // repaint edit + maybe listbox area
          o.combo.lb.listbox_shown = false;
          set_clicked_col_edit_or_combo
              (ref o,
                   x,
               ref o.combo.edit,
                   o.x_size - o.combo.listbox.arrow_box_width,
               is_drag => false,
               is_double_click => false);
        }
        else   // click on arrow box
        {
          repaint_control (o);  // repaint listbox area
        }
      }
      break;

    default:
      break;
  }

  if (old_focus != null && old_focus != g_dialog_ptr->focus)
  {
    repaint_control (old_focus^);
    if (old_focus^.typ == TYP_COMBO)
      old_focus^.combo.lb.listbox_shown = false;  // close open combobox that had focus
  }
}

//--------------------------------------------------------------------

// d_x, d_y : local dialog coordinates with border and title removed

public void mouse_click_left_released (CONTROL_INFO^ pc, int d_x, int d_y)
{
  if (g_drag_selected && pc != null)    // drop onto another control
  {
    ref CONTROL_INFO o = pc^;
    int x = d_x - o.x;
    int y = d_y - o.y;

    switch (pc^.typ)
    {
      case TYP_LISTBOX:
        drop_on_listbox (ref o, x, y, ref o.listbox, o.y_size);
        break;

      case TYP_TREE:
        drop_on_listbox (ref o, x, y, ref o.tree.listbox, o.y_size);
        break;

      case TYP_EDIT:
      case TYP_TEXT:
      case TYP_WINDOW:
        drop_on_control (ref o, x, y);
        break;

      default:
        break;
    }
  }

  if (g_dialog_ptr->focus != null)
  {
    ref CONTROL_INFO o = g_dialog_ptr->focus^;    // we work with the focus control
    int x = d_x - o.x;
    int y = d_y - o.y;

    switch (o.typ)
    {
      case TYP_BUTTON:
        if (o.button.pressed)
        {
          o.button.pressed = false;
          repaint_control (o);

          if (d_x != int'min)  // don't press button when leaving the control
          {
            EVENT e;
            clear e;
            send_event (EVENT_BUTTON_PRESSED, e, o);
          }
        }
        break;

      case TYP_LISTBOX:
        o.listbox.clicked_scrollbox_y = 0;
        break;

      case TYP_TREE:
        o.tree.listbox.clicked_scrollbox_y = 0;
        break;

      case TYP_COMBO:
        o.combo.listbox.clicked_scrollbox_y = 0;
        break;

      case TYP_WINDOW:
        {
          EVENT e;
          clear e;

          if (x >= 0 && x < o.x_size && y >= 0 && y < o.y_size)    // click in window
          {
            e.x = ui_unscale(x);
            e.y = ui_unscale(y);
          }
          else
          {
            e.x = int'min;
            e.y = int'min;
          }
          send_event (EVENT_WINDOW_DROP, e, o);
        }
        break;

      default:
        break;
    }
  }

  if (pc == null &&
#if ANDROID
      d_x >= g_dialog_ptr->rect.right - g_dialog_ptr->rect.left
               - 2*g_dialog_ptr->border_size - g_dialog_ptr->title_height &&
      d_x < g_dialog_ptr->rect.right - g_dialog_ptr->rect.left
               - 2*g_dialog_ptr->border_size &&
#endif
      d_y >= -g_dialog_ptr->title_height && d_y < 0 &&   // close button
      g_dialog_ptr->has_close_button)
  {
#if WINDOWS
    HWND hwnd = guihash . treat_delete_dialog (id => g_dialog_ptr->d);
    if (hwnd != 0)
      SendMessageA (hwnd, WM_USER + 22, 0, 0);  // close window
#elif ANDROID
    guiandroid.close_dialog (d => g_dialog_ptr->d);

#else
      bad
#endif
  }
}

//--------------------------------------------------------------------

public void mouse_click_right_released (CONTROL_INFO^ pc,
                                        int d_x,      int d_y,       // relative to inside the dialog (less border and title)
                                        int dialog_x, int dialog_y)  // relative to dialog
{
  if (pc != null)
  {
    ref CONTROL_INFO o = pc^;

    switch (o.typ)
    {
      case TYP_WINDOW:
        {
          EVENT e;
          clear e;
          e.x = ui_unscale(d_x - o.x);  // position relative to control (coordinates can be outside window control)
          e.y = ui_unscale(d_y - o.y);
          send_event (EVENT_WINDOW_DROP_RIGHT, e, o);  // right mouse button released in window control
        }
        break;

      default:
        break;
    }
  }

  if (g_dialog_ptr->focus != null)
  {
    ref CONTROL_INFO o = g_dialog_ptr->focus^;    // we work with the focus control
    int x = d_x - o.x;    // position inside the control
    int y = d_y - o.y;

    switch (o.typ)
    {
      case TYP_LISTBOX:
        {
          EVENT            e;
          ref LISTBOX_INFO l = o.listbox;

          clear e;

          if (x >= 2 && x < o.x_size-2-l.arrow_box_width && y >= 2 && y < o.y_size-2)
          {
            int ln = l.page + (y-2)/l.line_height;
            if (ln <= wnb_text_lines (l.wtext))
              e.line_nr = ln;
          }

          e.x = ui_unscale(dialog_x);   // coord relative to dialog
          e.y = ui_unscale(dialog_y);
          send_event (EVENT_CONTEXT_MENU, e, o);
        }
        break;

      case TYP_TREE:
        {
          EVENT            e;
          ref LISTBOX_INFO l = o.tree.listbox;

          clear e;

          if (x >= 2 && x < o.x_size-2-l.arrow_box_width && y >= 2 && y < o.y_size-2)
          {
            int ln = l.page + (y-2)/l.line_height;
            if (ln <= wnb_text_lines (l.wtext))
              e.line_nr = ln;
          }

          e.x = ui_unscale(dialog_x);   // coord relative to dialog
          e.y = ui_unscale(dialog_y);
          send_event (EVENT_CONTEXT_MENU, e, o);
        }
        break;

      default:
        {
          EVENT e;
          clear e;
          e.x = ui_unscale(dialog_x);   // coord relative to dialog
          e.y = ui_unscale(dialog_y);
          send_event (EVENT_CONTEXT_MENU, e, o);
        }
        break;
    }
  }
}

//--------------------------------------------------------------------

// the user drags a listbox bar

void execute_listbox_drag (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int rel_x, int rel_y, int y_size)
{
  int x = rel_x;
  int y = rel_y;
  int sh, scrollbox_y_offset, scrollbox_height, y0;
  int y_offset, move_height, line_offset, new_page;

  if (listbox.clicked_scrollbox_y == 0)    // no earlier click
    return;

  if (x >= o.x_size-2-3*listbox.arrow_box_width &&
      x <= o.x_size+2*listbox.arrow_box_width)
  {
    y0 = 2+listbox.arrow_box_height;
    if (y < y0)
      y = y0;

    y0 = y_size-2-listbox.arrow_box_height;
    if (y > y0)
      y = y0;

    compute_scrollbox_data (y_size-ui_scale(4),
                            listbox.arrow_box_height,
                            listbox.page,
                            wnb_text_lines (listbox.wtext),
                            listbox.nb_screen_lines,
                            out scrollbox_y_offset,
                            out scrollbox_height);
    _unused scrollbox_y_offset;

    sh = y_size - ui_scale(4) - 2 * listbox.arrow_box_height;


    // compute y-offset in pixels

    y_offset = y - listbox.clicked_scrollbox_y;

    move_height = sh - scrollbox_height;
    if (move_height < 1)
      move_height = 1;

    // can become negative
    line_offset = y_offset
                  * (wnb_text_lines (listbox.wtext)
                      - listbox.nb_screen_lines)
                  / move_height;

    new_page = listbox.clicked_page + line_offset;

    if (new_page > wnb_text_lines (listbox.wtext)
                    - listbox.nb_screen_lines + 1)
      new_page = wnb_text_lines (listbox.wtext)
                  - listbox.nb_screen_lines + 1;

    if (new_page < 1)
      new_page = 1;

    if (new_page == listbox.page)   // same page : nothing to do
      return;

    listbox.page = new_page;

    if (listbox.ln < new_page)
      listbox.ln = new_page;

    if (listbox.ln > new_page + listbox.nb_screen_lines - 1)
      listbox.ln = new_page + listbox.nb_screen_lines - 1;

    repaint_control (o);
  }
}

//--------------------------------------------------------------------

void execute_hscroll_drag (ref CONTROL_INFO o, ref SCROLL_INFO scroll, int rel_x)
{
  int x = rel_x;
  int sh, scrollbox_x_offset, scrollbox_width, x0;
  int x_offset, move_width, line_offset, new_page;

  if (scroll.clicked_scrollbox_c == 0)    // no earlier click
    return;

  x0 = scroll.arrow_box_width_or_height;
  if (x < x0)
    x = x0;

  x0 = o.x_size-scroll.arrow_box_width_or_height;
  if (x > x0)
    x = x0;

  compute_scrollbox_data (    o.x_size,
                              scroll.arrow_box_width_or_height,
                              scroll.page,
                              scroll.range,
                              scroll.shown,
                          out scrollbox_x_offset,
                          out scrollbox_width);
  _unused scrollbox_x_offset;

  sh = o.x_size - 2 * scroll.arrow_box_width_or_height;


  // compute y-offset in pixels

  x_offset = x - scroll.clicked_scrollbox_c;

  move_width = sh - scrollbox_width;
  if (move_width < 1)
    move_width = 1;

  // can become negative
  line_offset = x_offset * (scroll.range - scroll.shown) / move_width;

  new_page = scroll.clicked_page + line_offset;

  if (new_page > scroll.range - scroll.shown + 1)
    new_page = scroll.range - scroll.shown + 1;

  if (new_page < 1)
    new_page = 1;

  if (new_page == scroll.page)   // same page : nothing to do
    return;

  scroll.page = new_page;

  repaint_control (o);

  {
    EVENT event;
    clear event;
    event.x = new_page - 1;
    event.y = new_page - 1;
    send_event (EVENT_SCROLLBAR_MOVED, event, o);
  }
}

//--------------------------------------------------------------------

void execute_vscroll_drag (ref CONTROL_INFO o, ref SCROLL_INFO scroll, int rel_y)
{
  int y = rel_y;
  int sh, scrollbox_y_offset, scrollbox_height, y0;
  int y_offset, move_height, line_offset, new_page;

  if (scroll.clicked_scrollbox_c == 0)    // no earlier click
    return;

  y0 = scroll.arrow_box_width_or_height;
  if (y < y0)
    y = y0;

  y0 = o.y_size-scroll.arrow_box_width_or_height;
  if (y > y0)
    y = y0;

  compute_scrollbox_data (    o.y_size,
                              scroll.arrow_box_width_or_height,
                              scroll.page,
                              scroll.range,
                              scroll.shown,
                          out scrollbox_y_offset,
                          out scrollbox_height);
  _unused scrollbox_y_offset;

  sh = o.y_size - 2 * scroll.arrow_box_width_or_height;


  // compute y-offset in pixels

  y_offset = y - scroll.clicked_scrollbox_c;

  move_height = sh - scrollbox_height;
  if (move_height < 1)
    move_height = 1;

  // can become negative
  line_offset = y_offset * (scroll.range - scroll.shown) / move_height;

  new_page = scroll.clicked_page + line_offset;

  if (new_page > scroll.range - scroll.shown + 1)
    new_page = scroll.range - scroll.shown + 1;

  if (new_page < 1)
    new_page = 1;

  if (new_page == scroll.page)   // same page : nothing to do
    return;

  scroll.page = new_page;

  repaint_control (o);

  {
    EVENT event;
    clear event;
    event.x = new_page - 1;
    event.y = new_page - 1;
    send_event (EVENT_SCROLLBAR_MOVED, event, o);
  }
}

//--------------------------------------------------------------------

public void mouse_drag (CONTROL_INFO^ pc, int d_x, int d_y)
{
  ref DIALOG_INFO d = *g_dialog_ptr;

  if (d.focus != null)  // focus object
  {
    ref CONTROL_INFO o = d.focus^;
    int x = d_x - o.x;
    int y = d_y - o.y;

    switch (o.typ)
    {
      case TYP_EDIT:
//        if (x >= 0 && x < o.x_size && y >= 0 && y < o.y_size)   // click on edit
        {
          set_clicked_col_edit_or_combo (ref o,
                                             x,
                                         ref o.edit,
                                             o.x_size,
                                             is_drag => true,
                                             is_double_click => false);
          repaint_control (o);
        }
        break;

      case TYP_LISTBOX:
        execute_listbox_drag (ref o, ref o.listbox, x, y, o.y_size);
        break;

      case TYP_TREE:
        execute_listbox_drag (ref o, ref o.tree.listbox, x, y, o.y_size);
        break;

      case TYP_SCROLL:
        if (o.scroll.tscroll == HSCROLL)
          execute_hscroll_drag (ref o, ref o.scroll, x);
        else
          execute_vscroll_drag (ref o, ref o.scroll, y);
        break;

      case TYP_COMBO:
        if (o.combo.lb.listbox_shown)
        {
          int rel_y = d_y - o.combo.lb.y;
          if (rel_y >= 0 && rel_y < o.combo.lb.y_size)   // click on listbox
            execute_listbox_drag (ref o, ref o.combo.listbox, x, rel_y, o.combo.lb.y_size);
        }
        else
        {
//          if (y >= 0 && y < o.y_size && x < o.x_size - o.combo.listbox.arrow_box_width)   // click on edit
          {
            set_clicked_col_edit_or_combo (ref o, x,
                                           ref o.combo.edit,
                                               o.x_size - o.combo.listbox.arrow_box_width,
                                               is_drag => true,
                                               is_double_click => false);
            repaint_control (o);
          }
        }
        break;

      case TYP_EDITBOX:
        click_editbox (ref o, x, y, is_drag => true, is_double_click => false);
        break;

      default:
        break;
    }
  }

  if (pc != null)     // control under mouse
  {
    ref CONTROL_INFO o = pc^;
    int relative_x = d_x - o.x;
    int relative_y = d_y - o.y;

    switch (o.typ)
    {
      case TYP_WINDOW:
        {
          EVENT e;
          clear e;
          e.x = ui_unscale(relative_x);
          e.y = ui_unscale(relative_y);
          send_event (EVENT_WINDOW_DRAG, e, o);
        }
        break;

      default:
        break;
    }
  }
}

//--------------------------------------------------------------------

void hover_listbox (ref CONTROL_INFO o,
                        int          relative_x,
                        int          relative_y,
                    ref LISTBOX_INFO listbox,
                        int          lb_y_size)
{
  int margin;

  assert o.typ == TYP_LISTBOX || o.typ == TYP_TREE || o.typ == TYP_COMBO;

  margin = (o.typ != TYP_COMBO || wnb_text_lines (listbox.wtext) > listbox.nb_screen_lines)
         ? listbox.arrow_box_width : 0;

  if (relative_x >= 2 && relative_x < o.x_size-2-margin &&
      relative_y >= 2 && relative_y < lb_y_size-2)
  {
    // click on listbox text line

    if (listbox.page + (relative_y-2)/listbox.line_height <= wnb_text_lines (listbox.wtext))
    {
      listbox.ln = listbox.page + (relative_y-2) / listbox.line_height;

      if (o.typ == TYP_COMBO)
        listbox.selected_line = listbox.ln;
      else
        listbox.show_focus = true;

      repaint_control (o);
    }
  }
}

//--------------------------------------------------------------------

public void mouse_hover (CONTROL_INFO^ pc, int d_x, int d_y)
{
  {
    ref DIALOG_INFO d = *g_dialog_ptr;

    if (d.focus != null)  // focus object
    {
      ref CONTROL_INFO o = d.focus^;
      int relative_x = d_x - o.x;
      int relative_y = d_y - o.y;

      switch (o.typ)
      {
        case TYP_COMBO:
          relative_y = d_y - o.combo.lb.y;
          if (o.combo.lb.listbox_shown && relative_y >= 0 && relative_y < o.combo.lb.y_size)
            hover_listbox (ref o, relative_x, relative_y, ref o.combo.listbox, o.combo.lb.y_size);
          break;

        default:
          break;
      }
    }
  }

  if (pc != null)     // control under mouse
  {
    ref CONTROL_INFO o = pc^;
    int relative_x = d_x - o.x;
    int relative_y = d_y - o.y;

    switch (o.typ)
    {
      case TYP_WINDOW:
        {
          EVENT e;
          clear e;
          e.x = ui_unscale(relative_x);
          e.y = ui_unscale(relative_y);
          send_event (EVENT_WINDOW_HOVER, e, o);
        }
        break;

      default:
        break;
    }
  }
}

//--------------------------------------------------------------------

public void mouse_wheel (int delta)
{
  ref DIALOG_INFO d = *g_dialog_ptr;

  if (d.focus == null)   // no control selected
    return;

  {
    ref CONTROL_INFO o = d.focus^;
    switch (o.typ)
    {
      case TYP_LISTBOX:
        o.listbox.show_focus = true;
        if (delta > 0)
          listbox_roll_up (ref o, ref o.listbox, delta);
        else
          listbox_roll_down (ref o, ref o.listbox, -delta);
        break;

      case TYP_TREE:
        o.tree.listbox.show_focus = true;
        if (delta > 0)
          listbox_roll_up (ref o, ref o.tree.listbox, delta);
        else
          listbox_roll_down (ref o, ref o.tree.listbox, -delta);
        break;

      case TYP_SCROLL:
        if (delta > 0)
          scroll_up (ref o, ref o.scroll, delta*o.scroll.small_y_increment);
        else
          scroll_down (ref o, ref o.scroll, -delta*o.scroll.small_y_increment);
        break;

      case TYP_COMBO:
        if (!o.combo.lb.listbox_shown)
          break;
        if (delta > 0)
          listbox_roll_up (ref o, ref o.combo.listbox, delta);
        else
          listbox_roll_down (ref o, ref o.combo.listbox, -delta);
        o.combo.listbox.selected_line = o.combo.listbox.ln;
        break;

      default:
        break;
    }
  }
}

//--------------------------------------------------------------------

bool non_empty_text_marked_for_edit_or_combo (EDIT_INFO edit)
{
  LINE_MARK first, last;

  if (!edit.line_marking_in_progress)
    return false;

  edit_line_get_first_mark (edit.cr.edit_line, out first);
  edit_line_get_last_mark  (edit.cr.edit_line, out last);

  return first.col < last.col;
}

//--------------------------------------------------------------------

void cut_marked_text_for_edit_or_combo (ref EDIT_INFO edit, bool save_in_clipboard)
{
  if (!edit.line_marking_in_progress)
    return;

  if (save_in_clipboard)
  {
    wstring^ p = edit_line_get_marked_text (ref edit.cr.edit_line);
    if (p^'length > 0)
      save_data_to_clipboard (p^, _CF_UNICODETEXT);
    free p;
  }

  if (!edit_line_get_modify_allowed (edit.cr.edit_line))
    return;

  // delete the marked text and set col to its start
  // returns 0 if OK, 1 if no text marked
  edit_line_delete_block (ref edit.cr.edit_line);

  {
    LINE_MARK m;
    clear m;
    edit_line_set_first_mark (ref edit.cr.edit_line, m);
    edit_line_set_last_mark (ref edit.cr.edit_line, m);
  }
  edit.line_marking_in_progress = false;
}

//--------------------------------------------------------------------

void init_marks_for_edit_or_combo (int key, ref EDIT_INFO edit)
{
  if ((key & KEY_SHIFT) > 0)   // shift pressed
  {
    if (!edit.line_marking_in_progress)
    {
      int old_col = edit_line_get_col (edit.cr.edit_line);
      edit_set_marking_start_for_edit_and_combo (ref edit, old_col);
    }
  }
  else   // shift no longer pressed
  {
    if (edit.line_marking_in_progress)
    {
      LINE_MARK m;
      clear m;
      edit_line_set_first_mark (ref edit.cr.edit_line, m);
      edit_line_set_last_mark (ref edit.cr.edit_line, m);
      edit.line_marking_in_progress = false;
    }
  }
}

//--------------------------------------------------------------------

void paste_from_clipboard_for_edit_or_combo (ref EDIT_INFO edit)
{
  byte[]^   p;
  wstring^  fragment;

  if (!edit_line_get_modify_allowed (edit.cr.edit_line))
    return;

  p = load_data_from_clipboard (_CF_UNICODETEXT);
  if (p == null)
    return;

  fragment = new wchar [p^'length / 2];
  fragment^'byte = p^[0 : fragment^'length * 2];
  free p;

  // insert text at cursor col, set marks around inserted text, put cursor at end of insertion
  // returns 0 if OK, 1 if text too large
  edit_line_insert_text_block (ref edit.cr.edit_line, fragment^[0 : wstrlen(fragment^)]);

  {
    LINE_MARK m;
    clear m;
    edit_line_set_first_mark (ref edit.cr.edit_line, m);
    edit_line_set_last_mark (ref edit.cr.edit_line, m);
    edit.line_marking_in_progress = false;
  }

  free fragment;
}

//--------------------------------------------------------------------

void copy_to_clipboard_for_edit_or_combo (ref EDIT_INFO edit)
{
  wstring^ p;
  if (!edit.line_marking_in_progress)
    return;
  p = edit_line_get_marked_text (ref edit.cr.edit_line);
  if (p^'length > 0)
    save_data_to_clipboard (p^, _CF_UNICODETEXT);
  free p;
}

//--------------------------------------------------------------------

int compare_marks_for_edit_or_combo (LINE_MARK mark1, LINE_MARK mark2)
{
  if (mark1.col < mark2.col)
    return -1;
  if (mark1.col > mark2.col)
    return +1;
  return 0;
}

//--------------------------------------------------------------------

// returns 0 if we're on marking start, -1 if we are before, +1 if we are after

int cmp_to_marking_start_for_edit_or_combo (EDIT_INFO edit)
{
  return compare_marks_for_edit_or_combo (mark1 => {active => true, col => edit_line_get_col(edit.cr.edit_line)},
                                          mark2 => {active => true, col => edit.line_marking_start_col});
}

//--------------------------------------------------------------------

void copy_single_edit_info (ref SINGLE_EDIT_INFO source, out SINGLE_EDIT_INFO target)
{
  clear target;
  edit_line_unsafe_copy (ref source.edit_line, out target.edit_line);
  target.scroll_x_offset = source.scroll_x_offset;
}

//--------------------------------------------------------------------

void clone_single_edit_info (ref SINGLE_EDIT_INFO source, out SINGLE_EDIT_INFO target)
{
  clear target;
  edit_line_clone (source.edit_line, out target.edit_line);
  target.scroll_x_offset = source.scroll_x_offset;
}

//--------------------------------------------------------------------

public
void save_edit_undo_context (ref EDIT_INFO edit)
{
  int i;

  if (edit.undo_slots_filled == MAX_EDIT_UNDOS)
  {
    edit_line_dispose (ref edit.undo[0].edit_line);

    for (i=0; i<MAX_EDIT_UNDOS-1; i++)
      copy_single_edit_info (ref source => edit.undo[i+1], out target => edit.undo[i]);

    edit.next_undo_slot--;
    edit.undo_slots_filled--;
  }

  for (i=edit.next_undo_slot; i<edit.undo_slots_filled; i++)
  {
    edit_line_dispose (ref edit.undo[i].edit_line);
    clear edit.undo[i];
  }
  edit.undo_slots_filled = edit.next_undo_slot;

  clone_single_edit_info (ref source => edit.cr, out target => edit.undo[edit.next_undo_slot]);
  edit.next_undo_slot++;
  edit.undo_slots_filled++;

  if (edit.next_undo_slot > 1)
  {
    if (edit_line_identical (edit.undo[edit.next_undo_slot-1].edit_line, edit.undo[edit.next_undo_slot-2].edit_line) &&
        edit.undo[edit.next_undo_slot-1].scroll_x_offset == edit.undo[edit.next_undo_slot-2].scroll_x_offset)
    {
      // no need to save context, it's identical
      edit.next_undo_slot--;
      edit.undo_slots_filled--;
      edit_line_dispose (ref edit.undo[edit.next_undo_slot].edit_line);
      clear edit.undo[edit.next_undo_slot];
    }
  }
}

//----------------------------------------------------------------------

void process_key_edit_or_combo (ref CONTROL_INFO o, int key, ref EDIT_INFO edit, int x_size, out bool key_was_treated)
{
  ref EDIT_LINE edit_line = edit.cr.edit_line;

  assert o.typ == TYP_EDIT || o.typ == TYP_COMBO;

  if (edit.undo_slots_filled == 0)     // make sure at least 1 slot is filled initially
    save_edit_undo_context (ref edit);

  // init the edit line as being not modified
  edit_line_set_line_modified (ref edit_line, line_modified => false);

  key_was_treated = true;

  if (key >= 32 && key <= 65535)
  {
    cut_marked_text_for_edit_or_combo (ref edit, save_in_clipboard => false);
    edit_line_achar (ref edit_line, (wchar)key);
  }
  else
  {
    switch (key & (~KEY_SHIFT))
    {
      case KEY_CMD + VK_INSERT:
        if ((key & KEY_SHIFT) > 0)
          paste_from_clipboard_for_edit_or_combo (ref edit);
        else
          edit_line_switch_insert (ref edit_line);
        break;

      case KEY_CMD + VK_HOME:
        init_marks_for_edit_or_combo (key, ref edit);
        edit_line_home (ref edit_line);
        break;

      case KEY_CMD + VK_END:
        init_marks_for_edit_or_combo (key, ref edit);
        edit_line_end (ref edit_line);
        break;

      case KEY_CMD + VK_LEFT:
        init_marks_for_edit_or_combo (key, ref edit);
        edit_line_cursor_left (ref edit_line);
        break;

      case KEY_CMD + VK_RIGHT:
        init_marks_for_edit_or_combo (key, ref edit);
        edit_line_cursor_right (ref edit_line);
        break;

      case KEY_CMD + KEY_CONTROL + VK_LEFT:
        init_marks_for_edit_or_combo (key, ref edit);
        if (edit.line_marking_in_progress)
        {
          int old_col = edit_line_get_col (edit_line);
          int f = cmp_to_marking_start_for_edit_or_combo(edit);
          edit_line_previous_word (ref edit_line, begin_of_word => (f <= 0));
          if (f == +1 && cmp_to_marking_start_for_edit_or_combo(edit) == -1)   // cursor was after, now it's before marking start
          {
            edit_line_set_col (ref edit_line, old_col);
            edit_line_previous_word (ref edit_line, begin_of_word => true);
          }
        }
        else
        {
          edit_line_previous_word (ref edit_line, begin_of_word => true);
        }
        break;

      case KEY_CMD + KEY_CONTROL + VK_RIGHT:
        init_marks_for_edit_or_combo (key, ref edit);
        if (edit.line_marking_in_progress)
        {
          int old_col = edit_line_get_col (edit_line);
          int f = cmp_to_marking_start_for_edit_or_combo(edit);
          edit_line_next_word (ref edit_line, begin_of_word => (f < 0));
          if (f == -1 && cmp_to_marking_start_for_edit_or_combo(edit) == +1)   // cursor was before, now it's after marking start
          {
            edit_line_set_col (ref edit_line, old_col);
            edit_line_next_word (ref edit_line, begin_of_word => false);
          }
        }
        else
        {
          edit_line_next_word (ref edit_line, begin_of_word => true);
        }
        break;

      case KEY_CMD + VK_DELETE:
        if (non_empty_text_marked_for_edit_or_combo (edit))
          cut_marked_text_for_edit_or_combo (ref edit, save_in_clipboard => (key & KEY_SHIFT) > 0);
        else
        {
          init_marks_for_edit_or_combo (key, ref edit);
          edit_line_delete (ref edit_line);
        }
        break;

      case 1:   // CONTROL-A : mark all
        init_marks_for_edit_or_combo (key, ref edit);
        edit_line_end (ref edit_line);         // col at the end
        edit_line_mark_all (ref edit_line);
        edit.line_marking_in_progress = true;  // must be true otherwise later control-c does not work
        edit.line_marking_start_col = 0;
        break;

      case 3:   // CONTROL-C : copy to clipboard
        copy_to_clipboard_for_edit_or_combo (ref edit);
        break;

      case 8:   // BACKSPACE
        if (non_empty_text_marked_for_edit_or_combo (edit))
          cut_marked_text_for_edit_or_combo (ref edit, save_in_clipboard => false);
        else
        {
          init_marks_for_edit_or_combo (key, ref edit);
          edit_line_backspace (ref edit_line);
        }
        break;

      case 20:   // CONTROL-T
        cut_marked_text_for_edit_or_combo (ref edit, save_in_clipboard => false);
        edit_line_delete_word (ref edit_line);
        break;

      case 22:   // CONTROL-V : paste from clipboard
        cut_marked_text_for_edit_or_combo (ref edit, save_in_clipboard => false);
        paste_from_clipboard_for_edit_or_combo (ref edit);
        break;

      case 24:   // CONTROL-X : cut
        cut_marked_text_for_edit_or_combo (ref edit, save_in_clipboard => true);
        break;

      case 25:   // CTRL-Y : redo
        if (edit_line_get_modify_allowed (edit.cr.edit_line) && edit.next_undo_slot < edit.undo_slots_filled)
        {
          edit.next_undo_slot++;
          clone_single_edit_info (ref source => edit.undo[edit.next_undo_slot-1], out target => edit.cr);
          unselect_text (ref edit);
        }
        break;

      case 26:   // CTRL-Z : undo
        if (edit_line_get_modify_allowed (edit.cr.edit_line) && edit.next_undo_slot > 1)
        {
          edit.next_undo_slot--;
          clone_single_edit_info (ref source => edit.undo[edit.next_undo_slot-1], out target => edit.cr);
          unselect_text (ref edit);
        }
        break;

      default:
        key_was_treated = false;
        break;
    }


    // update ending mark

    if (edit.line_marking_in_progress)
    {
      int col = edit_line_get_col (edit_line);
      if (edit.line_marking_start_col < col)
      {
        edit_line_set_first_mark (ref edit_line, {active => true, col => edit.line_marking_start_col});
        edit_line_set_last_mark (ref edit_line, {active => true, col => col});
      }
      else
      {
        edit_line_set_first_mark (ref edit_line, {active => true, col => col});
        edit_line_set_last_mark (ref edit_line, {active => true, col => edit.line_marking_start_col});
      }
    }
  }

  if (key_was_treated)
  {
    update_scroll_x_edit_or_combo (ref o, ref edit, x_size);

    if (key != 25 && key != 26)     // don't save undo context for undo/redo
      save_edit_undo_context (ref edit);

    repaint_control (o);
  }

  {
    EVENT e;
    clear e;
    send_event (EVENT_EDIT_CHANGED, e, o);
  }
}

//--------------------------------------------------------------------

void select_deselect_all_lb_lines (
       ref CONTROL_INFO o,
       ref LISTBOX_INFO listbox,
           bool         select)
{
  if (listbox.allow_user_selection && listbox.ln > 0 && listbox.multiple_mode)
  {
    // reserve space for a buffer
    wstring^  line = new wchar [PREFIX_WLEN + listbox.line_length];
    int       length, ln;

    for (ln=1; ln<=wnb_text_lines(listbox.wtext); ln++)
    {
      wretrieve_text_line (ref listbox.wtext, ln, out line^, out length);

      ((PREFIX*)&line^)->selected = select;

      wupdate_text_line (ref listbox.wtext, ln, line^[0:length]);
    }

    free line;

    repaint_control (o);

    {
      EVENT e;
      clear e;
      send_event (EVENT_LISTBOX_LINE_SELECTED, e, o);
    }
  }
}

//--------------------------------------------------------------------

// returns the target line, or 0 if there is none

int search_listbox_line (ref W_TEXT text,
                             int    starting_ln,
                             int    line_length,
                             int    hotkey_position,
                             wchar  hotkey,
                         out bool   match)
{
  int      size, length, j;
  wstring^ line1, line2;
  int      ln, best;

  match = false;

  if (starting_ln > wnb_text_lines (text))    // text is empty
    return 0;

  best = starting_ln;

  size = PREFIX_WLEN + line_length;

  line1 = new wchar[size];
  line2 = new wchar[size];

  wretrieve_text_line (ref text, starting_ln, out line1^, out length);
  line1^[length:size-length] = {all => (wchar)32};


  // scan now within all lines starting with this one

  for (ln=starting_ln; ln<=wnb_text_lines (text); ln++)
  {
    wretrieve_text_line (ref text, ln, out line2^, out length);
    line2^[length:size-length] = {all => (wchar)32};

    // check if the prefix is the same
    for (j=0; j<hotkey_position; j++)
    {
      if (wtoupper (wnormalize_extended_character (line1^[PREFIX_WLEN+j]))
              != wtoupper (wnormalize_extended_character (line2^[PREFIX_WLEN+j])))
      {
        free line1;
        free line2;
        return best;
      }
    }

    // check if the hotkey is acceptable
    if (wtoupper (wnormalize_extended_character (line2^[PREFIX_WLEN+hotkey_position])) == hotkey) // match
    {
      match = true;
      free line1;
      free line2;
      return ln;
    }

    if (wtoupper (wnormalize_extended_character (line2^[PREFIX_WLEN+hotkey_position])) > hotkey) // too far
    {
      free line1;
      free line2;
      return best;
    }

    best = ln;
  }

  free line1;
  free line2;

  return best;
}

//--------------------------------------------------------------------

void listbox_top (ref CONTROL_INFO o, ref LISTBOX_INFO listbox)
{
  if (listbox.ln > 1)
  {
    listbox.ln   = 1;
    listbox.page = 1;
    repaint_control (o);
  }
}

//--------------------------------------------------------------------

void listbox_bottom (ref CONTROL_INFO o, ref LISTBOX_INFO listbox)
{
  if (listbox.ln != wnb_text_lines (listbox.wtext))
  {
    int new_page;

    listbox.ln = wnb_text_lines (listbox.wtext);
    if (listbox.ln >= listbox.page + listbox.nb_screen_lines)
    {
      new_page = listbox.ln - (listbox.nb_screen_lines-1);
      if (new_page < 1)
        new_page = 1;

      listbox.page = new_page;
    }

    repaint_control (o);
  }
}

//--------------------------------------------------------------------

void process_key_listbox (ref CONTROL_INFO o, ref LISTBOX_INFO listbox, int key,
                          out bool key_was_treated)
{
  int  ln, old_hotkey_position;
  bool match;

  assert o.typ == TYP_LISTBOX || o.typ == TYP_TREE || o.typ == TYP_COMBO;

  key_was_treated = true;

  // pressing + or - on a tree folder sends event EVENT_TREE_COLLAPSED or EVENT_TREE_EXPANDED.
  if (o.typ == TYP_TREE && (key == (int)'+' || key == (int)'-') && listbox.ln <= wnb_text_lines (listbox.wtext))
  {
    PREFIX prefix;

    {
      wstring^ line = new wchar [PREFIX_WLEN];
      int      length;
      wretrieve_text_line (ref listbox.wtext, listbox.ln, out line^, out length);
      _unused length;
      prefix = *(PREFIX*)&line^;
      free line;
    }

    if (prefix.is_folder)
    {
      EVENT e;
      clear e;
      e.line_nr = listbox.ln;
      send_event (tree_line_is_expanded (ref listbox.wtext, listbox.ln, prefix.level) ? EVENT_TREE_COLLAPSED : EVENT_TREE_EXPANDED, e, o);
      return;
    }
  }

  if (key >= 32 && key <= 65535)
  {
    listbox.show_focus = true;

    if (listbox.allow_hotkey_search)
    {
      if (listbox.hotkey_position > listbox.line_length)  // past last column
        listbox.hotkey_position = 0;

      if (listbox.hotkey_position == 0)     // search from start
        ln = 1;
      else                          // search from current line
        ln = listbox.ln;

      ln = search_listbox_line (ref listbox.wtext,
                                    ln,
                                    listbox.line_length,
                                    listbox.hotkey_position,
                                    wtoupper (wnormalize_extended_character ((wchar)key)),
                                out match);

      if (ln > 0)     // some appropriate line was found
      {
        if (match)
          listbox.hotkey_position++;
        else
          listbox.hotkey_position = 0;

        if (ln != listbox.ln)
        {
          listbox.ln = ln;
          if (ln < listbox.page || ln >= listbox.page + listbox.nb_screen_lines)
          {
            listbox.page = ln;
            if (listbox.page + listbox.nb_screen_lines - 1 > wnb_text_lines (listbox.wtext))
            {
              listbox.page = wnb_text_lines (listbox.wtext) - (listbox.nb_screen_lines - 1);
              if (listbox.page < 1)
                listbox.page = 1;
            }
          }

          repaint_control (o);
        }
      }
      else
      {
        listbox.hotkey_position = 0;
      }
    }
  }
  else
  {
    old_hotkey_position = listbox.hotkey_position;
    listbox.hotkey_position = 0;

    switch (key)
    {
#if WINDOWS
      case KEY_CMD + VK_HOME:
      case KEY_CMD + KEY_CONTROL + VK_HOME:
      case KEY_CMD + KEY_CONTROL + VK_PRIOR:  // CTRL + PAGE UP
        listbox_top (ref o, ref listbox);
        if (o.typ == TYP_COMBO)
          listbox.selected_line = listbox.ln;
        else
          listbox.show_focus = true;
        break;

      case KEY_CMD + VK_END:
      case KEY_CMD + KEY_CONTROL + VK_END:
      case KEY_CMD + KEY_CONTROL + VK_NEXT:  // CTRL + PAGE_DOWN
        listbox_bottom (ref o, ref listbox);
        if (o.typ == TYP_COMBO)
          listbox.selected_line = listbox.ln;
        else
          listbox.show_focus = true;
        break;

      case KEY_CMD + VK_UP:
        listbox_up (ref o, ref listbox, 1);
        if (o.typ == TYP_COMBO)
          listbox.selected_line = listbox.ln;
        else
          listbox.show_focus = true;
        break;

      case KEY_CMD + VK_DOWN:
        listbox_down (ref o, ref listbox, 1);
        if (o.typ == TYP_COMBO)
          listbox.selected_line = listbox.ln;
        else
          listbox.show_focus = true;
        break;

      case KEY_CMD + VK_PRIOR:  // PAGE_UP
        listbox_page_up (ref o, ref listbox);
        if (o.typ == TYP_COMBO)
          listbox.selected_line = listbox.ln;
        else
          listbox.show_focus = true;
        break;

      case KEY_CMD + VK_NEXT:  // PAGE_DOWN
        listbox_page_down (ref o, ref listbox);
        if (o.typ == TYP_COMBO)
          listbox.selected_line = listbox.ln;
        else
          listbox.show_focus = true;
        break;

      case KEY_CMD + VK_RETURN:   // select / deselect listbox line
        if (o.typ == TYP_COMBO)
        {
          select_combo_listbox_line (ref o, ref o.combo.edit, ref listbox);
        }
        else
        {
          listbox.show_focus = true;
          select_deselect_listbox_line (ref o, ref listbox, using_mouse => false, left_button => true, 0, 0);
        }
        break;

      case KEY_CMD + VK_INSERT:
        if (o.typ != TYP_COMBO)
          listbox.show_focus = true;
        select_deselect_all_lb_lines (ref o, ref listbox, true);
        break;

      case KEY_CMD + VK_DELETE:
        if (o.typ == TYP_TREE)
        {
          if (listbox.ln > 0)
          {
            EVENT e;
            clear e;
            e.line_nr = listbox.ln;
            send_event (EVENT_TREE_DELETE, e, o);
          }
        }
        else
        {
          if (o.typ != TYP_COMBO)
            listbox.show_focus = true;
          select_deselect_all_lb_lines (ref o, ref listbox, false);
        }
        break;

      case KEY_CMD + KEY_CONTROL + 0x41:   // CTRL-A : select all
        if (listbox.multiple_mode)
        {
          lb_deselect_all_lines (ref listbox);
          if (listbox.ln != 0)
            lb_select_all_in_same_folder (ref listbox, listbox.ln);
          repaint_control (o);
          {
            EVENT e;
            clear e;
            send_event (EVENT_LISTBOX_LINE_SELECTED, e, o);
          }
        }
        break;
#endif

      default:
        key_was_treated = false;
        listbox.hotkey_position = old_hotkey_position;
        break;
    }
  }
}

//--------------------------------------------------------------------

void process_key_scroll (ref CONTROL_INFO o, ref SCROLL_INFO scroll, int key, out bool key_was_treated)
{
  key_was_treated = true;

  switch (key)
  {
    case KEY_CMD + VK_HOME:
    case KEY_CMD + KEY_CONTROL + VK_HOME:
    case KEY_CMD + KEY_CONTROL + VK_PRIOR:  // CTRL + PAGE UP
      scroll_top (ref o, ref scroll);
      break;

    case KEY_CMD + VK_END:
    case KEY_CMD + KEY_CONTROL + VK_END:
    case KEY_CMD + KEY_CONTROL + VK_NEXT:  // CTRL + PAGE_DOWN
      scroll_bottom (ref o, ref scroll);
      break;

    case KEY_CMD + VK_UP:
      scroll_up (ref o, ref scroll, scroll.small_y_increment);
      break;

    case KEY_CMD + VK_DOWN:
      scroll_down (ref o, ref scroll, scroll.small_y_increment);
      break;

    case KEY_CMD + VK_PRIOR:  // PAGE_UP
      scroll_page_up (ref o, ref scroll);
      break;

    case KEY_CMD + VK_NEXT:  // PAGE_DOWN
      scroll_page_down (ref o, ref scroll);
      break;

    default:
      key_was_treated = false;
      break;
  }
}

//--------------------------------------------------------------------

public void set_tab_on_next_or_previous_object (ref DIALOG_INFO d, bool next)
{
  CONTROL_INFO^ l, old;

  l = d.focus;
  if (l == null)
  {
    l = d.list;
    if (l == null)
      return;

    l = next ? l^.prev : l^.next;
  }

  old = l;
  for (;;)
  {
    l = next ? l^.next : l^.prev;

    if (tabable[(int)l^.typ] && !l^.hide)
    {
      d.focus = l;
      if (old != l)
      {
        repaint_control (old^);
        repaint_control (l^);
        if (old^.typ == TYP_COMBO)
          old^.combo.lb.listbox_shown = false;
      }
      break;
    }

    if (l == old)   // none found
      break;
  }
}

//--------------------------------------------------------------------

bool non_empty_text_marked (EDITBOX_INFO editbox)
{
  TEXT_MARK first, last;

  if (!editbox.text_marking_in_progress)
    return false;

  edit_text_get_first_mark (editbox.cur.text, out first);
  edit_text_get_last_mark  (editbox.cur.text, out last);

  return !is_empty_range (first, last);
}

//--------------------------------------------------------------------

void cut_marked_text (ref EDITBOX_INFO editbox, bool save_in_clipboard)
{
  if (!editbox.text_marking_in_progress)
    return;

  if (save_in_clipboard)
  {
    wstring^ p = edit_text_get_marked_text (ref editbox.cur.text);
    if (p^'length > 0)
      save_data_to_clipboard (p^, _CF_UNICODETEXT);
    free p;
  }

  if (!edit_text_get_modify_allowed (editbox.cur.text))
    return;

  edit_text_delete_block (ref editbox.cur.text);

  edit_text_set_marks (ref editbox.cur.text, {ln => 1, col => 0}, {ln => 1, col => 0});
  editbox.text_marking_in_progress = false;
}

//--------------------------------------------------------------------

void init_marks (int key, ref EDITBOX_INFO editbox)
{
  if ((key & KEY_SHIFT) > 0)   // shift pressed
  {
    if (!editbox.text_marking_in_progress)
    {
      int old_col = edit_text_get_col (editbox.cur.text);
      int old_ln  = edit_text_get_ln (editbox.cur.text);
      editbox_set_marking_start (ref editbox, old_col, old_ln);
    }
  }
  else   // shift no longer pressed
  {
    if (editbox.text_marking_in_progress)
    {
      edit_text_set_marks (ref editbox.cur.text, {ln => 1, col => 0}, {ln => 1, col => 0});
      editbox.text_marking_in_progress = false;
      edit_text_set_redraw_screen_needed (ref editbox.cur.text);  // remove marked text on all lines
    }
  }
}

//--------------------------------------------------------------------

void paste_from_clipboard (ref EDITBOX_INFO editbox)
{
  byte[]^   p;
  wstring^  fragment;

  if (!edit_text_get_modify_allowed (editbox.cur.text))
    return;

  p = load_data_from_clipboard (_CF_UNICODETEXT);
  if (p == null)
    return;

  fragment = new wchar [p^'length / 2];
  fragment^'byte = p^[0 : fragment^'length * 2];
  free p;

  edit_text_insert_text_block (ref editbox.cur.text, fragment^[0 : wstrlen(fragment^)]);
  edit_text_set_marks (ref editbox.cur.text, {ln => 1, col => 0}, {ln => 1, col => 0});
  editbox.text_marking_in_progress = false;
  edit_text_set_redraw_screen_needed (ref editbox.cur.text);  // remove marked text on all lines

  free fragment;
}

//--------------------------------------------------------------------

void copy_to_clipboard (ref EDITBOX_INFO editbox)
{
  wstring^ p;
  if (!editbox.text_marking_in_progress)
    return;
  p = edit_text_get_marked_text (ref editbox.cur.text);
  if (p^'length > 0)
    save_data_to_clipboard (p^, _CF_UNICODETEXT);
  free p;
}

//--------------------------------------------------------------------

int compare_marks (TEXT_MARK mark1, TEXT_MARK mark2)
{
  if (mark1.ln < mark2.ln)
    return -1;
  if (mark1.ln > mark2.ln)
    return +1;
  if (mark1.col < mark2.col)
    return -1;
  if (mark1.col > mark2.col)
    return +1;
  return 0;
}

//--------------------------------------------------------------------

// returns 0 if we're on marking start, -1 if we are before, +1 if we are after

int cmp_to_marking_start (EDITBOX_INFO editbox)
{
  ref EDIT_TEXT we = editbox.cur.text;
  return compare_marks (mark1 => {ln => edit_text_get_ln(we), col => edit_text_get_col(we)},
                        mark2 => editbox.text_marking_start);
}

//--------------------------------------------------------------------

void copy_single_editbox_info (ref SINGLE_EDITBOX_INFO source, out SINGLE_EDITBOX_INFO target)
{
  clear target;
  edit_unsafe_copy (ref source.text, out target.text);
  target.page   = source.page;
  target.scroll = source.scroll;
}

//--------------------------------------------------------------------

void clone_single_editbox_info (ref SINGLE_EDITBOX_INFO source, out SINGLE_EDITBOX_INFO target)
{
  clear target;
  edit_clone (ref source.text, out target.text);
  target.page   = source.page;
  target.scroll = source.scroll;
}

//--------------------------------------------------------------------

void save_editbox_undo_context (ref EDITBOX_INFO editbox)
{
  int i;

  if (editbox.undo_slots_filled == MAX_EDITBOX_UNDOS)
  {
    edit_text_dispose (ref editbox.undo[0].text);

    for (i=0; i<MAX_EDITBOX_UNDOS-1; i++)
      copy_single_editbox_info (ref source => editbox.undo[i+1], out target => editbox.undo[i]);

    editbox.next_undo_slot--;
    editbox.undo_slots_filled--;
  }

  for (i=editbox.next_undo_slot; i<editbox.undo_slots_filled; i++)
  {
    edit_text_dispose (ref editbox.undo[i].text);
    clear editbox.undo[i];
  }
  editbox.undo_slots_filled = editbox.next_undo_slot;

  clone_single_editbox_info (ref source => editbox.cur, out target => editbox.undo[editbox.next_undo_slot]);
  editbox.next_undo_slot++;
  editbox.undo_slots_filled++;

  if (editbox.next_undo_slot > 1)
  {
    if (edit_are_identical (ref editbox.undo[editbox.next_undo_slot-1].text, ref editbox.undo[editbox.next_undo_slot-2].text) &&
        editbox.undo[editbox.next_undo_slot-1].page == editbox.undo[editbox.next_undo_slot-2].page &&
        editbox.undo[editbox.next_undo_slot-1].scroll == editbox.undo[editbox.next_undo_slot-2].scroll)
    {
      // no need to save context, it's identical
      editbox.next_undo_slot--;
      editbox.undo_slots_filled--;
      edit_text_dispose (ref editbox.undo[editbox.next_undo_slot].text);
      clear editbox.undo[editbox.next_undo_slot];
    }
  }
}

//----------------------------------------------------------------------

public
void process_key_editbox (ref CONTROL_INFO o,
                          ref EDITBOX_INFO editbox,
                              int          key,
                          out bool         key_was_treated)
{
  ref EDIT_TEXT we = editbox.cur.text;

  int old_page   = editbox.cur.page;
  int old_scroll = editbox.cur.scroll;
  int old_col    = edit_text_get_col (we);
  int old_ln     = edit_text_get_ln (we);

  if (editbox.undo_slots_filled == 0)     // make sure at least 1 slot is filled initially
    save_editbox_undo_context (ref editbox);

  if (key >= 32 && key <= 65535)
  {
    cut_marked_text (ref editbox, save_in_clipboard => false);
    edit_text_achar (ref we, (wchar)key);
  }
  else  // a control key
  {
    switch (key & (~KEY_SHIFT))
    {
#if WINDOWS
      case KEY_CMD + VK_INSERT:
        if ((key & KEY_SHIFT) > 0)
          paste_from_clipboard (ref editbox);
        else
          edit_text_switch_insert (ref we);  // switch insert/delete mode
        break;

      case KEY_CMD + VK_HOME:
        init_marks (key, ref editbox);
        edit_text_home (ref we);
        break;

      case KEY_CMD + VK_END:
        init_marks (key, ref editbox);
        edit_text_end (ref we);
        break;

      case KEY_CMD + VK_LEFT:
        init_marks (key, ref editbox);
        edit_text_cursor_left (ref we);
        break;

      case KEY_CMD + VK_RIGHT:
        init_marks (key, ref editbox);
        edit_text_cursor_right (ref we);
        break;

      case KEY_CMD + VK_UP:
        init_marks (key, ref editbox);
        edit_text_cursor_up (ref we);
        break;

      case KEY_CMD + VK_DOWN:
        init_marks (key, ref editbox);
        edit_text_cursor_down (ref we);
        break;

      case KEY_CMD + KEY_CONTROL + VK_LEFT:
        init_marks (key, ref editbox);
        if (editbox.text_marking_in_progress)
        {
          int f = cmp_to_marking_start(editbox);
          edit_text_previous_word (ref we, begin_of_word => (f <= 0));
          if (f == +1 && cmp_to_marking_start(editbox) == -1)   // cursor was after, now it's before marking start
          {
            edit_text_set_col (ref we, old_col);
            edit_text_set_ln (ref we, old_ln);
            edit_text_previous_word (ref we, begin_of_word => true);
          }
        }
        else
        {
          edit_text_previous_word (ref we, begin_of_word => true);
        }
        break;

      case KEY_CMD + KEY_CONTROL + VK_RIGHT:
        init_marks (key, ref editbox);
        if (editbox.text_marking_in_progress)
        {
          int f = cmp_to_marking_start(editbox);
          edit_text_next_word (ref we, begin_of_word => (f < 0));
          if (f == -1 && cmp_to_marking_start(editbox) == +1)       // cursor was before, now it's after marking start
          {
            edit_text_set_col (ref we, old_col);
            edit_text_set_ln (ref we, old_ln);
            edit_text_next_word (ref we, begin_of_word => false);
          }
        }
        else
        {
          edit_text_next_word (ref we, begin_of_word => true);
        }
        break;

      case KEY_CMD + KEY_CONTROL + VK_HOME:
        init_marks (key, ref editbox);
        edit_text_top_of_text (ref we);
        break;

      case KEY_CMD + KEY_CONTROL + VK_END:
        init_marks (key, ref editbox);
        edit_text_bottom_of_text (ref we);
        break;

      case KEY_CMD + VK_PRIOR:  // PAGE UP
        {
          int nb_lines = max(1, o.y_size / o.font.height);
          init_marks (key, ref editbox);
          edit_text_page (ref we, -nb_lines);
          editbox.cur.page -= nb_lines;
        }
        break;

      case KEY_CMD + VK_NEXT:  // PAGE_DOWN
        {
          int nb_lines = max(1, o.y_size / o.font.height);
          init_marks (key, ref editbox);
          edit_text_page (ref we, nb_lines);
          editbox.cur.page += nb_lines;
        }
        break;


      // these keys work on marks

      case KEY_CMD + VK_TAB:
        if (editbox.text_marking_in_progress)
        {
          if ((key & KEY_SHIFT) == 0)   // shift not pressed
            edit_text_indent_block (ref we, +1);
          else
            edit_text_indent_block (ref we, -1);
        }
        else
        {
          if ((key & KEY_SHIFT) == 0)   // shift not pressed
            edit_text_tab (ref we);
          else
            edit_text_untab (ref we);
        }
        break;


      // these keys cause marks to be deleted

      case KEY_CMD + VK_DELETE:
        if (non_empty_text_marked (editbox))
          cut_marked_text (ref editbox, save_in_clipboard => (key & KEY_SHIFT) > 0);
        else
        {
          init_marks (key, ref editbox);
          edit_text_delete (ref we);
        }
        break;

      case 1:  // CTRL-A (select all)
        init_marks (key, ref editbox);
        edit_text_bottom_of_text (ref we);
        edit_text_set_marks_on_all_text (ref we);
        editbox.text_marking_in_progress = true;  // must be true, otherwise ctrl-c does not work
        editbox.text_marking_start = {ln => 1, col => 0};
        edit_text_set_redraw_screen_needed (ref we);
        break;

      case 3:  // CTRL-C (copy to clipboard)
        copy_to_clipboard (ref editbox);
        break;

      case 8:  // BACKSPACE:
        if (non_empty_text_marked (editbox))
          cut_marked_text (ref editbox, save_in_clipboard => false);
        else
        {
          init_marks (key, ref editbox);
          edit_text_backspace (ref we);
        }
        break;

      case KEY_CMD + VK_RETURN:  // ENTER
        cut_marked_text (ref editbox, save_in_clipboard => false);
        edit_text_enter (ref we);
        break;

      case (int)((uint)'T' - (uint)'A' + 1):         // CONTROL-T
        cut_marked_text (ref editbox, save_in_clipboard => false);
        edit_text_delete_word (ref we);
        break;

      case 22:  // CTRL-V : paste from clipboard
        cut_marked_text (ref editbox, save_in_clipboard => false);
        paste_from_clipboard (ref editbox);
        break;

      case 24:   // CTRL-X : cut
        cut_marked_text (ref editbox, save_in_clipboard => true);
        break;

      case 25:   // CTRL-Y : redo
        if (edit_text_get_modify_allowed (editbox.cur.text) && editbox.next_undo_slot < editbox.undo_slots_filled)
        {
          editbox.next_undo_slot++;
          clone_single_editbox_info (ref source => editbox.undo[editbox.next_undo_slot-1], out target => editbox.cur);
          edit_text_set_marks (ref editbox.cur.text, {ln => 1, col => 0}, {ln => 1, col => 0});
          editbox.text_marking_in_progress = false;
        }
        break;

      case 26:   // CTRL-Z : undo
        if (edit_text_get_modify_allowed (editbox.cur.text) && editbox.next_undo_slot > 1)
        {
          editbox.next_undo_slot--;
          clone_single_editbox_info (ref source => editbox.undo[editbox.next_undo_slot-1], out target => editbox.cur);
          edit_text_set_marks (ref editbox.cur.text, {ln => 1, col => 0}, {ln => 1, col => 0});
          editbox.text_marking_in_progress = false;
        }
        break;
#endif

      default:
        key_was_treated = false;
        return;
    }


    // update ending mark

    if (editbox.text_marking_in_progress)
    {
      int col = edit_text_get_col (we);
      int ln  = edit_text_get_ln (we);

      edit_text_set_marks (ref we,
                               mark1 => editbox.text_marking_start,
                               mark2 => {ln => ln, col => col});
    }
  }

  key_was_treated = true;


  compute_editbox_scroll_field (ref o);
  compute_editbox_page         (ref o);


  // check if additional conditions for a screen refresh are fulfilled

  if (old_page != editbox.cur.page || old_ln != edit_text_get_ln (we) || old_scroll != editbox.cur.scroll)
  {
    edit_text_set_redraw_screen_needed (ref we);
  }
  else if (old_col != edit_text_get_col (we))
  {
    edit_text_set_redraw_line_needed (ref we);
  }

  if (key != 25 && key != 26)     // don't save undo context for undo/redo
    save_editbox_undo_context (ref editbox);

  repaint_control (o);

  {
    EVENT event;
    clear event;
    event.key = key;
    send_event (EVENT_EDITBOX_CHANGED, event, o);
  }
}

/**********************************************************************/

void treat_generic_key (ref DIALOG_INFO d, int key)
{
  switch (key)
  {
    case KEY_CMD + VK_TAB:
    case KEY_CMD + VK_RIGHT:
    case KEY_CMD + VK_DOWN:
//    case VK_RETURN:  ignore this : is ambiguous for shift and non-shift
    case KEY_CMD + VK_RETURN:      // will be returned as simple VK_RETURN below
    case KEY_CMD + KEY_SHIFT + VK_RETURN:
    case KEY_CMD + KEY_CONTROL + VK_RETURN:
      {
        int k = key;
        if (k == KEY_CMD + VK_RETURN)
          k = VK_RETURN;   // simulate simple VK_RETURN
        set_tab_on_next_or_previous_object (ref d, next => true);
        send_event_new_focus (d, k);
      }
      break;

    case KEY_CMD + KEY_SHIFT + VK_TAB:
    case KEY_CMD + VK_LEFT:
    case KEY_CMD + VK_UP:
      set_tab_on_next_or_previous_object (ref d, next => false);
      send_event_new_focus (d, key);
      break;

    default:
      break;
  }
}

//--------------------------------------------------------------------

public void treat_keyboard_key (int key)
{
  if ((key & KEY_RELEASE) != 0)    // release key or keyboard focus was lost
  {
    if (g_dialog_ptr->focus != null)
    {
      ref CONTROL_INFO o = g_dialog_ptr->focus^;

      switch (o.typ)
      {
        case TYP_BUTTON:
          if (key == KEY_CMD+32+KEY_RELEASE || key == 0)   // space was released or keyboard focus was lost
          {
            o.button.pressed = false;
            repaint_control (o);

            {
              EVENT e;
              clear e;
              send_event (EVENT_BUTTON_PRESSED, e, o);
            }
          }
          break;

        default:
          break;
      }
    }
  }
  else   // key was pressed
  {
    if (g_dialog_ptr->focus == null)
    {
      treat_generic_key (ref *g_dialog_ptr, key);
    }
    else
    {
      ref CONTROL_INFO o = g_dialog_ptr->focus^;
      EVENT e;
      bool  key_processed;

      clear e;
      key_processed = false;

      switch (o.typ)
      {
        case TYP_EDIT:
          process_key_edit_or_combo (ref o, key, ref o.edit, o.x_size, out key_processed);
          break;

        case TYP_BUTTON:
          if (key == 32)   // space
          {
            key_processed = true;
            o.button.pressed = true;
            repaint_control (o);
          }
          break;

        case TYP_CHECKBOX:
          if (key == 32 && o.checkbox.modify_allowed)   // space
          {
            key_processed = true;
            o.checkbox.setting = !o.checkbox.setting;
            repaint_control (o);

            send_event (EVENT_CHECKBOX_CHANGED, e, o);
          }
          break;

        case TYP_RADIOBUTTON:
          if (key == 32)   // space
          {
            key_processed = true;
            trigger_radiobutton (g_dialog_ptr->focus);
          }
          break;

        case TYP_LISTBOX:
          process_key_listbox (ref o, ref o.listbox, key, out key_processed);
          break;

        case TYP_TREE:
          process_key_listbox (ref o, ref o.tree.listbox, key, out key_processed);
          break;

        case TYP_SCROLL:
          process_key_scroll (ref o, ref o.scroll, key, out key_processed);
          break;

        case TYP_COMBO:
          if (o.combo.lb.listbox_shown)
          {
            process_key_listbox (ref o, ref o.combo.listbox, key, out key_processed);
          }
          else
          {
            process_key_edit_or_combo (ref o, key, ref o.combo.edit, o.x_size -  o.combo.listbox.arrow_box_width,
                                       out key_processed);

            if (!key_processed && (key == KEY_CMD + VK_PRIOR || key == KEY_CMD + VK_NEXT))
            {
              key_processed = true;
              o.combo.lb.listbox_shown = true;
              repaint_control (o);
            }
          }
          break;

        case TYP_EDITBOX:
          process_key_editbox (ref o, ref o.editbox, key, out key_processed);
          break;

        default:
          break;
      }

      if (!key_processed)
      {
        treat_generic_key (ref *g_dialog_ptr, key);
      }
    }
  }
}

//--------------------------------------------------------------------

public
void deallocate_controls (CONTROL_INFO^ list)
{
  CONTROL_INFO^ p, old;

  if (list == null)
    return;

  p = list;

  for (;;)
  {
    {
      ref CONTROL_INFO o = p^;

      switch (o.typ)
      {
        case TYP_TEXT:
          break;

        case TYP_EDIT:
          edit_line_dispose (ref o.edit.cr.edit_line);
          {
            int i;
            for (i=0; i<o.edit.undo_slots_filled; i++)
              edit_line_dispose (ref o.edit.undo[i].edit_line);
          }
          break;

        case TYP_CHECKBOX:
          break;

        case TYP_RADIOBUTTON:
          break;

        case TYP_BUTTON:
          break;

        case TYP_WINDOW:
#if WINDOWS
          SelectObject (o.window.hdcMemory, o.window.old_hbitmap);
          assert DeleteObject (o.window.hbitmap) != 0;
          assert DeleteDC (o.window.hdcMemory) != 0;
#elif ANDROID
          free o.window.hdcMemory.image;
          o.window.hdcMemory.image = null;
#endif
          break;

        case TYP_LISTBOX:
          wclose_text (ref o.listbox.wtext);
          break;

        case TYP_SCROLL:
          break;

        case TYP_TREE:
          wclose_text (ref o.tree.listbox.wtext);
          break;

        case TYP_COMBO:
          edit_line_dispose (ref o.combo.edit.cr.edit_line);
          {
            int i;
            for (i=0; i<o.combo.edit.undo_slots_filled; i++)
              edit_line_dispose (ref o.combo.edit.undo[i].edit_line);
          }
          wclose_text (ref o.combo.listbox.wtext);
          break;

        case TYP_EDITBOX:
         edit_text_dispose (ref o.editbox.cur.text);
         {
           int i;
           for (i=0; i<o.editbox.undo_slots_filled; i++)
             edit_text_dispose (ref o.editbox.undo[i].text);
         }
         break;

        default:
          abort;
      }

      free o.text;
    }

    old = p;
    p = p^.next;
    free old;

    if (p == list)
      break;
  }
}

//--------------------------------------------------------------------

// x, y are relative to the dialog window

public
CONTROL_INFO^ touched_control (DIALOG_INFO* p, int x, int y)
{
  int ofs_x = p->border_size;
  int ofs_y = p->border_size + p->title_height;
  CONTROL_INFO^ l;

  l = p->list;
  if (l == null)
    return null;

  // first open combo box
  for (;;)
  {
    if (l^.hide)
      ;
    else
    {
      ref CONTROL_INFO o = l^;

      if (o.typ == TYP_COMBO && o.combo.lb.listbox_shown &&
          x >= ofs_x+o.x && x < ofs_x+o.x+o.x_size && y >= ofs_y+o.combo.lb.y && y < ofs_y+o.combo.lb.y+o.combo.lb.y_size)
        return l;
    }

    l = l^.next;
    if (l == p->list)
      break;
  }

  // then all other controls
  l = p->list;
  for (;;)
  {
    if (l^.hide)
      ;
    else
    {
      ref CONTROL_INFO o = l^;

      if (x >= ofs_x+o.x && x < ofs_x+o.x+o.x_size && y >= ofs_y+o.y && y < ofs_y+o.y+o.y_size)
        return l;
    }

    l = l^.next;
    if (l == p->list)
      break;
  }

  return null;
}

//--------------------------------------------------------------------
#end unsafe
//--------------------------------------------------------------------
