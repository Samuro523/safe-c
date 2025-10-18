
//----------------------------------------------------------------
// hash.h : hash table (key -> value).
//----------------------------------------------------------------

// default functions that can be used in case KEY is a packed type.
bool default_same_key (byte[] key1, byte[] key2);
int default_hash_of (byte[] key);                    // Jenkins's one-at-a-time hash

//----------------------------------------------------------------

generic <KEY, VALUE>
  bool same_key (KEY key1, KEY key2);  // see default above
  int hash_of (KEY key);               // see default above
package HashTable

  struct HASH_TABLE;

  // for maximum speed, set a maximum nb_slots and set auto_grow & auto_shrink to false.
  void create_hash_table (out HASH_TABLE hash_table, int nb_slots = 0, bool auto_grow = true, bool auto_shrink = true);
  void close_hash_table (ref HASH_TABLE hash_table);

  // clear table contents
  void clear_hash_table (ref HASH_TABLE hash_table);

  // insert (key, value)
  // error if key exists.
  void insert (KEY key, VALUE value, ref HASH_TABLE hash_table);

  // associates a new value to key
  // error if key does not exist.
  void update (KEY key, VALUE value, ref HASH_TABLE hash_table);

  // delete key, error if key does not exist.
  void delete (KEY key, ref HASH_TABLE hash_table);

  // returns true if key was found, or false if key does not exist.
  bool find (KEY key, out VALUE value, HASH_TABLE hash_table);

  generic <USER>
  package Loop
    typedef int OPERATE (KEY key, VALUE value, ref USER user);  // return 0 to continue loop, else abort loop with return value.
    int loop (OPERATE operate, ref USER user, HASH_TABLE hash_table);
  end Loop;

end HashTable;

//----------------------------------------------------------------
