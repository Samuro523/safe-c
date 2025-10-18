
// open.c

use ../strings;
use ../db, config, fs, dbstruct, cache, transact, table, dbport, backup;

//--------------------------------------------------------------------------------
#begin unsafe
//--------------------------------------------------------------------------------

public
int intern_db_open_table (int               db_handle,
                          string            table_name,
                          TABLE_DEFINITION* ptable_definition)  // optional, can be null
{
  int                   rc, i, slot, j;
  DB_INFO*              p;
  LINK                  block_nr;
  BUFFER*               pbuffer, pindex_buffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  TABLE_DEFINITION^     pintern_table_definition;
  DB_INDEX_DESCRIPTION* pindex_description;
  TABLE_INFO*           t;
  uint                  size;
  bool                  is_corrupt;

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

  if (ptable_definition != null)
  {
    pintern_table_definition = new TABLE_DEFINITION;

    rc = load_table_definition (block_nr, ref *p, out pintern_table_definition^);
    if (rc < 0)
    {
      free pintern_table_definition;
      close_block (ref *p, pbuffer);
      return close_command (ref *p, rc, true);   /* fatal I/O error */
    }

    /* compare both table definitions */
    if (ptable_definition->record_size != pintern_table_definition^.record_size ||
        ptable_definition->nb_fields   != pintern_table_definition^.nb_fields)
    {
      free pintern_table_definition;
      close_block (ref *p, pbuffer);
      return close_command (ref *p, E_MISMATCH, false);  /* table definition mismatch */
    }

    for (i=0; i<(int)ptable_definition->nb_fields; i++)
    {
      if (ptable_definition->field[i].offset !=
               pintern_table_definition^.field[i].offset ||
          ptable_definition->field[i].size !=
               pintern_table_definition^.field[i].size ||
          ptable_definition->field[i].type !=
               pintern_table_definition^.field[i].type ||
          strcmp (ptable_definition->field[i].name,
                  pintern_table_definition^.field[i].name) != 0)
      {
        free pintern_table_definition;
        close_block (ref *p, pbuffer);
        return close_command (ref *p, E_MISMATCH, false);  /* table definition mismatch */
      }
    }

    free pintern_table_definition;    /* no longer needed */
  }


  /* search a free table slot */

  for (slot=0; slot<MAX_OPEN_TABLES; slot++)
  {
    if (p->table[slot] == null)
      break;
  }

  if (slot >= MAX_OPEN_TABLES)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, E_MAX_OPEN_TABLES, false); /* too many open tables */
  }


  /* allocate and initialize the new table slot */

  p->table[slot] = new TABLE_INFO;

  t = &p->table[slot]^;

  strcpy (out t->table_name, table_name);
  t->table_block_nr = block_nr;

  if (p->pbackup != null)
    backup_block (ref p->pbackup^, p->db, block_nr => ptable_description->locking_block);
  
  /* try to place a shared lock on the locking block */
  rc = __db_lock_share (out t->locking_info, p->db, ptable_description->locking_block);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    free p->table[slot];
    p->table[slot] = null;
    return close_command (ref *p, rc, false);
  }

  t->record_size = ptable_description->record_size;


  /* finally, load all indexes */

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (ptable_description->index_description_link[i] != 0)
    {
      rc = open_block (out pindex_buffer,
                       ref *p,
                       ptable_description->index_description_link[i],
                       false);
      if (rc < 0)
      {
        for (j=0; j<i; j++)
        {
          if (t->index[j] != null)
            free t->index[j];
        }
        (void)__db_unlock (t->locking_info);
        close_block (ref *p, pbuffer);
        free p->table[slot];
        p->table[slot] = null;
        return close_command (ref *p, rc, true);   /* fatal error */
      }

      pindex_description = (DB_INDEX_DESCRIPTION *)&pindex_buffer->data;


      is_corrupt = false;

      if (pindex_description->key_size < 1 ||
          pindex_description->key_size > MAX_KEY_SIZE ||
          pindex_description->nb_key_parts < 1 ||
          pindex_description->nb_key_parts > MAX_KEY_PARTS)
      {
        is_corrupt = true;
      }
      else
      {
        size = 0;

        for (j=0; j<(int)pindex_description->nb_key_parts; j++)
        {
          if (pindex_description->key_part[j].offset
               + pindex_description->key_part[j].size
               > t->record_size)
          {
            is_corrupt = true;
            break;
          }

          size += pindex_description->key_part[j].size;

          if (size > MAX_KEY_SIZE)
          {
            is_corrupt = true;
            break;
          }
        }

        if (size == 0)
          is_corrupt = true;
      }

      if (is_corrupt)
      {
        close_block (ref *p, pindex_buffer);
        for (j=0; j<i; j++)
        {
          if (t->index[j] != null)
            free t->index[j];
        }
        (void)__db_unlock (t->locking_info);
        close_block (ref *p, pbuffer);
        free p->table[slot];
        p->table[slot] = null;
        return close_command (ref *p, E_INTERN_19, false);
      }

      t->index[i] = new DB_INDEX_DESCRIPTION ' (*pindex_description);

      close_block (ref *p, pindex_buffer);
    }
  }

  close_block (ref *p, pbuffer);    /* not needed anymore */


  rc = close_command (ref *p, 0, false);
  if (rc < 0)
  {
    /* undo all */
    for (j=0; j<MAX_INDEXES_PER_TABLE; j++)
    {
      if (t->index[j] != null)
        free t->index[j];
    }
    (void)__db_unlock (t->locking_info);
    free p->table[slot];
    p->table[slot] = null;
    return rc;
  }

  /* compute a handle for the table */
  rc = db_handle + (slot + 1);
  return rc;
}

//--------------------------------------------------------------------------------

public
int check_table_handle (int table_handle, out DB_INFO* p, out TABLE_INFO^ t)
{
  int         i, j;
  DB_INFO^    q;

  p = null;
  t = null;

  i = (table_handle >> 8);             /* get db slot nr */
  if (i < 0 || i >= MAX_OPEN_DATABASES)
    return E_BAD_HANDLE;

  q = db_info[i];
  if (q == null)              /* bad db slot */
    return E_BAD_HANDLE;
  p = &q^;

  j = (table_handle & 255);
  if (j < 1 || j > MAX_OPEN_TABLES)    /* bad table nr */
    return E_BAD_HANDLE;

  t = q^.table[j-1];

  if (t == null)         /* table is not open */
    return E_BAD_HANDLE;

  return 0;
}

//--------------------------------------------------------------------------------

/* close a table.                             */
/* returns 0 if OK, or a negative error code. */

public
int intern_db_close_table (int table_handle)
{
  int         rc, i;
  DB_INFO*    p;
  TABLE_INFO^ t;

  rc = check_table_handle (table_handle, out p, out t);
  if (rc < 0)
    return rc;

  /* unlock shared lock on locking block */
  rc = __db_unlock (t^.locking_info);
  if (rc < 0)
    return rc;

  /* free index and table slots */
  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (t^.index[i] != null)
      free t^.index[i];
  }

  free p->table[(table_handle & 255) - 1];
  p->table[(table_handle & 255) - 1] = null;

  return 0;
}

//--------------------------------------------------------------------------------

/* returns the index nr or -1 if index_name was not found */

public
int get_index_nr (TABLE_INFO^ t, string index_name)
{
  int i;

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (t^.index[i] != null && strcmp (t^.index[i]^.index_name, index_name) == 0)
      return i;
  }

  return -1;  /* not found */
}

//--------------------------------------------------------------------------------
#end unsafe
//--------------------------------------------------------------------------------
