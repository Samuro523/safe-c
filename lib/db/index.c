
// index.c

use ../strings;

use ../db, config, fs, dbstruct, cache, table, index2, cmd;
use writelog, super, transact;

/**********************************************************************/
#begin unsafe
/**********************************************************************/

/* create an index on a table.                           */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

public
int intern_db_create_index (int              db_handle,
                            string           table_name,
                            string           index_name, /* MAX_INDEX_NAME_LENGTH */
                            INDEX_DEFINITION index_definition)
{
  int                   rc, i, j, slot;
  DB_INFO*              p;
  LINK                  table_block_nr, index_block_nr;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  TABLE_DEFINITION^     table_definition;
  DB_INDEX_DESCRIPTION  index_description;
  uint                  rest_size_for_flags;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* check table name */

  rc = check_name (table_name, MAX_TABLE_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* check index name */

  rc = check_name (index_name, MAX_INDEX_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* check index definition */

  rc = check_index_definition (index_definition);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* search the table */

  rc = search_table_name (ref *p,
                              table_name,
                          out table_block_nr,
                          out pbuffer,
                          out ptable_description);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (table_block_nr == 0)   /* table not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);


  /* try to place an exclusive lock on the locking block */
  rc = try_exclusive_lock (ref *p, ptable_description->locking_block);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, false);   /* table is locked */
  }


  /* allocate memory for a table definition */

  table_definition = new TABLE_DEFINITION;


  /* compute the table definition */

  rc = load_table_definition (table_block_nr, ref *p, out table_definition^);
  if (rc < 0)
  {
    free table_definition;
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, true);   /* fatal I/O error */
  }


  /* initialize an index description block and */
  /* check that the key size is not too long.  */

  clear index_description;
  strcpy (out index_description.index_name, index_name);
//  index_description.key_size     = 0;
//  index_description.k_flags      = 0;
  index_description.nb_key_parts = index_definition.nb_parts;
//  index_description.kp_info_size = 0;

  for (i=0; i<(int)index_description.nb_key_parts; i++)
  {
    /* search 'index_definition->part[i].name' */
    /* in 'table_definition->field[].name'     */

    for (j=0; j<(int)table_definition^.nb_fields; j++)
    {
      if (strcmp (index_definition.part[i].name,
                  table_definition^.field[j].name) == 0)
        break;
    }

    if (j >= (int)table_definition^.nb_fields)  /* not found */
    {
      free table_definition;
      close_block (ref *p, pbuffer);
      return close_command (ref *p, E_FIELD_NOT_FOUND, false);  /* field not found */
    }

    index_description.key_part[i].offset = (WORD)table_definition^.field[j].offset;
    index_description.key_part[i].size   = (WORD)table_definition^.field[j].size;
    index_description.key_part[i].cv_flags = field_flags[table_definition^.field[j].type];

    index_description.key_part[i].kp_flags = 0;

    if (table_definition^.field[j].type == TYPE_STRING &&
        table_definition^.field[j].size >= 4)
    {
      index_description.key_part[i].kp_flags |= KP_STRING_COMPRESSION;
      index_description.kp_info_size++;
    }

    if (table_definition^.field[j].type == TYPE_WSTRING &&
        table_definition^.field[j].size >= 4)
    {
      index_description.key_part[i].kp_flags |= KP_WSTRING_COMPRESSION;
      index_description.kp_info_size++;
    }

    if (table_definition^.field[j].type == TYPE_CHAR &&
        table_definition^.field[j].size >= 4)
    {
      index_description.key_part[i].kp_flags |= KP_TRAILING_BYTES_COMPRESSION;
      index_description.kp_info_size++;
    }

    if (index_description.key_part[i].size
           > MAX_KEY_SIZE - index_description.key_size)
    {
      free table_definition;
      close_block (ref *p, pbuffer);
      return close_command (ref *p, E_KEY_SIZE, false);  /* key size is too large */
    }

    index_description.key_size += index_description.key_part[i].size;
  }


  free table_definition;   /* is not needed anymore */

  if (index_description.key_size > 4)
    index_description.k_flags |= K_LEADING_BYTES_COMPRESSION;


  /* remove some compression flags if we don't have enough space.        */
  /* ! the sum of compression flags & key must not exceed MAX_KEY_SIZE ! */

  rest_size_for_flags = MAX_KEY_SIZE - index_description.key_size;

  /* remove key leading bytes compression */
  if ((index_description.k_flags & K_LEADING_BYTES_COMPRESSION) != 0)
  {
    if (rest_size_for_flags >= 1)
      rest_size_for_flags--;
    else
      index_description.k_flags -= K_LEADING_BYTES_COMPRESSION;
  }

  /* remove compression of key parts */
  for (i=0; i<(int)index_description.nb_key_parts; i++)
  {
    if ((index_description.key_part[i].kp_flags & KP_STRING_COMPRESSION) != 0)
    {
      if (rest_size_for_flags >= 1)
        rest_size_for_flags--;
      else
      {
        index_description.key_part[i].kp_flags -= KP_STRING_COMPRESSION;
        index_description.kp_info_size--;
      }
    }

    if ((index_description.key_part[i].kp_flags & KP_WSTRING_COMPRESSION) != 0)
    {
      if (rest_size_for_flags >= 1)
        rest_size_for_flags--;
      else
      {
        index_description.key_part[i].kp_flags -= KP_WSTRING_COMPRESSION;
        index_description.kp_info_size--;
      }
    }

    if ((index_description.key_part[i].kp_flags & KP_TRAILING_BYTES_COMPRESSION) != 0)
    {
      if (rest_size_for_flags >= 1)
        rest_size_for_flags--;
      else
      {
        index_description.key_part[i].kp_flags -= KP_TRAILING_BYTES_COMPRESSION;
        index_description.kp_info_size--;
      }
    }
  }


  /* check that the index name does not already exist */

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (ptable_description->index_description_link[i] != 0)  /* an index */
    {
      BUFFER               *pbuffer2;
      DB_INDEX_DESCRIPTION *pindex_description2;

      /* we must open the corresponding index description block */

      rc = open_block (out pbuffer2,
                       ref *p,
                       ptable_description->index_description_link[i],
                       false);
      if (rc < 0)
      {
        close_block (ref *p, pbuffer);
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      pindex_description2 = (DB_INDEX_DESCRIPTION *)&pbuffer2->data;

      if (strcmp (pindex_description2->index_name,
                   index_name) == 0)   /* duplicate index ! */
      {
        close_block (ref *p, pbuffer);
        close_block (ref *p, pbuffer2);
        return close_command (ref *p, E_DUPLICATE_NAME, false);
      }

      close_block (ref *p, pbuffer2);
    }
  }


  /* select a free slot for the new index */

  for (slot=0; slot<MAX_INDEXES_PER_TABLE; slot++)
  {
    if (ptable_description->index_description_link[slot] == 0) /* free slot */
      break;
  }

  if (slot >= MAX_INDEXES_PER_TABLE)   /* no slot was free */
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, E_TOO_MANY_INDEXES, false);  /* too many indexes */
  }


  /* check if the table contains data */

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (ptable_description->index_root_link[i] != 0)
      break;
  }

  if (i < MAX_INDEXES_PER_TABLE)   /* table contains data */
  {
    uint                  nb_blocks;
    byte[]^               data_record;
    BUFFER*               pbuffer2;
    DB_INDEX_DESCRIPTION* pold_index_description;


    /* compute number of required blocks for data record */

    nb_blocks = required_data_blocks_for_record_size (ptable_description->record_size);


    /* allocate space for the data record */

    data_record = new byte[ptable_description->record_size];


    /* open the old index_description */

    rc = open_block (out pbuffer2,
                     ref *p,
                     ptable_description->index_description_link[i],
                     false);
    if (rc < 0)
    {
      free data_record;
      close_block (ref *p, pbuffer);
      return close_command (ref *p, rc, true);   /* fatal error */
    }

    pold_index_description = (DB_INDEX_DESCRIPTION *)&pbuffer2->data;


    /* build an index Btree for the new index 'slot' using old index 'i' */

    rc = recursive_build_index
              (ref *p,
               ptable_description->index_root_link[i],  /* old index root */
               *pold_index_description,
               table_block_nr,
               slot,                  /* new index nr           */
               index_description,     /* new index description  */
               data_record,           /* buffer for data_record */
               nb_blocks,
               ptable_description->record_size);

    free data_record;                 /* not needed anymore */
    close_block (ref *p, pbuffer2);   /* not needed anymore */


    if (rc == E_DUPLICATE_KEY)
    {
      rc = recursive_delete_index (ref *p,
                                   ptable_description->index_root_link[slot],
                                   index_description,
                                   false, /* deallocate data records   */
                                   0);    /* nb blocks per data record */
      if (rc < 0)
      {
        close_block (ref *p, pbuffer);
        return close_command (ref *p, rc, true);     /* fatal error */
      }

      ptable_description->index_root_link[slot] = 0;
      close_block (ref *p, pbuffer);

      return close_command (ref *p, E_DUPLICATE_KEY, false);
    }

    if (rc < 0)
    {
      close_block (ref *p, pbuffer);
      return close_command (ref *p, rc, true);      /* fatal error */
    }
  }


  /* allocate and link a new index description block */

  rc = allocate_block (/* block_nr      = */ out index_block_nr,
                       /* db_info       = */ ref *p,
                       /* advised_block = */ table_block_nr,
                       /* alignment     = */ 1);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, true);    /* fatal error */
  }


  set_dirty (ref *p, pbuffer);

  ptable_description->index_description_link[slot] = index_block_nr;


  {
    BUFFER* pbuffer2;

    rc = open_block (out pbuffer2,
                     ref *p,
                     index_block_nr,
                     true);               /* clear with zeroes */
    if (rc < 0)
    {
      close_block (ref *p, pbuffer);
      return close_command (ref *p, rc, true);   /* fatal error */
    }

    set_dirty (ref *p, pbuffer2);

    pbuffer2->data[0:index_description'size] = index_description'byte;

    close_block (ref *p, pbuffer2);
  }

  close_block (ref *p, pbuffer);


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_CREATE_INDEX);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, table_name[0:strlen(table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* index_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (index_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, index_name[0:strlen(index_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* index definition */

  rc = write_logging_byte (ref *p, index_definition.nb_parts);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  {
    WORD id;
    for (id=0; id<index_definition.nb_parts; id++)
    {
      rc = write_logging_byte (ref *p, (BYTE)strlen(index_definition.part[id].name));
      if (rc < 0)
        return close_command (ref *p, rc, true);

      rc = write_logging_data (ref *p,
                               index_definition.part[id].name
                                 [0:strlen(index_definition.part[id].name)]);
      if (rc < 0)
        return close_command (ref *p, rc, true);
    }
  }

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

/*
  Example of an index_definition string : "employee_name, serial_nr"
*/

/* construct an index definition */

public int intern_db_construct_index_definition (out INDEX_DEFINITION index_definition,
                                                 string index_definition_string)
{
  ref string p = index_definition_string[0:strlen(index_definition_string)];
  uint       plast = (uint)p'length;
  uint       pi = 0;
  int        i;

  clear index_definition;
//  index_definition.nb_parts = 0;

  while (pi < plast)
  {
    if (index_definition.nb_parts == MAX_KEY_PARTS)
      return E_BAD_NB_KEY_PARTS;

    /* skip white space */
    while (pi < plast && p[pi] <= ' ')
      pi++;

    /* parse the field name */
    for (i=0; ; i++)
    {
      if (pi == plast || p[pi] <= ' ' || p[pi] == ',')
        break;

      if (i == MAX_FIELD_NAME_LENGTH)   /* too long */
        return E_BAD_NAME;

      index_definition.part[index_definition.nb_parts].name[i] = p[pi++];
    }

    if (i == 0)   /* too short */
      return E_BAD_NAME;

    index_definition.nb_parts++;

    /* skip white space */
    while (pi < plast && p[pi] <= ' ')
      pi++;

    /* parse comma */
    if (pi < plast)
    {
      if (p[pi] == ',')
        pi++;
      else
        return E_SYNTAX_ERROR;
    }
  }

  return check_index_definition (index_definition);
}

/**********************************************************************/

/* rename an index.                                      */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

public int intern_db_rename_index
                    (int    db_handle,
                     string table_name,
                     string old_index_name,
                     string new_index_name)  /* 1 .. MAX_INDEX_NAME_LENGTH */
{
  int                   rc, i;
  bool                  duplicate_index;
  DB_INFO*              p;
  LINK                  block_nr, index_block_nr;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* check table name */

  rc = check_name (table_name, MAX_TABLE_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* check old and new index names */

  rc = check_name (old_index_name, MAX_INDEX_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);

  rc = check_name (new_index_name, MAX_INDEX_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* search the table */

  rc = search_table_name (ref *p,
                          table_name,
                          out block_nr,
                          out pbuffer,
                          out ptable_description);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (block_nr == 0)   /* table not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);


  /* try to place an exclusive lock on the locking block */
  rc = try_exclusive_lock (ref *p, ptable_description->locking_block);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, false);   /* table is locked */
  }


  /* check that the new_index_name does not already exist */
  /* and find the index_block_nr of the old_index_name.   */

  duplicate_index = false;
  index_block_nr = 0;

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (ptable_description->index_description_link[i] != 0)  /* an index */
    {
      BUFFER*               pbuffer2;
      DB_INDEX_DESCRIPTION* pindex_description2;

      /* we must open the corresponding index description block */

      rc = open_block (out pbuffer2,
                       ref *p,
                       ptable_description->index_description_link[i],
                       false);
      if (rc < 0)
      {
        close_block (ref *p, pbuffer);
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      pindex_description2 = (DB_INDEX_DESCRIPTION *)&pbuffer2->data;

      if (strcmp (old_index_name, new_index_name) != 0)
      {
        if (strcmp (pindex_description2->index_name,
                    new_index_name) == 0)   /* duplicate index ! */
        {
          duplicate_index = true;
        }
      }

      if (strcmp (pindex_description2->index_name,
                  old_index_name) == 0)   /* old index name found */
      {
        index_block_nr = ptable_description->index_description_link[i];
      }

      close_block (ref *p, pbuffer2);
    }
  }


  close_block (ref *p, pbuffer);   /* table description no longer needed */

  if (index_block_nr == 0)
    return close_command (ref *p, E_INDEX_NOT_FOUND, false);    /* index not found */

  if (duplicate_index)
    return close_command (ref *p, E_DUPLICATE_NAME, false);  /* new index exists ! */


  /* open the index block and rename the index */

  {
    BUFFER*               pbuffer2;
    DB_INDEX_DESCRIPTION* pindex_description2;

    /* we must open the corresponding index description block */

    rc = open_block (out pbuffer2,
                     ref *p,
                     index_block_nr,
                     false);
    if (rc < 0)
      return close_command (ref *p, rc, true);   /* fatal error */

    set_dirty (ref *p, pbuffer2);

    pindex_description2 = (DB_INDEX_DESCRIPTION *)&pbuffer2->data;

    strcpy (out pindex_description2->index_name, new_index_name);

    close_block (ref *p, pbuffer2);
  }


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_RENAME_INDEX);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, table_name[0:strlen(table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* old_index_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (old_index_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, old_index_name[0:strlen(old_index_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* new_index_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (new_index_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, new_index_name[0:strlen(new_index_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

/* delete an index.                                      */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

public int intern_db_delete_index (int    db_handle,
                                   string table_name,
                                   string index_name)
{
  int                   rc, i, j, count;
  DB_INFO*              p;
  LINK                  block_nr;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  BUFFER*               pbuffer2;
  DB_INDEX_DESCRIPTION* pindex_description2;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* check table name */

  rc = check_name (table_name, MAX_TABLE_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* check index name */

  rc = check_name (index_name, MAX_INDEX_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* search the table */

  rc = search_table_name (    ref *p,
                              table_name,
                          out block_nr,
                          out pbuffer,
                          out ptable_description);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (block_nr == 0)   /* table not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);


  /* try to place an exclusive lock on the locking block */
  rc = try_exclusive_lock (ref *p, ptable_description->locking_block);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, false);   /* table is locked */
  }


  /* search the index name */

  clear pbuffer2;
  pindex_description2 = null;

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (ptable_description->index_description_link[i] != 0)  /* an index */
    {
      /* we must open the corresponding index description block */

      rc = open_block (out pbuffer2,
                       ref *p,
                       ptable_description->index_description_link[i],
                       false);
      if (rc < 0)
      {
        close_block (ref *p, pbuffer);
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      pindex_description2 = (DB_INDEX_DESCRIPTION *)&pbuffer2->data;

      if (strcmp (pindex_description2->index_name,
                  index_name) == 0)     /* found ! */
      {
        break;    /* leave loop with buffer2 remaining open */
      }

      close_block (ref *p, pbuffer2);
    }
  }

  if (i == MAX_INDEXES_PER_TABLE)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, E_INDEX_NOT_FOUND, false);    /* index not found */
  }


  /* if the table contains data records, check that it's not the last index */

  count = 0;
  for (j=0; j<MAX_INDEXES_PER_TABLE; j++)
    count += (int)(ptable_description->index_description_link[j] != 0);
  if (count == 1 && ptable_description->index_root_link[i] != 0)
  {
    close_block (ref *p, pbuffer);
    close_block (ref *p, pbuffer2);
    return close_command (ref *p, E_LAST_INDEX, false);    /* last index */
  }


  /* delete index Btree */

  rc = recursive_delete_index (ref *p,
                               ptable_description->index_root_link[i],
                               *pindex_description2,
                               false,    /* deallocate data records */
                               0);       /* nb blocks per data record */
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    close_block (ref *p, pbuffer2);
    return close_command (ref *p, rc, true);    /* fatal error */
  }

  close_block (ref *p, pbuffer2);    /* not used anymore */


  /* deallocate index description block */

  rc = deallocate_block (ptable_description->index_description_link[i], ref *p);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, true);   /* fatal error : cannot deallocate */
  }


  /* erase index entry in table description */

  set_dirty (ref *p, pbuffer);

  ptable_description->index_description_link[i] = 0;
  ptable_description->index_root_link[i]        = 0;

  close_block (ref *p, pbuffer);


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_DELETE_INDEX);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, table_name[0:strlen(table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* index_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (index_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, index_name[0:strlen(index_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
