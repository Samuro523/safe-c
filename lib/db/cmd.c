
// cmd.c

use ../strings;

use ../db, config, fs, dbstruct, cache, transact, open, badd, bdelete, bsearch;
use writelog, super;

/**********************************************************************/
#begin unsafe
/**********************************************************************/

public uint required_data_blocks_for_record_size (uint record_size)
{
  if (record_size <= SIZE_DATA_BLOCK_WITHOUT_SUCCESSOR)
    return 1;
  else
    return (record_size - SIZE_DATA_BLOCK_WITHOUT_SUCCESSOR + (SIZE_DATA_BLOCK_WITH_SUCCESSOR-1))
           / SIZE_DATA_BLOCK_WITH_SUCCESSOR
           + 1;
}

/**********************************************************************/

public
int intern_db_retrieve (    int     table_handle,
                        ref byte[]  record,
                            uint2   retrieve_mode,
                            string  index_name)
{
  int              rc;
  DB_INFO*         p;
  TABLE_INFO^      t;
  int              current_index;             /* -1 = none */
  bool             record_not_found;
  LINK             data_record_block_nr, block_nr;
  WORD             i;
  BUFFER*          pbuffer;
  DB_CHAINED_DATA* pdb_chained_data;
  DB_DATA*         pdb_data;
  uint             nb_blocks, offset, chunk_size;

//trace ("BEGIN  intern_db_retrieve2 (table_handle = %d, retrieve_mode = %u, index_name = %s\n", table_handle, retrieve_mode, index_name);
  
  rc = check_table_handle (table_handle, out p, out t);
  if (rc < 0)
    return rc;

  if (record'size != t^.record_size)
    return E_RECORD_SIZE_MISMATCH;

  if (retrieve_mode > 6)
    return E_BAD_MODE;

  current_index = get_index_nr (t, index_name);
  if (current_index < 0)
    return E_INDEX_NOT_FOUND;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;

// trace ("    open_command() ok\n");

  /* search the record using the current index */

  rc = search_btree (    ref *p,
                         t^.table_block_nr,
                         current_index,
                         t^.index[current_index]^,
                         record,
                         retrieve_mode,
                     out record_not_found,
                     out data_record_block_nr);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (record_not_found)
    return close_command (ref *p, E_KEY_NOT_FOUND, false);


  /* compute number of required blocks for data record */

  nb_blocks = required_data_blocks_for_record_size (record'size);


  /* retrieve the data record chain */

  offset   = 0;
  block_nr = data_record_block_nr;

  for (i=0; i<nb_blocks; i++)
  {
    rc = open_block (out pbuffer, ref *p, block_nr, false);
    if (rc < 0)
      return close_command (ref *p, rc, true);   /* fatal error */

    if (i < nb_blocks-1)    /* not last block */
    {
      pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer->data;
      block_nr = pdb_chained_data->chain_link;
      record[offset:SIZE_DATA_BLOCK_WITH_SUCCESSOR] = pdb_chained_data->data[0:SIZE_DATA_BLOCK_WITH_SUCCESSOR];
      offset += SIZE_DATA_BLOCK_WITH_SUCCESSOR;
    }
    else    /* last block */
    {
      pdb_data = (DB_DATA *)&pbuffer->data;

      chunk_size = record'size - SIZE_DATA_BLOCK_WITH_SUCCESSOR * (nb_blocks - 1);
      record[offset:chunk_size] = pdb_data->data[0:chunk_size];
      offset += chunk_size;
    }

    close_block (ref *p, pbuffer);
  }
  assert offset == record'size;
  
  rc = close_command (ref *p, 0, false);
  
// trace ("END intern_db_retrieve2 ()\n");

  return rc;          
}

/**********************************************************************/

public
int intern_db_insert (int table_handle, byte[] record)
{
  int              rc, i, j;
  DB_INFO*         p;
  TABLE_INFO^      t;
  LINK[]^          data_record_table;
  bool             record_exists, record_not_found;
  LINK             dummy_link;
  BUFFER*          pbuffer;
  DB_DATA*         pdb_data;
  DB_CHAINED_DATA* pdb_chained_data;
  uint             nb_blocks, offset, chunk_size;

  rc = check_table_handle (table_handle, out p, out t);
  if (rc < 0)
    return rc;

  if (record'size != t^.record_size)
    return E_RECORD_SIZE_MISMATCH;

  /* check that there is at least one index */
  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (t^.index[i] != null)
      break;
  }
  if (i == MAX_INDEXES_PER_TABLE)    /* there is no index */
    return E_NO_INDEX;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* compute number of required blocks for data record */

  nb_blocks = required_data_blocks_for_record_size (record'size);


  /* allocate a table for storing the data record block numbers */

  data_record_table = new LINK [nb_blocks];


  /* allocate blocks for the data record */

  for (i=0; i<(int)nb_blocks; i++)
  {
    rc = allocate_block (out /* block_nr      = */ data_record_table^[i],
                         ref /* db_info       = */ *p,
                             /* advised_block = */ 0,
                             /* alignment     = */ (WORD)(nb_blocks - (uint)i));
    if (rc < 0)
    {
      free data_record_table;
      return close_command (ref *p, rc, true);   /* fatal error */
    }
  }


  /* add references to this record on all indexes */

  record_exists = false;

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (t^.index[i] != null)
    {
      rc = add_btree (ref *p,
                          t^.table_block_nr,
                          i,
                          t^.index[i]^,
                          record,
                          data_record_table^[0],   /* first block of data record */
                      out record_exists,           /* true = was not added */
                      out dummy_link);

      _unused dummy_link;

      if (rc < 0)
      {
        free data_record_table;
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      if (record_exists)
        break;
    }
  }


  if (record_exists)
  {
    /* this is bad : we have to delete all previously added references */
    for (j=0; j<i; j++)
    {
      if (t^.index[j] != null)
      {
        rc = delete_btree (ref *p,
                               t^.table_block_nr,
                               j,
                               t^.index[j]^,
                               record,
                           out record_not_found, /* true = not deleted */
                           out dummy_link);
        if (rc < 0)
        {
          free data_record_table;
          return close_command (ref *p, rc, true);   /* fatal error */
        }
        if (record_not_found)
        {
          free data_record_table;
          return close_command (ref *p, E_INTERN_3, true);   /* fatal error */
        }
      }
    }

    /* we have to deallocate the blocks of the data record table */
    for (i=0; i<(int)nb_blocks; i++)
    {
      rc = deallocate_block (data_record_table^[i], ref *p);
      if (rc < 0)
      {
        free data_record_table;
        return close_command (ref *p, rc, true);   /* fatal error */
      }
    }

    free data_record_table;
    return close_command (ref *p, E_DUPLICATE_KEY, false);   /* duplicate key */
  }


  /* let's now write the chained data record */

  offset = 0;

  for (i=0; i<(int)nb_blocks; i++)
  {
    rc = open_block (out pbuffer, ref *p, data_record_table^[i], true);
    if (rc < 0)
    {
      free data_record_table;
      return close_command (ref *p, rc, true);   /* fatal error */
    }

    set_dirty (ref *p, pbuffer);

    if (i < (int)nb_blocks-1)    /* not last block */
    {
      pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer->data;
      pdb_chained_data->chain_link = data_record_table^[i+1];

      pdb_chained_data->data[0:SIZE_DATA_BLOCK_WITH_SUCCESSOR] = record[offset:SIZE_DATA_BLOCK_WITH_SUCCESSOR];
      offset += SIZE_DATA_BLOCK_WITH_SUCCESSOR;
    }
    else    /* last block */
    {
      pdb_data = (DB_DATA *)&pbuffer->data;

      chunk_size = record'size - SIZE_DATA_BLOCK_WITH_SUCCESSOR * (nb_blocks - 1);
      pdb_data->data[0:chunk_size] = record[offset:chunk_size];
      offset += chunk_size;
    }

    close_block (ref *p, pbuffer);
  }
  assert offset == record'size;
  
  free data_record_table;


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_INSERT);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (t^.table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, t^.table_name[0:strlen(t^.table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* record */

  rc = write_logging_word (ref *p, (WORD)record'size);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, record);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

public
int intern_db_delete (int table_handle, byte[] record, string index_name)
{
  int             rc;
  DB_INFO*        p;
  TABLE_INFO^     t;
  int             current_index;        /* -1 = none */
  LINK[]^         data_record_table;
  bool            record_not_found;
  LINK            data_record_block_nr;
  uint            nb_blocks;
  int             i;

  rc = check_table_handle (table_handle, out p, out t);
  if (rc < 0)
    return rc;

  if (record'size != t^.record_size)
    return E_RECORD_SIZE_MISMATCH;

  current_index = get_index_nr (t, index_name);
  if (current_index < 0)
    return E_INDEX_NOT_FOUND;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* compute number of required blocks for data record */

  nb_blocks = required_data_blocks_for_record_size (record'size);


  /* allocate a table for storing the data record block numbers */

  data_record_table = new LINK [nb_blocks];


  /* delete the key item using the current index */

  rc = delete_btree (ref *p,
                         t^.table_block_nr,
                         current_index,
                         t^.index[current_index]^,
                         record,
                     out record_not_found,      /* true = was not deleted */
                     out data_record_block_nr);
  if (rc < 0)
  {
    free data_record_table;
    return close_command (ref *p, rc, true);   /* fatal error */
  }

  if (record_not_found)
  {
    free data_record_table;
    return close_command (ref *p, E_KEY_NOT_FOUND, false);
  }


  /* check if there is more than one index */

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (t^.index[i] != null && current_index != (int)i)
      break;
  }

  if (i < MAX_INDEXES_PER_TABLE)    /* there is more than one index */
  {
    LINK             block_nr;
    byte[]^          data_record;
    WORD             j;
    BUFFER*          pbuffer;
    DB_DATA*         pdb_data;
    DB_CHAINED_DATA* pdb_chained_data;
    uint             offset, chunk_size;


    /* allocate space for the data record */

    data_record = new byte [record'size];


    /* retrieve the data record chain */

    offset = 0;
    block_nr = data_record_block_nr;

    for (j=0; j<nb_blocks; j++)
    {
      data_record_table^[j] = block_nr;

      rc = open_block (out pbuffer, ref *p, block_nr, false);
      if (rc < 0)
      {
        free data_record_table;
        free data_record;
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      if (j < nb_blocks-1)    /* not last block */
      {
        pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer->data;
        block_nr = pdb_chained_data->chain_link;
        data_record^[offset:SIZE_DATA_BLOCK_WITH_SUCCESSOR] = pdb_chained_data->data[0:SIZE_DATA_BLOCK_WITH_SUCCESSOR];
        offset += SIZE_DATA_BLOCK_WITH_SUCCESSOR;
      }
      else    /* last block */
      {
        pdb_data = (DB_DATA *)&pbuffer->data;

        chunk_size = data_record^'size - SIZE_DATA_BLOCK_WITH_SUCCESSOR * (nb_blocks - 1);
        data_record^[offset:chunk_size] = pdb_data->data[0:chunk_size];
        offset += chunk_size;
      }

      close_block (ref *p, pbuffer);
    }
    assert offset == data_record^'size;
    

    /* delete all indexes except the current index (that is already done) */

    for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
    {
      if (t^.index[i] != null && current_index != (int)i)
      {
        rc = delete_btree (ref *p,
                               t^.table_block_nr,
                               i,
                               t^.index[i]^,
                               data_record^,
                           out record_not_found,   /* true = was not deleted */
                           out block_nr);          /* data record nr */
        if (rc < 0)
        {
          free data_record_table;
          free data_record;
          return close_command (ref *p, rc, true);   /* fatal error */
        }

        if (block_nr != data_record_block_nr)   /* not same record nr ? */
        {
          free data_record_table;
          free data_record;
          return close_command (ref *p, E_INTERN_6, true);   /* fatal error */
        }

        if (record_not_found)      /* no entry for this index ? */
        {
          free data_record_table;
          free data_record;
          return close_command (ref *p, E_INTERN_8, true);   /* fatal error */
        }
      }
    }

    free data_record;        /* not needed anymore */


    /* deallocate all data record blocks */

    for (j=0; j<nb_blocks; j++)
    {
      rc = deallocate_block (data_record_table^[j], ref *p);
      if (rc < 0)
      {
        free data_record_table;
        return close_command (ref *p, rc, true);   /* fatal error */
      }
    }
  }

  else    /* there is only one index */

  {
    BUFFER*          pbuffer;
    DB_CHAINED_DATA* pdb_chained_data;
    uint             j;


    /* retrieve all data record chain links */

    for (j=0; j<nb_blocks; j++)
    {
      data_record_table^[j] = data_record_block_nr;

      if (j < nb_blocks-1)    /* not last block */
      {
        rc = open_block (out pbuffer, ref *p, data_record_block_nr, false);
        if (rc < 0)
        {
          free data_record_table;
          return close_command (ref *p, rc, true);   /* fatal error */
        }

        pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer->data;
        data_record_block_nr = pdb_chained_data->chain_link;

        close_block (ref *p, pbuffer);
      }
    }


    /* deallocate the data record chain */

    for (j=0; j<nb_blocks; j++)
    {
      rc = deallocate_block (data_record_table^[j], ref *p);
      if (rc < 0)
      {
        free data_record_table;
        return close_command (ref *p, rc, true);      /* fatal error */
      }
    }
  }

  free data_record_table;


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_DELETE);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (t^.table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, t^.table_name[0:strlen(t^.table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* current index name */

  {
    char name[MAX_INDEX_NAME_LENGTH] = t^.index[current_index]^.index_name;

    rc = write_logging_byte (ref *p, (BYTE)strlen (name));
    if (rc < 0)
      return close_command (ref *p, rc, true);

    rc = write_logging_data (ref *p, name[0:strlen(name)]);
    if (rc < 0)
      return close_command (ref *p, rc, true);
  }


  /* record */

  rc = write_logging_word (ref *p, (WORD)record'size);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, record);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

public
int intern_db_update (int table_handle, byte[] record, string index_name)
{
  int           rc;
  DB_INFO*      p;
  TABLE_INFO^   t;
  int           current_index;               /* -1 = none */
  bool          record_not_found;
  LINK          data_record_block_nr;
  uint          nb_blocks;
  int           i;

  rc = check_table_handle (table_handle, out p, out t);
  if (rc < 0)
    return rc;

  if (record'size != t^.record_size)
    return E_RECORD_SIZE_MISMATCH;

  current_index = get_index_nr (t, index_name);
  if (current_index < 0)
    return E_INDEX_NOT_FOUND;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* search the record using the current index */

  rc = search_btree (ref *p,
                         t^.table_block_nr,
                         current_index,
                         t^.index[current_index]^,
                         record,
                         DB_EQUAL,
                     out record_not_found,
                     out data_record_block_nr);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (record_not_found)
    return close_command (ref *p, E_KEY_NOT_FOUND, false);


  /* compute number of required blocks for data record */

  nb_blocks = required_data_blocks_for_record_size (record'size);


  /* check if there is more than one index */

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (t^.index[i] != null && current_index != i)
      break;
  }

  if (i < MAX_INDEXES_PER_TABLE)    /* there is more than one index */
  {
    LINK[]^          data_record_table;
    byte[]^          data_record;
    LINK             block_nr;
    BUFFER*          pbuffer;
    DB_CHAINED_DATA* pdb_chained_data;
    DB_DATA*         pdb_data;
    uint             j, offset, chunk_size;


    /* allocate a table for storing the data record block numbers */

    data_record_table = new LINK [nb_blocks];


    /* allocate space for the data record */

    data_record = new byte [record'size];


    /* retrieve the data record and its chain */

    offset   = 0;
    block_nr = data_record_block_nr;

    for (j=0; j<nb_blocks; j++)
    {
      data_record_table^[j] = block_nr;

      rc = open_block (out pbuffer, ref *p, block_nr, false);
      if (rc < 0)
      {
        free data_record_table;
        free data_record;
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      if (j < nb_blocks-1)    /* not last block */
      {
        pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer->data;
        block_nr = pdb_chained_data->chain_link;
        data_record^[offset:SIZE_DATA_BLOCK_WITH_SUCCESSOR] = pdb_chained_data->data[0:SIZE_DATA_BLOCK_WITH_SUCCESSOR];
        offset += SIZE_DATA_BLOCK_WITH_SUCCESSOR;
      }
      else    /* last block */
      {
        pdb_data = (DB_DATA *)&pbuffer->data;

        chunk_size = data_record^'size - SIZE_DATA_BLOCK_WITH_SUCCESSOR * (nb_blocks - 1);
        data_record^[offset:chunk_size] = pdb_data->data[0:chunk_size];
        offset += chunk_size;
      }

      close_block (ref *p, pbuffer);
    }
    assert offset == data_record^'size;
    

    /* update all indexes except the current index */

    for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
    {
      BYTE  old_key[MAX_KEY_SIZE];
      BYTE  new_key[MAX_KEY_SIZE];
      uint  key_size;
      bool  record_exists;
      LINK  dummy_link;

      if (t^.index[i] == null || current_index == i)
        continue;

      compute_key (t^.index[i]^, data_record^, out old_key);
      compute_key (t^.index[i]^, record,       out new_key);

      key_size = t^.index[i]^.key_size;

      if (memcmp (old_key[0:key_size], new_key[0:key_size]) == 0)
        continue;         /* key doesn't change for this index */

      rc = add_btree (ref *p,
                          t^.table_block_nr,
                          i,
                          t^.index[i]^,
                          record,                 /* new data_record */
                          data_record_table^[0],  /* first block of data record */
                      out record_exists,          /* true = was not added */
                      out dummy_link);

      _unused dummy_link;

      if (rc < 0)
      {
        free data_record_table;
        free data_record;
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      if (record_exists)     /* duplicate index ! */
        break;
    }



    if (i < MAX_INDEXES_PER_TABLE)
    {
      /* something went wrong above : an index already existed. */
      /* we must delete again all created indexes.              */

      i--;   /* go to previous index */

      for (; i>=0; i--)
      {
        BYTE  old_key[MAX_KEY_SIZE];
        BYTE  new_key[MAX_KEY_SIZE];
        uint  key_size;

        if (t^.index[i] == null || current_index == i)
          continue;

        compute_key (t^.index[i]^, data_record^, out old_key);
        compute_key (t^.index[i]^, record,       out new_key);

        key_size = t^.index[i]^.key_size;

        if (memcmp (old_key[0:key_size], new_key[0:key_size]) == 0)
          continue;         /* key doesn't change for this index */

        rc = delete_btree (ref *p,
                               t^.table_block_nr,
                               i,
                               t^.index[i]^,
                               record,             /* new data_record */
                           out record_not_found,   /* true = was not deleted */
                           out block_nr);
        if (rc < 0)
        {
          free data_record_table;
          free data_record;
          return close_command (ref *p, rc, true);   /* fatal error */
        }

        if (record_not_found)
        {
          free data_record_table;
          free data_record;
          return close_command (ref *p, E_INTERN_9, true);   /* fatal error */
        }

        if (block_nr != data_record_table^[0])   /* not same record nr ? */
        {
          free data_record_table;
          free data_record;
          return close_command (ref *p, E_INTERN_10, true);   /* fatal error */
        }
      }

      free data_record_table;
      free data_record;

      return close_command (ref *p, E_DUPLICATE_KEY, false);   /* DUPLICATE KEY */
    }


    /* remove now the old indexes */

    for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
    {
      BYTE  old_key[MAX_KEY_SIZE];
      BYTE  new_key[MAX_KEY_SIZE];
      uint  key_size;

      if (t^.index[i] == null || current_index == i)
        continue;

      compute_key (t^.index[i]^, data_record^, out old_key);
      compute_key (t^.index[i]^, record,       out new_key);

      key_size = t^.index[i]^.key_size;

      if (memcmp (old_key[0:key_size], new_key[0:key_size]) == 0)
        continue;         /* key doesn't change for this index */

      rc = delete_btree (ref *p,
                             t^.table_block_nr,
                             i,
                             t^.index[i]^,
                             data_record^,
                         out record_not_found,   /* true = was not deleted */
                         out block_nr);          /* data record nr */
      if (rc < 0)
      {
        free data_record_table;
        free data_record;
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      if (record_not_found)
      {
        free data_record_table;
        free data_record;
        return close_command (ref *p, E_INTERN_11, true);   /* fatal error */
      }

      if (block_nr != data_record_table^[0])   /* not same record nr ? */
      {
        free data_record_table;
        free data_record;
        return close_command (ref *p, E_INTERN_12, true);   /* fatal error */
      }
    }

    free data_record;   /* old data_record value is not needed anymore */


    /* finally, update the data_record value */

    offset = 0;

    for (i=0; i<(int)nb_blocks; i++)
    {
      rc = open_block (out pbuffer, ref *p, data_record_table^[i], false);
      if (rc < 0)
      {
        free data_record_table;
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      set_dirty (ref *p, pbuffer);

      if (i < (int)nb_blocks-1)    /* not last block */
      {
        pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer->data;

        pdb_chained_data->data[0:SIZE_DATA_BLOCK_WITH_SUCCESSOR] = record[offset:SIZE_DATA_BLOCK_WITH_SUCCESSOR];
        offset += SIZE_DATA_BLOCK_WITH_SUCCESSOR;
      }
      else    /* last block */
      {
        pdb_data = (DB_DATA *)&pbuffer->data;

        chunk_size = record'size - SIZE_DATA_BLOCK_WITH_SUCCESSOR * (nb_blocks - 1);
        pdb_data->data[0:chunk_size] = record[offset:chunk_size];
        offset += chunk_size;
      }

      close_block (ref *p, pbuffer);
    }
    assert offset == record'size;
    
    free data_record_table;
  }
  else              /* there is only one index */
  {
    LINK             block_nr;
    BUFFER*          pbuffer;
    DB_CHAINED_DATA* pdb_chained_data;
    DB_DATA*         pdb_data;
    uint             offset, chunk_size;


    /* retrieve and update all data record chained blocks */

    block_nr = data_record_block_nr;
    offset = 0;

    for (i=0; i<(int)nb_blocks; i++)
    {
      if (i < (int)nb_blocks-1)    /* not last block */
      {
        rc = open_block (out pbuffer, ref *p, block_nr, false);
        if (rc < 0)
          return close_command (ref *p, rc, true);   /* fatal error */

        set_dirty (ref *p, pbuffer);

        pdb_chained_data = (DB_CHAINED_DATA *)&pbuffer->data;
        block_nr = pdb_chained_data->chain_link;

        pdb_chained_data->data[0:SIZE_DATA_BLOCK_WITH_SUCCESSOR] = record[offset:SIZE_DATA_BLOCK_WITH_SUCCESSOR];
        offset += SIZE_DATA_BLOCK_WITH_SUCCESSOR;

        close_block (ref *p, pbuffer);
      }
      else    /* last block */
      {
        rc = open_block (out pbuffer, ref *p, block_nr, true);   /* clear data */
        if (rc < 0)
          return close_command (ref *p, rc, true);   /* fatal error */

        set_dirty (ref *p, pbuffer);

        pdb_data = (DB_DATA *)&pbuffer->data;

        chunk_size = record'size - SIZE_DATA_BLOCK_WITH_SUCCESSOR * (nb_blocks - 1);
        pdb_data->data[0:chunk_size] = record[offset:chunk_size];
        offset += chunk_size;

        close_block (ref *p, pbuffer);
      }
    }
    assert offset == record'size;
  }


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_UPDATE);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (t^.table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, t^.table_name[0:strlen(t^.table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* current index name */

  {
    char name[MAX_INDEX_NAME_LENGTH] = t^.index[current_index]^.index_name;

    rc = write_logging_byte (ref *p, (BYTE)strlen (name));
    if (rc < 0)
      return close_command (ref *p, rc, true);

    rc = write_logging_data (ref *p, name[0:strlen(name)]);
    if (rc < 0)
      return close_command (ref *p, rc, true);
  }


  /* record */

  rc = write_logging_word (ref *p, (WORD)record'size);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, record);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
