
// pool.c : constant pool

from std use arithm, bintree, sorting, strings;
use front/lex;
use error, goptions, fixup, exeout, blob, blob2;

/******************************************************************************************/

const int MAX_REFERENCES = 10;   // max references per REFERENCE_NODE (we create a new node when full)

struct REFERENCE
{
  uint4      offset; // offset within pool constant where to patch the reference
  POOL_CTE^  pool;   // pool constant to be referenced
}

struct REFERENCE_NODE
{
  int             count;             // nb of references in this node (1 .. MAX_REFERENCES)
  REFERENCE       r[MAX_REFERENCES];
  REFERENCE_NODE^ next;
}

struct BACKFILL_INFO               // ONLY USED FOR INTEL, NOT FOR ANDROID
{
  uint4          backfill_addr;  // 4-byte address to fill
  bool           absolute;       // absolute or relative address
  uint4          target_offset;  // offset to add to patched value
  BACKFILL_INFO^ move_next;      // next backfill to move (used within function)
  BACKFILL_INFO^ next;
}

struct POOL_CTE
{
  byte[]^         mem;         // allocated pool constant
  uint4           size;        // size of pool constant
  uint4           align;       // advised alignment
  int8            serial_nr;   // unique serial nr of pool constant, used for references
  REFERENCE_NODE^ first;       // first node of list of references to other pool constants
  REFERENCE_NODE^ last;        // last node
  bool            used;        // referenced by code generation
  bool            is_jump_table;
  BACKFILL_INFO^  backfill;    // list of backfill addresses to fill within code segment

  // after merging:
  POOL_CTE^       redirect_pool;    // non-null -> redirect to within another pool constant
  int4            redirect_offset;  //             at specified offset.

  bool            flushed;       // written in .exe
  uint4           rip_address;
}

struct USER_INFO
{
  uint cte_segment_load_address;
}

USER_INFO^ g_user_info;

package PCBTREE = new BALANCED_BINARY_TREE (ELEMENT => POOL, USER_INFO => USER_INFO);

BINARY_TREE tree;         // contains all referenced pool constants
int8        serial_nr;    // next unique nr

/******************************************************************************************/

// maximum pool size for all constants together
const uint4 MAX_POOL_SIZE = (256*1024*1024);   // 256 MB

int8 total_allocated_size;

/******************************************************************************************/

int compare (USER_INFO^ user,
             POOL       data1,
             POOL       data2)
{
  int8 a, b;

  _unused user;

  a = data1^.serial_nr;
  b = data2^.serial_nr;

  if (a < b)
    return -1;

  if (a > b)
    return +1;

  return 0;
}

/******************************************************************************************/

public
void init_constant_pool ()
{
  g_user_info = new USER_INFO;
  create_btree (out tree, g_user_info, compare);
  total_allocated_size = 0;
}

/******************************************************************************************/

public
POOL new_pool_constant (uint4 size, uint4 align)
{
  byte[]^   mem;
  POOL_CTE^ p;

  if (size + total_allocated_size > MAX_POOL_SIZE)
  {
    semantic_error ("too many large constants", lex.token.pos);
    fatal_out_of_memory_error ("out of memory");
  }

  total_allocated_size += size;

  mem = new byte[size];

  p = new POOL_CTE;
  p^.mem       = mem;
  p^.size      = size;
  p^.align     = align;
  p^.serial_nr = 0;
  p^.first     = null;
  p^.last      = null;
  p^.used      = false;
  p^.redirect_pool = null;
  p^.redirect_offset = 0;
  p^.flushed     = false;
  p^.rip_address = 0;

  return p;
}

/******************************************************************************************/

public
uint4 size_of_pool_cte (POOL p)
{
  return p^.size;
}

/******************************************************************************************/

public
uint4 align_of_pool_cte (POOL p)
{
  return p^.align;
}

/******************************************************************************************/

// load pool constant integer

public
void load_integer (POOL p, uint4 offset, out int8 pvalue, uint size, bool is_signed)
{
  if (size > 8 ||
      offset > p^.size ||
      offset + size > p^.size)
    fatal_compiler_error0 ("load_integer");


  pvalue = 0;

  {
    ref byte[] mem = p^.mem^;
    uint    i;
    byte    b;

    b = 0;

    for (i=0; i<size; i++)
    {
      if (big_endian)   // Motorola
        b = mem[offset + size-1-i];
      else
        b = mem[offset + i];
      pvalue += ((int8)b << (i*8));
    }

    if (is_signed && b >= 128)   // highest bit is set
    {
      for (; i<8; i++)   // fill rest of pvalue with 0xFF's
        pvalue += ((int8)0xFF << (i*8));
    }
  }
}

/******************************************************************************************/

// store pool constant integer

public
void store_integer (POOL p, uint4 offset, int8 value, uint size)
{
  if (size > 8 ||
      offset > p^.size ||
      offset + size > p^.size)
    fatal_compiler_error0 ("store_integer");

  {
    ref byte[] mem = p^.mem^;
    uint       i;

    if (big_endian)   // Motorola
    {
      for (i=0; i<size; i++)
        mem[offset + i] = (byte)((value >> ((size-1-i)*8)) & 255);
    }
    else   // Intel
    {
      for (i=0; i<size; i++)
        mem[offset + i] = (byte)((value >> (i*8)) & 255);
    }
  }
}

/******************************************************************************************/

// load pool constant float

public
void load_float (POOL p, uint4 offset, out double pvalue, uint size)
{
  byte[8] r;

  load_integer (p, offset, out r, size, is_signed => false);

  if (size == 4)
  {
    float f;
    f'byte = r[0:4];
    pvalue = f;
  }
  else
  {
    pvalue'byte = r;
  }
}

/******************************************************************************************/

// store pool constant float

public
void store_float (POOL p, uint4 offset, double value, uint size)
{
  byte[8] r;

  if (size == 4)
  {
    float f = (float)value;
    clear r;
    r[0:4] = f'byte;
  }
  else
  {
    r = value'byte;
  }

  store_integer (p, offset, r, size);
}

/******************************************************************************************/

// load/store pool references (references have address_size)
// reference is added in intern linked list
// ! assertion: all references are inserted in increasing offset order !!

public
void store_reference (POOL p, uint4 offset, POOL target)
{
  REFERENCE_NODE^ list, prev;

  if (offset > p^.size ||
      offset + (uint)address_size > p^.size)
    fatal_compiler_error0 ("store_reference(1)");

  list = p^.last;
  if (list == null || list^.count >= MAX_REFERENCES)
  {
    // allocate a new REF node
    prev = list;
    list = new REFERENCE_NODE;

    if (prev == null)
    {
      p^.first = list;
    }
    else
    {
      prev^.next = list;
      if (offset <= prev^.r[prev^.count-1].offset)        // references must be inserted in order !
        fatal_compiler_error0 ("store_reference(3)");
    }

    p^.last = list;
  }
  else
  {
    if (offset <= list^.r[list^.count-1].offset)        // references must be inserted in order !
      fatal_compiler_error0 ("store_reference(4)");
  }

  list^.r[list^.count].offset = offset;
  list^.r[list^.count].pool   = target;
  list^.count++;
}

/******************************************************************************************/

// is retrieved from intern linked list

public
POOL load_reference_of (POOL p, uint4 offset)
{
  REFERENCE_NODE^ list;
  int             i;

  if (offset > p^.size ||
      offset + (uint)address_size > p^.size)
    fatal_compiler_error0 ("load_reference_of(1)");

  list = p^.first;
  while (list != null)
  {
    for (i=0; i<list^.count; i++)
    {
      if (list^.r[i].offset == offset)
        return list^.r[i].pool;
    }

    list = list^.next;
  }

  fatal_compiler_error0 ("load_reference_of(2)");
  return null;
}

/******************************************************************************************/

// copy pool slice, copy and relocate also all references in the slice.

public
void copy_pool_to_pool (POOL source_pool, uint4 source_offset,
                        POOL target_pool, uint4 target_offset,
                        uint size)
{
  REFERENCE_NODE^ list;
  int             i;

  if (size > MAX_POOL_SIZE)
    fatal_compiler_error0 ("copy_pool_to_pool(1)");

  if (source_offset > source_pool^.size ||
      source_offset + size > source_pool^.size)
    fatal_compiler_error0 ("copy_pool_to_pool(2)");

  if (target_offset > target_pool^.size ||
      target_offset + size > target_pool^.size)
    fatal_compiler_error0 ("copy_pool_to_pool(3)");

  target_pool^.mem^[target_offset : size] = source_pool^.mem^[source_offset : size];

  list = source_pool^.first;
  while (list != null)
  {
    for (i=0; i<list^.count; i++)
    {
      if (list^.r[i].offset >= source_offset && list^.r[i].offset + (uint)address_size <= source_offset + size)
        store_reference (target_pool, list^.r[i].offset - source_offset + target_offset, list^.r[i].pool);
    }

    list = list^.next;
  }
}

/******************************************************************************************/

// incl. recursive test for constants containing references

public
bool pool_constants_are_identical (POOL p, POOL q)
{
  REFERENCE_NODE^ lp, lq;
  int             i;

  if (p == q)       // optimization
    return true;

  if (p^.size != q^.size ||
      memcmp (p^.mem^[0:p^.size], q^.mem^[0:q^.size]) != 0)
    return false;

  // check that all references match (we have checked that they were inserted in order)

  lp = p^.first;
  lq = q^.first;

  for (;;)
  {
    if (lp == null && lq == null)
      return true;
    if (lp == null || lq == null)
      return false;

    if (lp^.count != lq^.count)
      return false;

    for (i=0; i<lp^.count; i++)
    {
      if (lp^.r[i].offset != lq^.r[i].offset)
        return false;
      if (!pool_constants_are_identical (lp^.r[i].pool, lq^.r[i].pool))
        return false;
    }

    lp = lp^.next;
    lq = lq^.next;
  }
}

/******************************************************************************************/

void set_used_flag (POOL p)
{
  REFERENCE_NODE^ list;
  int             i;

  if (p^.used)     // .used already set for all dependencies
    return;

  p^.used = true;

  // assign unique serial nr
  p^.serial_nr = serial_nr++;

#if 0
  printf ("\n");
  printf ("serial cte %d : ", p^.serial_nr);
  for (i=0; i<(int)p^.size; i++)
    printf ("%c (%d) ", p^.mem[i], p^.mem[i]);
#endif

  // insert in btree, indexed by serial_nr
  if (insert_btree (ref tree, p) < 0)
    fatal_out_of_memory_error ("new_pool_constant() failed");

  list = p^.first;
  while (list != null)
  {
    for (i=0; i<list^.count; i++)
      set_used_flag (list^.r[i].pool);
    list = list^.next;
  }
}

/******************************************************************************************/

// must be called after the pool constant was fully initialized.

public
int8 serial_nr_of_pool_cte (POOL p)
{
  set_used_flag (p);      // signal that this constant is referenced
  return p^.serial_nr;
}

/******************************************************************************************/
/******************************************************************************************/
/******************************************************************************************/

package BACKFILL_DATA

  BACKFILL_INFO^ move_chain;

end BACKFILL_DATA;

/******************************************************************************************/

// called at start of new function

public
void reset_pool_backfills ()
{
  move_chain = null;
}

/******************************************************************************************/

public
void move_all_pool_backfills (uint4 start_addr, int4 offset)
{
  BACKFILL_INFO^ n = move_chain;

  while (n != null && n^.backfill_addr >= start_addr)
  {
    n^.backfill_addr += (uint)offset;
    n = n^.move_next;
  }
}

/************************************************************************/

POOL pool_cte_of_serial_nr (int8 nr)
{
  POOL p, q;

  p = new POOL_CTE;
  p^.serial_nr = nr;

  q = p;

  if (retrieve_btree (tree, ref p, BT_EQUAL) < 0)
    fatal_out_of_memory_error ("retrieve error for pool constant");

  free q;

  return p;
}

/******************************************************************************************/

public void mark_jumptable (POOL p)
{
  p^.is_jump_table = true;
}

/******************************************************************************************/

// for rip, use current_RIP()-4

public
void pool_add_backfill_addr4 (int8 serial_nr, uint4 rip, bool absolute, uint4 target_offset)
{
  POOL           p;
  BACKFILL_INFO^ n;

  p = pool_cte_of_serial_nr (serial_nr);

  n = new BACKFILL_INFO;
  n^.backfill_addr = rip;
  n^.absolute      = absolute;
  n^.target_offset = target_offset;
  n^.move_next     = move_chain;
  move_chain = n;

  n^.next = p^.backfill;
  p^.backfill = n;
}

/******************************************************************************************/

// called after a function's asm was generated, to convert label nr into relative jump vector.

public
void relocate_jumptable_pool_constant (int8 serial_nr, int label_table[])
{
  POOL p;
  int  offset, size;
  int8 label_nr;

  p = pool_cte_of_serial_nr (serial_nr);
  size = (int)size_of_pool_cte (p);

  for (offset=0; offset<size; offset+=address_size)
  {
    load_integer (p, (uint)offset, out label_nr, (uint)address_size, is_signed => false);
    if (label_nr < 0 || label_nr >= label_table'length)
      fatal_compiler_error0 ("relocate_jumptable_pool_constant(1)");
    store_integer (p, (uint)offset, label_table[(uint)label_nr], (uint)address_size);
  }
}

/******************************************************************************************/
/******************************************************************************************/
/******************************************************************************************/

package MERGING

  POOL[]^ g_merge_table;
  int     g_merge_table_count;

end MERGING;

/******************************************************************************************/

int operate (USER_INFO^  user,
             POOL        data)
{
  _unused user;
  g_merge_table^[g_merge_table_count++] = data;
  return 0;
}

/******************************************************************************************/

int compare_size (POOL p1, POOL p2)
{
  if (p1^.size < p2^.size)
    return -1;
  if (p1^.size > p2^.size)
    return +1;
  return 0;
}

package SORT = new HeapSort (ELEMENT => POOL, compare => compare_size);

/******************************************************************************************/

// returns -1 if no substring found with interesting alignment

int substring (byte fragment[], uint4 fragment_size, int fragment_align,
               byte mem[],      uint4 mem_size,      int mem_align)
{
  int i, last;

  if (fragment_align > mem_align)     // superior alignment cannot be provided
    return -1;

  if (fragment_size == 0)  // empty string always matches
    return 0;

  last = (int)(mem_size - fragment_size);
  for (i=0; i<=last; i+=fragment_align)
  {
    if (fragment[0] == mem[i] && memcmp (fragment[0:fragment_size], mem[i:fragment_size]) == 0)
      return i;
  }

  return -1;
}

/******************************************************************************************/

public
void merge_pool_subtrings ()
{
  int  i, j, index;
  uint size;

  g_merge_table = new POOL [(uint)serial_nr];
  g_merge_table_count = 0;

  // initialize table with all used pool constants, ordered by serial_nr
  traverse_btree (tree, operate, +1);

  {
    ref POOL[] merge_table = g_merge_table^;

    // sort table by increasing size
    sort (ref merge_table[0 : g_merge_table_count]);

    for (i=g_merge_table_count-2; i>=0; i--)   // process by decreasing index, from large size to small size
    {
      ref POOL_CTE pi = merge_table[i]^;

      if (pi.first != null)   // don't merge jagged arrays
        continue;

      if (pi.is_jump_table)   // don't merge jump tables
        continue;

      size = pi.size;

      for (j=g_merge_table_count-1; j>i; j--)   // consider redirecting fragment i into larger chain j
      {
        ref POOL_CTE pj = merge_table[j]^;

        if (pj.redirect_pool != null)  // already redirected
          continue;

        if (pj.first != null)   // don't merge jagged arrays
          continue;

        if (pj.is_jump_table)   // don't merge jump tables
          continue;

        index = substring (pi.mem^, size,    (int)pi.align,
                           pj.mem^, pj.size, (int)pj.align);
        if (index != -1)
        {
          pi.redirect_pool = merge_table[j];
          pi.redirect_offset = index;

  // printf ("merging cte %d into cte %d\n", i, j);

          break;
        }
      }
    }
  }

  free g_merge_table;
}

/******************************************************************************************/
/******************************************************************************************/

// returns rip address of constant

uint4 intel_flush_pool_constant (POOL p, uint cte_segment_load_address)
{
  REFERENCE_NODE^ r;
  int             i;
  uint4           addr;
  BACKFILL_INFO^  l;

  if (p^.flushed)    // was already flushed earlier
    return p^.rip_address;

  if (p^.redirect_pool != null)   // was redirected by substring merging operation
  {
    p^.rip_address = intel_flush_pool_constant (p^.redirect_pool, cte_segment_load_address) + (uint)p^.redirect_offset;
  }
  else
  {
    // first, flush all dependant constants and fill the jagged references

    r = p^.first;
    while (r != null)
    {
      for (i=0; i<r^.count; i++)
      {
        addr = intel_flush_pool_constant (r^.r[i].pool, cte_segment_load_address);     // recursive
        store_integer (p, r^.r[i].offset, addr, (uint)address_size);
      }

      r = r^.next;
    }

    // second flush the constant in exe

    {
      uint4 size, align;

      size = p^.size;

      if (size > 8)       // size 9..?
        align = 16;
      else if (size > 4)  // size 5..8
        align = 8;
      else if (size > 2)  // size 3..4
        align = 4;
      else if (size > 1)  // size 2
        align = 2;
      else                // size 1
        align = 1;

      p^.rip_address = cte_segment_load_address + exe_write_byte_sequence (p^.mem^[0:size], umax(align, p^.align));
    }
  }


  // backfill all code references to this pool constant

  l = p^.backfill;
  while (l != null)
  {
    uint val = p^.rip_address + l^.target_offset;

    if (!l^.absolute)
      val -= l^.backfill_addr + 4;

  #begin unsafe
    *(uint*)exe_ptr (l^.backfill_addr - cte_segment_load_address) = val;
  #end unsafe

    l = l^.next;
  }

  p^.flushed = true;

  return p^.rip_address;
}

/******************************************************************************************/

int intel_operate_flush_pool_constant (USER_INFO^ user_info,
                                 POOL       data)
{
  (void)intel_flush_pool_constant (data, user_info^.cte_segment_load_address);
  return 0;
}

/******************************************************************************************/

public
void intel_flush_pool_subtrings (uint cte_segment_load_address)
{
  g_user_info^ = {cte_segment_load_address => cte_segment_load_address};

  traverse_btree (tree, intel_operate_flush_pool_constant, +1);

  if (address_size == 8)  // append 4 byte dummy at the end, to avoid problems when pushing int4 as 8 byte pool constant on stack
  {
    int4 dummy = 0;
    (void)exe_write_byte_sequence (sequence => dummy, align => 4);
  }
}

/******************************************************************************************/
/******************************************************************************************/
/******************************************************************************************/

// returns position in g_blob_data

uint4 android_flush_pool_constant (POOL p)
{
  if (p^.flushed)            // was already flushed earlier
    return p^.rip_address;   // position in g_blob_data

  if (p^.redirect_pool != null)   // was redirected by substring merging operation
  {
    p^.rip_address = android_flush_pool_constant (p^.redirect_pool) + (uint)p^.redirect_offset;
  }
  else
  {
    REFERENCE_NODE^ r;
    int             i;
    uint4           addr;
    uint4           align, size;


    // first, recursively treat all dependant constants

    r = p^.first;
    while (r != null)
    {
      for (i=0; i<r^.count; i++)
        (void)android_flush_pool_constant (r^.r[i].pool);      // recursive
      r = r^.next;
    }


    // second, compute alignment and position of this constant in data blob

    size = p^.size;

    if (size > 8)       // size 9..?
      align = 16;
    else if (size > 4)  // size 5..8
      align = 8;
    else if (size > 2)  // size 3..4
      align = 4;
    else if (size > 1)  // size 2
      align = 2;
    else                // size 1
      align = 1;

    blob_align (ref g_blob_data, umax(align, p^.align));
    p^.rip_address = (uint)blob_index (g_blob_data);


    // third, fill the jagged references

    r = p^.first;
    while (r != null)
    {
      for (i=0; i<r^.count; i++)
      {
        addr = r^.r[i].pool^.rip_address;

        store_integer (p, r^.r[i].offset, addr, (uint)address_size);    // patch this constant

        // 64-byte address needs to be relocated statically (add memory address of g_blob_data)
        // and dynamically (add relocation record in elf file)
        fixup.store_code_data_relocation (fill_position => (int)p^.rip_address + (int)r^.r[i].offset,
                                          code_blob => false);
      }

      r = r^.next;
    }


    // fourth, relocate jump table

    if (p^.is_jump_table)  // relocate vectors to code blob address, then elf relocate
    {
      uint offset;
      for (offset=0; offset<size; offset+=(uint)address_size)
      {
        // 64-byte address needs to be relocated statically (add memory address of g_code_data)
        // and dynamically (add relocation record in elf file)
        fixup.store_code_data_relocation (fill_position => (int)p^.rip_address + (int)offset,
                                          code_blob => true);
      }
    }

    // fifth, write the patched constant in data blob

    blob_put_sequence (ref g_blob_data, p^.mem^[0:size]);
  }


  p^.flushed = true;

  return p^.rip_address;   // position in g_blob_data
}

/******************************************************************************************/

int android_operate_flush_pool_constant (USER_INFO^ user_info,
                                         POOL       data)
{
  _unused user_info;
  (void)android_flush_pool_constant (data);
  return 0;
}

/******************************************************************************************/

public
void android_flush_pool_subtrings ()
{
  g_user_info^ = {0};

  traverse_btree (tree, android_operate_flush_pool_constant, +1);

  // append 4 zero bytes
  {
    int4 dummy = 0;
    blob_put_sequence (ref g_blob_data, dummy);
  }

  blob_align (ref g_blob_data, 16);  // make sure data is at m16
}

/******************************************************************************************/

public int position_of_pool_cte_in_data_blob (int8 nr)
{
  POOL_CTE^ p = pool_cte_of_serial_nr (nr);
  return (int)p^.rip_address;   // position in g_blob_data
}

/******************************************************************************************/
