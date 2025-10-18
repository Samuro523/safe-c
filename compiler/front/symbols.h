
// symbols.h : symbol table tree

from std use bintree;

/**********************************************************************************/

struct SYMBOL_ENTRY
{
  wstring^ symbol;
  bool     value;
}

package P = new BALANCED_BINARY_TREE (ELEMENT => SYMBOL_ENTRY, USER_INFO => bool);

/**********************************************************************************/

void create_symbol_btree (out BINARY_TREE tree);
void free_symbol_btree (ref BINARY_TREE tree);

/**********************************************************************************/

// returns 0 if OK, -1 if error, +1 if already exists
int insert_symbol (ref BINARY_TREE tree, wstring s, bool value);

// returns 0 if OK, -1 if error, +1 if not found
// presult is symbol value.
int get_symbol_value (ref BINARY_TREE tree, wstring s, out bool presult);

/**********************************************************************************/
