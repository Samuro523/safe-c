
// libunit.c : stores a directory of all imported library unit

from std use arithm, bintree, files, strings, zip;
use lex, ../error, ../makelib0, glibpaths;

//=================================================================================

struct DATA
{
  string^   library;           // max 260 chars
  string^   source;            // max 260 chars
  NODE_INFO info;              // info where it can be found in zipped library (see makelib.h)
  string^   library_filename;  // max 260 chars
}

package P = new BALANCED_BINARY_TREE (ELEMENT => DATA, USER_INFO => bool);

BINARY_TREE tree;    // ordered by library + source file name

//=================================================================================

int sv_compare (bool^   user,
                DATA    data1,
                DATA    data2)
{
  _unused user;
  return 2 * strcmp (data1.library^, data2.library^)
           + strcmp (data1.source^, data2.source^);
}

//=================================================================================

public
void init_library_unit_tree ()
{
  create_btree (out tree, null, sv_compare);
}

//=================================================================================

string^ new_string (string str)
{
  return new string ' (str[0 : strlen(str)]);
}

//=================================================================================

bool library_unit_already_loaded (string library_unit)
{
  DATA d, d0;
  int  rc;

  clear d;
  d.library = new_string (library_unit);
  d.source = new string (0);

  d0 = d;

  rc = retrieve_btree (tree, ref d, BT_EQUAL_OR_LARGER);

  free d0.library;
  free d0.source;

  if (rc == BT_KEY_NOT_FOUND || strcmp (d.library^, library_unit) != 0)
    return false;

  return true;
}

//=================================================================================

void store_unit (string library, string source, NODE_INFO info, string library_filename)
{
  DATA u;
  int  rc;

  clear u;
  u.library = new_string (library);
  u.source  = new_string (source);
  u.info = info;
  u.library_filename = new_string (library_filename);

  rc = insert_btree (ref tree, u);
  if (rc < 0)
    fatal_compiler_error ("store_unit(1)", token.pos);
}

//=================================================================================

void load_library_unit (string library_unit)
{
  char       library_filename[260];
  char       msg[260+128];
  char       source[260];
  int        fd;
  HEADER     header;
  long       size;
  byte[]^    b;
  uint       idx;
  NODE_INFO  n;

  if (library_unit_already_loaded (library_unit))  // "std"
    return;

  // search for library unit in paths

  find_library (    library_unit,
                out library_filename);

  fd = open (library_filename);
  if (fd < 0)
  {
    sprintf (out msg, "cannot open library %s", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  if (read (fd, out header) != (int)header'size)
  {
    sprintf (out msg, "cannot read library %s", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  if (header.magic != MAGIC)
  {
    sprintf (out msg, "library %s has bad header", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  if (header.version != LIB_VERSION)
  {
    sprintf (out msg, "library %s belongs to another compiler version", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  size = lseek (fd, 0L, SEEK_END);
  if (size < 0)
  {
    sprintf (out msg, "cannot seek in library %s", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  if (lseek (fd, header.offset_dir, SEEK_SET) < 0)
  {
    sprintf (out msg, "cannot seek in library %s", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  if (header.offset_dir > (uint)size)
  {
    sprintf (out msg, "library %s has bad offset information", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  size -= header.offset_dir;

  b = new byte [(uint)size];

  if (read (fd, out b^) != (int)size)
  {
    free b;
    close (fd);
    sprintf (out msg, "cannot read library %s", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  close (fd);

  idx = 0;
  clear source;

  while (idx < (uint)size)
  {
    if (idx + NODE_INFO'size > (uint)size)
    {
      free b;
      close (fd);
      sprintf (out msg, "library %s is truncated", library_filename);
      fatal_compiler_error (msg, token.pos);
    }

    n'byte = b^[idx : n'size];
    idx += n'size;

    if (n.unit_name_length > 260 || idx + (uint)n.unit_name_length > size)
    {
      free (b);
      close (fd);
      sprintf (out msg, "library %s has bad data", library_filename);
      fatal_compiler_error (msg, token.pos);
    }

    source[0 : n.unit_name_length]'byte = b^[idx : n.unit_name_length];
    if (n.unit_name_length < source'length)
      source[n.unit_name_length] = nul;
    idx += (uint)n.unit_name_length;

    store_unit (library_unit, source, n, library_filename);
  }

  if (idx != (uint)size)
  {
    free (b);
    close (fd);
    sprintf (out msg, "library %s is truncated", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  free b;
  close (fd);
}

//=================================================================================

int retrieve_unit (    string    library,
                       string    source,
                   out NODE_INFO info,
                   out string    library_filename)
{
  DATA d, d0;
  int  rc;

  clear d;
  d.library = new_string (library);
  d.source  = new_string (source);

  d0 = d;

  rc = retrieve_btree (tree, ref d, BT_EQUAL);

  free d0.library;
  free d0.source;

  if (rc != 0 && rc != BT_KEY_NOT_FOUND)
    fatal_compiler_error ("retrieve_unit(1)", token.pos);

  if (rc == BT_KEY_NOT_FOUND)
  {
    clear info, library_filename;
    return -1;
  }

  info = d.info;
  strcpy (out library_filename, d.library_filename^);

  return 0;
}

//=================================================================================

package ZD

  struct ZIP_INFO
  {
    byte[]^ din;
    byte[]^ dout;
    int     in_index;
    int     out_index;
    int     in_size;
    int     out_size;
  }

end ZD;

package ZZIP = new UNPACK (USER_INFO => ZIP_INFO);

int my_read (ref ZIP_INFO u,
             out byte[]   buffer)
{
  int actual_size = min ((int)buffer'size, u.in_size - u.in_index);
#begin unsafe
  byte* p = &buffer;  // trick to avoid clearing buffer
  _unused p;
#end unsafe
  buffer[0 : actual_size] = u.din^[u.in_index:actual_size];
  u.in_index += actual_size;
  return actual_size;
}

int my_write (ref ZIP_INFO u,
                  byte[]   buffer)
{
  int actual_size = min ((int)buffer'size, u.out_size - u.out_index);
  u.dout^[u.out_index : actual_size] = buffer[0 : actual_size];
  u.out_index += actual_size;
  return actual_size;
}

//=================================================================================

// returns -1 if unit was not found in library

public
int load_library_source (    string  library,
                             string  source_name,
                         out byte[]^ source)
{
  NODE_INFO  info;
  char       library_filename[260];
  char       msg[128+260];
  int        fd;
  byte[]^    a, b;
  ZIP_INFO   z;
  READ_AHEAD extra;

  load_library_unit (library);

  if (retrieve_unit (library, source_name, out info, out library_filename) < 0)
  {
    clear source;
    return -1;   // source_name not found (note: can be optional)
  }

  if (stricmp (library, "std") == 0 && stricmp (source_name, "/console.h") == 0)
    is_console_app = true;    // set to true if console.h is loaded from std

  fd = open (library_filename);
  if (fd < 0)
  {
    sprintf (out msg, "cannot open library '%s'", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  if (lseek (fd, info.offset_in_file, SEEK_SET) < 0)
  {
    sprintf (out msg, "cannot seek in library '%s'", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  a = new byte [info.packed_size];
  b = new byte [info.unpacked_size];

  if (read (fd, out a^) != (int)info.packed_size)
  {
    sprintf (out msg, "cannot read library '%s'", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  close (fd);

  clear z;
  z.din = a;
  z.dout = b;
  z.in_index = 0;
  z.out_index = 0;
  z.in_size = (int)info.packed_size;
  z.out_size = (int)info.unpacked_size;

  if (unpack (ref z, my_read, my_write, out extra) < 0)
  {
    _unused extra;
    sprintf (out msg, "cannot retrieve data from library '%s'", library_filename);
    fatal_compiler_error (msg, token.pos);
  }

  free a;
  source = b;

  return 0;
}

//=================================================================================
