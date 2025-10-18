
// index2.c : build and delete indexes

use ../db, config, fs, dbstruct, cache, badd, super;

/**************************************************************************/
#begin unsafe
/**************************************************************************/

public int recursive_delete_index
            (ref DB_INFO              p,
                 LINK                 btree_root,
                 DB_INDEX_DESCRIPTION index_description,
                 bool                 deallocate_data_records,
                 uint                 nb_blocks_per_data_record)
{
  int         rc;
  BUFFER*     pbuffer;
  DB_INDEX*   pindex_node;
  INDEX_ITEM  item;
  uint        offset;

  if (btree_root == 0)
    return 0;

  /* open index node */
  rc = open_block (out pbuffer, ref p, btree_root, false);
  if (rc < 0)
    return rc;

  pindex_node = (DB_INDEX *)&pbuffer->data;

  offset = 0;

  if (pindex_node->head.has_children)
  {
    rc = recursive_delete_index (ref p,
                                 THE_LINK (pindex_node->data),
                                 index_description,
                                 deallocate_data_records,
                                 nb_blocks_per_data_record);
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    offset = LINK'size;
  }


  /* scan all items */

  clear item;

  for (;;)
  {
    rc = read_item (    index_description,
                        pindex_node->head,
                        pindex_node->data,
                    ref offset,
                    out item,
                        item);      /* previous read item */
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }


    /* deallocate the data record */

    if (deallocate_data_records)
    {
      uint             i;
      BUFFER*          pbuffer2;
      DB_CHAINED_DATA* db_chained_data;
      LINK             block_nr, next_block_nr;


      /* retrieve all data record links and dispose the data record blocks */

      next_block_nr = item.data_record_block_nr;

      for (i=0; i<nb_blocks_per_data_record; i++)
      {
        block_nr = next_block_nr;

        if (i < nb_blocks_per_data_record-1)      /* not last block */
        {
          rc = open_block (out pbuffer2, ref p, block_nr, false);
          if (rc < 0)
          {
            close_block (ref p, pbuffer);
            return rc;
          }

          db_chained_data = (DB_CHAINED_DATA *)&pbuffer2->data;
          next_block_nr = db_chained_data->chain_link;

          close_block (ref p, pbuffer2);
        }

        rc = deallocate_block (block_nr, ref p);
        if (rc < 0)
        {
          close_block (ref p, pbuffer);
          return rc;
        }
      }
    }


    /* deallocate the child Btrees */

    if (pindex_node->head.has_children)
    {
      rc = recursive_delete_index (ref p,
                                       item.child_btree,
                                       index_description,
                                       deallocate_data_records,
                                       nb_blocks_per_data_record);
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }
    }

    if (offset == pindex_node->head.nb_data_bytes)     /* no more items */
      break;
  }

  close_block (ref p, pbuffer);


  /* finally, deallocate the index block itself */

  rc = deallocate_block (btree_root, ref p);
  if (rc < 0)
    return rc;

  return 0;
}

/**************************************************************************/

/* can return E_DUPLICATE_KEY with a partially built index */

public int recursive_build_index
            (ref DB_INFO          p,
             LINK                 btree_root,
             DB_INDEX_DESCRIPTION index_description,
             LINK                 table_block_nr,
             int                  new_index_nr,
             DB_INDEX_DESCRIPTION new_index_description,
             byte[]^              data_record_buffer,
             uint                 nb_blocks_per_data_record,
             WORD                 record_size)
{
  int         rc;
  BUFFER*     pbuffer;
  DB_INDEX*   pindex_node;
  INDEX_ITEM  item;
  uint        offset;

  if (btree_root == 0)
    return 0;

  /* open index node */
  rc = open_block (out pbuffer, ref p, btree_root, false);
  if (rc < 0)
    return rc;

  pindex_node = (DB_INDEX *)&pbuffer->data;
  offset = 0;

  if (pindex_node->head.has_children)
  {
    rc = recursive_build_index (ref p,
                                THE_LINK (pindex_node->data),
                                index_description,
                                table_block_nr,
                                new_index_nr,
                                new_index_description,
                                data_record_buffer,
                                nb_blocks_per_data_record,
                                record_size);
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    offset = LINK'size;
  }


  /* scan all items */

  clear item;

  for (;;)
  {
    rc = read_item (    index_description,
                        pindex_node->head,
                        pindex_node->data,
                    ref offset,
                    out item,
                        item);      /* previous read item */
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }


    /* retrieve the data record */

    {
      int              i;
      LINK             block_nr;
      BUFFER*          pbuffer2;
      DB_CHAINED_DATA* pdb_chained_data;
      DB_DATA*         pdb_data;
      uint             rest;

      ref byte[] ptr = data_record_buffer^;
      uint       ofs = 0;

      block_nr = item.data_record_block_nr;

      for (i=0; i<(int)nb_blocks_per_data_record; i++)
      {
        rc = open_block (out pbuffer2, ref p, block_nr, false);
        if (rc < 0)
        {
          close_block (ref p, pbuffer);
          return rc;
        }

        if (i < (int)nb_blocks_per_data_record - 1)    /* not last block */
        {
          pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer2->data;
          block_nr = pdb_chained_data->chain_link;
          ptr[ofs:SIZE_DATA_BLOCK_WITH_SUCCESSOR] = pdb_chained_data->data[0:SIZE_DATA_BLOCK_WITH_SUCCESSOR];
          ofs += SIZE_DATA_BLOCK_WITH_SUCCESSOR;
        }
        else    /* last block */
        {
          pdb_data = (DB_DATA *)&pbuffer2->data;
          rest = record_size - SIZE_DATA_BLOCK_WITH_SUCCESSOR * (nb_blocks_per_data_record - 1);
          ptr[ofs:rest] = pdb_data->data[0:rest];
          ofs += rest;
        }

        close_block (ref p, pbuffer2);
      }
    }


    /* add new index item */

    {
      bool  record_exists;
      LINK  existing_data_record_block_nr;

      rc = add_btree (ref p,
                          table_block_nr,
                          new_index_nr,
                          new_index_description,
                          data_record_buffer^,
                          item.data_record_block_nr,
                      out record_exists,
                      out existing_data_record_block_nr);

      _unused existing_data_record_block_nr;

      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }

      if (record_exists)
      {
        close_block (ref p, pbuffer);
        return E_DUPLICATE_KEY;
      }
    }


    /* scan the child Btrees */

    if (pindex_node->head.has_children)
    {
      rc = recursive_build_index (ref p,
                                      item.child_btree,
                                      index_description,
                                      table_block_nr,
                                      new_index_nr,
                                      new_index_description,
                                      data_record_buffer,
                                      nb_blocks_per_data_record,
                                      record_size);
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }
    }

    if (offset == pindex_node->head.nb_data_bytes)     /* no more items */
      break;
  }

  close_block (ref p, pbuffer);

  return 0;
}

/**************************************************************************/
#end unsafe
/**************************************************************************/
