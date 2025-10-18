
// codout.h

use pcodes;

void put_code (PCODE code);

void put_byte (byte b);

void put_int4 (int4 i);
void put_int8 (int8 i);

void put_float  (float f);
void put_double (double d);

void put_address (int8 ad);

void sync_stacks (int i, int f, int a);
void check_all_xx_stacks_empty ();

// to be called before each new function
void codout_reset_pos ();

// to be called at the end of each function
void codout_fix_enter (uint4 size, uint4 freespace, uint4 block, int callee_registers_addr);

#begin unsafe
void out_obtain_pcode_mem (out byte* pcode_mem, out int pcode_mem_size);
#end unsafe

/************************************************************************************************************/

struct PCODE_INFO
{
  string name;
  string input;    // int_stack (b=bool i=int4 l=int8), float_stack (f=float d=double), addr_stack (a=address)
  string output;
}

const PCODE_INFO pcode_table[1+(int)PCODE'last] =
{
  {"P_CTE_BOOL",    "",   "b"},
  {"P_CTE_4",       "",   "i"},
  {"P_CTE_8",       "",   "l"},

  {"P_CTE_FLT4",    "",   "f"},
  {"P_CTE_FLT8",    "",   "d"},

  {"P_CTE_NULL",    "",   "a"},

  {"P_FUNC_LABEL",  "",   ""},
  {"P_LOAD_CODE",   "",   "a"},
  {"P_LOAD_DLL",    "",   "a"},
  {"P_LOAD_SYSCALL","",   "a"},
  {"P_LOAD_CONST",  "",   "a"},
  {"P_LOAD_GLOBAL", "",   "a"},
  {"P_LOAD_LOCAL",  "",   "a"},

  {"P_VALUE_BOOL",  "a",  "b"},
  {"P_VALUE_I1",    "a",  "i"},
  {"P_VALUE_I2",    "a",  "i"},
  {"P_VALUE_U1",    "a",  "i"},
  {"P_VALUE_U2",    "a",  "i"},
  {"P_VALUE_4",     "a",  "i"},
  {"P_VALUE_8",     "a",  "l"},

  {"P_VALUE_FLT4",  "a",   "f"},
  {"P_VALUE_FLT8",  "a",   "d"},

  {"P_VALUE_ADDR",  "a",   "a"},
  {"P_VALUE_ADDR1", "aa",  "aa"},

  {"P_STORE_BOOL", "ab", ""},
  {"P_STORE_1",    "ai", ""},
  {"P_STORE_2",    "ai", ""},
  {"P_STORE_4",    "ai", ""},
  {"P_STORE_8",    "al", ""},

  {"P_STORE_FLT4",  "af", ""},
  {"P_STORE_FLT8",  "ad", ""},

  {"P_STORE_ADDR",  "aa", ""},

  {"P_COPY_BLOCK",         "aai", ""},
  {"P_ORDERED_COPY_BLOCK", "aai", ""},
  {"P_MULTI_COPY",         "aia", "a"},


  // ------------
  // 2. branching
  // ------------

  {"P_NEAR_LABEL",   "",    ""},
  {"P_GOTO",         "",    ""},

  {"P_TSTBOOL",      "b",   ""},

  {"P_CMP_BOOL",     "bb",  ""},
  {"P_CMP_S4",       "ii",  ""},
  {"P_CMP_U4",       "ii",  ""},
  {"P_CMP_S8",       "ll",  ""},
  {"P_CMP_FLT4",     "ff",  ""},
  {"P_CMP_FLT8",     "dd",  ""},
  {"P_CMP_ADDR",     "aa",  ""},

  {"P_BTRUE",     "",    ""},
  {"P_BFALSE",    "",    ""},

  {"P_SETBOOL",   "",    "b"},

  {"P_CAND",      "b",    ""},
  {"P_COR",       "b",    ""},

  {"P_SWITCH_CMP_S4",  "i",    "i"},
  {"P_SWITCH_CMP_U4",  "i",    "i"},
  {"P_SWITCH_CMP_8",   "l",    "l"},

  {"P_JUMP_4",     "i",    ""},

  {"P_DROP_BOOL",  "b",     ""},
  {"P_DROP_4",     "i",     ""},
  {"P_DROP_8",     "l",     ""},
  {"P_DROP_FLT_4", "f",     ""},
  {"P_DROP_FLT_8", "d",     ""},
  {"P_DROP_ADDR",  "a",     ""},

  {"P_DUP_ADDR",   "a",     "aa"},

  // ----------------
  // 3. function call
  // ----------------

  {"P_SYSCALL_EXTRA_STACK_SPACE", "",     ""},
  {"P_PUSH_BOOL",  "b",    ""},
  {"P_PUSH_4",     "i",    ""},
  {"P_PUSH_8",     "l",    ""},
  {"P_PUSH_FLT4",  "f",    ""},
  {"P_PUSH_FLT8",  "d",    ""},
  {"P_PUSH_ADDR",  "a",    ""},

  {"P_HINT_PARAM",        "",    ""},
  {"P_STORE_PARAM_BOOL",  "b",    "b"},
  {"P_STORE_PARAM_INT4",  "i",    "i"},
  {"P_STORE_PARAM_INT8",  "l",    "l"},
  {"P_STORE_PARAM_FLT4",  "f",    "f"},
  {"P_STORE_PARAM_FLT8",  "d",    "d"},
  {"P_STORE_PARAM_ADDR",  "a",    "a"},

  {"P_CALL_INTEL",        "a",   ""},
  {"P_CALL_ARM",          "",    ""},

  {"P_RETVALUE_VOID",    "",   ""},
  {"P_RETVALUE_BOOL",    "",   "b"},
  {"P_RETVALUE_4",       "",   "i"},
  {"P_RETVALUE_8",       "",   "l"},
  {"P_RETVALUE_FLT4",    "",   "f"},
  {"P_RETVALUE_FLT8",    "",   "d"},
  {"P_RETVALUE_ADDR",    "",   "a"},


  // -------------------------
  // 4. function prolog/epilog
  // -------------------------

  {"P_ENTER",     "",   ""},
  {"P_SAVE_XREG", "",   ""},
  {"P_SAVE_FREG", "",   ""},
  {"P_LEAVE",     "",   ""},

  {"P_RETURN_VOID",  "",    ""},
  {"P_RETURN_BOOL",  "b",   ""},
  {"P_RETURN_4",     "i",   ""},
  {"P_RETURN_8",     "l",   ""},
  {"P_RETURN_FLT4",  "f",   ""},
  {"P_RETURN_FLT8",  "d",   ""},
  {"P_RETURN_ADDR",  "a",   ""},

  {"P_RET",     "",   ""},


  // -------------
  // 5. arithmetic
  // -------------

  {"P_NOT_BOOL",  "b",   "b"},

  {"P_OR_BOOL",   "bb",  "b"},
  {"P_AND_BOOL",  "bb",  "b"},
  {"P_XOR_BOOL",  "bb",  "b"},

  {"P_NEG4",     "i",   "i"},
  {"P_NOT4",     "i",   "i"},

  {"P_ADD4",     "ii",  "i"},
  {"P_SUB4",     "ii",  "i"},
  {"P_SMUL4",    "ii",  "i"},
  {"P_SDIV4",    "ii",  "i"},
  {"P_SMOD4",    "ii",  "i"},
  {"P_BITAND4",  "ii",  "i"},
  {"P_BITOR4",   "ii",  "i"},
  {"P_BITXOR4",  "ii",  "i"},
  {"P_ASL4",     "ii",  "i"},
  {"P_ASR4",     "ii",  "i"},

  {"P_UMUL4",    "ii",  "i"},
  {"P_UDIV4",    "ii",  "i"},
  {"P_UMOD4",    "ii",  "i"},
  {"P_SHL4",     "ii",  "i"},
  {"P_SHR4",     "ii",  "i"},

  {"P_NEG8",     "l",   "l"},
  {"P_NOT8",     "l",   "l"},

  {"P_ADD8",     "ll",  "l"},
  {"P_SUB8",     "ll",  "l"},
  {"P_SMUL8",    "ll",  "l"},
  {"P_SDIV8",    "ll",  "l"},
  {"P_SMOD8",    "ll",  "l"},
  {"P_BITAND8",  "ll",  "l"},
  {"P_BITOR8",   "ll",  "l"},
  {"P_BITXOR8",  "ll",  "l"},
  {"P_ASL8",     "ll",  "l"},
  {"P_ASR8",     "ll",  "l"},

  {"P_NEG_FLT4", "f",   "f"},

  {"P_ADD_FLT4", "ff",  "f"},
  {"P_SUB_FLT4", "ff",  "f"},
  {"P_MUL_FLT4", "ff",  "f"},
  {"P_DIV_FLT4", "ff",  "f"},

  {"P_NEG_FLT8", "d",   "d"},

  {"P_ADD_FLT8", "dd",  "d"},
  {"P_SUB_FLT8", "dd",  "d"},
  {"P_MUL_FLT8", "dd",  "d"},
  {"P_DIV_FLT8", "dd",  "d"},


  // -------------------------
  // 6. pre/post dec/increment
  // -------------------------

  {"P_INC1",    "a",   ""},
  {"P_INC2",    "a",   ""},
  {"P_INC4",    "a",   ""},
  {"P_INC8",    "a",   ""},

  {"P_DEC1",    "a",   ""},
  {"P_DEC2",    "a",   ""},
  {"P_DEC4",    "a",   ""},
  {"P_DEC8",    "a",   ""},


  // --------------------------------
  // 7. combined arithmetic and store
  // --------------------------------

  // bool   (bool on int_stack)
  // ----

  {"P_AND_BOOL_TO",  "ab",  ""},
  {"P_OR_BOOL_TO",   "ab",  ""},
  {"P_XOR_BOOL_TO",  "ab",  ""},

  // int4   (signed value on int_stack)
  // ----

  {"P_ADD1_TO",  "ai",  ""},
  {"P_SUB1_TO",  "ai",  ""},
  {"P_AND1_TO",  "ai",  ""},
  {"P_OR1_TO",   "ai",  ""},
  {"P_XOR1_TO",  "ai",  ""},
  {"P_ASL1_TO",  "ai",  ""},
  {"P_ASR1_TO",  "ai",  ""},

  {"P_ADD2_TO",  "ai",  ""},
  {"P_SUB2_TO",  "ai",  ""},
  {"P_AND2_TO",  "ai",  ""},
  {"P_OR2_TO",   "ai",  ""},
  {"P_XOR2_TO",  "ai",  ""},
  {"P_ASL2_TO",  "ai",  ""},
  {"P_ASR2_TO",  "ai",  ""},

  {"P_ADD4_TO",  "ai",  ""},
  {"P_SUB4_TO",  "ai",  ""},
  {"P_AND4_TO",  "ai",  ""},
  {"P_OR4_TO",   "ai",  ""},
  {"P_XOR4_TO",  "ai",  ""},
  {"P_ASL4_TO",  "ai",  ""},
  {"P_ASR4_TO",  "ai",  ""},

  {"P_SHL1_TO",  "ai",  ""},
  {"P_SHR1_TO",  "ai",  ""},

  {"P_SHL2_TO",  "ai",  ""},
  {"P_SHR2_TO",  "ai",  ""},

  {"P_SHL4_TO",  "ai",  ""},
  {"P_SHR4_TO",  "ai",  ""},

  {"P_ADD8_TO",  "al",  ""},
  {"P_SUB8_TO",  "al",  ""},
  {"P_AND8_TO",  "al",  ""},
  {"P_OR8_TO",   "al",  ""},
  {"P_XOR8_TO",  "al",  ""},
  {"P_ASL8_TO",  "al",  ""},
  {"P_ASR8_TO",  "al",  ""},

  {"P_UNSAFE_ADD_TO",   "ai",   ""},
  {"P_UNSAFE_SUB_TO",   "ai",   ""},

  {"P_INC_ADDR", "a",   ""},
  {"P_DEC_ADDR", "a",   ""},

  {"P_INC_VALUE_ADDR",  "a",   "a"},
  {"P_DEC_VALUE_ADDR",  "a",   "a"},
  {"P_VALUE_ADDR_INC",  "a",   "a"},
  {"P_VALUE_ADDR_DEC",  "a",   "a"},


  // -------------
  // 8. conversion
  // -------------

  {"P_CONV_BOOL_INT",   "b",   "i"},
  {"P_CONV_INT_BOOL",   "i",   "b"},

  {"P_CONV_INT_LONG",   "i",   "l"},
  {"P_CONV_UINT_LONG",  "i",   "l"},

  {"P_CONV_LONG_INT",   "l",   "i"},

  {"P_CONV_INT_FLT4",   "i",   "f"},
  {"P_CONV_INT_FLT8",   "i",   "d"},

  {"P_CONV_UINT_FLT4",  "i",   "f"},
  {"P_CONV_UINT_FLT8",  "i",   "d"},

  {"P_CONV_LONG_FLT4",  "l",   "f"},
  {"P_CONV_LONG_FLT8",  "l",   "d"},

  {"P_CONV_FLT4_UINT",  "f",   "i"},
  {"P_CONV_FLT4_INT",   "f",   "i"},
  {"P_CONV_FLT4_LONG",  "f",   "l"},

  {"P_CONV_FLT8_UINT",  "d",   "i"},
  {"P_CONV_FLT8_INT",   "d",   "i"},
  {"P_CONV_FLT8_LONG",  "d",   "l"},

  {"P_CONV_FLT4_FLT8",  "f",   "d"},
  {"P_CONV_FLT8_FLT4",  "d",   "f"},

  {"P_EXTEND",          "i",   "i"},

  
  // --------
  // 9. names
  // --------

  {"P_GET_CONSTR0",    "a",   "ai"},
  {"P_GET_CONSTR1",    "aa",  "aai"},

  {"P_CHECK_INDEX",    "ii",  "i"},
  {"P_ADD_INDEX",      "ai",  "a"},
  {"P_ADD_PTR_OFFSET", "ai",  "a"},
  {"P_SUB_PTR_OFFSET", "ai",  "a"},

  {"P_CHECK_SLICE_0",  "i",   "i"},
  {"P_CHECK_SLICE_1",  "ii",  "i"},
  {"P_CHECK_SLICE_2",  "ii",  "i"},

  {"P_ADD_OFFSET",     "a",   "a"},
  {"P_ADD_OFFSET1",    "aa",  "aa"},

  {"P_CHECK_SAME_1",   "i",   ""},
  {"P_CHECK_SAME_2",   "ii",  "i"},

  {"P_ARRAY_SIZE",     "i",   "i"},
  {"P_SUB_PTRS",       "aa",  "i"},
  {"P_DEREF",          "a",   "a"},
  {"P_UNDEREF",        "",    ""},

  {"P_FORCE_BOOL",     "b",   "b"},
  {"P_FORCE_4",        "i",   "i"},
  {"P_FORCE_8",        "l",   "l"},
  {"P_FORCE_FLT4",     "f",   "f"},
  {"P_FORCE_FLT8",     "d",   "d"},
  {"P_FORCE_ADDR",     "a",   "a"},

  {"P_SYNC_BOOL",      "b",   "b"},
  {"P_SYNC_4",         "i",   "i"},
  {"P_SYNC_8",         "l",   "l"},
  {"P_SYNC_FLT4",      "f",   "f"},
  {"P_SYNC_FLT8",      "d",   "d"},
  {"P_SYNC_ADDR",      "a",   "a"},
  {"P_SYNC_ADDR_INT4", "ai",  "ai"},

  {"P_SYNC_STACKS",    "",    ""},
  {"P_SHADOW_4",       "",    "i"},
  {"P_SHADOW_8",       "",    "l"},


  // --------------
  // 10. Aggregates
  // --------------

  {"P_STORE_FIELD_BOOL",  "ab",    "a"},
  {"P_STORE_FIELD_1",     "ai",    "a"},
  {"P_STORE_FIELD_2",     "ai",    "a"},
  {"P_STORE_FIELD_4",     "ai",    "a"},
  {"P_STORE_FIELD_8",     "al",    "a"},
  {"P_STORE_FIELD_FLT4",  "af",    "a"},
  {"P_STORE_FIELD_FLT8",  "ad",    "a"},
  {"P_STORE_FIELD_ADDR",  "aa",    "a"},

  {"P_STORE_FIELD_BLOCK", "aa",    "a"},


  // --------
  // 11. heap
  // --------

  {"P_MALLOC",      "i",    "a"},
  {"P_FREE",        "a",    ""},
  {"P_ALLOC_TOMB",  "a",    "a"},
  {"P_FREE_TOMB",   "a",    ""},


  // --------------
  // 12. statements
  // --------------

  {"P_INIT_THREADS",   "",    ""},
  {"P_RUN_VOID_PARAM", "a",  "i"},
  {"P_RUN_INT4_PARAM", "ai", "i"},
  {"P_RUN_ADDR_PARAM", "aa", "i"},
  {"P_CLEAR",          "ai",  ""},
  {"P_ABORT",          "",    ""},
  {"P_ASSERT",         "",    ""},
  {"P_SLEEP_CTE",      "",    ""},
  {"P_SLEEP_INT4",     "i",   ""},
  {"P_SLEEP_FLT4",     "f",   ""},
  {"P_CODE",           "",    ""},


  // -------------------
  // 13. source location
  // -------------------

  {"P_LOCATION",       "",    ""},

};

/************************************************************************************************************/

