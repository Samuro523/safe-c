
// open.h

use ../db, dbstruct;

#begin unsafe

int intern_db_open_table (int               db_handle,
                          string            table_name,
                          TABLE_DEFINITION* ptable_definition);  // optional, can be null

int check_table_handle (int table_handle, out DB_INFO* p, out TABLE_INFO^ t);

int intern_db_close_table (int table_handle);

int get_index_nr (TABLE_INFO^ t, string index_name);

#end unsafe
