
// fs.c

use config;

//-------------------------------------------------------------------------------

// this function need not be called, all assertions are constant.

void validate_data_structures ()
{
  assert DB_CHAINED_DATA'size               == DB_BLOCK_SIZE;
  assert DB_DATA'size                       == DB_BLOCK_SIZE;
  assert DB_INDEX'size                      == DB_BLOCK_SIZE;
  assert DB_INDEX_DESCRIPTION'size          == DB_BLOCK_SIZE;
  assert DB_TABLE_DESCRIPTION'size          == DB_BLOCK_SIZE;
  assert DB_EXTENDED_FIELD_DESCRIPTION'size == DB_BLOCK_SIZE;
  assert DB_ROLLBACK'size                   == DB_BLOCK_SIZE;
  assert DB_HEADER'size                     == DB_BLOCK_SIZE;
  assert LOGGING_HEADER'size                <= 256;
}

//-------------------------------------------------------------------------------
