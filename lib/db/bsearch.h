
// bsearch.h

use config, fs, dbstruct;

#begin unsafe

int search_btree
            (ref DB_INFO          p,
             LINK                 table_block_nr,
             int                  index_nr,
             DB_INDEX_DESCRIPTION index_description,
             byte[]               data_record,
             uint2                search_mode,
             out bool             record_not_found,
             out LINK             data_record_block_nr);

#end unsafe
