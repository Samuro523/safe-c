
// dbfile.h

use ../db;

int intern_db_create_database (string database_filename);

int intern_db_set_logging_filename (string database_filename,
                                    string logging_filename,
                                    bool   create_logging_file);
                                    
int intern_db_get_logging_filename (string database_filename,
                                    out string(MAX_LOGGING_FILENAME_LENGTH) logging_filename);
   
int intern_db_delete_database (string database_filename);

int intern_db_recover (string database_filename, string logging_filename);

