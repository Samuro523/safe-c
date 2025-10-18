
// codstatem.h : generate code for control flow statements

use entities, ../codgen;

/*******************************************************************************************/

// returns true if code after statement is unreachable
bool generate_code_for_if_statement (    PENTITY    e,
                                         wstring    func_id,
                                     ref FRAME_INFO frame,
                                         int        outer_label_break,
                                         int        outer_label_continue,
                                     ref bool       any_jumps_to_label_break,
                                     ref bool       any_jumps_to_label_continue);

/*******************************************************************************************/

// returns true if code after statement is unreachable
bool generate_code_for_while_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame);

/*******************************************************************************************/

// returns true if code after statement is unreachable
bool generate_code_for_for_statement (    PENTITY    e,
                                          wstring    func_id,
                                      ref FRAME_INFO frame);

/*******************************************************************************************/

void generate_code_for_break_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame,
                                            int        outer_label_break);

/*******************************************************************************************/

void generate_code_for_continue_statement (    PENTITY    e,
                                               wstring    func_id,
                                           ref FRAME_INFO frame,
                                               int        outer_label_continue);

/*******************************************************************************************/

void generate_code_for_switch_statement (    PENTITY    e,
                                             wstring    func_id,
                                         ref FRAME_INFO frame,
                                              int       outer_label_continue,
                                         ref bool       any_jumps_to_label_continue);

/*******************************************************************************************/
