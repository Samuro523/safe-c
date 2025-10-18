

package body ARRAY_EXTENDER

  public int nb_elements (ELEMENT[]^ table)
  {
    return (table == null) ? 0 : table^ ' length;
  }

  public void enlarge (ref ELEMENT[]^ table, int by_count)
  {
    int        length;
    ELEMENT[]^ n;

    if (by_count == 0)
      return;

    assert by_count > 0;

    length = (table == null) ? 0 : table^ ' length;

    n = new ELEMENT[length + by_count];

    if (length > 0)
      n^[0 : length] = table^;

    free table;

    table = n;
  }

  public void append (ref ELEMENT[]^ table, ELEMENT element)
  {
    enlarge (ref table, 1);
    table^[table^'length - 1] = element;
  }

  public void insert (ref ELEMENT[]^ table, int index, ELEMENT element)
  {
    int        length;
    ELEMENT[]^ n;

    length = (table == null) ? 0 : table^ ' length;

    assert index >= 0 && index <= length;

    n = new ELEMENT[length + 1];

    if (length > 0)
    {
      n^[0 : index] = table^[0 : index];
      n^[index + 1 : length - index] = table^[index : length - index];
    }
    n^[index] = element;
 
    free table;

    table = n;
  }
  
  public void insert_slice (ref ELEMENT[]^ table, int index, ELEMENT[] elements)
  {
    int        length;
    ELEMENT[]^ n;

    length = (table == null) ? 0 : table^ ' length;

    assert index >= 0 && index <= length;

    n = new ELEMENT[length + elements'length];

    if (length > 0)
    {
      n^[0 : index] = table^[0 : index];
      n^[index + elements'length : length - index] = table^[index : length - index];
    }
    n^[index : elements'length] = elements;
 
    free table;

    table = n;
  }
  
  public void append_slice (ref ELEMENT[]^ table, ELEMENT[] elements)
  {
    insert_slice (ref table, index => nb_elements(table), elements);
  }
  
  public void shrink (ref ELEMENT[]^ table, int by_count)
  {
    int        length, nb;
    ELEMENT[]^ n;

    if (by_count == 0)
      return;

    length = (table == null) ? 0 : table^ ' length;

    nb = length - by_count;

    assert by_count > 0 && nb >= 0;

    n = new ELEMENT[nb];

    if (nb > 0)
      n^ = table^[0 : nb];

    free table;

    table = n;
  }

  public void remove (ref ELEMENT[]^ table, int index)
  {
    int        length = (table == null) ? 0 : table^ ' length;
    ELEMENT[]^ n;

    assert index >= 0 && index < length;

    n = new ELEMENT[length-1];

    n^ [0 : index] = table^ [0 : index];
    n^ [index : length - index - 1] = table^ [index + 1 : length - index - 1];

    free table;

    table = n;
  }

  // remove count elements at index
  public void remove_slice (ref ELEMENT[]^ table, int index, int count)
  {
    int        length = (table == null) ? 0 : table^ ' length;
    ELEMENT[]^ n;

    assert index >= 0 && index <= length && count >= 0 && count <= length && index + count <= length;

    n = new ELEMENT[length-count];

    n^ [0 : index] = table^ [0 : index];
    n^ [index : length - index - count] = table^ [index + count : length - index - count];

    free table;

    table = n;
  }
  
end ARRAY_EXTENDER;
