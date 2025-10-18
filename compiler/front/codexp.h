
// codexp.h : code generator for expressions, declarations and simple statements

use entities, ../codgen;

/*******************************************************************************************/

void generate_code_for_assignment_statement (    PENTITY    e,
                                                 wstring    func_id,
                                             ref FRAME_INFO frame);

/*******************************************************************************************/

// allocate it if on heap, initialize it
void generate_code_for_local_variable_declaration (    PENTITY    e,
                                                       wstring    func_id,
                                                   ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_global_variable_declaration (    PENTITY    e,
                                                    ref FRAME_INFO frame);  // dummy

/*******************************************************************************************/

void generate_code_for_reference_declaration (    PENTITY    e,
                                                  wstring    func_id,
                                              ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_clear_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_pre_or_postfix_statement (    PENTITY    e,
                                                     wstring    func_id,
                                                 ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_function_call_statement (    PENTITY    e,
                                                    wstring    func_id,
                                                ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_free_statement (    PENTITY    e,
                                           wstring    func_id,
                                       ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_sleep_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame);

/*******************************************************************************************/

// returns true if code after statement is unreachable
bool generate_code_for_abort_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame);

/*******************************************************************************************/

// returns true if code after statement is unreachable
bool generate_code_for_assert_statement (    PENTITY    e,
                                             wstring    func_id,
                                         ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_code_statement (    PENTITY    e,
                                           wstring    func_id,
                                       ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_return_statement (    PENTITY    e,
                                             wstring    func_id,
                                         ref FRAME_INFO frame,
                                             PENTITY    e_parent);

/*******************************************************************************************/

void generate_code_for_branching (    PEXPRESSION exp,
                                      bool        branch_iftrue,
                                      int4        label_nr,
                                  ref FRAME_INFO  frame);

/*******************************************************************************************/

void generate_code_for_bool_expression (    PEXPRESSION exp,
                                        ref FRAME_INFO  frame);

/*******************************************************************************************/

void generate_code_for_discrete_expression (    PEXPRESSION exp,
                                            ref FRAME_INFO  frame);

/*******************************************************************************************/

// for objects, returns either _YES or _NO.
// for values, returns _YES, _NO or _YES_BUT_CAN_BE_NULL.

USES_TOMBSTONE exp_requires_tombstone_anchor (PEXPRESSION exp);

/*******************************************************************************************/

// used for 64bit for generating table of first 4 parameters to load in RCX, RDX, R8, R9.
// returns one of b (bool), i (int4), l (int8), f (float4), d (float8), a (address)

char code_datatype (PENTITY type);

/*******************************************************************************************/
