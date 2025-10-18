
// pcodes.h : P-Code Instructions


/*

the virtual machine has the following arithmetic stacks :

- int_stack   : contains bool(1 byte), int(4 byte), uint(4 byte) or long(8 byte) values.
- float_stack : contains float(4 byte) or double(8 byte) values.
- addr_stack  : contains addresses (4 or 8 bytes, depending on 32- or 64-bit version)

*/



enum PCODE (uint2)
{

  // --------------
  // 1. memory move
  // --------------

  // push constant value on int_stack         (   -->  <int> )

  P_CTE_BOOL,  //  <int1>   ; push constant uint1      on int_stack as 1 byte
  P_CTE_4,     //  <int4>   ; push constant int4/uint4 on int_stack as 4 bytes
  P_CTE_8,     //  <int8>   ; push constant int8       on int_stack as 8 bytes

  // push constant value on float_stack       (   -->  <float> )

  P_CTE_FLT4,  //  <float4> ; push constant float4     on float_stack as 4 bytes
  P_CTE_FLT8,  //  <float8> ; push constant float8     on float_stack as 8 bytes

  // push null value on addr_stack             (   -->  <addr> )

  P_CTE_NULL,  //           ; push constant 0          on addr_stack


  // load address of code, constant, global, local   (  -->  <addr> )

  P_FUNC_LABEL,   // <func_label_4>          ; each function must start with a func_label
  P_LOAD_CODE,    // <func_label_4>          ; load addr of func_label on addr_stack
  P_LOAD_DLL,     // <dll_func_id4>          ; load addr of dll_func_nr on addr_stack
  P_LOAD_SYSCALL, // <syscall_nr4>           ; pseudo load syscall number on addr_stack, for later P_CALL

  P_LOAD_CONST,   // <pool_id8>              ; load addr on addr_stack
  P_LOAD_GLOBAL,  // <global_offset_8>       ; load addr on addr_stack
  P_LOAD_LOCAL,   // <local_offset_4>        ; load addr on addr_stack


  // load item from memory address   (  <addr>  -->  <int> )

  P_VALUE_BOOL,   // pop addr_stack, load bool1,      push on int_stack as 1 byte
  P_VALUE_I1,     // pop addr_stack, load int1,       push on int_stack as 4 bytes sign-extended
  P_VALUE_I2,     // pop addr_stack, load int2,       push on int_stack as 4 bytes sign-extended
  P_VALUE_U1,     // pop addr_stack, load uint1,      push on int_stack as 4 bytes zero-extended
  P_VALUE_U2,     // pop addr_stack, load uint2,      push on int_stack as 4 bytes zero-extended
  P_VALUE_4,      // pop addr_stack, load int4/uint4, push on int_stack as 4 bytes
  P_VALUE_8,      // pop addr_stack, load int8,       push on int_stack as 8 bytes


  // load item from memory address   (  <addr>  -->  <float> )

  P_VALUE_FLT4,   // pop addr_stack, load float,  push on float_stack as 4 bytes
  P_VALUE_FLT8,   // pop addr_stack, load double, push on float_stack as 8 bytes


  // load item from memory address   (  <addr>  -->  <addr>  )

  P_VALUE_ADDR,   //  pop addr_stack, load address, push on addr_stack


  // load item from memory address   (  <addr>  <top_addr>  -->  <addr>  <top_addr> )

  P_VALUE_ADDR1,   //  take addr, load address, put back <addr>


  // store value to memory address     (  <addr> -->  /  )
  //                                   (  <int>       /  )

  P_STORE_BOOL,  // pop int_stack,   pop addr_stack, store 1 byte (used for bool)
  P_STORE_1,     // pop int_stack,   pop addr_stack, store 1 byte (used for int1, uint1)
  P_STORE_2,     // pop int_stack,   pop addr_stack, store 2 bytes (used for int2, uint2)
  P_STORE_4,     // pop int_stack,   pop addr_stack, store 4 bytes (used for int4, uint4)
  P_STORE_8,     // pop int_stack,   pop addr_stack, store 8 bytes (using for int8)


  // store value to memory address     (  <addr> -->  /  )
  //                                   (  <float>     /  )

  P_STORE_FLT4,  // pop float_stack, pop addr_stack, store 4 bytes (used for float4)
  P_STORE_FLT8,  // pop float_stack, pop addr_stack, store 8 bytes (used for float8)


  // store value to memory address     (  <target_addr> <source_addr> -->  /  )

  P_STORE_ADDR,  // pop source_addr, pop target_addr, store 4 or 8 addr bytes


  // store block        (  <target_addr>   <source_addr>   <size_uint4>  -->   )

  P_COPY_BLOCK,         // pop <size_uint4>, pop <source_addr>, pop <target_addr>
  P_ORDERED_COPY_BLOCK, // same as above for overlapping areas (copy high-to-low or low-to-high)


  // multi copy        (  <target_addr>   <repeat4>   <source_addr>   -->   <target_addr>  )

  P_MULTI_COPY,         // <size4>   <near_label_4> <near_label_4>
                        // pop <source_addr>, pop <repeat4>, pop <target_addr>
                        // while (repeat) {memcpy (target, source, size); target+=size; repeat--;}


  // ------------
  // 2. branching
  // ------------

  //    (  -->  )       (near labels numbering starts again at 0 at each next function)

  P_NEAR_LABEL,     // <near_label_4>   ; (each goto/btrue/bfalse/jump/switch target must have a near label)


  //    (  -->  )

  P_GOTO,           // <near_label_4>


  //    (  <bool>  -->   )

  P_TSTBOOL,

    // pop bool1 from int_stack and test it.
    // note: P_TSTBOOL is ALWAYS followed immediately either by P_BTRUE or P_BFALSE
    // intel:  test al,al


  //   (  <bool>  <bool>  -->   )

  P_CMP_BOOL,      // <mask>  ; compare two bool   on int_stack     - 1 byte unsigned


  //   (  <int4_a>  <int4_b>  -->   )

  P_CMP_S4,      // <mask>  ; compare two int4   on int_stack     - signed


  //   (  <uint4_a>  <uint4_b>  -->   )

  P_CMP_U4,      // <mask>  ; compare two uint4  on int_stack     - unsigned


  //   (  <int8_a>  <int8_b>  -->   )

  P_CMP_S8,      // <mask>  ; compare two int8   on int_stack     - signed


  //   (  <float_a>  <float_b>  -->   )

  P_CMP_FLT4,    // <mask>  ; compare two float  on float_stack
  P_CMP_FLT8,    // <mask>  ; compare two double on float_stack


  //   (  <addr_a>  <addr_b>  -->   )

  P_CMP_ADDR,    // <mask>  ; compare two addr   on addr_stack    - unsigned

   // note: mask is 1< 2= 3<= 4> 5!= 6>=
   // note: CMP_xx is ALWAYS followed immediately either by P_BTRUE, P_BFALSE or P_SETBOOL.


  // all registers must be stored in temporaries before a conditional branch (operator "? :")
  //   (   -->   )

  P_BTRUE,       // <near_label4>  ; branch if compare true
                 // intel: jcc L

  P_BFALSE,      // <near_label4>  ; branch if compare false
                 // intel: jcc L


  //   (    -->  <int1>  )

  P_SETBOOL,     // push bool1 on int_stack with boolean result of test
                 // intel: Scc al


  //   ( bool  -->   bool  )    (if branch)
  //   ( bool  -->         )    (if not branch)

  P_CAND,        // <near_label4>  ; branch if false

  P_COR,         // <near_label4>  ; branch if true


  //  (  <int4>   -->   <int4>  )        ;  comparison for switch statement

  P_SWITCH_CMP_S4,  // <cmp_int4> <mask>  <label4>
                    // ; compare <int4> with <cmp_int4> and branch if true

  //  (  <uint4>   -->   <uint4>  )      ;  comparison for switch statement

  P_SWITCH_CMP_U4,  // <cmp_uint4> <mask>  <label4>


  //  (  <int8>   -->   <int8>  )        ;  comparison for switch statement

  P_SWITCH_CMP_8,   // <cmp_int8> <mask>  <label4>


  //  (  <uint4>   -->  / )        ;  jump table for switch statement

  P_JUMP_4,  // <pool_id8>   pool constant contains a list of <label_nr>
             //              that will be converted into addresses later on.
    // jump to <pool_cte>[<uint4*address_size>]

  //  (  <bool>   -->   )

  P_DROP_BOOL,


  //  (  <int4/uint4>   -->   )

  P_DROP_4,


  //  (  <int8>   -->   )

  P_DROP_8,


  //  (  <float>   -->   )

  P_DROP_FLT_4,


  //  (  <double>   -->   )

  P_DROP_FLT_8,


  //  (  <addr>   -->   )

  P_DROP_ADDR,


  //  (  <addr>   -->  <addr>  <addr> )

  P_DUP_ADDR,


  // ----------------
  // 3. function call
  // ----------------


  // FOR INTEL

  //   (  -->   )

  P_SYSCALL_EXTRA_STACK_SPACE,     // <int4> : for 64-bit operating system calls.
                    // nb bytes to reserve on stack for shadow space and alignment.
  

  //   (  bool  -->   )

  P_PUSH_BOOL,      // pop int_stack,   push 4 or 8 bytes on function stack (depending on version)


  //   (  int  -->   )

  P_PUSH_4,      // pop int_stack,   push 4 or 8 bytes on function stack (depending on version)
  P_PUSH_8,      // pop int_stack,   push      8 bytes on function stack


  //   (  float  -->   )

  P_PUSH_FLT4,   // pop float_stack, push 4 or 8 bytes on function stack (depending on version)
  P_PUSH_FLT8,   // pop float_stack, push      8 bytes on function stack (depending on version)


  //   (  addr  -->   )

  P_PUSH_ADDR,   // pop addr_stack,  push 4 or 8 bytes on function stack (depending on version)
                 //
                 // intel:  push   0FFh
                 //         push   40B138h

  // END INTEL

  // FOR ANDROID
  
  P_HINT_PARAM,   // <typ> <nr>  :  typ 0 = Xreg, 1 = Freg   gives hint to use register >= hint
                  // cancel at next hint or at any P_STORE_PARAM_xx
                  
  // ( bool -->  bool )

  P_STORE_PARAM_BOOL,  // <typ> <nr>  :  typ 0 = Xreg, 1 = Freg, 2 = Onstack[SP]  /  nr: register nr or stack offset


  // ( int -->  int )
                  
  P_STORE_PARAM_INT4,  // <typ> <nr>
  P_STORE_PARAM_INT8,  // <typ> <nr>


  //   (  float  -->  float )

  P_STORE_PARAM_FLT4,  // <typ> <nr>
  P_STORE_PARAM_FLT8,  // <typ> <nr>


  //   (  addr  -->   addr)

  P_STORE_PARAM_ADDR,  // <typ> <nr>

  // END ANDROID
  

  //   (  addr  -->   )

  P_CALL_INTEL,  // <shadow space> <nb_86_entries_pushed> <is_os_call> ; push IP+n on stack, pop addr_stack, branch to address
                 //
                 // intel:  call 0040194C (direct)
                 // intel:  call eax      (indirect)
                 //
                 // causes a runtime error if branch address is null
                 //
                 // note: all xx_stacks (=registers) are EMPTY when calling a function


  // when returning from the function call, the return value will be
  // in top xx_stack position (always EAX / ST(0))

  // code that indicates that the called function pushed a value on xx_stack.
  // no effect for asm except indicating that eax / st(0) is allocated.

  //   (   -->   )

  P_CALL_ARM,
  

  //   (   -->   )

  P_RETVALUE_VOID,  // function returned nothing


  //   (   -->  <uint1> )

  P_RETVALUE_BOOL,  // function returned a 1-byte bool on int_stack


  //   (   -->  <int4/uint4> )

  P_RETVALUE_4,     // function returned an int4/uint4 value on int_stack


  //   (   -->  <int8> )

  P_RETVALUE_8,     // function returned an int8 value  on int_stack


  //   (   -->  <float> )

  P_RETVALUE_FLT4,  // function returned a float4 value on float_stack
  P_RETVALUE_FLT8,  // function returned a float8 value on float_stack


  //   (   -->  <addr> )

  P_RETVALUE_ADDR,  // function returned an addr value on addr_stack


  // -------------------------
  // 4. function prolog/epilog
  // -------------------------

  //   (   -->   )

  P_ENTER,   // <size4>  <free4>  <block4> <flag_1X_is_thread_entry_point_or_2X_callback>
             // ; size of frame to allocate,
             // ; free space for spilling registers,
             // ; allocate new space in blocks.
             //
             // intel:   push  ebp
             //          mov   ebp,esp
             //          sub   esp,<size4>
             //  + probe stack on each 4K page using (test ebp[-ofs],eax)
             //     if size larger than 4K page


  P_SAVE_XREG,  // used for android to save register onto stack frame when entering function
  P_SAVE_FREG,  // <reg_nr>, <offset>, <actual size>
                // reg_nr : 0 to 7
                // offset : add stack frame size to offset and use X29 (FP) as base
                // actual_size : 1, 2, 4 or 8 (important for float 4 or 8)
                // maybe combine several pcodes to use stp (store pair of registers)
  

  //   (   -->   )

  P_LEAVE,   //
             // intel:  leave      (equivalent to:   mov ESP, EBP  +  pop EBP)


  //   (   -->   )

  P_RETURN_VOID,  // indicates a function with no return value
                  // asm: main() and thread functions returning void should clean EAX


  //   ( <uint1>   -->   )

  P_RETURN_BOOL,  // return value is bool on int_stack


  //   (   <int4/uint4> -->  )

  P_RETURN_4,     // return value is int4/uint4 on int_stack


  //   (   <int8> -->  )

  P_RETURN_8,     // return value is int8 value on int_stack


  //   (   <float> -->  )

  P_RETURN_FLT4,  // return value is float4 value on float_stack
  P_RETURN_FLT8,  // return value is float8 value on float_stack


  //   (  <addr>  -->  )

  P_RETURN_ADDR,  // return value is addr value on addr_stack


  //   (   -->   )

  P_RET,     // <size4>      ; size of parameters to pop from stack
             //
             // pop address, branch to address, add size to ESP
             // intel:  ret  <size>


  // -------------
  // 5. arithmetic
  // -------------

  // bool   (1-byte values on int_stack)
  // ----

  P_NOT_BOOL,    //  ( bool       --> bool )
  P_OR_BOOL,     //  ( bool  bool --> bool )
  P_AND_BOOL,    //  ( bool  bool --> bool )
  P_XOR_BOOL,    //  ( bool  bool --> bool )

  // int4   (signed value on int_stack)
  // ----

  P_NEG4,    //  ( int4         --> int4 )
  P_NOT4,
  P_ADD4,    //  ( int4  int4   --> int4 )
  P_SUB4,
  P_SMUL4,
  P_SDIV4,
  P_SMOD4,
  P_BITAND4,
  P_BITOR4,
  P_BITXOR4,
  P_ASL4,
  P_ASR4,

  // uint4
  // -----

  P_UMUL4,    //  ( uint4  uint4   --> uint4 )
  P_UDIV4,
  P_UMOD4,
  P_SHL4,
  P_SHR4,

  // int8
  //-----

  P_NEG8,     //  ( int8         --> int8 )
  P_NOT8,
  P_ADD8,     //  ( int8  int8   --> int8 )
  P_SUB8,
  P_SMUL8,
  P_SDIV8,
  P_SMOD8,
  P_BITAND8,
  P_BITOR8,
  P_BITXOR8,
  P_ASL8,     // <near_label4>
  P_ASR8,     // <near_label4>

  // float
  // -----

  P_NEG_FLT4,  //  ( float4          -->  float4 )
  P_ADD_FLT4,  //  ( float4  float4  -->  float4 )
  P_SUB_FLT4,
  P_MUL_FLT4,
  P_DIV_FLT4,

  // double
  // ------

  P_NEG_FLT8,  //  ( float8          -->  float8 )
  P_ADD_FLT8,  //  ( float8  float8  -->  float8 )
  P_SUB_FLT8,
  P_MUL_FLT8,
  P_DIV_FLT8,


  // -------------------------
  // 6. pre/post dec/increment
  // -------------------------

  //  (  <addr>  -->  /  )

  P_INC1,      // pop addr_stack, increment int1 at address
  P_INC2,      // pop addr_stack, increment int2 at address
  P_INC4,      // pop addr_stack, increment int4 at address
  P_INC8,      // pop addr_stack, increment int8 at address

  P_DEC1,      // pop addr_stack, decrement int1 at address
  P_DEC2,      // pop addr_stack, decrement int2 at address
  P_DEC4,      // pop addr_stack, decrement int4 at address
  P_DEC8,      // pop addr_stack, decrement int8 at address


  // --------------------------------
  // 7. combined arithmetic and store
  // --------------------------------

  // bool   (bool on int_stack)
  // ----

  P_AND_BOOL_TO,  // &=    (  addr  bool  -->   )
  P_OR_BOOL_TO,   // |=
  P_XOR_BOOL_TO,  // ^=

  // int4   (signed value on int_stack)
  // ----

  P_ADD1_TO,  // +=     (  addr  int1  -->   )
  P_SUB1_TO,  // -=
  P_AND1_TO,  // &=
  P_OR1_TO,   // |=
  P_XOR1_TO,  // ^=
  P_ASL1_TO,  // <<=  (signed)
  P_ASR1_TO,  // >>=  (signed)

  P_ADD2_TO,  // +=     (  addr  int2  -->   )
  P_SUB2_TO,  // -=
  P_AND2_TO,  // &=
  P_OR2_TO,   // |=
  P_XOR2_TO,  // ^=
  P_ASL2_TO,  // <<=  (signed)
  P_ASR2_TO,  // >>=  (signed)

  P_ADD4_TO,  // +=     (  addr  int4  -->   )
  P_SUB4_TO,  // -=
  P_AND4_TO,  // &=
  P_OR4_TO,   // |=
  P_XOR4_TO,  // ^=
  P_ASL4_TO,  // <<=  (signed)
  P_ASR4_TO,  // >>=  (signed)

  // uint4 (unsigned)
  // -----

  P_SHL1_TO,  // <<=  (unsigned)     (  addr  uint1  -->   )
  P_SHR1_TO,  // >>=  (unsigned)

  P_SHL2_TO,  // <<=  (unsigned)     (  addr  uint2  -->   )
  P_SHR2_TO,  // >>=  (unsigned)

  P_SHL4_TO,  // <<=  (unsigned)     (  addr  uint4  -->   )
  P_SHR4_TO,  // >>=  (unsigned)

  // int8
  // ----

  P_ADD8_TO,  // +=       (  addr  int8  -->   )
  P_SUB8_TO,  // -=
  P_AND8_TO,  // &=
  P_OR8_TO,   // |=
  P_XOR8_TO,  // ^=
  P_ASL8_TO,  // <<=  (signed)     // <near_label4>
  P_ASR8_TO,  // >>=  (signed)     // <near_label4>


  // += -= for unsafe ptr

  // ( addr  int4  -->   ) 

  P_UNSAFE_ADD_TO,   // <size4>   ;  address[addr] += signed int4 * size4;
  P_UNSAFE_SUB_TO,   // <size4>   ;  address[addr] -= signed int4 * size4;


  //  (  <addr>  -->  /  )

  P_INC_ADDR,        // <size4>   ; pop addr_stack, increment addr at address by size
  P_DEC_ADDR,        // <size4>   ; pop addr_stack, decrement addr at address by size


  // ( addr  -->  addr1 ) 

  P_INC_VALUE_ADDR,  // <size4>   ;  example: ++p   (inc [addr] by size, then take addr1 at [addr])
  P_DEC_VALUE_ADDR,  // <size4>   ;  example: --p

  P_VALUE_ADDR_INC,  // <size4>   ;  example: p++   (take addr1 at [addr], then inc [addr] by size)
  P_VALUE_ADDR_DEC,  // <size4>   ;  example: p--


  // -------------
  // 8. conversion
  // -------------

  P_CONV_BOOL_INT,   //  (  bool1  -->   int4       )
  P_CONV_INT_BOOL,   //  (  int4   -->   bool1      )   extracts lower byte

  P_CONV_INT_LONG,   //  (  int4   -->   int8       )   sign-extend
  P_CONV_UINT_LONG,  //  (  uint4  -->   int8       )   unsigned-extend

  P_CONV_LONG_INT,   //  (  int8   -->   int4/uint4 )  truncate

  P_CONV_INT_FLT4,   //  (  int4   -->   float4  )
  P_CONV_INT_FLT8,   //  (  int4   -->   float8  )

  P_CONV_UINT_FLT4,  //  (  uint4  -->   float4  )
  P_CONV_UINT_FLT8,  //  (  uint4  -->   float8  )

  P_CONV_LONG_FLT4,  //  (  int8   -->   float4  )
  P_CONV_LONG_FLT8,  //  (  int8   -->   float8  )

  P_CONV_FLT4_UINT,  //  (  float4 -->   uint4 )
  P_CONV_FLT4_INT,   //  (  float4 -->   int4  )
  P_CONV_FLT4_LONG,  //  (  float4 -->   int8  )

  P_CONV_FLT8_UINT,  //  (  float8 -->   uint4 )
  P_CONV_FLT8_INT,   //  (  float8 -->   int4  )
  P_CONV_FLT8_LONG,  //  (  float8 -->   int8  )

  P_CONV_FLT4_FLT8,  //  (  float4 -->   float8 )
  P_CONV_FLT8_FLT4,  //  (  float8 -->   float4 )

  P_EXTEND,          //  (  int4  -->  uint4  )  0=extend uint1 to uint4, 1=extend uint2 to uint4, 2=extend int1 to int4, 3=extend int2 to int4.

  
  // --------
  // 9. names
  // --------

  // ----------------------------------------------------------------------

  // ( addr  -->  addr  uint4  )

  P_GET_CONSTR0,  // <offset4>

    // load constraint from memory at [addr_stack[top] + offset]
    // and push it on the int_stack;
    // this p-code is used to retrieve the length/discriminant of an object of an open type.

  // ----------------------------------------------------------------------

  // ( addr  addr2  -->  addr  addr2  uint4  )

  P_GET_CONSTR1,  // <offset4>

    // load constraint from memory at [addr_stack[top-1] + offset]
    // and push it on the int_stack;
    // this p-code is used to retrieve the length/discriminant of an object of an open type
    // when there are 2 addresses on addr_stack (in assignment)

  // ----------------------------------------------------------------------

  // There are two alternatives for indexes :

  // a) constant index, constant length  --> compile-time check, use P_ADD_OFFSET to increase address.
  // b) otherwise, use P_CHECK_INDEX + P_ADD_INDEX :

  //    (  index4  length  -->  index4  )

  P_CHECK_INDEX,   //  <near_label_nr4>

     // if index4 >= length -> error       (uint4 comparison)
     //
     //   intel:
     //     cmp x,length
     //     jae error


  //    (  addr  index4  -->  addr2  )

  P_ADD_INDEX,    // <size4>

     //   addr2 = addr + (index4 * size)
     //
     //   ; intel: imul  eax,eax,138h
     //          + effective address using eax (which will be zero-extended)
     //
     //   note for 64-bit addresses : the index4 is always < 2GB.


  //   (  addr  index4  -->  addr2  )

  P_ADD_PTR_OFFSET,   // <size4>

     //   addr2 = addr + (index4 * size)
     //
     //   note: this is used for adding an offset from an unsafe pointer.
     //   note for 64-bit addresses : the index4 is signed, so it must be sign-extended to int8 !


  //   (  addr  index4  -->  addr2  )

  P_SUB_PTR_OFFSET,   // <size4>

     //   addr2 = addr - (index4 * size)
     //
     //   note: this is used for subtracting an offset from an unsafe pointer.
     //   note for 64-bit addresses : the index4 is signed, so it must be sign-extended to int8 !

  // ----------------------------------------------------------------------

  // There are several alternatives for slices :
  //
  //  a) cte ofs, cte len, cte length --> compile-time check, use P_ADD_OFFSET to increase address.
  //  b)          cte len, cte length --> use P_CHECK_SLICE_0 + P_ADD_INDEX.
  //  c)          cte len             --> use P_CHECK_SLICE_1 + P_ADD_INDEX.
  //  d) otherwise use general case   --> use P_CHECK_SLICE_2 + P_ADD_INDEX.

  // case b : pcode to be used in case of constant len and constant length.
  // --------

  // (  ofs  -->  ofs )

  P_CHECK_SLICE_0,   //  <(length-len)_uint4>  <near_label4>

      // at compilation : check :  if len > length --> error    (uint4 comparison)
      //
      //  if ofs > length - len --> error    (uint4 comparison)

  // case c : pcode to be used in case of constant len and runtime length.
  // --------

  // (  ofs  length  -->  ofs )

  P_CHECK_SLICE_1,    //  <cte_len4>  <near_label4>

      //  if len > length       --> error    (uint4 comparison)
      //  if ofs > length - len --> error    (uint4 comparison)

  // case d : pcode to be used in case of runtime len (len must be stored in a local temporary variable).
  // --------

  //  (  ofs  length  -->  ofs  )

  P_CHECK_SLICE_2,    //  <len_local_addr4>  <near_label4>

      // len is evaluated and stored in a local variable before the pcode.
      //
      //  if len > length       --> error    (uint4 comparison)
      //  if ofs > length - len --> error    (uint4 comparison)

  // ----------------------------------------------------------------------

  //  (   addr --> addr2 )

  P_ADD_OFFSET,   // <offset4>

      //  addr2 = addr + offset4


  //   (   addr  top_addr --> addr2  top_addr )

  P_ADD_OFFSET1,  // <offset4>

      //  addr2 = addr + offset4

  // ----------------------------------------------------------------------

  // (  value4  -->    )

  P_CHECK_SAME_1,   //  <cte>   <near_label4>

    // used to check matching of array length or discriminant value with a constant
    //
    // if value != cte -> error


  //  (  value4  value4  -->  value4  )

  P_CHECK_SAME_2,   //    <near_label4>

    // used to check matching of two non-constant array lengths or discriminant values
    // leaves one copy on int_stack for further size computation (for assignment)
    //
    // if value != value -> error


  //  (   length  -->  size    )

  P_ARRAY_SIZE,     //  <size_element>

    // compute size of array (unsigned)
    //
    // size = length * <size_element>


  //    (  addr1  addr2  -->  value4  )

  P_SUB_PTRS,

     //   compute value4 = addr1 - addr2
     //   note: this is used for subtracting two unsafe pointers


  //  (  <pointer_addr>  -->  <heap_object_addr>  )

  P_DEREF,      // <type4>  <local_addr4>   <near_label4>

    // convert pointer address to heap object address.
    // lock and increment tombstone counter
    // check that <type> matches tombstone type -> error if check fails
    // saves address of tombstone (thus pointer value) in <local_addr4>
    //
    // lock inc addr->counter
    // mem[local_addr4] = addr;
    // if (type != addr->type) --> error
    // heap_object_addr = addr->data;


  P_UNDEREF,   //  <local_addr4>

    // lock and decrement tombstone counter at <local_addr>
    // lock dec (*local_addr)->counter


  // for machine-code : force simple value in any CPU register.
  // this might be used before a P_UNDEREF code to make sure
  // the value is read from a heap object before it gets released.
  // also: this forces evaluation of P_VALUE_xx pcodes before inc/dec
  // because they might be lazily flushed into registers otherwise.

  //   ( b --> b )

  P_FORCE_BOOL,  // for m-code : force b in a register

  //   ( int4 --> int4 )

  P_FORCE_4,     // for m-code : force int4 value in a register

  //   ( int8 --> int8 )

  P_FORCE_8,     // for m-code : force int8 value in a register

  //   ( float4 --> float4 )

  P_FORCE_FLT4,    // for m-code : force float4 value in a register

  //   ( float8 --> float8 )

  P_FORCE_FLT8,    // for m-code : force float8 value in a register

  //   ( addr --> addr )

  P_FORCE_ADDR,    // for m-code : force addr value in a register


  // for machine-code : force value in default CPU register.
  // sync pcodes are used for operator "?:" and for switch statement.

  //   ( b --> b )

  P_SYNC_BOOL,   // for m-code : force b in register AL

  //   ( int4 --> int4 )

  P_SYNC_4,     // for m-code : force int4 value in register EAX

  //   ( int8 --> int8 )

  P_SYNC_8,     // for m-code : force int8 value in registers EDX:EAX (or RAX for 64-bit)

  //   ( float4 --> float4 )

  P_SYNC_FLT4,    // for m-code : force float4 value on top float stack

  //   ( float8 --> float8 )

  P_SYNC_FLT8,    // for m-code : force float8 value on top float stack

  //   ( addr --> addr )

  P_SYNC_ADDR,    // for m-code : force addr value in register EAX

  //   ( addr int4 --> addr int4 )

  P_SYNC_ADDR_INT4,   // for m-code : force addr value in register ESI and int4 in register EAX


  // (for m-code only : remove specified nb of entries from xx_stacks) (used in ?: statement before goto)

  P_SYNC_STACKS,  // <int_stack> <float_stack> <addr_stack>


  //   (  --> int4 )

  P_SHADOW_4,     // for m-code : when starting a switch case, we assert that an int4 is in lowest CPU register (EAX).

  //   (  --> int8 )

  P_SHADOW_8,     // for m-code : when starting a switch case, we assert that an int8 is in lowest CPU register (RAX or EDX:EAX).


  // --------------
  // 10. Aggregates
  // --------------

  // load address of aggregate on addr_stack, then store all fields :
  //
  //  ( addr_target   value  -->  addr_target )
  //
  // pop xx_stack, store value at addr_target + offset4

  P_STORE_FIELD_BOOL,  // <offset4>    (used for bool)
  P_STORE_FIELD_1,     // <offset4>    (used for int1, uint1)
  P_STORE_FIELD_2,     // <offset4>
  P_STORE_FIELD_4,     // <offset4>
  P_STORE_FIELD_8,     // <offset4>
  P_STORE_FIELD_FLT4,  // <offset4>
  P_STORE_FIELD_FLT8,  // <offset4>
  P_STORE_FIELD_ADDR,  // <offset4>


  // pop address_stack (address_source), store block at address_target + offset
  //
  // ( addr_target   addr_source  -->  addr_target )

  P_STORE_FIELD_BLOCK,    // <offset4>  <size4>


  // after assigning last aggregate field, use P_DROP_ADDR
  // to drop address from addr_stack.


  // --------
  // 11. heap
  // --------


  //  (  size_uint4 --> heap_addr )

  P_MALLOC,      //  <bool_fill_zeroes>   ;  allocate heap block ;  causes error if out of memory


  //  (  heap_addr  -->  /   )   

  P_FREE,        //  <bool_never_null>   ; free heap block  ; causes error if bad ptr
                 //                      ; bool: 1 = must never be null, 0 = no effect if null


  //   (  heap_addr -->  pointer_addr )

  P_ALLOC_TOMB,    // <type4>             ;  allocate tombstone,  causes error if out of memory

    // requires a previous call to P_MALLOC
    // allocate tombstone block,
    // fill its <type4> & <heap_addr>, return its <pointer_addr>


  //  ( pointer_addr -->  / )     

  P_FREE_TOMB,   // <type4>               ; free tombstone + heap object ; causes error if bad ptr
                 //                       ; null address value has no effect.

  // --------------
  // 12. statements
  // --------------


  //  (    --->     )

  P_INIT_THREADS,    // special pcode used by android only at initialization to set up the global variable pthread_attr_t


  //  (  code_addr  -->   int4  )   stack frames of threads need to be 16-byte aligned.

  P_RUN_VOID_PARAM,     // <near_label_nr>


  //  (  code_addr  param_int4  -->   int4  )

  P_RUN_INT4_PARAM,     // <near_label_nr>


  //  (  code_addr  param_addr -->   int4  )

  P_RUN_ADDR_PARAM,     // <near_label_nr>


  //  (  addr   size4    --->   /   )

  P_CLEAR,


  //  (  /  /   )

  P_ABORT,


  //  (  /  /   )

  P_ASSERT,       // <label_nr>    ; define this label below the function for a failed assertion.


  //  (    --->     )

  P_SLEEP_CTE,    // <uint4>  ; sleep <uint4> milliseconds


  //  (  int4  --->   /   )

  P_SLEEP_INT4,   // <near_label_nr1>  ; sleep <int4> seconds (negative value mean no sleep)


  //  (  float4  --->   /   )

  P_SLEEP_FLT4,   // <near_label_nr1>  ; sleep <float4> seconds (negative value mean no sleep)


  //  (    --->     )

  P_CODE,    // <byte>  ; machine code


  // -------------------
  // 13. source location
  // -------------------

  //  (  /  --->   /   )

  P_LOCATION,   // <unit4>  <source_line4>


  // ----------------------------------------------------------------------
};
