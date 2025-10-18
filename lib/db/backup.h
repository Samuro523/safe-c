
// backup.h

use dbstruct, ../db;

long db_get_database_approximate_size (int db_handle);

int intern_db_backup_database (int db_handle, string output_filename, USER_CALLBACK user_callback = null);

void backup_block (ref BACKUP_INFO bi, int db, long block_nr);
