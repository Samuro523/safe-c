
// bsearch.c

use ../db, config, fs, dbstruct, cache, badd;

/**************************************************************************/
#begin unsafe
/**************************************************************************/

/* if 'record_not_found', nothing was done,         */
/* else the function returns 'data_record_block_nr' */

/* search mode :                                    */
/* DB_FIRST = 0, DB_LAST = 5                        */
/* DB_EQUAL = 2, DB_SMALLER = 1, DB_LARGER = 4      */
/* DB_EQUAL_OR_SMALLER = 3, DB_EQUAL_OR_LARGER = 6  */

/* the search_mode must NEVER be incorrect */

public
int search_btree
            (ref DB_INFO          p,
             LINK                 table_block_nr,
             int                  index_nr,
             DB_INDEX_DESCRIPTION index_description,
             byte[]               data_record,
             uint2                search_mode,
             out bool             record_not_found,
             out LINK             data_record_block_nr)
{
  uint2                 intern_search_mode = search_mode;
  BYTE                  key[MAX_KEY_SIZE];
  int                   rc;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  LINK                  l, matching_data_block_nr;
  DB_INDEX*             pindex_node;
  bool                  record_exists;
  uint                  insertion_point;
  INDEX_ITEM            item;


// trace ("  search_btree (table_block_nr=%d, index_nr=%d, search_mode=%u  \n", table_block_nr,  index_nr, search_mode);
  
  /* default result is : record was not found */
  record_not_found     = true;
  data_record_block_nr = 0;   /* dummy */


  /* compute the key value */

  switch (intern_search_mode)
  {
    case DB_FIRST:
      clear key;
      intern_search_mode = DB_EQUAL_OR_LARGER;
      break;

    case DB_LAST:
      key = {all => 0xFF};
      intern_search_mode = DB_EQUAL_OR_SMALLER;
      break;

    case DB_EQUAL:
    case DB_SMALLER:
    case DB_LARGER:
    case DB_EQUAL_OR_SMALLER:
    case DB_EQUAL_OR_LARGER:
      compute_key (index_description, data_record, out key);
      break;

    default:
      abort;
  }


  /* let's open the table description block and retrieve the root */

  rc = open_block (out pbuffer, ref p, table_block_nr, false);
  if (rc < 0)
    return rc;

  ptable_description = (DB_TABLE_DESCRIPTION *)&pbuffer->data;

  l = ptable_description->index_root_link[index_nr];   /* root */

  close_block (ref p, pbuffer);



  /* search by descending in the Btree */

  while (l != 0)
  {
    /* open index node 'l' */
    rc = open_block (out pbuffer, ref p, l, false);
    if (rc < 0)
      return rc;

    pindex_node = (DB_INDEX *)&pbuffer->data;

    rc = compute_fils (    index_description,
                           *pindex_node,
                           key,
                       out record_exists,
                       out matching_data_block_nr,
                       out l,
                       out insertion_point);
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    if (record_exists)    /* found */
    {
      if (intern_search_mode == DB_EQUAL            ||
          intern_search_mode == DB_EQUAL_OR_SMALLER ||
          intern_search_mode == DB_EQUAL_OR_LARGER)
      {
        record_not_found     = false;        /* found ! */
        data_record_block_nr = matching_data_block_nr;

        close_block (ref p, pbuffer);
        return 0;
      }
      else if (intern_search_mode == DB_SMALLER)
      {
        /* retrieve the item to the left of insertion point, if any */
        if (insertion_point >= LINK'size+1)  /* there is a smaller item */
        {
          /* use this item in case no better item is found later */
          record_not_found = false;      /* found ! */
          data_record_block_nr'byte = pindex_node->data[insertion_point - LINK'size*(uint)(l!=0) - LINK'size : LINK'size];
        }

        if (l != 0)    /* there are children */
        {
          /* continue searching in child btree 'l' */
        }
        else            /* this is a leaf node : end the search here */
        {
          close_block (ref p, pbuffer);
          return 0;
        }
      }
      else   /* intern_search_mode == DB_LARGER */
      {
        clear item;

        /* retrieve the matching item */
        rc = read_item (    index_description,
                            pindex_node->head,
                            pindex_node->data,
                        ref insertion_point,
                        out item,
                            item);  /* (garbage) */
        if (rc < 0)
        {
          close_block (ref p, pbuffer);
          return rc;
        }

        l = item.child_btree;    /* for search in child btree (if any) */

        /* if there is an item to the right, read it */
        if (insertion_point < pindex_node->head.nb_data_bytes)
        {
          rc = read_item (    index_description,
                              pindex_node->head,
                              pindex_node->data,
                          ref insertion_point,
                          out item,
                              item);
          if (rc < 0)
          {
            close_block (ref p, pbuffer);
            return rc;
          }

          /* use this item in case no better item is found later */
          record_not_found     = false;                          /* found */
          data_record_block_nr = item.data_record_block_nr;
        }

        if (l != 0)  /* there are children */
        {
          /* continue searching in child btree 'l' */
        }
        else            /* this is a leaf node : end the search here */
        {
          close_block (ref p, pbuffer);
          return 0;
        }
      }
    }
    else   /* record was not found */
    {
      if (intern_search_mode == DB_SMALLER || intern_search_mode == DB_EQUAL_OR_SMALLER)
      {
        /* retrieve the item to the left of insertion point, if any */
        if (insertion_point >= LINK'size+1)  /* there is a smaller item */
        {
          /* use this item in case no better item is found later */
          record_not_found = false;      /* found ! */
          data_record_block_nr'byte = pindex_node->data[insertion_point - LINK'size*(uint)(l!=0) - LINK'size : LINK'size];
        }
      }
      else if (intern_search_mode == DB_LARGER || intern_search_mode == DB_EQUAL_OR_LARGER)
      {
        /* if there is an item to the right, read it */
        if (insertion_point < pindex_node->head.nb_data_bytes)
        {
          clear item;

          // retrieved item will not be correct as we didn't read the previous item,
          // but we don't care as we're only interested in the link.

          rc = read_item (    index_description,
                              pindex_node->head,
                              pindex_node->data,
                          ref insertion_point,
                          out item,
                              item);
          if (rc < 0)
          {
            close_block (ref p, pbuffer);
            return rc;
          }

          /* use this item in case no better item is found later */
          record_not_found     = false;                          /* found */
          data_record_block_nr = item.data_record_block_nr;
        }
      }

      if (l != 0L)    /* there are children */
      {
        /* continue searching in child btree 'l' */
      }
      else            /* this is a leaf node : end the search here */
      {
        close_block (ref p, pbuffer);
        return 0;
      }
    }

    close_block (ref p, pbuffer);
  }

  return 0;
}

/**************************************************************************/
#end unsafe
/**************************************************************************/
