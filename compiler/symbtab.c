
// symbtab.c

from std use bintree;

package body SymbolTable

  struct MY_INFO
  {
    COMPARE  comp;
    OPERATE  oper;
  }

  package P = new BALANCED_BINARY_TREE (ELEMENT => ELEMENT, USER_INFO => MY_INFO);

  struct SYMBOL_TABLE
  {
    P.BINARY_TREE btree;
    MY_INFO^      puser;
  }

  int COMPARE_ELEMENT (MY_INFO^ my_info, ELEMENT data1, ELEMENT data2)
  {
    return my_info^.comp (data1, data2);
  }

  public void create (out SYMBOL_TABLE st, COMPARE compare)
  {
    clear st;
    st.puser = new MY_INFO ' {compare, null};   // will never be freed, but that's ok
    create_btree (out st.btree, st.puser, COMPARE_ELEMENT);
  }

  public void close (ref SYMBOL_TABLE st)
  {
     close_btree (ref st.btree);
     free st.puser;
     clear st;
  }

  // returns true if OK
  public bool insert (ref SYMBOL_TABLE st, ELEMENT data)
  {
    int rc = insert_btree (ref st.btree, data);
    assert rc == 0 || rc == BT_DUPLICATE_KEY;
    return rc == 0;
  }

  // returns true if OK
  public bool update (ref SYMBOL_TABLE st, ELEMENT data)
  {
    int rc = update_btree (ref st.btree, data);
    assert rc == 0 || rc == BT_KEY_NOT_FOUND;
    return rc == 0;
  }

  // returns true if OK
  public bool retrieve (ref SYMBOL_TABLE st, ELEMENT key, out ELEMENT data)
  {
    ELEMENT e = key;
    int rc = retrieve_btree (st.btree, ref e, BT_EQUAL);
    assert rc == 0 || rc == BT_KEY_NOT_FOUND;

    if (rc == 0)
    {
      data = e;
      return true;
    }
    else
    {
      clear data;
      return false;
    }
  }

  int my_operate (MY_INFO^ my_info, ELEMENT data)
  {
    my_info^.oper (data);
    return 0;
  }                               
                               
  public void traverse (ref SYMBOL_TABLE  st,
                            OPERATE       operate,
                            short         order = +1)     /* -1 descending, or +1 ascending */
  {
    st.puser^.oper = operate;
    assert traverse_btree (st.btree, my_operate, order) == 0;
  }

end SymbolTable;
