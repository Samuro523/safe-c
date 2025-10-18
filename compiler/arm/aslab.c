
// aslab.c : tree that stores labels for jumping under the function's code and raise int5 exceptions

from std use bintree;
use ../error;

/***********************************************************************************/

package B1 = new BALANCED_BINARY_TREE (ELEMENT => LINE_LABEL_DATA, USER_INFO => bool);

B1.BINARY_TREE ll_tree;

/***********************************************************************************/

int ll_compare (bool^           user,
                LINE_LABEL_DATA data1,
                LINE_LABEL_DATA data2)
{
  _unused user;
  
  if (data1.line < data2.line)
    return -1;
  if (data1.line > data2.line)
    return +1;
  if (data1.occurence < data2.occurence)
    return -1;
  if (data1.occurence > data2.occurence)
    return +1;
  return 0;
}

/***********************************************************************************/

// returns label_nr (either new of from existing node for that line)

public
int4 store_ll (int line, int4 label_nr)
{
  LINE_LABEL_DATA u;
  int             rc;

  clear u;
  u.line      = line;
  u.occurence = 0;
  u.label_nr  = label_nr;

  rc = insert_btree (ref ll_tree, u);
  if (rc == 0)
    return label_nr;

  if (rc != BT_DUPLICATE_KEY)
    fatal_compiler_error0 ("store_ll(1)");

  u.line = line;

  rc = retrieve_btree (ll_tree, ref u, BT_EQUAL);
  if (rc < 0)
    fatal_compiler_error0 ("store_ll(2)");

  return u.label_nr;
}

/***********************************************************************************/

// stores always the label, with an occurence >= 1 (for P_ASSERT)

public
void store_ll_forced (int line, int4 label_nr)
{
  LINE_LABEL_DATA u;
  int             rc, i;

  for (i=1; ; i++)
  {
    clear u;
    u.line      = line;
    u.occurence = i;
    u.label_nr  = label_nr;

    rc = insert_btree (ref ll_tree, u);
    if (rc == 0)
      return;

    if (rc != BT_DUPLICATE_KEY)
      fatal_compiler_error0 ("store_ll_forced(1)");
  }
}

/***********************************************************************************/

public
void create_ll_tree ()
{
  create_btree (out ll_tree, null, ll_compare);
}

/***********************************************************************************/

public
void traverse_ll_tree (LL_FUNC func)
{
  traverse_btree (ll_tree, func, +1);
}

/***********************************************************************************/

public
void close_ll_tree ()
{
  close_btree (ref ll_tree);
}

/***********************************************************************************/
