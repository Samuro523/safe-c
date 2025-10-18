
// query.h

use ../db;


#begin unsafe

int intern_db_query_table_name (int  db_handle,
                                int  table_nr,
                                out string(MAX_TABLE_NAME_LENGTH) table_name);

int intern_db_query_table_definition (int                  db_handle,
                                      string               table_name,
                                      out TABLE_DEFINITION table_definition);

int intern_db_query_index_name (int    db_handle,
                                string table_name,
                                int    index_nr,
                                out string(MAX_INDEX_NAME_LENGTH) index_name);

int intern_db_query_index_definition (int                  db_handle,
                                      string               table_name,
                                      string               index_name,
                                      out INDEX_DEFINITION index_definition);

int intern_db_get_write_transaction_count (int db_handle, out long count);

#end unsafe

