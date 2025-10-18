
// sorting.h : sort an array or array slice.

//----------------------------------------------------------------
// HeapSort
//----------------------------------------------------------------
// stable performance : always N * LOG2(N).
//----------------------------------------------------------------

generic <ELEMENT>

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  int compare (ELEMENT a, ELEMENT b);

package HeapSort

  void sort (ref ELEMENT table[]);

end HeapSort;



//----------------------------------------------------------------
// QuickSort
//----------------------------------------------------------------
// unstable performance:
//       N           (for almost sorted arrays)
//       N * LOG2(N) (most cases)
//       N^2         (in case the array contains some bad data)
//
// - don't use this method for time-critical applications.
// - good cache locality (well suited for very large arrays)
// - limits recursion to 32 calls, using bubble-sort after that.
//----------------------------------------------------------------

generic <ELEMENT>

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  int compare (ELEMENT a, ELEMENT b);

package QuickSort

  void sort (ref ELEMENT table[]);

end QuickSort;



//----------------------------------------------------------------
// BubbleSort
//----------------------------------------------------------------
// very slow method (N**2), useful only for small arrays (<= 10)
//----------------------------------------------------------------

generic <ELEMENT>

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  int compare (ELEMENT a, ELEMENT b);

package BubbleSort

  void sort (ref ELEMENT table[]);

end BubbleSort;



//=================================================================

// alternative forms 2 of the generic packages :

generic <ELEMENT>
package HeapSort2

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  typedef int COMPARE (ELEMENT a, ELEMENT b);

  void sort (ref ELEMENT table[], COMPARE compare);

end HeapSort2;


generic <ELEMENT>
package QuickSort2

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  typedef int COMPARE (ELEMENT a, ELEMENT b);

  void sort (ref ELEMENT table[], COMPARE compare);

end QuickSort2;


generic <ELEMENT>
package BubbleSort2

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  typedef int COMPARE (ELEMENT a, ELEMENT b);

  void sort (ref ELEMENT table[], COMPARE compare);

end BubbleSort2;

//=================================================================

// alternative forms 3 of the generic packages :

generic <ELEMENT, USER_INFO>
package HeapSort3

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  typedef int COMPARE (ELEMENT a, ELEMENT b, ref USER_INFO user_info);

  void sort (ref ELEMENT table[], COMPARE compare, ref USER_INFO user_info);

end HeapSort3;


generic <ELEMENT, USER_INFO>
package QuickSort3

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  typedef int COMPARE (ELEMENT a, ELEMENT b, ref USER_INFO user_info);

  void sort (ref ELEMENT table[], COMPARE compare, ref USER_INFO user_info);

end QuickSort3;


generic <ELEMENT, USER_INFO>
package BubbleSort3

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  typedef int COMPARE (ELEMENT a, ELEMENT b, ref USER_INFO user_info);

  void sort (ref ELEMENT table[], COMPARE compare, ref USER_INFO user_info);

end BubbleSort3;

//=================================================================

// alternative forms 4 of the generic packages :

generic <ELEMENT, USER_INFO>

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  int compare (ELEMENT a, ELEMENT b, ref USER_INFO user_info);
  
package HeapSort4

  void sort (ref ELEMENT table[], ref USER_INFO user_info);

end HeapSort4;


generic <ELEMENT, USER_INFO>

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  int compare (ELEMENT a, ELEMENT b, ref USER_INFO user_info);
  
package QuickSort4

  void sort (ref ELEMENT table[], ref USER_INFO user_info);

end QuickSort4;


generic <ELEMENT, USER_INFO>

  // user-written function that must return -1 if a<b, 0 if a==b, +1 if a>b.
  int compare (ELEMENT a, ELEMENT b, ref USER_INFO user_info);
  
package BubbleSort4

  void sort (ref ELEMENT table[], ref USER_INFO user_info);

end BubbleSort4;

//=================================================================
