
// pool.h : constant pool

/******************************************************************************************/

struct POOL_CTE;
typedef POOL_CTE^ POOL;    // pool constant

void init_constant_pool ();

POOL new_pool_constant (uint4 size, uint4 align);
uint4 size_of_pool_cte (POOL p);
uint4 align_of_pool_cte (POOL p);

// load/store pool constant integer (size=1, 2, 4, 8)
void load_integer (POOL p, uint4 offset, out int8 pvalue, uint size, bool is_signed);
void store_integer (POOL p, uint4 offset, int8 value, uint size);

// load/store pool constant float (size=4, 8)
void load_float (POOL p, uint4 offset, out double pvalue, uint size);
void store_float (POOL p, uint4 offset, double value, uint size);

// load/store pool references (references have address_size)
// ! assertion: all references are inserted in increasing offset order !!
void store_reference (POOL p, uint4 offset, POOL target);   // is added in intern linked list
POOL load_reference_of (POOL p, uint4 offset);              // is retrieved from intern linked list

// copy pool slice, copy and relocate also all references in the slice.
void copy_pool_to_pool (POOL source_pool, uint4 source_offset,
                        POOL target_pool, uint4 target_offset,
                        uint size);

// incl. recursive test for constants containing references
bool pool_constants_are_identical (POOL p, POOL q);

// used for code generation (references a constant and set the .used flag on it and all references)
// must be called after the pool constant was fully initialized.
int8 serial_nr_of_pool_cte (POOL p);

/******************************************************************************************/

// backfill : for INTEL only, not for ANDROID (for android it is done in fixup.c)

// called at start of new function
void reset_pool_backfills ();

void move_all_pool_backfills (uint4 start_addr, int4 offset);

// for rip, use current_RIP()-4
void pool_add_backfill_addr4 (int8 serial_nr, uint4 rip, bool absolute, uint4 target_offset);

/******************************************************************************************/

void mark_jumptable (POOL p);
void relocate_jumptable_pool_constant (int8 serial_nr, int label_table[]);

/******************************************************************************************/

void merge_pool_subtrings ();

/******************************************************************************************/

void intel_flush_pool_subtrings (uint cte_segment_load_address);
void android_flush_pool_subtrings ();

int position_of_pool_cte_in_data_blob (int8 nr);

/******************************************************************************************/
