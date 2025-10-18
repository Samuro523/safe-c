
// set.c : sets stored using balanced binary tree of intervals

use arithm, bintree;

#define unit_tests 0

#if unit_tests
  from std use exception, console;
#endif

/************************************************************************/

struct INTERVAL
{
  long first;
  long last;
}

/************************************************************************/

package P = new BALANCED_BINARY_TREE (ELEMENT => INTERVAL, USER_INFO => bool);

struct COUNTERS
{
  long nb_intervals;
  long nb_items;
}

struct SET
{
  BINARY_TREE b;
  COUNTERS    count;
}

/************************************************************************/

int compare (bool^      user,
             INTERVAL   data1,
             INTERVAL   data2)
{
  _unused user;
  if (data1.first < data2.first)
    return -1;
  if (data1.first > data2.first)
    return +1;
  return 0;
}

/************************************************************************/

public void SET_create (out SET set)
{
  clear set;
  create_btree (out set.b, null, compare);
}

/************************************************************************/

public void SET_close (ref SET set)
{
  close_btree (ref set.b);
  clear set;
}

/************************************************************************/

void insert (ref SET set, INTERVAL i)
{
  assert insert_btree (ref set.b, i) == 0;
  set.count.nb_intervals++;
  set.count.nb_items += (i.last - i.first + 1);
}

/************************************************************************/

void update (ref SET set, INTERVAL i)
{
  INTERVAL old_i;
  assert read_then_update_btree (ref set.b, i, out old_i) == 0;
  set.count.nb_items += (i.last - i.first) - (old_i.last - old_i.first);
}

/************************************************************************/

void delete (ref SET set, INTERVAL i)
{
  INTERVAL i2 = i;
  assert read_then_delete_btree (ref set.b, ref i2) == 0;
  set.count.nb_intervals--;
  set.count.nb_items -= (i2.last - i2.first + 1);
}

/************************************************************************/

/* insert all items between first and last into the set.                  */
/* returns true if OK (all items were inserted),                          */
/*         false if not done (some or all items were already in the set). */

public bool SET_insert_interval (ref SET set, long first, long last)
{
  INTERVAL  left, right;
  int       rc1, rc2;

  if (first > last)     // empty set : done
    return true;

  /* load intervals surrounding 'first' */

  left = {first => first, last => 0};
  right = left;

  rc1 = retrieve_btree (set.b, ref left, BT_SMALLER);
  if (rc1 == 0 && first <= left.last)  // first is contained in interval
    return false;

  rc2 = retrieve_btree (set.b, ref right, BT_EQUAL_OR_LARGER);
  if (rc2 == 0 && last >= right.first)  // last is contained in interval
    return false;

  if (rc1 == 0 && first == left.last + 1 &&
      rc2 == 0 && last  == right.first - 1)
  {
    /* concatenate both intervals */
    left.last = right.last;

    update (ref set, left);
    delete (ref set, right);
  }
  else if (rc1 == 0 && first == left.last + 1)
  {
    /* enlarge left interval */
    left.last = last;
    update (ref set, left);
  }
  else if (rc2 == 0 && last == right.first - 1)
  {
    /* enlarge right interval */
    delete (ref set, right);
    right.first = first;
    insert (ref set, right);
  }
  else
  {
    /* create new node */
    insert (ref set, {first => first, last => last});
  }

  return true;
}

/************************************************************************/

public bool SET_insert_item (ref SET set, long item)
{
  return SET_insert_interval (ref set, item, item);
}

/************************************************************************/

/* delete all items between first and last from the set.                      */
/* returns true if OK (all items were deleted),                               */
/*         false if not done (some or all items were not present in the set). */

public bool SET_delete_interval (ref SET set, long first, long last)
{
  INTERVAL  left, right;
  int       rc;

  if (first > last)     // empty set : done
    return true;

  /* load interval containing 'first' */

  left = {first => first, last => 0};
  rc = retrieve_btree (set.b, ref left, BT_EQUAL_OR_SMALLER);


  /* check legality of the interval */

  if (rc < 0 || last > left.last)
    return false;


  if (first == left.first && last == left.last)
  {
    /* delete the node */
    delete (ref set, left);
  }
  else if (last == left.last)
  {
    /* shrink right bound */
    left.last = first - 1;
    update (ref set, left);
  }
  else if (first == left.first)
  {
    /* shrink left bound */
    delete (ref set, left);
    left.first = last + 1;
    insert (ref set, left);
  }
  else
  {
    /* split the node */
    right = left;

    left.last   = first - 1;
    right.first = last  + 1;

    update (ref set, left);
    insert (ref set, right);
  }

  return true;
}

/************************************************************************/

public bool SET_delete_item (ref SET set, long item)
{
  return SET_delete_interval (ref set, item, item);
}

/************************************************************************/

/* get first/last item in the set.                           */
/* returns with first == 1 && last == 0 if the set is empty. */

public void SET_get_range (SET set, out long first, out long last)
{
  INTERVAL left, right;
  int      rc;

  clear left;

  rc = retrieve_btree (set.b, ref left, BT_FIRST);
  if (rc < 0)
  {
    first = 1;
    last  = 0;
    return;
  }

  clear right;
  assert retrieve_btree (set.b, ref right, BT_LAST) == 0;

  first = left.first;
  last  = right.last;
}

/************************************************************************/

/* count the number of items in the set */

public long SET_nb_items (SET set)
{
  return set.count.nb_items;
}

/************************************************************************/

public long SET_nb_intervals (SET set)
{
  return set.count.nb_intervals;
}

/************************************************************************/

/* test if all items in the interval [first .. last] are in the set */

public bool SET_interval_found (SET set, long first, long last)
{
  INTERVAL left;
  int      rc;


  if (first > last)     /* empty set : done */
    return true;


  /* load interval containing 'first' */

  left = {first => first, last => 0};
  rc = retrieve_btree (set.b, ref left, BT_EQUAL_OR_SMALLER);

  /* check interval */

  if (rc != 0 || last > left.last)
    return false;

  return true;
}

/************************************************************************/

/* test if an item is in the set */

public bool SET_item_found (SET set, long item)
{
  return SET_interval_found (set, item, item);
}

/************************************************************************/

/* search for the nearest interval of consecutive elements  */
/* that are >= 'low'.                                       */
/* returns true if found,                                   */
/*         false if no items larger or equal to 'low'.      */

public bool SET_search_any_interval (    SET  set,
                                         long low,
                                     out long first,
                                     out long last)
{
  INTERVAL left, right;
  int      rc1, rc2;


  /* load intervals surrounding 'low' */

  left = {first => low, last => 0};
  right = left;

  /* check possible results */

  rc1 = retrieve_btree (set.b, ref left, BT_SMALLER);
  if (rc1 == 0 && low <= left.last)
  {
    first = low;
    last  = left.last;
    return true;
  }

  rc2 = retrieve_btree (set.b, ref right, BT_EQUAL_OR_LARGER);
  if (rc2 == 0)
  {
    first = right.first;
    last  = right.last;
    return true;
  }

  clear first, last;
  return false;
}

/************************************************************************/

/* search an item that is >= 'low'.              */
/* returns true if found,                        */
/*         false if no such item is in the set.  */

public bool SET_search_any_item (    SET  set,
                                     long low,
                                 out long item)
{
  long last;
  return SET_search_any_interval (set, low, out item, out last);
  _unused last;
}

/************************************************************************/

/* search for the nearest interval of consecutive elements  */
/* that are <= 'high'.                                      */
/* returns true if found,                                   */
/*         false if no items smaller or equal to 'high'.    */

public bool SET_search_any_interval2 (    SET  set,
                                          long high,
                                      out long first,
                                      out long last)
{
  INTERVAL  left;
  int       rc;

  left = {first => high, last => 0};
  rc = retrieve_btree (set.b, ref left, BT_EQUAL_OR_SMALLER);
  if (rc == 0)
  {
    first = left.first;
    if (high > left.last)
      last = left.last;
    else
      last = high;
    return true;
  }

  clear first, last;
  return false;
}

/************************************************************************/

/* search an item that is <= 'high'.            */
/* returns true if found,                       */
/*         false if no such item is in the set. */

public bool SET_search_any_item2 (    SET  set,
                                      long high,
                                  out long item)
{
  long first;
  return SET_search_any_interval2 (set, high, out first, out item);
  _unused first;
}

/************************************************************************/

void union_interval (ref SET set, INTERVAL interval)
{
  INTERVAL left, right;
  int      rc;

  // get interval touching first point
  left = interval;
  rc = retrieve_btree (set.b, ref left, BT_EQUAL_OR_SMALLER);  // if ok, assert left.first <= interval.first
  if (rc < 0 || interval.first > left.last+1) 
  {
    // is disjoint from left interval
    insert (ref set, interval);
    left = interval;
  }
  else
  {
    assert interval.first >= left.first && interval.first <= left.last+1;
    if (interval.last > left.last)
      left.last = interval.last;
    update (ref set, left);
  }

  // cleanup : adapt all following intervals (update, delete)
  right = left;
  for (;;)
  {
    rc = retrieve_btree (set.b, ref right, BT_LARGER);
    if (rc < 0 || right.first > left.last+1)  // nothing more to merge
      break;

    assert right.first <= left.last + 1;  // merge

    delete (ref set, right);

    if (left.last <= right.last)
    {
      if (left.last < right.last)
      {
        left.last = right.last;
        update (ref set, left);
      }
      break;
    }
  }
}

/************************************************************************/

/* insert all items of set 'source' to set 'target' */

public void SET_union (ref SET target, SET source)
{
  INTERVAL  interval;
  ushort    mode;

  mode = BT_FIRST;
  clear interval;

  for (;;)
  {
    if (retrieve_btree (source.b, ref interval, mode) < 0)
      break;

    union_interval (ref target, interval);

    mode = BT_LARGER;
  }
}

/************************************************************************/

/* remove all items from set 'target' that do not exist in set 'source' */

public void SET_intersect (ref SET target, SET source)
{
  INTERVAL  s, t;
  int       rcs, rct;
  SET       result;

  clear s, t, rcs, rct;
  SET_create (out result);

  rcs = retrieve_btree (source.b, ref s, BT_FIRST);
  rct = retrieve_btree (target.b, ref t, BT_FIRST);

  while (rcs == 0 && rct == 0)
  {
    if (s.last < t.first)  // no overlap, s is too small
    {
      rcs = retrieve_btree (source.b, ref s, BT_LARGER);
    }
    else if (s.first > t.last)  // no overlap, t is too small
    {
      rct = retrieve_btree (target.b, ref t, BT_LARGER);
    }
    else   // overlap
    {
      insert (ref result, {first => lmax(s.first, t.first), last => lmin(s.last, t.last)});
      if (s.last < t.last)   // s is smallest
        rcs = retrieve_btree (source.b, ref s, BT_LARGER);
      else                   // advance t
        rct = retrieve_btree (target.b, ref t, BT_LARGER);
    }
  }

  SET_close (ref target);
  SET_copy (out target => target, source => result);
}

/************************************************************************/

/* copy set 'source' to set 'target' */

public void SET_copy (out SET target, SET source)
{
  INTERVAL  interval;
  ushort    mode;

  SET_create (out target);

  mode = BT_FIRST;
  clear interval;

  for (;;)
  {
    if (retrieve_btree (source.b, ref interval, mode) < 0)
      break;

    insert (ref target, interval);

    mode = BT_LARGER;
  }
}

/************************************************************************/

#if unit_tests   // exhaustive unit tests

package Q

  const int LENGTH = 10;

  typedef bool[LENGTH] SSET;

  void SSET_create (out SSET set);
  void SSET_close (ref SSET set);
  bool SSET_insert_interval (ref SSET set, long first, long last);
  bool SSET_insert_item (ref SSET set, long item);
  bool SSET_delete_interval (ref SSET set, long first, long last);
  bool SSET_delete_item (ref SSET set, long item);
  void SSET_get_range (SSET set, out long first, out long last);
  long SSET_nb_items (SSET set);
  long SSET_nb_intervals (SSET set);
  bool SSET_interval_found (SSET set, long first, long last);
  bool SSET_item_found (SSET set, long item);
  bool SSET_search_any_interval (    SSET  set, long low, out long first, out long last);
  bool SSET_search_any_item (   SSET  set,  long low, out long item);
  bool SSET_search_any_interval2 (   SSET  set,  long high, out long first,  out long last);
  bool SSET_search_any_item2 (    SSET  set,    long high,  out long item);
  void SSET_union (ref SSET target, SSET source);
  void SSET_intersect (ref SSET target, SSET source);
  void SSET_copy (out SSET target, SSET source);

end Q;

package R

  void SET_make (out SET set, int n);
  void SSET_make (out SSET set, int n);
  int SET_value (SET set);
  int SSET_value (SSET set);
  void SET_check_equal (SET set, SSET sset, int testnr, int j, int k);

end R;


package body Q

  public void SSET_create (out SSET set)
  {
    clear set;
  }

  /************************************************************************/

  public void SSET_close (ref SSET set)
  {
    clear set;
  }

  /************************************************************************/

  public bool SSET_insert_interval (ref SSET set, long first, long last)
  {
    int i;
    SSET old = set;
    for (i=(int)first; i<=(int)last; i++)
    {
      if (set[i])
      {
        set = old;
        return false;
      }
      set[i] = true;
    }
    return true;
  }

  /************************************************************************/

  public bool SSET_insert_item (ref SSET set, long item)
  {
    return SSET_insert_interval (ref set, item, item);
  }

  /************************************************************************/

  public bool SSET_delete_interval (ref SSET set, long first, long last)
  {
    int i;
    SSET old = set;
    for (i=(int)first; i<=(int)last; i++)
    {
      if (!set[i])
      {
        set = old;
        return false;
      }
      set[i] = false;
    }
    return true;
  }

  /************************************************************************/

  public bool SSET_delete_item (ref SSET set, long item)
  {
    return SSET_delete_interval (ref set, item, item);
  }

  /************************************************************************/

  public void SSET_get_range (SSET set, out long first, out long last)
  {
    first = 0;
    last = set'length - 1;
    while (first <= last && !set[(int)first])
      first++;
    while (first <= last && !set[(int)last])
      last--;
    if (first > last)
    {
      first = 1;
      last  = 0;
      return;
    }
  }

  /************************************************************************/

  public long SSET_nb_items (SSET set)
  {
    int i, count;
    count = 0;
    for (i=0; i<set'length; i++)
    {
      if (set[i])
        count++;
    }
    return count;
  }

  /************************************************************************/

  public long SSET_nb_intervals (SSET set)
  {
    int i, count;
    bool previous = false;
    count = 0;
    for (i=0; i<=set'length-1; i++)
    {
      if (previous == false && set[i])  // toggle 0->1
        count++;
      previous = set[i];
    }
    return count;
  }

  /************************************************************************/

  public bool SSET_interval_found (SSET set, long first, long last)
  {
    int i;
    for (i=(int)first; i<=(int)last; i++)
    {
      if (!set[i])
        return false;
    }
    return true;
  }

  /************************************************************************/

  public bool SSET_item_found (SSET set, long item)
  {
    return SSET_interval_found (set, item, item);
  }

  /************************************************************************/

  public bool SSET_search_any_interval (    SSET set,
                                            long low,
                                        out long first,
                                        out long last)
  {
    int i;
    for (i=(int)low; i<(int)set'length; i++)
    {
      if (set[i])
      {
        first = i;
        last = i;
        while (last+1<(int)set'length && set[(int)last+1])
          last++;
        return true;
      }
    }

    first = 0;
    last = 0;
    return false;
  }

  /************************************************************************/

  public bool SSET_search_any_item (   SSET  set,
                                       long low,
                                   out long item)
  {
    long last;
    return SSET_search_any_interval (set, low, out item, out last);
    _unused last;
  }

  /************************************************************************/

  public bool SSET_search_any_interval2 (   SSET  set,
                                            long high,
                                        out long first,
                                        out long last)
  {
    int i;
    for (i=(int)high; i>=0; i--)
    {
      if (set[i])
      {
        last = i;
        first = i;
        while (first > 0 && set[(int)first-1])
          first--;
        return true;
      }
    }

    first = 0;
    last = 0;
    return false;
  }

  /************************************************************************/

  public bool SSET_search_any_item2 (    SSET  set,
                                         long high,
                                     out long item)
  {
    long first;
    return SSET_search_any_interval2 (set, high, out first, out item);
    _unused first;
  }

  /************************************************************************/

  public void SSET_union (ref SSET target, SSET source)
  {
    int i;
    for (i=0; i<source'length; i++)
      target[i] |= source[i];
  }

  /************************************************************************/

  public void SSET_intersect (ref SSET target, SSET source)
  {
    int i;
    for (i=0; i<source'length; i++)
      target[i] &= source[i];
  }

  /************************************************************************/

  public void SSET_copy (out SSET target, SSET source)
  {
    target = source;
  }

  /************************************************************************/
end Q;

package body R

  public void SET_make (out SET set, int n)
  {
    int i;

    SET_create (out set);

    for (i=0; i<SSET'length; )
    {
      if (((1 << i) & n) != 0)
      {
        INTERVAL it = {i, i};

        i++;
        while (i < SSET'length && (((1 << i) & n) != 0))
        {
          it.last = i;
          i++;
        }

        insert (ref set, it);
      }
      else
        i++;
    }
  }

  public int SET_value (SET set)
  {
    INTERVAL i;
    int      rc, n, r;

    clear i, n;
    i.first = -1;

    for (;;)
    {
      rc = retrieve_btree (set.b, ref i, BT_LARGER);
      if (rc < 0)
        return n;
      for (r=(int)i.first; r<=(int)i.last; r++)
        n |= (1 << r);
    }
  }

  public int SSET_value (SSET set)
  {
    int i;
    int n = 0;
    for (i=0; i<SSET'length; i++)
    {
      if (set[i])
        n |= (1 << i);
    }
    return n;
  }

  public void SSET_make (out SSET set, int n)
  {
    int i;
    int r = n;
    clear set;
    for (i=0; i<SSET'length; i++)
    {
      int f = 1 << i;
      if ((f & n) != 0)
      {
        set[i] = true;
        r -= f;
      }
    }

    assert r == 0;   // n too large ?  (max 2^set'length - 1)
  }


  // non-overlapping increasing intervals
  void SET_check_valid (SET set, int testnr)
  {
    INTERVAL i, previous;
    int      rc;

    clear i;
    rc = retrieve_btree (set.b, ref i, BT_FIRST);
    if (rc < 0)
      return;  // empty

    if (i.first > i.last)
    {
      printf ("testnr %d : error empty interval\n", testnr);
      abort;
    }

    for (;;)
    {
      previous = i;

      rc = retrieve_btree (set.b, ref i, BT_LARGER);
      if (rc < 0)
        return;  // no more

      if (i.first > i.last)
      {
        printf ("testnr %d : error empty interval\n", testnr);
        abort;
      }

      if (i.first > previous.last + 1)
        ;  // ok
      else
      {
        printf ("testnr %d : error overlapping range\n", testnr);
        abort;
      }
    }
  }

  public void SET_check_equal (SET set, SSET sset, int testnr, int j, int k)
  {
    SET_check_valid (set, testnr);

    // check matching values
    if (SET_value (set) != SSET_value (sset))
    {
      printf ("test %d, %d, %d : mismatching values (expected=%d, actual=%d)\n", testnr, j, k, SSET_value (sset), SET_value (set));
      abort;
    }

    // nb items & nb interval match
    if (SET_nb_items (set) != SSET_nb_items (sset))
    {
      printf ("test %d, %d, %d : nb items mismatch (expected=%d, actual=%d)\n", testnr, j, k, SSET_nb_items (sset), SET_nb_items (set));
      abort;
    }
    if (SET_nb_intervals (set) != SSET_nb_intervals (sset))
    {
      printf ("test %d, %d, %d : nb interval mismatch (expected=%d, actual=%d)\n", testnr, j, k, SSET_nb_intervals (sset), SET_nb_intervals (set));
      abort;
    }
  }

end R;

void main()
{
  const int COUNT = 1 << LENGTH;
  int i, j, k;

  arm_exception_handler();

  printf ("Unit tests\n");
  printf ("==========\n");
  
  //-------------------------------------------------------------

  printf ("A. create values\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;

    SET_make (out set, i);
    SSET_make (out sset, i);

    if (SET_value (set) != i)
    {
      printf ("test %d : set bad value\n", i);
      abort;
    }
    if (SSET_value (sset) != i)
    {
      printf ("test %d : sset bad value\n", i);
      abort;
    }
    SET_check_equal (set, sset, i, 0, 0);

    SET_close (ref set);
    SSET_close (ref sset);
  }

  //-------------------------------------------------------------

  printf ("B. SET_insert_item\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;

    for (j=0; j<LENGTH; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      if (SET_insert_item (ref set, j) != SSET_insert_item (ref sset, j))
      {
        printf ("test %d : SET_insert_item() returned different bool\n", i);
        abort;
      }

      SET_check_equal (set, sset, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
    }
  }

  //-------------------------------------------------------------

  printf ("C. SET_insert_interval\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;

    for (j=0; j<LENGTH; j++)
    {
      for (k=0; k<LENGTH; k++)
      {
        SET_make (out set, i);
        SSET_make (out sset, i);

        if (SET_insert_interval (ref set, j, k) != SSET_insert_interval (ref sset, j, k))
        {
          printf ("test %d,%d,%d : SET_insert_interval() returned different bool\n", i, j, k);
          abort;
        }

        SET_check_equal (set, sset, i, j, k);

        SET_close (ref set);
        SSET_close (ref sset);
      }
    }
  }

  //-------------------------------------------------------------

  printf ("D. SET_delete_item\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;

    for (j=0; j<LENGTH; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      if (SET_delete_item (ref set, j) != SSET_delete_item (ref sset, j))
      {
        printf ("test %d : SET_delete_item() returned different bool\n", i);
        abort;
      }

      SET_check_equal (set, sset, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
    }
  }

  //-------------------------------------------------------------

  printf ("E. SET_delete_interval\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;

    for (j=0; j<LENGTH; j++)
    {
      for (k=0; k<LENGTH; k++)
      {
        SET_make (out set, i);
        SSET_make (out sset, i);

        if (SET_delete_interval (ref set, j, k) != SSET_delete_interval (ref sset, j, k))
        {
          printf ("test %d,%d,%d : SET_delete_interval() returned different bool\n", i, j, k);
          abort;
        }

        SET_check_equal (set, sset, i, j, k);

        SET_close (ref set);
        SSET_close (ref sset);
      }
    }
  }

  //-------------------------------------------------------------

  printf ("F. SET_get_range\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;
    long a, b, a2, b2;

    SET_make (out set, i);
    SSET_make (out sset, i);

    SET_get_range (set, out a, out b);
    SSET_get_range (sset, out a2, out b2);

    if (a != a2 || b != b2)
    {
      printf ("test %d : SET_get_range() returned different values\n", i);
      abort;
    }

    SET_check_equal (set, sset, i, 0, 0);

    SET_close (ref set);
    SSET_close (ref sset);
  }

  //-------------------------------------------------------------

  printf ("G. SET_interval_found\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;

    for (j=0; j<LENGTH; j++)
    {
      for (k=0; k<LENGTH; k++)
      {
        SET_make (out set, i);
        SSET_make (out sset, i);

        if (SET_interval_found (set, j, k) != SSET_interval_found (sset, j, k))
        {
          printf ("test %d,%d,%d : SET_interval_found() returned different bool\n", i, j, k);
          abort;
        }

        SET_check_equal (set, sset, i, j, k);

        SET_close (ref set);
        SSET_close (ref sset);
      }
    }
  }

  //-------------------------------------------------------------

  printf ("H. SET_item_found\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;

    for (j=0; j<LENGTH; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      if (SET_item_found (set, j) != SSET_item_found (sset, j))
      {
        printf ("test %d,%d,%d : SET_item_found() returned different bool\n", i, j, 0);
        abort;
      }

      SET_check_equal (set, sset, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
    }
  }

  //-------------------------------------------------------------

  printf ("I. SET_search_any_interval\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;
    long a, b, a2, b2;

    for (j=0; j<LENGTH; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      if (SET_search_any_interval (set, j, out a, out b) != SSET_search_any_interval (sset, j, out a2, out b2))
      {
        printf ("test %d,%d,%d : SET_search_any_interval() returned different bool\n", i, j, 0);
        abort;
      }

      if (a != a2 || b != b2)
      {
        printf ("test %d,%d,%d : SET_search_any_interval() returned different ranges\n", i, j, 0);
        abort;
      }

      SET_check_equal (set, sset, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
    }
  }

  //-------------------------------------------------------------

  printf ("J. SET_search_any_item\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;
    long a, a2;

    for (j=0; j<LENGTH; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      if (SET_search_any_item (set, j, out a) != SSET_search_any_item (sset, j, out a2))
      {
        printf ("test %d,%d,%d : SET_search_any_item() returned different bool\n", i, j, 0);
        abort;
      }

      if (a != a2)
      {
        printf ("test %d,%d,%d : SET_search_any_item() returned different ranges\n", i, j, 0);
        abort;
      }

      SET_check_equal (set, sset, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
    }
  }

  //-------------------------------------------------------------

  printf ("K. SET_search_any_interval2\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;
    long a, b, a2, b2;

    for (j=0; j<LENGTH; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      if (SET_search_any_interval2 (set, j, out a, out b) != SSET_search_any_interval2 (sset, j, out a2, out b2))
      {
        printf ("test %d,%d,%d : SET_search_any_interval2() returned different bool\n", i, j, 0);
        abort;
      }

      if (a != a2 || b != b2)
      {
        printf ("test %d,%d,%d : SET_search_any_interval2() returned different ranges\n", i, j, 0);
        abort;
      }

      SET_check_equal (set, sset, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
    }
  }

  //-------------------------------------------------------------

  printf ("L. SET_search_any_item2\n");

  for (i=0; i<COUNT; i++)
  {
    SET set;
    SSET sset;
    long a, a2;

    for (j=0; j<LENGTH; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      if (SET_search_any_item2 (set, j, out a) != SSET_search_any_item2 (sset, j, out a2))
      {
        printf ("test %d,%d,%d : SET_search_any_item2() returned different bool\n", i, j, 0);
        abort;
      }

      if (a != a2)
      {
        printf ("test %d,%d,%d : SET_search_any_item2() returned different ranges\n", i, j, 0);
        abort;
      }

      SET_check_equal (set, sset, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
    }
  }

  //-------------------------------------------------------------

  printf ("M. SET_union\n");

  for (i=0; i<COUNT; i++)
  {
    for (j=0; j<COUNT; j++)
    {
      SET set, set2;
      SSET sset, sset2;

      SET_make (out set, i);
      SSET_make (out sset, i);

      SET_make (out set2, j);
      SSET_make (out sset2, j);

      SET_union (ref set2, set);
      SSET_union (ref sset2, sset);

      SET_check_equal (set, sset, i, j, 0);
      SET_check_equal (set2, sset2, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
      SET_close (ref set2);
      SSET_close (ref sset2);
    }
  }

  //-------------------------------------------------------------

  printf ("N. SET_intersect\n");

  for (i=0; i<COUNT; i++)
  {
    SET set, set2;
    SSET sset, sset2;

    for (j=0; j<COUNT; j++)
    {
      SET_make (out set, i);
      SSET_make (out sset, i);

      SET_make (out set2, j);
      SSET_make (out sset2, j);

      SET_intersect (ref set2, set);
      SSET_intersect (ref sset2, sset);

      SET_check_equal (set, sset, i, j, 0);
      SET_check_equal (set2, sset2, i, j, 0);

      SET_close (ref set);
      SSET_close (ref sset);
      SET_close (ref set2);
      SSET_close (ref sset2);
    }
  }

  //-------------------------------------------------------------

  printf ("O. SET_copy\n");

  for (i=0; i<COUNT; i++)
  {
    SET set, set2;
    SSET sset, sset2;

    SET_make (out set, i);
    SSET_make (out sset, i);

    SET_copy (out set2, set);
    SSET_copy (out sset2, sset);
    
    SET_check_equal (set, sset, i, 0, 0);
    SET_check_equal (set2, sset2, i, 0, 0);

    SET_close (ref set);
    SSET_close (ref sset);
    SET_close (ref set2);
    SSET_close (ref sset2);
  }

  //-------------------------------------------------------------

  printf ("ok\n");
}


#endif

/************************************************************************/
