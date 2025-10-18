
// writelog.h

use config, dbstruct;

#begin unsafe
int write_logging_data (ref DB_INFO p, byte[] buffer);
int write_logging_byte (ref DB_INFO p, BYTE b);
int write_logging_word (ref DB_INFO p, WORD w);
#end unsafe
