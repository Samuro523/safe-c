
// transact.h

use dbstruct;

int intern_db_open_database (string database_filename, bool open_exclusive, bool open_also_logfile);
int intern_db_close_database (int db_handle);

#begin unsafe
int check_db_handle (int db_handle, out DB_INFO* p);

int open_command (ref DB_INFO p);

int close_command (ref DB_INFO p,
                   int         command_rc,
                   bool        fatal_error_occured);

int intern_db_begin_transaction    (int db_handle);
int intern_db_end_transaction      (int db_handle);
int intern_db_rollback_transaction (int db_handle);

int lock_header (ref DB_INFO p);
void unlock_header (ref DB_INFO p);
#end unsafe
