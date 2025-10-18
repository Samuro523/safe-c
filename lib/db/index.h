
// index.h

use ../db;

int intern_db_create_index (int              db_handle,
                            string           table_name,
                            string           index_name, /* MAX_INDEX_NAME_LENGTH */
                            INDEX_DEFINITION index_definition);

int intern_db_construct_index_definition (out INDEX_DEFINITION index_definition,
                                              string index_definition_string);

int intern_db_rename_index
                    (int    db_handle,
                     string table_name,
                     string old_index_name,
                     string new_index_name);  /* 1 .. MAX_INDEX_NAME_LENGTH */

int intern_db_delete_index (int    db_handle,
                            string table_name,
                            string index_name);

