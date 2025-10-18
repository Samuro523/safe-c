
// cmd.h

uint required_data_blocks_for_record_size (uint record_size);

int intern_db_retrieve (int table_handle, ref byte[] record, uint2 retrieve_mode, string index_name);
int intern_db_insert (int table_handle, byte[] record);
int intern_db_delete (int table_handle, byte[] record, string index_name);
int intern_db_update (int table_handle, byte[] record, string index_name);
