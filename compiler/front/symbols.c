
// symbols.c

from std use bintree, strings;

/**********************************************************************************/

int compare (bool^        user,
             SYMBOL_ENTRY a,
             SYMBOL_ENTRY b)
{
  _unused user;
  return wstrcmp (a.symbol^, b.symbol^);
}

/**********************************************************************************/

public
void create_symbol_btree (out BINARY_TREE tree)
{
  create_btree (out tree, null, compare);
}

/**********************************************************************************/

// returns 0 if OK, -1 if error, +1 if already exists

public
int insert_symbol (ref BINARY_TREE tree, wstring s, bool value)
{
  SYMBOL_ENTRY e;
  int          rc;

  clear e;
  e.symbol = new wstring ' (s[0 : wstrlen(s)]);
  e.value = value;

  rc = insert_btree (ref tree, e);
  if (rc == 0)
    return 0;

  free e.symbol;

  if (rc == BT_DUPLICATE_KEY)
    return +1;

  return -1;
}

/**********************************************************************************/

int operate (bool^        user,
             SYMBOL_ENTRY data)
{
  _unused user;
  free data.symbol;
  return 0;
}

/**********************************************************************************/

public
void free_symbol_btree (ref BINARY_TREE tree)
{
  traverse_btree (tree, operate, +1);
  close_btree (ref tree);
}

/**********************************************************************************/

// returns 0 if OK, -1 if error, +1 if not found
// presult is symbol value

public
int get_symbol_value (ref BINARY_TREE tree, wstring s, out bool presult)
{
  SYMBOL_ENTRY e;
  int          rc;
  wstring^     p;

  p = new wstring ' (s[0 : wstrlen(s)]);

  clear e;
  e.symbol = p;
  e.value = false;

  rc = retrieve_btree (tree, ref e, BT_EQUAL);

  free p;

  if (rc == 0)
  {
    presult = e.value;
    return 0;
  }
  else
  {
    presult = false;
    if (rc == BT_KEY_NOT_FOUND)
      return +1;
    return -1;
  }
}

/**********************************************************************************/
