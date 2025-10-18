
// init86.h

from std use tracing;
use ../common, ../pool, ../goptions, ../error;
use pe, a86, as86;

//======================================================================================

#if 0

"c:\CC\Comp\test26.exe" Hello "a b" World

1) edx = nb quotes even or odd

fquote = 0;           xor edx,edx
p = address;          mov ebx,eax   (address = eax;  p = ebx)   rax = start of string
                      jmp L10
                L9:
while (*p)
{
  if quote            cmp [ebx],34  (byte)34
    toggle fquote     jne L11
  p++;                not edx;
                L11:
                      inc ebx
                L10:
                      cmp [ebx],0  (byte)0
                      jne L9
}
                                                           ebx = end of string
2) allocate space on stack to copy string
    target = esp-1;
                     lea edi,[esp]-1

    esp -= (ebx - eax + 15) & -2*address_size;
                     mov ecx,ebx
                     sub ecx,eax
                     add ecx,(2*address_size-1)
                     and ecx,-2*address_size
                     sub esp,ecx

3)
 argc = 0;                xor ecx,ecx

 while (p >= address) L0: cmp ebx,eax
 {                        jb  L12
   if (*p <= 32)          cmp [ebx],32  (byte)
   {                      ja  L1
     p--;                 dec ebx
     continue;            jmp L0
   }
                      L1:
   argc++;                      inc ecx
   argv -= (2 * address_size);  push 0
   argv.len = 0;                push 0

///  target = p;            mov edi,ebx      (edi is target)

   while (p >= address)  L3: cmp ebx,eax
   {                         jb  L12

     if (*p <= 32 &&        cmp edx,0
         fquote==0)         jne L5
       break;
                            cmp [ebx],32  (byte)
                            jbe L0

                        L5:
     if (*p == quote)       cmp [ebx],34 (byte)
     {                      jne  L6
       fquote = !fquote;    not  edx
       if (fquote == 0)     cmp  edx,0
                            jne  L7
         if (p[1] != 34)    cmp  [ebx]1,34  (byte)  was double-quote
           goto L7          jne  L7
     }
                        L6:
                            push eax
     *target = *p;          mov al,[ebx]  (byte)
                            mov [edi],al  (byte)
                            pop eax

     argv.addr = target;    mov [esp],edi
     argv.len++;            inc address_size[esp]   (dword)

     target--;              dec edi

                        L7:
     p--;                  dec ebx
                           jmp L3:
   }
 }
                        L12:
                            mov esi,esp   ; table
                            and esp,-16   ; align esp

                            push ecx   ; argc
                            push esi   ; argv
                            call main
#endif

//======================================================================================

// "c:\CC\Comp\test26.exe" Hello "a b" World

// loads unquoted argline and argv table on stack
// realigns stack, then pushes argc, argv

void push_argc_argv ()
{
  EA  ea;
  int lab[10], i;

  if (g_tracing)
  {
    trace ("\n");
    trace ("code for push_argc_argv\n");
  }

  // call GetCommandLineA
  align_stack ();
  alloc_shadow_space ();
  call_kernel ("GetCommandLineA", /*nb_arguments=*/ 0);
  free_shadow_space_and_dealign_stack ();

  clear lab;
  for (i=0; i<10; i++)
    lab[i] = i+1;

  // edx = nb quotes even or odd

  // since we parse the arguments backwards, we need to know if quotes are even or odd

  c_mov_reg_imm (RDX, 0, 4);
  // fquote = 0;           xor edx,edx

  c_mov_reg_reg (RBX, RAX, address_size);
  // p = address;          mov ebx,eax   (address = eax;  p = ebx)

  c_jump_relative (lab[8]);
  //                      jmp L8

  // ------------------------------------------------
  exe_declare_near_label (lab[9]);
  //                 L9:

  // while (*p)
  // {

  clear ea;
  ea.base = RBX;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_cmp_mem_imm (ea, 34, 1);
  // if quote            cmp [ebx],34  (byte)     is next char quote ?

  c_jcond (CMP_NOT_EQUAL, false, lab[4]);
  //                     jne L4                   no -> lab 4

  c_not_reg (RDX, 4);
  //                     not edx;  toggle fquote

  // ------------------------------------------------
  exe_declare_near_label (lab[4]);
  //                 L4:

  c_inc_reg (RBX, address_size);
  //  p++;                    inc ebx

  // ------------------------------------------------
  exe_declare_near_label (lab[8]);
  //                 L8:

  ea.base = RBX;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_cmp_mem_imm (ea, 0, 1);
  // if zero             cmp [ebx],0  (byte)        is next char zero ?

  c_jcond (CMP_NOT_EQUAL, false, lab[9]);
  //                     jne L9                     no -> lab 9

  // 2) allocate space on stack to copy string
  //  target = esp-1;
  //                 lea edi,[esp]-1
  ea.base = RSP;   ea.index = NONE;  ea.scale = 1;   ea.offset = -1;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_lea_reg_mem (RDI, ea, /*size=*/address_size);

  //  esp -= (ebx - eax + 15) & -address_size;
  //                   mov ecx,ebx
  c_mov_reg_reg (RCX, RBX, address_size);

  //                   sub ecx,eax
  c_sub_reg_reg (RCX, RAX, address_size);

#if 1
  //                   add ecx,#15
  c_add_reg_imm (RCX, 15, address_size);

  //                   and ecx,#-16
  c_and_reg_imm (RCX, -16, address_size);
#else
  //                   add ecx,(address_size-1)
  c_add_reg_imm (RCX, address_size-1, address_size);

  //                   and ecx,-address_size
  c_and_reg_imm (RCX, -address_size, address_size);
#endif

  //                   sub esp,ecx
  c_sub_reg_reg (RSP, RCX, address_size);

  // 3)

  c_mov_reg_imm (RCX, 0, 4);
  // argc = 0;                xor ecx,ecx

  // ------------------------------------------------
  exe_declare_near_label (lab[0]);
  //                 L0:

  c_cmp_reg_reg (RBX, RAX, address_size);
  // while (p >= address) cmp ebx,eax

  c_jcond (CMP_SMALLER, false, lab[2]);
  // {                        jb  L2                -> DONE (ECX = argc, ESP argv)

  // ** remove trailing spaces **

  ea.base = RBX;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_cmp_mem_imm (ea, 32, 1);
  // if (*p <= 32)          cmp [ebx],32  (byte)

  c_jcond (CMP_LARGER, false, lab[1]);
  // {                      ja  L1

  c_dec_reg (RBX, address_size);
  //   p--;                 dec ebx

  c_jump_relative (lab[0]);
  //   continue;            jmp L0
  // }


  // ------------------------------------------------
  exe_declare_near_label (lab[1]);
  //                    L1:

  c_inc_reg (RCX, 4);
  // argc++;                      inc ecx      ( inc argc )

  c_push_imm (0);
  // argv -= (2 * address_size);  push 0       table has 2 entries per string (address and length)

  c_push_imm (0);
  // argv.len = 0;                push 0

  // ------------------------------------------------
  exe_declare_near_label (lab[3]);
  //                    L3:

  c_cmp_reg_reg (RBX, RAX, address_size);
  // while (p >= address)   cmp ebx,eax

  c_jcond (CMP_SMALLER, false, lab[2]);
  // {                         jb  L2

  c_cmp_reg_imm (RDX, 0, 4);
  //   if (*p <= 32 &&        cmp edx,0

  c_jcond (CMP_NOT_EQUAL, false, lab[5]);
  //       fquote==0)         jne L5
  //     break;

  ea.base = RBX;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_cmp_mem_imm (ea, 32, 1);
  //                          cmp [ebx],32  (byte)

  c_jcond (CMP_SMALLER_OR_EQUAL, false, lab[0]);
  //                          jbe L0

  // ------------------------------------------------
  exe_declare_near_label (lab[5]);
  //                        L5:

  ea.base = RBX;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_cmp_mem_imm (ea, 34, 1);
  //   if (*p == quote)       cmp [ebx],34 (byte)

  c_jcond (CMP_NOT_EQUAL, false, lab[6]);
  //   {                      jne  L6

  c_not_reg (RDX, 4);
  // fquote = !fquote;    not  edx

  c_cmp_reg_imm (RDX, 0, 4);
  //  if (fquote == 0)     cmp  edx,0

  c_jcond (CMP_NOT_EQUAL, false, lab[7]);
  //                          jne  L7

  ea.base = RBX;   ea.index = NONE;  ea.scale = 1;   ea.offset = 1;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_cmp_mem_imm (ea, 34, 1);
  // if (p[1] != 34)    cmp  [ebx]1,34  (byte)  was double-quote

  c_jcond (CMP_NOT_EQUAL, false, lab[7]);
  //        goto L7          jne  L7
  //   }

  // ------------------------------------------------
  exe_declare_near_label (lab[6]);
  //                      L6:

  c_push_reg (RAX, address_size);
  //                          push eax

  ea.base = RBX;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_mov_reg_mem (RAX, ea, 1);
  //   *target = *p;          mov al,[ebx]  (byte)

  ea.base = RDI;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RAX, 1);
  //                        mov [edi],al  (byte)

  c_pop_reg (RAX, address_size);
  //                        pop eax

  ea.base = RSP;   ea.index = NONE;  ea.scale = 1;   ea.offset = 0;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RDI, address_size);
  //  argv.addr = target;    mov [esp],edi

  ea.base = RSP;   ea.index = NONE;  ea.scale = 1;   ea.offset = address_size;  ea.reloc.kind = RELOC_NONE;  ea.reloc.nr = 0;
  c_inc_mem (ea, 4);
  // argv.len++;            inc address_size[esp]   (dword)

  c_dec_reg (RDI, address_size);
  //   target--;              dec edi

  // ------------------------------------------------
  exe_declare_near_label (lab[7]);
  //                      L7:

  c_dec_reg (RBX, address_size);
  //   p--;                  dec ebx

  c_jump_relative (lab[3]);
  //                         jmp L3:

 //  }
 //}

  // ------------------------------------------------
  exe_declare_near_label (lab[2]);
  //                        L2:

  c_mov_reg_reg (RSI, RSP, address_size);
  //                          mov esi,esp   ; table

  c_and_reg_imm (RSP, -16, address_size); // align ESP
  //                          and esp,-16   ; align esp

  c_push_reg (RCX, address_size);
  //                        push ecx

  c_push_reg (RSI, address_size);
  //                        push esi

  if (g_tracing)
  {
    trace ("end code for push_argc_argv\n");
    trace ("\n");
  }
}

//======================================================================================

void gen_code_for_mult_int8 ()
{
  EA ea;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for mult_int8\n");
  }

exe_add_func_label (label_multiply_int8);

  clear ea;
  ea.base = RBP;
  ea.index = NONE;
  ea.scale = 1;
  ea.offset = 0;
  ea.reloc.kind = RELOC_NONE;
  ea.reloc.nr   = 0;

  c_push_reg    (RBP,      address_size);  // push ebp
  c_mov_reg_reg (RBP, RSP, address_size);  // mov  ebp,esp
  c_sub_reg_imm (RSP, 8,   address_size);  // sub  esp,8

  // multiply first high term
  ea.offset = 8;
  c_mov_reg_mem (RAX, ea, 4);
  ea.offset = 20;
  c_umul_rax_mem (ea, 4);
  c_mov_reg_reg (RCX, RAX, 4);  // save in ECX

  // multiply second high term
  ea.offset = 12;
  c_mov_reg_mem (RAX, ea, 4);
  ea.offset = 16;
  c_umul_rax_mem (ea, 4);
  c_add_reg_reg (RCX, RAX, 4);  // add in ECX

  // multiply low term
  ea.offset = 8;
  c_mov_reg_mem (RAX, ea, 4);
  ea.offset = 16;
  c_umul_rax_mem (ea, 4);
  c_add_reg_reg (RDX, RCX, 4);  // add ECX to high term

  c_leave ();
  c_ret (16);  // two int8 arguments
}

//======================================================================================

void gen_code_for_div_int8 ()
{
  EA ea;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for div_int8\n");
  }

exe_add_func_label (label_divide_int8);

  clear ea;
  ea.base   = RBP;
  ea.index  = NONE;
  ea.scale  = 1;
  ea.offset = 0;
  ea.reloc.kind = RELOC_NONE;
  ea.reloc.nr   = 0;

  c_push_reg    (RBP,      address_size);  // push ebp
  c_mov_reg_reg (RBP, RSP, address_size);  // mov  ebp,esp
  c_sub_reg_imm (RSP, 24,  address_size);  // sub  esp,24


  // load B into ESI:EDI
  ea.offset = 16;               // low B -> ESI
  c_mov_reg_mem (RSI, ea, 4);
  ea.offset = 20;               // high B -> EDI
  c_mov_reg_mem (RDI, ea, 4);


  // if B zero -> int0
  c_mov_reg_reg (RAX, RSI, 4);
  c_or_reg_reg (RAX, RDI, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  5);   // label 5

  c_int (0);   // interrupt 0 (division by zero)

  exe_declare_near_label (5);   // label 5

  // compute sign of quotient and remainder
  ea.offset = 12;               // high A
  c_mov_reg_mem (RAX, ea, 4);
  ea.offset = -12;              // save sign of remainder in temp at -12
  c_mov_mem_reg (ea, RAX, 4);

  c_xor_reg_reg (RAX, RDI, 4);  // xor with high B
  ea.offset = -16;              // save sign of quotient in temp at -16
  c_mov_mem_reg (ea, RAX, 4);

  // if A negative, negate it
  ea.offset = 12;               // high A
  c_cmp_mem_imm (ea, 0, 4);
  c_jcond (CMP_LARGER_OR_EQUAL, /*signed=*/ true,  1);   // label 1

  // not
  ea.offset = 12;               // high A
  c_not_mem (ea, 4);
  ea.offset = 8;                // low A
  c_not_mem (ea, 4);

  // add 1
  ea.offset = 8;                // low A
  c_add_mem_imm (ea, 1, 4);
  ea.offset = 12;               // high A
  c_adc_mem_imm (ea, 0, 4);

  exe_declare_near_label (1);   // label 1


  // if B negative, negate it
  c_cmp_reg_imm (RDI, 0, 4);
  c_jcond (CMP_LARGER_OR_EQUAL, /*signed=*/ true,  2);   // label 2

  // not
  c_not_reg (RSI, 4);
  c_not_reg (RDI, 4);

  // add 1
  c_add_reg_imm (RSI, 1, 4);
  c_adc_reg_imm (RDI, 0, 4);

  exe_declare_near_label (2);   // label 2



  // if EDI == 0 && 12[EBP] == 0 -> use 32-bit division

  c_test_reg_reg (RDI, RDI, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  8);   // label 8

  ea.offset = 12;               // high A
  c_cmp_mem_imm (ea, 0, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  8);   // label 8

  // use 32-bit unsigned division
  c_mov_reg_imm (RDX, 0, 4);
  ea.offset = 8;               // low A
  c_mov_reg_mem (RAX, ea, 4);
  c_udiv_rax_reg (RSI, 4);     // quotient in EAX, remainder in EDX.

  c_mov_reg_imm (RDX, 0, 4);
  c_jump_relative (9);

  exe_declare_near_label (8);   // label 8


  // clear EDX, EAX
  c_mov_reg_imm (RDX, 0, 4);
  c_mov_reg_imm (RAX, 0, 4);

  // loop 64 times
  c_mov_reg_imm (RCX, 64, 4);
  exe_declare_near_label (3);   // label 3

  // roll a bit into EDX:EAX
  ea.offset = 8;                // low A
  c_rcl_mem (ea, 4);
  ea.offset = 12;               // high A
  c_rcl_mem (ea, 4);
  c_rcl_reg (RAX, 4);
  c_rcl_reg (RDX, 4);

  // compare EDX:EAX with EDI:ESI
  c_cmp_reg_reg (RDX, RDI, 4);
  c_jcond (CMP_SMALLER, /*signed=*/ false,  6);   // label 6  (jumps with CF=1)
  c_jcond (CMP_LARGER,  /*signed=*/ false,  7);   // label 7

  c_cmp_reg_reg (RAX, RSI, 4);
  c_jcond (CMP_SMALLER, /*signed=*/ false,  6);   // label 6  (jumps with CF=1)

  exe_declare_near_label (7);   // label 7

  // EDX:EAX >= EDI:ESI  -> subtract EDI:ESI from EDX:EAX

  c_sub_reg_reg (RAX, RSI, 4);
  c_sbb_reg_reg (RDX, RDI, 4);   // CF=0 after this

  exe_declare_near_label (6);   // label 6

  c_cmc ();   // inverse CF flag

  ea.offset = -8;               // low Q
  c_rcl_mem (ea, 4);
  ea.offset = -4;               // high Q
  c_rcl_mem (ea, 4);

  c_dec_reg (RCX, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  3);   // label 3

  // load quotient into EDX:EAX
  ea.offset = -8;               // low Q
  c_mov_reg_mem (RAX, ea, 4);
  ea.offset = -4;               // high Q
  c_mov_reg_mem (RDX, ea, 4);

  exe_declare_near_label (9);   // label 9

  // if Q-sign negative, negate it
  ea.offset = -16;              // Q-sign
  c_cmp_mem_imm (ea, 0, 4);
  c_jcond (CMP_LARGER_OR_EQUAL, /*signed=*/ true,  4);   // label 4

  // not
  c_not_reg (RAX, 4);
  c_not_reg (RDX, 4);

  // add 1
  c_add_reg_imm (RAX, 1, 4);
  c_adc_reg_imm (RDX, 0, 4);

  exe_declare_near_label (4);   // label 4


  c_leave ();
  c_ret (16);  // two int8 arguments
}

//======================================================================================

void gen_code_for_mod_int8 ()
{
  EA ea;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for mod_int8\n");
  }

exe_add_func_label (label_modulo_int8);

  clear ea;
  ea.base   = RBP;
  ea.index  = NONE;
  ea.scale  = 1;
  ea.offset = 0;
  ea.reloc.kind = RELOC_NONE;
  ea.reloc.nr   = 0;

  c_push_reg    (RBP,      address_size);  // push ebp
  c_mov_reg_reg (RBP, RSP, address_size);  // mov  ebp,esp
  c_sub_reg_imm (RSP, 24,  address_size);  // sub  esp,24


  // load B into ESI:EDI
  ea.offset = 16;               // low B -> ESI
  c_mov_reg_mem (RSI, ea, 4);
  ea.offset = 20;               // high B -> EDI
  c_mov_reg_mem (RDI, ea, 4);


  // if B zero -> int0
  c_mov_reg_reg (RAX, RSI, 4);
  c_or_reg_reg (RAX, RDI, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  5);   // label 5

  c_int (0);   // interrupt 0 (division by zero)

  exe_declare_near_label (5);   // label 5

  // compute sign of quotient and remainder
  ea.offset = 12;               // high A
  c_mov_reg_mem (RAX, ea, 4);
  ea.offset = -12;              // save sign of remainder in temp at -12
  c_mov_mem_reg (ea, RAX, 4);

  c_xor_reg_reg (RAX, RDI, 4);  // xor with high B
  ea.offset = -16;              // save sign of quotient in temp at -16
  c_mov_mem_reg (ea, RAX, 4);

  // if A negative, negate it
  ea.offset = 12;               // high A
  c_cmp_mem_imm (ea, 0, 4);
  c_jcond (CMP_LARGER_OR_EQUAL, /*signed=*/ true,  1);   // label 1

  // not
  ea.offset = 12;               // high A
  c_not_mem (ea, 4);
  ea.offset = 8;                // low A
  c_not_mem (ea, 4);

  // add 1
  ea.offset = 8;                // low A
  c_add_mem_imm (ea, 1, 4);
  ea.offset = 12;               // high A
  c_adc_mem_imm (ea, 0, 4);

  exe_declare_near_label (1);   // label 1


  // if B negative, negate it
  c_cmp_reg_imm (RDI, 0, 4);
  c_jcond (CMP_LARGER_OR_EQUAL, /*signed=*/ true,  2);   // label 2

  // not
  c_not_reg (RSI, 4);
  c_not_reg (RDI, 4);

  // add 1
  c_add_reg_imm (RSI, 1, 4);
  c_adc_reg_imm (RDI, 0, 4);

  exe_declare_near_label (2);   // label 2



  // if EDI == 0 && 12[EBP] == 0 -> use 32-bit division

  c_test_reg_reg (RDI, RDI, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  8);   // label 8

  ea.offset = 12;               // high A
  c_cmp_mem_imm (ea, 0, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  8);   // label 8

  // use 32-bit unsigned division
  c_mov_reg_imm (RDX, 0, 4);
  ea.offset = 8;               // low A
  c_mov_reg_mem (RAX, ea, 4);
  c_udiv_rax_reg (RSI, 4);     // quotient in EAX, remainder in EDX.

  c_mov_reg_reg (RAX, RDX, 4);
  c_mov_reg_imm (RDX, 0, 4);
  c_jump_relative (9);

  exe_declare_near_label (8);   // label 8



  // clear EDX, EAX
  c_mov_reg_imm (RDX, 0, 4);
  c_mov_reg_imm (RAX, 0, 4);


  // loop 64 times
  c_mov_reg_imm (RCX, 64, 4);
  exe_declare_near_label (3);   // label 3

  // roll a bit into EDX:EAX
  ea.offset = 8;                // low A
  c_rcl_mem (ea, 4);
  ea.offset = 12;               // high A
  c_rcl_mem (ea, 4);
  c_rcl_reg (RAX, 4);
  c_rcl_reg (RDX, 4);

  // compare EDX:EAX with EDI:ESI
  c_cmp_reg_reg (RDX, RDI, 4);
  c_jcond (CMP_SMALLER, /*signed=*/ false,  6);   // label 6  (jumps with CF=1)
  c_jcond (CMP_LARGER,  /*signed=*/ false,  7);   // label 7

  c_cmp_reg_reg (RAX, RSI, 4);
  c_jcond (CMP_SMALLER, /*signed=*/ false,  6);   // label 6  (jumps with CF=1)

  exe_declare_near_label (7);   // label 7

  // EDX:EAX >= EDI:ESI  -> subtract EDI:ESI from EDX:EAX

  c_sub_reg_reg (RAX, RSI, 4);
  c_sbb_reg_reg (RDX, RDI, 4);   // CF=0 after this

  exe_declare_near_label (6);   // label 6

  c_cmc ();   // inverse CF flag

  ea.offset = -8;               // low Q
  c_rcl_mem (ea, 4);
  ea.offset = -4;               // high Q
  c_rcl_mem (ea, 4);

  c_dec_reg (RCX, 4);
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  3);   // label 3

  exe_declare_near_label (9);   // label 9

  // if Rest-sign negative, negate it
  ea.offset = -12;              // Q-sign
  c_cmp_mem_imm (ea, 0, 4);
  c_jcond (CMP_LARGER_OR_EQUAL, /*signed=*/ true,  4);   // label 4

  // not
  c_not_reg (RAX, 4);
  c_not_reg (RDX, 4);

  // add 1
  c_add_reg_imm (RAX, 1, 4);
  c_adc_reg_imm (RDX, 0, 4);

  exe_declare_near_label (4);   // label 4


  c_leave ();
  c_ret (16);  // two int8 arguments
}

//======================================================================================

void set_global (out EA ea, int ofs)
{
  clear ea;
  ea.base = NONE;   ea.index = NONE;  ea.scale = 1;  ea.offset = ofs;
  ea.reloc.kind = RELOC_GLOBAL;  ea.reloc.nr = 0;
}

//======================================================================================

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

  // ACQUIRE_LOCK (uses EAX)
  // ------------

exe_declare_near_label (lab+1);   // spin_loop (L1):

  c_pause ();   // gives hint to processor that improves performance of spin-wait loops.

  set_global (out ea, 8);
  c_cmp_mem_imm (ea, 0, /*size=*/4);

  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  /*label*/lab+1);  // as long as it's not zero, loop

  // otherwise try again to aquire the lock

exe_add_func_label (label_getlock_tombstone);     // <-- entry point

  c_mov_reg_imm (RAX, 1, 4);       // acquisition value

  set_global (out ea, 8);
  c_xchg_reg_mem (RAX, ea, /*size=*/4);    // ds:lockf
  c_test_reg_reg (RAX, RAX, 4);

  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  /*label*/lab+1);  // spin loop
  c_ret (0);

#if 0
to release lock:
  c_mov_reg_imm (RAX, 0, 4);    // free lock value
  set_global (ea, 8);
  c_xchg_reg_mem (RAX, ea, 4);  // xchg global[8],eax
  c_ret (0);
#endif
}

//======================================================================================

package TOMB
  const int TOMBSTONE_BLOCK_SIZE = 4*1024;   // at least 4096 (PAGE SIZE) !!   POWER OF TWO !!
  const int NB_TB_BLOCKS_PER_ALLOCATION = 256;  // number of tombstone blocks to allocate at once to reduce fragmentation
  // see assertions below
  // 1 MB allocated at once, enough for 65536 entries.
end TOMB;

//======================================================================================

void gen_code_for_tombstone_malloc ()
{
  int lab;
  EA  ea;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for tombstone_malloc\n");
  }

  if (TOMBSTONE_BLOCK_SIZE  < 4096)   // at least PAGE SIZE
    fatal_compiler_error0 ("aligntb1");
  if (NB_TB_BLOCKS_PER_ALLOCATION * TOMBSTONE_BLOCK_SIZE < 64*1024)
    fatal_compiler_error0 ("aligntb2");


  // ADDRESS allocate_tombstone (ADDRESS allocated_block) ; offset 4  (push first)
  // 32-bit : passed on stack,  64-bit : passed in RAX.

  exe_add_func_label (label_allocate_tombstone);

lab = 0;   // new label range

  if (address_size == 8)
  {
    c_push_reg (RBP, address_size);         // for M16 alignment
    c_mov_reg_reg (R12, RAX, address_size); // save address in R12
  }

  reset_ESP_correction ();    // assume RSP aligned here


  // allocation of a new page happens each 4095 allocations at program start,
  // but it does not occur later when the program has run a long time
  // and has enough tombstone entries.
  // -> use spin loop because allocation is fast.

  c_call_relative (label_getlock_tombstone, /*nb_arguments=*/ 0);

  set_global (out ea, 16);   // Tombstone_HEAD
  c_mov_reg_mem (RSI, ea, address_size);
  c_test_reg_reg (RSI, RSI, address_size);    // if (Tombstone_HEAD == null)
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  /*label*/lab+2);

  // tombstone list is empty, allocate a new chunk of tombstone blocks

  // init 64K virtual page, init with zeroes
  if (address_size == 4)
  {
    c_push_imm (4);                         // arg 4 : DWORD flProtect          // PAGE_READWRITE 0x04
    c_push_imm (0x3000);                    // arg 3 : DWORD flAllocationType   // MEM_COMMIT 0x1000 + MEM_RESERVE 0x2000
    c_push_imm (NB_TB_BLOCKS_PER_ALLOCATION * TOMBSTONE_BLOCK_SIZE);  // arg 2 : SIZE_T dwSize  // 64K (16 pages) (minimum granularity)
    c_push_imm (0);                         // arg 1 : LPVOID lpAddress,        // 0
    call_kernel ("VirtualAlloc", /*nb_arguments=*/ 4);
  }
  else
  {
    align_stack ();

    c_mov_reg_imm (R9,  4,                        4);
    c_mov_reg_imm (R8,  0x3000,                   4);
    c_mov_reg_imm (RDX, NB_TB_BLOCKS_PER_ALLOCATION * TOMBSTONE_BLOCK_SIZE, 4);    // allocate a zero-filled block of 64K
    c_mov_reg_imm (RCX, 0,                        4);

    alloc_shadow_space ();
    call_kernel ("VirtualAlloc", /*nb_arguments=*/ 0);
    free_shadow_space_and_dealign_stack ();
  }

  // if null -> L4
  c_test_reg_reg (RAX, RAX, address_size);
  c_jcond (CMP_EQUAL, /*signed=*/ false,  /*label*/lab+4);    // will return 0 to call and cause a crash


  // link all RAX blocks in head/tail links
  set_global (out ea, 16);
  c_mov_mem_reg (ea, RAX, address_size);    // store first block in head

  set_global (out ea, 24);
  c_mov_mem_reg (ea, RAX, address_size);    // store last block in tail
  c_add_mem_imm (ea, (NB_TB_BLOCKS_PER_ALLOCATION-1) * TOMBSTONE_BLOCK_SIZE, address_size);


  // we will loop on RDX to initialize all blocks
  c_mov_reg_imm (RDX, NB_TB_BLOCKS_PER_ALLOCATION, 4);

  exe_declare_near_label (lab+5);   // L5:

  // init ptr to next block
  ea.base = RAX;  ea.index = NONE; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RAX, /*size=*/address_size);   // first store own address
  c_add_mem_imm (ea, TOMBSTONE_BLOCK_SIZE, address_size);          // then add 64K to point on next block

  // initialize new free block RAX.
  // initialize a free list covering all entries : each entry pointing to the next;
  // the chain list is at offset 8 in each entry, starts at first unused entry, and null indicates end of list.

  c_mov_reg_imm (RCX, (TOMBSTONE_BLOCK_SIZE/16)-1, 4);   // 4095 entries
                 // free list head pointer + 4094 entries to fill, keep last ptr null

  // RDI = entry 8 (head of list)
  ea.base = RAX;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_lea_reg_mem (RDI, ea, /*size=*/address_size);      //  lea rdi,[rax]8

exe_declare_near_label (lab+1);   // L1

  // temporarily store free slot in itself
  ea.base = RDI;  ea.index = NONE; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RDI, address_size);   // mov dword [rdi],rdi

  // add #8 so it points to the tombstone entry
  c_add_mem_imm (ea, 8, address_size);     // add dword [rdi],8   ; point 8 bytes further

  // move RDI to next tombstone slot
  c_add_reg_imm (RDI, 16, address_size);    // add edi,16          ; advance 16 bytes

  c_dec_reg (RCX, 4);                       // dec ecx
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  lab+1); //  jne L1

  // end loop
  c_add_reg_imm (RAX, TOMBSTONE_BLOCK_SIZE, address_size);   // next 64K block
  c_dec_reg (RDX, 4);                         // dec edx
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ false,  /*label*/lab+5);

  // clear next ptr of last block
  c_sub_reg_imm (RAX, TOMBSTONE_BLOCK_SIZE, address_size);         // go back to last block

  ea.base = RAX;  ea.index = NONE; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_imm (ea, 0, /*size=*/address_size);   // set zero to next block

  set_global (out ea, 16);
  c_mov_reg_mem (RSI, ea, address_size);   // mov esi,global[16]    ; load head value

exe_declare_near_label (lab+2);   // L2:

  // here we suppose we have a tombstone block that has some free entries, at RSI.

  // load address of first free tombstone in RDX
  ea.base = RSI;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_reg_mem (RDX, ea, address_size); //  mov rdx,[esi]8

  // load address if second free tombstone (or null) in RBX
  ea.base = RDX;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_reg_mem (RBX, ea, address_size); //  mov rbx,[rdx]8

  // store RBX (adddress of second free tombstone or null) into free list head of tombstone block
  ea.base = RSI;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RBX, address_size);   // mov [esi]8,ebx

  // test if the tombstone block has any eny entries left after this
  c_test_reg_reg (RBX, RBX, address_size);  // test ebx,ebx
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  lab+3); //  jne L3   still entries left -> L3

  // tombstone block has no free slots anymore, we just allocated the last available entry.
  // so unlink block from tombstone block list head.

  // load RSI = second tombstone block with free entries, or null
  ea.base = RSI;  ea.index = NONE; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_reg_mem (RSI, ea, address_size);  // mov esi,[esi]

  // store RSI into head
  set_global (out ea, 16);
  c_mov_mem_reg (ea, RSI, address_size);  // mov [global]16,esi  ; store second block into head

  // test if null
  c_test_reg_reg (RSI, RSI, address_size);  //   test esi,esi
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  lab+3); //  jne L3

  // if null, set also tail
  set_global (out ea, 24);
  c_mov_mem_imm (ea, 0, address_size);   // mov [global]24,0    ; head is null -> clear also tail ptr

exe_declare_near_label (lab+3);   // L3:

  // release_lock
  c_mov_reg_imm (RAX, 0, 4);    // free lock value
  set_global (out ea, 8);
  c_xchg_reg_mem (RAX, ea, 4);    // xchg global[8],eax

  // here, the new tombstone 16-byte-entry is at EDX

  // save tombstone address in RAX for return value
  c_mov_reg_reg (RAX, RDX, address_size);   // mov eax,edx


  // store parameter (actual allocation address) at offset 8 in tombstone entry

  if (address_size == 4)
  {
    // load actual allocated block address (parameter)
    ea.base = RSP;  ea.index = NONE; ea.scale = 1;  ea.offset = 4;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
    c_mov_reg_mem (RCX, ea, address_size);   // mov ecx,[esp]4

    // store at offset 8 in tombstone
    ea.base = RAX;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
    c_mov_mem_reg (ea, RCX, address_size);  // mov [eax]8,ecx   ; ASSIGN POINTER FIRST !

    // new tombstone is in EAX
    c_ret (4);
  }
  else
  {
    // store actual allocated block address (parameter) at offset 8 in tombstone
    ea.base = RAX;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
    c_mov_mem_reg (ea, R12, address_size);  // mov [eax]8,r12

    c_pop_reg (RBP, address_size);   // for M16 alignment
    c_ret (0);    // new tombstone is in EAX
  }

  reset_ESP_correction ();    // assume RSP aligned here


exe_declare_near_label (lab+4);   // L4: (jumped-to after VirtualAlloc() failed)

  // release_lock
  c_mov_reg_imm (RAX, 0, 4);    // free lock value
  set_global (out ea, 8);
  c_xchg_reg_mem (RAX, ea, 4);    // xchg global[8],eax

  c_mov_reg_imm (RAX, 0, 4);   // new tombstone is in EAX

  if (address_size == 4)
  {
    c_ret (4);      // will crash at calling point so debug info is available
  }
  else
  {
    c_pop_reg (RBP, address_size);   // for M16 alignment
    c_ret (0);                       // will crash at calling point so debug info is available
  }

  reset_ESP_correction ();    // assume RSP aligned here

#if 0
to do after allocate_tombstone :
  mov [eax]4,typ       ; assign type to entry (makes it valid)
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

  exe_add_func_label (label_free_tombstone);

lab = 0;

  // free_tombstone (ADDRESS tb,    ; offset 8[esp] (push first)  tombstone address
  //                 int4 typ)      ; offset 4[esp] (push second) typ
  // 32-bit : passed on stack,  64-bit : passed in RAX=address, RBX=typ.

  if (address_size == 4)
  {
    clear ea;
    ea.base = RSP;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
    c_mov_reg_mem (RSI, ea, address_size);   // mov  esi,[esp]8   ; rsi = tombstone
  }
  else
  {
    c_push_reg (RBP, address_size);         // for M16 alignment
    c_mov_reg_reg (RSI, RAX, address_size);
  }

  reset_ESP_correction ();    // assume RSP aligned here

  c_test_reg_reg (RSI, RSI, address_size);  // test esi,esi
  c_jcond (CMP_EQUAL, /*signed=*/ true,  lab+4); //  jne L4 ; tombstone is null -> ret

  // to avoid that two free_tombstone calls occur at the same time !
  c_call_relative (label_getlock_tombstone, /*nb_arguments=*/ 0);

  // stores illegal value in type so further dereferencings will fail
  c_mov_reg_imm (RAX, -1, 4);   // mov  eax,-1

  clear ea;
  ea.base = RSI;  ea.index = NONE; ea.scale = 1;  ea.offset = 4;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_xchg_reg_mem (RAX, ea, 4);   // xchg eax,[esi]4     ; Store A (type) --> protects pdata which can't be read now

  // check type matches

  if (address_size == 4)
  {
    ea.base = RSP;  ea.index = NONE; ea.scale = 1;  ea.offset = 4;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
    c_cmp_reg_mem (RAX, ea, 4);   // cmp  eax,[esp]4     ; typ
  }
  else
  {
    c_cmp_reg_reg (RAX, RBX, 4);   // cmp  eax,ebx    ; typ
  }

  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  lab+5); //  jne L5 ; error_bad_type


  // check count is zero
  c_mov_reg_imm (RAX, 0, 4);  // mov  eax,0
  ea.base = RSI;  ea.index = NONE; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_xchg_reg_mem (RAX, ea, 4);  // xchg eax,[esi]    (implicit lock)  Store B (count)
  c_test_reg_reg (RAX, RAX, 4);  // test eax,eax
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  lab+5); //  jne  L5 ; error_in_use

  // compute RDI = actual memory block to be freed
  ea.base = RSI;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_reg_mem (RDI, ea, address_size);  // mov edi,8[esi]      ; save data block to be free'd (edi)

  // compute EBX = address of 64K block of this tombstone
  c_mov_reg_reg (RBX, RSI, address_size);       // mov ebx,esi     ; block_address = tb & -4096;
  c_and_reg_imm (RBX, -TOMBSTONE_BLOCK_SIZE, address_size); // and ebx,-65536   ; align at 65536

  // load RCX = head of free list
  ea.base = RBX;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_reg_mem (RCX, ea, address_size);  // mov ecx,[ebx]8  ; load free ptr in ecx

  // if tombstone block has already free entries, goto L3
  c_test_reg_reg (RCX, RCX, address_size);   // test ecx,ecx
  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  lab+3); //  jne  L3 ; block already has free entries (no need to chain it back)

  // tombstone block EBX is fully allocated, there were no free entries before

  // chain tombstone block EBX back to the end of the list
  ea.base = RBX;  ea.index = NONE; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_imm (ea, 0, address_size);   // mov dword ptr [ebx],0   ; clear .next field

  set_global (out ea, 16);
  c_mov_reg_mem (RAX, ea, address_size);  // mov eax,global[16]   ; head
  c_test_reg_reg (RAX, RAX, address_size);   // test eax,eax

  c_jcond (CMP_NOT_EQUAL, /*signed=*/ true,  lab+1);  // jne  L1 ; head not null

  // head was null, store tombstone block RBX in it
  set_global (out ea, 16);
  c_mov_mem_reg (ea, RBX, address_size);  // mov global[16],ebx    ; Tombstone_HEAD = block_address;
  c_jump_relative (lab+2);  // jmp L2

exe_declare_near_label (lab+1);   // L1:

  // load address of tail block (or null) in RAX
  set_global (out ea, 24);
  c_mov_reg_mem (RAX, ea, address_size);  // mov eax,global[24]   ; Tombstone_TAIL^.next = block_address;

  // last tombstone's block's next link is set to the new tombstone block RBX
  ea.base = RAX;  ea.index = NONE; ea.scale = 1;  ea.offset = 0;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RBX, address_size);  // mov [eax],ebx

exe_declare_near_label (lab+2);   // L2:

  // store new tombstone block RBX in tail
  set_global (out ea, 24);
  c_mov_mem_reg (ea, RBX, address_size); // mov global[24],ebx   ; Tombstone_TAIL = block_address;

exe_declare_near_label (lab+3);   // L3:

  // now, chain tb to free list of the tombstone block RBX
  ea.base = RBX;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_reg_mem (RAX, ea, address_size);   // mov eax,[ebx]8    ;  tb^.pdata = block_address^.free;

  ea.base = RSI;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RAX, address_size);   // mov [esi]8,eax

  ea.base = RBX;  ea.index = NONE; ea.scale = 1;  ea.offset = 8;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
  c_mov_mem_reg (ea, RSI, address_size);   // mov [ebx]8,esi    ; block_address^.free = tb;

  // release_lock
  c_mov_reg_imm (RAX, 0, 4);    // free lock value
  set_global (out ea, 8);
  c_xchg_reg_mem (RAX, ea, 4);    // xchg global[8],eax

   // call Windows API now we're no longer in the spinloop
  if (address_size == 4)
  {
    c_push_reg (RDI, 4);   // arg 3 : address of block to be freed

    c_push_imm (0);        // arg 2 : push flags

    set_global (out ea, 0);   // arg 1 : push process heap handle that was stored at global address zero.
    c_push_mem (ea, 4);

    call_kernel ("HeapFree", /*nb_arguments=*/ 3);
  }
  else
  {
    align_stack ();

    c_mov_reg_reg (R8, RDI, 8);   // arg 3 : address of block to be freed
    c_mov_reg_imm (RDX, 0, 8);    // arg 2

    set_global (out ea, 0);          // process heap handle that was stored at global address zero.
    c_mov_reg_mem (RCX, ea, 8);  // arg 1

    alloc_shadow_space ();
    call_kernel ("HeapFree", /*nb_arguments=*/ 0);
    free_shadow_space_and_dealign_stack ();
  }

exe_declare_near_label (lab+4);   // L4:    (we arrive here in case we try to free a null pointer)

  if (address_size == 4)
  {
    c_ret (8);
  }
  else
  {
    c_pop_reg (RBP, address_size);   // for M16 alignment
    c_ret (0);
  }

  reset_ESP_correction ();    // assume RSP aligned here

exe_declare_near_label (lab+5);   // L5: (error_bad_type:  error_in_use:)
  // release_lock
  c_mov_reg_imm (RAX, 0, 4);    // free lock value
  set_global (out ea, 8);
  c_xchg_reg_mem (RAX, ea, 4);    // xchg global[8],eax

  // this will allow the exception handler to unwind the stack normally to find the call address
  if (address_size == 4)
    c_push_reg    (RBP,      address_size);  // push ebp
  c_mov_reg_reg (RBP, RSP, address_size);  // mov  ebp,esp

  c_int (5);

  reset_ESP_correction ();    // assume RSP aligned here
}

//======================================================================================

void gen_code_for_free_possible_null ()
{
  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for free_possible_null\n");
  }

  if (address_size == 4)
  {
    int lab;
    EA  ea;

    lab = 0;

    // requires address to be freed in EAX
    exe_add_func_label (label_free_possible_null);

    clear ea;
    ea.base = RSP;  ea.index = NONE; ea.scale = 1;  ea.offset = 4;  ea.reloc.kind = RELOC_NONE; ea.reloc.nr = 0;
    c_mov_reg_mem (RAX, ea, address_size);  // mov ecx,[esp]4  ; load addr in eax

    c_test_reg_reg (RAX, RAX, address_size);   // test eax,eax
    c_jcond (CMP_EQUAL, /*signed=*/ false,  lab); //  je L1

    c_push_reg (RAX, 4);   // arg 3 : push address of block to be freed

    c_push_imm (0);        // arg 2 : push flags

    set_global (out ea, 0);
    c_push_mem (ea, 4);   // arg 1 : push process heap handle that was stored at global address zero.

    call_kernel ("HeapFree", /*nb_arguments=*/ 3);


    exe_declare_near_label (lab);

    c_ret (4);
  }
  else
  {
    int lab;
    EA  ea;

    lab = 0;

    exe_add_func_label (label_free_possible_null);
    c_push_reg (RBP, address_size);         // for M16 alignment

    reset_ESP_correction ();    // assume RSP aligned here

    c_test_reg_reg (RAX, RAX, address_size);   // test RAX,RAX
    c_jcond (CMP_EQUAL, /*signed=*/ false,  lab); //  je L1


    align_stack ();

    c_mov_reg_reg (R8, RAX, address_size);  // arg 3 IN R8

    c_mov_reg_imm (RDX, 0, 4);    // arg 2  (will be zero extended to 8 bytes)

    set_global (out ea, 0);          // process heap handle that was stored at global address zero.
    c_mov_reg_mem (RCX, ea, 8);  // arg 1

    alloc_shadow_space ();
    call_kernel ("HeapFree", /*nb_arguments=*/ 3);
    free_shadow_space_and_dealign_stack ();


    exe_declare_near_label (lab);

    c_pop_reg (RBP, address_size);   // for M16 alignment
    c_ret (0);
  }

  reset_ESP_correction ();    // assume RSP aligned here
}

//======================================================================================

void gen_code_for_check_stack_M16_alignment ()
{
  int lab;

  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for check_stack_M16_alignment\n");
  }

lab = 0;
  exe_add_func_label (label_check_stack_M16_alignment);

  c_sub_reg_imm (RSP, 8, 8);     // sub esp,#8

  c_test_reg_imm (RSP, 15, 4);   // test esp,#15

  c_jcond (CMP_EQUAL, /*signed=*/ false,  lab); //  je L1

  c_int (5);


exe_declare_near_label (lab);

  c_add_reg_imm (RSP, 8, 8);     // add esp,#8

  c_ret (0);
}

//======================================================================================

public
void generate_bootstrap_86_code (int  func_label_to_main,
                                 int  func_label_to_init_constants,
                                 bool function_main_has_parameter_array_of_string)
{
  // prelog function to call initialization routines

  if (g_tracing)
    trace ("Start of Code\n");

  reset_pool_backfills ();
  exe_reset_dll_move_chain ();
  exe_allocate_near_label_table (20);  // enough for prolog function argc/argv  (we must do this in advance before no P-code is generated for this prolog) !


  // store fp87 control word value.
  {
    POOL p = new_pool_constant (/*size=*/ 2, /*align=*/2);
    store_integer (p, 0, 0x037F + (3 << 10), 2);  // use extended 64-bit mantissa + trunc mode.
    pool_nr_87_control_word_trunc = serial_nr_of_pool_cte (p);
  }

  c_push_reg    (RBP,      address_size);  // push ebp
  c_mov_reg_reg (RBP, RSP, address_size);  // mov  ebp,esp
  reset_ESP_correction ();    // assume RSP aligned here

  if (address_size == 4)
    c_and_reg_imm (RSP, -16, address_size); // align RSP

  // call GetProcessHeap and store ID in global address 0.
  align_stack ();
  alloc_shadow_space ();
  call_kernel ("GetProcessHeap", /*nb_arguments=*/ 0);
  free_shadow_space_and_dealign_stack ();
  {
    EA ea;
    set_global (out ea, 0);
    c_mov_mem_reg (ea, RAX, address_size);
  }


  if (g_tracing)
    trace ("call to init global variables\n");
  c_call_relative (func_label_to_init_constants, /*nb_arguments=*/ 0);


  if (function_main_has_parameter_array_of_string)
    push_argc_argv ();                    // creates near labels 1..10


  if (g_tracing)
    trace ("call to main\n");
  c_call_relative (func_label_to_main, /*nb_arguments=*/ 0);


  // call ExitProcess with EAX

  if (g_tracing)
    trace ("call to exit process\n");

  if (address_size == 4)
  {
    c_push_reg (RAX, address_size);
  }
  else
  {
    align_stack ();
    c_mov_reg_reg (RCX, RAX, 8);   // RAX into RCX
  }
  alloc_shadow_space ();
  call_kernel ("ExitProcess", 1);
  free_shadow_space_and_dealign_stack ();

  pe.relocate_all_near_labels ();       // treat all near labels
}  

//======================================================================================

public
void extra_86_code ()
{
  // for 32-bit : extra functions for mult/div/mod int8

  {
    int i;
    for (i=0; i<3; i++)
    {
      if (i == 0 && used_multiply_int8 == false)
        continue;
      if (i == 1 && used_divide_int8 == false)
        continue;
      if (i == 2 && used_modulo_int8 == false)
        continue;

      reset_pool_backfills ();
      exe_reset_dll_move_chain ();

      // ! we must do this in advance before no P-code is generated
      exe_allocate_near_label_table (20);  // enough

      if (i == 0)
        gen_code_for_mult_int8 ();
      else if (i == 1)
        gen_code_for_div_int8 ();
      else if (i == 2)
        gen_code_for_mod_int8 ();

      relocate_all_near_labels ();
    }
  }


  // extra functions for allocate/free tombstones

  if (pointer_checks_enabled)
  {
    int i;
    for (i=0; i<3; i++)
    {
      reset_pool_backfills ();
      exe_reset_dll_move_chain ();
      exe_allocate_near_label_table (20);  // enough    // ! we must do this in advance before P-code is generated

      if (i == 0)
        gen_code_for_tombstone_get_lock ();
      else if (i == 1)
        gen_code_for_tombstone_malloc ();
      else if (i == 2)
        gen_code_for_tombstone_free ();

      relocate_all_near_labels ();
    }
  }
  else
  {
    reset_pool_backfills ();
    exe_reset_dll_move_chain ();
    exe_allocate_near_label_table (20);  // enough    // ! we must do this in advance before P-code is generated

    gen_code_for_free_possible_null ();

    relocate_all_near_labels ();
  }


  // 64-bit debug function for testing stack alignment at M16 (to be removed in final version)

  if (address_size == 8 && option_check_stack_M16_alignment)
  {
    reset_pool_backfills ();
    exe_reset_dll_move_chain ();
    exe_allocate_near_label_table (20);  // enough    // ! we must do this in advance before P-code is generated

    gen_code_for_check_stack_M16_alignment ();

    relocate_all_near_labels ();
  }
}

//======================================================================================
