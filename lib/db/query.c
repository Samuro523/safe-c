
// query.c

use ../strings;

use ../db, config, fs, dbstruct, transact, cache, table;

/**********************************************************************/
#begin unsafe
/**********************************************************************/

/* obtain the name of the nth table of the database.                    */
/* 'table_nr' starts at index zero.                                     */
/* 'table_name' : a buffer of at least (MAX_TABLE_NAME_LENGTH+1) bytes. */
/* returns 0 if OK, E_INDEX_RANGE if 'table_nr' is out of range,        */
/*         or if the db_handle is invalid.                              */

public
int intern_db_query_table_name (int  db_handle,
                                int  table_nr,
                                out string(MAX_TABLE_NAME_LENGTH) table_name)
{
  int                   rc, i;
  DB_INFO*              p;
  LINK                  block_nr;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;

  clear table_name;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* get the nth table */

  block_nr = p->db_header.table_description_link;

  i = 0;

  for (;;)
  {
    if (block_nr == 0)
      return close_command (ref *p, E_INDEX_RANGE, false); /* index is out of range */

    rc = open_block (out pbuffer, ref *p, block_nr, false);
    if (rc < 0)
      return close_command (ref *p, rc, true);   /* fatal error */

    ptable_description = (DB_TABLE_DESCRIPTION *)&pbuffer->data;

    if (i == table_nr)
      break;

    i++;

    block_nr = ptable_description->next_table_link;

    close_block (ref *p, pbuffer);
  }

  strcpy (out table_name, ptable_description->table_name);

  close_block (ref *p, pbuffer);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

/* obtain the definition of a database table.             */
/* returns 0 if OK, an error if the table does not exist  */
/* or if the db_handle is invalid.                        */

public
int intern_db_query_table_definition (    int              db_handle,
                                          string           table_name,
                                      out TABLE_DEFINITION table_definition)
{
  int                   rc;
  DB_INFO*              p;
  LINK                  block_nr;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;

  clear table_definition;

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


  /* search the table */

  rc = search_table_name (ref *p,
                          table_name,
                          out block_nr,
                          out pbuffer,
                          out ptable_description);

  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  _unused ptable_description;

  if (block_nr == 0)   /* table was not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);

  close_block (ref *p, pbuffer);


  /* load the table definition */

  rc = load_table_definition (block_nr, ref *p, out table_definition);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal I/O error */

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

/* obtain the name of the nth index of a table.                         */
/* 'index_nr' starts at index zero.                                     */
/* 'table_name' : a buffer of at least (MAX_INDEX_NAME_LENGTH+1) bytes. */
/* returns 0 if OK, an error if 'index_nr' is out of range              */
/* or if the table does not exist, or if the db_handle is invalid.      */

public
int intern_db_query_index_name (    int    db_handle,
                                    string table_name,
                                    int    index_nr,
                                out string(MAX_INDEX_NAME_LENGTH) index_name)
{
  int                   rc, i, slot;
  DB_INFO*              p;
  LINK                  block_nr;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  DB_INDEX_DESCRIPTION* pindex_description;

  clear index_name;

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


  /* search the table */

  rc = search_table_name (ref *p,
                          table_name,
                          out block_nr,
                          out pbuffer,
                          out ptable_description);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (block_nr == 0)   /* table was not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);


  /* compute slot of nth index */

  i = 0;

  for (slot=0; slot<MAX_INDEXES_PER_TABLE; slot++)
  {
    if (ptable_description->index_description_link[slot] != 0)
    {
      if (i == index_nr)
        break;
      i++;
    }
  }

  if (slot >= MAX_INDEXES_PER_TABLE)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, E_INDEX_RANGE, false);  /* index is out of range */
  }

  block_nr = ptable_description->index_description_link[slot];

  close_block (ref *p, pbuffer);


  /* open the index description block */

  rc = open_block (out pbuffer, ref *p, block_nr, false);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  pindex_description = (DB_INDEX_DESCRIPTION *)&pbuffer->data;

  strcpy (out index_name, pindex_description->index_name);

  close_block (ref *p, pbuffer);


  return close_command (ref *p, 0, false);
}

/**********************************************************************/

public
int intern_db_query_index_definition (    int              db_handle,
                                          string           table_name,
                                          string           index_name,
                                      out INDEX_DEFINITION index_definition)
{
  int                   rc, slot, i, j;
  DB_INFO*              p;
  LINK                  block_nr, index_block_nr;
  BUFFER*               pbuffer, pindex_buffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  TABLE_DEFINITION^     ptable_definition;
  DB_INDEX_DESCRIPTION* pindex_description;

  clear index_definition;

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


  /* search the table */

  rc = search_table_name (ref *p,
                          table_name,
                          out block_nr,
                          out pbuffer,
                          out ptable_description);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (block_nr == 0)   /* table was not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);


  /* load the table definition */

  ptable_definition = new TABLE_DEFINITION;

  rc = load_table_definition (block_nr, ref *p, out ptable_definition^);
  if (rc < 0)
  {
    free ptable_definition;
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, true);   /* fatal I/O error */
  }


  /* search the index name */

  pindex_description = null;
  pindex_buffer = null;

  for (slot=0; slot<MAX_INDEXES_PER_TABLE; slot++)
  {
    if (ptable_description->index_description_link[slot] != 0)
    {
      index_block_nr = ptable_description->index_description_link[slot];

      rc = open_block (out pindex_buffer, ref *p, index_block_nr, false);
      if (rc < 0)
      {
        free ptable_definition;
        close_block (ref *p, pbuffer);
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      pindex_description = (DB_INDEX_DESCRIPTION *)&pindex_buffer->data;

      if (strcmp (index_name, pindex_description->index_name) == 0)
        break;

      close_block (ref *p, pindex_buffer);
    }
  }

  if (slot >= MAX_INDEXES_PER_TABLE)
  {
    free ptable_definition;
    close_block (ref *p, pbuffer);
    return close_command (ref *p, E_INDEX_NOT_FOUND, false);
  }

  index_definition.nb_parts = pindex_description->nb_key_parts;
  for (i=0; i<(int)index_definition.nb_parts; i++)
  {
    for (j=0; j<(int)ptable_definition^.nb_fields; j++)
    {
      if (ptable_definition^.field[j].offset ==
            pindex_description->key_part[i].offset)
        break;
    }

    if (j >= (int)ptable_definition^.nb_fields)
    {
      free ptable_definition;
      close_block (ref *p, pbuffer);
      close_block (ref *p, pindex_buffer);
      return close_command (ref *p, E_INTERN_2, true);   /* intern error */
    }

    strcpy (out index_definition.part[i].name, ptable_definition^.field[j].name);
  }

  free ptable_definition;
  close_block (ref *p, pbuffer);
  close_block (ref *p, pindex_buffer);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

public int intern_db_get_write_transaction_count (int db_handle, out long count)
{
  int      rc;
  DB_INFO* p;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
  {
    count = 0;
    return rc;
  }

  rc = open_command (ref *p);
  if (rc < 0)
  {
    count = 0;
    return rc;
  }

  count = p->db_header.current_transaction.number;

  return close_command (ref *p, command_rc => 0, fatal_error_occured => false);
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
