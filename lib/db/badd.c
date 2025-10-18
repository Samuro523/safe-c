
// badd.c

#define debug_index_node  0

#if debug_index_node
  use ../tracing;
#endif

use ../db, config, fs, dbstruct, cache, super;

/**************************************************************************/

public void compute_key (    DB_INDEX_DESCRIPTION index_description,
                             byte[]               data_record,
                         out byte[]               the_key)
{
  int   i, nb_parts;
  uint  x, ofs;

  clear the_key;

  nb_parts = (int)index_description.nb_key_parts;
  ofs = 0;  // target index

  for (i=0; i<nb_parts; i++)
  {
    ref A_KEY_PART k = index_description.key_part[i];

    if ((k.cv_flags & CV_ORDER) != 0)  /* load in reverse order */
    {
      if ((k.cv_flags & CV_WSTRING) != 0)  /* 2 bytes by 2 bytes */
      {
        for (x=0; x+1<k.size; x+=2)
        {
          the_key[ofs+x] = data_record[k.offset+x+1];
          the_key[ofs+x+1] = data_record[k.offset+x];
        }
      }
      else
      {
        for (x=0; x<k.size; x++)
          the_key[ofs+x] = data_record[k.offset + ((k.size - 1) - x)];
      }
    }
    else                          /* load in order */
    {
      the_key[ofs:k.size] = data_record[k.offset:k.size];
    }

    // the_key[ofs : k.size] is now in lexical order

    if ((k.cv_flags & CV_FLOAT) != 0)
    {
      if (the_key[ofs] >= 128)   // negative float
      {
        uint idx = ofs + (k.size-1);
        int  borrow = 0;
        for (;;)
        {
          int val = -(int)the_key[idx] - borrow;
          the_key[idx] = (byte)val;
          if (idx == ofs)
            break;
          idx--;
          borrow = val < 0 ? 1 : 0;
        }

        the_key[ofs] |= 128;
      }
    }

    if ((k.cv_flags & CV_SIGNED) != 0)
    {
      the_key[ofs] ^= 128;     /* reverse sign bit of first byte */
    }

    if ((k.cv_flags & CV_STRING) != 0)
    {
      for (x=0; x<k.size; x++)
      {
        if (the_key[ofs+x] == 0)
          break;
      }
      clear the_key[ofs+x:k.size-x];
    }
    else if ((k.cv_flags & CV_WSTRING) != 0)
    {
      for (x=0; x+1<k.size; x+=2)
      {
        if (the_key[ofs+x] == 0 && the_key[ofs+x+1] == 0)
          break;
      }
      clear the_key[ofs+x:k.size-x];
    }

    ofs += k.size;
  }
}

/**************************************************************************/

void compress_key (    DB_INDEX_DESCRIPTION index_description,
                       byte[]               key,
                   out INDEX_ITEM           compressed_key)
{
  uint nb_parts, i, source, target, size, length, nb_field_compression_bytes;
  byte kp_flags;

  clear compressed_key;

  nb_field_compression_bytes = 0;

  nb_parts = index_description.nb_key_parts;
  source   = 0;
  target   = 0;

  for (i=0; i<nb_parts; i++)
  {
    size     = index_description.key_part[i].size;
    kp_flags = index_description.key_part[i].kp_flags;

    if (kp_flags != 0)       /* a compressed field */
    {
      if ((kp_flags & KP_STRING_COMPRESSION) != 0)
      {
        for (length=0; length<size; length++)
        {
          if (key[source+length] == 0)
            break;
        }
      }
      else if ((kp_flags & KP_WSTRING_COMPRESSION) != 0)
      {
        for (length=0; length<size; length+=2)
        {
          if (key[source+length] == 0 && key[source+length+1] == 0)
            break;
        }
      }
      else if ((kp_flags & KP_TRAILING_BYTES_COMPRESSION) != 0)
      {
        length = size;
        while (length >= 2)    /* at least two bytes remaining */
        {
          if (key[source+length-1] != key[source+length-2])
            break;
          length--;
        }
      }
      else
      {
        abort;
      }

      compressed_key.compressed_key[target:length] = key[source:length];
      target += length;

      compressed_key.field_compression_byte[nb_field_compression_bytes++] = (BYTE)(size - length);
    }
    else             /* a non-compressed field */
    {
      compressed_key.compressed_key[target:size] = key[source:size];
      target += size;
    }

    source += size;
  }

  compressed_key.compressed_key_size = target;

  assert (nb_field_compression_bytes == index_description.kp_info_size);
  assert (source == index_description.key_size);
}

/**************************************************************************/

int uncompress_key (    DB_INDEX_DESCRIPTION index_description,
                        INDEX_ITEM           compressed_key,
                    out byte[]               key)
{
  uint nb_parts, nb_field_compression_bytes, nb_compressed, length;
  uint source, target, i, size;
  byte kp_flags;

  clear key;

  nb_field_compression_bytes = 0;

  nb_parts = index_description.nb_key_parts;
  source = 0;
  target = 0;

  for (i=0; i<nb_parts; i++)
  {
    size     = index_description.key_part[i].size;
    kp_flags = index_description.key_part[i].kp_flags;

    if (kp_flags != 0)
    {
      /* a compressed field */
      nb_compressed = compressed_key.field_compression_byte[nb_field_compression_bytes++];
      if (nb_compressed > size)
        return E_INTERN_7;

      length = size - nb_compressed;
      key[target:length] = compressed_key.compressed_key[source:length];
      source += length;

      if ((kp_flags & (KP_STRING_COMPRESSION|KP_WSTRING_COMPRESSION)) != 0)
        clear key[target+length:nb_compressed];
      else if ((kp_flags & KP_TRAILING_BYTES_COMPRESSION) != 0)
        key[target+length:nb_compressed] = {all => key[target+length-1]};
      else
        abort;
    }
    else    /* a non-compressed field */
    {
      key[target:size] = compressed_key.compressed_key[source:size];
      source += size;
    }

    target += size;
  }

  assert (nb_field_compression_bytes == index_description.kp_info_size);
  assert (source == compressed_key.compressed_key_size);
  assert (target == index_description.key_size);

  return 0;
}

/**************************************************************************/

/* 'offset' is the read pointer; it is increased by the function.     */
/* 'the_item' should be copied into 'previous_item' after each call.  */
/* 'the_item' and 'previous_item' can denote the same variable.       */
/* 'previous_item' is unused for the first call.                      */

public int read_item (    DB_INDEX_DESCRIPTION index_description,
                          XHEADER              index_node_head,
                          byte[]               index_node_data,
                      ref uint                 offset,
                      out INDEX_ITEM           the_item,
                          INDEX_ITEM           previous_item)
{
  uint        size, u, nb_leading_bytes, nb_bytes_compressed;
  INDEX_ITEM  prev_item = previous_item;

  clear the_item;

  if (offset >= index_node_head.nb_data_bytes || index_node_head.nb_data_bytes > index_node_data'size)
    return E_INTERN_5;    /* inconsistent index data */


  /* get key compression byte */

  nb_leading_bytes = 0;

  if (offset > LINK'size &&      // we are after first item
      (index_description.k_flags & K_LEADING_BYTES_COMPRESSION) != 0)
  {
    if (offset + 1 > index_node_head.nb_data_bytes)
      return E_INTERN_5;    /* inconsistent index data */

    nb_leading_bytes = THE_BYTE (index_node_data[offset]);
    offset++;
  }


  /* get key part compression bytes */

  size = index_description.kp_info_size;

  if (offset + size > index_node_head.nb_data_bytes)
    return E_INTERN_5;    /* inconsistent index data */

  the_item.field_compression_byte[0:size] = index_node_data[offset:size];
  offset += size;

  nb_bytes_compressed = 0;
  for (u=0; u<size; u++)
    nb_bytes_compressed += the_item.field_compression_byte[u];


  /* get compressed key value */

  if (index_description.key_size < nb_leading_bytes + nb_bytes_compressed)
    return E_INTERN_4;    /* inconsistent index data */

  the_item.compressed_key[0:nb_leading_bytes] = prev_item.compressed_key[0:nb_leading_bytes];

  size = index_description.key_size - nb_bytes_compressed - nb_leading_bytes;

  if (offset + size > index_node_head.nb_data_bytes)
    return E_INTERN_5;    /* inconsistent index data */

  the_item.compressed_key[nb_leading_bytes:size] = index_node_data[offset:size];
  offset += size;

  the_item.compressed_key_size = (WORD)(nb_leading_bytes + size);


  /* get data record link */

  if (offset + LINK'size > index_node_head.nb_data_bytes)
    return E_INTERN_5;    /* inconsistent index data */

  the_item.data_record_block_nr'byte = index_node_data[offset:LINK'size];
  offset += LINK'size;


  /* get child btree link */

  if (index_node_head.has_children)
  {
    if (offset + LINK'size > index_node_head.nb_data_bytes)
      return E_INTERN_5;    /* inconsistent index data */

    the_item.child_btree'byte = index_node_data[offset:LINK'size];
    offset += LINK'size;
  }
  else
  {
    the_item.child_btree = 0;
  }

  return 0;
}

/**************************************************************************/

/* 'offset' is the write pointer; it is increased by the function.    */
/* 'the_item' should be copied into 'previous_item' after each call.  */
/* 'previous_item' is unused for the first call.                      */
/* the function updates the 'index_node_head.nb_data_bytes' field.    */

public int write_item (    DB_INDEX_DESCRIPTION  index_description,
                       ref XHEADER               index_node_head,
                       ref byte[]                index_node_data,
                       ref uint                  offset,
                           INDEX_ITEM            the_item,
                           INDEX_ITEM            previous_item)
{
  uint  size, u, nb_leading_bytes, max_leading_bytes;

  if (offset > index_node_data'size)
    return E_INTERN_20;    /* index full */


  /* write key compression byte */

  nb_leading_bytes = 0;

  if (offset > LINK'size &&      // we are after first item
      (index_description.k_flags & K_LEADING_BYTES_COMPRESSION) != 0)
  {
    /* compute nb of common bytes with previous compressed key */

    size = the_item.compressed_key_size;
    if (previous_item.compressed_key_size < size)
      size = previous_item.compressed_key_size;

    max_leading_bytes = MAX_KEY_SIZE
                        - ((uint)((index_description.k_flags & K_LEADING_BYTES_COMPRESSION) != 0)
                           + index_description.key_size + index_description.kp_info_size);

    for (u=0; u<size && u<max_leading_bytes; u++)
    {
      if (the_item.compressed_key[u] != previous_item.compressed_key[u])
        break;
    }

    nb_leading_bytes = u;

    if (offset + 1 > index_node_data'size)
      return E_INTERN_20;    /* index full */

    index_node_data[offset++] = (BYTE)nb_leading_bytes;
  }


  /* write key part compression bytes */

  size = index_description.kp_info_size;

  if (offset + size > index_node_data'size)
    return E_INTERN_20;    /* index full */

  index_node_data[offset:size] = the_item.field_compression_byte[0:size];
  offset += size;


  /* write compressed key */

  size = the_item.compressed_key_size - nb_leading_bytes;

  if (offset + size > index_node_data'size)
    return E_INTERN_20;    /* index full */

  index_node_data[offset:size] = the_item.compressed_key[nb_leading_bytes:size];
  offset += size;


  /* write data record link */

// trace ("offset %u : writing data block link\n", offset);

  if (offset + LINK'size > index_node_data'size)
    return E_INTERN_20;    /* index full */

  index_node_data[offset:LINK'size] = the_item.data_record_block_nr'byte;
  offset += LINK'size;


  /* write child btree link */

  if (index_node_head.has_children)
  {
    if (offset + LINK'size > index_node_data'size)
      return E_INTERN_20;    /* index full */

    index_node_data[offset:LINK'size] = the_item.child_btree'byte;
    offset += LINK'size;
  }

  index_node_head.nb_data_bytes = (WORD)offset;

  return 0;
}

/**************************************************************************/

#if debug_index_node

void debug_index_description (DB_INDEX_DESCRIPTION index_description)
{
  trace ("-------------------------------------------------\n");
  trace ("index name   : %s\n", index_description.index_name);
  trace ("key_size     : %u\n", index_description.key_size);
  trace ("k_flags      : %u\n", index_description.k_flags);
  trace ("nb_key_parts : %u\n", index_description.nb_key_parts);
  trace ("kp_info_size : %u\n", index_description.kp_info_size);
  trace ("-------------------------------------------------\n");
}


void debug_node (DB_INDEX_DESCRIPTION  index_description,
                 DB_INDEX              index_node,
                 uint                  max_offset_size)
{
  INDEX_ITEM  item;
  uint        offset, i;
  int         rc;
  BYTE        key[MAX_KEY_SIZE];

  debug_index_description (index_description);

  trace ("node (size = %3u)\n", index_node.nb_data_bytes);
  assert index_node.nb_data_bytes <= max_offset_size;

  offset = 0;

  if ((index_node.flags & NODE_HAS_CHILDREN) != 0)
  {
    trace ("next0 link : %d\n", THE_LINK (index_node.data));
    assert (THE_LINK (index_node.data) != 0);
    offset = LINK'size;
  }


  clear item;

  for (;;)
  {
    if (offset == index_node.nb_data_bytes)  /* no more items */
      break;

    trace ("item (at offset %3u) : ", offset);

    rc = read_item (    index_description,
                        index_node,
                    ref offset,
                        max_offset_size,
                    out item,
                        item);  /* previous read item or new item ! */

    assert (rc == 0);

    trace ("(compressed_key=");
    for (i=0; i<item.compressed_key_size; i++)
    {
      trace ("%02x", item.compressed_key[i]);
      if (i < item.compressed_key_size - 1)
        trace (" ");
    }
    trace (", ");
    trace ("compression_bytes=");
    for (i=0; i<index_description.kp_info_size; i++)
    {
      trace ("%02x", item.field_compression_byte[i]);
      if (i + 1 < index_description.kp_info_size)
        trace (" ");
    }
    trace (", ");

    clear key;
    rc = uncompress_key (index_description, item, out key);
    assert rc == 0;
    trace ("key=");

    for (i=0; i<index_description.key_size; i++)
    {
      trace ("%02x", key[i]);
      if (i < index_description.key_size - 1)
        trace (" ");
    }

#if 0
//    trace_block (key);

    for (i=0; i<index_description.key_size; i+=4)
    {
      trace ("%u",
             (key[i] << 24) + (key[i+1] << 16) + (key[i+2] << 8) + key[i+3]);
      if (i+4 < index_description.key_size)
        trace (" ");
    }
#endif

    trace (", data=%d", item.data_record_block_nr);

    assert item.data_record_block_nr != 0;

    if ((index_node.flags & NODE_HAS_CHILDREN) != 0)
    {
      trace (", next=%d", item.child_btree);
      assert item.child_btree != 0;
    }
    trace (")\n");
  }

  trace_block (((byte *)&index_node.data)[0:index_node.nb_data_bytes]);
  trace ("-------------------------------------------------------\n");
}

#endif

/**************************************************************************/

/* copy 'source_node' to 'target_node',                                 */
/*   and insert (new_item1 || new_item2) at position 'insertion_point'. */
/* 'target_node' can be larger than the block size.                     */
/* if 'new_item2' is not provided, it is not catenated.                 */

public
int copy_node_and_insert_items
               (    DB_INDEX_DESCRIPTION  index_description,
                out XHEADER               target_node_head,   // actual size is always huge
                out byte[]                target_node_data,   // actual size is always huge
                    XHEADER               source_node_head,   // actual size can be normal or huge
                    byte[]                source_node_data,   // actual size can be normal or huge
                    uint                  insertion_point,
                    INDEX_ITEM            new_item1,
                    bool                  item2_provided,
                    INDEX_ITEM            new_item2)
{
  uint        source_offset, target_offset, remaining_bytes;
  INDEX_ITEM  the_item, previously_written_item;
  int         rc;

#if debug_index_node
  trace ("copy_node_and_insert_items(SOURCE) : \n");
  debug_node (index_description, *(DB_INDEX *)&source_node, source_node_max_offset);
#endif

  clear target_node_head, target_node_data;

  source_offset = 0;

  if (source_node_head.has_children)
    source_offset = LINK'size;


  /* advance until reaching the insertion point */

  clear the_item;

  while (source_offset != insertion_point)
  {
    rc = read_item (    index_description,
                        source_node_head,
                        source_node_data,
                    ref source_offset,
                    out the_item,
                        the_item);     // previously read item - empty at first call
    if (rc < 0)
      return rc;
  }


  /* copy the already scanned data to the target node */

  /* "target_node_head.nb_data_bytes" is not initialized here */
  target_node_head.has_children     = source_node_head.has_children;
  target_node_data[0:source_offset] = source_node_data[0:source_offset];


  /* copy now the new_items */

  target_offset = source_offset;

  rc = write_item (    index_description,
                   ref target_node_head,
                   ref target_node_data,
                   ref target_offset,
                       new_item1,
                       the_item);       // previous item
  if (rc < 0)
    return rc;

#if debug_index_node
  trace ("copy_node_and_insert_items(STEP 1) : \n");
  debug_node (index_description, *(DB_INDEX *)&target_node, target_node.data'size);
#endif


  previously_written_item = new_item1;

  if (item2_provided)
  {
    rc = write_item (    index_description,
                     ref target_node_head,
                     ref target_node_data,
                     ref target_offset,
                         new_item2,
                         new_item1);      // previously written item
    if (rc < 0)
      return rc;

    previously_written_item = new_item2;

#if debug_index_node
  trace ("copy_node_and_insert_items(STEP 2) : \n");
  debug_node (index_description, *(DB_INDEX *)&target_node, target_node.data'size);
#endif
  }


  /* copy the next item, if any */

  if (source_offset != source_node_head.nb_data_bytes)   // more items
  {
    rc = read_item (    index_description,
                        source_node_head,
                        source_node_data,
                    ref source_offset,
                    out the_item,
                        the_item);     // previously read item
    if (rc < 0)
      return rc;

    rc = write_item (    index_description,
                     ref target_node_head,
                     ref target_node_data,
                     ref target_offset,
                         the_item,
                         previously_written_item);
    if (rc < 0)
      return rc;

#if debug_index_node
  trace ("copy_node_and_insert_items(STEP 3) : \n");
  debug_node (index_description, *(DB_INDEX *)&target_node, target_node.data'size);
#endif
  }


  // copy the remaining bytes

  remaining_bytes = source_node_head.nb_data_bytes - source_offset;

  if (target_offset + remaining_bytes > target_node_data'size)
    return E_INTERN_21;

  target_node_data[target_offset:remaining_bytes] = source_node_data[source_offset:remaining_bytes];

  target_node_head.nb_data_bytes = (WORD)(target_offset + remaining_bytes);

#if debug_index_node
  trace ("copy_node_and_insert_items(TARGET) : \n");
  debug_node (index_description, *(DB_INDEX *)&target_node, target_node.data'size);
#endif

  return 0;
}

/**************************************************************************/

/* 'source_node' is larger than the allowed max_data_size;          */
/*     it contains at least 3 items.                                */
/* it is split into 'left_node', 'overflow_item' and 'right_node'.  */
/* 'emerging_child_btree' is copied into emerging_item.child_btree. */

public
int split_node
              (    DB_INDEX_DESCRIPTION  index_description,
                   XHEADER               source_node_head,   // actual size is always huge
                   byte[]                source_node_data,   // actual size is always huge
               out XHEADER               left_node_head,
               out byte[]                left_node_data,
               out INDEX_ITEM            emerging_item,
               out DB_INDEX              right_node,
                   LINK                  emerging_child_btree)
{
  uint        source_offset, left_size, right_offset, remaining_bytes, split_point;
  int         rc, count_items, emerging_idx, emerging_max_idx;
  INDEX_ITEM  the_item;

  clear left_node_head, left_node_data, emerging_item, right_node;

  /* select an emerging_item so that :                             */
  /* . it is neither the first nor the last item,                  */
  /* . neither the left nor the right node underflows,             */
  /* . both left and right nodes have approximately the same size. */

  assert (source_node_head.nb_data_bytes > MAX_INDEX_DATA_SIZE);


  split_point = LINK'size + (MAX_INDEX_DATA_SIZE - LINK'size) / 2;     // normally, 256


  /* count items */

  count_items = 0;
  emerging_max_idx = 0;
  emerging_idx = 0;
  source_offset = 0;

  if (source_node_head.has_children)
    source_offset = LINK'size;

  clear the_item;

  for (;;)
  {
    if (source_offset <= MAX_INDEX_DATA_SIZE)
      emerging_max_idx = count_items;

    if (source_offset <= split_point)
      emerging_idx = count_items;

    rc = read_item (    index_description,
                        source_node_head,
                        source_node_data,
                    ref source_offset,
                    out the_item,
                        the_item);     // previously read item
    if (rc < 0)
      return rc;

    count_items++;

    if (source_offset == source_node_head.nb_data_bytes)
      break;
  }

  assert count_items >= 3;

  if (emerging_max_idx > count_items - 2)
    emerging_max_idx = count_items - 2;

  if (emerging_idx < 1)
    emerging_idx = 1;
  if (emerging_idx > emerging_max_idx)
    emerging_idx = emerging_max_idx;

  for (;;)
  {
    /* scan source items until some condition occurs */

    count_items = 0;
    source_offset = 0;

    if (source_node_head.has_children)
      source_offset = LINK'size;

    clear the_item;

    for (;;)
    {
      left_size = source_offset;   // save current size of left node

      rc = read_item (    index_description,
                          source_node_head,
                          source_node_data,
                      ref source_offset,
                      out the_item,
                          the_item);     // previously read item
      if (rc < 0)
        return rc;

      if (count_items == emerging_idx)   // this item will emerge
        break;

      count_items++;
    }

    if (left_size > MAX_INDEX_DATA_SIZE)
      return E_INTERN_18;    /* inconsistent index data */

    /* terminate the left node */
    left_node_head.nb_data_bytes = (WORD)left_size;
    left_node_head.has_children  = source_node_head.has_children;
    left_node_data[0:left_size] = source_node_data[0:left_size];
    clear left_node_data[left_size : left_node_data'size - left_size];

    /* terminate the emerging item */
    emerging_item = the_item;

    /* read the next item */
    rc = read_item (    index_description,
                        source_node_head,
                        source_node_data,
                    ref source_offset,
                    out the_item,
                        the_item);     // previously read item
    if (rc < 0)
      return rc;


    /* terminate the right node */

    /* "right_node.nb_data_bytes" will be initialized later */
    right_node.head.has_children = source_node_head.has_children;

    right_offset = 0;

    if (source_node_head.has_children)
    {
      right_node.data[0:LINK'size] = emerging_item.child_btree'byte;
      right_offset = LINK'size;
    }

    rc = write_item (    index_description,
                     ref right_node.head,
                     ref right_node.data,
                     ref right_offset,
                         the_item,
                         the_item);  // unused
    if (rc < 0)
      return rc;


    /* copy the remaining bytes */

    remaining_bytes = source_node_head.nb_data_bytes - source_offset;

    if (right_offset + remaining_bytes > right_node.data'size)
    {
      if (emerging_idx == emerging_max_idx)
        return E_INTERN_17;    /* inconsistent index data */
      emerging_idx++;
      continue;
    }

    if (right_offset + remaining_bytes > left_size && emerging_idx < emerging_max_idx)
    {
      emerging_idx++;
      continue;
    }

    right_node.data[right_offset:remaining_bytes] = source_node_data[source_offset:remaining_bytes];

    right_node.head.nb_data_bytes = (WORD)(right_offset + remaining_bytes);

    clear right_node.data[right_node.head.nb_data_bytes : right_node.data'size - right_node.head.nb_data_bytes];


    /* finally, initialize the emerging item's child_btree field */

    emerging_item.child_btree = emerging_child_btree;

    return 0;
  }
}

/**************************************************************************/
#begin unsafe
/**************************************************************************/

/* insert 'new_item' in 'index_node'.                                */
/* if 'overflow' is returned with TRUE, there is an 'overflow_item'. */
/* 'new_item' and 'overflow_item' can denote the same variable.      */
/* 'index_node' can initially be as large as a node + 1 item !       */
/* (this extreme case is used when handling underflows).             */
/* ! don't forget to set the index_node as 'dirty' !                 */

public int insert_item_in_node
          (ref DB_INFO               p,
               DB_INDEX_DESCRIPTION  index_description,
           ref XHEADER               index_node_head,    // actual size depends on index_node_max_offset
           ref byte[]                index_node_data,    // actual size depends on index_node_max_offset
               uint                  insertion_point,
               INDEX_ITEM            new_item,
               LINK                  advised_block_nr,   // for allocation
           out bool                  overflow,
           out INDEX_ITEM            overflow_item)
{
  INDEX_ITEM    new_index_item = new_item;   // avoids that the clearing of overflow item erases it
  HUGE_DB_INDEX large_node;
  int           rc;
  LINK          block_nr;

  clear overflow, overflow_item, large_node;


  /* build the large node */

  rc = copy_node_and_insert_items (    index_description,
                                   out large_node.head,
                                   out large_node.data,
                                       index_node_head,
                                       index_node_data,
                                       insertion_point,
                                       new_index_item,
                                       false,
                                       new_index_item);
  if (rc < 0)
  {
    return rc;
  }


  // check if a split is needed

  if (large_node.head.nb_data_bytes <= MAX_INDEX_DATA_SIZE)   // no split is needed
  {
    index_node_head.nb_data_bytes = large_node.head.nb_data_bytes;
    index_node_data[0:large_node.head.nb_data_bytes] = large_node.data[0:large_node.head.nb_data_bytes];
    clear index_node_data[index_node_head.nb_data_bytes : index_node_data'size - index_node_head.nb_data_bytes];

    overflow = false;

    return 0;
  }

  else       /* we have to split the node in two */

  {
    DB_INDEX*  pindex_node2;
    BUFFER*    pbuffer;

    /* allocate and open a new node */

    rc = allocate_block (/* block_nr      = */ out block_nr,
                         /* db_info       = */ ref p,
                         /* advised_block = */ advised_block_nr,
                         /* alignment     = */ 1);
    if (rc < 0)
    {
      return rc;
    }

    rc = open_block (out pbuffer,
                     ref p,
                     block_nr,
                     true);      /* clear_data */
    if (rc < 0)
    {
      return rc;
    }

    set_dirty (ref p, pbuffer);

    pindex_node2 = (DB_INDEX *)&pbuffer->data;


    /* split the large node in two */

    rc = split_node (    index_description,
                         large_node.head,       /* source */
                         large_node.data,
                     out index_node_head,       /* left target */
                     out index_node_data,
                     out overflow_item,         /* middle item */
                     out *pindex_node2,         /* right target */
                         block_nr);             /* for overflow_item.child_btree */
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    close_block (ref p, pbuffer);

    overflow = true;

    return 0;
  }
}

/**************************************************************************/

/* if 'record_exists', return 'existing_data_record_block_nr',      */
/*   'insertion_point' pointing to it and 'fils' pointing to        */
/*   an index node with smaller keys, or 0L if none.                */
/* otherwise return 'fils' and 'insertion_point' where the new item */
/*   must be inserted (it can be at the end of the .data field).    */

public
int compute_fils
    (    DB_INDEX_DESCRIPTION  index_description,
         DB_INDEX              index_node,
         byte[]                key,
     out bool                  record_exists,
     out LINK                  existing_data_record_block_nr,
     out LINK                  fils,
     out uint                  insertion_point)
{
  int        rc;
  uint       offset;
  INDEX_ITEM local_item;
  BYTE       key_value[MAX_KEY_SIZE];
  uint       key_size, u;

  record_exists = false;
  existing_data_record_block_nr = 0;
  fils  = 0;

  offset = 0;

  if (index_node.head.has_children)
  {
    fils = THE_LINK (index_node.data);
    offset = LINK'size;
  }

  insertion_point = offset;

  clear local_item;

  for (;;)
  {
    rc = read_item (    index_description,
                        index_node.head,
                        index_node.data,
                    ref offset,
                    out local_item,
                        local_item);   /* previous item */
    if (rc < 0)
      return rc;


    /* examine the key value */

    uncompress_key (    index_description,
                        local_item,
                    out key_value);


    /* compare both key values */

    key_size = index_description.key_size;
    for (u=0; u<key_size; u++)
    {
      if (key_value[u] != key[u])
        break;
    }

    if (u == key_size)           /* equal ! */
    {
      record_exists                 = true;
      existing_data_record_block_nr = local_item.data_record_block_nr;
      /* 'fils' points to index block with smaller keys. */
      /* 'insertion_point' is already correct.           */
      return 0;
    }

    if (key_value[u] > key[u])  /* we have now a too large item_key */
      break;

    fils = local_item.child_btree;
    insertion_point = offset;

    if (offset == index_node.head.nb_data_bytes)    /* no more items */
      break;
  }

  record_exists                 = false;
  existing_data_record_block_nr = 0;
  return 0;
}

/**************************************************************************/

int recursive_add
   (ref DB_INFO              p,
        DB_INDEX_DESCRIPTION index_description,
        LINK                 l,                   /* entering index node 'l' */
        byte[]               key,                 /* non-compressed */
        LINK                 data_record_block_nr,
    out bool                 record_exists,
    out LINK                 existing_data_record_block_nr,
    out bool                 overflow,
    out INDEX_ITEM           extra_item)
{
  int        rc;
  BUFFER*    pbuffer;
  DB_INDEX*  pindex_node;
  LINK       fils;
  uint       insertion_point;

  clear record_exists, existing_data_record_block_nr, overflow, extra_item;

  if (l == 0)   /* key not found in Btree */
  {
    record_exists                 = false;
    existing_data_record_block_nr = 0;
    overflow                      = true;

    compress_key (    index_description,
                      key,
                  out extra_item);

    extra_item.data_record_block_nr = data_record_block_nr;
    extra_item.child_btree          = 0;         /* no children */

    return 0;
  }


  /* open index node 'l' */

  rc = open_block (out pbuffer, ref p, l, false);
  if (rc < 0)
    return rc;

  pindex_node = (DB_INDEX *)&pbuffer->data;

  rc = compute_fils (    index_description,
                         *pindex_node,
                         key,
                     out record_exists,
                     out existing_data_record_block_nr,
                     out fils,
                     out insertion_point);
  if (rc < 0)
  {
    close_block (ref p, pbuffer);
    return rc;
  }

  if (record_exists)
  {
    overflow = false;
    close_block (ref p, pbuffer);
    return 0;
  }


  /* search in sub-Btree 'fils' */

  rc = recursive_add
            (ref p,
                 index_description,
                 fils,                    /* entering index node 'fils' */
                 key,                     /* non-compressed */
                 data_record_block_nr,
             out record_exists,
             out existing_data_record_block_nr,
             out overflow,
             out extra_item);
  if (rc < 0)
  {
    close_block (ref p, pbuffer);
    return rc;
  }

  if (overflow)
  {
    set_dirty (ref p, pbuffer);

    rc = insert_item_in_node (ref p,
                                  index_description,
                              ref pindex_node->head,
                              ref pindex_node->data,
                                  insertion_point,
                                  extra_item,
                                  l,     /* advised_block_nr for allocation */
                              out overflow,
                              out extra_item);
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }
  }

  close_block (ref p, pbuffer);

  return 0;
}

/**************************************************************************/

public int add_btree
            (ref DB_INFO              p,
                 LINK                 table_block_nr,
                 int                  index_nr,
                 DB_INDEX_DESCRIPTION index_description,
                 byte[]               data_record,
                 LINK                 data_record_block_nr,
             out bool                 record_exists,
             out LINK                 existing_data_record_block_nr)
{
  int                   rc;
  BYTE                  key[MAX_KEY_SIZE];
  BUFFER*               pbuffer, pindex_buffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  bool                  overflow;
  INDEX_ITEM            extra_item;


  clear record_exists, existing_data_record_block_nr;


  /* compute the key value */

  compute_key (index_description, data_record, out key);


  /* let's open the table description block */

  rc = open_block (out pbuffer, ref p, table_block_nr, false);
  if (rc < 0)
    return rc;

  ptable_description = (DB_TABLE_DESCRIPTION *)&pbuffer->data;

  rc = recursive_add (ref p,
                          index_description,
                          ptable_description->index_root_link[index_nr], /* root */
                          key,
                          data_record_block_nr,
                      out record_exists,
                      out existing_data_record_block_nr,
                      out overflow,
                      out extra_item);
  if (rc < 0)
  {
    close_block (ref p, pbuffer);
    return rc;
  }

  if (overflow)
  {
    LINK        root;
    DB_INDEX*   pindex_node;

    /* we must create a new root (the Btree receives a new level) */

    rc = allocate_block
      (/* block_nr      = */ out root,
       /* db_info       = */ ref p,
       /* advised_block = */ ptable_description->index_root_link[index_nr],
       /* alignment     = */ 1);

    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    rc = open_block (out pindex_buffer,
                     ref p,
                     root,
                     true);     /* clear_data */
    if (rc < 0)
    {
      close_block (ref p, pbuffer);      /* close table_description block */
      return rc;
    }

    set_dirty (ref p, pindex_buffer);

    pindex_node = (DB_INDEX *)&pindex_buffer->data;

    if (ptable_description->index_root_link[index_nr] != 0)
      pindex_node->head.has_children = true;

    {
      ref BYTE[] pdata = pindex_node->data;
      uint       offset = 0;

      /* append NEXT0 */

      if (pindex_node->head.has_children)
      {
        pdata[offset:LINK'size] = ptable_description->index_root_link[index_nr]'byte;
        offset = LINK'size;
      }


      /* append the extra_item */

      rc = write_item (    index_description,
                       ref pindex_node->head,
                       ref pindex_node->data,
                       ref offset,
                           extra_item,
                           extra_item);    // not used for first item
      if (rc < 0)
      {
        close_block (ref p, pindex_buffer);
        close_block (ref p, pbuffer);
        return E_INTERN_22;
      }
    }

    close_block (ref p, pindex_buffer);

    /* update the Btree root pointer */
    set_dirty (ref p, pbuffer);
    ptable_description->index_root_link[index_nr] = root;
  }

  close_block (ref p, pbuffer);

  return 0;
}

/**************************************************************************/
#end unsafe
/**************************************************************************/
