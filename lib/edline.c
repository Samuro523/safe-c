
// edline.c

use arithm, strings;

/************************************************************************/

struct EDIT_LINE
{
  wstring^      line;                // preallocated current line, max length
  int           length;              // active length of line, has no trailing spaces
  int           col;                 // in range 0 ..  line^'length-1, or 0 .. line^.length if extra_col.
  bool          extra_col;           // can cursor go past last col

  bool          modify_allowed;      // false = line may not change
  bool          line_modified;       // true = line was changed

  bool          insert_mode;         // false = delete, true = insert
  bool          switch_mode_allowed; // false = no switch allowed

  bool          set_marks_allowed;   // true = marks can be set
  bool          marks_modified;      // true = marks were modified
  LINE_MARK     first;               // .col included
  LINE_MARK     last;                // .col excluded
}

/************************************************************************/

public
void edit_line_allocate (out EDIT_LINE edit_line,
                             wstring^  line,                // preallocated current line
                             int       length,              // active length of line, has no trailing spaces
                             int       col,                 // in range 0 ..  line^'length-1, or 0 .. line^.length if extra_col.
                             bool      extra_col,           // can cursor go past last col
                             bool      modify_allowed,      // false = line may not change
                             bool      line_modified,       // true = line was changed
                             bool      insert_mode,         // false = delete, true = insert
                             bool      switch_mode_allowed, // false = no switch allowed
                             bool      set_marks_allowed,   // true = marks can be set
                             bool      marks_modified,      // true = marks were modified
                             LINE_MARK first,               // .col included
                             LINE_MARK last)                // .col excluded
{
  edit_line = {line                => line,
               length              => length,
               col                 => col,
               extra_col           => extra_col,
               modify_allowed      => modify_allowed,
               line_modified       => line_modified,
               insert_mode         => insert_mode,
               switch_mode_allowed => switch_mode_allowed,
               set_marks_allowed   => set_marks_allowed,
               marks_modified      => marks_modified,
               first               => first,
               last                => last};

  assert edit_line.col >= 0;
  if (edit_line.extra_col)
    assert edit_line.col <= edit_line.line^'length;
  else
    assert edit_line.col < edit_line.line^'length;
}

/************************************************************************/

public void edit_line_dispose (ref EDIT_LINE edit_line)
{
  free edit_line.line;
  clear edit_line;
}

/************************************************************************/

// assertion: line'length must have at least max_length.
// line is filled with filler char.

public void edit_line_get_line (EDIT_LINE edit_line, out wstring line, wchar filler = L' ')
{
  assert line'length >= edit_line.line^'length;
  line = {all => filler};
  line[0:edit_line.length] = edit_line.line^[0:edit_line.length];
}

/************************************************************************/

public int  edit_line_get_max_length       (EDIT_LINE edit_line)  { return edit_line.line^'length; }
public int  edit_line_get_length           (EDIT_LINE edit_line)  { return edit_line.length; }
public int  edit_line_get_col              (EDIT_LINE edit_line)  { return edit_line.col; }
public bool edit_line_get_extra_col        (EDIT_LINE edit_line)  { return edit_line.extra_col; }
public bool edit_line_get_modify_allowed   (EDIT_LINE edit_line)  { return edit_line.modify_allowed; }
public bool edit_line_get_line_modified    (EDIT_LINE edit_line)  { return edit_line.line_modified; }
public bool edit_line_get_insert_mode      (EDIT_LINE edit_line)  { return edit_line.insert_mode; }
public bool edit_line_get_marks_modified   (EDIT_LINE edit_line)  { return edit_line.marks_modified; }
public void edit_line_get_first_mark       (EDIT_LINE edit_line, out LINE_MARK first)  { first = edit_line.first; }
public void edit_line_get_last_mark        (EDIT_LINE edit_line, out LINE_MARK last)   { last = edit_line.last; }

/************************************************************************/

public void edit_line_set_line (ref EDIT_LINE edit_line, wstring line)
{
  edit_line.line^[0 : line'length] = line;
  edit_line.length = line'length;
  edit_line.line_modified = true;
}

/************************************************************************/

public void edit_line_set_col (ref EDIT_LINE edit_line, int col)
{
  assert col >= 0;
  if (edit_line.extra_col)
    assert col <= edit_line.line^'length;
  else
    assert col < edit_line.line^'length;

  edit_line.col = col;
}

/************************************************************************/

public void edit_line_set_modify_allowed (ref EDIT_LINE edit_line, bool modify_allowed)
{
  edit_line.modify_allowed = modify_allowed;
}

/************************************************************************/

public void edit_line_set_line_modified (ref EDIT_LINE edit_line, bool line_modified)
{
  edit_line.line_modified = line_modified;
}

/************************************************************************/

public void edit_line_set_insert_mode (ref EDIT_LINE edit_line, bool insert_mode)
{
  edit_line.insert_mode = insert_mode;
}

/************************************************************************/

public void edit_line_set_first_mark (ref EDIT_LINE edit_line, LINE_MARK first)
{
  edit_line.first = first;
  edit_line.marks_modified = true;
}

/************************************************************************/

public void edit_line_set_last_mark (ref EDIT_LINE edit_line, LINE_MARK last)
{
  edit_line.last = last;
  edit_line.marks_modified = true;
}

/************************************************************************/

public void edit_line_achar (ref EDIT_LINE edit_line, wchar achar)
{
  if (achar < L' ' || achar == (wchar)127)
    return;

  if (!edit_line.modify_allowed)
    return;

  if (edit_line.col >= edit_line.line^'length)   // past last col
    return;

  if (edit_line.insert_mode)                   // first, insert a space
  {
    if (edit_line.length >= edit_line.line^'length)    // line is full
      return;

    if (edit_line.col < edit_line.length)   // else 'line' is undefined
    {
      int len = edit_line.length - edit_line.col;
      edit_line.line^[edit_line.col+1 : len] = edit_line.line^[edit_line.col :  len];
      edit_line.line^[edit_line.col] = L' ';
      edit_line.length++;
      edit_line.line_modified = true;
    }
  }


  // write achar in 'line'
  if (achar == L' ' && edit_line.col >= edit_line.length)
  {
    // do nothing (this avoids flagging the line as modified)
  }
  else
  {
    if (edit_line.col >= edit_line.length)   // length correction
    {
      edit_line.line^[edit_line.length : edit_line.col - edit_line.length] = {all => L' '};
      edit_line.length = edit_line.col + 1;
    }
    edit_line.line^[edit_line.col] = achar;

    // remove trailing spaces
    while (edit_line.length > 0 && edit_line.line^[edit_line.length-1] == L' ')
      edit_line.length--;

    edit_line.line_modified = true;
  }

  // move the cursor 1 position to the right
  edit_line_cursor_right (ref edit_line);
}

/************************************************************************/

public void edit_line_switch_insert (ref EDIT_LINE edit_line)
{
  if (edit_line.switch_mode_allowed)
    edit_line.insert_mode = (!edit_line.insert_mode);
}

/************************************************************************/

public void edit_line_home (ref EDIT_LINE edit_line)
{
  edit_line.col = 0;
}

/************************************************************************/

public void edit_line_end (ref EDIT_LINE edit_line)
{
  edit_line.col = edit_line.length;

  if (edit_line.col == edit_line.line^'length && (!edit_line.extra_col))
    edit_line.col--;
}

/************************************************************************/

public void edit_line_cursor_left (ref EDIT_LINE edit_line)
{
  if (edit_line.col > 0)
    edit_line.col--;
}

/************************************************************************/

public void edit_line_cursor_right (ref EDIT_LINE edit_line)
{
  int limit;

  if (edit_line.extra_col)
    limit = edit_line.line^'length;
  else
    limit = edit_line.line^'length - 1;

  if (edit_line.col < limit)
    edit_line.col++;
}

/************************************************************************/

public void edit_line_set_top_mark (ref EDIT_LINE edit_line)
{
  if (edit_line.set_marks_allowed)
  {
    edit_line.marks_modified = true;
    edit_line.first.active   = true;
    edit_line.first.col      = edit_line.col;
  }
}

/************************************************************************/

public void edit_line_set_bottom_mark (ref EDIT_LINE edit_line)
{
  if (edit_line.set_marks_allowed)
  {
    edit_line.marks_modified = true;
    edit_line.last.active    = true;
    edit_line.last.col       = edit_line.col;
  }
}

/************************************************************************/

public void edit_line_mark_all (ref EDIT_LINE edit_line)
{
  if (edit_line.set_marks_allowed)
  {
    edit_line.marks_modified = true;
    edit_line.first.active   = true;
    edit_line.first.col      = 0;
    edit_line.last.active    = true;
    edit_line.last.col       = edit_line.length;
  }
}

/************************************************************************/

bool is_word (wchar c, wchar adj_c)
{
  return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
         (c >= L'0' && c <= L'9') || (c == L'_') || (c >= (wchar)128 ||
         (c == L'.' && adj_c >= L'0' && adj_c <= L'9'));  // accept dot of float number
}

/************************************************************************/

public int edit_line_delete (ref EDIT_LINE edit_line)
{
  int len;

  if (!edit_line.modify_allowed)
    return 0;

  if (edit_line.col >= edit_line.length)   // join with line below
  {
    if (edit_line.insert_mode)
      return 1;
    else
      return 0;
  }

  len = edit_line.length-1-edit_line.col;
  edit_line.line^[edit_line.col : len] = edit_line.line^[edit_line.col+1 : len];
  edit_line.length--;

  // remove trailing spaces
  while (edit_line.length > 0 && edit_line.line^[edit_line.length-1] == L' ')
    edit_line.length--;

  edit_line.line_modified = true;

  return 0;
}

/************************************************************************/

public int edit_line_delete_word (ref EDIT_LINE edit_line)
{
  wchar c;

  if (!edit_line.modify_allowed)
    return 0;

  if (edit_line.col >= edit_line.length)   // join with line below
    return edit_line_delete (ref edit_line);

  c = edit_line.line^[edit_line.col];

  if (c == L' ')
  {
    while (edit_line.col < edit_line.length && edit_line.line^[edit_line.col] == L' ')
      (void)edit_line_delete (ref edit_line);
    return 0;
  }
  else if (is_word (c, adj_c => (edit_line.col+1 < edit_line.length ? edit_line.line^[edit_line.col+1] : Lnul)))
  {
    while (edit_line.col < edit_line.length &&
           is_word (edit_line.line^[edit_line.col], adj_c => (edit_line.col+1 < edit_line.length) ? edit_line.line^[edit_line.col+1] : Lnul))
    {
      (void)edit_line_delete (ref edit_line);
    }
    return 0;
  }
  else
  {
    return edit_line_delete (ref edit_line);
  }
}

/************************************************************************/

public int edit_line_backspace (ref EDIT_LINE edit_line)
{
  if (!edit_line.modify_allowed)
    return 0;

  if (edit_line.col == 0)      // join with line above
  {
    if (edit_line.insert_mode)
      return 1;
    else
      return 0;
  }

  edit_line.col--;

  (void)edit_line_delete (ref edit_line);

  return 0;
}

/************************************************************************/

public int edit_line_previous_word (ref EDIT_LINE edit_line, bool begin_of_word, bool come_from_next_line = false)
{
  int length = edit_line.length;
  int col;

  if (length == 0)     // special case : an empty line
    return 1;

  if (begin_of_word)
  {
    if (come_from_next_line)
      col = length;
    else
      col = min(edit_line.col, length);

    // wwww  wwww
    // ^     ^   ^

    col--;        // this can become negative

    // skip non-alphanumeric/underscore sequence
    while (col >= 0 && !is_word (edit_line.line^[col], adj_c => (col-1 >= 0) ? edit_line.line^[col-1] : Lnul))
      col--;      // this can become negative

    if (col < 0)    // no word on this line
      return 1;

    // goto first alphanumeric/underscore char of the sequence
    while (col > 0 && is_word (edit_line.line^[col-1], adj_c => (col-2 >= 0) ? edit_line.line^[col-2] : Lnul))
      col--;

    if (col < 0)    // cannot be handled on this line
      return 1;
  }
  else  // end of word
  {
    if (come_from_next_line)
      col = length + 1;
    else
      col = min(edit_line.col, length + 1);

    // wwww  wwww
    //    ^     ^ +

    col--;

    // skip alphanumeric/underscore sequence
    while (col >= 0 && col < length && is_word (edit_line.line^[col], adj_c => (col-1 >= 0) ? edit_line.line^[col-1] : Lnul))
      col--;      // this can become negative

    // skip non-alphanumeric/underscore sequence
    while (col >= 0 && (col >= length || !is_word (edit_line.line^[col], adj_c => (col-1 >= 0) ? edit_line.line^[col-1] : Lnul)))
      col--;      // this can become negative

    if (col < 0)    // cannot be handled on this line
      return 1;

    col++;
  }

  edit_line.col = col;
  return 0;
}

/************************************************************************/

public int edit_line_next_word (ref EDIT_LINE edit_line, bool begin_of_word, bool come_from_previous_line = false)
{
  int col, length;

  length = edit_line.length;

  if (come_from_previous_line)
    col = -1;
  else
    col = edit_line.col;

  if (col >= length)  // we're at or past end-of-line
    return 1;

  if (begin_of_word)
  {
    // wwww  wwww
    // ^     ^   ^

    // skip alphanumeric/underscore sequence
    while (col >= 0 && col < length && is_word (edit_line.line^[col], adj_c => (col-1 >= 0) ? edit_line.line^[col-1] : Lnul))
      col++;

    // skip non-alphanumeric/underscore sequence
    while (col < 0 || (col < length && !is_word (edit_line.line^[col], adj_c => (col-1 >= 0) ? edit_line.line^[col-1] : Lnul)))
      col++;

    if (col >= length)     // cannot be handled on this line
      return 1;
  }
  else  // end of word
  {
    // wwww  wwww
    //    ^     ^ +

    // skip non-alphanumeric/underscore sequence
    while (col < 0 || (col < length && !is_word (edit_line.line^[col], adj_c => (col-1 >= 0) ? edit_line.line^[col-1] : Lnul)))
      col++;

    if (col >= length)     // cannot be handled on this line
      return 1;

    // skip alphanumeric/underscore sequence
    while (col < length && is_word (edit_line.line^[col], adj_c => (col-1 >= 0) ? edit_line.line^[col-1] : Lnul))
      col++;
  }

  edit_line.col = col;
  return 0;
}

/************************************************************************/

public wstring^ edit_line_get_marked_text (ref EDIT_LINE edit_line)
{
  if (edit_line.first.active && edit_line.last.active && edit_line.first.col < edit_line.last.col)
  {
    edit_line.line^[edit_line.length : edit_line.line^'length - edit_line.length] = {all => L' '};
    return new wstring ' (edit_line.line^[edit_line.first.col : edit_line.last.col - edit_line.first.col]);
  }
  else
    return new wchar[0];
}

/************************************************************************/

// delete the marked text and set col to its start
// returns 0 if OK, 1 if no text marked

public int edit_line_delete_block (ref EDIT_LINE edit_line)
{
  if (edit_line.first.active && edit_line.last.active)
  {
    int first = edit_line.first.col;
    int last = min (edit_line.last.col, edit_line.length);

    if (first < last)
    {
      int len_to_move = edit_line.length - last;
      edit_line.line^[first : len_to_move] = edit_line.line^[last : len_to_move];
      edit_line.length = first + len_to_move;
      edit_line.col = first;
      clear edit_line.first, edit_line.last;
      edit_line.line_modified = true;
      edit_line.marks_modified = true;

      return 0;
    }
  }

  return 1;
}

/************************************************************************/

// insert text at cursor col, set marks around inserted text, put cursor at end of insertion
// returns 0 if OK, 1 if text too large

public int edit_line_insert_text_block (ref EDIT_LINE edit_line, wstring text)
{
  int length = max (edit_line.length, edit_line.col);
  int len_to_move, i, j;

  if (length + text'length > edit_line.line^'length)   // too large
    return 1;

  // pad with spaces
  edit_line.line^[edit_line.length : edit_line.line^'length - edit_line.length] = {all => L' '};

  len_to_move = length - edit_line.col;
  edit_line.line^[edit_line.col + text'length : len_to_move] = edit_line.line^[edit_line.col : len_to_move];
  edit_line.line^[edit_line.col : text'length] = text;
  for (i=0,j=edit_line.col; i<text'length; i++,j++)
  {
    wchar c = text[i];
    if (c < L' ')
      c = L' ';
    edit_line.line^[j] = c;
  }
  edit_line.length = length + text'length;
  edit_line.col += text'length;
  edit_line.first = {active => true, col => edit_line.col};
  edit_line.last = {active => true, col => edit_line.col + text'length};
  edit_line.line_modified = true;
  edit_line.marks_modified = true;

  return 0;
}

/************************************************************************/

public
void edit_line_clone (EDIT_LINE source, out EDIT_LINE target)
{
  target = source;
  target.line = new wstring ' (source.line^);
}

//--------------------------------------------------------------------------

// source is deleted
public
void edit_line_unsafe_copy (ref EDIT_LINE source, out EDIT_LINE target)
{
  target = source;
  clear source;
}

//--------------------------------------------------------------------------

public
bool edit_line_identical (EDIT_LINE a, EDIT_LINE b)
{
  return a.length == b.length && memcmp (a.line^[0 : a.length], b.line^[0 : b.length]) == 0;
}

//--------------------------------------------------------------------------
