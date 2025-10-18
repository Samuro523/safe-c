
/* bintree.h : balanced binary trees (AVL) */

//--------------------------------------------------------------------------

generic <ELEMENT, USER_INFO>
package BALANCED_BINARY_TREE

  /************************************************************************/

  /* extract and compare key part of user data. */
  /* returns -1 if (data1) < (data2),           */
  /*          0 if equal,                       */
  /*         +1 if (data1) > (data2),           */

  typedef int COMPARE_ELEMENT (USER_INFO^ user,
                               ELEMENT    data1,
                               ELEMENT    data2);

  /************************************************************************/

  struct BINARY_TREE;     /* this structure is private */

  /************************************************************************/

  /* initialize a binary tree structure */

  void create_btree (out BINARY_TREE     btree,
                         USER_INFO^      user,
                         COMPARE_ELEMENT compare);

  /************************************************************************/

  /* deallocate the tree */

  void close_btree (ref BINARY_TREE btree);

  /************************************************************************/

  /* insert new data in binary tree */
  /* returns 0 if OK or a negative error (BT_DUPLICATE_KEY) */

  int insert_btree (ref BINARY_TREE btree,
                        ELEMENT     data);

  /************************************************************************/

  /* delete data in binary tree */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  int delete_btree (ref BINARY_TREE  btree,
                        ELEMENT      data);

  /************************************************************************/

  /* modify data in binary tree.                                 */
  /* the btree must already contain data with the same key part. */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  int update_btree (ref BINARY_TREE  btree,
                        ELEMENT      data);

  /************************************************************************/

  /* retrieve data in binary tree.                                       */
  /* 'data' must contain enough information to allow key comparison.     */
  /* assertion: the retrieve_mode is one of the constants defined below. */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  int retrieve_btree (    BINARY_TREE  btree,
                      ref ELEMENT      data,
                          ushort       retrieve_mode);

  /************************************************************************/

  /* combines a read and a delete operation */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  int read_then_delete_btree (ref BINARY_TREE btree,
                              ref ELEMENT     data);

  /************************************************************************/

  /* combines a read and an update operation */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  int read_then_update_btree (ref BINARY_TREE  btree,
                                  ELEMENT      data,
                              out ELEMENT      data_before_update);

  /************************************************************************/

  /* this function is called once for each traversed node. */
  /* the user function must return 0 to continue. */
  /* Any other value terminates the function 'traverse_tree' with BT_ABORT. */

  typedef int OPERATE_ELEMENT (USER_INFO^ user,
                               ELEMENT    data);

  /************************************************************************/

  /* traverse btree in ascending (+1) or descending (-1) order.  */
  /* returns 0 if OK or a negative error.                        */

  int traverse_btree (    BINARY_TREE     btree,
                          OPERATE_ELEMENT operate,
                          short           order);     /* -1 or +1 */

  /************************************************************************/

end BALANCED_BINARY_TREE;

//--------------------------------------------------------------------------

generic <ELEMENT, USER_INFO>
package BALANCED_BINARY_TREE_OF_POINTERS

  /************************************************************************/

  /* extract and compare key part of user data. */
  /* returns -1 if (data1) < (data2),           */
  /*          0 if equal,                       */
  /*         +1 if (data1) > (data2),           */

  typedef int COMPARE_ELEMENT (USER_INFO^ user,
                               ELEMENT    data1,
                               ELEMENT    data2);

  /************************************************************************/

  typedef ELEMENT^ PTR_ELEMENT;

  /************************************************************************/

  struct BINARY_TREE;     /* this structure is private */

  /************************************************************************/

  /* initialize a binary tree structure */

  void create_btree (out BINARY_TREE      btree,
                         USER_INFO^       user,
                         COMPARE_ELEMENT  compare);

  /************************************************************************/

  /* this function is called once for each traversed node.  */
  /* the user must return 0 to continue. Any other value    */
  /* terminates the function 'traverse_tree' with BT_ABORT. */

  typedef int OPERATE_ELEMENT (USER_INFO^ user, PTR_ELEMENT pdata);

  /************************************************************************/

  /* this function can be passed to close_btree() to free all nodes; */
  /* it just frees pdata */
  int free_element (USER_INFO^ user, PTR_ELEMENT pdata);

  /************************************************************************/

  /* deallocate the tree */
  /* operate() can be used to terminate each element, */
  /* for example the function free_element() can be passed to free all elements. */

  void close_btree (ref BINARY_TREE     btree,
                        OPERATE_ELEMENT operate = null);

  /************************************************************************/

  /* insert ptr to new data in binary tree */
  /* the ELEMENT must have been allocated before calling the function */
  /* returns 0 if OK or a negative error (BT_DUPLICATE_KEY) */

  int insert_btree (ref BINARY_TREE btree,
                        PTR_ELEMENT pdata);

  /************************************************************************/

  /* delete ptr to data in binary tree */
  /* the node itself is not freed : pdata is returned so the caller can do it */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  int delete_btree (ref BINARY_TREE  btree,
                        ELEMENT      data,
                    out PTR_ELEMENT  pdata);

  /************************************************************************/

  /* retrieve ptr to data in binary tree.                                */
  /* 'data' must contain enough information to allow key comparison.     */
  /* assertion: the retrieve_mode is one of the constants defined below. */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  int retrieve_btree (    BINARY_TREE  btree,
                          ELEMENT      data,
                      out PTR_ELEMENT  pdata,
                          ushort       retrieve_mode);

  /************************************************************************/

  /* traverse btree in ascending (+1) or descending (-1) order.  */
  /* returns 0 if OK or a negative error.                        */

  int traverse_btree (    BINARY_TREE     btree,
                          OPERATE_ELEMENT operate,
                          short           order);     /* -1 or +1 */

  /************************************************************************/

end BALANCED_BINARY_TREE_OF_POINTERS;

//--------------------------------------------------------------------------

/* absolute retrieve modes (do not use the data value) */
const ushort BT_FIRST            = 0;
const ushort BT_LAST             = 5;

/* relative retrieve modes (use the data value) */
const ushort BT_EQUAL            = 2;
const ushort BT_SMALLER          = 1;
const ushort BT_LARGER           = 4;
const ushort BT_EQUAL_OR_SMALLER = 3;
const ushort BT_EQUAL_OR_LARGER  = 6;

//--------------------------------------------------------------------------

/* possible error codes: */
const int BT_KEY_NOT_FOUND    = (-1);  /* data with this key was not found  */
const int BT_DUPLICATE_KEY    = (-2);  /* data with this key already exists */
const int BT_ABORT            = (-3);  /* user aborted traverse_btree       */

//--------------------------------------------------------------------------
