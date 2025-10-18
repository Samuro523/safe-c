//----------------------------------------------------------------
// hash.c : hash table (key -> value).
//----------------------------------------------------------------

use strings, thread;

#define debug 0

#if debug
  from std use console, tracing, files, exception;
#endif

//--------------------------------------------------------------------------------

public bool default_same_key (byte[] key1, byte[] key2)
{
  return memcmp (key1, key2) == 0;
}

//--------------------------------------------------------------------------------

// Jenkins's one-at-a-time hash

public int default_hash_of (byte[] key)
{
  int  i;
  uint hash = 0;

  for (i=0; i<key'length; i++)
  {
    hash += key[i];
    hash += hash << 10;
    hash ^= hash >> 6;
  }

  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;

  return (int)hash;
}

//--------------------------------------------------------------------------------

package body HashTable

  //--------------------------------------------------------------------------------

  struct BUCKET
  {
    int first;  // index into entry, -1 = unused
  }

  //--------------------------------------------------------------------------------

  struct ENTRY
  {
    bool  entry_used;
    KEY   key;
    VALUE value;
    int   next;           // -1 = none
  }

  //--------------------------------------------------------------------------------

  struct HASH_TABLE
  {
    BUCKET[]^ buckets;
    ENTRY[]^  entries;
    int       count;        // nb of entries used
    int       freel;        // first free entry, -1 = none
    int       total_used_entries;
    bool      auto_grow;
    bool      auto_shrink;
  }

  //--------------------------------------------------------------------------------

  public void create_hash_table (out HASH_TABLE hash_table, int nb_slots = 0, bool auto_grow = true, bool auto_shrink = true)
  {
    int i, length = 0;

    for (i=0; i<28; i++)
    {
      length = 1 << i;
      if (length >= nb_slots)
        break;
    }

    hash_table = {buckets            => new BUCKET[length] ' {all => {first => -1}},
                  entries            => new ENTRY[length],
                  count              => 0,
                  freel              => -1,
                  total_used_entries => 0,
                  auto_grow          => auto_grow,
                  auto_shrink        => auto_shrink};
    
    thread.flush_cache ();
  }

  //--------------------------------------------------------------------------------

  public void close_hash_table (ref HASH_TABLE hash_table)
  {
    free hash_table.buckets;
    free hash_table.entries;
    clear hash_table;

    thread.flush_cache ();
  }

  //--------------------------------------------------------------------------------

  public void clear_hash_table (ref HASH_TABLE hash_table)
  {
    thread.fetch_cache ();

    if (hash_table.auto_shrink)
    {
      bool old_auto_grow = hash_table.auto_grow;
      close_hash_table (ref hash_table);
      create_hash_table (out hash_table, nb_slots => 0, auto_grow => old_auto_grow, auto_shrink => true);
    }
    else   // keep size
    {
      hash_table.buckets^ = {all => {first => -1}};
      hash_table.count = 0;
      hash_table.freel = -1;
      hash_table.total_used_entries = 0;
    }

    thread.flush_cache ();
  }

  //--------------------------------------------------------------------------------

  int allocate_entry (KEY key, VALUE value, int next, ref HASH_TABLE hash_table, out bool overflow)
  {
    int e;

    if (hash_table.freel != -1)
    {
      e = hash_table.freel;
      hash_table.freel = hash_table.entries^[e].next;
      overflow = false;
    }
    else
    {
      e = hash_table.count++;
      overflow = (hash_table.count >= 3*(hash_table.entries^'length >> 2));   // 75% full
    }

    hash_table.total_used_entries++;

    hash_table.entries^[e] = {entry_used => true, key => key, value => value, next => next};
    return e;
  }

  //--------------------------------------------------------------------------------

  void dispose_entry (int e, ref HASH_TABLE hash_table, out bool underflow)
  {
    if (e == hash_table.count - 1)
    {
      hash_table.count--;
    }
    else
    {
      hash_table.entries^[e].entry_used = false;  // indicates unused entry in case of reorganization
      hash_table.entries^[e].next = hash_table.freel;
      hash_table.freel = e;
    }

    hash_table.total_used_entries--;

    underflow = (hash_table.total_used_entries < (hash_table.entries^'length >> 2));  // less than 25% full
  }

  //--------------------------------------------------------------------------------

  void resize (ref HASH_TABLE hash_table, int delta)
  {
    HASH_TABLE old_hash_table = hash_table;
    int        new_size;
    int        e;

    new_size = old_hash_table.buckets^'length;
    if (delta > 0)
      new_size <<= 1;
    else
      new_size >>= 1;

    create_hash_table (out hash_table, nb_slots => new_size, auto_grow => false, auto_shrink => false);

    for (e=0; e<old_hash_table.count; e++)   // loop on all entries
    {
      ref ENTRY entry = old_hash_table.entries^[e];

      if (entry.entry_used)
        insert (entry.key, entry.value, ref hash_table);
    }

    hash_table.auto_grow   = old_hash_table.auto_grow;
    hash_table.auto_shrink = old_hash_table.auto_shrink;

    close_hash_table (ref old_hash_table);
  }

  //--------------------------------------------------------------------------------

  int hash_of_key (KEY key, HASH_TABLE hash_table)
  {
    return hash_of(key) & (hash_table.buckets^'length - 1);
  }

  //--------------------------------------------------------------------------------

  // insert (key, value)
  // error if key already exists.

  public void insert (KEY key, VALUE value, ref HASH_TABLE hash_table)
  {
    int  idx, first;
    bool overflow;

    thread.fetch_cache ();

    idx = hash_of_key (key, hash_table);

    first = hash_table.buckets^[idx].first;

    hash_table.buckets^[idx].first = allocate_entry (key => key, value => value, next => first, ref hash_table, out overflow);

    // check if key is unique
    while (first != -1)
    {
      assert !same_key (key, hash_table.entries^[first].key);   // error if key already exists
      first = hash_table.entries^[first].next;
    }

    if (overflow && hash_table.auto_grow)
    {
      resize (ref hash_table, +1);
    }

    thread.flush_cache ();
  }

  //--------------------------------------------------------------------------------

  // associates a new value to key
  // error if key does not exist

  public void update (KEY key, VALUE value, ref HASH_TABLE hash_table)
  {
    int idx, first;

    thread.fetch_cache ();

    idx = hash_of_key (key, hash_table);

    first = hash_table.buckets^[idx].first;

    while (!same_key (key, hash_table.entries^[first].key))   // array index error if key not found
      first = hash_table.entries^[first].next;

    hash_table.entries^[first].value = value;

    thread.flush_cache ();
  }

  //--------------------------------------------------------------------------------

  // delete key, error if key does not exist

  public void delete (KEY key, ref HASH_TABLE hash_table)
  {
    int  idx, previous, first;
    bool underflow;

    thread.fetch_cache ();

    idx = hash_of_key (key, hash_table);

    first = hash_table.buckets^[idx].first;
    previous = -1;

    while (!same_key (key, hash_table.entries^[first].key))   // array index error if key not found
    {
      previous = first;
      first = hash_table.entries^[first].next;
    }

    if (previous == -1)  // remove head entry
    {
      hash_table.buckets^[idx].first = hash_table.entries^[first].next;
    }
    else
    {
      hash_table.entries^[previous].next = hash_table.entries^[first].next;
    }

    dispose_entry (first, ref hash_table, out underflow);

    if (underflow && hash_table.auto_shrink)
    {
      resize (ref hash_table, -1);
    }

    thread.flush_cache ();
  }

  //--------------------------------------------------------------------------------

  // returns true if key was found, or false if key does not exist.

  public bool find (KEY key, out VALUE value, HASH_TABLE hash_table)
  {
    int idx, first;

    thread.fetch_cache ();

    idx = hash_of_key (key, hash_table);

    first = hash_table.buckets^[idx].first;

    while (first != -1)
    {
      ref ENTRY e = hash_table.entries^[first];
      if (same_key (key, e.key))     // found
      {
        value = e.value;
        return true;
      }
      first = e.next;
    }

    clear value;
    return false;
  }

  //--------------------------------------------------------------------------------

  package body Loop

    //------------------------------------------------------------------------------

    public int loop (OPERATE operate, ref USER user, HASH_TABLE hash_table)
    {
      int e;
      
      thread.fetch_cache ();

      for (e=0; e<hash_table.count; e++)   // loop on all entries
      {
        ref ENTRY entry = hash_table.entries^[e];
        if (entry.entry_used)
        {
          int rc = operate (entry.key, entry.value, ref user);
          if (rc != 0)
            return rc;
        }
      }
      return 0;
    }

    //------------------------------------------------------------------------------

  end Loop;

  //--------------------------------------------------------------------------------

end HashTable;

//--------------------------------------------------------------------------------
