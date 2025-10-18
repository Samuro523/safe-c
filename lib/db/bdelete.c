
// bdelete.c

use ../db, config, fs, dbstruct, cache, badd, super;

/**************************************************************************/

// test if node is less than 50% full.

bool node_underflows (DB_INDEX_DESCRIPTION index_description,
                      XHEADER              index_node_head)
{
  uint split_point, max_item_size;

  split_point = LINK'size + (MAX_INDEX_DATA_SIZE - LINK'size) / 2;     // normally, 256

  max_item_size =
          (uint)((index_description.k_flags & K_LEADING_BYTES_COMPRESSION) != 0)
          + index_description.kp_info_size
          + index_description.key_size
          + LINK'size
          + LINK'size * (uint)index_node_head.has_children;

  return (index_node_head.nb_data_bytes <= split_point - max_item_size);
}

/**************************************************************************/

/* delete the item indexed by 'deletion_point'. */
/* this can cause the node to underflow.        */

int delete_item_in_node (    DB_INDEX_DESCRIPTION index_description,
                         ref XHEADER              index_node_head,
                         ref byte[]               index_node_data,
                             uint                 deletion_point)
{
  int         rc;
  INDEX_ITEM  the_item, previous_item;
  uint        previous_nb_leading_bytes, current_nb_leading_bytes;
  uint        read_offset, rest_begin, rest_end, write_offset, i;

  /* read the item to delete */

  read_offset = deletion_point;

  /* pre-fetch nb leading bytes of the item to delete */
  previous_nb_leading_bytes = 0;
  if (read_offset > LINK'size &&
      (index_description.k_flags & K_LEADING_BYTES_COMPRESSION) != 0)
  {
    previous_nb_leading_bytes = THE_BYTE (index_node_data[read_offset:1]);
  }

  clear previous_item;

  rc = read_item (    index_description,
                      index_node_head,
                      index_node_data,
                  ref read_offset,
                  out the_item,
                      previous_item);  // not used
  if (rc < 0)
    return rc;

  /* attention: the leading bytes of 'the_item' are undefined */
  /* because 'previous_item' was undefined in the above call. */


  /* is there another item following ? */

  if (read_offset == index_node_head.nb_data_bytes)   // no
  {
    /* there is no next item ! */
    index_node_head.nb_data_bytes = (WORD)deletion_point;

    /* erase the just deleted item */
    clear index_node_data[deletion_point : read_offset - deletion_point];
  }
  else    /* another item follows */
  {
    /* read the item just following */

    previous_item = the_item;

    /* pre-fetch nb leading bytes of the next item */
    current_nb_leading_bytes = 0;
    if (read_offset > LINK'size &&
        (index_description.k_flags & K_LEADING_BYTES_COMPRESSION) != 0)
    {
      current_nb_leading_bytes = THE_BYTE (index_node_data[read_offset:1]);
    }

    rc = read_item (    index_description,
                        index_node_head,
                        index_node_data,
                    ref read_offset,
                    out the_item,        /* read item        */
                        previous_item);  /* item read before */
    if (rc < 0)
      return rc;

    /* attention: the leading bytes of 'the_item' are partly undefined */
    /* because 'previous_item' was partly undefined in the above call. */


    /* keep pointers to rest data */

    rest_begin = read_offset;
    rest_end   = index_node_head.nb_data_bytes;


    /* write the item at earlier deletion_point */

    write_offset = deletion_point;

    /* make sure to keep the minimum number of common leading bytes */
    if (previous_nb_leading_bytes < current_nb_leading_bytes)
      previous_item.compressed_key_size = previous_nb_leading_bytes;
    else
      previous_item.compressed_key_size = current_nb_leading_bytes;

    rc = write_item (    index_description,
                     ref index_node_head,
                     ref index_node_data,
                     ref write_offset,
                         the_item,
                         previous_item);
    if (rc < 0)
      return rc;


    /* move the remaining data bytes backwards, just after this item */

    for (i=rest_begin; i<rest_end; i++)
      index_node_data[write_offset++] = index_node_data[i];
    index_node_head.nb_data_bytes = (WORD)write_offset;
    clear index_node_data[write_offset : rest_end - write_offset];
  }

  return 0;
}

/**************************************************************************/

/* target_node := (left_node || middle_item || right_node).   */
/* note: within the result, middle_item->child_btree receives */
/*       the .next0 link of the right node.                   */

int catenate_node_item_node (    DB_INDEX_DESCRIPTION index_description,
                             out XHEADER              target_node_head,   // actual size is always HUGE
                             out byte[]               target_node_data,   // actual size is always HUGE
                                 DB_INDEX             left_node,
                                 INDEX_ITEM           middle_item,
                                 DB_INDEX             right_node)
{
  uint        source_offset, target_offset, remaining_bytes;
  INDEX_ITEM  my_item, middle_item0;
  int         rc;

  clear target_node_head, target_node_data;

  
  /* scan the source node until reaching the last item */

  source_offset = 0;

  if (left_node.head.has_children)
    source_offset = LINK'size;

  clear my_item;

  for (;;)
  {
    if (source_offset == left_node.head.nb_data_bytes)   /* no more items */
      break;

    rc = read_item (    index_description,
                        left_node.head,
                        left_node.data,
                    ref source_offset,
                    out my_item,
                        my_item);           /* (previous read item) */
    if (rc < 0)
      return rc;
  }


  /* copy the left node to the target node */

  target_node_head.has_children     = left_node.head.has_children;
  target_node_data[0:source_offset] = left_node.data[0:source_offset];

  target_offset = source_offset;


  /* write now the middle_item, with modified child_btree link */
  /* and the right node.                                       */

  source_offset = 0;

  middle_item0 = middle_item;
  if (right_node.head.has_children)
  {
    middle_item0.child_btree'byte = right_node.data[0:LINK'size];
    source_offset = LINK'size;
  }

  rc = write_item (    index_description,
                   ref target_node_head,
                   ref target_node_data,
                   ref target_offset,
                       middle_item0,
                       my_item);         /* (previous item, if any) */
  if (rc < 0)
    return rc;

  /* copy first item of right node, if any */
  if (source_offset != right_node.head.nb_data_bytes)
  {
    rc = read_item (    index_description,
                        right_node.head,
                        right_node.data,
                    ref source_offset,
                    out my_item,
                        my_item);       /* previous item read (ignored) */
    if (rc < 0)
      return rc;

    rc = write_item (    index_description,
                     ref target_node_head,
                     ref target_node_data,
                     ref target_offset,
                         my_item,
                         middle_item);        /* (previous item written) */
    if (rc < 0)
      return rc;
  }


  /* finally, copy the remaining bytes from the right node */

  remaining_bytes = right_node.head.nb_data_bytes - source_offset;

  if (target_node_head.nb_data_bytes + remaining_bytes > target_node_data'size)
    return E_INTERN_23;

  target_node_data[target_offset : remaining_bytes] = right_node.data[source_offset : remaining_bytes];

  target_node_head.nb_data_bytes += (WORD)remaining_bytes;

  return 0;
}

/**************************************************************************/
#begin unsafe
/**************************************************************************/

/* rebalance the triplet (father, fils, and a node left or right to fils). */
/* 'insertion_point' points just after the item of fils, or after next0.   */
/* ! this function usually deletes/updates items in the father node !      */
/* 'father_node' can initially be as large as a node + 1 item !            */
/* after the call, 'father_node' can be larger than a node if it was       */
/*   initially larger than a node.                                         */
/* ! do not forget to set the father buffer as 'dirty' !                   */

int handle_underflow (ref DB_INFO              p,
                          DB_INDEX_DESCRIPTION index_description,
                      ref XHEADER              father_node_head,
                      ref byte[]               father_node_data,
                          uint                 insertion_point0,
                          LINK                 fils0,
                      out bool                 underflow,
                      out bool                 overflow,
                      out INDEX_ITEM           emergeant)
{
  uint          insertion_point = insertion_point0;
  LINK          fils            = fils0;
  uint          offset, result_offset;
  LINK          left_fils, right_fils;
  INDEX_ITEM    pivot_item;
  HUGE_DB_INDEX large_node;       // can contain an index node + an item + a half-full node
  BUFFER*       pleft_buffer, pright_buffer;
  DB_INDEX*     pleft_node, pright_node;
  int           rc;

  /* be sure that the father node contains at least one item */

  assert (father_node_head.nb_data_bytes > LINK'size);


  clear underflow, overflow, emergeant, pivot_item;


  /* test if there is a node to the right of 'fils' */

  if (insertion_point == father_node_head.nb_data_bytes)      /* no */
  {
    /* 'fils' is the right-most link of the father node.            */
    /* we must shift left 'insertion_point' and 'fils' by one item. */

    offset = LINK'size;       /* the father node has already a next0 link */

    for (;;)
    {
      result_offset = offset;    /* new insertion point so far */

      /* advance by 1 item */
      rc = read_item (    index_description,
                          father_node_head,
                          father_node_data,
                      ref offset,
                      out emergeant,   /* (dummy) */
                          emergeant);  /* (dummy) */
      if (rc < 0)
        return rc;

      if (offset == insertion_point)    /* one item too far */
        break;
    }

    insertion_point = result_offset;
    fils'byte = father_node_data[result_offset-LINK'size : LINK'size];
  }


  /* 'fils' is always the left-hand node */

  left_fils = fils;


  /* let's compute 'pivot_item' and its 'right_fils' */

  offset = LINK'size;       /* the father node has already a next0 link */

  for (;;)
  {
    if (offset == insertion_point)   /* we reached the insertion point */
      break;

    /* advance by 1 item */
    rc = read_item (    index_description,
                        father_node_head,
                        father_node_data,
                    ref offset,
                    out pivot_item,
                        pivot_item);       /* (unused the first time) */
    if (rc < 0)
      return rc;
  }

  /* read now the 'pivot_item' */
  rc = read_item (    index_description,
                      father_node_head,
                      father_node_data,
                  ref offset,
                  out pivot_item,
                      pivot_item);   /* in (maybe unused) */
  if (rc < 0)
    return rc;

  right_fils = pivot_item.child_btree;



  /* open the 'left_fils' and 'right_fils' nodes */

  rc = open_block (out pleft_buffer, ref p, left_fils, false);
  if (rc < 0)
    return rc;

  pleft_node = (DB_INDEX *)&pleft_buffer->data;

  rc = open_block (out pright_buffer, ref p, right_fils, false);
  if (rc < 0)
  {
    close_block (ref p, pleft_buffer);
    return rc;
  }

  pright_node = (DB_INDEX *)&pright_buffer->data;


  /* build the large node as the catenation of */
  /* (left_node || pivot_item || right_node)   */

  clear large_node;
  rc = catenate_node_item_node (    index_description,
                                out large_node.head,        /* target */
                                out large_node.data,
                                    *pleft_node,            /* left source   */
                                    pivot_item,             /* middle source */
                                    *pright_node);          /* right source  */
  if (rc < 0)
  {
    close_block (ref p, pleft_buffer);
    close_block (ref p, pright_buffer);
    return rc;
  }


  if (large_node.head.nb_data_bytes <= MAX_INDEX_DATA_SIZE)
  {
    /* a catenation (the right node will disappear) */

    /* copy large_node into left_node */
    set_dirty (ref p, pleft_buffer);
    pleft_node->head.nb_data_bytes = large_node.head.nb_data_bytes;
    pleft_node->data[0:large_node.head.nb_data_bytes] = large_node.data[0:large_node.head.nb_data_bytes];

    /* close all */
    close_block (ref p, pleft_buffer);
    close_block (ref p, pright_buffer);


    /* deallocate the right node */
    rc = deallocate_block (right_fils, ref p);
    if (rc < 0)
      return rc;


    /* delete the father_node's item at insertion_point. */
    /* (this can cause the father node to underflow).    */

    rc = delete_item_in_node (    index_description,
                              ref father_node_head,
                              ref father_node_data,
                                  insertion_point);   /* deletion point */
    if (rc < 0)
      return rc;

    /* test if the father node underflows */
    underflow = node_underflows (index_description, father_node_head);

    /* the father_node can be larger than a node here, in case the */
    /* initial father_node was already larger than a node,         */
    /* but we leave this problem to the calling function.          */
    overflow  = false;

    return 0;
  }


  /* a rebalancing : put approx. half of items (at least one) left & right */
  /* and produce an emerging item.                                         */

  set_dirty (ref p, pleft_buffer);
  set_dirty (ref p, pright_buffer);

  rc = split_node (    index_description,
                       large_node.head,       /* source node */
                       large_node.data,       /* source node */
                   out pleft_node->head,      /* target 1 */
                   out pleft_node->data,      /* target 1 */
                   out emergeant,             /* target 2 */
                   out *pright_node,          /* target 3 */
                       right_fils);           /* for emergeant.child_btree */
  if (rc < 0)
  {
    close_block (ref p, pleft_buffer);
    close_block (ref p, pright_buffer);
    return rc;
  }


  /* close all */

  close_block (ref p, pleft_buffer);
  close_block (ref p, pright_buffer);


  /* delete the father_node's item at insertion_point. */
  /* (this can cause the father node to underflow).    */

  rc = delete_item_in_node (    index_description,
                            ref father_node_head,
                            ref father_node_data,
                                insertion_point);   /* deletion point */
  if (rc < 0)
    return rc;


  /* insert the 'emergeant' item in the father_node. */
  /* (this can cause the father_node to overflow)    */

  rc = insert_item_in_node (ref p,
                                index_description,
                            ref father_node_head,  /* init. max: node + 1 item */
                            ref father_node_data,
                                insertion_point,   /* in */
                                emergeant,         /* in: the new item */
                                left_fils,         /* advised_block_nr */
                            out overflow,
                            out emergeant);
  if (rc < 0)
    return rc;



  /* test the father_node's underflow/overflow state */

  if (overflow)
  {
    underflow = false;         /* the node surely didn't underflow */
  }
  else    /* maybe the node underflowed - test this */
  {
    underflow = node_underflows (index_description, father_node_head);
  }

  return 0;
}

/**************************************************************************/

int extract_smallest
        (ref DB_INFO              p,
             DB_INDEX_DESCRIPTION index_description,
             LINK                 fils,
         out INDEX_ITEM           replacement_item,
         out bool                 underflow,
         out bool                 overflow,
         out INDEX_ITEM           emergeant)
{
  int       rc;
  BUFFER*   pbuffer;
  DB_INDEX* pindex_node;


  clear replacement_item, underflow, overflow, emergeant;


  /* open 'fils' */

  rc = open_block (out pbuffer, ref p, fils, false);
  if (rc < 0)
    return rc;

  pindex_node = (DB_INDEX *)&pbuffer->data;


  if (pindex_node->head.has_children)  /* we must search deeper */
  {
    rc = extract_smallest (ref p,
                               index_description,
                               THE_LINK (pindex_node->data),  /* next0 */
                           out replacement_item,
                           out underflow,
                           out overflow,
                           out emergeant);
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    /* did an overflow or underflow occur in the child node ? */

    if (underflow)
    {
      set_dirty (ref p, pbuffer);

      rc = handle_underflow (ref p,
                                 index_description,
                             ref pindex_node->head,               /* father node */
                             ref pindex_node->data,               /* father node */
                                 LINK'size,                       /* in: insertion_point */
                                 THE_LINK (pindex_node->data),    /* in: next0 */
                             out underflow,
                             out overflow,
                             out emergeant);
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }
    }

    else if (overflow)

    {
      set_dirty (ref p, pbuffer);

      rc = insert_item_in_node (ref p,
                                    index_description,
                                ref pindex_node->head,
                                ref pindex_node->data,
                                    LINK'size,         /* insertion_point */
                                    emergeant,         /* new item */
                                    fils,              /* advised_block_nr */
                                out overflow,
                                out emergeant);
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }
    }

  }
  else     /* we're at the Btree bottom */
  {
    uint  read_offset, write_offset, initial_size, i;

    set_dirty (ref p, pbuffer);

    write_offset = 0;
    initial_size = pindex_node->head.nb_data_bytes;


    /* let's read the first item */

    read_offset = 0;

    rc = read_item (    index_description,
                        pindex_node->head,
                        pindex_node->data,
                    ref read_offset,
                    out replacement_item,   /* read item */
                        replacement_item);  /* (unused)  */
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    if (read_offset < pindex_node->head.nb_data_bytes) /* there is a second item */
    {
      rc = read_item (    index_description,
                          pindex_node->head,
                          pindex_node->data,
                      ref read_offset,
                      out emergeant,
                          replacement_item);  /* previous read item */
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }


      /* write this item */

      rc = write_item (    index_description,
                       ref pindex_node->head,
                       ref pindex_node->data,
                       ref write_offset,
                           emergeant,
                           emergeant);    /* (unused) */
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }
    }


    /* append the remaining bytes */

    if (initial_size > read_offset)
    {
      if (write_offset + (initial_size - read_offset) > MAX_INDEX_DATA_SIZE)
      {
        close_block (ref p, pbuffer);
        return E_INTERN_24;
      }

      for (i=read_offset; i<initial_size; i++)
        pindex_node->data[write_offset++] = pindex_node->data[i];
    }

    pindex_node->head.nb_data_bytes = (WORD)write_offset;

    overflow  = false;
    underflow = node_underflows (index_description, pindex_node->head);
  }

  close_block (ref p, pbuffer);
  return 0;
}

/**************************************************************************/

int replace_by_smallest
            (ref DB_INFO              p,
                 DB_INDEX_DESCRIPTION index_description,
             ref DB_INDEX             index_node,             // actual data size is p.max_index_size
                 uint                 insertion_point0,
             out bool                 underflow,
             out bool                 overflow,
             out INDEX_ITEM           emergeant)
{
  uint           insertion_point = insertion_point0;
  uint           read_offset;
  int            rc;
  LINK           fils, block_nr;
  INDEX_ITEM     replacement_item;
  HUGE_DB_INDEX  large_node;     // can contain a node, a replacement item, and possibly an overflow item.
  DB_INDEX*      pindex_node2;
  BUFFER*        pbuffer;


  /* read the item to be deleted (we want only the child_btree link) */

  read_offset = insertion_point;

  clear underflow, overflow, emergeant;

  rc = read_item (    index_description,
                      index_node.head,
                      index_node.data,
                  ref read_offset,
                  out emergeant,     /* (temporary use) */
                      emergeant);    /* (garbage) */
  if (rc < 0)
    return rc;

  fils = emergeant.child_btree;   /* path to explore to find smallest */


  /* delete the item.                                           */
  /* (this can cause the node to underflow, but we ignore this) */

  rc = delete_item_in_node (    index_description,
                            ref index_node.head,
                            ref index_node.data,
                                insertion_point);     /* deletion point */
  if (rc < 0)
    return rc;


  /* obtain replacement item extracted from Btree leaf */

  rc = extract_smallest (ref p,
                             index_description,
                             fils,                /* child Btree to explore */
                         out replacement_item,
                         out underflow,
                         out overflow,
                         out emergeant);
  if (rc < 0)
    return rc;


  /* insert the replacement item and possibly an overflow item */

  replacement_item.child_btree = fils;     /* update link to fils */

  clear large_node;
  rc = copy_node_and_insert_items (    index_description,
                                   out large_node.head,       /* target node */
                                   out large_node.data,
                                       index_node.head,       /* source node */
                                       index_node.data,
                                       insertion_point,
                                       new_item1      => replacement_item,
                                       item2_provided => overflow,
                                       new_item2      => emergeant);
  if (rc < 0)
    return rc;


  /* we must now handle any underflow that occurred on the child */
  /* Btree 'fils' during the extraction.                         */

  if (underflow)
  {
    /* we must first move 'insertion_point' to the end of the item       */
    /* (the large_node contains max a node + 1 item because an underflow */
    /*  occured -> the fils surely didn't overflow).                     */

    rc = read_item (    index_description,
                        large_node.head,
                        large_node.data,
                    ref insertion_point,
                    out emergeant,
                        emergeant);  /* (garbage) */
    if (rc < 0)
      return rc;


    /* re-balance the 'fils' child Btree */

    rc = handle_underflow (ref p,
                               index_description,
                           ref large_node.head,    /* init. max: node + 1 item */
                           ref large_node.data,
                               insertion_point,
                               fils,
                           out underflow,
                           out overflow,
                           out emergeant);
    if (rc < 0)
      return rc;


    /* check if the large node is too large, and if so, split it in two. */
    /* (this can happen after a catenation if the node was already too   */
    /*  large before the above call).                                    */

    if (large_node.head.nb_data_bytes <= MAX_INDEX_DATA_SIZE)   /* not too large */
    {
      /* copy again 'large_node' into 'index_node' */
      index_node.head.nb_data_bytes = large_node.head.nb_data_bytes;
      index_node.data[0:large_node.head.nb_data_bytes] = large_node.data[0:large_node.head.nb_data_bytes];
      clear index_node.data[index_node.head.nb_data_bytes : index_node.data'size - index_node.head.nb_data_bytes];
    }
    else   /* this case slipped through the function 'handle_underflow' */
    {
      /* split the large_node in two -> index_node, index_node2 */

      /* allocate and open a new node */
      rc = allocate_block (out /* block_nr      = */ block_nr,
                           ref /* db_info       = */ p,
                               /* advised_block = */ fils,
                               /* alignment     = */ 1);
      if (rc < 0)
        return rc;

      rc = open_block (out pbuffer,
                       ref p,
                       block_nr,
                       true);     /* clear_data */
      if (rc < 0)
        return rc;

      set_dirty (ref p, pbuffer);

      pindex_node2 = (DB_INDEX *)&pbuffer->data;


      /* split the large node in two */

      rc = split_node (    index_description,
                           large_node.head,  /* source */
                           large_node.data,
                       out index_node.head,  /* left target */
                       out index_node.data,
                       out emergeant,        /* middle item */
                       out *pindex_node2,    /* right target */
                           block_nr);        /* for emergeant->child_btree */
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }

      close_block (ref p, pbuffer);

      overflow = true;
    }

    return 0;
  }
  else
  {
    /* 'large_node' has an unbound length   */
    /* (underflow or overflow is possible). */

    if (large_node.head.nb_data_bytes <= MAX_INDEX_DATA_SIZE)
    {
      /* copy back 'large_node' into 'index_node' */
      index_node.head.nb_data_bytes = large_node.head.nb_data_bytes;
      index_node.data[0:large_node.head.nb_data_bytes] = large_node.data[0:large_node.head.nb_data_bytes];
      clear index_node.data[index_node.head.nb_data_bytes : index_node.data'size - index_node.head.nb_data_bytes];

      /* init result parameters */
      overflow  = false;
      underflow = node_underflows (index_description, index_node.head);

      return 0;
    }
    else      /* 'large_node' must be split in two */
    {
      /* allocate and open a new node */

      rc = allocate_block (out /* block_nr      = */ block_nr,
                           ref /* db_info       = */ p,
                               /* advised_block = */ fils,
                               /* alignment     = */ 1);
      if (rc < 0)
        return rc;

      rc = open_block (out pbuffer,
                       ref p,
                       block_nr,
                       true);   /* clear_data */
      if (rc < 0)
        return rc;

      set_dirty (ref p, pbuffer);

      pindex_node2 = (DB_INDEX *)&pbuffer->data;


      /* split the large node in two */

      rc = split_node (    index_description,
                           large_node.head,  /* source */
                           large_node.data,
                       out index_node.head,  /* left target */
                       out index_node.data,
                       out emergeant,        /* middle item */
                       out *pindex_node2,    /* right target */
                           block_nr);        /* for emergeant->child_btree */
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }

      close_block (ref p, pbuffer);

      overflow  = true;
      underflow = false;

      return 0;
    }
  }
}

/**************************************************************************/

int recursive_delete
        (ref DB_INFO              p,
             DB_INDEX_DESCRIPTION index_description,
             LINK                 l,
             BYTE[]               key,
         out bool                 record_not_found,
         out bool                 underflow,
         out bool                 overflow,
         out INDEX_ITEM           emergeant,
         out LINK                 old_data_record_block_nr)
{
  int        rc;
  BUFFER*    pbuffer;
  DB_INDEX*  pindex_node;
  bool       record_exists;
  LINK       fils;
  uint       insertion_point;


  clear record_not_found, underflow, overflow, emergeant, old_data_record_block_nr;


  if (l == 0)    /* key not found in Btree */
  {
    record_not_found         = true;
    underflow                = false;
    overflow                 = false;
    old_data_record_block_nr = 0;      /* dummy */
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
                     out old_data_record_block_nr,
                     out fils,
                     out insertion_point);
  if (rc < 0)
  {
    close_block (ref p, pbuffer);
    return rc;
  }

  if (record_exists)
  {
    if (fils != 0)         /* NODE is in the middle of the btree */
    {
      set_dirty (ref p, pbuffer);

      rc = replace_by_smallest (ref p,
                                    index_description,
                                ref *pindex_node,
                                    insertion_point,
                                out underflow,
                                out overflow,
                                out emergeant);
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }
    }
    else        /* NODE is at the bottom of the btree */
    {
      set_dirty (ref p, pbuffer);

      rc = delete_item_in_node (    index_description,
                                ref pindex_node->head,
                                ref pindex_node->data,
                                    insertion_point);   /* deletion point */
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }

      /* test if the node underflows */
      underflow = node_underflows (index_description, pindex_node->head);
      overflow = false;
    }

    record_not_found = false;

    close_block (ref p, pbuffer);
    return 0;
  }


  /* search in child-Btree 'fils' */

  rc = recursive_delete (ref p,
                             index_description,
                             fils,
                             key,
                         out record_not_found,
                         out underflow,
                         out overflow,
                         out emergeant,
                         out old_data_record_block_nr);
  if (rc < 0)
  {
    close_block (ref p, pbuffer);
    return rc;
  }


  if (underflow)
  {
    set_dirty (ref p, pbuffer);

    rc = handle_underflow (ref p,
                               index_description,
                           ref pindex_node->head,  /* father node */
                           ref pindex_node->data,
                               insertion_point,
                               fils,                 /* link to fils */
                           out underflow,
                           out overflow,
                           out emergeant);
    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }
  }

  else if (overflow)

  {
    set_dirty (ref p, pbuffer);

    rc = insert_item_in_node (ref p,
                                  index_description,
                              ref pindex_node->head,
                              ref pindex_node->data,
                                  insertion_point,
                                  emergeant,         /* new item */
                                  l,                 /* advised_block_nr */
                              out overflow,
                              out emergeant);
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

/* if 'record_not_found', nothing was done,              */
/* else the function returns 'old_data_record_block_nr'. */

public
int delete_btree
        (ref DB_INFO              p,
             LINK                 table_block_nr,
             int                  index_nr,
             DB_INDEX_DESCRIPTION index_description,
             byte[]               data_record,
         out bool                 record_not_found,
         out LINK                 old_data_record_block_nr)
{
  BYTE                  key[MAX_KEY_SIZE];
  int                   rc;
  BUFFER*               pbuffer, pindex_buffer;
  DB_TABLE_DESCRIPTION* ptable_description;
  bool                  underflow, overflow;
  DB_INDEX*             pindex_node;
  LINK                  root;
  INDEX_ITEM            extra_item;


  clear record_not_found, old_data_record_block_nr;


  /* compute the key value */

  compute_key (index_description, data_record, out key);


  /* let's open the table description block */

  rc = open_block (out pbuffer, ref p, table_block_nr, false);
  if (rc < 0)
    return rc;

  ptable_description = (DB_TABLE_DESCRIPTION *)&pbuffer->data;


  rc = recursive_delete (ref p,
                             index_description,
                             ptable_description->index_root_link[index_nr], /* root */
                             key,
                         out record_not_found,
                         out underflow,
                         out overflow,
                         out extra_item,
                         out old_data_record_block_nr);
  if (rc < 0)
  {
    close_block (ref p, pbuffer);
    return rc;
  }


  if (underflow)
  {
    /* check if the root node has become empty */

    rc = open_block (out pindex_buffer,
                     ref p,
                         ptable_description->index_root_link[index_nr],
                         false);
    if (rc < 0)
    {
      close_block (ref p, pbuffer);      /* close table_description block */
      return rc;
    }

    pindex_node = (DB_INDEX *)&pindex_buffer->data;

    if (pindex_node->head.nb_data_bytes <= LINK'size)    /* index node is empty */
    {
      if (pindex_node->head.has_children)
        root'byte = pindex_node->data[0:LINK'size];    /* use NEXT0 as new root */
      else
        root = 0;                              /* Btree is really empty */


      close_block (ref p, pindex_buffer);

      /* deallocate old root */
      rc = deallocate_block (ptable_description->index_root_link[index_nr], ref p);
      if (rc < 0)
      {
        close_block (ref p, pbuffer);
        return rc;
      }

      /* set new root */
      set_dirty (ref p, pbuffer);
      ptable_description->index_root_link[index_nr] = root;
    }
    else
    {
      close_block (ref p, pindex_buffer);
    }
  }

  else if (overflow)

  {
    /* create a new root node (new Btree level) */

    rc = allocate_block
      (/* block_nr      = */ out root,
       /* db_info       = */ ref p,
       /* advised_block = */     ptable_description->index_root_link[index_nr],
       /* alignment     = */     1);

    if (rc < 0)
    {
      close_block (ref p, pbuffer);
      return rc;
    }

    rc = open_block (out pindex_buffer,
                     ref p,
                     root,
                     true);              /* in : clear_data */
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
      ref BYTE[] data = pindex_node->data;
      uint       offset = 0;
    
      /* append NEXT0 */

      if (pindex_node->head.has_children)
      {
        data[offset:LINK'size] = ptable_description->index_root_link[index_nr]'byte;
        offset = LINK'size;
      }


      /* append the extra_item */

      rc = write_item (    index_description,
                       ref pindex_node->head,
                       ref pindex_node->data,
                       ref offset,
                           extra_item,
                           extra_item);   /* not used for first item */
      if (rc < 0)
      {
        close_block (ref p, pindex_buffer);
        close_block (ref p, pbuffer);
        return rc;
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
