
// text.c

use sorting, strings;

/* ----------------------------------------------------------------- */

typedef byte[]^ PLINE;

struct A_LINE
{
  A_LINE^ previous;
  A_LINE^ next;
  PLINE   line;
}

struct A_TEXT
{
  int      nb_lines;
  A_LINE^  first;
  A_LINE^  last;
  int      cached_line_number;  /* only if at least 1 line in text */
  A_LINE^  cached_handle;       /* only if at least 1 line in text */
}

/* ----------------------------------------------------------------- */

typedef wstring^ PWLINE;

struct W_LINE
{
  W_LINE^ previous;
  W_LINE^ next;
  PWLINE  line;
}

struct W_TEXT
{
  int      nb_lines;
  W_LINE^  first;
  W_LINE^  last;
  int      cached_line_number;  /* only if at least 1 line in text */
  W_LINE^  cached_handle;       /* only if at least 1 line in text */
}

/* ----------------------------------------------------------------- */

public void creat_text (out A_TEXT handle)
{
  clear handle;
}

/* ----------------------------------------------------------------- */

public void close_text (ref A_TEXT handle)
{
  delete_text (ref handle);
}

/* ----------------------------------------------------------------- */

public int nb_text_lines (A_TEXT handle)
{
  return handle.nb_lines;
}

/* ----------------------------------------------------------------- */

public void delete_text (ref A_TEXT handle)
{
  A_LINE^  h, temp;

  h = handle.first;

  while (h != null)
  {
    temp = h^.next;

    free h^.line;
    free h;

    h = temp;
  }

  clear handle;
}

/* ----------------------------------------------------------------- */

A_LINE^ load_node (ref A_TEXT text_handle,
                   int        line_number)
{
  int     line_nr;
  A_LINE^ p;

  assert (line_number >= 1 && line_number <= text_handle.nb_lines);

  if (line_number == 1)
    return text_handle.first;


  /* here, we are sure that the cache is valid    */
  /* because the text contains at least one line. */

  if (line_number < text_handle.cached_line_number)
  {
    /* between [first and cached[ line */

    if (line_number <= (text_handle.cached_line_number >> 1))
    {
      /* search downwards from first line */
      line_nr = 1;
      p = text_handle.first;
    }
    else
    {
      /* search upwards from cached line */
      line_nr = text_handle.cached_line_number;
      p = text_handle.cached_handle;
    }
  }
  else
  {
    /* between [cached and last] line */

    if (line_number <= text_handle.cached_line_number
         + ((text_handle.nb_lines - text_handle.cached_line_number) >> 1))
    {
      /* search downwards from cache line */
      line_nr = text_handle.cached_line_number;
      p = text_handle.cached_handle;
    }
    else
    {
      /* search upwards from last line */
      line_nr = text_handle.nb_lines;
      p = text_handle.last;
    }
  }

  for (;;)
  {
    if (line_nr == line_number)
    {
      text_handle.cached_line_number = line_number;
      text_handle.cached_handle = p;
      break;
    }

    if (line_number > line_nr)
    {
      p = p^.next;
      line_nr++;
    }
    else
    {
      p = p^.previous;
      line_nr--;
    }
  }

  return p;
}

/* ----------------------------------------------------------------- */

public void insert_text_line (ref A_TEXT handle,
                              int        line_number,
                              byte[]     line)
{
  A_LINE^ p;

  assert (line_number >= 1 && line_number <= handle.nb_lines+1);

  p = new A_LINE'{previous=>null, next=>null, line=>new byte[]'(line)};

  /* init previous field of node */
  if (line_number > 1)
    p^.previous = load_node (ref handle, line_number-1);

  /* init next field of node */
  if (line_number <= handle.nb_lines)
    p^.next = load_node (ref handle, line_number);


  /* link the new node into the current text */

  if (line_number > handle.nb_lines)
    handle.last = p;
  else
    p^.next^.previous = p;

  if (line_number == 1)
    handle.first = p;
  else
    p^.previous^.next = p;

  /* enter newly inserted line into cache */
  handle.cached_line_number = line_number;
  handle.cached_handle = p;

  handle.nb_lines++;
}

/* ----------------------------------------------------------------- */

public void update_text_line (ref A_TEXT handle,
                              int        line_number,
                              byte[]     line)
{
  A_LINE^ p;

  p = load_node (ref handle, line_number);

  free p^.line;
  p^.line = new byte[]'(line);
}

/* ----------------------------------------------------------------- */

public void retrieve_text_line (ref A_TEXT handle,
                                int        line_number,
                                out byte[] buffer,
                                out int    length)
{
  A_LINE^ p;

  clear buffer;

  p = load_node (ref handle, line_number);

  length = p^.line^ ' length;
  if (length > buffer'length)
    length = buffer'length;

  buffer[0:length] = p^.line^[0:length];
}

/* ----------------------------------------------------------------- */

public void delete_text_line (ref A_TEXT handle,
                              int        line_number)
{
  A_LINE^ p, previous, next;

  p = load_node (ref handle, line_number);

  previous = null;
  next = null;

  if (line_number > 1)
  {
    previous = load_node (ref handle, line_number-1);
  }

  if (line_number < handle.nb_lines)
  {
    next = load_node (ref handle, line_number+1);
  }


  /* update next node */
  if (line_number == handle.nb_lines)
    handle.last = previous;
  else
  {
    next^.previous = previous;
  }

  /* update previous node */
  if (line_number == 1)
    handle.first = next;
  else
  {
    previous^.next = next;
  }

  if (handle.nb_lines > 1)  // we must update the cache
  {
    if (line_number == handle.nb_lines)    // we deleted the last line
    {
      handle.cached_line_number = handle.nb_lines - 1;
      handle.cached_handle = handle.last;
    }
    else
    {
      handle.cached_line_number = line_number;
      handle.cached_handle = next;
    }
  }

  handle.nb_lines--;

  free p^.line;
  free p;
}

/* ----------------------------------------------------------------- */
/* --  SORTING  ---------------------------------------------------- */
/* ----------------------------------------------------------------- */

package body USER_SORT_TEXT_LINES

  /* ----------------------------------------------------------------- */

  package PKG = new sorting.HeapSort3 (ELEMENT => PLINE, USER_INFO => USER_INFO);

  /* ----------------------------------------------------------------- */

  public void sort (ref A_TEXT handle, COMPARE_TEXT_LINES compare, ref USER_INFO user_info)
  {
    PLINE[]^ table;
    A_LINE^  p;
    int      i;

    table = new PLINE [handle.nb_lines];

    p = handle.first;
    i = 0;

    while (p != null)
    {
      table^[i++] = p^.line;
      p = p^.next;
    }

    PKG.sort (ref table^, compare, ref user_info);

    p = handle.first;
    i = 0;

    while (p != null)
    {
      p^.line = table^[i++];
      p = p^.next;
    }
  }

  /* ----------------------------------------------------------------- */

end USER_SORT_TEXT_LINES;

/* -------------------------------------------------------------------- */

// compare two strings
// returns -1 if a<b, 0 if equal, +1 if a>b

int cmp (byte[]^ pa, byte[]^ pb, ref bool dummy)
{
  int  len, i;
  byte ca, cb;
  ref byte[] a = pa^;
  ref byte[] b = pb^;

  _unused dummy;

  len = (a'length < b'length) ? a'length : b'length;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != 0)
      continue;

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;
    if (ca == 0)  // both 0
      return 0;
  }

  if (a'length < b'length)
    return b[i] == 0 ? 0 : -1;

  if (a'length > b'length)
    return a[i] == 0 ? 0 : +1;

  return 0;
}

/* -------------------------------------------------------------------- */

package U1 = new USER_SORT_TEXT_LINES (USER_INFO => bool);

/* -------------------------------------------------------------------- */

public void sort_text_lines (ref A_TEXT handle)
{
  bool dummy = false;
  U1.sort (ref handle, cmp, ref dummy);
}


//--------------------------------------------------------------------------

public
void text_copy (A_TEXT source, out A_TEXT target)
{
  A_LINE^ p;
  int     line;

  clear target;
  line = 1;
  
  p = source.first;
  while (p != null)
  {
    insert_text_line (ref target, line++, p^.line^);
    p = p^.next;    
  }
}

//--------------------------------------------------------------------------

// source is deleted

public
void text_unsafe_copy (ref A_TEXT source, out A_TEXT target)
{
  target = source;
  clear source;
}

//--------------------------------------------------------------------------

public
bool text_identical (A_TEXT a, A_TEXT b)
{
  A_LINE^ p, q;

  p = a.first;
  q = b.first;
  
  while (p != null && q != null)
  {
    if (p^.line^'length == q^.line^'length && memcmp (p^.line^, q^.line^) == 0)
      ;
    else
      return false;

    p = p^.next;    
    q = q^.next;    
  }

  return p == null && q == null;
}

/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */

public void wcreat_text (out W_TEXT handle)
{
  clear handle;
}

/* ----------------------------------------------------------------- */

public void wclose_text (ref W_TEXT handle)
{
  wdelete_text (ref handle);
}

/* ----------------------------------------------------------------- */

public int wnb_text_lines (W_TEXT handle)
{
  return handle.nb_lines;
}

/* ----------------------------------------------------------------- */

public void wdelete_text (ref W_TEXT handle)
{
  W_LINE^  h, temp;

  h = handle.first;

  while (h != null)
  {
    temp = h^.next;

    free h^.line;
    free h;

    h = temp;
  }

  clear handle;
}

/* ----------------------------------------------------------------- */

W_LINE^ wload_node (ref W_TEXT text_handle,
                        int    line_number)
{
  int     line_nr;
  W_LINE^ p;

  assert (line_number >= 1 && line_number <= text_handle.nb_lines);

  if (line_number == 1)
    return text_handle.first;


  /* here, we are sure that the cache is valid    */
  /* because the text contains at least one line. */

  if (line_number < text_handle.cached_line_number)
  {
    /* between [first and cached[ line */

    if (line_number <= (text_handle.cached_line_number >> 1))
    {
      /* search downwards from first line */
      line_nr = 1;
      p = text_handle.first;
    }
    else
    {
      /* search upwards from cached line */
      line_nr = text_handle.cached_line_number;
      p = text_handle.cached_handle;
    }
  }
  else
  {
    /* between [cached and last] line */

    if (line_number <= text_handle.cached_line_number
         + ((text_handle.nb_lines - text_handle.cached_line_number) >> 1))
    {
      /* search downwards from cache line */
      line_nr = text_handle.cached_line_number;
      p = text_handle.cached_handle;
    }
    else
    {
      /* search upwards from last line */
      line_nr = text_handle.nb_lines;
      p = text_handle.last;
    }
  }

  for (;;)
  {
    if (line_nr == line_number)
    {
      text_handle.cached_line_number = line_number;
      text_handle.cached_handle = p;
      break;
    }

    if (line_number > line_nr)
    {
      p = p^.next;
      line_nr++;
    }
    else
    {
      p = p^.previous;
      line_nr--;
    }
  }

  return p;
}

/* ----------------------------------------------------------------- */

public void winsert_text_line (ref W_TEXT   handle,
                                   int      line_number,
                                   wstring  line)
{
  W_LINE^ p;

  assert (line_number >= 1 && line_number <= handle.nb_lines+1);

  p = new W_LINE'{previous=>null, next=>null, line=>new wstring'(line)};

  /* init previous field of node */
  if (line_number > 1)
    p^.previous = wload_node (ref handle, line_number-1);

  /* init next field of node */
  if (line_number <= handle.nb_lines)
    p^.next = wload_node (ref handle, line_number);


  /* link the new node into the current text */

  if (line_number > handle.nb_lines)
    handle.last = p;
  else
    p^.next^.previous = p;

  if (line_number == 1)
    handle.first = p;
  else
    p^.previous^.next = p;

  /* enter newly inserted line into cache */
  handle.cached_line_number = line_number;
  handle.cached_handle = p;

  handle.nb_lines++;
}

/* ----------------------------------------------------------------- */

public void wupdate_text_line (ref W_TEXT   handle,
                                   int      line_number,
                                   wstring  line)
{
  W_LINE^ p;

  p = wload_node (ref handle, line_number);

  free p^.line;
  p^.line = new wstring'(line);
}

/* ----------------------------------------------------------------- */

public void wretrieve_text_line (ref W_TEXT   handle,
                                     int      line_number,
                                 out wstring  buffer,
                                 out int      length)
{
  W_LINE^ p;

  clear buffer;

  p = wload_node (ref handle, line_number);

  length = p^.line^ ' length;
  if (length > buffer'length)
    length = buffer'length;

  buffer[0:length] = p^.line^[0:length];
}

/* ----------------------------------------------------------------- */

public void wdelete_text_line (ref W_TEXT handle,
                                   int    line_number)
{
  W_LINE^ p, previous, next;

  p = wload_node (ref handle, line_number);

  previous = null;
  next = null;

  if (line_number > 1)
  {
    previous = wload_node (ref handle, line_number-1);
  }

  if (line_number < handle.nb_lines)
  {
    next = wload_node (ref handle, line_number+1);
  }


  /* update next node */
  if (line_number == handle.nb_lines)
    handle.last = previous;
  else
  {
    next^.previous = previous;
  }

  /* update previous node */
  if (line_number == 1)
    handle.first = next;
  else
  {
    previous^.next = next;
  }

  if (handle.nb_lines > 1)  // we must update the cache
  {
    if (line_number == handle.nb_lines)    // we deleted the last line
    {
      handle.cached_line_number = handle.nb_lines - 1;
      handle.cached_handle = handle.last;
    }
    else
    {
      handle.cached_line_number = line_number;
      handle.cached_handle = next;
    }
  }

  handle.nb_lines--;

  free p^.line;
  free p;
}

/* ----------------------------------------------------------------- */
/* --  SORTING  ---------------------------------------------------- */
/* ----------------------------------------------------------------- */

package body WUSER_SORT_TEXT_LINES

  /* ----------------------------------------------------------------- */

  package PKGW = new sorting.HeapSort3 (ELEMENT => PWLINE, USER_INFO => USER_INFO);

  /* ----------------------------------------------------------------- */

  public void sort (ref W_TEXT handle, WCOMPARE_TEXT_LINES compare, ref USER_INFO user_info)
  {
    PWLINE[]^ table;
    W_LINE^   p;
    int       i;

    table = new PWLINE [handle.nb_lines];

    p = handle.first;
    i = 0;

    while (p != null)
    {
      table^[i++] = p^.line;
      p = p^.next;
    }

    PKGW.sort (ref table^, compare, ref user_info);

    p = handle.first;
    i = 0;

    while (p != null)
    {
      p^.line = table^[i++];
      p = p^.next;
    }
  }

  /* ----------------------------------------------------------------- */
  
end WUSER_SORT_TEXT_LINES;

/* -------------------------------------------------------------------- */

// compare two strings
// returns -1 if a<b, 0 if equal, +1 if a>b

int wcmp (wstring^ pa, wstring^ pb, ref bool dummy)
{
  int   len, i;
  wchar ca, cb;
  ref wstring a = pa^;
  ref wstring b = pb^;

  _unused dummy;
  
  len = (a'length < b'length) ? a'length : b'length;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != Lnul)
      continue;

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;
    if (ca == Lnul)  // both 0
      return 0;
  }

  if (a'length < b'length)
    return b[i] == Lnul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == Lnul ? 0 : +1;

  return 0;
}

/* -------------------------------------------------------------------- */

package U2 = new WUSER_SORT_TEXT_LINES (USER_INFO => bool);

/* -------------------------------------------------------------------- */

public void wsort_text_lines (ref W_TEXT handle)
{
  bool dummy = false;
  U2.sort (ref handle, wcmp, ref dummy);
}

/* -------------------------------------------------------------------- */

public
void wtext_clone (W_TEXT source, out W_TEXT target)
{
  W_LINE^ p;
  int     line;

  clear target;
  line = 1;
  
  p = source.first;
  while (p != null)
  {
    winsert_text_line (ref target, line++, p^.line^);
    p = p^.next;    
  }
}

//--------------------------------------------------------------------------

// source is deleted

public
void wtext_unsafe_copy (ref W_TEXT source, out W_TEXT target)
{
  target = source;
  clear source;
}

//--------------------------------------------------------------------------

public
bool wtext_identical (W_TEXT a, W_TEXT b)
{
  W_LINE^ p, q;

  p = a.first;
  q = b.first;
  
  while (p != null && q != null)
  {
    if (p^.line^'length == q^.line^'length && memcmp (p^.line^, q^.line^) == 0)
      ;
    else
      return false;

    p = p^.next;    
    q = q^.next;    
  }

  return p == null && q == null;
}
