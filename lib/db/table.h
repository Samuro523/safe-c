
// table.h

use ../db, config, dbstruct, fs;

#begin unsafe

int check_name (string name, int max_length);


/* returns a value != 0 only in case of fatal error.           */
/* table_block_nr is non-zero if the table was found.          */
/* the buffer remains open if (rc == 0 && table_block_nr != 0) */

int search_table_name (ref DB_INFO               p,
                       string                    table_name,
                       out LINK                  table_block_nr,
                       out BUFFER*               table_buffer,
                       out DB_TABLE_DESCRIPTION* table_description);


int intern_db_create_table (int              db_handle,
                            string           table_name,
                            TABLE_DEFINITION table_definition);


int intern_db_construct_table_definition (out TABLE_DEFINITION table_definition,
                                          string               table_definition_string);


int intern_db_rename_table (int    db_handle,
                            string old_table_name,
                            string new_table_name);   /* 1 .. MAX_TABLE_NAME_LENGTH */

int intern_db_delete_table (int db_handle, string table_name);

int check_index_definition (INDEX_DEFINITION index_definition);

int load_table_definition (LINK                 table_description_block_nr,
                           ref DB_INFO          p,
                           out TABLE_DEFINITION table_definition);

int try_exclusive_lock (ref DB_INFO p,
                        LINK        block_nr);

#end unsafe
