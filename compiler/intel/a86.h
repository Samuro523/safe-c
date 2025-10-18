
// a86.h : generate intel/amd 80x86 machine code (32- or 64-bit)

use ../common;

/************************************************************************/

enum REG {RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI,       // both 32- and 64-bit
          R8,  R9,  R10, R11, R12, R13, R14, R15, RIP,  // only 64-bit
          NONE};

enum XM {X0, X1, X2,  X3,  X4,  X5,  X6,  X7,
         X8, X9, X10, X11, X12, X13, X14, X15};

enum RELOC_KIND { RELOC_NONE, RELOC_FUNC, RELOC_DLL, RELOC_POOL, RELOC_GLOBAL };

struct RELOC_INFO
{
  RELOC_KIND  kind;
  int8        nr;   // func-label, dll-nr, pool-id or global-rel-addr.
}

struct EA  // effective address
{
  REG        base;     // RAX..RDI or NONE  (for 64-bit RAX..RIP or NONE)
  REG        index;    // as base but RSP,RIP not allowed (index must be NONE if base is RIP)
  int        scale;    // 1, 2, 4 or 8 (!= 1 allowed only if index != NONE)
  int        offset;   // limited to 32-bit signed
  RELOC_INFO reloc;
}

/************************************************************************/

// REG :

// 32 bit : size 1 :  AL,  BL,  CL,  DL
//          size 2 :  AX,  BX,  CX,  DX,  BP,  SP,  SI,  DI
//          size 4 : EAX, EBX, ECX, EDL, EBP, ESP, ESI, EDI

// 64 bit : size 1, 2, 4, 8 (all registers)

void c_add_reg_reg (REG target, REG source, int size);
void c_add_reg_mem (REG target, EA  source, int size);
void c_add_mem_reg (EA  target, REG source, int size);
void c_add_reg_imm (REG target, int imm,    int size);    // imm limited to 32-bit
void c_add_mem_imm (EA  target, int imm,    int size);    // imm limited to 32-bit

void c_adc_reg_reg (REG target, REG source, int size);
void c_adc_reg_mem (REG target, EA  source, int size);
void c_adc_mem_reg (EA  target, REG source, int size);
void c_adc_reg_imm (REG target, int imm,    int size);    // imm limited to 32-bit
void c_adc_mem_imm (EA  target, int imm,    int size);    // imm limited to 32-bit

void c_sub_reg_reg (REG target, REG source, int size);
void c_sub_reg_mem (REG target, EA  source, int size);
void c_sub_mem_reg (EA  target, REG source, int size);
void c_sub_reg_imm (REG target, int imm,    int size);    // imm limited to 32-bit
void c_sub_mem_imm (EA  target, int imm,    int size);    // imm limited to 32-bit

void c_sbb_reg_reg (REG target, REG source, int size);
void c_sbb_reg_mem (REG target, EA  source, int size);
void c_sbb_mem_reg (EA  target, REG source, int size);
void c_sbb_reg_imm (REG target, int imm,    int size);   // imm limited to 32-bit
void c_sbb_mem_imm (EA  target, int imm,    int size);   // imm limited to 32-bit

void c_and_reg_reg (REG target, REG source, int size);
void c_and_reg_mem (REG target, EA  source, int size);
void c_and_mem_reg (EA  target, REG source, int size);
void c_and_reg_imm (REG target, int imm,    int size);   // imm limited to 32-bit
void c_and_mem_imm (EA  target, int imm,    int size);   // imm limited to 32-bit

void c_or_reg_reg (REG target, REG source, int size);
void c_or_reg_mem (REG target, EA  source, int size);
void c_or_mem_reg (EA  target, REG source, int size);
void c_or_reg_imm (REG target, int imm,    int size);   // imm limited to 32-bit
void c_or_mem_imm (EA  target, int imm,    int size);   // imm limited to 32-bit

void c_xor_reg_reg (REG target, REG source, int size);
void c_xor_reg_mem (REG target, EA  source, int size);
void c_xor_mem_reg (EA  target, REG source, int size);
void c_xor_reg_imm (REG target, int imm,    int size);   // imm limited to 32-bit
void c_xor_mem_imm (EA  target, int imm,    int size);   // imm limited to 32-bit

void c_cmp_reg_reg (REG target, REG source, int size);
void c_cmp_reg_mem (REG target, EA  source, int size);
void c_cmp_mem_reg (EA  target, REG source, int size);
void c_cmp_reg_imm (REG target, int imm,    int size);   // imm limited to 32-bit
void c_cmp_mem_imm (EA  target, int imm,    int size);   // imm limited to 32-bit

/************************************************************************/

// generates 5 bytes of code
void c_call_relative (int func_label_nr, int nb_arguments);

// size MUST be address size
// 32-bit : EAX .. EDI
// 64-bit : RAX .. R15
void c_call_reg (REG source, int size, int nb_arguments);

// size MUST be address size
void c_call_indirect (EA source, int size, int nb_arguments);

/************************************************************************/

// generates 2 bytes of code (maybe later expanded to 5 bytes)
void c_jump_relative (int near_label_nr);

// size MUST be address size
void c_jump_indirect (EA source, int size);

/************************************************************************/

// generates 2 bytes of code (maybe later expanded to 6 bytes)
// depending if relative_offset fits into -128 .. +127 or not.
// ! offset is relative to RIP of following instruction !

void c_jcond_raw (byte opcode2,    // 0=jo, 1=jno, 8=js, 9=jns, 10=jp, 11=jnp
                  int  near_label_nr);

/************************************************************************/

// generates 2 bytes of code (maybe later expanded to 6 bytes)
// depending if relative_offset fits into -128 .. +127 or not.
// ! offset is relative to RIP of following instruction !

void c_jcond (COMPARISON_FLAG condition,
              bool            signed_operands,
              int             near_label_nr);

/************************************************************************/

// REG
// 32-bit : al, bl, cl, dl
// 64-bit : al, bl, cl, dl, bpl, spl, sil, dil, r8b to r15b

void c_setcond_reg (COMPARISON_FLAG condition,
                    bool            signed_operands,
                    REG             target,
                    int             size);       // always 1

void c_setcond_mem (COMPARISON_FLAG condition,
                    bool            signed_operands,
                    EA              target,
                    int             size);       // always 1

/************************************************************************/

void c_leave ();
void c_ret (int rsp_offset);   // rsp_offset in range 0 .. 32767

/************************************************************************/

// generate interrupt (usually 0 for division by zero, or 5 for constraint error)
void c_int (byte nr);

/************************************************************************/

// size indicates REG : 4 or 8 bytes

void c_lea_reg_mem (REG target, EA source, int size);

/************************************************************************/

// 32 bit : EAX, EBX, ECX, EDX, EBP, ESP, ESI, EDI
// 64 bit : all registers
// ! always pushes address-size bytes on stack
// ! only allows size == address_size
void c_push_reg (REG source, int size);

/************************************************************************/

// ! always pushes address-size bytes on stack
// ! only allows size == address_size
void c_push_mem (EA source, int size);

/************************************************************************/

// ! always pushes address-size bytes on stack
void c_push_imm (int4 imm);      // imm is limited to 32-bit values

/************************************************************************/

// ! always pushes address-size bytes on stack
void c_push_imm_reloc (int4 imm, RELOC_INFO preloc);   // relocatable constant

/************************************************************************/

void c_push_flags ();
void c_pop_flags ();

/************************************************************************/

// ! only allows size == address_size
// ! always pops address-size bytes from stack

void c_pop_reg (REG target, int size);
void c_pop_mem (EA source, int size);

/************************************************************************/

// 32-bit : EAX .. EDI
// 64-bit : RAX .. R15

void c_neg_reg (REG target, int size);
void c_neg_mem (EA  target, int size);
void c_not_reg (REG target, int size);
void c_not_mem (EA  target, int size);


/*
- 'not' on EDX:EAX:

   not eax
   not edx

- 'neg' the 64-bit value in (EDX:EAX) :

   neg edx
   neg eax
   sbb 0,edx
*/

/************************************************************************/

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

void c_inc_reg (REG target, int size);
void c_dec_reg (REG target, int size);

void c_inc_mem (EA target, int size);
void c_dec_mem (EA target, int size);

/************************************************************************/

// REG

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

void c_xchg_reg_reg (REG source1, REG source2, int size);

void c_xchg_reg_mem (REG target, EA source, int size);

/************************************************************************/

// REG

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

void c_test_reg_reg (REG source1, REG source2, int size);
void c_test_mem_reg (EA  source1, REG source2, int size);
void c_test_reg_imm (REG source, int4 imm,     int size);  // imm is limited to 32-bit
void c_test_mem_imm (EA  source, int4 imm,     int size);  // imm is limited to 32-bit

/************************************************************************/

// REG

// 32 bit mode : size 1 (AL, CL, DL, BL)
//               size 2 (AX .. DI)
//               size 4 (EAX .. EDI)

// 64-bit mode : size 1, 2, 4, 8  all registers

void c_mov_reg_reg (REG target, REG source, int size);
void c_mov_reg_mem (REG target, EA  source, int size);
void c_mov_mem_reg (EA  target, REG source, int size);
void c_mov_reg_imm (REG target, int8 imm,   int size);  // 64-bit imm allowed.
void c_mov_mem_imm (EA  target, int4 imm,   int size);  // imm is limited to 32-bit

// size always address_size, imm limited to 32-bit as it's an address inside 32 bit code+data+bss.
void c_mov_reg_imm_reloc (REG target, int4 imm, int size, RELOC_INFO  preloc);
void c_mov_mem_imm_reloc (EA  target, int4 imm, int size, RELOC_INFO  preloc);

/************************************************************************/

// assert: size_target > size-source

void c_movsx_reg_reg (REG target, int size_target,
                      REG source, int size_source);

void c_movsx_reg_mem (REG target, int size_target,
                      EA  source, int size_source);

void c_movzx_reg_reg (REG target, int size_target,
                      REG source, int size_source);

void c_movzx_reg_mem (REG target, int size_target,
                      EA  source, int size_source);

/************************************************************************/

// size == 1 is not allowed.
// size == 8 allowed for 64 bit

void c_imul_reg_reg     (REG target, REG source,           int size);
void c_imul_reg_mem     (REG target, EA  source,           int size);
void c_imul_reg_imm     (REG target, int imm,              int size);  // imm is limited to 32-bit.
void c_imul_reg_reg_imm (REG target, REG source, int imm,  int size);  // imm is limited to 32-bit.
void c_imul_reg_mem_imm (REG target, EA  source, int imm,  int size);  // imm is limited to 32-bit.

/************************************************************************/

// EDX:EAX = EAX * source  (4 byte)
// RDX:RAX = RAX * source  (8 byte) (64 bit only)

void c_umul_rax_reg (REG source, int size);
void c_umul_rax_mem (EA  source, int size);

/************************************************************************/

// EAX = EAX / source  (4 byte)
// RAX = RAX / source  (8 byte) (64 bit only)

// destroys RDX !!

void c_idiv_rax_reg (REG source, int size);
void c_idiv_rax_mem (EA  source, int size);

/************************************************************************/

// EDX = EAX % source  (4 byte)
// RDX = RAX % source  (8 byte) (64 bit only)

void c_imod_rax_reg (REG source, int size);
void c_imod_rax_mem (EA  source, int size);

/************************************************************************/

// EAX = EAX / source  (4 byte)
// RAX = RAX / source  (8 byte) (64 bit only)

// destroys RDX !!

void c_udiv_rax_reg (REG source, int size);
void c_udiv_rax_mem (EA  source, int size);

/************************************************************************/

// EDX = EAX % source  (4 byte)
// RDX = RAX % source  (8 byte) (64 bit only)

void c_umod_rax_reg (REG source, int size);
void c_umod_rax_mem (EA  source, int size);

/************************************************************************/

// multiplies by powers of 2
// imm between 1..31 (or 1..63 for 64-bit).

void c_shl_reg_imm (REG target, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
void c_shl_reg_CL (REG target, int size);

// imm between 1..31 (or 1..63 for 64-bit).
void c_shl_mem_imm (EA target, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
void c_shl_mem_CL (EA target, int size);

/************************************************************************/

// signed divide by powers of 2 (preserves the sign bit)
// sar is not the same as idiv for negative values
// imm between 1..31 (or 1..63 for 64-bit).

void c_sar_reg_imm (REG target, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
void c_sar_reg_CL (REG target, int size);

// imm between 1..31 (or 1..63 for 64-bit).
// sar is not the same as idiv for negative values
void c_sar_mem_imm (EA target, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
void c_sar_mem_CL (EA target, int size);

/************************************************************************/

// unsigned divide by powers of 2
// imm between 0..31 (or 0..63 for 64-bit).

// imm between 1..31 (or 1..63 for 64-bit).
void c_shr_reg_imm (REG target, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
void c_shr_reg_CL (REG target, int size);

// imm between 1..31 (or 1..63 for 64-bit).
void c_shr_mem_imm (EA target, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
void c_shr_mem_CL (EA target, int size);

/************************************************************************/

// multiply 2 registers by powers of two

// shifts left by CL bits or IM value.
// registers = high,low

// CL between 1..31 (or 1..63 for 64-bit).
// size cannot be 1 !
void c_shld_reg_reg_CL (REG target, REG source, int size);
void c_shld_reg_reg_imm (REG target, REG source, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !
void c_shld_mem_reg_CL (EA target, REG source, int size);
void c_shld_mem_reg_imm (EA target, REG source, int imm, int size);

/***
  EDX:EAX  << cl

  if (cl >= 32)
  {
    mov edx,eax
    xor eax,eax
  }

  shld edx,eax,cl  ; modifies edx;   cl between 0 and 31; 32-bit mode only
  shl  eax,cl
***/

/************************************************************************/

// divides 2 registers by powers of two
// registers = low,high

// CL between 1..31 (or 1..63 for 64-bit).
// size cannot be 1 !

void c_shrd_reg_reg_CL (REG target, REG source, int size);
void c_shrd_reg_reg_imm (REG target, REG source, int imm, int size);

// CL between 0..31 (or 0..63 for 64-bit).
// size cannot be 1 !
void c_shrd_mem_reg_CL (EA target, REG source, int size);
void c_shrd_mem_reg_imm (EA target, REG source, int imm, int size);

/****
   EDX:EAX  >> cl  (unsigned)

   if (cl >= 32)
   {
     mov eax,edx    ; zero-extend
     xor edx,edx
   }

   shrd eax,edx,cl  ; modifies edx;   cl between 0 and 31 ; 32-bit mode only
   shr  edx,cl
****/

/****
   EDX:EAX  >> cl  (signed)

   if (cl >= 32)
   {
     mov eax,edx
     sar edx,31  ; copy sign bit to all bits
   }

   shrd eax,edx,cl  ; modifies edx;   cl between 0 and 31 ; 32-bit mode only
   sar  edx,cl
****/

/************************************************************************/

// rotate left through carry
// multiply by 2 and add carry

void c_rcl_reg (REG target, int size);
void c_rcl_mem (EA target, int size);

/************************************************************************/

// inverse Carry Flag
void c_cmc ();

void c_lock_prefix ();
void c_pause ();

/************************************************************************/

/* UNUSED
// load random number in register
// carry flag set if successful

void c_rdrand (REG target, int size);
*/

/************************************************************************/

// for 32bit only

void c_lahf ();   // flags -> AH (not supported in 64-bit !!)
void c_sahf ();   // AH -> flags (not supported in 64-bit !!)

void c_mov_al_ah ();
void c_and_ah_al ();

/************************************************************************/

// FLD Load floating-point value

void c_fld      (EA source, int size);  // size is 4(float) or 8(double).
void c_fld_imem (EA source, int size);  // size is 2(int2), 4(int4) or 8(int8).

void c_fld_zero ();
void c_fld_one ();

/************************************************************************/

// FSTP   Store floating-point value and pop

void c_fstp  (EA target, int size);  // size is 4(float) or 8(double).
void c_fistp (EA target, int size);  // (with rounding from status word) size is 2(int2), 4(int4) or 8(int8).

// SSE3 !!
void c_fisttp (EA target, int size); // (with truncation)  size is 2(int2), 4(int4) or 8(int8).

/************************************************************************/

// void c_finit ();     // reset 87 floating point unit
// void c_fnclex ();    // clear flags of x87 status word

void c_fldcw (EA source);   // 16-bit operand (value 0x037F)
void c_fstcw (EA target);   // 16-bit operand

/************************************************************************/

// push top float register on stack and pop
// size is 4(float) or 8(double).

void c_push_fltp (int size);

/************************************************************************/

// FADD Add float

void c_fadd_fmem (EA source, int size);  // size is 4(float) or 8(double).
void c_fadd_imem (EA source, int size);  // size is 2(int2) or 4(int4).
void c_faddp ();

/************************************************************************/

// FSUB Sub float

void c_fsub_fmem (EA source, int size);   // size is 4(float) or 8(double).
void c_fsub_imem (EA source, int size);   // size is 2(int2) or 4(int4).
void c_fsubp ();

void c_fsubr_fmem (EA source, int size);  // size is 4(float) or 8(double).
void c_fsubr_imem (EA source, int size);  // size is 2(int2) or 4(int4).
void c_fsubrp ();

/************************************************************************/

// FMUL float

void c_fmul_fmem (EA source, int size);  // size is 4(float) or 8(double).
void c_fmul_imem (EA source, int size);  // size is 2(int2) or 4(int4).
void c_fmulp ();

/************************************************************************/

// FDIV float

void c_fdiv_fmem (EA source, int size);   // size is 4(float) or 8(double).
void c_fdiv_imem (EA source, int size);   // size is 2(int2) or 4(int4).
void c_fdivp ();

void c_fdivr_fmem (EA source, int size);  // size is 4(float) or 8(double).
void c_fdivr_imem (EA source, int size);  // size is 2(int2) or 4(int4).
void c_fdivrp ();

/************************************************************************/

void c_fneg ();

/************************************************************************/

// destroys RAX !
// !! not supported in 64-bit mode !!
// attention: this instruction compares the memory operand with ST0, not the reverse !!!
void c_fcomp_fmem (EA source, int size);  // size is 4(float) or 8(double).

// destroys RAX in 32-bit mode.
// attention: this instruction compares ST0 with ST1, not the reverse !!!
void c_fcomp ();

/************************************************************************/

// this instruction compares ST0 with itself,
// sets the condition codes, and pops the stack.
void c_fpop ();

/************************************************************************/

// XMM opcodes (for 64-bit mode only)

void c_mov_xm_xm  (XM target, XM source,   int size);  // dest = X0 to X15   size = 4 or 8
void c_mov_xm_mem (XM target, EA source,  int size);  // dest = X0 to X15   size = 4 or 8
void c_mov_mem_xm (EA target, XM source,  int size);  // size = 4 or 8

void c_mov_xm_reg (XM target, REG source,  int size);  // XM = 0 to 15   size = 4 or 8
void c_mov_reg_xm (REG target, XM source,  int size);  // XM = 0 to 15   size = 4 or 8

// convert between float and double
void c_cnv_xm_xm (XM target, XM source, int target_size, int source_size);

// load float from integer
void c_load_xm_size_ireg_size (XM target,  REG source, int target_size, int source_size);   // int4/8 to float/double
void c_load_xm_size_imem_size (XM target,  EA  source, int target_size, int source_size);   // int4/8 to float/double

// trunc+store float as integer
void c_store_ireg_size_xm_size (REG target, XM source,  int target_size, int source_size);   // float/double to int4/8 (truncates)
void c_store_ireg_size_memf_size (REG target, EA source, int target_size, int source_size); // float/double to int4/8


void c_add_xm_xm  (XM target, XM source, int size);  // XM = 0 to 15   size = 4 or 8
void c_add_xm_mem (XM target, EA source,  int size);

void c_sub_xm_xm  (XM target, XM source, int size);  // XM = 0 to 15   size = 4 or 8
void c_sub_xm_mem (XM target, EA source,  int size);

void c_mul_xm_xm (XM target, XM source, int size);  // XM = 0 to 15   size = 4 or 8
void c_mul_xm_mem (XM target, EA source,  int size);

// only 16 / 22 cycles
void c_div_xm_xm (XM target, XM source, int size);  // XM = 0 to 15   size = 4 or 8
void c_div_xm_mem (XM target, EA source,  int size);

void c_sqr_xm_xm (XM target, XM source, int size);  // XM = 0 to 15   size = 4 or 8
void c_sqr_xm_mem (XM target, EA source,  int size);

/************************************************************************/

// ! the result should be tested using the unsigned condition suffixes (a,ae,b,be)
void c_cmp_xm_xm (XM target, XM source, int size);  // XM = 0 to 15   size = 4 or 8
void c_cmp_xm_mem (XM target, EA source,  int size);

/*
test:

signals a SIMD floating-point invalid operation exception (#I) if a source operand is an SNaN !
some NANs for testing displayed as uint4 :
snan 7fa00000
 inf 7f800000
-inf ff800000
nan0 7fc00000
nan1 7fc00001
nan2 7fc00002
 0/0 ffc00000
qNaN is generated by regular built-in (software or hardware) arithmetic operations with weird values
sNaN is never generated by built-in operations, it can only be explicitly added by programmers
- compare NAN causes exceptions
- (snan + 1.0) causes exception FE_INVALID, but (qnan + 1.0) does not

*/

/************************************************************************/

void c_code (byte b);

/************************************************************************/

void c_verify_ESP_correction ();

/************************************************************************/

void reset_ESP_correction ();

/************************************************************************/

void add_ESP_correction (int offset);

/************************************************************************/

void c_ESP_correction_ON (bool on);

bool is_ESP_correction_ON ();

/************************************************************************/

int ESP_correction_value ();

/************************************************************************/
