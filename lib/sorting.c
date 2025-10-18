
// sorting.c : sort an array or array slice.

//===================================================================

package body HeapSort

#begin unsafe   // use unsafe pointers to avoid array index checks

  //---------------------------------------------------------------------

  // stores largest element from [inf..sup-1] at index inf

  void maximize (ELEMENT *table, int inf, int sup)
  {
    ELEMENT bubulle;
    int     i_pere, i_fils, demi_sup;

    bubulle  = table[inf];
    i_pere   = inf + 1;
    demi_sup = sup >> 1;

    while (i_pere <= demi_sup)
    {
      i_fils = i_pere << 1;

      if (i_fils < sup)    // index i_fils is valid
      {
        i_fils += (int)(compare (table[i_fils-1], table[i_fils]) < 0);
      }

      if (compare (bubulle, table[i_fils-1]) >= 0)
        break;

      table[i_pere-1] = table[i_fils-1];
      i_pere = i_fils;
    }

    table[i_pere-1] = bubulle;
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[])
  {
    int min, sup;

    min = table'length >> 1;

    while (min >= 1)
    {
      min--;
      maximize (&table, min, table'length);
    }

    sup = table'length - 1;

    while (sup > 0)
    {
      ELEMENT temp;

      // largest element is at index 0, move it to index sup.
      temp       = table[0];
      table[0]   = table[sup];
      table[sup] = temp;

      // stores largest element from [0..sup-1] at index 0
      maximize (&table, 0, sup);

      sup--;
    }
  }

  //---------------------------------------------------------------------

#end unsafe

end HeapSort;

//===================================================================

package body HeapSort2

#begin unsafe   // use unsafe pointers to avoid array index checks

  //---------------------------------------------------------------------

  // stores largest element from [inf..sup-1] at index inf

  void maximize (ELEMENT *table, int inf, int sup, COMPARE compare)
  {
    ELEMENT bubulle;
    int     i_pere, i_fils, demi_sup;

    bubulle  = table[inf];
    i_pere   = inf + 1;
    demi_sup = sup >> 1;

    while (i_pere <= demi_sup)
    {
      i_fils = i_pere << 1;

      if (i_fils < sup)    // index i_fils is valid
      {
        i_fils += (int)(compare (table[i_fils-1], table[i_fils]) < 0);
      }

      if (compare (bubulle, table[i_fils-1]) >= 0)
        break;

      table[i_pere-1] = table[i_fils-1];
      i_pere = i_fils;
    }

    table[i_pere-1] = bubulle;
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[], COMPARE compare)
  {
    int min, sup;

    min = table'length >> 1;

    while (min >= 1)
    {
      min--;
      maximize (&table, min, table'length, compare);
    }

    sup = table'length - 1;

    while (sup > 0)
    {
      ELEMENT temp;

      // largest element is at index 0, move it to index sup.
      temp       = table[0];
      table[0]   = table[sup];
      table[sup] = temp;

      // stores largest element from [0..sup-1] at index 0
      maximize (&table, 0, sup, compare);

      sup--;
    }
  }

  //---------------------------------------------------------------------

#end unsafe

end HeapSort2;

//===================================================================

package body HeapSort3

#begin unsafe   // use unsafe pointers to avoid array index checks

  //---------------------------------------------------------------------

  // stores largest element from [inf..sup-1] at index inf

  void maximize (ELEMENT *table, int inf, int sup, COMPARE compare, ref USER_INFO user_info)
  {
    ELEMENT bubulle;
    int     i_pere, i_fils, demi_sup;

    bubulle  = table[inf];
    i_pere   = inf + 1;
    demi_sup = sup >> 1;

    while (i_pere <= demi_sup)
    {
      i_fils = i_pere << 1;

      if (i_fils < sup)    // index i_fils is valid
      {
        i_fils += (int)(compare (table[i_fils-1], table[i_fils], ref user_info) < 0);
      }

      if (compare (bubulle, table[i_fils-1], ref user_info) >= 0)
        break;

      table[i_pere-1] = table[i_fils-1];
      i_pere = i_fils;
    }

    table[i_pere-1] = bubulle;
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[], COMPARE compare, ref USER_INFO user_info)
  {
    int min, sup;

    min = table'length >> 1;

    while (min >= 1)
    {
      min--;
      maximize (&table, min, table'length, compare, ref user_info);
    }

    sup = table'length - 1;

    while (sup > 0)
    {
      ELEMENT temp;

      // largest element is at index 0, move it to index sup.
      temp       = table[0];
      table[0]   = table[sup];
      table[sup] = temp;

      // stores largest element from [0..sup-1] at index 0
      maximize (&table, 0, sup, compare, ref user_info);

      sup--;
    }
  }

  //---------------------------------------------------------------------

#end unsafe

end HeapSort3;

//===================================================================

package body HeapSort4

#begin unsafe   // use unsafe pointers to avoid array index checks

  //---------------------------------------------------------------------

  // stores largest element from [inf..sup-1] at index inf

  void maximize (ELEMENT *table, int inf, int sup, ref USER_INFO user_info)
  {
    ELEMENT bubulle;
    int     i_pere, i_fils, demi_sup;

    bubulle  = table[inf];
    i_pere   = inf + 1;
    demi_sup = sup >> 1;

    while (i_pere <= demi_sup)
    {
      i_fils = i_pere << 1;

      if (i_fils < sup)    // index i_fils is valid
      {
        i_fils += (int)(compare (table[i_fils-1], table[i_fils], ref user_info) < 0);
      }

      if (compare (bubulle, table[i_fils-1], ref user_info) >= 0)
        break;

      table[i_pere-1] = table[i_fils-1];
      i_pere = i_fils;
    }

    table[i_pere-1] = bubulle;
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[], ref USER_INFO user_info)
  {
    int min, sup;

    min = table'length >> 1;

    while (min >= 1)
    {
      min--;
      maximize (&table, min, table'length, ref user_info);
    }

    sup = table'length - 1;

    while (sup > 0)
    {
      ELEMENT temp;

      // largest element is at index 0, move it to index sup.
      temp       = table[0];
      table[0]   = table[sup];
      table[sup] = temp;

      // stores largest element from [0..sup-1] at index 0
      maximize (&table, 0, sup, ref user_info);

      sup--;
    }
  }

  //---------------------------------------------------------------------

#end unsafe

end HeapSort4;

//===================================================================

package body QuickSort

  //---------------------------------------------------------------------

  void swap (ref ELEMENT a, ref ELEMENT b)
  {
    ELEMENT c;
    c = a;
    a = b;
    b = c;
  }

  //---------------------------------------------------------------------

  // return the index, not of the smallest or of the largest element,
  // but of the element in the middle among the 3 elements :
  // TABLE (A), TABLE (B), TABLE (C).

  int get_medium_index (ELEMENT table[], int a, int b, int c)
  {
    int xi, yi;

    if (compare (table[a], table[b]) < 0)
    {
      xi = a;    // smallest
      yi = b;    // largest
    }
    else
    {
      xi = b;    // smallest
      yi = a;    // largest
    }

    // assertion: table (xi) <= table (yi)

    if (compare (table[c], table[xi]) < 0)
    {
      // element TABLE (C) is smallest, so TABLE (XI) is in the middle.
      return xi;
    }

    if (compare (table[yi], table[c]) < 0)
    {
      // element TABLE (C) is largest, so TABLE (Y1) is in the middle.
      return yi;
    }

    return c;     // element TABLE (C) is in the middle.
  }

  //---------------------------------------------------------------------

  // compute the final place of some element TABLE (MIDDLE) and return MIDDLE.
  // move all elements that are smaller to its left,
  // and all elements that are larger to its right.
  // WARNING: INF and SUP must never leave INDEX_TYPE.

  int partition (ref ELEMENT table[])
  {
    int     middle_index, medium_index, inf, sup;
    ELEMENT temp;

    // compute the index in the middle of the array
    middle_index = (table'length >> 1);

    // compute some index whichs element has an average value
    medium_index = get_medium_index (table,
                                     0,
                                     middle_index,
                                     table'length - 1);

    // save the average value element
    temp = table[medium_index];

    // working in TABLE (INF .. SUP)
    inf = 0;
    sup = table'length - 1;

    for (;;)
    {
      // increase INF til an element larger or equal to TEMP is found
      while (inf < sup && compare (table[inf], temp) < 0)
        inf++;                           // element smaller than TEMP

      // decrease SUP til an element smaller or equal to TEMP is found
      while (inf < sup && compare (temp, table[sup]) < 0)
        sup--;                           // element larger than TEMP

      if (inf == sup)
        break;

      if (compare (temp, table[inf]) != 0 || compare (temp, table[sup]) != 0)
      {
        // exchange the two values smaller and larger than TEMP
        // and update MEDIUM_INDEX.

        swap (ref table[inf], ref table[sup]);

        if (medium_index == inf)
          medium_index = sup;
        else if (medium_index == sup)
          medium_index = inf;
      }
      else
      {
        // both elements at INF and SUP have the value TEMP
        // we must skip one of them.
        inf++;
      }
    }

    // put the medium index element at its final place
    swap (ref table[medium_index], ref table[inf]);

    // and return its index
    return inf;
  }

  //---------------------------------------------------------------------

  package BubbleS = new BubbleSort (ELEMENT => ELEMENT,
                                    compare => compare);

  //---------------------------------------------------------------------

  void do_quick_sort (ref ELEMENT table[], int nesting)
  {
    int middle;

    if (table'length <= 1)
      return;

    if (table'length <= 10 ||      // BUBBLE_SORT is faster
        nesting > 32)              // prevent stack overflow
    {
      BubbleS.sort (ref table);
    }
    else                           // QUICK_SORT is faster
    {
      middle = partition (ref table);

      if (middle > 1)
      {
        do_quick_sort (ref table[0:middle],
                       nesting+1);
      }

      if (middle < table'length - 2)
      {
        do_quick_sort (ref table[middle+1 : table'length-middle-1],
                       nesting+1);
      }
    }
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[])
  {
    do_quick_sort (ref table, 1);
  }

  //---------------------------------------------------------------------

end QuickSort;

//===================================================================

package body QuickSort2

  //---------------------------------------------------------------------

  void swap (ref ELEMENT a, ref ELEMENT b)
  {
    ELEMENT c;
    c = a;
    a = b;
    b = c;
  }

  //---------------------------------------------------------------------

  // return the index, not of the smallest or of the largest element,
  // but of the element in the middle among the 3 elements :
  // TABLE (A), TABLE (B), TABLE (C).

  int get_medium_index (ELEMENT table[], int a, int b, int c, COMPARE compare)
  {
    int xi, yi;

    if (compare (table[a], table[b]) < 0)
    {
      xi = a;    // smallest
      yi = b;    // largest
    }
    else
    {
      xi = b;    // smallest
      yi = a;    // largest
    }

    // assertion: table (xi) <= table (yi)

    if (compare (table[c], table[xi]) < 0)
    {
      // element TABLE (C) is smallest, so TABLE (XI) is in the middle.
      return xi;
    }

    if (compare (table[yi], table[c]) < 0)
    {
      // element TABLE (C) is largest, so TABLE (Y1) is in the middle.
      return yi;
    }

    return c;     // element TABLE (C) is in the middle.
  }

  //---------------------------------------------------------------------

  // compute the final place of some element TABLE (MIDDLE) and return MIDDLE.
  // move all elements that are smaller to its left,
  // and all elements that are larger to its right.
  // WARNING: INF and SUP must never leave INDEX_TYPE.

  int partition (ref ELEMENT table[], COMPARE compare)
  {
    int     middle_index, medium_index, inf, sup;
    ELEMENT temp;

    // compute the index in the middle of the array
    middle_index = (table'length >> 1);

    // compute some index whichs element has an average value
    medium_index = get_medium_index (table,
                                     0,
                                     middle_index,
                                     table'length - 1,
                                     compare);

    // save the average value element
    temp = table[medium_index];

    // working in TABLE (INF .. SUP)
    inf = 0;
    sup = table'length - 1;

    for (;;)
    {
      // increase INF til an element larger or equal to TEMP is found
      while (inf < sup && compare (table[inf], temp) < 0)
        inf++;                           // element smaller than TEMP

      // decrease SUP til an element smaller or equal to TEMP is found
      while (inf < sup && compare (temp, table[sup]) < 0)
        sup--;                           // element larger than TEMP

      if (inf == sup)
        break;

      if (compare (temp, table[inf]) != 0 || compare (temp, table[sup]) != 0)
      {
        // exchange the two values smaller and larger than TEMP
        // and update MEDIUM_INDEX.

        swap (ref table[inf], ref table[sup]);

        if (medium_index == inf)
          medium_index = sup;
        else if (medium_index == sup)
          medium_index = inf;
      }
      else
      {
        // both elements at INF and SUP have the value TEMP
        // we must skip one of them.
        inf++;
      }
    }

    // put the medium index element at its final place
    swap (ref table[medium_index], ref table[inf]);

    // and return its index
    return inf;
  }

  //---------------------------------------------------------------------

  package BubbleS = new BubbleSort2 (ELEMENT => ELEMENT);

  //---------------------------------------------------------------------

  void do_quick_sort (ref ELEMENT table[], int nesting, COMPARE compare)
  {
    int middle;

    if (table'length <= 1)
      return;

    if (table'length <= 10 ||      // BUBBLE_SORT is faster
        nesting > 32)              // prevent stack overflow
    {
      BubbleS.sort (ref table, compare);
    }
    else                           // QUICK_SORT is faster
    {
      middle = partition (ref table, compare);

      if (middle > 1)
      {
        do_quick_sort (ref table[0:middle],
                       nesting+1,
                       compare);
      }

      if (middle < table'length - 2)
      {
        do_quick_sort (ref table[middle+1 : table'length-middle-1],
                       nesting+1,
                       compare);
      }
    }
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[], COMPARE compare)
  {
    do_quick_sort (ref table, 1, compare);
  }

  //---------------------------------------------------------------------

end QuickSort2;

//===================================================================

package body QuickSort3

  //---------------------------------------------------------------------

  void swap (ref ELEMENT a, ref ELEMENT b)
  {
    ELEMENT c;
    c = a;
    a = b;
    b = c;
  }

  //---------------------------------------------------------------------

  // return the index, not of the smallest or of the largest element,
  // but of the element in the middle among the 3 elements :
  // TABLE (A), TABLE (B), TABLE (C).

  int get_medium_index (ELEMENT table[], int a, int b, int c, COMPARE compare, ref USER_INFO user_info)
  {
    int xi, yi;

    if (compare (table[a], table[b], ref user_info) < 0)
    {
      xi = a;    // smallest
      yi = b;    // largest
    }
    else
    {
      xi = b;    // smallest
      yi = a;    // largest
    }

    // assertion: table (xi) <= table (yi)

    if (compare (table[c], table[xi], ref user_info) < 0)
    {
      // element TABLE (C) is smallest, so TABLE (XI) is in the middle.
      return xi;
    }

    if (compare (table[yi], table[c], ref user_info) < 0)
    {
      // element TABLE (C) is largest, so TABLE (Y1) is in the middle.
      return yi;
    }

    return c;     // element TABLE (C) is in the middle.
  }

  //---------------------------------------------------------------------

  // compute the final place of some element TABLE (MIDDLE) and return MIDDLE.
  // move all elements that are smaller to its left,
  // and all elements that are larger to its right.
  // WARNING: INF and SUP must never leave INDEX_TYPE.

  int partition (ref ELEMENT table[], COMPARE compare, ref USER_INFO user_info)
  {
    int     middle_index, medium_index, inf, sup;
    ELEMENT temp;

    // compute the index in the middle of the array
    middle_index = (table'length >> 1);

    // compute some index whichs element has an average value
    medium_index = get_medium_index (table,
                                     0,
                                     middle_index,
                                     table'length - 1,
                                     compare,
                                     ref user_info);

    // save the average value element
    temp = table[medium_index];

    // working in TABLE (INF .. SUP)
    inf = 0;
    sup = table'length - 1;

    for (;;)
    {
      // increase INF til an element larger or equal to TEMP is found
      while (inf < sup && compare (table[inf], temp, ref user_info) < 0)
        inf++;                           // element smaller than TEMP

      // decrease SUP til an element smaller or equal to TEMP is found
      while (inf < sup && compare (temp, table[sup], ref user_info) < 0)
        sup--;                           // element larger than TEMP

      if (inf == sup)
        break;

      if (compare (temp, table[inf], ref user_info) != 0 || compare (temp, table[sup], ref user_info) != 0)
      {
        // exchange the two values smaller and larger than TEMP
        // and update MEDIUM_INDEX.

        swap (ref table[inf], ref table[sup]);

        if (medium_index == inf)
          medium_index = sup;
        else if (medium_index == sup)
          medium_index = inf;
      }
      else
      {
        // both elements at INF and SUP have the value TEMP
        // we must skip one of them.
        inf++;
      }
    }

    // put the medium index element at its final place
    swap (ref table[medium_index], ref table[inf]);

    // and return its index
    return inf;
  }

  //---------------------------------------------------------------------

  package BubbleS = new BubbleSort3 (ELEMENT => ELEMENT, USER_INFO => USER_INFO);

  //---------------------------------------------------------------------

  void do_quick_sort (ref ELEMENT table[], int nesting, COMPARE compare, ref USER_INFO user_info)
  {
    int middle;

    if (table'length <= 1)
      return;

    if (table'length <= 10 ||      // BUBBLE_SORT is faster
        nesting > 32)              // prevent stack overflow
    {
      BubbleS.sort (ref table, compare, ref user_info);
    }
    else                           // QUICK_SORT is faster
    {
      middle = partition (ref table, compare, ref user_info);

      if (middle > 1)
      {
        do_quick_sort (ref table[0:middle],
                       nesting+1,
                       compare,
                       ref user_info);
      }

      if (middle < table'length - 2)
      {
        do_quick_sort (ref table[middle+1 : table'length-middle-1],
                       nesting+1,
                       compare,
                       ref user_info);
      }
    }
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[], COMPARE compare, ref USER_INFO user_info)
  {
    do_quick_sort (ref table, 1, compare, ref user_info);
  }

  //---------------------------------------------------------------------

end QuickSort3;

//===================================================================

package body QuickSort4

  //---------------------------------------------------------------------

  void swap (ref ELEMENT a, ref ELEMENT b)
  {
    ELEMENT c;
    c = a;
    a = b;
    b = c;
  }

  //---------------------------------------------------------------------

  // return the index, not of the smallest or of the largest element,
  // but of the element in the middle among the 3 elements :
  // TABLE (A), TABLE (B), TABLE (C).

  int get_medium_index (ELEMENT table[], int a, int b, int c, ref USER_INFO user_info)
  {
    int xi, yi;

    if (compare (table[a], table[b], ref user_info) < 0)
    {
      xi = a;    // smallest
      yi = b;    // largest
    }
    else
    {
      xi = b;    // smallest
      yi = a;    // largest
    }

    // assertion: table (xi) <= table (yi)

    if (compare (table[c], table[xi], ref user_info) < 0)
    {
      // element TABLE (C) is smallest, so TABLE (XI) is in the middle.
      return xi;
    }

    if (compare (table[yi], table[c], ref user_info) < 0)
    {
      // element TABLE (C) is largest, so TABLE (Y1) is in the middle.
      return yi;
    }

    return c;     // element TABLE (C) is in the middle.
  }

  //---------------------------------------------------------------------

  // compute the final place of some element TABLE (MIDDLE) and return MIDDLE.
  // move all elements that are smaller to its left,
  // and all elements that are larger to its right.
  // WARNING: INF and SUP must never leave INDEX_TYPE.

  int partition (ref ELEMENT table[], ref USER_INFO user_info)
  {
    int     middle_index, medium_index, inf, sup;
    ELEMENT temp;

    // compute the index in the middle of the array
    middle_index = (table'length >> 1);

    // compute some index whichs element has an average value
    medium_index = get_medium_index (table,
                                     0,
                                     middle_index,
                                     table'length - 1,
                                     ref user_info);

    // save the average value element
    temp = table[medium_index];

    // working in TABLE (INF .. SUP)
    inf = 0;
    sup = table'length - 1;

    for (;;)
    {
      // increase INF til an element larger or equal to TEMP is found
      while (inf < sup && compare (table[inf], temp, ref user_info) < 0)
        inf++;                           // element smaller than TEMP

      // decrease SUP til an element smaller or equal to TEMP is found
      while (inf < sup && compare (temp, table[sup], ref user_info) < 0)
        sup--;                           // element larger than TEMP

      if (inf == sup)
        break;

      if (compare (temp, table[inf], ref user_info) != 0 || compare (temp, table[sup], ref user_info) != 0)
      {
        // exchange the two values smaller and larger than TEMP
        // and update MEDIUM_INDEX.

        swap (ref table[inf], ref table[sup]);

        if (medium_index == inf)
          medium_index = sup;
        else if (medium_index == sup)
          medium_index = inf;
      }
      else
      {
        // both elements at INF and SUP have the value TEMP
        // we must skip one of them.
        inf++;
      }
    }

    // put the medium index element at its final place
    swap (ref table[medium_index], ref table[inf]);

    // and return its index
    return inf;
  }

  //---------------------------------------------------------------------

  package BubbleS = new BubbleSort4 (ELEMENT => ELEMENT, USER_INFO => USER_INFO, compare => compare);

  //---------------------------------------------------------------------

  void do_quick_sort (ref ELEMENT table[], int nesting, ref USER_INFO user_info)
  {
    int middle;

    if (table'length <= 1)
      return;

    if (table'length <= 10 ||      // BUBBLE_SORT is faster
        nesting > 32)              // prevent stack overflow
    {
      BubbleS.sort (ref table, ref user_info);
    }
    else                           // QUICK_SORT is faster
    {
      middle = partition (ref table, ref user_info);

      if (middle > 1)
      {
        do_quick_sort (ref table[0:middle],
                       nesting+1,
                       ref user_info);
      }

      if (middle < table'length - 2)
      {
        do_quick_sort (ref table[middle+1 : table'length-middle-1],
                       nesting+1,
                       ref user_info);
      }
    }
  }

  //---------------------------------------------------------------------

  public void sort (ref ELEMENT table[], ref USER_INFO user_info)
  {
    do_quick_sort (ref table, 1, ref user_info);
  }

  //---------------------------------------------------------------------

end QuickSort4;

//===================================================================

package body BubbleSort

  public void sort (ref ELEMENT table[])
  {
    int     i, j;
    ELEMENT temp;

    for (i=1; i<table'length; i++)
    {
      for (j=i; j>0; j--)
      {
        if (compare (table[j-1], table[j]) <= 0)
          break;

        temp       = table[j-1];
        table[j-1] = table[j];
        table[j]   = temp;
      }
    }
  }

end BubbleSort;

//===================================================================

package body BubbleSort2

  public void sort (ref ELEMENT table[], COMPARE compare)
  {
    int     i, j;
    ELEMENT temp;

    for (i=1; i<table'length; i++)
    {
      for (j=i; j>0; j--)
      {
        if (compare (table[j-1], table[j]) <= 0)
          break;

        temp       = table[j-1];
        table[j-1] = table[j];
        table[j]   = temp;
      }
    }
  }

end BubbleSort2;

//===================================================================

package body BubbleSort3

  public void sort (ref ELEMENT table[], COMPARE compare, ref USER_INFO user_info)
  {
    int     i, j;
    ELEMENT temp;

    for (i=1; i<table'length; i++)
    {
      for (j=i; j>0; j--)
      {
        if (compare (table[j-1], table[j], ref user_info) <= 0)
          break;

        temp       = table[j-1];
        table[j-1] = table[j];
        table[j]   = temp;
      }
    }
  }

end BubbleSort3;

//===================================================================

package body BubbleSort4

  public void sort (ref ELEMENT table[], ref USER_INFO user_info)
  {
    int     i, j;
    ELEMENT temp;

    for (i=1; i<table'length; i++)
    {
      for (j=i; j>0; j--)
      {
        if (compare (table[j-1], table[j], ref user_info) <= 0)
          break;

        temp       = table[j-1];
        table[j-1] = table[j];
        table[j]   = temp;
      }
    }
  }

end BubbleSort4;

//===================================================================
