
// arraye.h

generic <ELEMENT>
package ARRAY_EXTENDER

  int nb_elements (ELEMENT[]^ table);

  // make place for by_count new elements at the end
  void enlarge (ref ELEMENT[]^ table, int by_count);

  // append an element at the end
  void append (ref ELEMENT[]^ table, ELEMENT element);

  // append a slice of elements at the end
  void append_slice (ref ELEMENT[]^ table, ELEMENT[] elements);
  
  // insert an element at index
  void insert (ref ELEMENT[]^ table, int index, ELEMENT element);

  // insert a slice at index
  void insert_slice (ref ELEMENT[]^ table, int index, ELEMENT[] elements);
  
  // remove the last by_count elements
  void shrink (ref ELEMENT[]^ table, int by_count);

  // remove an element at index
  void remove (ref ELEMENT[]^ table, int index);

  // remove count elements at index
  void remove_slice (ref ELEMENT[]^ table, int index, int count);

end ARRAY_EXTENDER;
