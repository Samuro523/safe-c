
// edtext.c

use arithm, strings;
use text, edline;

// =====================================================================

enum CACHE_STATE {
  _UNUSED,       // .line^ is not in use
  _LOADED,       // .line^ mirrors the text line 'line'
  _DIRTY,        // .line^ content was modified and must be rewritten
};

struct EDIT_TEXT
{
  wstring^      line;            // current line (no trailing spaces !)
  int           length;          // active length of line
  wstring^      temp_line;       // temp working line (same length as line)
  int           temp_length;     // working variable
  CACHE_STATE   cache_state;     // _UNUSED, _LOADED or _DIRTY

  int           ln;              // current line number (>=1)
  int           nb_active_lines; // >=1, limit for cursor going down
  int           max_lines;       // upper limit on nb lines
  text.W_TEXT   text;            // holds the text lines
  int           col;             // in range 0 ..  max_length

  bool          modify_allowed;       // true = user can change text
  bool          text_modified;        // true = text was changed

  bool          insert_mode;          // false = delete, true = insert
  bool          switch_mode_allowed;  // true = user can switch mode

  bool          set_marks_allowed;    // true = user can set marks
  bool          marks_modified;       // true = marks were modified
  TEXT_MARK     first;                // column included
  TEXT_MARK     last;                 // column excluded

  bool          redraw_line_needed;   // true = line must be redrawn
  bool          redraw_screen_needed; // true = screen must be redrawn (in case text or marks were changed)
}

// =====================================================================

public void edit_text_allocate (out EDIT_TEXT edit_text,
                                    int       max_line_length,
                                    int       max_text_lines,
                                    bool      insert_mode)
{
  clear edit_text;
  edit_text.line      = new wchar[max_line_length];
//edit_text.length    = 0;
  edit_text.temp_line = new wchar[max_line_length];
//edit_text.cache_state         = _UNUSED;
  edit_text.ln                  = 1;
  edit_text.nb_active_lines     = 1;
  edit_text.max_lines           = max_text_lines;

  text.wcreat_text (out edit_text.text);

//edit_text.col                 = 0;
  edit_text.modify_allowed      = true;
//edit_text.text_modified       = false;
  edit_text.insert_mode         = insert_mode;
  edit_text.switch_mode_allowed = true;
//edit_text.set_marks_allowed   = false;
//edit_text.marks_modified      = false;
  edit_text.first               = {ln => 1, col => 0};
  edit_text.last                = {ln => 1, col => 0};

  edit_text.redraw_line_needed   = true;
  edit_text.redraw_screen_needed = true;
}

// =====================================================================

public void edit_text_dispose (ref EDIT_TEXT edit_text)
{
  free edit_text.line;
  free edit_text.temp_line;
  wclose_text (ref edit_text.text);
  clear edit_text;
}

// =====================================================================

// load .text line ln into cache .line
// to be called before working on .line

void load_cache (ref EDIT_TEXT edit_text)
{
  assert edit_text.nb_active_lines == wnb_text_lines (edit_text.text)
      || edit_text.nb_active_lines == wnb_text_lines (edit_text.text) + 1;

  if (edit_text.cache_state != _UNUSED)   // .line^ already in use
    return;

  if (edit_text.ln <= wnb_text_lines (edit_text.text))   // within text
  {
    wretrieve_text_line (ref edit_text.text,
                             edit_text.ln,
                         out edit_text.line^,
                         out edit_text.length);

    edit_text.cache_state = _LOADED;
  }
  else if (edit_text.ln == wnb_text_lines (edit_text.text) + 1)   // line below text
  {
    edit_text.length = 0;              // an empty line
    edit_text.cache_state = _LOADED;
  }
  else
    abort;    // invalid ln
}

// =====================================================================

// save cache .line into .text line ln
// to be called before working on .text

void save_cache (ref EDIT_TEXT edit_text)
{
  assert edit_text.nb_active_lines == wnb_text_lines (edit_text.text)
      || edit_text.nb_active_lines == wnb_text_lines (edit_text.text) + 1;

  if (edit_text.cache_state != _DIRTY)   // .line^ need not be rewritten
    return;

  if (edit_text.ln <= wnb_text_lines (edit_text.text))   // within active text
  {
    wupdate_text_line (ref edit_text.text,
                           edit_text.ln,
                           edit_text.line^[0 : edit_text.length]);
  }
  else if (edit_text.ln == wnb_text_lines (edit_text.text) + 1)   // line below text
  {
    assert edit_text.nb_active_lines == wnb_text_lines (edit_text.text) + 1;

    winsert_text_line (ref edit_text.text,
                           edit_text.ln,
                           edit_text.line^[0 : edit_text.length]);
  }
  else
    abort;    // invalid ln

  edit_text.cache_state = _LOADED;    // cache is not dirty anymore
}

// =====================================================================

// make sure line ln exists in text

void force_create_current_line (ref EDIT_TEXT edit_text)
{
  assert edit_text.nb_active_lines == wnb_text_lines (edit_text.text)
      || edit_text.nb_active_lines == wnb_text_lines (edit_text.text) + 1;

  if (edit_text.ln == edit_text.nb_active_lines &&
      edit_text.nb_active_lines == wnb_text_lines (edit_text.text) + 1)
  {
    winsert_text_line (ref edit_text.text,
                           edit_text.ln,
                           L"");
  }
}

// =====================================================================

// initialize an EDIT_LINE before editing a line
// assertion: the cache must be _LOADED or _DIRTY

void prepare_edit_line (EDIT_TEXT t, out EDIT_LINE l)
{
  assert t.cache_state != _UNUSED;

  edit_line_allocate (out l,
    line                => t.line,                // preallocated current line
    length              => t.length,              // active length of line, has no trailing spaces
    col                 => t.col,                 // in range 0 ..  line^'length-1, or 0 .. line^.length if extra_col.
    extra_col           => true,                  // can cursor go past last col
    modify_allowed      => t.modify_allowed,      // false = line may not change
    line_modified       => false,                 // true = line was changed
    insert_mode         => t.insert_mode,         // false = delete, true = insert
    switch_mode_allowed => t.switch_mode_allowed, // false = no switch allowed
    set_marks_allowed   => t.set_marks_allowed,   // true = marks can be set
    marks_modified      => false,                 // true = marks were modified
    first               => {active => (t.first.ln == t.ln), col => t.first.col},    // .col included
    last                => {active => (t.last.ln == t.ln), col => t.last.col});     // .col excluded
}

// =====================================================================

// terminate a EDIT_LINE structure when finished editing a line

void terminate_edit_line (ref EDIT_TEXT t, EDIT_LINE l)
{
  if (edit_line_get_line_modified (l))
  {
    t.cache_state        = _DIRTY;
    t.length             = edit_line_get_length (l);
    t.text_modified      = true;
    t.redraw_line_needed = true;
  }

  t.col         = edit_line_get_col (l);
  t.insert_mode = edit_line_get_insert_mode (l);

  if (edit_line_get_marks_modified (l))
  {
    LINE_MARK first, last;

    edit_line_get_first_mark (l, out first);
    edit_line_get_last_mark  (l, out last);

    if (first.active)
    {
      t.first.ln  = t.ln;
      t.first.col = first.col;
    }

    if (last.active)
    {
      t.last.ln   = t.ln;
      t.last.col  = last.col;
    }

    t.redraw_screen_needed = true;
  }
}

// =====================================================================

public int edit_text_count_lines (EDIT_TEXT edit_text)
{
  return edit_text.nb_active_lines;
}

// =====================================================================

public void edit_text_get_line (ref EDIT_TEXT edit_text, int ln, out wstring line, out int actual_length)
{
  assert ln >= 1 && ln <= edit_text.nb_active_lines;
  assert line'length == edit_text.line^'length;

  line = {all => L' '};

  if (edit_text.ln == ln)
  {
    load_cache (ref edit_text);
    line[0 : edit_text.length] = edit_text.line^[0 : edit_text.length];
    actual_length = edit_text.length;
  }
  else if (ln <= wnb_text_lines (edit_text.text))   // within text
  {
    wretrieve_text_line (ref edit_text.text,
                             ln,
                         out line,
                         out actual_length);
  }
  else
  {
    actual_length = 0;
  }
}

// =====================================================================

public int edit_text_get_col (EDIT_TEXT edit_text)
{
  return edit_text.col;
}

// =====================================================================

public int edit_text_get_ln (EDIT_TEXT edit_text)
{
  return edit_text.ln;
}

// =====================================================================

public int edit_text_get_max_line_length (EDIT_TEXT edit_text)
{
  return edit_text.line^'length;
}

// =====================================================================

public bool edit_text_get_modify_allowed (EDIT_TEXT edit_text)
{
  return edit_text.modify_allowed;
}

// =====================================================================

public bool edit_text_get_text_modified (EDIT_TEXT edit_text)
{
  return edit_text.text_modified;
}

// =====================================================================

public bool edit_text_get_insert_mode (EDIT_TEXT edit_text)
{
  return edit_text.insert_mode;
}

// =====================================================================

public wstring^ edit_text_get_full_text (ref EDIT_TEXT edit_text)
{
  int       i, length, ofs;
  wstring^  p;

  save_cache (ref edit_text);

  length = 0;
  for (i=1; i<=edit_text.nb_active_lines; i++)
  {
    if (i <= wnb_text_lines (edit_text.text))
      wretrieve_text_line (ref edit_text.text, i, out edit_text.temp_line^, out edit_text.temp_length);
    else
      edit_text.temp_length = 0;
    length += (edit_text.temp_length + 2);
  }

  p = new wchar[length - 2];
  ofs = 0;

  for (i=1; i<=edit_text.nb_active_lines; i++)
  {
    if (i <= wnb_text_lines (edit_text.text))
      wretrieve_text_line (ref edit_text.text, i, out edit_text.temp_line^, out edit_text.temp_length);
    else
      edit_text.temp_length = 0;

    p^[ofs : edit_text.temp_length] = edit_text.temp_line^[0 : edit_text.temp_length];
    ofs += edit_text.temp_length;

    if (i < edit_text.nb_active_lines)  // not last line
    {
      p^[ofs : 2] = {(wchar)13, (wchar)10};
      ofs += 2;
    }
  }

  assert ofs == length - 2;

  return p;
}

// =====================================================================

public void edit_text_get_first_mark (EDIT_TEXT edit_text, out TEXT_MARK first)
{
  first = edit_text.first;
}

// =====================================================================

public void edit_text_get_last_mark  (EDIT_TEXT edit_text, out TEXT_MARK last)
{
  last = edit_text.last;
}

// =====================================================================

public bool edit_text_get_redraw_line_needed (EDIT_TEXT edit_text)
{
  return edit_text.redraw_line_needed;
}

// =====================================================================

public bool edit_text_get_redraw_screen_needed (EDIT_TEXT edit_text)
{
  return edit_text.redraw_screen_needed;
}

// =====================================================================

public void edit_text_set_col (ref EDIT_TEXT edit_text, int col)
{
  assert col >= 0 && col <= edit_text.line^'length;
  edit_text.col = col;
}

// =====================================================================

public void edit_text_set_ln (ref EDIT_TEXT edit_text, int ln)
{
  assert ln >= 1 && ln <= edit_text.nb_active_lines;

  save_cache (ref edit_text);
  edit_text.cache_state = _UNUSED;
  edit_text.ln = ln;
}

// =====================================================================

public void edit_text_set_insert_mode (ref EDIT_TEXT edit_text, bool insert_mode)
{
  edit_text.insert_mode = insert_mode;
}

// =====================================================================

public void edit_text_set_text_modified (ref EDIT_TEXT edit_text, bool text_modified)
{
  edit_text.text_modified = text_modified;
}

// =====================================================================

public void edit_text_set_modify_allowed (ref EDIT_TEXT edit_text, bool modify_allowed)
{
  edit_text.modify_allowed = modify_allowed;
}

// =====================================================================

public void edit_text_set_redraw_line_needed (ref EDIT_TEXT edit_text, bool redraw_line_needed = true)
{
  edit_text.redraw_line_needed = redraw_line_needed;
}

// =====================================================================

public void edit_text_set_redraw_screen_needed (ref EDIT_TEXT edit_text, bool redraw_screen_needed = true)
{
  edit_text.redraw_screen_needed = redraw_screen_needed;
}

// =====================================================================

void remove_trailing_spaces (wstring s, ref int length)
{
  while (length > 0 && s[length-1] == L' ')
    length--;
}

// =====================================================================

public void edit_text_set_full_text (ref EDIT_TEXT edit_text, wstring text)
{
  int  ofs, ln, len=-1;
  bool trailing_cr;

  edit_text_clear_text (ref edit_text);
  edit_text.nb_active_lines = 0;

  trailing_cr = false;
  ln = 1;
  for (ofs=0; ofs<text'length; )
  {
    int eol = ofs;
    while (eol < text'length && eol-ofs < edit_text.line^'length && text[eol] >= L' ')
      eol++;

    len = eol-ofs;
    remove_trailing_spaces (text[ofs:eol-ofs], ref len);
    edit_text.nb_active_lines++;
    winsert_text_line (ref edit_text.text, ln, text[ofs:len]);
    ln++;

    // skip any CR's
    while (eol < text'length && (text[eol] < L' ' && text[eol] != (wchar)10))
      eol++;

    // skip a single LF, if any
    trailing_cr = false;
    if (eol < text'length && text[eol] == (wchar)10)
    {
      trailing_cr = true;
      eol++;
    }
    ofs = eol;
  }

  if (edit_text.nb_active_lines == 0 || trailing_cr)    // empty text or trailing cr : add one empty line
    edit_text.nb_active_lines++;
}

// =====================================================================

public void edit_text_clear_text (ref EDIT_TEXT edit_text)
{
  edit_text.length              = 0;
  edit_text.cache_state         = _UNUSED;
  edit_text.ln                  = 1;
  edit_text.nb_active_lines     = 1;

  text.wclose_text (ref edit_text.text);
  text.wcreat_text (out edit_text.text);

  edit_text.col                 = 0;
  edit_text.text_modified       = false;
  edit_text.marks_modified      = false;
  edit_text.first               = {ln => 1, col => 0};
  edit_text.last                = {ln => 1, col => 0};

  edit_text.redraw_line_needed   = true;
  edit_text.redraw_screen_needed = true;
}

// =====================================================================

// marks in any order

public void edit_text_set_marks (ref EDIT_TEXT edit_text, TEXT_MARK mark1, TEXT_MARK mark2)
{
  if (is_empty_range (mark1, mark2))
  {
    edit_text.first = mark2;
    edit_text.last = mark1;
  }
  else
  {
    edit_text.first = mark1;
    edit_text.last = mark2;
  }

  edit_text.marks_modified = true;
}

// =====================================================================

public void edit_text_set_marks_on_all_text (ref EDIT_TEXT edit_text)
{
  edit_text.first = {ln => 1, col => 0};
  edit_text.last = {ln => edit_text.nb_active_lines, col => 0};
  edit_text.marks_modified = true;

  if (edit_text.ln == edit_text.nb_active_lines &&   // cursor is on last line
      edit_text.cache_state != _UNUSED)
  {
    edit_text.last.col = edit_text.length;
  }
}

// =====================================================================

public void edit_text_achar (ref EDIT_TEXT edit_text, wchar achar)
{
  EDIT_LINE edit_line;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  edit_line_achar (ref edit_line, achar);
  terminate_edit_line (ref edit_text, edit_line);
}

// =====================================================================

public void edit_text_switch_insert (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  edit_line_switch_insert (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);
}

// =====================================================================

public void edit_text_home (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  edit_line_home (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);
}

// =====================================================================

public void edit_text_end (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  edit_line_end (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);
}

// =====================================================================

public void edit_text_cursor_left (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  edit_line_cursor_left (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);
}

// =====================================================================

public void edit_text_cursor_right (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  edit_line_cursor_right (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);
}

// =====================================================================

public void edit_text_enter (ref EDIT_TEXT edit_text)
{
  int tab, new_col, ln, len;

  load_cache (ref edit_text);


  // compute 'tab' by searching for this or the earliest preceding
  // non-empty line, and getting the position of its first
  // non-blank character.

  if (edit_text.length > 0)    // current line is non-empty
  {
    tab = 0;
    while (tab < edit_text.length && edit_text.line^[tab] == L' ')
      tab++;
  }
  else   // we must search in the preceding text lines
  {
    tab = 0;
    for (ln=edit_text.ln-1;
         ln>=1 && ln>=edit_text.ln-200;    // go max 200 lines upwards
         ln--)
    {
      wretrieve_text_line (ref edit_text.text,
                               ln,
                           out edit_text.temp_line^,
                           out edit_text.temp_length);

      if (edit_text.temp_length > 0)     // is non-empty
      {
        while (tab < edit_text.temp_length && edit_text.temp_line^[tab] == L' ')
          tab++;
        break;
      }
    }
  }


  // however, 'tab' must not exceed max_length-1

  if (tab >= edit_text.line^'length)
    tab = edit_text.line^'length - 1;


  if (!edit_text.insert_mode)    // current mode is DELETE
  {
    // go to column 'tab'
    edit_text.col = tab;

    edit_text_cursor_down (ref edit_text);
  }
  else             // current mode is INSERT
  {
    if (!edit_text.modify_allowed)   // no modifications are allowed
      return;

    if (edit_text.nb_active_lines >= edit_text.max_lines)  // text is full
      return;


    // new column will be at 'tab'
    new_col = tab;


    // split current line at cursor column

    edit_text.temp_length = 0;

    if (edit_text.col < edit_text.length)     // text will be modified
    {
      // decrease 'tab' til current cursor column
      if (tab > edit_text.col)
        tab = edit_text.col;

      // first some blanks ... */
      edit_text.temp_line^[0 : tab] = {all => L' '};

      // ... then a piece of the current line
      len = edit_text.length - edit_text.col;
      edit_text.temp_line^[tab : len] = edit_text.line^[edit_text.col : len];

      edit_text.temp_length = tab + edit_text.length - edit_text.col;
      edit_text.length = edit_text.col;

      while (edit_text.length > 0 && edit_text.line^[edit_text.length-1] == L' ')
        edit_text.length--;       // remove trailing spaces
    }


    // we must insert the cache at the current line

    edit_text.nb_active_lines++;

    winsert_text_line (ref edit_text.text,
                           edit_text.ln,
                           edit_text.line^[0 : edit_text.length]);

    edit_text.text_modified        = true;
    edit_text.redraw_screen_needed = true;


    // go down 1 line, where temp line will become the current cache

    edit_text.ln++;
    edit_text.line^[0 : edit_text.temp_length] = edit_text.temp_line^[0 : edit_text.temp_length];
    edit_text.length = edit_text.temp_length;

    if (edit_text.ln > wnb_text_lines (edit_text.text) && // below text
        edit_text.length == 0)                            // empty line
      edit_text.cache_state = _LOADED;
    else
      edit_text.cache_state = _DIRTY;


    // go to new column

    edit_text.col = new_col;
  }
}

// =====================================================================

void global_delete (ref EDIT_TEXT edit_text)
{
  // being here means the current column was behind end-of-line and
  // thus the line below has to be joined with this line at cursor
  // position (only for mode insert)

  if (edit_text.ln == edit_text.nb_active_lines)   // no line below
    return;


  // load the current line in the cache

  load_cache (ref edit_text);


  // special case : current line is empty -> just delete it

  if (edit_text.length == 0)
  {
    // delete current text line (throwing away the cache content)

    edit_text.nb_active_lines--;
    wdelete_text_line (ref edit_text.text, edit_text.ln);

    edit_text.cache_state = _UNUSED;
    edit_text.text_modified = true;
    edit_text.redraw_screen_needed = true;
  }
  else    // current line is not empty
  {
    if (edit_text.ln + 1 <= wnb_text_lines (edit_text.text))
    {
      // let's now retrieve the line below in the temporary buffer 'temp_line'

      wretrieve_text_line (ref edit_text.text,
                               edit_text.ln + 1,
                           out edit_text.temp_line^,
                           out edit_text.temp_length);


      if (edit_text.temp_length > 0)      // line below is non-empty
      {
        // check total size of (cursor_pos + size of line below)
        // if too large, cancel the operation.

        if (edit_text.col + edit_text.temp_length > edit_text.line^'length)
          return;


        // update current line in cache
        edit_text.line^[edit_text.length : edit_text.col - edit_text.length] = {all => L' '};
        edit_text.line^[edit_text.col : edit_text.temp_length] = edit_text.temp_line^[0 : edit_text.temp_length];
        edit_text.length = edit_text.col + edit_text.temp_length;

        while (edit_text.length > 0 && edit_text.line^[edit_text.length-1] == L' ')
          edit_text.length--;           // remove trailing spaces

        edit_text.cache_state = _DIRTY;
      }


      // delete text line below current line

      wdelete_text_line (ref edit_text.text, edit_text.ln + 1);
    }

    edit_text.nb_active_lines--;

    edit_text.text_modified = true;
    edit_text.redraw_screen_needed = true;
  }
}

// =====================================================================

public void edit_text_delete (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;
  int        rc;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  rc = edit_line_delete (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);

  if (rc == 0)    // all was done on current line
    return;

  global_delete (ref edit_text);
}

// =====================================================================

public void edit_text_delete_word (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;
  int        rc;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  rc = edit_line_delete_word (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);

  if (rc == 0)    // all was done on current line
    return;

  global_delete (ref edit_text);
}

// =====================================================================

public void edit_text_cursor_up (ref EDIT_TEXT edit_text)
{
  if (edit_text.ln == 1)         // we're already at top of text
    return;

  save_cache (ref edit_text);

  edit_text.ln--;
  edit_text.cache_state = _UNUSED;
}

// =====================================================================

public void edit_text_cursor_down (ref EDIT_TEXT edit_text)
{
  assert edit_text.ln >= 1 && edit_text.ln <= edit_text.nb_active_lines;

  if (edit_text.ln < edit_text.nb_active_lines)
  {
    save_cache (ref edit_text);
    edit_text.ln++;
    edit_text.cache_state = _UNUSED;
  }
}

// =====================================================================

public void edit_text_backspace (ref EDIT_TEXT edit_text)
{
  EDIT_LINE edit_line;
  int        rc;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  rc = edit_line_backspace (ref edit_line);
  terminate_edit_line (ref edit_text, edit_line);

  if (rc == 0)   // everything done
    return;


  // check if there is a line above

  if (edit_text.ln == 1)         // no line above
    return;


  // retrieve line above in 'temp_line'

  wretrieve_text_line (ref edit_text.text,
                           edit_text.ln - 1,
                       out edit_text.temp_line^,
                       out edit_text.temp_length);


  // check total size of both lines. if too large, cancel the operation.

  if (edit_text.length + edit_text.temp_length > edit_text.line^'length)
    return;


  save_cache (ref edit_text);
  edit_text.cache_state = _UNUSED;


  // go up 1 line & set new column

  edit_text.ln--;
  edit_text.col = edit_text.temp_length;


  global_delete (ref edit_text);
}

// =====================================================================

public void edit_text_previous_word (ref EDIT_TEXT edit_text, bool begin_of_word)
{
  EDIT_LINE edit_line;
  int        rc;
  int        saved_ln;
  int        saved_col;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  rc = edit_line_previous_word (ref edit_line, begin_of_word);
  terminate_edit_line (ref edit_text, edit_line);

  if (rc == 0)   // everything done
    return;


  // save current line and col numbers

  saved_ln  = edit_text.ln;
  saved_col = edit_text.col;


  while (edit_text.ln > 1)          // while there is a line above
  {
    // save this line

    save_cache (ref edit_text);


    // go one line up

    edit_text.ln--;
    edit_text.cache_state = _UNUSED;


    // load the line in cache

    load_cache (ref edit_text);


    // go to previous word on this line

    prepare_edit_line (edit_text, out edit_line);
    rc = edit_line_previous_word (ref edit_line, begin_of_word, come_from_next_line => true);
    terminate_edit_line (ref edit_text, edit_line);

    if (rc == 0)   // everything done
      return;
  }


  // there is no previous line : rollback

  if (edit_text.ln != saved_ln)
  {
    // save this line

    save_cache (ref edit_text);


    // go to old line

    edit_text.ln = saved_ln;
    edit_text.cache_state = _UNUSED;
  }

  edit_text.col = saved_col;
}

// =====================================================================

public void edit_text_next_word (ref EDIT_TEXT edit_text, bool begin_of_word)
{
  EDIT_LINE edit_line;
  int       rc;
  int       saved_ln;
  int       saved_col;

  load_cache (ref edit_text);

  prepare_edit_line (edit_text, out edit_line);
  rc = edit_line_next_word (ref edit_line, begin_of_word);
  terminate_edit_line (ref edit_text, edit_line);

  if (rc == 0)   // everything done
    return;


  // save current line and column numbers

  saved_ln  = edit_text.ln;
  saved_col = edit_text.col;


  // while there is a line below

  while (edit_text.ln < edit_text.nb_active_lines)
  {
    // save this line

    save_cache (ref edit_text);


    // go one line down

    edit_text.ln++;
    edit_text.cache_state = _UNUSED;


    // load the line in cache

    load_cache (ref edit_text);


    // go to next word on this line

    prepare_edit_line (edit_text, out edit_line);
    rc = edit_line_next_word (ref edit_line, begin_of_word, come_from_previous_line => true);
    terminate_edit_line (ref edit_text, edit_line);

    if (rc == 0)   // everything done
      return;
  }


  // there is no next word : rollback

  if (edit_text.ln != saved_ln)
  {
    // save this line

    save_cache (ref edit_text);


    // go to old line

    edit_text.ln = saved_ln;
    edit_text.cache_state = _UNUSED;
  }

  edit_text.col = saved_col;
}

// =====================================================================

public void edit_text_top_of_text (ref EDIT_TEXT edit_text)
{
  edit_text.col = 0;

  if (edit_text.ln != 1)         // we're not already at top of text
  {
    save_cache (ref edit_text);
    edit_text.cache_state = _UNUSED;
    edit_text.ln  = 1;
  }
}

// =====================================================================

public void edit_text_bottom_of_text (ref EDIT_TEXT edit_text)
{
  edit_text.col = 0;

  if (edit_text.ln != edit_text.nb_active_lines)
  {
    save_cache (ref edit_text);   // this might add a line to the text
    edit_text.cache_state = _UNUSED;
    edit_text.ln = edit_text.nb_active_lines;
  }
}

// =====================================================================

public void edit_text_delete_line (ref EDIT_TEXT edit_text)
{
  if (!edit_text.modify_allowed)
    return;


  // go always to first column

  edit_text.col = 0;


  // the cache will not be used anymore

  if (edit_text.cache_state == _DIRTY)
  {
    edit_text.text_modified = true;
    edit_text.redraw_screen_needed = true;
  }
  edit_text.cache_state = _UNUSED;


  // if this line exists, delete it

  if (edit_text.ln <= wnb_text_lines (edit_text.text))
  {
    edit_text.nb_active_lines--;
    wdelete_text_line (ref edit_text.text, edit_text.ln);

    edit_text.text_modified = true;
    edit_text.redraw_screen_needed = true;
  }
}

// =====================================================================

public void edit_text_tab (ref EDIT_TEXT edit_text)
{
  int        tab, i;
  int        ln;
  EDIT_LINE edit_line;

  // compute 'tab' by searching for the earliest preceding
  // non-empty line, and getting the position of its first
  // tab position after the current column.

  tab = edit_text.col;

  for (ln=edit_text.ln-1;
       ln>=1 && ln>=edit_text.ln-200;    // go max 200 lines upwards
       ln--)
  {
    wretrieve_text_line (ref edit_text.text,
                             ln,
                         out edit_text.temp_line^,
                         out edit_text.temp_length);

    if (edit_text.temp_length > 0)     // line is non-empty
    {
      while (tab < edit_text.temp_length && edit_text.temp_line^[tab] != L' ')
        tab++;
      while (tab < edit_text.temp_length && edit_text.temp_line^[tab] == L' ')
        tab++;
      if (tab == edit_text.line^'length)
        tab--;
      break;
    }
  }


  if (!edit_text.insert_mode)     // current mode is DELETE
  {
    // go to column 'tab'
    edit_text.col = tab;
  }
  else                            // current mode is INSERT
  {
    if (!edit_text.modify_allowed)
      return;

    load_cache (ref edit_text);


    // check if we can insert "tab - col" spaces on this line

    if (edit_text.length + (tab - edit_text.col) > edit_text.line^'length)
      return;           // the line would become too long


    // insert the spaces

    prepare_edit_line (edit_text, out edit_line);
    for (i=edit_text.col; i<tab; i++)
      edit_line_achar (ref edit_line, L' ');
    terminate_edit_line (ref edit_text, edit_line);
  }
}

// =====================================================================

public void edit_text_untab (ref EDIT_TEXT edit_text)
{
  int        tab, i;
  int        ln;
  EDIT_LINE edit_line;

  // compute 'tab' by searching for the earliest preceding
  // non-empty line, and getting the position of its first
  // tab position before the current column.

  tab = edit_text.col;

  for (ln=edit_text.ln-1;
       ln>=1 && ln>=edit_text.ln-200;    // go max 200 lines upwards
       ln--)
  {
    wretrieve_text_line (ref edit_text.text,
                             ln,
                         out edit_text.temp_line^,
                         out edit_text.temp_length);

    if (edit_text.temp_length > 0)     // line is non-empty
    {
      if (edit_text.temp_length < tab)
        tab = edit_text.temp_length;
      else
      {
        while (tab > 0 && edit_text.temp_line^[tab-1] == L' ')
          tab--;
        while (tab > 0 && edit_text.temp_line^[tab-1] != L' ')
          tab--;
      }
      break;
    }
  }


  if (!edit_text.insert_mode)     // current mode is DELETE
  {
    // go to column 'tab'
    edit_text.col = tab;
  }
  else             // current mode is INSERT
  {
    if (!edit_text.modify_allowed)
      return;

    load_cache (ref edit_text);


    // test if the cursor column is preceded by "col - tab" spaces

    for (i=tab; i<edit_text.col; i++)
    {
      if (i < edit_text.length && edit_text.line^[i] != L' ')
        return;
    }


    // execute "col - tab" backspaces on this line

    prepare_edit_line (edit_text, out edit_line);
    for (i=tab; i<edit_text.col; i++)
      (void)edit_line_backspace (ref edit_line);
    terminate_edit_line (ref edit_text, edit_line);
  }
}

// =====================================================================

public bool is_empty_range (TEXT_MARK first, TEXT_MARK last)
{
  if (first.ln > last.ln)
    return true;

  if (first.ln < last.ln)
    return false;

  if (first.col >= last.col)
    return true;

  return false;
}

// =====================================================================

public int edit_text_indent_block (ref EDIT_TEXT edit_text, int indent)
{
  int        old_col;
  int        ln, old_ln;
  EDIT_LINE edit_line;

  if (!edit_text.modify_allowed)
    return 0;

  if (is_empty_range (edit_text.first, edit_text.last))
    return 0;

  save_cache (ref edit_text);


  // save old line and column

  old_ln  = edit_text.ln;
  old_col = edit_text.col;


  // indent line by line

  for (ln=edit_text.first.ln;
       (ln < edit_text.last.ln) ||
        (ln == edit_text.last.ln && edit_text.last.col > 0);
       ln++)
  {
    if (ln >= 1 && ln <= wnb_text_lines (edit_text.text))
    {
      edit_text.ln  = ln;
      edit_text.col = 0;
      edit_text.cache_state = _UNUSED;

      load_cache (ref edit_text);

      if (indent == +1)
      {
        prepare_edit_line (edit_text, out edit_line);
        edit_line_achar (ref edit_line, L' ');
        terminate_edit_line (ref edit_text, edit_line);
      }
      else if (indent == -1)
      {
        if (edit_text.length > 0 && edit_text.line^[0] == L' ')
        {
          prepare_edit_line (edit_text, out edit_line);
          edit_line_delete (ref edit_line);
          terminate_edit_line (ref edit_text, edit_line);
        }
      }

      edit_text.redraw_screen_needed = true;

      save_cache (ref edit_text);
    }
  }


  // restore old line and column

  edit_text.ln  = old_ln;
  edit_text.col = old_col;
  edit_text.cache_state = _UNUSED;


  return 0;
}

// =====================================================================

public void edit_text_page (ref EDIT_TEXT edit_text, int offset)
{
  assert edit_text.ln >= 1 && edit_text.ln <= edit_text.nb_active_lines;

  if (offset > 0)
  {
    if (edit_text.ln == edit_text.nb_active_lines)    // we're already at bottom of text
      return;

    save_cache (ref edit_text);

    edit_text.ln += offset;
    if (edit_text.ln > edit_text.nb_active_lines)
      edit_text.ln = edit_text.nb_active_lines;

    edit_text.cache_state = _UNUSED;
  }
  else if (offset < 0)
  {
    if (edit_text.ln == 1)         // we're already at top of text
      return;

    save_cache (ref edit_text);

    edit_text.ln += offset;
    if (edit_text.ln < 1)
      edit_text.ln = 1;

    edit_text.cache_state = _UNUSED;
  }
}

// =====================================================================

// copy marked block of 'edit_text' into 'text'.
// 'text' must be created before the call and closed after it.
// The function returns 0, or +1 if no text block was marked.

public int edit_text_copy_block_to_text (ref EDIT_TEXT edit_text, ref W_TEXT text)
{
  int i, ln;

  save_cache (ref edit_text);

  assert (edit_text.first.col >= 0 &&
          edit_text.first.col <= edit_text.line^'length);
  assert (edit_text.last.col >= 0 &&
          edit_text.last.col <= edit_text.line^'length);
  assert (edit_text.first.ln >= 1 && edit_text.first.ln <= edit_text.nb_active_lines);
  assert (edit_text.last.ln  >= 1 && edit_text.last.ln <= edit_text.nb_active_lines);

  if (is_empty_range (edit_text.first, edit_text.last))
    return +1;

  for (ln=edit_text.first.ln; ln<=edit_text.last.ln; ln++)
  {
    if (ln <= wnb_text_lines (edit_text.text))
    {
      wretrieve_text_line (ref edit_text.text,
                               ln,
                           out edit_text.temp_line^,
                           out edit_text.temp_length);
    }
    else
    {
      edit_text.temp_length = 0;
    }

    // cut or fill tail of marked lines
    if (ln == edit_text.last.ln)
    {
      if (edit_text.temp_length < edit_text.last.col)
      {
        edit_text.temp_line^
           [edit_text.temp_length : edit_text.last.col - edit_text.temp_length]
               = {all => L' '};
      }
      edit_text.temp_length = edit_text.last.col;
    }

    // cut head of marked lines
    if (ln == edit_text.first.ln)
    {
      if (edit_text.first.col > 0)     // a very frequent simple case
      {
        for (i=edit_text.first.col; i<edit_text.temp_length; i++)
        {
          edit_text.temp_line^[i - edit_text.first.col] =
            edit_text.temp_line^[i];
        }

        edit_text.temp_length -= edit_text.first.col;
        if (edit_text.temp_length < 0)
          edit_text.temp_length = 0;
      }
    }

    // append the line to 'text'
    winsert_text_line (ref text,
                           wnb_text_lines (text) + 1,
                           edit_text.temp_line^[0:edit_text.temp_length]);
  }

  return 0;
}

// =====================================================================

public wstring^ edit_text_get_marked_text (ref EDIT_TEXT edit_text)
{
  int      i, ln, pass, length;
  wstring^ p;
  bool     is_last_line;

  save_cache (ref edit_text);

  assert (edit_text.first.col >= 0 && edit_text.first.col <= edit_text.line^'length);
  assert (edit_text.last.col  >= 0 && edit_text.last.col  <= edit_text.line^'length);
  assert (edit_text.first.ln  >= 1 && edit_text.first.ln  <= edit_text.nb_active_lines);
  assert (edit_text.last.ln   >= 1 && edit_text.last.ln   <= edit_text.nb_active_lines);

  if (is_empty_range (edit_text.first, edit_text.last))
    return new wstring(0);

  p = null;
  length = 0;

  for (pass=1; pass<=2; pass++)
  {
    if (pass == 2)
      p = new wstring(length);

    length = 0;

    for (ln=edit_text.first.ln; ln<=edit_text.last.ln; ln++)
    {
      is_last_line = (ln == edit_text.last.ln);

      if (ln <= wnb_text_lines (edit_text.text))
      {
        wretrieve_text_line (ref edit_text.text,
                                 ln,
                             out edit_text.temp_line^,
                             out edit_text.temp_length);
      }
      else
      {
        edit_text.temp_length = 0;
      }

      // cut or fill tail of marked lines
      if (ln == edit_text.last.ln)
      {
        if (edit_text.temp_length < edit_text.last.col)
        {
          edit_text.temp_line^
             [edit_text.temp_length : edit_text.last.col - edit_text.temp_length]
                 = {all => L' '};
        }
        edit_text.temp_length = edit_text.last.col;
      }

      // cut head of marked lines
      if (ln == edit_text.first.ln)
      {
        if (edit_text.first.col > 0)     // a very frequent simple case
        {
          for (i=edit_text.first.col; i<edit_text.temp_length; i++)
          {
            edit_text.temp_line^[i - edit_text.first.col] =
              edit_text.temp_line^[i];
          }

          edit_text.temp_length -= edit_text.first.col;
          if (edit_text.temp_length < 0)
            edit_text.temp_length = 0;
        }
      }

      if (pass == 1)
      {
        length += edit_text.temp_length;
        if (!is_last_line)
          length += 2;
      }
      else
      {
        p^[length : edit_text.temp_length] = edit_text.temp_line^[0:edit_text.temp_length];
        length += edit_text.temp_length;

        if (!is_last_line)
        {
          p^[length : 2] = {(wchar)13, (wchar)10};
          length += 2;
        }
      }
    }
  }

  return p;
}

// =====================================================================

// insert 'text' in 'edit_text' at current cursor position.
// 'text' must be created before the call and closed after it.
// The last crlf of 'text' is not copied.
// The marks of 'edit_text' are set around the inserted text
// and the cursor position is set to the end of the inserted text.
// The function returns :
//    0 if OK,
//   +1 if the text is read-only and thus no insertion was done.
//   -1 if text too large,

public int edit_text_insert_block (ref EDIT_TEXT edit_text, ref W_TEXT text)
{
  int count, tail;

  if (!edit_text.modify_allowed)
    return +1;

  count = wnb_text_lines (text);
  if (count == 0)               // text is empty : nothing to do
    return 0;

  save_cache (ref edit_text);
  load_cache (ref edit_text);

  if (count == 1)
  {
    if (edit_text.col > edit_text.length)    // cursor is after line length
    {
      int len = edit_text.col - edit_text.length;
      edit_text.line^[edit_text.length : len] = {all => L' '};  // append spaces
      edit_text.length = edit_text.col;
    }

    tail = edit_text.length - edit_text.col;   // length of part after cursor

    wretrieve_text_line (ref text, 1, out edit_text.temp_line^, out edit_text.temp_length);  // fragment to insert

    if (edit_text.length + edit_text.temp_length <= edit_text.line^'length)     // all 3 parts hold on one line
    {
      edit_text.line^[edit_text.col + edit_text.temp_length : tail] = edit_text.line^[edit_text.col : tail];
      edit_text.line^[edit_text.col : edit_text.temp_length] = edit_text.temp_line^[0 : edit_text.temp_length];
      edit_text.length += edit_text.temp_length;
      remove_trailing_spaces (edit_text.line^, ref edit_text.length);
      edit_text.cache_state = _DIRTY;

      // set marks
      edit_text.first = {ln => edit_text.ln, col => edit_text.col};
      edit_text.last = {ln => edit_text.ln, col => edit_text.col + edit_text.temp_length};
    }
    else if (edit_text.col + edit_text.temp_length <= edit_text.line^'length)   // prefix + fragment on current line, tail on new line below.
    {
      if (edit_text.nb_active_lines + 1 > edit_text.max_lines)
        return -1;   // too large

      force_create_current_line (ref edit_text);

      // insert trailing part on a new line below
      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.line^[edit_text.col : tail], ref tail);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 1,
                             edit_text.line^[edit_text.col : tail]);

      edit_text.line^[edit_text.col : edit_text.temp_length] = edit_text.temp_line^[0 : edit_text.temp_length];
      edit_text.length = edit_text.col + edit_text.temp_length;
      remove_trailing_spaces (edit_text.line^, ref edit_text.length);
      edit_text.cache_state = _DIRTY;

      // set marks
      edit_text.first = {ln => edit_text.ln, col => edit_text.col};
      edit_text.last = {ln => edit_text.ln, col => edit_text.col + edit_text.temp_length};
    }
    else      // split in 3 parts, add 2 new lines
    {
      if (edit_text.nb_active_lines + 2 > edit_text.max_lines)
        return -1;   // too large

      // shorten current line
      edit_text.length = edit_text.col;
      remove_trailing_spaces (edit_text.line^, ref edit_text.length);
      edit_text.cache_state = _DIRTY;

      force_create_current_line (ref edit_text);

      // insert trailing part on a new line below
      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.temp_line^, ref edit_text.temp_length);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 1,
                             edit_text.temp_line^[0 : edit_text.temp_length]);


      // insert trailing part on a new line below
      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.line^[edit_text.col : tail], ref tail);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 2,
                             edit_text.line^[edit_text.col : tail]);

      // set mark
      edit_text.first = {ln => edit_text.ln+1, col => 0};
      edit_text.last = {ln => edit_text.ln+1, col => edit_text.temp_length};
    }
  }
  else  // count >= 2
  {
    bool merge_first, merge_last;
    int  nb_lines_to_insert, i;

    // make sure last line exists
    if (edit_text.ln == wnb_text_lines (edit_text.text) + 1)
    {
      edit_text.cache_state = _DIRTY;      // force creating last line
      save_cache (ref edit_text);
    }

    if (edit_text.col > edit_text.length)    // cursor is after line length
    {
      int len = edit_text.col - edit_text.length;
      edit_text.line^[edit_text.length : len] = {all => L' '};  // append spaces
      edit_text.length = edit_text.col;
    }

    tail = edit_text.length - edit_text.col;

    wretrieve_text_line (ref text, 1, out edit_text.temp_line^, out edit_text.temp_length);
    merge_first = (edit_text.col + edit_text.temp_length <= edit_text.line^'length);

    wretrieve_text_line (ref text, count, out edit_text.temp_line^, out edit_text.temp_length);
    merge_last = (edit_text.temp_length + tail <= edit_text.line^'length);


    // check if we can insert the block

    nb_lines_to_insert = count + 1;

    if (merge_first)
      nb_lines_to_insert--;

    if (merge_last)
      nb_lines_to_insert--;

    if (edit_text.nb_active_lines + nb_lines_to_insert > edit_text.max_lines)
      return -1;   // too large



    // treat last line : insert 1 or 2 lines

    force_create_current_line (ref edit_text);

    wretrieve_text_line (ref text, count, out edit_text.temp_line^, out edit_text.temp_length);
    if (merge_last)
    {
      // append last line + tail on a new line below
      edit_text.temp_line^[edit_text.temp_length : tail] = edit_text.line^[edit_text.col : tail];

      // set mark
      edit_text.last = {ln => edit_text.ln + 1, col => edit_text.temp_length};

      edit_text.temp_length += tail;

      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.temp_line^, ref edit_text.temp_length);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 1,
                             edit_text.temp_line^[0 : edit_text.temp_length]);
    }
    else
    {
      // insert 2 lines

      // insert tail on a new line below
      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.line^[edit_text.col : tail], ref tail);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 1,
                             edit_text.line^[edit_text.col : tail]);

      // insert last line
      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.temp_line^, ref edit_text.temp_length);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 1,
                             edit_text.temp_line^[0 : edit_text.temp_length]);

      // set mark
      edit_text.last = {ln => edit_text.ln+1, col => edit_text.temp_length};
    }


    // insert middle lines

    for (i=count-1; i>1; i--)
    {
      wretrieve_text_line (ref text, i, out edit_text.temp_line^, out edit_text.temp_length);

      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.temp_line^, ref edit_text.temp_length);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 1,
                             edit_text.temp_line^[0 : edit_text.temp_length]);
    }

    edit_text.last.ln += (count - 2);


    // treat first line : insert or update line

    wretrieve_text_line (ref text, 1, out edit_text.temp_line^, out edit_text.temp_length);
    if (merge_first)   // append new line 1 at the end of current line
    {
      // update line
      edit_text.line^[edit_text.col : edit_text.temp_length] = edit_text.temp_line^[0 : edit_text.temp_length];
      edit_text.length = edit_text.col + edit_text.temp_length;
      remove_trailing_spaces (edit_text.line^, ref edit_text.length);
      edit_text.cache_state = _DIRTY;

      // set mark
      edit_text.first = {ln => edit_text.ln, col => edit_text.col};
    }
    else
    {
      // update shortened current line
      edit_text.length = edit_text.col;
      remove_trailing_spaces (edit_text.line^, ref edit_text.length);
      edit_text.cache_state = _DIRTY;

      // insert line 1 of text below
      edit_text.nb_active_lines++;
      remove_trailing_spaces (edit_text.temp_line^, ref edit_text.temp_length);
      winsert_text_line (ref edit_text.text,
                             edit_text.ln + 1,
                             edit_text.temp_line^[0 : edit_text.temp_length]);

      // set mark
      edit_text.first = {ln => edit_text.ln+1, col => 0};

      edit_text.last.ln++;
    }
  }

  save_cache (ref edit_text);

  edit_text.col = edit_text.last.col;
  edit_text.ln = edit_text.last.ln;
  edit_text.cache_state = _UNUSED;

  edit_text.text_modified = true;
  edit_text.marks_modified = true;
  edit_text.redraw_screen_needed = true;

  return 0;
}

// =====================================================================

public int edit_text_insert_text_block (ref EDIT_TEXT edit_text, wstring text)
{
  int    ofs, ln, len, rc;
  W_TEXT itext;
  bool   ends_with_cr = false;

  wcreat_text (out itext);

  ln = 1;
  for (ofs=0; ofs<text'length; )
  {
    int eol = ofs;
    while (eol < text'length && eol-ofs < edit_text.line^'length && text[eol] >= L' ')
      eol++;

    len = eol-ofs;
    remove_trailing_spaces (text[ofs:eol-ofs], ref len);
    winsert_text_line (ref itext, ln, text[ofs:len]);
    ln++;

    while (eol < text'length && (text[eol] < L' ' && text[eol] != (wchar)10))
      eol++;
    if (eol < text'length && text[eol] == (wchar)10)
    {
      eol++;
      ends_with_cr = true;
    }
    else
    {
      ends_with_cr = false;
    }
    ofs = eol;
  }

  if (ends_with_cr)
    winsert_text_line (ref itext, ln, L"");

  rc = edit_text_insert_block (ref edit_text, ref itext);

  wclose_text (ref itext);

  return rc;
}

// =====================================================================

// current col,line is always set to the start of the marked text.
// delete marked block of 'edit_text'
// returns 0 if OK, +1 if not done

public int edit_text_delete_block (ref EDIT_TEXT edit_text)
{
  assert (edit_text.first.col >= 0 && edit_text.first.col <= edit_text.line^'length);
  assert (edit_text.last.col  >= 0 && edit_text.last.col  <= edit_text.line^'length);
  assert (edit_text.first.ln  >= 1 && edit_text.first.ln  <= edit_text.nb_active_lines);
  assert (edit_text.last.ln   >= 1 && edit_text.last.ln   <= edit_text.nb_active_lines);

  save_cache (ref edit_text);
  edit_text.cache_state = _UNUSED;

  edit_text.col = edit_text.first.col;
  edit_text.ln = edit_text.first.ln;

  if (!edit_text.modify_allowed)
    return +1;

  if (is_empty_range (edit_text.first, edit_text.last))
    return 0;

  if (edit_text.first.ln == edit_text.last.ln)    // special case : delete single line block
  {
    if (edit_text.first.ln <= wnb_text_lines (edit_text.text))      // line exists and is not empty
    {
      int len, i;

      wretrieve_text_line (ref edit_text.text, edit_text.first.ln, out edit_text.temp_line^, out edit_text.temp_length);

      // compute future length 'len' of line
      len = min (edit_text.temp_length, edit_text.first.col);

      for (i=edit_text.last.col; i<edit_text.temp_length; i++)
        edit_text.temp_line^[len++] = edit_text.temp_line^[i];

      edit_text.temp_length = len;

      // update current line
      remove_trailing_spaces (edit_text.temp_line^, ref edit_text.temp_length);
      wupdate_text_line (ref edit_text.text, edit_text.first.ln, edit_text.temp_line^[0 : edit_text.temp_length]);
    }
    else
    {
      // single line is below text and empty : text must not be updated
    }
  }
  else    // multiple line delete block
  {
    while (edit_text.last.ln-1 > edit_text.first.ln)    // delete all full lines
    {
      edit_text.nb_active_lines--;
      wdelete_text_line (ref edit_text.text, edit_text.last.ln-1);
      edit_text.last.ln--;
    }


    assert edit_text.first.ln + 1 == edit_text.last.ln;


    // cut first line of multi-line block
    wretrieve_text_line (ref edit_text.text, edit_text.first.ln, out edit_text.line^, out edit_text.length);

    edit_text.length = min (edit_text.first.col, edit_text.length);    // cut tail of line

    remove_trailing_spaces (edit_text.line^, ref edit_text.length);
    wupdate_text_line (ref edit_text.text, edit_text.first.ln, edit_text.line^[0 : edit_text.length]);


    // cut last line of multi-line block, if the line exists

    if (edit_text.last.ln <= wnb_text_lines (edit_text.text))
    {
      int len, i;

      wretrieve_text_line (ref edit_text.text, edit_text.last.ln, out edit_text.temp_line^, out edit_text.temp_length);

      // compute future length 'len' of line
      len = 0;
      for (i=edit_text.last.col; i<edit_text.temp_length; i++)
        edit_text.temp_line^[len++] = edit_text.temp_line^[i];
      edit_text.temp_length = len;

      remove_trailing_spaces (edit_text.temp_line^, ref edit_text.temp_length);
      wupdate_text_line (ref edit_text.text, edit_text.last.ln, edit_text.temp_line^[0 : edit_text.temp_length]);


      // check if we can join both lines

      if (edit_text.first.col + len <= edit_text.line^'length)   // we can join both lines
      {
        edit_text.line^[edit_text.first.col : len] = edit_text.temp_line^[0 : len];
        edit_text.length = edit_text.first.col + len;

        remove_trailing_spaces (edit_text.line^, ref edit_text.length);
        wupdate_text_line (ref edit_text.text, edit_text.first.ln, edit_text.line^[0 : edit_text.length]);

        edit_text.nb_active_lines--;
        wdelete_text_line (ref edit_text.text, edit_text.last.ln);
      }
    }
  }


  // clear marks

  edit_text.last = edit_text.first;

  edit_text.text_modified        = true;
  edit_text.marks_modified       = true;
  edit_text.redraw_screen_needed = true;

  return 0;
}

// =====================================================================

bool isalnum (wchar c)
{
  return wisalpha(c) || wisdigit(c);
}

// =====================================================================

// find text fragment, start searching at current text position.
// returns (+1) if no further occurence of the fragment was found.
// returns -3 in case of illegal value for 'direction'.
// returns -6 if fragment_length is zero.

public
int edit_text_find (ref EDIT_TEXT edit_text,
                        wstring    fragment,
                        int        direction,    // -1 = up, +1 = down
                        bool       match_case,
                        bool       whole_word)
{
  int  col;
  int  count, ln;
  int  last, i;
  bool delimiter1, delimiter2;

  if (direction != +1 && direction != -1)
    return -3;

  if (fragment'length == 0)
    return -6;

  save_cache (ref edit_text);

  count = wnb_text_lines (edit_text.text);

  ln  = edit_text.ln;
  col = edit_text.col;

  for (;;)
  {
    wretrieve_text_line (ref edit_text.text, ln, out edit_text.temp_line^, out edit_text.temp_length);


    // compute first and last possible column for a match

    last = edit_text.temp_length - fragment'length;

    if (col > last && direction < 0)
      col = last;

    while (col >= 0 && col <= last)
    {
      // check if we have a match
      if (match_case)
      {
        if (edit_text.temp_line^[col] == fragment[0])
        {
          if (memcmp (edit_text.temp_line^[col:fragment'length], fragment) == 0)  // match !
          {
            clear delimiter1, delimiter2;

            if (whole_word)
            {
              delimiter1 = (col == 0) || (!isalnum(edit_text.temp_line^[col-1]));
              delimiter2 = (col+fragment'length >= edit_text.temp_length) || (!isalnum(edit_text.temp_line^[col+fragment'length]));
            }

            if ((!whole_word) || (delimiter1 && delimiter2))
            {
              // go to (col, ln)
              edit_text.col = col;
              edit_text.ln  = ln;
              edit_text.cache_state = _UNUSED;
              return 0;
            }
          }
        }
      }
      else
      {
        if (wtoupper(edit_text.temp_line^[col]) == wtoupper(fragment[0]))
        {
          for (i=0; i<fragment'length; i++)
            if (wtoupper(edit_text.temp_line^[col+i]) != wtoupper(fragment[i]))
              break;

          if (i == fragment'length)   // match !
          {
            clear delimiter1, delimiter2;

            if (whole_word)
            {
              delimiter1 = (col == 0) || (!isalnum(edit_text.temp_line^[col-1]));
              delimiter2 = (col+fragment'length >= edit_text.temp_length) || (!isalnum(edit_text.temp_line^[col+fragment'length]));
            }

            if ((!whole_word) || (delimiter1 && delimiter2))
            {
              // go to (col, ln)
              edit_text.col = col;
              edit_text.ln  = ln;
              edit_text.cache_state = _UNUSED;
              return 0;
            }

          }
        }
      }

      col += direction;
    }


    // continue with next line

    if (direction > 0)
      col = 0;
    else
      col = edit_text.line^'length;

    ln += direction;

    if (ln < 1 || ln > count)
      return +1;
  }
}

// =====================================================================

// replace text of length 'fragment_length' by 'replacer' of length
//   'replacer_length'.
// this function should be called after 'edit_text_find' found
//   a fragment to be replaced.
// returns (+2) if the text is read-only.
// returns (-8) if the fragment cannot be contained in the line.
// returns (-9) if the line becomes too long.

public
int edit_text_replace (ref EDIT_TEXT edit_text,
                           int        fragment_length,
                           wstring    replacer)
{
  int len;

  if (!edit_text.modify_allowed)
    return +2;

  load_cache (ref edit_text);

  // check if the line is long enough for the replacement

  if (edit_text.col + fragment_length > edit_text.length)
    return -8;

  if (edit_text.length - fragment_length + replacer'length > edit_text.line^'length)    // line becomes too long
    return -9;

  len = edit_text.length - (edit_text.col + fragment_length);

  edit_text.temp_line^[0 : len] = edit_text.line^[edit_text.col + fragment_length : len];

  edit_text.line^[edit_text.col : replacer'length] = replacer[0 : replacer'length];
  edit_text.line^[edit_text.col + replacer'length : len] = edit_text.temp_line^[0 : len];

  edit_text.length += replacer'length - fragment_length;

  edit_text.cache_state          = _DIRTY;
  edit_text.text_modified        = true;
  edit_text.redraw_line_needed   = true;
  edit_text.redraw_screen_needed = true;

  return 0;
}

// =====================================================================

public
bool edit_are_identical (ref EDIT_TEXT a, ref EDIT_TEXT b)
{
  return wstrcmp (a.line^, b.line^) == 0 &&
         a.length          == b.length &&
         a.cache_state     == b.cache_state &&
         a.nb_active_lines == b.nb_active_lines &&
         text . wtext_identical (a.text, b.text);
}

// =================================================================

void copy_or_clone (ref EDIT_TEXT source, out EDIT_TEXT target, bool clone)
{
  clear target;

  if (clone)  
  {
    target.line      = new wstring ' (source.line^);
    target.temp_line = new wstring ' (source.temp_line^);
    text . wtext_clone (source => source.text, out target => target.text);
  }
  else
  {
    target.line      = source.line;
    target.temp_line = source.temp_line;
    text . wtext_unsafe_copy (ref source => source.text, out target => target.text);
  }

  target.length      = source.length;
  target.temp_length = source.temp_length;
  target.cache_state = source.cache_state;
  
  target.ln              = source.ln;
  target.nb_active_lines = source.nb_active_lines;
  target.max_lines       = source.max_lines;
  target.col             = source.col;


  target.modify_allowed  = source.modify_allowed;
  target.text_modified   = source.text_modified;

  target.insert_mode          = source.insert_mode;
  target.switch_mode_allowed  = source.switch_mode_allowed;

  target.set_marks_allowed    = source.set_marks_allowed;
  target.marks_modified       = source.marks_modified;
  target.first                = source.first;
  target.last                 = source.last;

  target.redraw_line_needed   = source.redraw_line_needed;
  target.redraw_screen_needed = source.redraw_screen_needed;
}

// =================================================================

// copies source into target, then deletes source.

public
void edit_unsafe_copy (ref EDIT_TEXT source, out EDIT_TEXT target)
{
  copy_or_clone (ref source, out target, clone => false);
  clear source;
}

// =================================================================

public
void edit_clone (ref EDIT_TEXT source, out EDIT_TEXT target)
{
  copy_or_clone (ref source, out target, clone => true);
}

// =================================================================
