
// dllnames.c

from std use bintree, strings;
use error;

/**********************************************************************************/

struct NODE
{
  uint4   nr;
  string^ dll;
  string^ func;
}

package P = new BALANCED_BINARY_TREE (ELEMENT => NODE, USER_INFO => bool);

BINARY_TREE tree;
bool        tree_created;
uint4       unique_nr;

/**********************************************************************************/

int compare (bool^  user,
             NODE   a,
             NODE   b)
{
  _unused user;
  if (a.nr < b.nr)
    return -1;
  if (a.nr > b.nr)
    return +1;
  return 0;
}

/**********************************************************************************/

string^ new_string (string s)
{
  return new string ' (s[0 : strlen(s)]);
}

/**********************************************************************************/

public
uint4 insert_dll_name (string dll, string func)
{
  NODE n;
  int  rc;

  if (!tree_created)
  {
    create_btree (out tree, null, compare);
    tree_created = true;
  }

  clear n;
  n.nr = ++unique_nr;
  n.dll = new_string (dll);
  n.func = new_string (func);

  rc = insert_btree (ref tree, n);
  if (rc < 0)
    fatal_out_of_memory_error ("insert_dll_name(1)");

  return unique_nr;
}

/**********************************************************************************/

// never free dll and func, they are shared and always stay allocated.

public
void get_dll_name (uint4 nr, out string^ dll, out string^ func)
{
  NODE n;
  int  rc;

  clear n;
  n.nr = nr;

  rc = retrieve_btree (tree, ref n, BT_EQUAL);
  if (rc < 0)
    fatal_out_of_memory_error ("get_dll_name(1)");

  dll  = n.dll;
  func = n.func;
}

/**********************************************************************************/
