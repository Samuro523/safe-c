
// unit.c : unit analysis

from std use bintree, console, files, strings, thread;
use ../error, ../dbginfo, ../common;
use tokens, source, lex, entities, decl, typedecl, complete, libunit, wstrings;

//=================================================================================

struct PENDING_BODY
{
  string^       library;    // max 256 chars
  string^       source;
  bool          must_exist;
  PENDING_BODY^ next;
}

PENDING_BODY^ g_pending_bodies;

//=================================================================================

struct UNIT_DATA
{
  string^ library;
  string^ source;
  PENTITY unit;     // not filled for g_unit_tree2
  int     key;      // unique unit key (0=none) (only .c unit receive a key)
}

package P = new BALANCED_BINARY_TREE (ELEMENT => UNIT_DATA, USER_INFO => bool);

BINARY_TREE g_unit_tree;    // ordered by library + source file name
BINARY_TREE g_unit_tree2;   // ordered by key

//=================================================================================

int g_file_unit_unique_key;      // source .c units receive positive values
int g_library_unit_unique_key;   // library .c units receive negative values

//=================================================================================

PENTITY DUMMY_UNIT;   // special unique value, not zero

//=================================================================================

int sv_compare (bool^     user,
                UNIT_DATA data1,
                UNIT_DATA data2)
{
  _unused user;
  return 2 * strcmp (data1.library^, data2.library^)
           + strcmp (data1.source^, data2.source^);
}

//=================================================================================

int sv_compare2 (bool^     user,
                 UNIT_DATA data1,
                 UNIT_DATA data2)
{
  _unused user;
  if (data1.key < data2.key)
    return -1;
  if (data1.key > data2.key)
    return +1;
  return 0;
}

//=================================================================================

public
void init_unit_tree ()
{
  create_btree (out g_unit_tree,  null, sv_compare);
  create_btree (out g_unit_tree2, null, sv_compare2);

#begin unsafe
  clear DUMMY_UNIT;
  *((byte*)(&DUMMY_UNIT)) = 1;  // special unique value, not zero
#end unsafe
}

//=================================================================================

string^ new_string (string str)
{
  return new string ' (str[0 : strlen(str)]);
}

//=================================================================================

// returns unit's unique key

int store_empty_unit (string library, string source, bool is_body)
{
  UNIT_DATA u;
  int       rc;

  clear u;
  u.library = new_string (library);
  u.source  = new_string (source);
//  u.unit    = null;

  if (is_body)
  {
    if (library'length == 0 || library[0] == nul)
      u.key = ++g_file_unit_unique_key;
    else
      u.key = --g_library_unit_unique_key;
  }
  else
  {
    u.key = 0;
  }

  rc = insert_btree (ref g_unit_tree, u);
  if (rc < 0)
    fatal_compiler_error ("store_empty_unit(1)", token.pos);

  if (u.key != 0)
  {
    rc = insert_btree (ref g_unit_tree2, u);
    if (rc < 0)
      fatal_compiler_error ("store_empty_unit(2)", token.pos);
  }

  return u.key;
}

//=================================================================================

void update_unit (string library, string source, PENTITY e)
{
  UNIT_DATA u, v;
  int       rc;

  clear u;
  u.library = new_string (library);
  u.source  = new_string (source);
  u.unit    = e;

  v = u;
  rc = retrieve_btree (g_unit_tree, ref v, BT_EQUAL);
  if (rc < 0)
    fatal_compiler_error ("update_unit(1)", token.pos);

  free u.library;
  free u.source;

  v.unit = e;

  rc = update_btree (ref g_unit_tree, v);
  if (rc < 0)
    fatal_compiler_error ("update_unit(2)", token.pos);
}

//=================================================================================

bool unit_exists (string library, string source)
{
  UNIT_DATA u, v;
  int       rc;

  clear u;
  u.library = new_string (library);
  u.source  = new_string (source);

  v = u;
  rc = retrieve_btree (g_unit_tree, ref v, BT_EQUAL);

  free u.library;
  free u.source;

  if (rc == BT_KEY_NOT_FOUND)
    return false;

  if (rc < 0)
    fatal_compiler_error ("unit_exists", token.pos);

  return true;
}

//=================================================================================

PENTITY retrieve_unit (string library, string source)
{
  UNIT_DATA u, v;
  int       rc;

  clear u;
  u.library = new_string (library);
  u.source  = new_string (source);

  v = u;

  rc = retrieve_btree (g_unit_tree, ref v, BT_EQUAL);

  free u.library;
  free u.source;

  if (rc < 0)
    fatal_compiler_error ("retrieve_unit", token.pos);

  return v.unit;
}

//=================================================================================

int unit_operate (bool^     user,
                  UNIT_DATA p)
{
  _unused user;
  dbg_store_unit_name (p.key, p.library^, p.source^);
  return 0;
}

//=================================================================================

public
void list_units_for_debug ()
{
  traverse_btree (g_unit_tree2, unit_operate, +1);
}

//=================================================================================

public
void retrieve_library_and_source_names (int key, out string library, out string source)
{
  UNIT_DATA u;
  int       rc;

  clear u;
  u.key = key;

  rc = retrieve_btree (g_unit_tree2, ref u, BT_EQUAL);
  if (rc < 0)
  {
    fatal_compiler_error ("retrieve_library_and_source_names", token.pos);
    clear library, source;
  }

  strcpy (out library, u.library^);
  strcpy (out source, u.source^);
}

//=================================================================================

int compute_absolute_filename (    string dir,
                                   string source,
                               out string result)
{
  int d, i, j;

  // dir is a real filename ending with / or \
  // source has the form  {../} {id/} id

  // important thing is : don't cut dir too much, the device
  //   part must not be changed, so first we need to compute
  //   the device part length.

  // compute d so that it points to the first / not to be deleted

  if (dir'length >= 2 && dir[0] != '\0' && dir[1] == ':')     // drive letter
  {
    d = 2;   // points at first '/'
  }
  else if (dir'length >= 2 && dir[0] == '\\' && dir[1] == '\\')  // computer name
  {
    d = 2;
    while (d < dir'length && dir[d] != '\0' && dir[d] != '\\' && dir[d] != '/')
      d++;
  }
  else
  {
    d = 0;
  }

  i = strlen(dir) - 1;   // end of dir
  j = 0;                 // start of source

  while (j+3 <= source'length && memcmp (source[j:3], "../") == 0)
  {
    j += 3;

    for (;;)
    {
      i--;

      if (i < d)
      {
        clear result;
        return -1;
      }

      if (dir[i] == '/' || dir[i] == '\\')
        break;
    }
  }

  // source[j] starts with name

  if ((i+1) + strlen(source[j:source'length-j]) + 1 > result'length)  // too long
  {
    clear result;
    return -1;
  }

  sprintf (out result, "%s%s", dir[0:i+1], source[j:source'length-j]);

  return 0;
}

//=================================================================================

package IMPORTING

  struct IMPORT_DATA
  {
    string^         library;   // max 260 chars
    string^         source;    // max 260 chars
    string^         alias;     // max 128 chars (MAX_IDENTIFIER_LENGTH)
    TEXT_POSITION   lib_pos, src_pos, alia_pos;
    IMPORT_DATA^    next;
  }

  struct IMPORT_QUEUE
  {
    IMPORT_DATA^ head;
    IMPORT_DATA^ tail;
  }

end IMPORTING;

//=================================================================================

void append_import_data (ref IMPORT_QUEUE  queue,
                             string        library,
                             string        source,
                             string        alias,
                             TEXT_POSITION lib_pos,
                             TEXT_POSITION src_pos,
                             TEXT_POSITION alia_pos)
{
  IMPORT_DATA^ p = new IMPORT_DATA;

  p^.library = new_string (library);
  p^.source = new_string (source);
  p^.alias = new_string (alias);

  p^.lib_pos  = lib_pos;
  p^.src_pos  = src_pos;
  p^.alia_pos = alia_pos;

  p^.next = null;

  if (queue.head == null)
    queue.head = p;
  else
    queue.tail^.next = p;
  queue.tail = p;
}

//=================================================================================

void free_import_list (ref IMPORT_QUEUE q)
{
  IMPORT_DATA^ p, t;

  p = q.head;

  while (p != null)
  {
    t = p;
    p = p^.next;

    free t^.library;
    free t^.source;
    free t^.alias;
    free t;
  }
  q.head = null;
  q.tail = null;
}

//=================================================================================

void parse_import_clause (    string        parent_library,
                              string        parent_dir,
                          ref IMPORT_QUEUE  queue)
{
  char[MAX_IDENTIFIER_LENGTH] id, alias_name;
  char[260]                   library_name, source_name, full_filename;
  TEXT_POSITION               lib_pos, src_pos, alia_pos;

  lib_pos = token.pos;

  if (token.kind == TOKEN_from)
  {
    get_token ();

    lib_pos = token.pos;

    if (token.kind != IDENTIFIER)
    {
      syntax_error ("library identifier expected", token.pos);
      skip_until (SEMICOLON, true);
      return;
    }

    if (!is_all_in_lowercase (token.info._identifier.value))
      syntax_error ("library identifier must be in lower case", token.pos);

    wstring_to_string (token.info._identifier.value, out id);
    strcpy (out library_name, id);

    get_token ();


    while (token.kind == DOT)
    {
      get_token ();

      if (token.kind != IDENTIFIER)
      {
        syntax_error ("library identifier expected", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      if (!is_all_in_lowercase (token.info._identifier.value))
        syntax_error ("library identifier must be in lower case", token.pos);

      wstring_to_string (token.info._identifier.value, out id);

      if (strlen(library_name) + 1 + strlen(id) > library_name'length)
      {
        syntax_error ("library name is too long", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      strcat (ref library_name, ".");
      strcat (ref library_name, id);

      get_token ();
    }
  }
  else
  {
    clear library_name;
  }


  if (token.kind != TOKEN_use)
  {
    syntax_error ("keyword 'use' expected here", token.pos);
    skip_until (SEMICOLON, true);
    return;
  }

  get_token ();   // skip 'use'

  for (;;)
  {
    src_pos = token.pos;

    clear source_name;

    while (token.kind == DOUBLE_DOT)
    {
      if (library_name[0] != nul)
        semantic_error ("'..' is not allowed for source names of library units", token.pos);

      get_token ();   // skip '..'

      if (token.kind != SLASH)
      {
        syntax_error ("'/' expected here", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      get_token ();   // skip /


      if (strlen(source_name) + 3 > source_name'length)
      {
        syntax_error ("source name is too long", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      strcat (ref source_name, "../");
    }


    for (;;)
    {
      if (token.kind != IDENTIFIER)
      {
        syntax_error ("source identifier expected", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      if (!is_all_in_lowercase (token.info._identifier.value))
        syntax_error ("source identifier must be in lower case", token.pos);

      wstring_to_string (token.info._identifier.value, out id);

      if (strlen(source_name) + strlen(id) > source_name'length)
      {
        syntax_error ("source name is too long", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      strcat (ref source_name, id);

      get_token ();   // skip id


      if (token.kind != SLASH)
        break;

      get_token ();   // skip '/'

      if (strlen(source_name) + 1 > source_name'length)
      {
        syntax_error ("source name is too long", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      strcat (ref source_name, "/");
    }


    alia_pos = src_pos;    // default pos is source name

    if (token.kind == IDENTIFIER && wstrcmp (token.info._identifier.value, L"as") == 0)
    {
      get_token ();   // skip 'as'

      alia_pos = token.pos;

      if (token.kind != IDENTIFIER)
      {
        syntax_error ("alias identifier expected", token.pos);
        skip_until (SEMICOLON, true);
        return;
      }

      wstring_to_string (token.info._identifier.value, out alias_name);

      get_token ();   // skip id
    }
    else   // no alias name specified : extract alias from source_name's last component
    {
      int i;

      i = strlen(source_name);
      while (i-1 >= 0 && source_name[i-1] != '/')
        i--;

      strcpy (out alias_name, source_name[i:strlen(source_name) - i]);
    }


    if (library_name[0] == '\0')    // use parent's library/disk
    {
      clear full_filename;
      if (compute_absolute_filename (parent_dir, source_name, out full_filename[0 : full_filename'length-6]) < 0)
      {
        syntax_error ("illegal filename", src_pos);
      }
      else
      {
        strcat (ref full_filename, ".h");

        append_import_data (ref queue, parent_library, full_filename, alias_name,
                            lib_pos, src_pos, alia_pos);
      }
    }
    else    // absolute source filename from another library than parent
    {
      sprintf (out full_filename, "/%s%s", source_name, ".h");

      append_import_data (ref queue, library_name, full_filename, alias_name,
                          lib_pos, src_pos, alia_pos);
    }

    if (token.kind != COMMA)
      break;

    get_token ();   // skip comma
  }

  parse_semicolon ();
}

//=================================================================================

// returns the created unit entity

PENTITY parse_unit (string library,
                    string source,     // ends with ".h" or ".c"
                    bool   is_body)
{
  PENTITY  e;
  wstring^ punit_name;

  {
    int len = strlen(source) - 2;  // remove extension
    int i   = len;

    while (i-1 >= 0 && source[i-1] != '/' && source[i-1] != '\\')  // find char after last / or \
      i--;

    len = len - i;    // just keep last name, no extension
    punit_name = new wstring (len);
    string_to_wstring (source[i:len], out punit_name^);
  }


  if (is_body)      // body unit (.c)
  {
    PENTITY             edecl, ebody;
    ENTITY(A_UNIT_BODY) ubody;

    edecl = search_entity_in_local_scope (punit_name^);

    if (edecl == null)  // no earlier unit interface of this name declared
    {
      ENTITY (A_UNIT_INTERFACE) interf;

      clear interf;
      interf.the_unit_interface.to_unit_body     = null;
      interf.the_unit_interface.is_body_required = false;
      interf.the_unit_interface.unit_is_used     = false;
      interf.the_unit_interface.declarations     = new_region ();

      edecl = append_new_entity (interf, punit_name^, token.pos);

      {
        int     len = strlen(source);
        string^ psource2 = new string (len);
        sprintf (out psource2^, "%s%s", source[0 : len-2], ".h");
        update_unit (library, psource2^, edecl);
        free psource2;
      }
    }

    clear ubody;
    ubody.the_unit_body.to_unit_interface = edecl;
    ubody.the_unit_body.declarations = new_region ();

    ebody = append_new_entity (ubody, L"", token.pos);
    update_unit (library, source, ebody);

    edecl^.the_unit_interface.to_unit_body = ebody;

    create_region_level ();
    append_region (edecl^.the_unit_interface.declarations);
    append_region (ubody.the_unit_body.declarations);


    // when entering region (package body. or unit body),
    // scan package decl. or unit interface :
    // - for opaque type :
    //   . set non-limited

    e = edecl^.the_unit_interface.declarations^.entities.first;
    while (e != null)
    {
      if (e^.kind == AN_OPAQUE_TYPE)
      {
        e^.the_opaque_type.is_limited = false;
      }

      e = e^.next;
    }



    parse_global_declarations (true);



    check_global_completion (edecl^.the_unit_interface.declarations^.entities.first);
    check_global_completion (ebody^.the_unit_body.declarations^.entities.first);



    // when leaving region (package body. or unit body),
    // scan package decl. or unit interface :
    // - for incomplete type :
    //   . if full type's keynr is past the package body nr,
    //     it means they are in different parts ! -> full type no longer available.
    // - for opaque type :
    //   . full type no longer available.

    e = edecl^.the_unit_interface.declarations^.entities.first;
    while (e != null)
    {
      if (e^.kind == AN_INCOMPLETE_TYPE &&
          e^.the_incomplete_type.full_type != null &&
          e^.the_incomplete_type.full_type^.nr > ebody^.nr)
      {
        e^.the_incomplete_type.is_full_type_visible = false;
      }

      if (e^.kind == AN_OPAQUE_TYPE)
      {
        e^.the_opaque_type.is_full_type_visible = false;
        e^.the_opaque_type.is_limited = true;
      }

      e = e^.next;
    }

    e = ebody;  // created entity
  }
  else    // interface unit (.h)
  {
    ENTITY (A_UNIT_INTERFACE) interf;
    PENTITY                   edecl;

    clear interf;
    interf.the_unit_interface.to_unit_body     = null;
    interf.the_unit_interface.is_body_required = false;
    interf.the_unit_interface.unit_is_used     = false;
    interf.the_unit_interface.declarations     = new_region ();

    edecl = append_new_entity (interf, punit_name^, token.pos);
    update_unit (library, source, edecl);

    create_region_level ();
    append_region (interf.the_unit_interface.declarations);


    parse_global_declarations (false);


    // make opaque types limited

    e = edecl^.the_unit_interface.declarations^.entities.first;
    while (e != null)
    {
      if (e^.kind == AN_OPAQUE_TYPE)
      {
        e^.the_opaque_type.is_limited = true;
      }

      e = e^.next;
    }


    // tests if a unit body is required

    edecl^.the_unit_interface.is_body_required =
         is_body_required (edecl^.the_unit_interface.declarations^.entities.first);


    e = edecl;   // created entity
  }


  if (token.kind != LAST_TOKEN)
    syntax_error ("declaration expected", token.pos);

  unlink_region_level ();

  cleanup_main_region_after_compilation ();

  free punit_name;

  return e;
}

//=================================================================================

void intern_append_new_unit_entity_for_import
                      (PENTITY       e,
                       string        filename,
                       TEXT_POSITION pos,
                       bool          add_in_package_list)
{
  int      len = strlen(filename);
  wstring^ pwfilename = new wstring (len);

  string_to_wstring (filename, out pwfilename^);

  append_new_unit_entity_for_import (e, pwfilename^, pos, add_in_package_list);

  free pwfilename;
}

//=================================================================================

void append_to_pending_list (string library, string filename, bool must_exist)
{
  PENDING_BODY^ p = new PENDING_BODY;

  p^.library = new_string (library);
  p^.source = new_string (filename);
  p^.must_exist = must_exist;
  p^.next = g_pending_bodies;

  g_pending_bodies = p;
}

//=================================================================================

// returns 0 if OK (file was loaded)
// returns +1 if NOK (file not found),
//   then ret is initialized : 0 if default unit was added, +1 if mandatory and was missing.

int load_in_lex (    string   library,    // "", "std", "webc.org"
                     string   filename,
                     bool     must_exist,
                 out wchar[]^ utf16_source_text,
                 out int      ret)        // if returning 1 : ret => 0 = done, +1 = file not found
{
  byte[]^  source;
  wchar[]^ source_text;   // Lnul-terminated
  int      rc;

  utf16_source_text = null;

  if (library'length > 0 && library[0] != '\0')  // load file from library
  {
    if (load_library_source (library, filename, out source) < 0)  // was not found
    {
      if (!must_exist)
      {
        // store a special entity signaling a non-existing optional unit
        update_unit (library, filename, DUMMY_UNIT);
        ret = 0;     // done
        return +1;
      }

      // was mandatory
      ret = +1;    // returns code for file not found
      return +1;
    }
  }
  else      // load from disk
  {
    if (!must_exist)
    {
      if (!exists (filename))      // file does not exist
      {
        // store a special entity signaling a non-existing optional unit
        update_unit (library, filename, DUMMY_UNIT);
        ret = 0;     // done
        return +1;
      }
    }

    rc = load_source_file (filename, out source);
    if (rc < 0)
    {
      // was mandatory
      ret = +1;    // returns code for file not found
      return +1;
    }
  }


  rc = convert_source_file_to_utf16 (source^, out source_text);
  if (rc < 0)
  {
    printf ("fatal error: converting '%s' to utf-16 failed\n", filename);
    exit (-1);
  }

  free source;

  lex_start_new_context (source_text);
  get_token ();

  utf16_source_text = source_text;

  ret = 0;
  return 0;
}

//=================================================================================

int compile_unit_interface (string library,      // "", or "/lib/xxx"
                            string filename,     // ends with ".h"
                            bool   must_exist);

//=================================================================================

void import_and_compile_all_libraries (    string       library,      // "", "std", "webc.org"
                                           string       filename,     // ends with ".h" or ".c"
                                           int          key,
                                       out IMPORT_QUEUE pqueue)
{
  IMPORT_DATA^ l;


  // compute unit's directory and name

  {
    int len = strlen(filename) - 2;   // remove extension
    int i;

    if (len < 1)         // at least "p"
    {
      printf ("intern error unit_name\n");
      printf ("stopping compilation\n");
      exit (-1);
    }

    i = len;
    while (i-1 >= 0 && filename[i-1] != '/' && filename[i-1] != '\\')  // find char after last / or \
      i--;

    {
      ref string unit_name = filename[i : len - i];   // no extension, just keep last name
      ref string unit_dir  = filename[0 : i];         // "/dirname/"


      // collect all import libraries

      set_current_error_source_file (library, filename, key);

      clear pqueue;
      while (token.kind == TOKEN_from || token.kind == TOKEN_use)
        parse_import_clause (library, unit_dir, ref pqueue);


      // recursively compile all dependant unit interfaces

      l = pqueue.head;
      while (l != null)
      {
        if (strcmp (unit_name, l^.alias^) == 0)
        {
          // on heap to avoid overloading the stack
          string^ msg = new string (128 + l^.alias^'length + filename'length + 2*l^.alias^'length + library'length);
          set_current_error_source_file (library, filename, key);  // set again this file
          sprintf (out msg^, "name collision importing unit '%s' into file '%s' - change filename or try \"use %s as %s2\"",
                   l^.alias^, filename, l^.alias^, l^.alias^);
          if (library'length > 0 && library[0] != nul)
            strcatf (ref msg^, " [%s]", library);
          semantic_error (msg^, l^.alia_pos);
          printf ("stopping compilation\n");
          exit (-1);
        }

        if (compile_unit_interface (l^.library^, l^.source^, must_exist => true) != 0)
        {
          // on heap to avoid overloading the stack
          string^ msg = new string (128 + l^.source^'length + l^.library^'length);
          set_current_error_source_file (library, filename, key);  // set again this file
          sprintf (out msg^, "cannot open file %s", l^.source^);
          if (l^.library^'length > 0 && l^.library^[0] != nul)
            strcatf (ref msg^, " [%s]", l^.library^);
          semantic_error (msg^, l^.src_pos);
          printf ("stopping compilation\n");
          exit (-1);
        }
        l = l^.next;
      }
    }
  }
}

//=================================================================================

void generate_warnings_for_unused_units (ref IMPORT_QUEUE queue)
{
  IMPORT_DATA^ l;
  char         msg[120];
  PENTITY      e;
  int          len, idx;

  if (nb_of_compilation_errors() > 0)
    return;

  l = queue.head;
  while (l != null)
  {
    e = retrieve_unit (l^.library^, l^.source^);
    if (e != null && e != DUMMY_UNIT && !e^.the_unit_interface.unit_is_used)
    {
      idx = strrchr (l^.source^, '/');   // index of last slash character
      if (idx < 0) // not found
        idx = 0;     // take all string
      else
        idx++;       // name after slash

      len = strlen(l^.source^) - idx - 2;   // no .c or .h extension
      if (len > 90)
        len = 90;

      sprintf (out msg, "unit '%s' is not used", l^.source^[idx : len]);

      warning (msg, l^.src_pos);
    }
    l = l^.next;
  }
}

//=================================================================================

// returns 0 if OK, +1 if file not found

int compile_unit_interface (string library,      // "", "std", "webc.org"
                            string filename,     // ends with ".h"
                            bool   must_exist)
{
  int          key, rc;
  wchar[]^     utf16_source_text;
  IMPORT_QUEUE queue;
  IMPORT_DATA^ l;
  PENTITY      e, edecl;

  // check if this unit already exists in library, if yes -> done.

  if (unit_exists (library, filename))
    return 0;

  // store a first entry with null entity to avoid infinite recursion
  key = store_empty_unit (library, filename, is_body => false);

  set_current_error_source_file (library, filename, key);


  if (load_in_lex (library, filename, must_exist, out utf16_source_text, out rc) != 0)
    return rc;  // 0 or 1


  import_and_compile_all_libraries (library, filename, key, out queue);


  set_current_error_source_file (library, filename, key);  // set again this file



  // prepare scope with all imported libraries being directly visible
  // using aliases if any;
  // if units were imported twice, make them visible only once.

  l = queue.head;
  while (l != null)
  {
    e = retrieve_unit (l^.library^, l^.source^);
    if (e == null)
    {
      semantic_error ("imported unit has circular dependencies", l^.src_pos);
    }
    else if (e == DUMMY_UNIT)   // unit imported non-existing .h of main program
    {
      semantic_error ("unit interface file does not exist", l^.src_pos);
    }
    else
    {
      e^.the_unit_interface.unit_is_used = false;

      intern_append_new_unit_entity_for_import
                      (e,
                       l^.alias^,
                       l^.alia_pos,
                       add_in_package_list => true);
    }

    l = l^.next;
  }

  edecl = parse_unit (library, filename, is_body => false);


  generate_warnings_for_unused_units (ref queue);

  free_import_list (ref queue);


  lex_close_context ();
  free utf16_source_text;



  // put the corresponding .c body on a pending list

  {
    int     len = strlen(filename);
    string^ import_filename = new string (len);   // on heap to avoid overloading the stack

    sprintf (out import_filename^, "%s%s", filename[0 : len-2], ".c");
    append_to_pending_list (library, import_filename^, edecl^.the_unit_interface.is_body_required);

    free import_filename;
  }

  return 0;
}

//=================================================================================

// returns 0 if OK, +1 if file not found

int compile_unit_body (string library,      // "", "std", "webc.org"
                       string filename,     // ends with ".c"
                       bool   must_exist)
{
  int          key, rc;
  wchar[]^     utf16_source_text;
  IMPORT_QUEUE queue;
  IMPORT_DATA^ l;
  PENTITY      e;


  // check if this unit already exists in library, if yes -> done.

  if (unit_exists (library, filename))    // could occur for main.c if there's a main.h
    return 0;

  // store a first entry with null entity to avoid infinite recursion
  key = store_empty_unit (library, filename, is_body => true);

  set_current_error_source_file (library, filename, key);

  if (load_in_lex (library, filename, must_exist, out utf16_source_text, out rc) != 0)
    return rc;  // 0 or 1

  import_and_compile_all_libraries (library, filename, key, out queue);


  set_current_error_source_file (library, filename, key);  // set again this file


  // prepare the corresponding .h unit in scope

  {
    int     len  = strlen(filename);
    string^ pimport_filename = new string (len);  // on heap to avoid overloading the stack

    sprintf (out pimport_filename^, "%s%s", filename[0 : len-2], ".h");

    e = retrieve_unit (library, pimport_filename^);

    free pimport_filename;

    if (e == null)
    {
      semantic_error ("interface unit of this file has circular dependencies", token.pos);
    }
    else if (e == DUMMY_UNIT)
    {
      // no .h for main .c
    }
    else
    {
      int i = len - 2;
      
      while (i-1 >= 0 && filename[i-1] != '/' && filename[i-1] != '\\')
        i--;
      len = len - i - 2;

      intern_append_new_unit_entity_for_import
                      (e                   => e,
                       filename            => filename[i:len],    // short name
                       pos                 => token.pos,
                       add_in_package_list => false);
    }
  }

  // prepare scope with all imported libraries being directly visible
  // using aliases if any;
  // if units were imported twice, make them visible only once.

  l = queue.head;
  while (l != null)
  {
    e = retrieve_unit (l^.library^, l^.source^);
    if (e == null)
    {
      semantic_error ("imported unit has circular dependencies", l^.src_pos);
    }
    else if (e == DUMMY_UNIT)   // unit imported non-existing .h of main program
    {
      semantic_error ("unit interface file does not exist", l^.src_pos);
    }
    else
    {
      e^.the_unit_interface.unit_is_used = false;

      intern_append_new_unit_entity_for_import
                      (e,
                       l^.alias^,
                       l^.alia_pos,
                       add_in_package_list => true);
    }

    l = l^.next;
  }

  parse_unit (library, filename, is_body => true);

  generate_warnings_for_unused_units (ref queue);

  free_import_list (ref queue);


  lex_close_context ();
  free utf16_source_text;

  return 0;
}

//=================================================================================

// compile main .c
// returns 0 if OK, +1 if file not found

public
int compile_main_unit (string filename)    // ends with ".c"
{
  int rc;

  {
    int     len = strlen(filename);
    string^ import_filename = new string (len);   // on heap to avoid overloading the stack

    sprintf (out import_filename^, "%s%s", filename[0 : len-2], ".h");    // ends with ".h"

    (void)compile_unit_interface ("", import_filename^, must_exist => false);

    free import_filename;
  }


  rc = compile_unit_body ("", filename, must_exist => true);
  // returns 0 or 1
  if (rc != 0)
  {
    printf ("error: cannot open '%s'\n", filename);
    return rc;
  }

  // compile pending bodies

  while (g_pending_bodies != null)
  {
    PENDING_BODY^ item;

    item = g_pending_bodies;
    g_pending_bodies = g_pending_bodies^.next;

    rc = compile_unit_body (item^.library^, item^.source^, item^.must_exist);
    // returns 0 or 1
    if (rc != 0)
    {
      if (item^.library^'length == 0 || item^.library^[0] == nul)
        printf ("error: cannot open '%s'\n", item^.source^);
      else
        printf ("error: cannot open '%s' [%s]\n", item^.source^, item^.library^);
      return rc;
    }

    free item^.library;
    free item^.source;
    free item;
  }

  return 0;
}

//=================================================================================
