
// badd.h

use config, fs, dbstruct;

void compute_key (    DB_INDEX_DESCRIPTION index_description,
                      byte[]               data_record,
                  out byte[]               the_key);

int read_item (    DB_INDEX_DESCRIPTION index_description,
                   XHEADER              index_node_head,
                   byte[]               index_node_data,
               ref uint                 offset,
               out INDEX_ITEM           the_item,
                   INDEX_ITEM           previous_item);

int write_item (    DB_INDEX_DESCRIPTION  index_description,
                ref XHEADER               index_node_head,
                ref byte[]                index_node_data,
                ref uint                  offset,
                    INDEX_ITEM            the_item,
                    INDEX_ITEM            previous_item);

int copy_node_and_insert_items
               (    DB_INDEX_DESCRIPTION  index_description,
                out XHEADER               target_node_head,   // actual size is always huge
                out byte[]                target_node_data,   // actual size is always huge
                    XHEADER               source_node_head,   // actual size can be normal or huge
                    byte[]                source_node_data,   // actual size can be normal or huge
                    uint                  insertion_point,
                    INDEX_ITEM            new_item1,
                    bool                  item2_provided,
                    INDEX_ITEM            new_item2);

int split_node(    DB_INDEX_DESCRIPTION  index_description,
                   XHEADER               source_node_head,   // actual size is always huge
                   byte[]                source_node_data,   // actual size is always huge
               out XHEADER               left_node_head,
               out byte[]                left_node_data,
               out INDEX_ITEM            emerging_item,
               out DB_INDEX              right_node,
                   LINK                  emerging_child_btree);

int compute_fils
    (    DB_INDEX_DESCRIPTION  index_description,
         DB_INDEX              index_node,
         byte[]                key,
     out bool                  record_exists,
     out LINK                  existing_data_record_block_nr,
     out LINK                  fils,
     out uint                  insertion_point);

#begin unsafe
int insert_item_in_node
          (ref DB_INFO               p,
               DB_INDEX_DESCRIPTION  index_description,
           ref XHEADER               index_node_head,    // actual size depends on index_node_max_offset
           ref byte[]                index_node_data,    // actual size depends on index_node_max_offset
               uint                  insertion_point,
               INDEX_ITEM            new_item,
               LINK                  advised_block_nr,   // for allocation
           out bool                  overflow,
           out INDEX_ITEM            overflow_item);

int add_btree
            (ref DB_INFO              p,
                 LINK                 table_block_nr,
                 int                  index_nr,
                 DB_INDEX_DESCRIPTION index_description,
                 byte[]               data_record,
                 LINK                 data_record_block_nr,
             out bool                 record_exists,
             out LINK                 existing_data_record_block_nr);
#end unsafe
