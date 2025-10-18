
// initarm.c

from std use tracing;
use ../common, ../pool, ../goptions, ../error, ../fixup;
use arm64, astacks, asm_arm;

//======================================================================================

void set_global (out EA ea, int ofs)
{
  clear ea;
  ea.base       = ZERO;
  ea.index      = ZERO;
  ea.scale      = 1;
  ea.offset     = ofs;
  ea.reloc.kind = RELOC_GLOBAL;
  ea.reloc.nr   = 0;
}

//======================================================================================

// ACQUIRE_LOCK (uses X11, X12, X13, X14)

void gen_code_for_tombstone_get_lock ()
{
  int lab;
  EA  ea;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for tombstone_get_lock\n");
  }

lab = 0;   // new label range

// entry point
declare_function (func_getlock_tombstone);

  // address of spin uint4 in X14
  set_global (out ea, 8);
  compute_effective_address_in_register (ea, X14);

  // acquisition value 1 in X12
  move_register_immediate (target => X12,
                           imm    => 1,
                           size   => 4);   // 4 or 8

// L1:
declare_near_label (near_label => lab+1);

  c_load_excl (target    => X13,   // ZERO allowed (data_size bytes)
               base      => X14,   // SP allowed (base effective address)(64 bit address)
               data_size => 4);    // 1, 2, 4 or 8

  // old value (X13) is non-zero -> goto lab+2
  branch_if_not_zero (reg => X13, size => 4, near_label => lab+2);

  c_store_excl (target    => X11,  // 32-bit register will contain status result : 0 = ok, 1 = failed
                base      => X14,  // SP allowed (base effective address)(64 bit address)
                source    => X12,  // ZERO allowed (data_size bytes)
                data_size => 4);   // 1, 2, 4 or 8

  // if store was unsuccessful, goto lab+2
  branch_if_not_zero (reg => X11, size => 4, near_label => lab+2);

  // old value is finally zero, lock acquired, we can return
  c_ret ();


// L2:
declare_near_label (near_label => lab+2);

  c_yield ();   // gives hint to processor that improves performance of spin-wait loops.

  branch (lab+1);
}

// to release lock, write zero uint4 in spin flag at global+8

//======================================================================================

package TOMB
  const int BLOCK_SIZE = 4*1024;   // at least 4096 (PAGE SIZE) !!   POWER OF TWO !!  can hold 255 entries of 16 bytes
  const int NB_BLOCKS = 256;       // number of blocks to allocate at once to reduce fragmentation, can hold 65536 entries
  // 1 MB allocated at once, see also assertions below.
end TOMB;

//======================================================================================

// X0 = address returned by malloc()
// X1 = typ

// ADDRESS allocate_tombstone (ADDRESS allocated_block)

void gen_code_for_tombstone_malloc ()
{
  int lab;
  EA  ea;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for tombstone_malloc\n");
  }

  if (BLOCK_SIZE  < 4096)   // at least PAGE SIZE
    fatal_compiler_error0 ("aligntb1");
  if (NB_BLOCKS * BLOCK_SIZE < 64*1024)
    fatal_compiler_error0 ("aligntb2");

lab = 0;   // new label range

declare_function (func_allocate_tombstone);

  // save X30 (return address) in X22
  c_mov_reg_reg (target    => X22,            // ZERO allowed (SP not allowed)
                 source    => X30,            // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

  // save actual allocated memory address in X19 for later
  c_mov_reg_reg (target    => X19,            // ZERO allowed (SP not allowed)
                 source    => X0,             // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

  // save typ in X21 for later
  c_mov_reg_reg (target    => X21,            // ZERO allowed (SP not allowed)
                 source    => X1,             // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

  // allocation of a new page happens each 4095 allocations at program start,
  // but it does not occur later when the program has run a long time
  // and has enough tombstone entries.
  // -> use spin loop because allocation is fast.

  // ACQUIRE_LOCK (uses X11, X12, X13, X14)     [begin protect global variables]
  call_function (func_getlock_tombstone);

  c_dmb (CRM_FETCH_CACHE);

  set_global (out ea, 16);   // Tombstone_HEAD
  c_load_register_from_memory  (X20, ea, false, address_size);   // Tombstone_HEAD in X20

  // if (Tombstone_HEAD == null)
  branch_if_not_zero (reg => X20, size => address_size, near_label => lab+2);

  // block list is empty, allocate a new block with tombstone slots
  // init 64K virtual page, init with zeroes

  // int posix_memalign (byte** memptr, long alignment, long size);

  set_global (out ea, 16);   // Tombstone_HEAD
  compute_effective_address_in_register (ea, X0);

  // X1 = alignment (4K)
  move_register_immediate (target => X1, imm => BLOCK_SIZE, size => 4);   // 4 or 8
  // X2 = size (1MB)
  move_register_immediate (target => X2, imm => NB_BLOCKS * BLOCK_SIZE, size => 4);   // 4 or 8

  call_libc ("posix_memalign");

  set_global (out ea, 16);   // Tombstone_HEAD
  c_load_register_from_memory  (X0, ea, false, address_size);   // Tombstone_HEAD in X0

  // if null -> L4  (will return 0 to call and cause a crash)
  branch_if_zero (reg => X0, size => address_size, near_label => lab+4);

  // save X0 (block's address) in X20
  c_mov_reg_reg (target    => X20,            // ZERO allowed (SP not allowed)
                 source    => X0,             // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

  // memset (addr, 0x00, size);
  // X1 = 0
  move_register_immediate (target => X1, imm => 0, size => 4);   // 4 or 8
  // X2 = size (1MB)
  move_register_immediate (target => X2, imm => NB_BLOCKS * BLOCK_SIZE, size => 4);   // 4 or 8
  call_libc ("memset");

  // link all blocks in head/tail links

  // make X20 point to last 4K block (of 1MB space)
  add_offset_using_x17 (target => X20, source => X20, offset => (NB_BLOCKS-1) * BLOCK_SIZE /* 255 x 4K */, size => address_size);

  // there are 256 x 4k-blocks.
  // store addr of last 4K-block in tail.
  set_global (out ea, 24);            //  24: ADDR : tail of tombstone structure
  c_store_register_in_memory (X20, ea, address_size);  // store in tail

  // restore X20 to start of 1MB space
  add_offset_using_x17 (target => X20, source => X20, offset => - (NB_BLOCKS-1) * BLOCK_SIZE, size => address_size);

  // we will loop on X0 to initialize all 4K blocks
  move_register_immediate (target => X0, imm => NB_BLOCKS, size => 4);    // loop 256 times

  // X1 = address of 4K block
  c_mov_reg_reg (target    => X1,             // ZERO allowed (SP not allowed)
                 source    => X20,            // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

declare_near_label (lab+5);   // L5:

  // block at X1 : init ptr to next 4K-block (address itself + 4096)
  c_add_reg_imm (target     => X4,                // SP allowed (ZERO not allowed)
                 source     => X1,                // SP allowed (ZERO not allowed)
                 imm12      => BLOCK_SIZE >> 12,  // 0 to 4095
                 shl_imm_12 => true,              // true to shift imm12 << 12 (can reach 16 MB)
                 size       => address_size);     // 4 or 8

  clear ea;
  ea.base = X1;  ea.index = ZERO; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (r => X4, ea, size => address_size);

  // initialize a new 4K-block at X1.
  // initialize a free list covering all tombstone entries : each entry pointing to the next;
  // the free tombstone chain list is stored at offset 8 in each entry. null indicates end of list.

  // 255 entries for each 4K block
  // free list head pointer + 254 entries to fill, keep last ptr null
  move_register_immediate (target => X2, imm => (BLOCK_SIZE/16)-1, size => 4);   // X2 = 255

  // X4 = address of first entry, computed as X1 + 8
  c_add_reg_imm (target     => X4,             // SP allowed (ZERO not allowed)
                 source     => X1,             // SP allowed (ZERO not allowed)
                 imm12      => 8,              // 0 to 4095
                 shl_imm_12 => false,          // true to shift imm12 << 12 (can reach 16 MB)
                 size       => address_size);  // 4 or 8

declare_near_label (lab+1);   // L1

  // X6 = X4 + 8   add #8 so it points to entry 2 (first tombstone entry)
  c_add_reg_imm (target     => X6,             // SP allowed (ZERO not allowed)
                 source     => X4,             // SP allowed (ZERO not allowed)
                 imm12      => 8,              // 0 to 4095
                 shl_imm_12 => false,          // true to shift imm12 << 12 (can reach 16 MB)
                 size       => address_size);  // 4 or 8

  // store at X4
  clear ea;  
  ea.base = X4;  ea.index = ZERO; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (X6, ea, address_size);

  // move X4 to next vector to fill
  c_add_reg_imm (target     => X4,             // SP allowed (ZERO not allowed)
                 source     => X4,             // SP allowed (ZERO not allowed)
                 imm12      => 16,             // 0 to 4095
                 shl_imm_12 => false,          // true to shift imm12 << 12 (can reach 16 MB)
                 size       => address_size);  // 4 or 8

  c_subs_reg_imm (target     => X2,     // ZERO allowed (SP not allowed)
                  source     => X2,     // SP allowed (ZERO not allowed)
                  imm12      => 1,      // 0 to 4095
                  shl_imm_12 => false,  // true to shift imm12 << 12
                  size       => 4);     // 4 or 8
  cond_branch (CMP_NOT_EQUAL, signed => false, near_label => lab+1);
  // end loop

  // next 4K block
  c_add_reg_imm (target     => X1,     // SP allowed (ZERO not allowed)
                 source     => X1,     // SP allowed (ZERO not allowed)
                 imm12      => BLOCK_SIZE >> 12,  // 0 to 4095
                 shl_imm_12 => true, // true to shift imm12 << 12 (can reach 16 MB)
                 size       => address_size);     // 4 or 8

  c_subs_reg_imm (target     => X0,     // ZERO allowed (SP not allowed)
                  source     => X0,     // SP allowed (ZERO not allowed)
                  imm12      => 1,      // 0 to 4095
                  shl_imm_12 => false,  // true to shift imm12 << 12
                  size       => 4);     // 4 or 8
  cond_branch (CMP_NOT_EQUAL, signed => false,  near_label => lab+5);

  // X1 : go back to last 4K-block
  c_sub_reg_imm (target     => X1,     // SP allowed (ZERO not allowed)
                 source     => X1,     // SP allowed (ZERO not allowed)
                 imm12      => BLOCK_SIZE >> 12,  // 0 to 4095
                 shl_imm_12 => true, // true to shift imm12 << 12 (can reach 16 MB)
                 size       => address_size);     // 4 or 8

  // clear next ptr of last block
  ea.base = X1;  ea.index = ZERO; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  move_memory_immediate (target => ea, imm => 0, size => address_size);


declare_near_label (lab+2);   // L2:


// what happens if a 4K block is fully allocated ? it stays on block list ?  no !  does it come back on it in free ? yes
// is the first tombstone of a 4K block ever allocated as active tombstone ? no

/*
globals:
//  0: ADDR : heap ID (windows only)
//  8: int4 : lockf (0=unlocked, 1=lock) ; protects head & tail of tombstone structure
// 16: ADDR : head of 4K block list
// 24: ADDR : tail of 4K block list

4K BLOCK (first tombstone entry):
0: next 4K block (can be 0)
8: ptr to first free tombstone entry list of this block (never 0, at least 1 free entry)

tombstone entry (if free):
0:
8: ptr to next free tombstone entry (can be 0)

tombstone entry (if in use):
  struct Tombstone (16 bytes)
  {
    uint4   count;   // nb of references into the heap object
    uint4   type;    // unique nr of designated type (ex: int, char[],.)
    ADDRESS pdata;   // address of heap object
  }

 X0 : loop NB_BLOCKS
 X1 : address of 4K block / second free entry (can be 0)
 X2 : loop on entries of block
 X3 : new entry
 X4 : temp
 X6 : temp

 X12-X14 : spin lock
 X15-X17 : astack layer

 untouched by OS calls :
 X19 : saved actual allocated memory address from malloc
 X20 : block address
 X21 : typ
 X22 : return address



Registers You Must Save in a Callback (Callee-Saved)
x19 to x28 — Must be preserved across function calls.
x29 (FP) — Frame pointer; typically preserved if used.
x30 (LR) — Link register; must be saved if the function makes further calls.
SP (x31) — Stack pointer; must be restored to its original value before returning.


23 to 28 must never be used !!

//======================================================================================
*/

  // here we suppose we have, at X20, a 4K-block that has at least one free tombstone entry.

  // load address of first free entry of block X20[8] in X3
  clear ea;
  ea.base = X20;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_load_register_from_memory  (X3, ea, false, address_size);   // Tombstone_HEAD in X20
  // here X3 is new tombstone entry !

  // follow free entry chain at ofs=8, load address of second free entry (or null) in X1
  clear ea;
  ea.base = X3;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_load_register_from_memory (X1, ea, false, address_size);

  // store X1 (address of second free entry or null) into 4K-block's free list field at ofs 8
  clear ea;  
  ea.base = X20;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (X1, ea, address_size);

  // test if the 4K-block has any free entries left after this (if still entries left -> L3)
  branch_if_not_zero (reg => X1, size => address_size, near_label => lab+3);

  // block has no free entries anymore -> unlink 4K-block from list.

  // load X4 = 0[X20] : next block with free entries, or null
  clear ea;
  ea.base = X20;  ea.index = ZERO; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_load_register_from_memory (X4, ea, false, address_size);

  // store X4 into head
  set_global (out ea, 16);
  c_store_register_in_memory (X4, ea, address_size);

  // test if null
  branch_if_not_zero (reg => X4, size => address_size, near_label => lab+3);

  // if head is null, set also tail to null
  set_global (out ea, 24);
  move_memory_immediate (ea, imm => 0, size => address_size);  // head is null -> clear also tail ptr

declare_near_label (lab+3);   // L3:

  // here, X3 contains the new tombstone entry to fill and return.

  // here, the new tombstone 16-byte-entry is at X3

  // save tombstone address in X0 for return value
  c_mov_reg_reg (target    => X0,             // ZERO allowed (SP not allowed)
                 source    => X3,             // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

  // store parameter (actual allocation address) at offset 8 in tombstone entry

  // store actual allocated block address (parameter) at offset 8 in tombstone
  clear ea;  
  ea.base = X0;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (X19, ea, address_size);

  // store typ at offset 4 in tombstone
  clear ea;  
  ea.base = X0;  ea.index = ZERO; ea.scale = 1;  ea.offset = 4;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (X21, ea, 4);

  c_dmb (CRM_FLUSH_CACHE);

  // release spin lock              [end protect memory]
  set_global (out ea, 8);
  compute_effective_address_in_register (ea, X14);
  c_store_release (ZERO, X14, data_size => 4);

  // restore X30
  c_mov_reg_reg (target    => X30,            // ZERO allowed (SP not allowed)
                 source    => X22,            // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

#if 0  // $
  c_mov_reg_reg (X1, ZERO, 8);
  c_mov_reg_reg (X2, ZERO, 8);
  c_mov_reg_reg (X3, ZERO, 8);
  c_mov_reg_reg (X4, ZERO, 8);
  c_mov_reg_reg (X5, ZERO, 8);
  c_mov_reg_reg (X6, ZERO, 8);
  c_mov_reg_reg (X7, ZERO, 8);
  c_mov_reg_reg (X8, ZERO, 8);
  c_mov_reg_reg (X9, ZERO, 8);
  c_mov_reg_reg (X10, ZERO, 8);
  c_mov_reg_reg (X11, ZERO, 8);
  c_mov_reg_reg (X12, ZERO, 8);
  c_mov_reg_reg (X13, ZERO, 8);
  c_mov_reg_reg (X14, ZERO, 8);
  c_mov_reg_reg (X15, ZERO, 8);
  c_mov_reg_reg (X16, ZERO, 8);
  c_mov_reg_reg (X17, ZERO, 8);
  c_mov_reg_reg (X19, ZERO, 8);
  c_mov_reg_reg (X20, ZERO, 8);
  c_mov_reg_reg (X21, ZERO, 8);
  c_mov_reg_reg (X22, ZERO, 8);
#endif

  c_ret ();  // new tombstone is at X0


declare_near_label (lab+4);   // L4: (jumped-to after allocation failed)

  // release spin lock              [end protect memory]
  set_global (out ea, 8);
  compute_effective_address_in_register (ea, X14);
  c_store_release (ZERO, X14, data_size => 4);

  // new (NULL) tombstone is in X0
  move_register_immediate (target => X0,
                           imm    => 0,
                           size   => 4);   // 4 or 8

  // restore X30
  c_mov_reg_reg (target    => X30,            // ZERO allowed (SP not allowed)
                 source    => X22,            // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

  c_ret ();        // will crash at calling point so debug info is available

#if 0
to do after allocate_tombstone :
  mov [tombstone]4,typ       ; assign type to entry (makes it valid)
#endif

}

//======================================================================================

void gen_code_for_tombstone_free ()
{
  int lab;
  EA  ea;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for tombstone_free\n");
  }

lab = 0;

declare_function (func_free_tombstone);

  // free_tombstone (ADDRESS tb, int4 typ)
  // passed in X0=address, X1=typ.

/*
globals:
//  0: ADDR : heap ID (windows only)
//  8: int4 : lockf (0=unlocked, 1=lock) ; protects head & tail of tombstone structure
// 16: ADDR : head of 4K block list
// 24: ADDR : tail of 4K block list

4K BLOCK (first tombstone entry):
0: next 4K block (can be 0)
8: ptr to first free tombstone entry list of this block (never 0, at least 1 free entry)

tombstone entry (if free):
0:
8: ptr to next free tombstone entry (can be 0)

tombstone entry (if in use):
  struct Tombstone (16 bytes)
  {
    uint4   count;   // nb of references into the heap object
    uint4   type;    // unique nr of designated type (ex: int, char[],.)
    ADDRESS pdata;   // address of heap object
  }

 X0 : addr of tombstone to free
 X1 : expected typ of tombstone
 X2 :
 X3 :
 X4 :
 X6 :

 X12-X14 : spin lock
 X15-X17 : astack layer

 untouched by OS calls :
 X19 : saved actual allocated memory address from malloc
 X20 : block address
 X21 :
 X22 : return address
*/


  // save X30
  c_mov_reg_reg (target    => X22,            // ZERO allowed (SP not allowed)
                 source    => X30,            // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

  // tombstone is null -> ret
  branch_if_zero (reg => X0, size => address_size, near_label => lab+4);

  // ACQUIRE_LOCK (uses X11, X12, X13, X14)     [begin protect global variables]
  // to avoid that two free_tombstone calls occur at the same time !
  call_function (func_getlock_tombstone);

  c_dmb (CRM_FETCH_CACHE);

  // retrieve type in X3
  assert c_load_ofs8 (target      => X3,     // ZERO allowed
                      base        => X0,     // SP allowed (effective address)
                      offset      => 4,      // 9 bits (-256 to 255) to be added to base address
                      data_signed => false,  // data to load is signed
                      data_size   => 4);     // size of data to load : 1, 2, 4, 8

  // stores illegal value 0 at [X2] in type so further dereferencings will fail
  assert c_store_ofs8 (source    => ZERO,   // ZERO allowed
                       base      => X0,     // SP allowed (effective address)
                       offset    => 4,      // 9 bits (-256 to 255) to be added to base address
                       data_size => 4);     // size of data to load : 1, 2, 4, 8

  // check type matches
  c_cmp_reg_reg (source1 => X1, // ZERO allowed (SP not allowed)
                 source2 => X3, // ZERO allowed (SP not allowed)
                 source2_shift_type  => LSL,  // LSL, LSR, ASR
                 source2_shift_value => 0,    // range 0..31 (or 0..63 for size==8)
                 size    => 4);   // 4 or 8

  cond_branch (CMP_NOT_EQUAL, signed => false,  near_label => lab+5);    // error_bad_type


  // retrieve count in X3
  assert c_load_ofs8 (target      => X3,     // ZERO allowed
                      base        => X0,     // SP allowed (effective address)
                      offset      => 0,      // 9 bits (-256 to 255) to be added to base address
                      data_signed => false,  // data to load is signed
                      data_size   => 4);     // size of data to load : 1, 2, 4, 8

  // check count is zero.
  // if count not 0 -> error_in_use
  branch_if_not_zero (reg => X3, size => 4, near_label => lab+6);


  // save X19 = actual data storage block to be freed
  assert c_load_ofs8 (target      => X19,            // ZERO allowed
                      base        => X0,             // SP allowed (effective address)
                      offset      => 8,              // 9 bits (-256 to 255) to be added to base address
                      data_signed => false,          // data to load is signed
                      data_size   => address_size);  // size of data to load : 1, 2, 4, 8


  // compute X1 = address of 4K block of this tombstone
  // block_address = tb & -4096;  and x1,-4096   ; align at 4K
  move_register_immediate (target => X2, imm => -BLOCK_SIZE, size => 8);

  c_and_reg_reg (target  => X1,  // ZERO allowed (SP not allowed)
                 source1 => X0,  // ZERO allowed (SP not allowed)
                 source2 => X2,  // ZERO allowed (SP not allowed)
                 source2_shift_type  => LSL,            // LSL, LSR, ASR
                 source2_shift_value => 0,              // range 0..31 (or 0..63 for size==8)
                 size                => address_size);  // 4 or 8

  // load X3 = X1[8]  (head of free list of this 4K-block)
  clear ea;
  ea.base = X1;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_load_register_from_memory (X3, ea, false, address_size);

  // if free list has entries, goto L3
  branch_if_not_zero (reg => X3, size => address_size, near_label => lab+3);

  // block X1 is unchained and has no free entries.

// X0 = tombstone
// X1 = 4K-block

  // chain block X1 back at the end of the 4K-block list

  // clear .next field
  ea.base = X1;  ea.index = ZERO; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  move_memory_immediate (target => ea, imm => 0, size => address_size);   // 1, 2, 4 or 8  // uses X15-X17

  set_global (out ea, 16);   // Tombstone_HEAD
  c_load_register_from_memory (X2, ea, false, address_size);   // Tombstone_HEAD in X2

  // head not null
  branch_if_not_zero (reg => X2, size => address_size, near_label => lab+1);

  // Tombstone_HEAD = X1
  set_global (out ea, 16);
  c_store_register_in_memory (X1, ea, address_size);
  branch (lab+2);  // jmp L2

declare_near_label (lab+1);   // L1:

  // load address of tail block (or null) in X2
  set_global (out ea, 24);
  c_load_register_from_memory (X2, ea, false, address_size);   // Tombstone TAIL in X2

  // last block's next = X1
  // Tombstone_TAIL^.next = block_address;
  clear ea;  
  ea.base = X2;  ea.index = ZERO; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (X1, ea, address_size);

declare_near_label (lab+2);   // L2:

  // Tombstone_TAIL = X1
  set_global (out ea, 24);
  c_store_register_in_memory (X1, ea, address_size);

declare_near_label (lab+3);   // L3:


  // now, chain tombstone (X0) in free list of 4K-block (X1)

  // X2 = head of free list
  clear ea;
  ea.base = X1;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_load_register_from_memory (X2, ea, false, address_size);

  // store as next of new tombstone
  clear ea;  
  ea.base = X0;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (X2, ea, address_size);

  // store X0 as head of 4K-block's free list
  clear ea;  
  ea.base = X1;  ea.index = ZERO; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_store_register_in_memory (X0, ea, address_size);


#if 0  // $
  c_mov_reg_reg (X0, ZERO, 8);
  c_mov_reg_reg (X1, ZERO, 8);
  c_mov_reg_reg (X2, ZERO, 8);
  c_mov_reg_reg (X3, ZERO, 8);
  c_mov_reg_reg (X4, ZERO, 8);
  c_mov_reg_reg (X5, ZERO, 8);
  c_mov_reg_reg (X6, ZERO, 8);
  c_mov_reg_reg (X7, ZERO, 8);
  c_mov_reg_reg (X8, ZERO, 8);
  c_mov_reg_reg (X9, ZERO, 8);
  c_mov_reg_reg (X10, ZERO, 8);
  c_mov_reg_reg (X11, ZERO, 8);
  c_mov_reg_reg (X12, ZERO, 8);
  c_mov_reg_reg (X13, ZERO, 8);
  c_mov_reg_reg (X14, ZERO, 8);
  c_mov_reg_reg (X15, ZERO, 8);
  c_mov_reg_reg (X16, ZERO, 8);
  c_mov_reg_reg (X17, ZERO, 8);

  c_mov_reg_reg (X20, ZERO, 8);
  c_mov_reg_reg (X21, ZERO, 8);
#endif

  c_dmb (CRM_FLUSH_CACHE);

  // release spin lock              [end protect memory]
  set_global (out ea, 8);
  compute_effective_address_in_register (ea, X14);
  c_store_release (ZERO, X14, data_size => 4);

  // free actual memory block
  c_mov_reg_reg (target    => X0,             // ZERO allowed (SP not allowed)
                 source    => X19,            // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8
  call_libc ("free");


declare_near_label (lab+4);   // L4:    (we arrive here in case we try to free a null pointer)

  // restore X30
  c_mov_reg_reg (target    => X30,            // ZERO allowed (SP not allowed)
                 source    => X22,            // ZERO allowed (SP not allowed)
                 data_size => address_size);  // 4 or 8

#if 0  // $
  c_mov_reg_reg (X0, ZERO, 8);
  c_mov_reg_reg (X1, ZERO, 8);
  c_mov_reg_reg (X2, ZERO, 8);
  c_mov_reg_reg (X3, ZERO, 8);
  c_mov_reg_reg (X4, ZERO, 8);
  c_mov_reg_reg (X5, ZERO, 8);
  c_mov_reg_reg (X6, ZERO, 8);
  c_mov_reg_reg (X7, ZERO, 8);
  c_mov_reg_reg (X8, ZERO, 8);
  c_mov_reg_reg (X9, ZERO, 8);
  c_mov_reg_reg (X10, ZERO, 8);
  c_mov_reg_reg (X11, ZERO, 8);
  c_mov_reg_reg (X12, ZERO, 8);
  c_mov_reg_reg (X13, ZERO, 8);
  c_mov_reg_reg (X14, ZERO, 8);
  c_mov_reg_reg (X15, ZERO, 8);
  c_mov_reg_reg (X16, ZERO, 8);
  c_mov_reg_reg (X17, ZERO, 8);
  c_mov_reg_reg (X19, ZERO, 8);
  c_mov_reg_reg (X20, ZERO, 8);
  c_mov_reg_reg (X21, ZERO, 8);
  c_mov_reg_reg (X22, ZERO, 8);
#endif

  c_ret ();


declare_near_label (lab+5);   // L5: (error_bad_type:)

  // release spin lock              [end protect memory]
  set_global (out ea, 8);
  compute_effective_address_in_register (ea, X14);
  c_store_release (ZERO, X14, data_size => 4);

  // SMC Secure Monitor Call
  c_smc (0); // 0 to 65535


declare_near_label (lab+6);   // L6: (error_in_use:)

  // release spin lock              [end protect memory]
  set_global (out ea, 8);
  compute_effective_address_in_register (ea, X14);
  c_store_release (ZERO, X14, data_size => 4);

  // SMC Secure Monitor Call
  c_smc (0); // 0 to 65535
}

//======================================================================================

/*
MODEL for USLEEP:

void usleep (int sec, int msec)   // positive or zero
{
  TIME t;

  clock_gettime (1, out t);

  t.sec += sec;
  t.nsec += msec * 1_000_000;  // msec to nsec (X 10^6)

  if (t.nsec >= 1_000_000_000)
  {
    t.nsec -= 1_000_000_000;
    t.sec++;
  }

  while (clock_nanosleep (1, 1, ref t, 0) != 0)
    ;
}
*/

/*
    4688: a9bd7bfd     	stp	x29, x30, [sp, #-0x30]!
    468c: 8b3f63fd     	add	x29, sp, xzr
    4690: 2a1f0013     	orr	w19, w0, wzr
    4694: 2a1f0034     	orr	w20, w1, wzr
    4698: 910043b5     	add	x21, x29, #0x10

    469c: 320003e0     	orr	w0, wzr, #0x1
    46a0: aa1f02a1     	orr	x1, x21, xzr
    46a4: 90000031     	adrp	x17, 0x8000 <export_test+0x3ba0>
    46a8: f943a628     	ldr	x8, [x17, #0x748]
    46ac: d63f0100     	blr	x8

    46b0: f84002a0     	ldur	x0, [x21]
    46b4: 8b33e000     	add	x0, x0, x19, sxtx
    46b8: f80002a0     	stur	x0, [x21]

    46bc: f84082a0     	ldur	x0, [x21, #0x8]
    46c0: 52884816     	mov	w22, #0x4240            // =16960
    46c4: 72a001f6     	movk	w22, #0xf, lsl #16
    46c8: 9b160283     	madd	x3, x20, x22, x0
    46cc: 52994016     	mov	w22, #0xca00            // =51712
    46d0: 72a77356     	movk	w22, #0x3b9a, lsl #16
    46d4: eb16007f     	cmp	x3, x22
    46d8: 540000ab     	b.lt	0x46ec <export_test+0x28c>
    46dc: cb160063     	sub	x3, x3, x22
    46e0: f84002a0     	ldur	x0, [x21]
    46e4: 91000400     	add	x0, x0, #0x1
    46e8: f80002a0     	stur	x0, [x21]
    46ec: f80082a3     	stur	x3, [x21, #0x8]

    46f0: 320003e0     	orr	w0, wzr, #0x1
    46f4: 320003e1     	orr	w1, wzr, #0x1
    46f8: aa1f02a2     	orr	x2, x21, xzr
    46fc: 52800003     	mov	w3, #0x0                // =0
    4700: 90000031     	adrp	x17, 0x8000 <export_test+0x3ba0>
    4704: f943aa28     	ldr	x8, [x17, #0x750]
    4708: d63f0100     	blr	x8
    470c: 35ffff20     	cbnz	w0, 0x46f0 <export_test+0x290>
    4710: a8c37bbd     	ldp	x29, x30, [sp], #0x30
    4714: d65f03c0     	ret
*/

void gen_code_for_usleep ()
{
  int lab;

  lab = 0;

  declare_function (func_usleep);

  // X0 = seconds
  // X1 = milliseconds

  // untouched by OS calls : X19-X22

  // stp	x29, x30, [sp, #-0x30]!

  assert c_store_pair (source1   => X29,     // ZERO allowed
                       source2   => X30,     // ZERO allowed
                       base      => SP,      // SP allowed (effective address)
                       offset    => -0x30,   // -512 to +504 to be added to base address
                       ldp_mode  => PRE_ADD, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                       data_size => 8);      // 4 or 8

  // mov	x29, sp

  c_add_ext_reg (target          => X29,     // SP allowed (ZERO not allowed)
                 source1         => SP,      // SP allowed (ZERO not allowed)
                 source2         => ZERO,    // ZERO allowed (SP not allowed)
                 source2_size    => 8,       // 1, 2, 4 or 8
                 source2_signed  => false,
                 source2_shl_imm => 0,       // 0 .. 4
                 size            => 8);      // target size (4 or 8)

  // X19 = seconds
  c_mov_reg_reg (target    => X19,    // ZERO allowed (SP not allowed)
                 source    => X0,     // ZERO allowed (SP not allowed)
                 data_size => 4);     // 4 or 8

  // X20 = milliseconds
  c_mov_reg_reg (target    => X20,    // ZERO allowed (SP not allowed)
                 source    => X1,     // ZERO allowed (SP not allowed)
                 data_size => 4);     // 4 or 8

  // X21 = local temporary 16 bytes
  c_add_reg_imm (target     => X21,     // SP allowed (ZERO not allowed)
                 source     => FP,      // SP allowed (ZERO not allowed)
                 imm12      => 16,      // 0 to 4095
                 shl_imm_12 => false,   // true to shift imm12 << 12 (can reach 16 MB)
                 size       => 8);      // 4 or 8


  // clock_gettime (1, out t);

  move_register_immediate (target => X0, imm => 1, size => 4);

  c_mov_reg_reg (target    => X1,    // ZERO allowed (SP not allowed)
                 source    => X21,   // ZERO allowed (SP not allowed)
                 data_size => 8);    // 4 or 8
  call_libc ("clock_gettime");


  // t.sec += sec;

  assert c_load_ofs8 (target      => X0,    // ZERO allowed
                      base        => X21,   // SP allowed (effective address)
                      offset      => 0,     // 9 bits (-256 to 255) to be added to base address
                      data_signed => true,  // data to load is signed
                      data_size   => 8);    // size of data to load : 1, 2, 4, 8

  c_add_ext_reg (target          => X0,     // SP allowed (ZERO not allowed)
                 source1         => X0,     // SP allowed (ZERO not allowed)
                 source2         => X19,    // ZERO allowed (SP not allowed)
                 source2_size    => 8,      // 1, 2, 4 or 8
                 source2_signed  => true,
                 source2_shl_imm => 0,      // 0 .. 4
                 size            => 8);     // target size (4 or 8)

  assert c_store_ofs8 (source      => X0,    // ZERO allowed
                       base        => X21,   // SP allowed (effective address)
                       offset      => 0,     // 9 bits (-256 to 255) to be added to base address
                       data_size   => 8);    // size of data to load : 1, 2, 4, 8



  // t.nsec += msec * 1_000_000;      // msec to nsec (X 10^6)

  // t.nsec
  assert c_load_ofs8 (target      => X0,    // ZERO allowed
                      base        => X21,   // SP allowed (effective address)
                      offset      => 8,     // 9 bits (-256 to 255) to be added to base address
                      data_signed => true,  // data to load is signed
                      data_size   => 8);    // size of data to load : 1, 2, 4, 8

  move_register_immediate (target => X22,
                           imm    => 1_000_000L,   // 1 million
                           size   => 4);           // 4 or 8

  // (( X3 = t.nsec + msec * 1_000_000 ))
  c_mult_add (target    => X3,    // ZERO allowed
              sum       => X0,    // ZERO allowed  (t.nsec)
              mul1      => X20,   // ZERO allowed  (msec)
              mul2      => X22,   // ZERO allowed  (1 million)
              data_size => 8);    // 4 or 8

  move_register_immediate (target => X22,
                           imm    => 1_000_000_000L,   // 1 billion
                           size   => 4);               // 4 or 8


  // if (t.nsec >= 1_000_000_000)

  // CMP (shifted register)
  // can be used to shift values
  c_cmp_reg_reg (source1             => X3,   // ZERO allowed (SP not allowed)
                 source2             => X22,  // ZERO allowed (SP not allowed)
                 source2_shift_type  => LSL,  // LSL, LSR, ASR
                 source2_shift_value => 0,    // range 0..31 (or 0..63 for size==8)
                 size                => 8);   // 4 or 8

  cond_branch (CMP_SMALLER, signed => true, lab+1);

  // t.nsec -= 1_000_000_000;

  c_sub_reg_reg (target              => X3,   // ZERO allowed (SP not allowed)
                 source1             => X3,   // ZERO allowed (SP not allowed)
                 source2             => X22,  // ZERO allowed (SP not allowed)
                 source2_shift_type  => LSL,  // LSL, LSR, ASR
                 source2_shift_value => 0,    // range 0..31 (or 0..63 for size==8)
                 size                => 8);   // 4 or 8

   // t.sec++;

  // load t.sec
  assert c_load_ofs8 (target      => X0,    // ZERO allowed
                      base        => X21,   // SP allowed (effective address)
                      offset      => 0,     // 9 bits (-256 to 255) to be added to base address
                      data_signed => true,  // data to load is signed
                      data_size   => 8);    // size of data to load : 1, 2, 4, 8

  // add 1
  c_add_reg_imm (target     => X0,     // SP allowed (ZERO not allowed)
                 source     => X0,     // SP allowed (ZERO not allowed)
                 imm12      => 1,      // 0 to 4095
                 shl_imm_12 => false,  // true to shift imm12 << 12 (can reach 16 MB)
                 size       => 8);     // 4 or 8

  // store t.sec
  assert c_store_ofs8 (source      => X0,    // ZERO allowed
                       base        => X21,   // SP allowed (effective address)
                       offset      => 0,     // 9 bits (-256 to 255) to be added to base address
                       data_size   => 8);    // size of data to load : 1, 2, 4, 8

declare_near_label (lab+1);

  // store t.nsec
  assert c_store_ofs8 (source      => X3,    // ZERO allowed
                       base        => X21,   // SP allowed (effective address)
                       offset      => 8,     // 9 bits (-256 to 255) to be added to base address
                       data_size   => 8);    // size of data to load : 1, 2, 4, 8

  // while (clock_nanosleep (1, 1, ref t, 0) != 0)
  //   ;

declare_near_label (lab+2);

  move_register_immediate (target => X0,
                           imm    => 1,
                           size   => 4);  // 4 or 8

  move_register_immediate (target => X1,
                           imm    => 1,
                           size   => 4);  // 4 or 8

  c_mov_reg_reg (target    => X2,     // ZERO allowed (SP not allowed)
                 source    => X21,    // ZERO allowed (SP not allowed)
                 data_size => 8);     // 4 or 8

  move_register_immediate (target => X3,
                           imm    => 0,
                           size   => 4);  // 4 or 8

  call_libc ("clock_nanosleep");

  branch_if_not_zero (reg => X0, size => 4, near_label => lab+2);


  // ldp	x29, x30, [sp], #0x30
  assert c_load_pair (target1     => X29,       // ZERO allowed
                      target2     => X30,       // ZERO allowed
                      base        => SP,        // SP allowed (effective address)
                      offset      => 0x30,      // -512 to +504 to be added to base address
                      ldp_mode    => POST_ADD,  // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                      data_signed => false,     // for data_size 4 only
                      data_size   => 8);        // 4 or 8

  c_ret ();
}

//======================================================================================

public
void extra_arm_code ()
{
  // extra functions for allocate/free tombstones

  if (goptions . pointer_checks_enabled)
  {
    int i;
    for (i=0; i<3; i++)
    {
      reset_pool_backfills ();
      near_label_allocate_table (20);  // enough for code - we must do this in advance before P-code is generated

      if (i == 0)
        gen_code_for_tombstone_get_lock ();
      else if (i == 1)
        gen_code_for_tombstone_malloc ();
      else if (i == 2)
        gen_code_for_tombstone_free ();

      // shrink/extend near_label unconditional and conditional jumps
      // possibly expand code by converting label offset fields

      fixup.near_labels_relocate_all ();
    }
  }


  // extra function for usleep

  {
    reset_pool_backfills ();
    near_label_allocate_table (20);  // enough for code - we must do this in advance before P-code is generated

    gen_code_for_usleep ();

    fixup.near_labels_relocate_all ();
  }
}

//======================================================================================
