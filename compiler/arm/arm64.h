
// arm64.h

use ../common;

/************************************************************************/

// (64-bit) integer
enum REG { X0,  X1,  X2,  X3,  X4,  X5 , X6,  X7,  X8,  X9,
          X10, X11, X12, X13, X14, X15, X16, X17, X18, X19,
          X20, X21, X22, X23, X24, X25, X26, X27, X28,
          X29,     // X29 = frame pointer
          X30,     // X30 = procedure link register (contains return address at function entry)
          ZERO,    // X31 = zero register (always value zero)
          SP};     // X31 = stack pointer

const REG FP      = X29;   // frame pointer
const REG RETADDR = X30;   // procedure link register (contains return address at function entry)

// (64-bit) floating point
enum FREG { F0,  F1,  F2,  F3,  F4,  F5,  F6,  F7,  F8,  F9,
           F10, F11, F12, F13, F14, F15, F16, F17, F18, F19,
           F20, F21, F22, F23, F24, F25, F26, F27, F28, F29,
           F30, F31};

/*
Registers R0 to R7: are used to save arguments when calling a function
 R0 is used also to store the result which is returned by a function.
 Register R8: (Indirect result location register),
   used in C++ for returning non-trivial objects (set by the caller).
Registers R9 to R15: (known as scratch registers)
  can be used any time without any assumptions about their contents.
Registers R16, R17: (intra-procedure-call temporary registers)
  the linker may use these in PLT code. Can be used as temporary registers
  between calls.

Register R18: (platform register) reserved for the use of platform ABI.
   For example, for the windows ABI, in kernel mode,
   points to KPCR for the current processor; in user mode, points to TEB.
On Android, the platform-specific x18 register is reserved for ShadowCallStack
and should not be touched by your code.

Registers R19-R28: can also be used as scratch registers,
  but their contents must be saved before usage and restored afterwards.

Register R29: is used as a Frame Pointer
Register R30: is known as the link register (lr)
 and it can be used to store the return address during a function call,
 an alternative of saving the address to the call stack

---------------------------------------------------------------
The first eight registers, r0-r7, are used to pass argument values into a subroutine
and to return result values from a function. They may also be used to hold
intermediate values within a routine (but, in general, only between subroutine calls).

Registers r16 (IP0) and r17 (IP1) may be used by a linker as a scratch register
between a routine and any subroutine it calls (for details, see Use of IP0 and IP1 by the linker).
They can also be used within a routine to hold intermediate values between subroutine calls.

The role of register r18 is platform specific. If a platform ABI has need of a dedicated
general-purpose register to carry inter-procedural state (for example, the thread context)
then it should use this register for that purpose.
Software developers creating platform-independent code are advised to avoid using r18 if at all possible.
It should not be assumed that treating the register as callee-saved will be sufficient to satisfy the requirements of the platform.

A subroutine invocation must preserve the contents of the registers r19-r29 and SP.
All 64 bits of each value stored in r19-r29 must be preserved.

In all variants of the procedure call standard, registers r16, r17, r29 and r30 have special roles.
In these roles they are labeled IP0, IP1, FP and LR when being used for holding addresses
(that is, the special name implies accessing the register as a 64-bit entity).

floating point
The first eight registers, v0-v7, are used to pass argument values into a subroutine and
to return result values from a function. They may also be used to hold intermediate values
within a routine (but, in general, only between subroutine calls).

Registers v8-v15 must be preserved by a callee across subroutine calls;
the remaining registers (v0-v7, v16-v31) do not need to be preserved
(or should be preserved by the caller).
Additionally, only the bottom 64 bits of each value stored in v8-v15 need to be preserved ;
it is the responsibility of the caller to preserve larger values.

At any point at which memory is accessed via SP, the hardware requires that SP mod 16 = 0.
The stack must also conform to the following constraint at a public interface: SP mod 16 = 0.


[FP] -> points to previous FP frame, or zero
[FP+8] -> return address  (store X30 here at function entry)


For a caller, sufficient stack space to hold stacked argument values is assumed to have been allocated prior to marshaling
-> decrease SP for all parameters

*/


/************************************************************************/

enum SHIFT_TYPE {LSL,   // left shift (signed or unsigned)
                 LSR,   // right shift (unsigned)
                 ASR};  // right shift (signed)

enum LDP_MODE {INVALID_MODE, POST_ADD, SIMPLE_LOAD, PRE_ADD};

/************************************************************************/

// ADD (extended register) : t = s1 + (ext(s2) << shift)
void c_add_ext_reg (REG  target,          // SP allowed (ZERO not allowed)
                    REG  source1,         // SP allowed (ZERO not allowed)
                    REG  source2,         // ZERO allowed (SP not allowed)
                    int  source2_size,    // 1, 2, 4 or 8
                    bool source2_signed,
                    uint source2_shl_imm, // 0 .. 4
                    int  size);           // target size (4 or 8)

/************************************************************************/

// ADD (immediate)
void c_add_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095
                    bool shl_imm_12, // true to shift imm12 << 12 (can reach 16 MB)
                    int  size);      // 4 or 8

/************************************************************************/

// ADDS (immediate) (sets flags)
void c_adds_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                     REG  source,     // SP allowed (ZERO not allowed)
                     int  imm12,      // 0 to 4095
                     bool shl_imm_12, // true to shift imm12 << 12 (can reach 16 MB)
                     int  size);      // 4 or 8

/************************************************************************/

// ADD (shifted register)
// can be used to shift values
void c_add_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size);   // 4 or 8

/************************************************************************/

// SUB (extended register) : t = s1 + (ext(s2) << shift)
void c_sub_ext_reg (REG  target,          // SP allowed (ZERO not allowed)
                    REG  source1,         // SP allowed (ZERO not allowed)
                    REG  source2,         // ZERO allowed (SP not allowed)
                    int  source2_size,    // 1, 2, 4 or 8
                    bool source2_signed,
                    uint source2_shl_imm, // 0 .. 4
                    int  size);           // target size (4 or 8)

/************************************************************************/

// SUB (extended register) : t = s1 + (ext(s2) << shift)
void c_subs_ext_reg (REG  target,          // ZERO allowed (SP not allowed)
                     REG  source1,         // SP allowed (ZERO not allowed)
                     REG  source2,         // ZERO allowed (SP not allowed)
                     int  source2_size,    // 1, 2, 4 or 8
                     bool source2_signed,
                     uint source2_shl_imm, // 0 .. 4
                     int  size);           // target size (4 or 8)

/************************************************************************/

// SUB (immediate)
void c_sub_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095
                    bool shl_imm_12, // true to shift imm12 << 12
                    int  size);      // 4 or 8

/************************************************************************/

// SUBS (immediate) (sets flags)
void c_subs_reg_imm (REG  target,     // ZERO allowed (SP not allowed)
                     REG  source,     // SP allowed (ZERO not allowed)
                     int  imm12,      // 0 to 4095
                     bool shl_imm_12, // true to shift imm12 << 12
                     int  size);      // 4 or 8

/************************************************************************/

// SUB (shifted register)
// can be used to shift values
void c_sub_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size);   // 4 or 8

/************************************************************************/

// SUB (shifted register)
// can be used to shift values
void c_subs_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                     REG        source1, // ZERO allowed (SP not allowed)
                     REG        source2, // ZERO allowed (SP not allowed)
                     SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                     uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                     int        size);   // 4 or 8

/************************************************************************/

// NEG (shifted register)
void c_neg_reg_reg (REG target,  // ZERO allowed (SP not allowed)
                    REG source,  // ZERO allowed (SP not allowed)
                    int size);   // 4 or 8

/************************************************************************/

// MADD
// t = sum + mul1 x mul2
// unsigned multiplication and signed multiplication are exactly the same (ignoring flags).
void c_mult_add (REG   target,      // ZERO allowed
                 REG   sum,         // ZERO allowed
                 REG   mul1,        // ZERO allowed
                 REG   mul2,        // ZERO allowed
                 int   data_size);  // 4 or 8

/************************************************************************/

// MUL
// unsigned multiplication and signed multiplication are exactly the same (ignoring flags).
void c_mult (REG   target,      // ZERO allowed
             REG   mul1,        // ZERO allowed
             REG   mul2,        // ZERO allowed
             int   data_size);  // 4 or 8

/************************************************************************/

// MSUB: Multiply-Subtract
// t = term - mul1 x mul2
// unsigned multiplication and signed multiplication are exactly the same (ignoring flags).
void c_mult_sub (REG   target,      // ZERO allowed
                 REG   term,        // ZERO allowed
                 REG   mul1,        // ZERO allowed
                 REG   mul2,        // ZERO allowed
                 int   data_size);  // 4 or 8

/************************************************************************/

// UMADDL
// Unsigned Multiply-Add Long multiplies two 32-bit register values, adds a 64-bit register value, and writes the result to the 64-bit destination register.
// This instruction is used by the alias UMULL.

void c_umul_32_32_64_add_64 (REG   xtarget,      // ZERO allowed
                             REG   xsum,         // ZERO allowed
                             REG   wmul1,        // ZERO allowed
                             REG   wmul2);       // ZERO allowed

/************************************************************************/

// SMADDL
// Signed Multiply-Add Long multiplies two 32-bit register values, adds a 64-bit register value, and writes the result to the 64-bit destination register.
// This instruction is used by the alias SMULL

void c_smul_32_32_64_add_64 (REG   xtarget,      // ZERO allowed
                             REG   xsum,         // ZERO allowed
                             REG   wmul1,        // ZERO allowed
                             REG   wmul2);       // ZERO allowed

/************************************************************************/

// UMULH
// Unsigned Multiply High multiplies two 64-bit register values, and writes bits[127:64] of the 128-bit result to the 64-bit destination register.
// -> use for 64 bit unsigned div by constant

void c_umul_64_64_high_64 (REG xtarget,      // ZERO allowed
                           REG xmul1,        // ZERO allowed
                           REG xmul2);       // ZERO allowed

/************************************************************************/

// SMULH
// Signed Multiply High multiplies two 64-bit register values,
// and writes bits[127:64] of the 128-bit result to the 64-bit destination register.

void c_smul_64_64_high_64 (REG xtarget,      // ZERO allowed
                           REG xmul1,        // ZERO allowed
                           REG xmul2);       // ZERO allowed

/************************************************************************/

// SDIV / UDIV
void c_div (REG   target,      // ZERO allowed
            REG   source1,     // ZERO allowed
            REG   source2,     // ZERO allowed
            bool  signed,
            int   data_size);  // 4 or 8

/************************************************************************/

// ADR : target = PC + offset (+/- 1 MB range)
// load 21 bit unsigned value in register
void c_adr (REG target,  // ZERO allowed (SP not allowed)
            int imm);    // 21 bit

/************************************************************************/

// ADRP : target = PC + 4K-offset (+/- 4 GB range)
// load 4K page address in register
void c_adrp (REG target,  // ZERO allowed (SP not allowed)
             int imm);    // 21 bit (will be shifted 12 bits to the left)

/************************************************************************/

// AND (immediate)
// returns false if the imm value is not supported
bool c_and_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // ZERO allowed (SP not allowed)
                    int8 imm,
                    int  size);      // 4 or 8

/************************************************************************/

// ANDS (immediate)
// returns false if the imm value is not supported
bool c_ands_reg_imm (REG  target,     // ZERO allowed (SP not allowed)
                     REG  source,     // ZERO allowed (SP not allowed)
                     int8 imm,
                     int  size);      // 4 or 8

/************************************************************************/

// AND (shifted register)
// can be used to shift values
void c_and_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size);   // 4 or 8

/************************************************************************/

// ANDS (shifted register)
// can be used to shift values
void c_ands_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                     REG        source1, // ZERO allowed (SP not allowed)
                     REG        source2, // ZERO allowed (SP not allowed)
                     SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                     uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                     int        size);   // 4 or 8

/************************************************************************/

// EOR (immediate)
// returns false if the imm value is not supported
bool c_eor_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                    REG  source,     // ZERO allowed (SP not allowed)
                    int8 imm,
                    int  size);      // 4 or 8

/************************************************************************/

// EOR (shifted register)
// can be used to shift values
void c_eor_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size);   // 4 or 8

/************************************************************************/

// OR (immediate)
// returns false if the imm value is not supported
bool c_or_reg_imm (REG  target,     // SP allowed (ZERO not allowed)
                   REG  source,     // ZERO allowed (SP not allowed)
                   int8 imm,
                   int  size);      // 4 or 8

/************************************************************************/

// OR (shifted register)
// can be used to shift values
void c_or_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                   REG        source1, // ZERO allowed (SP not allowed)
                   REG        source2, // ZERO allowed (SP not allowed)
                   SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                   uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                   int        size);   // 4 or 8

/************************************************************************/

// ORN (shifted register)
// OR of a register and the complement of an optionally shifted register
// can be used to shift values
void c_orn_reg_reg (REG        target,  // ZERO allowed (SP not allowed)
                    REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size);   // 4 or 8

/************************************************************************/

// NOT
void c_not_reg_reg (REG target,     // ZERO allowed (SP not allowed)
                    REG source,     // ZERO allowed (SP not allowed)
                    int data_size); // 4 or 8

/************************************************************************/

// LSL (immediate)
// multiply by power of 2
// signed or unsigned
void c_lsl_imm (REG   target,      // ZERO allowed
                REG   source,      // ZERO allowed (base effective address)(64 bit address)
                int   shifts,      // 1 to 63  (or 1 to 31 for size 4)
                int   data_size);  // 4 or 8

/************************************************************************/

// ASR (immediate)
// divide by power of 2
// signed
void c_asr_imm (REG  target,     // ZERO allowed (SP not allowed)
                REG  source,     // ZERO allowed (SP not allowed)
                int  shifts,     // 1 to 63  (or 1 to 31 for size 4)
                int  data_size); // 4 or 8

/************************************************************************/

// LSR (immediate)
// divide by power of 2
// unsigned
void c_lsr_imm (REG   target,      // ZERO allowed
                REG   source,      // SP allowed (base effective address)(64 bit address)
                int   shifts,      // 1 to 63  (or 1 to 31 for size 4)
                int   data_size);  // 4 or 8

/************************************************************************/

// LSL (register)
// multiply by power of 2
// signed or unsigned
void c_lsl_reg (REG   target,      // ZERO allowed
                REG   source,      // ZERO allowed (base effective address)(64 bit address)
                REG   shifts,      // 0 to 63  (or 0 to 31 for size 4)
                int   data_size);  // 4 or 8

/************************************************************************/

// ASR (register)
// divide by power of 2
// signed
void c_asr_reg (REG  target,     // ZERO allowed (SP not allowed)
                REG  source,     // ZERO allowed (SP not allowed)
                REG  shifts,     // ZERO allowed (SP not allowed) (0 .. 31, or 0 .. 63)
                int  data_size); // 4 or 8

/************************************************************************/

// LSR (register)
// divide by power of 2
// unsigned
void c_lsr_reg (REG   target,      // ZERO allowed
                REG   source,      // ZERO allowed (base effective address)(64 bit address)
                REG   shifts,      // 0 to 63  (or 0 to 31 for size 4)
                int   data_size);  // 4 or 8

/************************************************************************/

// B  (branch PC +/- 128 MB)
void c_jmp (int offset); // 28 bits (+/- 1<<27)  +/- 128 MB

/************************************************************************/

// BR  (branch to register)
void c_jmp_reg (REG target);

/************************************************************************/

// BL  (branch to subroutine relative to PC +/- 128 MB)
// store PC+4 in X30 then branch
void c_jsr (int offset);  // 28 bits (+/- 1<<27)  +/- 128 MB

/************************************************************************/

// BL  (branch to subroutine in register)
// store PC+4 in X30 then branch
void c_jsr_reg (REG target);

/************************************************************************/

void c_ret (REG target = X30);

/************************************************************************/

// B.cond  (conditional branch PC +/- 1MB)
void c_cond_branch (COMPARISON_FLAG cmp,
                    bool signed,
                    int  offset,  // 21 bits (+/- 1<<20)  +/- 1MB
                    bool often_same_choice = true);

/************************************************************************/

// B.cond  (conditional branch PC +/- 1MB)
void c_cond_branch_raw (byte mask,
                        int  offset,  // 21 bits (+/- 1<<20)  +/- 1MB
                        bool often_same_choice = true);
/*
mask:
0b0100 : sign bit set
0b0101 : sign bit clear
0b0110 : overflow set
0b0111 : overflow clear
*/

/************************************************************************/

// CSEL
// If the condition is true, Conditional Select writes the value of the first source register to the destination register.
// If the condition is false, it writes the value of the second source register to the destination register.

void csel (REG             target,
           REG             source1,
           REG             source2,
           COMPARISON_FLAG cmp,
           bool            signed,
           int             data_size);  // 4 or 8

/************************************************************************/

// CBNZ (branch if not zero)
void c_bnz_reg (REG source,  // register being tested
                int size,    // 4 or 8 bytes
                int offset); // 21 bits (+/- 1<<20)  +/- 1MB

/************************************************************************/

// CBZ (branch if zero)
void c_bz_reg (REG source,  // register being tested
               int size,    // 4 or 8 bytes
               int offset); // 21 bits (+/- 1<<20)  +/- 1MB

/************************************************************************/

// TBNZ: Test bit and Branch if Nonzero.
void c_tbnz (REG source,         // register being tested
             int bit,            // bit number (0 to 63)
             int branch_offset); // 16 bits (-32768 .. +32764)  +/- 32K

/************************************************************************/

// TBZ: Test bit and Branch if Zero.
void c_tbz (REG source,         // register being tested
            int bit,            // bit number (0 to 63)
            int branch_offset); // 16 bits (-32768 .. +32764)  +/- 32K

/************************************************************************/

// CMP (extended register)
void c_cmp_ext_reg (REG  source1,         // SP allowed (ZERO not allowed)
                    REG  source2,         // ZERO allowed (SP not allowed)
                    int  source2_size,    // 1, 2, 4 or 8
                    bool source2_signed,
                    uint source2_shl_imm, // 0 .. 4
                    int  size);           // compare size (4 or 8)

/************************************************************************/

// CMP (immediate)
void c_cmp_reg_imm (REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095
                    bool shl_imm_12, // true to shift imm12 << 12
                    int  size);      // 4 or 8

/************************************************************************/

// CMN (immediate) : compare reg with negative value
void c_cmn_reg_imm (REG  source,     // SP allowed (ZERO not allowed)
                    int  imm12,      // 0 to 4095 (used as negative value)
                    bool shl_imm_12, // true to shift imm12 << 12
                    int  size);      // 4 or 8

/************************************************************************/

// CMP (shifted register)
// can be used to shift values
void c_cmp_reg_reg (REG        source1, // ZERO allowed (SP not allowed)
                    REG        source2, // ZERO allowed (SP not allowed)
                    SHIFT_TYPE source2_shift_type,  // LSL, LSR, ASR
                    uint       source2_shift_value, // range 0..31 (or 0..63 for size==8)
                    int        size);   // 4 or 8

/************************************************************************/

// TST (shifted register)
// sets condition flags
void c_tst_reg (REG source,  // ZERO allowed (SP not allowed)
                int size);   // 4 or 8

/************************************************************************/

// CSET : Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
// used for: b = (x < y);
void c_cset (COMPARISON_FLAG cmp,
             bool            signed,
             REG             target,
             int             size);      // 4 or 8

/************************************************************************/

// LDADD : atomic ADD to
void c_addto (REG target,   // ZERO allowed - register receives loaded initial value
              REG addr,     // SP allowed   - address of memory to change
              REG source,   // ZERO allowed - register value to add
              int size);    // 1, 2, 4 or 8

/************************************************************************/

// LDCLR : atomic AND to (FEAT_LSE - only ARM 8.1)
void c_andto (REG target,   // ZERO allowed - register receives loaded initial value
              REG addr,     // SP allowed   - address of memory to change
              REG source,   // ZERO allowed - register value to and
              int size);    // 1, 2, 4 or 8

/************************************************************************/

// LDSET : atomic OR to (FEAT_LSE - only ARM 8.1)
void c_orto (REG target,   // ZERO allowed - register receives loaded initial value
             REG addr,     // SP allowed   - address of memory to change
             REG source,   // ZERO allowed - register value to or
             int size);    // 1, 2, 4 or 8

/************************************************************************/

// LDEOR : Atomic Exclusive-OR  (FEAT_LSE - only ARM 8.1)
void c_eorto (REG target,   // ZERO allowed - register receives loaded initial value
              REG addr,     // SP allowed   - address of memory to change
              REG source,   // ZERO allowed - register value to eor
              int size);    // 1, 2, 4 or 8

/************************************************************************/

// LDR (literal) LDRSW
// load int4/uint4/int8 constant from PC + offset (+/- 1MB) into 8 byte register
void c_load_cte (REG  target,
                 int  offset,    // +/- 1 MB (must be aligned to 4)
                 bool signed,
                 int  cte_size); // 4 or 8 bytes

/************************************************************************/

// LDR (literal, SIMD&FP)
// load float/double constant from PC + offset (+/- 1 MB) into float register
void c_fload_cte (FREG  target,
                  int  offset,    // +/- 1 MB (must be aligned to 4)
                  int  cte_size); // 4 or 8 bytes

/************************************************************************/

// LDUR (immediate), LDURB LDURH LDURSB LDURSH LDURSW
// returns false if offset is not encodable
bool c_load_ofs8 (REG   target,      // ZERO allowed
                  REG   base,        // SP allowed (effective address)
                  int   offset,      // 9 bits (-256 to 255) to be added to base address
                  bool  data_signed, // data to load is signed
                  int   data_size);  // size of data to load : 1, 2, 4, 8

/************************************************************************/

// LDR (immediate), LDRB, LDRH, LDRSB, LDRSH, LDRSW
// load item at address (source + offset)
// returns false if offset is not encodable
bool c_load_ofs12 (REG   target,      // ZERO allowed
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // 12 bits (0 to 4095 * size) to be added to base address
                   bool  data_signed,
                   int   data_size);  // 1, 2, 4 or 8

/************************************************************************/

// LDR (immediate), LDRB, LDRH, LDRSH, LDRSH, LDRSW
// load item at address (source + offset)
// source will be increased by offset before or after the loading.
void c_load_add (REG   target,      // ZERO allowed
                 REG   base,        // SP allowed (effective address)
                 int   offset,      // 9 bits (-256 to 255) to be added to base address
                 bool  pre_add,     // false : post_add base by offset, true : pre_add base by offset
                 bool  data_signed,
                 int   data_size);  // 1, 2, 4 or 8

/************************************************************************/

// LDP
// returns false if offset could not be encoded
bool c_load_pair (REG   target1,     // ZERO allowed
                  REG   target2,     // ZERO allowed
                  REG   base,        // SP allowed (effective address)
                  int   offset,      // -512 to +504 to be added to base address
                  LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                  bool  data_signed,  // for data_size 4 only
                  int   data_size);  // 4 or 8

/************************************************************************/

// LDR (register), LDRB, LDRH, LDRSB, LDRSH, LDRSW
void c_load_reg_reg (REG   target,         // ZERO allowed
                     REG   base,           // SP allowed (base effective address)(64 bit address)
                     REG   index,          // ZERO allowed
                     bool  index_signed,
                     int   index_size,     // 4 or 8
                     bool  mult_index_by_data_size,
                     bool  data_signed,
                     int   data_size);     // 1, 2, 4 or 8

/************************************************************************/

// MOV (bitmask immediate)
// alias of ORR (immediate) with zero
// returns false if this imm value is not supported
bool move_imm (REG  target,     // SP allowed
               int8 imm,
               int  data_size);  // 4 or 8

/************************************************************************/

// MOVZ
// move 16 bit value in 2 bytes of the register, setting the rest to zero.
void c_movz (REG   target,
             uint2 imm,         // 0 to 65535
             int   shifts_left, // 0, 16, 32 or 48
             int   data_size);  // 4 or 8

/************************************************************************/

// MOVN
// move 16 bit value in 2 bytes of the register, setting the rest to zero,
// then inverting all bits.
void c_movn (REG   target,
             uint2 imm,         // 0 to 65535
             int   shifts_left, // 0, 16, 32 or 48
             int   data_size);  // 4 or 8

/************************************************************************/

// MOVK
// move 16 bit value in 2 bytes of the register, leaving other bits unchanged.
void c_movk (REG   target,
             uint2 imm,         // 0 to 65535
             int   shifts_left, // 0, 16, 32 or 48
             int   data_size);  // 4 or 8

/************************************************************************/

// STUR STURB STURH
// returns false if offset is not encodable
bool c_store_ofs8 (REG   source,      // ZERO allowed
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // 9 bits (-256 to 255) to be added to base address
                   int   data_size);  // size of data to load : 1, 2, 4, 8

/************************************************************************/

// STR (immediate) STRB STRH
// store item at address (base + offset)
// returns false if offset is not encodable
bool c_store_ofs12 (REG   source,      // ZERO allowed
                    REG   base,        // SP allowed (effective address)
                    int   offset,      // 12 bits (0 to 4095 * size) to be added to base address
                    int   data_size);  // 1, 2, 4 or 8

/************************************************************************/

// STR (immediate) STRB STRH
// store item at address (base + offset)
// base will be increased by offset before or after the storing.
void c_store_add (REG  source,      // ZERO allowed
                  REG  base,        // SP allowed
                  int  offset,      // 9 bits (-256 to 255) to be added to base address
                  bool pre_add,     // false : post_add base by offset, true : pre_add base by offset
                  int  data_size);  // 1, 2, 4 or 8

/************************************************************************/

// STP
// returns false if offset could not be encoded
bool c_store_pair (REG   source1,     // ZERO allowed
                   REG   source2,     // ZERO allowed
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // -512 to +504 to be added to base address
                   LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                   int   data_size);  // 4 or 8

/************************************************************************/

// STR (register) STRB STRH
void c_store_reg_reg (REG   source,         // ZERO allowed
                      REG   base,           // SP allowed (base effective address)(64 bit address)
                      REG   index,          // ZERO allowed
                      bool  index_signed,
                      int   index_size,     // 4 or 8
                      bool  mult_index_by_data_size,
                      int   data_size);     // 1, 2, 4 or 8

/************************************************************************/

// SXTB SXTH SXTW
void c_extend_signed (REG  target,       // 8 bytes
                      REG  source,
                      int  source_size); // 1, 2 or 4 bytes

/************************************************************************/

// UXTB UXTH
void c_extend_unsigned (REG  target,       // 8 bytes
                        REG  source,
                        int  source_size); // 1 or 2 bytes

/************************************************************************/

// SVC supervisor call
void c_svc (int imm);  // 0 to 65535

/************************************************************************/

// SMC Secure Monitor Call
void c_smc (int imm); // 0 to 65535

/************************************************************************/

// YIELD (give up time slice)
void c_yield ();

/************************************************************************/

void c_nop ();

/************************************************************************/

void c_fneg (FREG target, FREG source, int size);    // 4 or 8

void c_fadd (FREG target, FREG source1, FREG source2, int size);  // 4 or 8
void c_fsub (FREG target, FREG source1, FREG source2, int size);  // 4 or 8
void c_fmul (FREG target, FREG source1, FREG source2, int size);  // 4 or 8
void c_fdiv (FREG target, FREG source1, FREG source2, int size);  // 4 or 8
void c_fcmp (FREG source1, FREG source2, int size);  // 4 or 8
void c_fcmp_zero (FREG source1, int size);  // 4 or 8

/************************************************************************/

// LDUR (SIMD&FP): Load SIMD&FP Register (unscaled offset)
// returns false if offset is not encodable
bool c_fload_ofs8 (FREG  target,
                   REG   base,        // SP allowed (effective address)
                   int   offset,      // 9 bits (-256 to 255) to be added to base address
                   int   data_size);  // size of data to load : 4 or 8

/************************************************************************/

// LDR (immediate, SIMD&FP)
// load item at address (base + offset)
// returns false if offset is not encodable
bool c_fload_ofs12 (FREG target,
                    REG  base,        // SP allowed (effective address)
                    int  offset,      // 12 bits (0 to 4095 * size) to be added to base address
                    int  data_size);  // 4 or 8

/************************************************************************/

// LDR (immediate, SIMD&FP)
// load item at address (base + offset)
// base will be increased by offset before or after the loading.
void c_fload_add (FREG  target,
                  REG   base,        // SP allowed (effective address)
                  int   offset,      // 9 bits (-256 to 255) to be added to base address
                  bool  pre_add,     // false : post_add base by offset, true : pre_add base by offset
                  int   data_size);  // 4 or 8

/************************************************************************/

// LDP (SIMD&FP): Load Pair of SIMD&FP registers
// returns false if offset could not be encoded
bool c_fload_pair (FREG   target1,
                   FREG   target2,
                   REG    base,        // SP allowed (effective address)
                   int    offset,      // -512 to +504 to be added to base address
                   LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                   int    data_size);  // 4 or 8

/************************************************************************/

// LDR (register, SIMD&FP): Load SIMD&FP Register (register offset).
void c_fload_reg_reg (FREG  target,
                      REG   base,           // SP allowed (base effective address)(64 bit address)
                      REG   index,          // ZERO allowed
                      bool  index_signed,
                      int   index_size,     // 4 or 8
                      bool  mult_index_by_data_size,
                      int   data_size);     // 4 or 8

/************************************************************************/

// STUR (SIMD&FP): Store SIMD&FP register (unscaled offset).
// returns false if offset is not encodable
bool c_fstore_ofs8 (FREG  source,
                    REG   base,        // SP allowed (effective address)
                    int   offset,      // 9 bits (-256 to 255) to be added to base address
                    int   data_size);  // size of data to load : 4, 8

/************************************************************************/

// STR (immediate, SIMD&FP): Store SIMD&FP register (immediate offset).
// store item at address (base + offset)
// returns false if offset is not encodable
bool c_fstore_ofs12 (FREG  source,
                     REG   base,        // SP allowed (effective address)
                     int   offset,      // 12 bits (0 to 4095 * size) to be added to base address
                     int   data_size);  // 4 or 8

/************************************************************************/

// STR (immediate, SIMD&FP): Store SIMD&FP register (immediate offset).
// store item at address (base + offset)
// base will be increased by offset before or after the storing.
void c_fstore_add (FREG source,
                   REG  base,        // SP allowed
                   int  offset,      // 9 bits (-256 to 255) to be added to base address
                   bool pre_add,     // false : post_add base by offset, true : pre_add base by offset
                   int  data_size);  // 4 or 8

/************************************************************************/

// STP (SIMD&FP): Store Pair of SIMD&FP registers.
// returns false if offset could not be encoded
bool c_fstore_pair (FREG  source1,
                    FREG  source2,
                    REG   base,        // SP allowed (effective address)
                    int   offset,      // -512 to +504 to be added to base address
                    LDP_MODE ldp_mode, // POST_ADD, SIMPLE_LOAD, PRE_ADD : adds offset to base
                    int   data_size);  // 4 or 8

/************************************************************************/

// STR (register, SIMD&FP): Store SIMD&FP register (register offset).
void c_fstore_reg_reg (FREG  source,
                       REG   base,           // SP allowed (base effective address)(64 bit address)
                       REG   index,          // ZERO allowed
                       bool  index_signed,
                       int   index_size,     // 4 or 8
                       bool  mult_index_by_data_size,
                       int   data_size);     // 4 or 8

/************************************************************************/

// MOV (register)
// move register to register
void c_mov_reg_reg (REG  target,     // ZERO allowed (SP not allowed)
                    REG  source,     // ZERO allowed (SP not allowed)
                    int  data_size); // 4 or 8

/************************************************************************/

// FMOV (general) Floating-point to general-purpose register without conversion.
void c_regf_to_reg (REG  target,
                    FREG source,
                    int  data_size); // 4 or 8

/************************************************************************/

// FMOV (general) general-purpose register to Floating-point without conversion.
// can be used to set float register to zero
void c_reg_to_regf (FREG target,
                    REG  source,     // can be ZERO
                    int  data_size); // 4 or 8

/************************************************************************/

// FMOV (register): float to float without conversion
void c_regf_to_regf (FREG target,
                     FREG source,
                     int  data_size); // 4 or 8

/************************************************************************/

// FMOV (scalar, immediate): Floating-point move immediate (scalar).
// returns false if immediate value could not be encoded.
bool c_fmov (FREG  target,
             float cte,
             int   size);  // 4 or 8

/************************************************************************/

// FCVT  Floating-point Convert precision (scalar).
// single <-> double
void c_fconv_precision (FREG  target,
                        int   target_size,   // 4 or 8
                        FREG  source,
                        int   source_size);  // 4 or 8

/************************************************************************/

// FCVTZS (scalar, integer): Floating-point Convert to Signed integer, rounding toward Zero (scalar).
// FCVTZU (scalar, integer): Floating-point Convert to Unsigned integer, rounding toward Zero (scalar).
// truncate float to int
void c_conv_float_to_int (REG   target,        // can be ZERO
                          bool  target_signed,
                          int   target_size,   // 4 or 8
                          FREG  source,
                          int   source_size);  // 4 or 8

/************************************************************************/

// SCVTF (scalar, integer)	Signed integer convert to floating-point
// UCVTF (scalar, integer)	Unsigned integer convert to floating-point
// int to float
void c_conv_int_to_float (FREG  target,
                          int   target_size,   // 4 or 8
                          REG   source,        // can be ZERO
                          bool  source_signed,
                          int   source_size);  // 4 or 8

/************************************************************************/

// SWP (FEAT_LSE - only ARM 8.1)
void c_swap (REG  address,   // can be SP
             REG  new_value, // can be ZERO
             REG  old_value, // can be ZERO
             int  size);     // 4 or 8

/************************************************************************/

// LDAXRB/LDAXRH/LDAXR : Load Exclusive
void c_load_excl (REG   target,         // ZERO allowed (data_size bytes)
                  REG   base,           // SP allowed (base effective address)(64 bit address)
                  int   data_size);     // 1, 2, 4 or 8

/************************************************************************/

// STLXRB/STLXRH/STLXR : Store Exclusive
void c_store_excl (REG  target,     // 32-bit register will contain status result : 0 = ok, 1 = failed
                   REG  base,       // SP allowed (base effective address)(64 bit address)
                   REG  source,     // ZERO allowed (data_size bytes)
                   int  data_size); // 1, 2, 4 or 8

/************************************************************************/

// STLR/STLRB/STLRH
// store item at address, and release
void c_store_release (REG   source,      // ZERO allowed
                      REG   base,        // SP allowed (effective address)
                      int   data_size);  // 1, 2, 4 or 8

/************************************************************************/

const uint CRM_ISHLD = 0b1001;   // reads from cache
const uint CRM_ISHST = 0b1010;   // writes to cache
const uint CRM_ISH   = 0b1011;   // reads and writes cache

const uint CRM_FETCH_CACHE = CRM_ISHLD;   // reads from cache
const uint CRM_FLUSH_CACHE = CRM_ISHST;   // writes to cache
const uint CRM_BARRIER     = CRM_ISH;     // reads and writes cache

// memory barrier
void c_dmb (uint crm);

/************************************************************************/

// raw assembler code
void c_code (byte b);

/************************************************************************/
