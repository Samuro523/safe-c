
/* bintree.c */

use thread;

/************************************************************************/

package body BALANCED_BINARY_TREE

  /************************************************************************/

  struct NODE
  {
    NODE^   ptr[2];   /* pointers to left/right nodes */
    int     bal;      /* -1, 0 or +1 */
    ELEMENT data;     /* user data */
  }

  struct BINARY_TREE
  {
    COMPARE_ELEMENT compare;
    USER_INFO^      user;
    NODE^           root;
  }

  /************************************************************************/

  public void create_btree (out BINARY_TREE  btree,
                            USER_INFO^       user,
                            COMPARE_ELEMENT  compare)
  {
    btree = {compare => compare, user => user, root => null};
   
    thread.flush_cache ();
  }

  /************************************************************************/

  void deallocate_tree (ref NODE^ tree)
  {
    if (tree == null)
      return;

    {
      ref NODE p = tree^;

      deallocate_tree (ref p.ptr[0]);
      deallocate_tree (ref p.ptr[1]);
    }

    free tree;
    tree = null;
  }

  /************************************************************************/

  public void close_btree (ref BINARY_TREE btree)
  {
    deallocate_tree (ref btree.root);
    clear btree;

    thread.flush_cache ();
  }

  /************************************************************************/

  int insert_tree (BINARY_TREE  btree,        /* main fields      */
                   ref NODE^    tree,         /* insertion point  */
                   ELEMENT      data,         /* new data         */
                   out bool     help)         /* true=overflow, false=ok */
  {
    if (tree == null)
    {
      tree = new NODE ' {ptr=> {null,null}, bal => 0, data => data};

      help = true;
      return 0;
    }

    {
      ref NODE p = tree^;
      int      cmp;
      int      rc;
      int      bal0, bal1, ins0, ins1;

      cmp = (btree.compare) (btree.user, data, p.data);

      if (cmp == 0)                   /* key was found */
      {
        help = false;
        return BT_DUPLICATE_KEY;
      }

      if (cmp < 0)
      {
        ins0 = 0;     /* insertion in left subtree */
        ins1 = 1;
        bal0 = +1;
        bal1 = -1;
      }
      else    /* cmp > 0 */
      {
        ins0 = 1;     /* insertion in right subtree */
        ins1 = 0;
        bal0 = -1;
        bal1 = +1;
      }

      rc = insert_tree (btree, ref p.ptr[ins0], data, out help);
      if (rc < 0)
      {
        help = false;
        return rc;
      }

      if (help)
      {
        if (p.bal == bal0)
        {
          p.bal = 0;
          help = false;
        }
        else if (p.bal == 0)
        {
          p.bal = bal1;
        }
        else  /* p.bal == bal1 */
        {
          NODE^    ptr1 = p.ptr[ins0];
          ref NODE p1   = ptr1^;

          if (p1.bal == bal1)
          {
            p.ptr[ins0] = p1.ptr[ins1];
            p1.ptr[ins1] = tree;
            p.bal = 0;
            p1.bal = 0;
            tree = ptr1;
          }
          else
          {
            NODE^    ptr2 = p1.ptr[ins1];
            ref NODE p2   = ptr2^;

            p1.ptr[ins1] = p2.ptr[ins0];
            p2.ptr[ins0] = ptr1;
            p.ptr[ins0]  = p2.ptr[ins1];
            p2.ptr[ins1] = tree;

            if (p2.bal == bal1)
              p.bal = bal0;
            else
              p.bal = 0;

            if (p2.bal == bal0)
              p1.bal = bal1;
            else
              p1.bal = 0;

            tree = ptr2;
            p2.bal = 0;
          }

          help = false;
        }
      }
    }

    return 0;
  }

  /************************************************************************/

  public int insert_btree (ref BINARY_TREE btree,
                           ELEMENT         data)
  {
    int  rc;
    bool help;
    
    thread.fetch_cache ();
    
    assert btree.compare != null;
    rc = insert_tree (btree, ref btree.root, data, out help);
    _unused help;

    thread.flush_cache ();

    return rc;
  }

  /************************************************************************/

  /* this function is called to rebalance the tree */

  void rebalance (ref NODE  p,
                  int       mode,     /* 1 = left, 2 = right */
                  ref bool  help)     /* always true before the call */
  {
    ELEMENT temp;
    int     bal0, bal1, b1, b2, del0, del1;

    assert help;

    if (mode == 1)
    {
      bal0 = -1;
      bal1 = +1;
      del0 = 0;
      del1 = 1;
    }
    else
    {
      bal0 = +1;
      bal1 = -1;
      del0 = 1;
      del1 = 0;
    }

    if (p.bal == bal0)   /* a subtree was taller : it is balanced now */
    {
      p.bal = 0;
      return;           /* rebalance the node above (help still true) */
    }

    if (p.bal == 0)    /* it was balanced : it is slighly unbalanced now */
    {
      p.bal = bal1;
      help = false;
      return;
    }

    /* subtree is already taller : we're out of balance ! */

    {
      NODE^    ptr1 = p.ptr[del1];
      ref NODE p1   = ptr1^;

      b1 = p1.bal;
      if (b1 == bal1 || b1 == 0)
      {
        p.ptr[del1]  = p1.ptr[del1];
        p1.ptr[del1] = p1.ptr[del0];
        p1.ptr[del0] = p.ptr[del0];
        p.ptr[del0]  = ptr1;

        /* exchange data fields of p and p1 */
        temp    = p.data;
        p.data  = p1.data;
        p1.data = temp;

        if (b1 == 0)
        {
          p1.bal = bal1;
          p.bal  = bal0;
          help = false;
        }
        else
        {
          p.bal = 0;
          p1.bal = 0;
        }
      }
      else   /* b1 == bal0 */
      {
        NODE^    ptr2 = p1.ptr[del0];
        ref NODE p2   = ptr2^;

        b2 = p2.bal;

        p1.ptr[del0] = p2.ptr[del1];
        p2.ptr[del1] = p2.ptr[del0];
        p2.ptr[del0] = p.ptr[del0];
        p.ptr[del0] = ptr2;
        p.ptr[del1] = ptr1;

        /* exchange data fields of p and p2 */
        temp    = p.data;
        p.data  = p2.data;
        p2.data = temp;

        if (b2 == bal1)
          p2.bal = bal0;
        else
          p2.bal = 0;

        if (b2 == bal0)
          p1.bal = bal1;
        else
          p1.bal = 0;

        p.bal = 0;
      }
    }
  }

  /************************************************************************/

  void del (ref NODE^  pr,      // node just smaller than p
                NODE^  p,       // node to be deleted
            out bool   help)
  {
    NODE^ q;

    {
      ref NODE r = pr^;

      if (r.ptr[1] != null)
      {
        del (ref r.ptr[1], p, out help);  /* r.ptr[1] changes if (help) */

        if (help)    /* this means also that r.ptr[1] was changed above */
        {
          rebalance (ref r, 2, ref help);
        }

        return;
      }

      /* we arrive at the bottom of the tree (r.ptr[1] is null) */

      /* fill node p with data of node r */
      p^.data = r.data;

      q = r.ptr[0];
    }

    /* delete node r */
    free pr;

    pr = q;
    help = true;       /* node above r must be rewritten */
  }

  /************************************************************************/

  int delete_tree (BINARY_TREE btree,    /* main fields       */
                   ref NODE^   tree,     /* deletion point    */
                   ELEMENT     data,     /* data              */
                   out bool    help)     /* true=underflow, false=ok */
  {
    int   rc, cmp;
    NODE^ up;

    if (tree == null)
    {
      help = false;
      return BT_KEY_NOT_FOUND;
    }

    /* load node 'tree' into 'p' */

    {
      cmp = (btree.compare) (btree.user, data, tree^.data);

      if (cmp == 0)    /* found node to be deleted */
      {
        if (tree^.ptr[1] == null)      /* right subtree is empty */
        {
          up = tree^.ptr[0];
          free tree;
          tree = up;

          help = true;     /* node above must be updated */
        }
        else if (tree^.ptr[0] == null) /* left subtree is empty */
        {
          up = tree^.ptr[1];
          free tree;
          tree = up;

          help = true;     /* node above must be updated */
        }
        else     /* both subtrees exist */
        {
          del (ref tree^.ptr[0], tree, out help);   /* this always changes 'tree' */

          if (help)  /* this means also that tree^.ptr[0] was changed */
          {
            rebalance (ref tree^, 1, ref help);
          }
        }

        return 0;
      }

      if (cmp < 0)    /* delete in left subtree */
      {
        rc = delete_tree (btree, ref tree^.ptr[0], data, out help);
        if (rc < 0)
          return rc;

        if (help)   /* this means also that tree^.ptr[0] was changed above */
        {
          rebalance (ref tree^, 1, ref help);
        }
      }
      else /* cmp > 0 */    /* delete in ptr[1] subtree */
      {
        rc = delete_tree (btree, ref tree^.ptr[1], data, out help);
        if (rc < 0)
          return rc;

        if (help)
        {
          rebalance (ref tree^, 2, ref help);
        }
      }
    }

    return 0;
  }

  /************************************************************************/

  public int delete_btree (ref BINARY_TREE btree,
                           ELEMENT         data)
  {
    bool help;
    int  rc;

    thread.fetch_cache ();
  
    assert btree.compare != null;
    rc = delete_tree (btree, ref btree.root, data, out help);
    _unused help;

    thread.flush_cache ();

    return rc;
  }

  /************************************************************************/

  int read_then_delete_tree (    BINARY_TREE btree,    /* main fields       */
                             ref NODE^       tree,     /* deletion point    */
                             ref ELEMENT     data,     /* data              */
                             out bool        help)     /* true=underflow, false=ok */
  {
    int   rc, cmp;
    NODE^ up;

    if (tree == null)
    {
      help = false;
      return BT_KEY_NOT_FOUND;
    }

    /* load node 'tree' into 'p' */

    {
      cmp = (btree.compare) (btree.user, data, tree^.data);

      if (cmp == 0)    /* found node to be deleted */
      {
        data = tree^.data;   // read it

        if (tree^.ptr[1] == null)      /* right subtree is empty */
        {
          up = tree^.ptr[0];
          free tree;
          tree = up;

          help = true;     /* node above must be updated */
        }
        else if (tree^.ptr[0] == null) /* left subtree is empty */
        {
          up = tree^.ptr[1];
          free tree;
          tree = up;

          help = true;     /* node above must be updated */
        }
        else     /* both subtrees exist */
        {
          del (ref tree^.ptr[0], tree, out help);   /* this always changes 'tree' */

          if (help)  /* this means also that tree^.ptr[0] was changed */
          {
            rebalance (ref tree^, 1, ref help);
          }
        }

        return 0;
      }

      if (cmp < 0)    /* delete in left subtree */
      {
        rc = read_then_delete_tree (btree, ref tree^.ptr[0], ref data, out help);
        if (rc < 0)
          return rc;

        if (help)   /* this means also that tree^.ptr[0] was changed above */
        {
          rebalance (ref tree^, 1, ref help);
        }
      }
      else /* cmp > 0 */    /* delete in ptr[1] subtree */
      {
        rc = read_then_delete_tree (btree, ref tree^.ptr[1], ref data, out help);
        if (rc < 0)
          return rc;

        if (help)
        {
          rebalance (ref tree^, 2, ref help);
        }
      }
    }

    return 0;
  }

  /************************************************************************/

  public int read_then_delete_btree (ref BINARY_TREE btree,
                                     ref ELEMENT     data)
  {
    bool help;
    int  rc;

    thread.fetch_cache ();
   
    assert btree.compare != null;
    rc = read_then_delete_tree (btree, ref btree.root, ref data, out help);
    _unused help;

    thread.flush_cache ();
 
    return rc;
  }

  /************************************************************************/

  public int update_btree (ref BINARY_TREE  btree,
                           ELEMENT          data)
  {
    NODE^ tree;
    int   cmp;

    thread.fetch_cache ();

    assert btree.compare != null;
    tree = btree.root;

    for (;;)
    {
      if (tree == null)
        return BT_KEY_NOT_FOUND;

      cmp = (btree.compare) (btree.user, data, tree^.data);
      if (cmp == 0)                    /* found node to be modified */
        break;

      if (cmp < 0)                     /* modify in left subtree */
        tree = tree^.ptr[0];
      else                             /* modify in right subtree */
        tree = tree^.ptr[1];
    }

    /* modify the data of node p */

    tree^.data = data;

    thread.flush_cache ();

    return 0;
  }

  /************************************************************************/

  /* combines a read and an update operation */
  /* returns 0 if OK or a negative error (BT_KEY_NOT_FOUND) */

  public int read_then_update_btree (ref BINARY_TREE  btree,
                                         ELEMENT      data,
                                     out ELEMENT      data_before_update)
  {
    NODE^ tree;
    int   cmp;

    thread.fetch_cache ();

    assert btree.compare != null;
    tree = btree.root;

    for (;;)
    {
      if (tree == null)
      {
        clear data_before_update;
        return BT_KEY_NOT_FOUND;
      }
      
      cmp = (btree.compare) (btree.user, data, tree^.data);
      if (cmp == 0)                    /* found node to be modified */
        break;

      if (cmp < 0)                     /* modify in left subtree */
        tree = tree^.ptr[0];
      else                             /* modify in right subtree */
        tree = tree^.ptr[1];
    }

    /* read then modify the data of node p */

    data_before_update = tree^.data;
    tree^.data = data;

    thread.flush_cache ();

    return 0;
  }                                     

  /************************************************************************/

  public int retrieve_btree (    BINARY_TREE btree,
                             ref ELEMENT     data,
                                 ushort      retrieve_mode)
  {
    NODE^ tree, result;
    int   cmp;

    assert (retrieve_mode <= 6);

    thread.fetch_cache ();

    assert btree.compare != null;
    tree = btree.root;

    result = null;

    while (tree != null)
    {
      ref NODE p = tree^;    /* load node 'tree' into 'p' */
      
      if (retrieve_mode == BT_FIRST || retrieve_mode == BT_LAST)
      {
        result = tree;  /* save in case it's a leaf */

        if (retrieve_mode == BT_FIRST)
          tree = p.ptr[0];
        else
          tree = p.ptr[1];

        continue;   /* continue til reaching a leaf node */
      }

      cmp = (btree.compare) (btree.user, data, tree^.data);

      if (cmp == 0)                    /* found equal node */
      {
        if (retrieve_mode == BT_EQUAL            ||
            retrieve_mode == BT_EQUAL_OR_SMALLER ||
            retrieve_mode == BT_EQUAL_OR_LARGER)
        {
          result = tree;
          break;                            /* the search ends here */
        }
        else
        {
          if (retrieve_mode == BT_SMALLER)
            tree = p.ptr[0];
          else  /* retrieve_mode == BT_LARGER */
            tree = p.ptr[1];
        }
      }
      else
      {
        /* test if the node matches the criteria */

        switch (retrieve_mode)
        {
          case BT_EQUAL:   /* do nothing */
            break;

          case BT_EQUAL_OR_SMALLER:
          case BT_SMALLER:
            if (cmp > 0)     /* provided value is larger than node data */
              result = tree;
            break;

          case BT_EQUAL_OR_LARGER:
          case BT_LARGER:
            if (cmp < 0)     /* provided value is smaller than node data */
              result = tree;
            break;

          default:
            abort;
        }

        /* try to find a node with a closer value */
        if (cmp < 0)
          tree = tree^.ptr[0];
        else
          tree = tree^.ptr[1];
      }
    }

    if (result == null)
      return BT_KEY_NOT_FOUND;

    /* retrieve result data */
    data = result^.data;

    return 0;
  }

  /************************************************************************/

  int intern_traverse_tree (BINARY_TREE     btree,
                            NODE^           tree,
                            OPERATE_ELEMENT operate,
                            int             index)     /* 0 or 1 */
  {
    if (tree == null)
      return 0;

    {
      ref NODE p = tree^;    /* load node 'tree' into 'p' */
      int      rc;

      rc = intern_traverse_tree (btree, p.ptr[index], operate, index);
      if (rc < 0)
        return rc;

      rc = (operate) (btree.user, p.data);
      if (rc != 0)
        return BT_ABORT;

      rc = intern_traverse_tree (btree, p.ptr[1-index], operate, index);
      if (rc < 0)
        return rc;
    }

    return 0;
  }

  /************************************************************************/

  public int traverse_btree (BINARY_TREE     btree,
                             OPERATE_ELEMENT operate,
                             short           order)     /* -1 or +1 */
  {
    int index;

    assert (order == -1 || order == +1);

    thread.fetch_cache ();

    assert btree.compare != null;
    
    index = (order == +1) ? 0 : 1;

    return intern_traverse_tree (btree, btree.root, operate, index);
  }

  /************************************************************************/

end BALANCED_BINARY_TREE;


//===============================================================================================


package body BALANCED_BINARY_TREE_OF_POINTERS

  /************************************************************************/

  struct NODE
  {
    NODE^       ptr[2];   /* pointers to left/right nodes */
    int         bal;      /* -1, 0 or +1 */
    PTR_ELEMENT data;     /* user data */
  }

  struct BINARY_TREE
  {
    COMPARE_ELEMENT compare;
    USER_INFO^      user;
    NODE^           root;
  }

  /************************************************************************/

  public void create_btree (out BINARY_TREE  btree,
                            USER_INFO^       user,
                            COMPARE_ELEMENT  compare)
  {
    btree = {compare => compare, user => user, root => null};

    thread.flush_cache ();
  }

  /************************************************************************/

  void deallocate_tree (ref NODE^ tree)
  {
    if (tree == null)
      return;

    {
      ref NODE p = tree^;

      deallocate_tree (ref p.ptr[0]);
      deallocate_tree (ref p.ptr[1]);
    }

    free tree;
    tree = null;
  }

  /************************************************************************/

  void deallocate_tree2 (NODE^           tree,
                         USER_INFO^      user,
                         OPERATE_ELEMENT operate)
  {
    if (tree == null)
      return;

    {
      ref NODE p = tree^;

      operate (user, p.data);

      deallocate_tree2 (p.ptr[0], user, operate);
      deallocate_tree2 (p.ptr[1], user, operate);
    }

    free tree;
  }

  /************************************************************************/

  public int free_element (USER_INFO^ user, PTR_ELEMENT pdata)
  {
    _unused user;
    free pdata;
    thread.flush_cache ();
    return 0;
  }

  /************************************************************************/

  public void close_btree (ref BINARY_TREE btree,
                           OPERATE_ELEMENT operate = null)
  {
    if (operate == null)
      deallocate_tree (ref btree.root);
    else
      deallocate_tree2 (btree.root, btree.user, operate);
    clear btree;
    thread.flush_cache ();
  }

  /************************************************************************/

  int insert_tree (BINARY_TREE  btree,        /* main fields      */
                   ref NODE^    tree,         /* insertion point  */
                   PTR_ELEMENT  pdata,        /* new data         */
                   out bool     help)         /* true=overflow, false=ok */
  {
    if (tree == null)
    {
      tree = new NODE ' {ptr=> {null,null}, bal => 0, data => pdata};

      help = true;
      return 0;
    }

    {
      ref NODE p = tree^;
      int      cmp;
      int      rc;
      int      bal0, bal1, ins0, ins1;

      cmp = (btree.compare) (btree.user, pdata^, p.data^);

      if (cmp == 0)                   /* key was found */
      {
        help = false;
        return BT_DUPLICATE_KEY;
      }

      if (cmp < 0)
      {
        ins0 = 0;     /* insertion in left subtree */
        ins1 = 1;
        bal0 = +1;
        bal1 = -1;
      }
      else    /* cmp > 0 */
      {
        ins0 = 1;     /* insertion in right subtree */
        ins1 = 0;
        bal0 = -1;
        bal1 = +1;
      }

      rc = insert_tree (btree, ref p.ptr[ins0], pdata, out help);
      if (rc < 0)
      {
        help = false;
        return rc;
      }

      if (help)
      {
        if (p.bal == bal0)
        {
          p.bal = 0;
          help = false;
        }
        else if (p.bal == 0)
        {
          p.bal = bal1;
        }
        else  /* p.bal == bal1 */
        {
          NODE^    ptr1 = p.ptr[ins0];
          ref NODE p1   = ptr1^;

          if (p1.bal == bal1)
          {
            p.ptr[ins0] = p1.ptr[ins1];
            p1.ptr[ins1] = tree;
            p.bal = 0;
            p1.bal = 0;
            tree = ptr1;
          }
          else
          {
            NODE^    ptr2 = p1.ptr[ins1];
            ref NODE p2   = ptr2^;

            p1.ptr[ins1] = p2.ptr[ins0];
            p2.ptr[ins0] = ptr1;
            p.ptr[ins0]  = p2.ptr[ins1];
            p2.ptr[ins1] = tree;

            if (p2.bal == bal1)
              p.bal = bal0;
            else
              p.bal = 0;

            if (p2.bal == bal0)
              p1.bal = bal1;
            else
              p1.bal = 0;

            tree = ptr2;
            p2.bal = 0;
          }

          help = false;
        }
      }
    }

    return 0;
  }

  /************************************************************************/

  public int insert_btree (ref BINARY_TREE btree,
                           PTR_ELEMENT     pdata)
  {
    int  rc;
    bool help;

    thread.fetch_cache ();
   
    assert btree.compare != null;
    rc = insert_tree (btree, ref btree.root, pdata, out help);
    _unused help;

    thread.flush_cache ();

    return rc;
  }

  /************************************************************************/

  /* this function is called to rebalance the tree */

  void rebalance (ref NODE  p,
                      int   mode,     /* 1 = left, 2 = right */
                  ref bool  help)     /* always true before the call */
  {
    PTR_ELEMENT temp;
    int         bal0, bal1, b1, b2, del0, del1;

    assert help;

    if (mode == 1)
    {
      bal0 = -1;
      bal1 = +1;
      del0 = 0;
      del1 = 1;
    }
    else
    {
      bal0 = +1;
      bal1 = -1;
      del0 = 1;
      del1 = 0;
    }

    if (p.bal == bal0)   /* a subtree was taller : it is balanced now */
    {
      p.bal = 0;
      return;           /* rebalance the node above (help still true) */
    }

    if (p.bal == 0)    /* it was balanced : it is slighly unbalanced now */
    {
      p.bal = bal1;
      help = false;
      return;
    }

    /* subtree is already taller : we're out of balance ! */

    {
      NODE^    ptr1 = p.ptr[del1];
      ref NODE p1   = ptr1^;

      b1 = p1.bal;
      if (b1 == bal1 || b1 == 0)
      {
        p.ptr[del1]  = p1.ptr[del1];
        p1.ptr[del1] = p1.ptr[del0];
        p1.ptr[del0] = p.ptr[del0];
        p.ptr[del0]  = ptr1;

        /* exchange data fields of p and p1 */
        temp    = p.data;
        p.data  = p1.data;
        p1.data = temp;

        if (b1 == 0)
        {
          p1.bal = bal1;
          p.bal  = bal0;
          help = false;
        }
        else
        {
          p.bal = 0;
          p1.bal = 0;
        }
      }
      else   /* b1 == bal0 */
      {
        NODE^    ptr2 = p1.ptr[del0];
        ref NODE p2   = ptr2^;

        b2 = p2.bal;

        p1.ptr[del0] = p2.ptr[del1];
        p2.ptr[del1] = p2.ptr[del0];
        p2.ptr[del0] = p.ptr[del0];
        p.ptr[del0] = ptr2;
        p.ptr[del1] = ptr1;

        /* exchange data fields of p and p2 */
        temp    = p.data;
        p.data  = p2.data;
        p2.data = temp;

        if (b2 == bal1)
          p2.bal = bal0;
        else
          p2.bal = 0;

        if (b2 == bal0)
          p1.bal = bal1;
        else
          p1.bal = 0;

        p.bal = 0;
      }
    }
  }

  /************************************************************************/

  void del (ref NODE^  pr,      // node just smaller than p
                NODE^  p,       // node to be deleted
            out bool   help)
  {
    NODE^ q;

    {
      ref NODE r = pr^;

      if (r.ptr[1] != null)
      {
        del (ref r.ptr[1], p, out help);  /* r.ptr[1] changes if (help) */

        if (help)    /* this means also that r.ptr[1] was changed above */
        {
          rebalance (ref r, 2, ref help);
        }

        return;
      }

      /* we arrive at the bottom of the tree (r.ptr[1] is null) */

      /* fill node p with data of node r */
      p^.data = r.data;

      q = r.ptr[0];
    }

    /* delete node r */
    free pr;

    pr = q;
    help = true;       /* node above r must be rewritten */
  }

  /************************************************************************/

  int delete_tree (BINARY_TREE     btree,    /* main fields       */
                   ref NODE^       tree,     /* deletion point    */
                   ELEMENT         data,     /* data              */
                   out PTR_ELEMENT pdata,
                   out bool        help)     /* true=underflow, false=ok */
  {
    int   rc, cmp;
    NODE^ up;

    if (tree == null)
    {
      help = false;
      pdata = null;
      return BT_KEY_NOT_FOUND;
    }

    /* load node 'tree' into 'p' */

    {
      cmp = (btree.compare) (btree.user, data, tree^.data^);

      if (cmp == 0)    /* found node to be deleted */
      {
        pdata = tree^.data;

        if (tree^.ptr[1] == null)      /* right subtree is empty */
        {
          up = tree^.ptr[0];
          free tree;
          tree = up;

          help = true;     /* node above must be updated */
        }
        else if (tree^.ptr[0] == null) /* left subtree is empty */
        {
          up = tree^.ptr[1];
          free tree;
          tree = up;

          help = true;     /* node above must be updated */
        }
        else     /* both subtrees exist */
        {
          del (ref tree^.ptr[0], tree, out help);   /* this always changes 'tree' */

          if (help)  /* this means also that tree^.ptr[0] was changed */
          {
            rebalance (ref tree^, 1, ref help);
          }
        }

        return 0;
      }

      if (cmp < 0)    /* delete in left subtree */
      {
        rc = delete_tree (btree, ref tree^.ptr[0], data, out pdata, out help);
        if (rc < 0)
          return rc;

        if (help)   /* this means also that tree^.ptr[0] was changed above */
        {
          rebalance (ref tree^, 1, ref help);
        }
      }
      else /* cmp > 0 */    /* delete in ptr[1] subtree */
      {
        rc = delete_tree (btree, ref tree^.ptr[1], data, out pdata, out help);
        if (rc < 0)
          return rc;

        if (help)
        {
          rebalance (ref tree^, 2, ref help);
        }
      }
    }

    return 0;
  }

  /************************************************************************/

  public int delete_btree (ref BINARY_TREE  btree,
                           ELEMENT          data,
                           out PTR_ELEMENT  pdata)
  {
    bool help;
    int  rc;

    thread.fetch_cache ();

    assert btree.compare != null;
    rc = delete_tree (btree, ref btree.root, data, out pdata, out help);
    _unused help;

    thread.flush_cache ();
 
    return rc;
  }

  /************************************************************************/

  public int retrieve_btree (    BINARY_TREE btree,
                                 ELEMENT     data,
                             out PTR_ELEMENT pdata,
                                 ushort      retrieve_mode)
  {
    NODE^ tree, result;
    int   cmp;

    assert (retrieve_mode <= 6);

    thread.fetch_cache ();

    assert btree.compare != null;
    tree = btree.root;

    result = null;

    while (tree != null)
    {
      ref NODE p = tree^;    /* load node 'tree' into 'p' */
      
      if (retrieve_mode == BT_FIRST || retrieve_mode == BT_LAST)
      {
        result = tree;  /* save in case it's a leaf */
        
        if (retrieve_mode == BT_FIRST)
          tree = p.ptr[0];
        else
          tree = p.ptr[1];

        continue;   /* continue til reaching a leaf node */
      }

      cmp = (btree.compare) (btree.user, data, tree^.data^);

      if (cmp == 0)                    /* found equal node */
      {
        if (retrieve_mode == BT_EQUAL            ||
            retrieve_mode == BT_EQUAL_OR_SMALLER ||
            retrieve_mode == BT_EQUAL_OR_LARGER)
        {
          result = tree;
          break;                            /* the search ends here */
        }
        else
        {
          if (retrieve_mode == BT_SMALLER)
            tree = p.ptr[0];
          else  /* retrieve_mode == BT_LARGER */
            tree = p.ptr[1];
        }
      }
      else
      {
        /* test if the node matches the criteria */

        switch (retrieve_mode)
        {
          case BT_EQUAL:   /* do nothing */
            break;

          case BT_EQUAL_OR_SMALLER:
          case BT_SMALLER:
            if (cmp > 0)     /* provided value is larger than node data */
              result = tree;
            break;

          case BT_EQUAL_OR_LARGER:
          case BT_LARGER:
            if (cmp < 0)     /* provided value is smaller than node data */
              result = tree;
            break;

          default:
            abort;
        }

        /* try to find a node with a closer value */
        if (cmp < 0)
          tree = tree^.ptr[0];
        else
          tree = tree^.ptr[1];
      }
    }

    if (result == null)
    {
      pdata = null;
      return BT_KEY_NOT_FOUND;
    }

    /* retrieve result data */
    pdata = result^.data;

    return 0;
  }

  /************************************************************************/

  int intern_traverse_tree (BINARY_TREE     btree,
                            NODE^           tree,
                            OPERATE_ELEMENT operate,
                            int             index)     /* 0 or 1 */
  {
    if (tree == null)
      return 0;

    {
      ref NODE p = tree^;    /* load node 'tree' into 'p' */
      int      rc;

      rc = intern_traverse_tree (btree, p.ptr[index], operate, index);
      if (rc < 0)
        return rc;

      rc = (operate) (btree.user, p.data);
      if (rc != 0)
        return BT_ABORT;

      rc = intern_traverse_tree (btree, p.ptr[1-index], operate, index);
      if (rc < 0)
        return rc;
    }

    return 0;
  }

  /************************************************************************/

  public int traverse_btree (BINARY_TREE     btree,
                             OPERATE_ELEMENT operate,
                             short           order)     /* -1 or +1 */
  {
    int index;

    assert (order == -1 || order == +1);

    thread.fetch_cache ();

    assert btree.compare != null;

    index = (order == +1) ? 0 : 1;

    return intern_traverse_tree (btree, btree.root, operate, index);
  }

  /************************************************************************/

end BALANCED_BINARY_TREE_OF_POINTERS;
