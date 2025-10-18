
// table.c

use ../strings;

use ../db, config, fs, dbstruct, cache, cmd, transact, dbport, index2, writelog, super;

/**********************************************************************/
#begin unsafe
/**********************************************************************/

public int check_name (string name, int max_length)
{
  int  length, i;
  char c;

  length = strlen(name);

  if (length == 0 || length > max_length)
    return E_BAD_NAME;

  for (i=0; i<length; i++)
  {
    c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || (c == '_'))
    {
      /* legal characters */;
    }
    else
    {
      return E_BAD_NAME;
    }
  }

  if (name[0] >= '0' && name[0] <= '9')
    return E_BAD_NAME;

  return 0;
}

/**********************************************************************/

int check_table_definition (TABLE_DEFINITION table_definition)
{
  uint i, j, offset;
  int  rc;

  if (table_definition.record_size < 1 ||
      table_definition.record_size > MAX_RECORD_SIZE)
    return E_BAD_RECORD_SIZE;

  if (table_definition.nb_fields < 1 ||
      table_definition.nb_fields > MAX_TABLE_FIELDS)
    return E_BAD_NB_FIELDS;

  offset = 0;

  for (i=0; i<table_definition.nb_fields; i++)
  {
    ref FIELD_DEFINITION f = table_definition.field[i];

    if (f.offset < offset || f.offset > MAX_RECORD_SIZE)
      return E_BAD_OFFSET;

    offset = f.offset;

    if (f.size < 1 || f.size > MAX_RECORD_SIZE)
      return E_BAD_SIZE;

    if (MAX_RECORD_SIZE - offset < f.size)
      return E_BAD_RECORD_SIZE;

    offset += f.size;

    if (f.type >= MAX_DB_TYPES)
      return E_BAD_TYPE;

    rc = check_name (f.name, MAX_FIELD_NAME_LENGTH);
    if (rc < 0)
      return rc;

    /* check that field name is different from all previous field names */
    for (j=0; j<i; j++)
    {
      if (strcmp (f.name, table_definition.field[j].name) == 0)
        return E_DUPLICATE_NAME;
    }
  }

  if (offset > table_definition.record_size)
    return E_BAD_RECORD_SIZE;

  return 0;
}

/**********************************************************************/

/* returns a value != 0 only in case of fatal error.           */
/* table_block_nr is non-zero if the table was found.          */
/* the buffer remains open if (rc == 0 && table_block_nr != 0) */

public int search_table_name (ref DB_INFO               p,
                              string                    table_name,
                              out LINK                  table_block_nr,
                              out BUFFER*               table_buffer,
                              out DB_TABLE_DESCRIPTION* table_description)
{
  int rc;

  table_block_nr = p.db_header.table_description_link;

  while (table_block_nr != 0)
  {
    rc = open_block (out table_buffer, ref p, table_block_nr, clear_data => false);
    if (rc < 0)
    {
      clear table_block_nr, table_buffer, table_description;
      return rc;
    }

    table_description = (DB_TABLE_DESCRIPTION *) &table_buffer->data;

    if (strcmp (table_description->table_name, table_name) == 0)  /* found ! */
      return 0;

    table_block_nr = table_description->next_table_link;

    close_block (ref p, table_buffer);
  }

  clear table_buffer, table_description;
  return 0;   /* not found : (table_block_nr == 0) */
}

/**********************************************************************/

public int intern_db_create_table (int              db_handle,
                                   string           table_name,
                                   TABLE_DEFINITION table_definition)
{
  int               rc;
  DB_INFO*          p;
  LINK              block_nr;
  BUFFER            *pbuffer;

  DB_TABLE_DESCRIPTION          *ptable_description;
  DB_EXTENDED_FIELD_DESCRIPTION *pfield_description;
  HEADER_FIELD_DESC             *phead;

  uint              index, last;
  FINFO             inf;
  WORD              i;
  int               name_length;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;


  /* check table name */

  rc = check_name (table_name, MAX_TABLE_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, fatal_error_occured => false);


  /* check table definition */

  rc = check_table_definition (table_definition);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* check that the table_name is unique */

  rc = search_table_name (ref *p,
                          table_name,
                          out block_nr,
                          out pbuffer,
                          out ptable_description);
  if (rc < 0)
    return close_command (ref *p, rc, fatal_error_occured => true);

  if (block_nr != 0)   /* table was found */
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, E_DUPLICATE_NAME, false);
  }


  /* allocate a new block for the table description */

  rc = allocate_block (out block_nr      => block_nr,
                       ref p             => *p,
                           advised_block => 0,
                           alignment     => 1);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  rc = open_block (out pbuffer,
                   ref *p,
                   block_nr,
                   clear_data => true);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error : loose block ! */

  set_dirty (ref *p, pbuffer);

  ptable_description = (DB_TABLE_DESCRIPTION *)&pbuffer->data;
  phead = &ptable_description->fields;


  /* initialize table description */

  ptable_description->next_table_link = p->db_header.table_description_link;
  p->db_header.table_description_link = block_nr;
  set_db_header_dirty (ref *p);

  strcpy (out ptable_description->table_name, table_name);

  ptable_description->record_size = (WORD)table_definition.record_size;


  /* allocate locking block */
  rc = allocate_block
       (out block_nr      => ptable_description->locking_block,
        ref p             => *p,
            advised_block => 0,
            alignment     => 1);

  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */


  /* initialize fields */

  i = 0;   /* source index of current field in table_definition */

  {
    ref byte[] f = ptable_description->field_description;

    phead->nb_fields = 0;

    index = 0;      // index into f
    last = f'size;

    while (i < table_definition.nb_fields)
    {
      name_length = strlen(table_definition.field[i].name);

      if (FINFO'size + (uint)name_length > last - index)
        break;

      /* append next field to field description */
      inf = {offset => (WORD)table_definition.field[i].offset,
             size   => (WORD)table_definition.field[i].size,
             type   => (BYTE)table_definition.field[i].type,
             field_name_length => (BYTE)name_length};

      f[index:inf'size] = inf'byte;
      index += inf'size;

      f[index:name_length] = table_definition.field[i].name[0:name_length]'byte;
      index += (uint)name_length;

      i++;
      phead->nb_fields++;
    }
  }


  while (i < table_definition.nb_fields)
  {
    /* another extended field description block is needed */

    rc = allocate_block (out block_nr      => block_nr,
                         ref p             => *p,
                             advised_block => block_nr,
                             alignment     => 1);
    if (rc < 0)
      return close_command (ref *p, rc, true);   /* fatal error */

    /* close previous block (table /or/ extended field description) */
    phead->next_field_description_link = block_nr;
    close_block (ref *p, pbuffer);

    rc = open_block (out pbuffer, ref *p, block_nr, clear_data => true);
    if (rc < 0)
      return close_command (ref *p, rc, true);   /* fatal error : loose blocks ! */

    set_dirty (ref *p, pbuffer);

    pfield_description = (DB_EXTENDED_FIELD_DESCRIPTION *)&pbuffer->data;
    phead = &pfield_description->fields;


    /* initialize extended field description block */

    {
      ref byte[] f = pfield_description->field_description;

      phead->nb_fields = 0;

      index = 0;      // index into f
      last = f'size;

      while (i < table_definition.nb_fields)
      {
        name_length = strlen(table_definition.field[i].name);

        if (FINFO'size + (uint)name_length > last - index)
          break;

        inf = {offset => (WORD)table_definition.field[i].offset,
               size   => (WORD)table_definition.field[i].size,
               type   => (BYTE)table_definition.field[i].type,
               field_name_length => (BYTE)name_length};

        f[index:inf'size] = inf'byte;
        index += inf'size;

        f[index:name_length] = table_definition.field[i].name[0:name_length]'byte;
        index += (uint)name_length;

        i++;
        phead->nb_fields++;
      }
    }
  }

  close_block (ref *p, pbuffer);


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_CREATE_TABLE);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, table_name[0:strlen(table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* table_definition */

  rc = write_logging_word (ref *p, (WORD)table_definition.record_size);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_word (ref *p, (WORD)table_definition.nb_fields);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  for (i=0; i<table_definition.nb_fields; i++)
  {
    rc = write_logging_word (ref *p, (WORD)table_definition.field[i].offset);
    if (rc < 0)
      return close_command (ref *p, rc, true);

    rc = write_logging_word (ref *p, (WORD)table_definition.field[i].size);
    if (rc < 0)
      return close_command (ref *p, rc, true);

    rc = write_logging_byte (ref *p, table_definition.field[i].type);
    if (rc < 0)
      return close_command (ref *p, rc, true);

    rc = write_logging_byte (ref *p, (BYTE)strlen(table_definition.field[i].name));
    if (rc < 0)
      return close_command (ref *p, rc, true);

    rc = write_logging_data (ref *p,
                             table_definition.field[i].name[0:strlen(table_definition.field[i].name)]);
    if (rc < 0)
      return close_command (ref *p, rc, true);
  }

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

public int intern_db_construct_table_definition (out TABLE_DEFINITION table_definition,
                                                     string           table_definition_string)
{
  ref string p = table_definition_string;
  int        pi = 0;
  int        plen = strlen(table_definition_string);

  uint             typ, i;
  FIELD_DEFINITION *f;

  clear table_definition;
//  table_definition.record_size = 0;
//  table_definition.nb_fields   = 0;

  while (pi < plen)
  {
    if (table_definition.nb_fields == MAX_TABLE_FIELDS)
      return E_BAD_NB_FIELDS;

    f = &table_definition.field[table_definition.nb_fields++];

    /* skip white space */
    while (pi < plen && p[pi] <= ' ')
      pi++;

    /* compare first type */
    for (typ=0; typ<MAX_DB_TYPES; typ++)
    {
      if (strncmp (db_field_types[typ], p[pi:plen-pi], db_field_types[typ]'length) == 0)
      {
        pi += db_field_types[typ]'length;
        break;
      }
    }

    if (typ == MAX_DB_TYPES)  /* type name was not found */
      return E_BAD_TYPE;

    f->offset = table_definition.record_size;
    f->type = (byte)typ;


    /* parse either a space or a size */

    if (pi < plen && p[pi] == ' ')
    {
      f->size = 1;
    }
    else if (pi < plen && p[pi] >= '0' && p[pi] <= '9')
    {
      f->size = (WORD)((uint)p[pi++] - (uint)'0');
      while (pi < plen && p[pi] >= '0' && p[pi] <= '9' &&
             f->size <= MAX_RECORD_SIZE / 10 &&
             MAX_RECORD_SIZE - f->size * 10 >= (uint)p[pi] - (uint)'0')
      {
        f->size = f->size * 10 + (uint)p[pi++] - (uint)'0';
      }
    }
    else
    {
      return E_SYNTAX_ERROR;
    }

    table_definition.record_size += f->size;


    /* skip white space */
    while (pi < plen && p[pi] <= ' ')
      pi++;

    /* parse the field name */
    for (i=0; ; i++)
    {
      if (pi == plen || p[pi] <= ' ')
        break;

      if (i == (uint)MAX_FIELD_NAME_LENGTH)   // too long
        return E_BAD_NAME;

      f->name[i] = p[pi++];
    }

    if (i == 0)   /* too short */
      return E_BAD_NAME;


    /* skip white space */
    while (pi < plen && p[pi] <= ' ')
      pi++;
  }

  return check_table_definition (table_definition);
}

/**********************************************************************/

public int try_exclusive_lock (ref DB_INFO p,
                               LINK        block_nr)
{
  int          rc;
  LOCKING_INFO locking_info;

  /* try to place an exclusive lock on the locking block */
  rc = __db_lock_exclusive (out locking_info,
                            p.db,
                            block_nr);
  if (rc < 0)
    return rc;

  /* ok, we can unlock it again */
  rc = __db_unlock (locking_info);
  if (rc < 0)
    return rc;       /* this is a strange error */

  return 0;
}

/**********************************************************************/

/* rename a table.                                       */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

public int intern_db_rename_table (int    db_handle,
                                   string old_table_name,
                                   string new_table_name)  /* 1 .. MAX_TABLE_NAME_LENGTH */
{
  int                   rc;
  DB_INFO*              p;
  bool                  is_duplicate;
  LINK                  block_nr;
  BUFFER*               pbuffer;
  DB_TABLE_DESCRIPTION* ptable_description;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  rc = open_command (ref *p);
  if (rc < 0)
    return rc;

  /* check old & new table names */

  rc = check_name (old_table_name, MAX_TABLE_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);

  rc = check_name (new_table_name, MAX_TABLE_NAME_LENGTH);
  if (rc < 0)
    return close_command (ref *p, rc, false);


  /* check that the new_table_name is unique */

  is_duplicate = false;

  if (strcmp (new_table_name, old_table_name) != 0)
  {
    rc = search_table_name (ref *p,
                            new_table_name,
                            out block_nr,
                            out pbuffer,
                            out ptable_description);
    if (rc < 0)
      return close_command (ref *p, rc, true);   /* fatal error */

    if (block_nr != 0)   /* table was found */
    {
      is_duplicate = true;
      close_block (ref *p, pbuffer);
    }
  }


  /* search the table definition block */

  rc = search_table_name (ref *p,
                          old_table_name,
                          out block_nr,
                          out pbuffer,
                          out ptable_description);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error */

  if (block_nr == 0)   /* table was not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);

  /* try to place an exclusive lock on the locking block */
  rc = try_exclusive_lock (ref *p, ptable_description->locking_block);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, false);   /* table is locked */
  }

  if (is_duplicate)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, E_DUPLICATE_NAME, false);
  }

  /* modify the name and rewrite it */
  set_dirty (ref *p, pbuffer);
  strcpy (out ptable_description->table_name, new_table_name);
  close_block (ref *p, pbuffer);


  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_RENAME_TABLE);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* old_table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (old_table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, old_table_name[0:strlen(old_table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);


  /* new_table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (new_table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, new_table_name[0:strlen(new_table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

/* delete a table.                                       */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

public int intern_db_delete_table (int db_handle, string table_name)
{
  int                   rc, i;
  DB_INFO*              p;
  LINK                  block_nr, predecessor_block_nr, extended_block_nr;
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


  /* search the table */

  ptable_description = null;
  pbuffer = null;

  predecessor_block_nr = 0;
  block_nr = p->db_header.table_description_link;

  while (block_nr != 0)
  {
    rc = open_block (out pbuffer, ref *p, block_nr, false);
    if (rc < 0)
      return close_command (ref *p, rc, true);   /* fatal error */

    ptable_description = (DB_TABLE_DESCRIPTION *)&pbuffer->data;

    if (strcmp (ptable_description->table_name, table_name) == 0)   /* table found ! */
    {
      break;   /* leave loop with open buffer (table_description) */
    }

    predecessor_block_nr = block_nr;
    block_nr = ptable_description->next_table_link;

    close_block (ref *p, pbuffer);
  }

  if (block_nr == 0)   /* table not found */
    return close_command (ref *p, E_TABLE_NOT_FOUND, false);

  /* try to place an exclusive lock on the locking block */
  rc = try_exclusive_lock (ref *p, ptable_description->locking_block);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);
    return close_command (ref *p, rc, false);   /* table is locked */
  }

  /* deallocate locking block */
  rc = deallocate_block (ptable_description->locking_block, ref *p);
  if (rc < 0)
  {
    close_block (ref *p, pbuffer);            /* close table description */
    return close_command (ref *p, rc, true); /* fatal error : cannot deallocate */
  }


  /* delete all index Btrees and all data records */

  {
    uint                  nb_blocks;
    BUFFER*               pbuffer2;
    DB_INDEX_DESCRIPTION* pindex_description;
    bool                  first_time = true;


    /* compute number of blocks of a data record */

    nb_blocks = required_data_blocks_for_record_size (ptable_description->record_size);

    for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
    {
      if (ptable_description->index_root_link[i] != 0)  /* an index */
      {
        rc = open_block (out pbuffer2,
                         ref *p,
                         ptable_description->index_description_link[i],
                         false);
        if (rc < 0)
        {
          close_block (ref *p, pbuffer);   /* close table description */
          return close_command (ref *p, rc, true);   /* fatal error */
        }

        pindex_description = (DB_INDEX_DESCRIPTION *)&pbuffer2->data;

        rc = recursive_delete_index (ref *p,
                                     ptable_description->index_root_link[i],
                                     *pindex_description,
                                     first_time, /* deallocate data records */
                                     nb_blocks); /* nb blocks per data record */
        if (rc < 0)
        {
          close_block (ref *p, pbuffer);
          close_block (ref *p, pbuffer2);
          return close_command (ref *p, rc, true);     /* fatal error */
        }

        first_time = false;

        close_block (ref *p, pbuffer2);
      }
    }
  }


  /* deallocate all index description blocks */

  for (i=0; i<MAX_INDEXES_PER_TABLE; i++)
  {
    if (ptable_description->index_description_link[i] != 0)  /* an index */
    {
      rc = deallocate_block (ptable_description->index_description_link[i], ref *p);
      if (rc < 0)
      {
        close_block (ref *p, pbuffer);
        return close_command (ref *p, rc, true); /* fatal error : cannot deallocate */
      }
    }
  }


  /* deallocate table from table list */

  if (predecessor_block_nr == 0)    /* front ptr in header */
  {
    p->db_header.table_description_link = ptable_description->next_table_link;
    set_db_header_dirty (ref *p);
  }
  else           /* delete node in the middle of the list */
  {
    BUFFER*               pbuffer2;
    DB_TABLE_DESCRIPTION* ptable_description2;

    rc = open_block (out pbuffer2, ref *p, predecessor_block_nr, false);
    if (rc < 0)
    {
      close_block (ref *p, pbuffer);
      return close_command (ref *p, rc, true);   /* fatal error */
    }

    ptable_description2 = (DB_TABLE_DESCRIPTION *)&pbuffer2->data;

    set_dirty (ref *p, pbuffer2);
    ptable_description2->next_table_link = ptable_description->next_table_link;

    close_block (ref *p, pbuffer2);
  }


  /* deallocate all extended field description blocks */

  extended_block_nr = ptable_description->fields.next_field_description_link;

  while (extended_block_nr != 0)
  {
    BUFFER*                        pbuffer2;
    DB_EXTENDED_FIELD_DESCRIPTION* fdes;
    LINK                           next_link;

    rc = open_block (out pbuffer2, ref *p, extended_block_nr, false);
    if (rc < 0)
    {
      close_block (ref *p, pbuffer);
      return close_command (ref *p, rc, true);   /* fatal error */
    }

    fdes = (DB_EXTENDED_FIELD_DESCRIPTION *)&pbuffer2->data;

    next_link = fdes->fields.next_field_description_link;

    close_block (ref *p, pbuffer2);

    rc = deallocate_block (extended_block_nr, ref *p);
    if (rc < 0)
    {
      close_block (ref *p, pbuffer);
      return close_command (ref *p, rc, true);  /* fatal error : cannot deallocate */
    }

    extended_block_nr = next_link;
  }


  close_block (ref *p, pbuffer);     /* we don't need it anymore */


  /* finally, deallocate table description block */

  rc = deallocate_block (block_nr, ref *p);
  if (rc < 0)
    return close_command (ref *p, rc, true);   /* fatal error : cannot deallocate */

  /* write logging information */

  /* logging record type */

  rc = write_logging_byte (ref *p, LOGTYPE_DB_DELETE_TABLE);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  /* table_name */

  rc = write_logging_byte (ref *p, (BYTE)strlen (table_name));
  if (rc < 0)
    return close_command (ref *p, rc, true);

  rc = write_logging_data (ref *p, table_name[0:strlen(table_name)]);
  if (rc < 0)
    return close_command (ref *p, rc, true);

  return close_command (ref *p, 0, false);
}

/**********************************************************************/

/* check nb key parts & field name syntax */

public int check_index_definition (INDEX_DEFINITION index_definition)
{
  uint i;
  int  rc;

  if (index_definition.nb_parts < 1 ||
      index_definition.nb_parts > MAX_KEY_PARTS)
    return E_BAD_NB_KEY_PARTS;

  for (i=0; i<index_definition.nb_parts; i++)
  {
    rc = check_name (index_definition.part[i].name, MAX_FIELD_NAME_LENGTH);
    if (rc < 0)
      return rc;
  }

  return 0;
}

/**********************************************************************/

int append_fields (    HEADER_FIELD_DESC    head,
                       byte[]               s,
                   ref TABLE_DEFINITION     table_definition)
{
  int   si, name_length;
  uint  i;
  FINFO inf;

  si = 0;
  for (i=0; i<head.nb_fields; i++)
  {
    ref FIELD_DEFINITION t = table_definition.field[table_definition.nb_fields++];

    if (inf'size > s'size - (uint)si)
      return -1;

    inf'byte = s[si:inf'size];
    si += (int)inf'size;

    t.offset = inf.offset;
    t.size   = inf.size;
    t.type   = inf.type;
    name_length = inf.field_name_length;

    if (name_length <= 0 || name_length > t.name'length)
      return -1;

    if (name_length > (int)s'size - si)
      return -1;

    t.name[0:name_length]'byte = s[si:name_length];
    si += name_length;

    // check if no space left in structure
    if (table_definition.nb_fields == (uint)table_definition.field'length &&
        i+1 < head.nb_fields)
      return -1;
  }

  return 0;
}

/**********************************************************************/

public int load_table_definition (    LINK             table_description_block_nr,
                                  ref DB_INFO          p,
                                  out TABLE_DEFINITION table_definition)
{
  int                            rc;
  BUFFER*                        pbuffer;
  DB_TABLE_DESCRIPTION*          ptable_description;
  LINK                           block_nr;
  DB_EXTENDED_FIELD_DESCRIPTION* pextended_fdes;

  clear table_definition;

  rc = open_block (out pbuffer, ref p, table_description_block_nr, false);
  if (rc < 0)
    return rc;

  ptable_description = (DB_TABLE_DESCRIPTION *)&pbuffer->data;

  table_definition.record_size = ptable_description->record_size;
  table_definition.nb_fields   = 0;

  if (append_fields (    ptable_description->fields,
                         ptable_description->field_description
                           [0:ptable_description->field_description'size],
                     ref table_definition) < 0)
    return E_INTERN_28;

  block_nr = ptable_description->fields.next_field_description_link;

  close_block (ref p, pbuffer);


  while (block_nr != 0)
  {
    rc = open_block (out pbuffer, ref p, block_nr, false);
    if (rc < 0)
      return rc;

    pextended_fdes = (DB_EXTENDED_FIELD_DESCRIPTION *)&pbuffer->data;

    if (append_fields (    pextended_fdes->fields,
                           pextended_fdes->field_description
                              [0:pextended_fdes->field_description'size],
                       ref table_definition) < 0)
      return E_INTERN_28;

    block_nr = pextended_fdes->fields.next_field_description_link;

    close_block (ref p, pbuffer);
  }

  rc = check_table_definition (table_definition);
  if (rc < 0)
    return rc;

  return 0;
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
