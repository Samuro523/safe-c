
// entities.c : central structure tree.

from std use strings;
use lex, ../common, ../error, tokens, typedecl;

/******************************************************************************/

struct REGION_QUEUE
{
  PREGION first, last;
}

struct LEVEL_NODE
{
  REGION_QUEUE  regions;
  LEVEL_NODE^   outer;
}

/******************************************************************************/

LEVEL_NODE^  current;
long         unique_entity_nr;
ENTITY_LIST^ hidden_entities;

/******************************************************************************/

struct ENTITY_FROM_UNIT_LIST
{
  PENTITY                e;
  PENTITY                unit_decl;
  ENTITY_FROM_UNIT_LIST^ next;
}

/******************************************************************************/

public
PENTITY surrounding_entity ()
{
  LEVEL_NODE^ outer;
  PREGION     r;

  outer = current^.outer;
  r = outer^.regions.last;
  return r^.entities.last;
}

/******************************************************************************/

public
void insert_entity_in_list (PENTITY e, ref ENTITY_LIST^ root)
{
  ENTITY_LIST^ n;

  n = new ENTITY_LIST;

  n^.e = e;
  n^.next = root;

  root = n;
}

/******************************************************************************/

void insert_entity_from_unit_in_list (PENTITY e, PENTITY unit_decl, ref ENTITY_FROM_UNIT_LIST^ root)
{
  ENTITY_FROM_UNIT_LIST^ n;

  n = new ENTITY_FROM_UNIT_LIST;

  n^.e = e;
  n^.unit_decl = unit_decl;
  n^.next = root;

  root = n;
}

/******************************************************************************/

void insert_each_entity_of_list_to_list (    ENTITY_LIST^ list,
                                         ref ENTITY_LIST^ root)
{
  ENTITY_LIST^ l;

  l = list;
  while (l != null)
  {
    insert_entity_in_list (l^.e, ref root);
    l = l^.next;
  }
}

/******************************************************************************/

void insert_each_entity_of_list_to_from_unit_list (    ENTITY_LIST^           list,
                                                       PENTITY                unit_decl,
                                                   ref ENTITY_FROM_UNIT_LIST^ root)
{
  ENTITY_LIST^ l;

  l = list;
  while (l != null)
  {
    insert_entity_from_unit_in_list (l^.e, unit_decl, ref root);
    l = l^.next;
  }
}

/******************************************************************************/

bool entity_exists_in_list (PENTITY e, ref ENTITY_LIST^ root)
{
  ENTITY_LIST^ p;

  p = root;

  while (p != null)
  {
    if (p^.e == e)
      return true;
    p = p^.next;
  }

  return false;
}

/******************************************************************************/

void deallocate_entity_list (ref ENTITY_LIST^ root)
{
  ENTITY_LIST^ p, temp;

  p = root;
  root = null;

  while (p != null)
  {
    temp = p;
    p = p^.next;
    free temp;
  }
}

/******************************************************************************/

void deallocate_entity_from_unit_list (ref ENTITY_FROM_UNIT_LIST^ root)
{
  ENTITY_FROM_UNIT_LIST^ p, temp;

  p = root;
  root = null;

  while (p != null)
  {
    temp = p;
    p = p^.next;
    free temp;
  }
}

/******************************************************************************/

public
void create_region_level ()
{
  LEVEL_NODE^ p;

  p = new LEVEL_NODE;

  p^.outer = current;

  current = p;
}

/******************************************************************************/

public
void unlink_region_level ()
{
  LEVEL_NODE^ p;

  if (current == null)
    fatal_compiler_error ("unlink_region_level()", token.pos);

  p = current;
  current = current^.outer;

  free p;
}

/******************************************************************************/

public
PREGION new_region ()
{
  PREGION r;

  r = new REGION;

  return r;
}

/******************************************************************************/

public
void append_region (PREGION r)
{
  if (current == null)
    fatal_compiler_error ("append_new_region()", token.pos);

  if (current^.regions.first == null)
    current^.regions.first = r;
  else
    current^.regions.last^.next = r;

  current^.regions.last = r;

  r^.next = null;
}

/******************************************************************************/

public
void cleanup_main_region_after_compilation ()
{
  deallocate_entity_list (ref current^.regions.last^.packages);
  clear current^.regions.last^;    // clear last region
}

/******************************************************************************/

// returns 0 if OK, -1 if symbol already exists

int insert_tree (ref PENTITY tree,       // insertion point
                     PENTITY new_entity,  // new entity to insert
                 out bool    overflow)
{
  PENTITY  p, p1, p2;
  int      rc, cmp, ins0, ins1;
  tiny     bal1, bal2;

  if (tree == null)    // tree is empty
  {
    tree = new_entity;
    new_entity^.ptr[0] = null;
    new_entity^.ptr[1] = null;
    new_entity^.bal = 0;

    overflow = true;
    return 0;
  }

  p = tree;

  cmp = wstrcmp (new_entity^.identifier_or_null^,
                 p^.identifier_or_null^);

  if (cmp == 0)   // identifier was found
  {
    overflow = false;
    return -1;    // already exists
  }

  if (cmp < 0)
  {
    ins0 = 0;     // insertion in left subtree
    ins1 = 1;
    bal1 = +1;
    bal2 = -1;
  }
  else    // cmp > 0
  {
    ins0 = 1;     // insertion in right subtree
    ins1 = 0;
    bal1 = -1;
    bal2 = +1;
  }

  rc = insert_tree (ref p^.ptr[ins0], new_entity, out overflow);
  if (rc < 0)
    return rc;    // already exists

  if (overflow)
  {
    if (p^.bal == bal1)
    {
      p^.bal = 0;
      overflow = false;
    }
    else if (p^.bal == 0)
    {
      p^.bal = bal2;
    }
    else  // p^.bal == bal2
    {
      p1 = p^.ptr[ins0];

      if (p1^.bal == bal2)
      {
        p^.ptr[ins0] = p1^.ptr[ins1];
        p1^.ptr[ins1] = p;
        p^.bal = 0;

        p = p1;
        tree = p1;
      }
      else
      {
        p2 = p1^.ptr[ins1];

        p1^.ptr[ins1] = p2^.ptr[ins0];
        p2^.ptr[ins0] = p1;
        p^.ptr[ins0]  = p2^.ptr[ins1];
        p2^.ptr[ins1] = p;

        if (p2^.bal == bal2)
          p^.bal = bal1;
        else
          p^.bal = 0;

        if (p2^.bal == bal1)
          p1^.bal = bal2;
        else
          p1^.bal = 0;

        p = p2;
        tree = p2;
      }

      p^.bal = 0;
      overflow = false;
    }
  }

  return 0;
}

/************************************************************************/

// returns null if not found

public
PENTITY find_tree (PENTITY tree, wstring identifier)
{
  PENTITY  p;
  int      cmp;

  p = tree;
  while (p != null)
  {
    cmp = wstrcmp (identifier, p^.identifier_or_null^);

    if (cmp == 0)   // identifier was found
      return p;

    if (cmp < 0)
      p = p^.ptr[0];
    else
      p = p^.ptr[1];
  }

  return null;
}

/************************************************************************/

// give an identifier to an entity that was declared without identifier

public
void patch_entity_by_setting_an_identifier (PENTITY       e,
                                            wstring       identifier,   // can be empty string
                                            TEXT_POSITION pos)
{
  bool     dummy_overflow, found;
  int      rc;
  PREGION  r;

  if (wstrlen(identifier) > 0)
  {
    e^.identifier_or_null = new wstring ' (identifier[0 : wstrlen(identifier)]);

    // check if the identifier exists in any region of this level,
    // EXCEPT LAST REGION (because it will be checked by insert_tree)

    found = false;

    r = current^.regions.first;
    while (r != current^.regions.last)
    {
      if (find_tree (r^.identifier_btree, e^.identifier_or_null^) != null)
      {
        found = true;
        break;
      }

      r = r^.next;
    }


    // if we're inside a compound statement, search it also in surrounding levels

    {
      LEVEL_NODE^ n;
      PREGION     r0;
      PENTITY     e0;
      ENTITY_KIND k;

      n = current^.outer;     // surrounding level

      for (;;)
      {
        if (n == null)
          break;

        r0 = n^.regions.last;
        if (r0 == null)
          break;

        e0 = r0^.entities.last;
        if (e0 == null)
          break;

        k = e0^.kind;

        if (k >= FIRST_ENTITY_DENOTING_A_STATEMENT && k <= LAST_ENTITY_DENOTING_A_STATEMENT)
        {
          // last entity of scope to be searched contains a statement

          r = n^.regions.first;
          while (r != null)
          {
            if (find_tree (r^.identifier_btree, e^.identifier_or_null^) != null)
            {
              found = true;
              break;
            }

            r = r^.next;
          }
        }
        else
        {
          break;   // it's not a statement
        }

        n = n^.outer;
      }
    }


    // insert it even if it's not unique

    rc = insert_tree (ref current^.regions.last^.identifier_btree, e, out dummy_overflow);
    if (rc < 0)   // duplicate
      found = true;

    _unused dummy_overflow;

    if (found)
      semantic_error ("identifier declared twice", pos);
  }
}

/******************************************************************************/

// create new entity and insert it in symbol tree,
// but does not append it in elaboration queue or assign it a unique nr.

public
PENTITY create_new_entity (ENTITY        entity,
                           wstring       identifier,   // can be empty string
                           TEXT_POSITION pos)
{
  PENTITY  e;

  if (current == null || current^.regions.last == null)
    fatal_compiler_error ("append_entity()", token.pos);

  e = new ENTITY ' (entity);

  patch_entity_by_setting_an_identifier (e, identifier, pos);

  return e;
}

/******************************************************************************/

// assign unique entity nr and append newly created entity in elaboration queue
// (this function is separate so that forward references in entity links are avoided)

public
void append_entity_in_entity_queue (PENTITY e)
{
  ref DECLARATION_QUEUE queue = current^.regions.last^.entities;

  if (queue.first == null)
    queue.first = e;
  else
    queue.last^.next = e;

  queue.last = e;

  e^.nr = ++unique_entity_nr;
}

/******************************************************************************/

// create new entity and insert it in symbol tree and append it in elaboration queue

public
PENTITY append_new_entity (ENTITY        entity,
                           wstring       identifier,   // can be empty string
                           TEXT_POSITION pos)
{
  PENTITY e;

  e = create_new_entity (entity, identifier, pos);

  append_entity_in_entity_queue (e);

  return e;
}

/******************************************************************************/

public
PENTITY append_new_entity_for_instantiation (PENTITY       e0,
                                             TEXT_POSITION pos,
                                             PREGION       pregion)
{
  PENTITY  e;
  int      rc;
  bool     dumy_overflow;

  e = new ENTITY ' (e0^);

  clear e^.next, e^.ptr, e^.bal;

  if (e0^.identifier_or_null != null)
  {
    e^.identifier_or_null = new wstring ' (e0^.identifier_or_null^);

    rc = insert_tree (ref pregion^.identifier_btree, e, out dumy_overflow);
    if (rc < 0)   // duplicate
      fatal_compiler_error ("app.e.inst", pos);
    _unused dumy_overflow;
  }

  e^.nr = ++unique_entity_nr;

  {
    ref DECLARATION_QUEUE queue = pregion^.entities;

    if (queue.first == null)
      queue.first = e;
    else
      queue.last^.next = e;

    queue.last = e;
  }

  return e;
}

/******************************************************************************/

public
void append_new_unit_entity_for_import
                      (PENTITY       e,
                       wstring       identifier,   // can be empty string
                       TEXT_POSITION pos,
                       bool          add_in_package_list)
{
  int     rc;
  bool    dummy_overflow;
  PENTITY q;


  e^.next = null;

  // check if this entity already exists in the elaboration queue

  q = current^.regions.last^.entities.first;
  while (q != null)
  {
    if (q == e)
    {
      semantic_error ("this unit must not be imported twice", pos);
      return;
    }

    q = q^.next;
  }


  free (e^.identifier_or_null);
  e^.identifier_or_null = null;

  if (wstrlen(identifier) > 0)
  {
    e^.identifier_or_null = new wstring ' (identifier[0 : wstrlen(identifier)]);

    rc = insert_tree (ref current^.regions.last^.identifier_btree, e, out dummy_overflow);
    if (rc < 0)  // duplicate
      semantic_error ("a unit of this name already exists", pos);
    _unused dummy_overflow;
  }


  {
    ref DECLARATION_QUEUE queue = current^.regions.last^.entities;

    if (queue.first == null)
      queue.first = e;
    else
      queue.last^.next = e;

    queue.last = e;
  }


  if (add_in_package_list)
  {
    insert_entity_in_list (e, ref current^.regions.last^.packages);
  }
}

/******************************************************************************/

package TYPES

  struct TYPE_NAME_INFO
  {
    TOKEN_KIND kind;
    PENTITY    e;
  }

  const int MAX_TYPES = (24+1);

  TYPE_NAME_INFO type_name_info[MAX_TYPES];
  int            type_name_count;

end TYPES;

/******************************************************************************/

// returns the type entity corresponding to the token,
// or null if the token does not denote a type.

public
PENTITY type_entity_of_token (TOKEN_KIND kind)
{
  int i;

  for (i=0; type_name_info[i].kind != LAST_TOKEN; i++)
  {
    if (type_name_info[i].kind == kind)
      return type_name_info[i].e;
  }

  return null;
}

/******************************************************************************/

void add_type (TOKEN_KIND kind, PENTITY e)
{
  if (type_name_count == MAX_TYPES)
    fatal_compiler_error ("add_type", token.pos);
  type_name_info[type_name_count].kind = kind;
  type_name_info[type_name_count].e    = e;
  type_name_count++;
}

/******************************************************************************/

public
void create_predefined_entities ()
{
  ENTITY (A_UNIT_INTERFACE)       std;
  ENTITY (AN_INTEGER_TYPE)        integer;
  ENTITY (A_FLOAT_TYPE)           floating;
  ENTITY (AN_ENUMERATION_TYPE)    enumer;
  ENTITY (AN_OPEN_ARRAY_TYPE)     array;
  int                             i;
  PENTITY                         e;
  ENTITY (A_NULL_POINTER_TYPE)    null_lit;
  ENTITY (AN_ENUMERATION_LITERAL) lit;
  ENTITY (AN_UNSAFE_POINTER_TYPE) ptr;
  ENTITY (A_VOID_TYPE)            vtype;

  create_region_level ();
  append_region (new_region());

  // create empty predefined unit
  clear std;
  std.the_unit_interface.declarations = new_region();
  append_new_entity (std, L"<>", token.pos);

  create_region_level ();
  append_region (std.the_unit_interface.declarations);

  for (i=0; i<MAX_INTEGER_TYPES; i++)
  {
    clear integer;
    integer.the_integer_type.type = INTEGER_DATA[i].typ;
    e = append_new_entity (integer, INTEGER_DATA[i].name, token.pos);

    if (integer.the_integer_type.type == a_uint1)
      type_byte = e;

    if (integer.the_integer_type.type == a_int4)
      type_int = e;

    if (integer.the_integer_type.type == a_uint4)
      type_uint = e;

    if (integer.the_integer_type.type == a_int8)
      type_long = e;

    add_type (INTEGER_DATA[i].token1, e);
    add_type (INTEGER_DATA[i].token2, e);
  }

  for (i=0; i<MAX_FLOAT_TYPES; i++)
  {
    clear floating;
    floating.the_float_type.type = FLOAT_DATA[i].typ;
    e = append_new_entity (floating, FLOAT_DATA[i].name, token.pos);

    if (floating.the_float_type.type == a_float)
      type_float = e;

    if (floating.the_float_type.type == a_double)
      type_double = e;

    add_type (FLOAT_DATA[i].token1, e);
    add_type (FLOAT_DATA[i].token2, e);
  }


  clear enumer;
  enumer.the_enumeration_type.base = a_uint1;
  enumer.the_enumeration_type.last = 255;
  type_char = append_new_entity (enumer, L"char", token.pos);
  add_type (TOKEN_char, type_char);

  clear enumer;
  enumer.the_enumeration_type.base = a_uint2;
  enumer.the_enumeration_type.last = 65535;
  type_wchar = append_new_entity (enumer, L"wchar", token.pos);
  add_type (TOKEN_wchar, type_wchar);

  clear array;
  array.the_open_array_type.element = type_char;
  type_string = append_new_entity (array, L"string", token.pos);
  add_type (TOKEN_string, type_string);

  clear array;
  array.the_open_array_type.element = type_wchar;
  type_wstring = append_new_entity (array, L"wstring", token.pos);
  add_type (TOKEN_wstring, type_wstring);

  clear array;
  array.the_open_array_type.element = type_byte;
  type_object = append_new_entity (array, L"object", token.pos);
  add_type (TOKEN_object, type_object);


  // array of string (for attribute 'string)

  clear array;
  array.the_open_array_type.element = type_string;
  type_array_of_string = append_new_entity (array, L"", token.pos);


  // array of object (for varying parameter lists)

  clear array;
  array.the_open_array_type.element = type_object;
  type_array_of_object = append_new_entity (array, L"", token.pos);


  // now array of string is defined, we can define type bool and its enumeration literals

  clear enumer;
  enumer.the_enumeration_type.base = a_uint1;
  enumer.the_enumeration_type.last = 1;
  type_bool = append_new_entity (enumer, L"bool", token.pos);
  add_type (TOKEN_bool, type_bool);

  clear lit;
  lit.the_enumeration_literal.type = type_bool;
  lit.the_enumeration_literal.value = 0;
  enum_false = append_new_entity (lit, L"false", token.pos);

  clear lit;
  lit.the_enumeration_literal.type = type_bool;
  lit.the_enumeration_literal.value = 1;
  enum_true = append_new_entity (lit, L"true", token.pos);

  create_enumeration_literal_string_table (type_bool);



  // add pseudo types

  clear integer;
  integer.the_integer_type.type = a_int8;
  type_int_literal = append_new_entity (integer, L"int_literal", token.pos);

  clear floating;
  floating.the_float_type.type = a_double;
  type_float_literal = append_new_entity (floating, L"float_literal", token.pos);

  clear null_lit;
  type_null_literal = append_new_entity (null_lit, L"null_literal", token.pos);

  clear ptr;
  ptr.the_unsafe_pointer_type.designated_type = type_byte;
  type_unsafe_ptr_to_byte = append_new_entity (ptr, L"", token.pos);


  // type void

  clear vtype;
  type_void = append_new_entity (vtype, L"", token.pos);


  add_type (LAST_TOKEN, null);

  unlink_region_level ();
  cleanup_main_region_after_compilation ();
}

/******************************************************************************/

// produces error messages if not found or ambiguous
// returns null if not found

public
PENTITY find_entity (wstring identifier, TEXT_POSITION pos)
{
  LEVEL_NODE^ n;
  PREGION     r;
  PENTITY     e, result;
  ENTITY_KIND kind;
  int         count;
  ENTITY_FROM_UNIT_LIST^ list, list2;

  n = current;
  while (n != null)
  {
    r = n^.regions.first;

    while (r != null)
    {
      e = find_tree (r^.identifier_btree, identifier);
      if (e != null)
        return e;

      r = r^.next;
    }


    // search within all local unit interfaces or package declarations

    count = 0;
    result = null;
    list = null;
    r = n^.regions.first;

    while (r != null)
    {
      ENTITY_LIST^ l;

      l = r^.packages;     // list of local unit-or-package declarations

      while (l != null)
      {
        kind = l^.e^.kind;

        if (kind == A_UNIT_INTERFACE)
        {
          e = find_tree (l^.e^.the_unit_interface.declarations^.identifier_btree, identifier);
          if (e != null)
          {
            count++;
            result = e;
            l^.e^.the_unit_interface.unit_is_used = true;
          }

          insert_each_entity_of_list_to_from_unit_list
               (l^.e^.the_unit_interface.declarations^.packages,
                l^.e,
                ref list);
        }
        else if (kind == A_PACKAGE_DECLARATION)
        {
          e = find_tree (l^.e^.the_package_declaration.declarations^.identifier_btree, identifier);
          if (e != null)
          {
            count++;
            result = e;
          }
          insert_each_entity_of_list_to_from_unit_list
               (l^.e^.the_package_declaration.declarations^.packages,
                null,
                ref list);
        }

        l = l^.next;
      }

      r = r^.next;
    }

    if (count == 1)
    {
      deallocate_entity_from_unit_list (ref list);
      return result;
    }

    if (count > 1)
    {
      semantic_error ("identifier is ambiguous (was declared twice)", pos);
      deallocate_entity_from_unit_list (ref list);
      return null;
    }



    // search in inner nested packages

    while (list != null)
    {
      ENTITY_FROM_UNIT_LIST^ l;

      list2 = null;

      l = list;

      while (l != null)
      {
        kind = l^.e^.kind;

        if (kind == A_PACKAGE_DECLARATION)
        {
          e = find_tree (l^.e^.the_package_declaration.declarations^.identifier_btree, identifier);
          if (e != null)
          {
            count++;
            result = e;
            if (l^.unit_decl != null)
              l^.unit_decl^.the_unit_interface.unit_is_used = true;
          }
          insert_each_entity_of_list_to_from_unit_list
               (l^.e^.the_package_declaration.declarations^.packages,
                l^.unit_decl,
                ref list2);
        }

        l = l^.next;
      }

      deallocate_entity_from_unit_list (ref list);

      list = list2;

      if (count == 1)
      {
        deallocate_entity_from_unit_list (ref list);
        return result;
      }

      if (count > 1)
      {
        semantic_error ("identifier is ambiguous (was declared twice)", pos);
        deallocate_entity_from_unit_list (ref list);
        return null;
      }
    }

    n = n^.outer;
  }

  semantic_error ("identifier was not declared", pos);
  return null;
}

/******************************************************************************/

// does not produce error messages
// returns null if not found

public
PENTITY search_entity_in_local_scope (wstring identifier)
{
  PREGION r;
  PENTITY e;

  r = current^.regions.first;

  while (r != null)
  {
    e = find_tree (r^.identifier_btree, identifier);
    if (e != null)
      return e;

    r = r^.next;
  }

  return null;
}

/******************************************************************************/

// check that the current search point is within the package declaration or package body

public
bool are_we_in_package (PENTITY epackage)
{
  LEVEL_NODE^ ln;
  PREGION     r;
  PENTITY     se;

  ln = current;
  for (;;)
  {
    ln = ln^.outer;

    if (ln == null)
      return false;

    r = ln^.regions.last;
    se = r^.entities.last;

    if (se^.kind == A_PACKAGE_BODY)
      se = se^.the_package_body.to_package_declaration;

    if (se == epackage)    // found (we're inside)
      return true;
  }
}

/******************************************************************************/

// assert: current token is an identifier
// returns null if not found

public
PENTITY parse_expanded_name ()
{
  PENTITY p, e;

  if (token.kind != IDENTIFIER)
  {
    syntax_error ("identifier expected", token.pos);
    return null;
  }

  p = find_entity (token.info._identifier.value, token.pos);

  if (p == null)     // error already given above
  {
    get_token ();    // skip identifier
    return null;
  }

  if (entity_exists_in_list (p, ref hidden_entities))
  {
    semantic_error ("identifier is not fully declared", token.pos);
    get_token ();    // skip identifier
    return null;
  }

  get_token ();    // skip identifier

  if (p^.kind == A_UNIT_INTERFACE && token.kind == DOT)
  {
    p^.the_unit_interface.unit_is_used = true;

    get_token ();   // skip DOT

    if (token.kind != IDENTIFIER)
    {
      syntax_error ("identifier expected", token.pos);
      return p;
    }

    e = find_tree (p^.the_unit_interface.declarations^.identifier_btree, token.info._identifier.value);
    if (e == null)
    {
      syntax_error ("identifier was not declared in unit", token.pos);
      get_token ();    // skip identifier
      return p;
    }

    p = e;

    if (entity_exists_in_list (p, ref hidden_entities))
    {
      semantic_error ("identifier is not fully declared", token.pos);
      get_token ();    // skip identifier
      return null;
    }

    get_token ();    // skip identifier
  }


  while (p^.kind == A_PACKAGE_DECLARATION && token.kind == DOT)
  {
    if (p^.the_package_declaration.is_generic)
    {
      if (!are_we_in_package (p))   // we're outside the generic package
        break;
    }

    get_token ();   // skip DOT

    if (token.kind != IDENTIFIER)
    {
      syntax_error ("identifier expected", token.pos);
      return p;
    }

    e = find_tree (p^.the_package_declaration.declarations^.identifier_btree, token.info._identifier.value);
    if (e == null)
    {
      syntax_error ("identifier was not declared in package", token.pos);
      get_token ();    // skip identifier
      return p;
    }

    p = e;

    if (entity_exists_in_list (p, ref hidden_entities))
    {
      semantic_error ("identifier is not fully declared", token.pos);
      get_token ();    // skip identifier
      return null;
    }

    get_token ();    // skip identifier
  }

  return p;
}

/******************************************************************************/

public
void hide_entity (PENTITY e)
{
  insert_entity_in_list (e, ref hidden_entities);
}

/******************************************************************************/

public
void unhide_all_entities ()
{
  deallocate_entity_list (ref hidden_entities);
}

/******************************************************************************/

// insert package declaration in package list of surrounding scope

public
void append_package_declaration_to_surrounding_scope_package_list (PENTITY e)
{
  LEVEL_NODE^ outer;
  PREGION     r;

  outer = current^.outer;
  r = outer^.regions.last;

  insert_entity_in_list (e, ref r^.packages);
}

/******************************************************************************/

public
ENTITY_LIST^ build_list_of_surrounding_generic_packages ()
{
  ENTITY_LIST^ l;
  LEVEL_NODE^  lev;
  PREGION      r;
  PENTITY      e, edecl;

  l = null;

  lev = current^.outer;
  while (lev != null)
  {
    r = lev^.regions.last;
    e = r^.entities.last;

    if (e^.kind == A_PACKAGE_DECLARATION)
    {
      if (e^.the_package_declaration.is_generic)
      {
        insert_entity_in_list (e, ref l);
      }
    }

    if (e^.kind == A_PACKAGE_BODY)
    {
      edecl = e^.the_package_body.to_package_declaration;
      if (edecl^.the_package_declaration.is_generic)
      {
        insert_entity_in_list (edecl, ref l);
      }
    }

    lev = lev^.outer;
  }

  return l;
}

/******************************************************************************/

public
ENTITY_LIST^ duplicate_entity_list (ENTITY_LIST^ list)
{
  ENTITY_LIST^ a, b, n, l;

  a = list;
  b = null;
  l = null;

  while (a != null)
  {
    n = new ENTITY_LIST;
    n^.e    = a^.e;
//    n^.next = null;

    if (l == null)
      b = n;
    else
      l^.next = n;
    l = n;

    a = a^.next;
  }

  return b;
}

/******************************************************************************/

public
LIST_OF_EXPRESSIONS^ duplicate_list_of_expressions (LIST_OF_EXPRESSIONS^ list)
{
  LIST_OF_EXPRESSIONS^ a, b, n, l;

  a = list;
  b = null;
  l = null;

  while (a != null)
  {
    n = new LIST_OF_EXPRESSIONS;
    n^.prev = l;
//    n^.next = null;
    n^.exp  = a^.exp;
    n^.type = a^.type;
    n^.e    = a^.e;

    if (l == null)
      b = n;
    else
      l^.next = n;
    l = n;

    a = a^.next;
  }

  return b;
}

/******************************************************************************/

public
CASE_CONSTANT^ duplicate_case_constant_list (CASE_CONSTANT^ list)
{
  CASE_CONSTANT^ a, b, n, l;

  a = list;
  b = null;
  l = null;

  while (a != null)
  {
    n = new CASE_CONSTANT;
    n^.value = a^.value;
    n^.next  = null;

    if (l == null)
      b = n;
    else
      l^.next = n;
    l = n;

    a = a^.next;
  }

  return b;
}

/******************************************************************************/

public
void store_location (out LOCATION l)
{
  l = {unit_key    => g_unit_key,
       source_line => token.pos.line};
}

/******************************************************************************/
