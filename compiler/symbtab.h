
// symbtab.h

generic <ELEMENT>
package SymbolTable

  struct SYMBOL_TABLE;

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  // elements must be unique
  typedef int COMPARE (ELEMENT a, ELEMENT b);
 
  void create   (out SYMBOL_TABLE st, COMPARE compare);
  void close    (ref SYMBOL_TABLE st);
  
  // returns true if OK
  bool insert   (ref SYMBOL_TABLE st, ELEMENT data);
  
  // returns true if OK
  bool update   (ref SYMBOL_TABLE st, ELEMENT data);

  // returns true if OK
  bool retrieve (ref SYMBOL_TABLE st, ELEMENT key, out ELEMENT data);

  typedef void OPERATE (ELEMENT data);

  void traverse (ref SYMBOL_TABLE  st,
                     OPERATE       operate,
                     short         order = +1);     /* -1 descending, or +1 ascending */
end SymbolTable;
