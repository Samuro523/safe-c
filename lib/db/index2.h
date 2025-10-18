
// index2.h

use config, fs, dbstruct;

#begin unsafe


int recursive_delete_index
            (ref DB_INFO          p,
             LINK                 btree_root,
             DB_INDEX_DESCRIPTION index_description,
             bool                 deallocate_data_records,
             uint                 nb_blocks_per_data_record);


int recursive_build_index
            (ref DB_INFO          p,
             LINK                 btree_root,
             DB_INDEX_DESCRIPTION index_description,
             LINK                 table_block_nr,
             int                  new_index_nr,
             DB_INDEX_DESCRIPTION new_index_description,
             byte[]^              data_record_buffer,
             uint                 nb_blocks_per_data_record,
             WORD                 record_size);


#end unsafe

