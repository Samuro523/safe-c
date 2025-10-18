
// codstatem.c : code generator for statements

from std use sorting;
use entities, codexp, type;
use ../pcodes, ../error, ../goptions, ../codout, ../pool, ../common, ../codgen;

/*******************************************************************************************/

bool is_empty_statement_sequence (PENTITY e_list)
{
  PENTITY e = e_list;

  while (e != null)
  {
    if (e^.kind == AN_UNUSED_STATEMENT)
      ;
    else if (e^.kind == A_BLOCK_STATEMENT)
    {
      if (!is_empty_statement_sequence (e^.the_block_statement.inner^.entities.first))
        return false;
    }
    else
    {
      return false;
    }

    e = e^.next;
  }

  return true;
}

/*******************************************************************************************/

// returns true if code after statement is unreachable

public
bool generate_code_for_if_statement (    PENTITY    e,
                                         wstring    func_id,
                                     ref FRAME_INFO frame,
                                         int        outer_label_break,
                                         int        outer_label_continue,
                                     ref bool       any_jumps_to_label_break,
                                     ref bool       any_jumps_to_label_continue)
{
  PEXPRESSION exp;
  bool        unreachable, unreachable2;

  exp = e^.the_if_statement.condition;


  // optimize constant case

  if (exp^.kind == A_CONST_ENUMERATION_VALUE)
  {
    if (exp^.const_enumeration_value_info.value == 1)  // true
    {
      unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_if_statement.true_branch^.entities.first,
                                ref frame,
                                    outer_label_break,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
    }
    else
    {
      unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_if_statement.false_branch^.entities.first,
                                ref frame,
                                    outer_label_break,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
    }

    return unreachable;
  }


  generate_p_location (e^.the_if_statement.loc);


  // optimize   "if (e) break;"  and   "if (e) continue;"

  {
    PENTITY first_st;

    first_st = e^.the_if_statement.true_branch^.entities.first;

    if (first_st != null)
    {
      if (first_st^.kind == A_BREAK_STATEMENT)
      {
        if (release_objects_in_outer_scope (K_FOR | K_WHILE | K_SWITCH, e, false) == false)
        {
          generate_code_for_branching (    e^.the_if_statement.condition,
                                           branch_iftrue => true,
                                           outer_label_break,
                                       ref frame);
          any_jumps_to_label_break = true;

          unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_if_statement.false_branch^.entities.first,
                                ref frame,
                                    outer_label_break,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
          return unreachable;
        }
      }
      else if (first_st^.kind == A_CONTINUE_STATEMENT)
      {
        if (release_objects_in_outer_scope (K_FOR | K_WHILE, e, false) == false)
        {
          generate_code_for_branching (    e^.the_if_statement.condition,
                                           branch_iftrue => true,
                                           outer_label_continue,
                                       ref frame);
          any_jumps_to_label_continue = true;

          unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_if_statement.false_branch^.entities.first,
                                ref frame,
                                    outer_label_break,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
          return unreachable;
        }
      }
    }
  }


  // optimize   "if (e) st; else break;"  and   "if (e) st; else continue;"

  {
    PENTITY first_st;

    first_st = e^.the_if_statement.false_branch^.entities.first;

    if (first_st != null)
    {
      if (first_st^.kind == A_BREAK_STATEMENT)
      {
        if (release_objects_in_outer_scope (K_FOR | K_WHILE | K_SWITCH, e, false) == false)
        {
          generate_code_for_branching (    e^.the_if_statement.condition,
                                           branch_iftrue => false,
                                           outer_label_break,
                                       ref frame);
          any_jumps_to_label_break = true;

          unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_if_statement.true_branch^.entities.first,
                                ref frame,
                                    outer_label_break,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
          return unreachable;
        }
      }
      else if (first_st^.kind == A_CONTINUE_STATEMENT)
      {
        if (release_objects_in_outer_scope (K_FOR | K_WHILE, e, false) == false)
        {
          generate_code_for_branching (    e^.the_if_statement.condition,
                                           branch_iftrue => false,
                                           outer_label_continue,
                                       ref frame);
          any_jumps_to_label_continue = true;

          unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_if_statement.true_branch^.entities.first,
                                ref frame,
                                    outer_label_break,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
          return unreachable;
        }
      }
    }
  }


  // optimize   "if (e) ; else ;"

  if (is_empty_statement_sequence (e^.the_if_statement.true_branch^.entities.first) &&   // true branch empty
      is_empty_statement_sequence (e^.the_if_statement.false_branch^.entities.first))    // false branch empty
  {
    generate_code_for_bool_expression (e^.the_if_statement.condition, ref frame);
    put_code (P_DROP_BOOL);
    return false;
  }


  // optimize   "if (e) ; else st;"

  if (is_empty_statement_sequence (e^.the_if_statement.true_branch^.entities.first))   // true branch empty
  {
    int label_nr1;

    label_nr1 = get_new_near_label_nr ();

    generate_code_for_branching (    e^.the_if_statement.condition,
                                     branch_iftrue => true,
                                     label_nr1,
                                 ref frame);

    generate_code_for_region (    func_id,
                                  e,
                                  e^.the_if_statement.false_branch^.entities.first,
                              ref frame,
                                  outer_label_break,
                                  outer_label_continue,
                              ref any_jumps_to_label_break,
                              ref any_jumps_to_label_continue);
    put_code (P_NEAR_LABEL);
    put_int4 (label_nr1);

    return false;
  }


  // general case

  {
    int label_nr1, label_nr2=0;

    label_nr1 = get_new_near_label_nr ();

    generate_code_for_branching (    e^.the_if_statement.condition,
                                     branch_iftrue => false,
                                     label_nr1,
                                 ref frame);

    unreachable = generate_code_for_region
                             (    func_id,
                                  e,
                                  e^.the_if_statement.true_branch^.entities.first,
                              ref frame,
                                  outer_label_break,
                                  outer_label_continue,
                              ref any_jumps_to_label_break,
                              ref any_jumps_to_label_continue);

    if (is_empty_statement_sequence (e^.the_if_statement.false_branch^.entities.first))
    {
      put_code (P_NEAR_LABEL);
      put_int4 (label_nr1);

      return false;    // reachable through jmp to label_nr1
    }
    else  // there is an "else"
    {
      if (!unreachable)
      {
        label_nr2 = get_new_near_label_nr ();

        put_code (P_GOTO);
        put_int4 (label_nr2);
      }

      put_code (P_NEAR_LABEL);
      put_int4 (label_nr1);

      unreachable2 = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_if_statement.false_branch^.entities.first,
                                ref frame,
                                    outer_label_break,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
      if (!unreachable)
      {
        put_code (P_NEAR_LABEL);
        put_int4 (label_nr2);
      }

      return unreachable & unreachable2;  // unreachable if both are unreachable
    }
  }
}

/*******************************************************************************************/

// returns true if code after statement is unreachable

public
bool generate_code_for_while_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame)
{
  PEXPRESSION exp;
  int         label_break, label_continue;
  bool        unreachable;
  bool        any_jumps_to_label_break    = false;
  bool        any_jumps_to_label_continue = false;

  exp = e^.the_while_statement.condition;

  label_continue = get_new_near_label_nr ();
  label_break    = get_new_near_label_nr ();

  if (exp^.kind == A_CONST_ENUMERATION_VALUE)
  {
    if (exp^.const_enumeration_value_info.value == 1)  // true
    {
      put_code (P_NEAR_LABEL);
      put_int4 (label_continue);

      unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_while_statement.inner^.entities.first,
                                ref frame,
                                    label_break,
                                    label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
      if (!unreachable)
      {
        put_code (P_GOTO);
        put_int4 (label_continue);
      }

      put_code (P_NEAR_LABEL);
      put_int4 (label_break);

      return (!any_jumps_to_label_break);
    }
    else   // constant false
    {
      return false;   // reachable
    }
  }
  else
  {
    int label_up;

    label_up = get_new_near_label_nr ();

    if (!is_empty_statement_sequence (e^.the_while_statement.inner^.entities.first))   // there are inner statements
    {
      put_code (P_GOTO);
      put_int4 (label_continue);
    }

    put_code (P_NEAR_LABEL);
    put_int4 (label_up);

    generate_code_for_region (    func_id,
                                  e,
                                  e^.the_while_statement.inner^.entities.first,
                              ref frame,
                                  label_break,
                                  label_continue,
                              ref any_jumps_to_label_break,
                              ref any_jumps_to_label_continue);

    put_code (P_NEAR_LABEL);
    put_int4 (label_continue);

    generate_p_location (e^.the_while_statement.loc);

    generate_code_for_branching (    e^.the_while_statement.condition,
                                     branch_iftrue => true,
                                     label_up,
                                 ref frame);

    put_code (P_NEAR_LABEL);
    put_int4 (label_break);

    return false;  // always reachable (through false runtime condition or break)
  }
}

/*******************************************************************************************/

// returns true if code after statement is unreachable

public
bool generate_code_for_for_statement (    PENTITY    e,
                                          wstring    func_id,
                                      ref FRAME_INFO frame)
{
  PEXPRESSION exp;
  int         label_break, label_continue, label_up;
  bool        unreachable;
  bool        any_jumps_to_label_break    = false;
  bool        any_jumps_to_label_continue = false;

  exp = e^.the_for_statement.condition;

  label_continue = get_new_near_label_nr ();
  label_break    = get_new_near_label_nr ();
  label_up       = get_new_near_label_nr ();

  generate_code_for_region (    func_id,
                                e,
                                e^.the_for_statement.pre^.entities.first,
                            ref frame,
                                label_break,
                                label_continue,
                            ref any_jumps_to_label_break,
                            ref any_jumps_to_label_continue);

  if (exp == null || exp^.kind == A_CONST_ENUMERATION_VALUE)
  {
    if (exp == null || exp^.const_enumeration_value_info.value == 1)  // true
    {
      put_code (P_NEAR_LABEL);
      put_int4 (label_up);

      unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    e^.the_for_statement.inner^.entities.first,
                                ref frame,
                                    label_break,
                                    label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);

      put_code (P_NEAR_LABEL);
      put_int4 (label_continue);

      if ((!unreachable) | any_jumps_to_label_continue)
      {
        generate_code_for_region (    func_id,
                                      e,
                                      e^.the_for_statement.post^.entities.first,
                                  ref frame,
                                      label_break,
                                      label_continue,
                                  ref any_jumps_to_label_break,
                                  ref any_jumps_to_label_continue);
        put_code (P_GOTO);
        put_int4 (label_up);
      }

      put_code (P_NEAR_LABEL);
      put_int4 (label_break);

      return (!any_jumps_to_label_break);
    }
    else   // condition : constant false
    {
      return false;  // always reachable
    }
  }
  else
  {
    int label_start;

    label_start = get_new_near_label_nr ();

    if ((!is_empty_statement_sequence (e^.the_for_statement.inner^.entities.first)) ||
        (!is_empty_statement_sequence (e^.the_for_statement.post^.entities.first)))
    {
      put_code (P_GOTO);
      put_int4 (label_start);
    }

    put_code (P_NEAR_LABEL);
    put_int4 (label_up);

    unreachable = generate_code_for_region
                             (    func_id,
                                  e,
                                  e^.the_for_statement.inner^.entities.first,
                              ref frame,
                                  label_break,
                                  label_continue,
                              ref any_jumps_to_label_break,
                              ref any_jumps_to_label_continue);

    put_code (P_NEAR_LABEL);
    put_int4 (label_continue);

    if ((!unreachable) | any_jumps_to_label_continue)
    {
      generate_code_for_region (    func_id,
                                    e,
                                    e^.the_for_statement.post^.entities.first,
                                ref frame,
                                    label_break,
                                    label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
    }

    put_code (P_NEAR_LABEL);
    put_int4 (label_start);

    generate_code_for_region (    func_id,
                                  e,
                                  e^.the_for_statement.exp_region^.entities.first,
                              ref frame,
                                  label_break,
                                  label_continue,
                              ref any_jumps_to_label_break,
                              ref any_jumps_to_label_continue);

    generate_p_location (e^.the_for_statement.loc);

    generate_code_for_branching (    e^.the_for_statement.condition,
                                     branch_iftrue => true,
                                     label_up,
                                 ref frame);
    put_code (P_NEAR_LABEL);
    put_int4 (label_break);

    return false;  // always reachable (through false runtime condition or break)
  }
}

/*******************************************************************************************/

public
void generate_code_for_break_statement (    PENTITY    e,
                                            wstring    func_id,
                                        ref FRAME_INFO frame,
                                            int        outer_label_break)
{
  _unused e;
  _unused func_id;
  _unused frame;

  put_code (P_GOTO);
  put_int4 (outer_label_break);
}

/*******************************************************************************************/

public
void generate_code_for_continue_statement (    PENTITY    e,
                                               wstring    func_id,
                                           ref FRAME_INFO frame,
                                               int        outer_label_continue)
{
  _unused e;
  _unused func_id;
  _unused frame;

  put_code (P_GOTO);
  put_int4 (outer_label_continue);
}

/*******************************************************************************************/

package ALTERNATIVES

  struct CASE_NODE
  {
    int8              value;
    FLOW_ALTERNATIVE^ f;
  }

  struct INODE
  {
    uint  first;   // first index into case table
    uint  last;    // last index into case table
  }

end ALTERNATIVES;

int compare (CASE_NODE p1, CASE_NODE p2);
package Sort = new HeapSort (ELEMENT => CASE_NODE, compare => compare);

/*****************************************************************************/

int compare (CASE_NODE p1, CASE_NODE p2)
{
  if (p1.value < p2.value)
    return -1;
  if (p1.value > p2.value)
    return +1;
  return 0;
}

/*******************************************************************************************/

void generate_switch_flow (    int8         min,     // range of expression to evaluate
                               int8         max,
                               INTEGER_INFO type_info,
                               INODE[]      table2,
                               CASE_NODE[]  table,
                               int          label_default_drop,
                               int          label_default_no_drop,
                               int          label_exit,
                               PENTITY      e,
                               wstring      func_id,
                           ref FRAME_INFO   frame,
                               int          outer_label_continue,
                           ref bool         any_jumps_to_label_continue)
{
  uint4 middle;
  int8  first_value, last_value;
  int   label_larger_or_equal;
  uint4 i, pool_offset;
  int8  n;
  bool  unreachable, any_jumps_to_label_break;
  POOL  pool;

  if (table2'length > 1)
  {
    // we must split the possible intervals in two groups.
    middle = (uint)table2'length / 2;

    first_value = table[table2[middle].first].value;

    label_larger_or_equal = get_new_near_label_nr ();


    // if larger or equal than first_value, branch to label_larger_or_equal

    if (type_info.size <= 4)
    {
      if (type_info.is_signed)
        put_code (P_SWITCH_CMP_S4);
      else
        put_code (P_SWITCH_CMP_U4);
      put_int4 ((int4)first_value);
    }
    else
    {
      put_code (P_SWITCH_CMP_8);
      put_int8 (first_value);
    }
    put_byte ((byte)CMP_LARGER_OR_EQUAL);
    put_int4 (label_larger_or_equal);


    // smaller than middle
    generate_switch_flow (    min,
                              first_value-1,
                              type_info,
                              table2 [0 : middle],
                              table,
                              label_default_drop,
                              label_default_no_drop,
                              label_exit,
                              e,
                              func_id,
                          ref frame,
                              outer_label_continue,
                          ref any_jumps_to_label_continue);

    put_code (type_info.size <= 4 ? P_SHADOW_4 : P_SHADOW_8);

    put_code (P_NEAR_LABEL);
    put_int4 (label_larger_or_equal);

    // larger or equal than middle
    generate_switch_flow (    first_value,
                              max,
                              type_info,
                              table2[middle : (uint)table2'length - middle],
                              table,
                              label_default_drop,
                              label_default_no_drop,
                              label_exit,
                              e,
                              func_id,
                          ref frame,
                              outer_label_continue,
                          ref any_jumps_to_label_continue);
  }
  else   // table_count == 1 : only 1 interval left
  {
    first_value = table[table2[0].first].value;
    last_value  = table[table2[0].last].value;

    if (first_value < last_value)   // a jump table with several values
    {
      if (type_info.size == 8)
      {
        if (min < first_value)    // exp possibly smaller
        {
          put_code (P_SWITCH_CMP_8);
          put_int8 (first_value);
          put_byte ((byte)CMP_SMALLER);
          put_int4 (label_default_drop);
        }

        if (max > last_value)    // exp possibly larger
        {
          put_code (P_SWITCH_CMP_8);
          put_int8 (last_value);
          put_byte ((byte)CMP_LARGER);
          put_int4 (label_default_drop);
        }

        //  subtract lower bound
        put_code (P_CTE_8);
        put_int8 (first_value);
        put_code (P_SUB8);

        // conversion from int8 to uint4
        put_code (P_CONV_LONG_INT);
      }
      else    // for int4 / uint4
      {
        // subtract lower bound (yields unsigned result)
        put_code (P_CTE_4);
        put_int4 ((int4)first_value);
        put_code (P_SUB4);

        if (min < first_value || max > last_value)    // exp has be outside jump table
        {
          put_code (P_SWITCH_CMP_U4);      // always unsigned !
          put_int4 ((int4)(last_value - first_value));
          put_byte ((byte)CMP_LARGER);
          put_int4 (label_default_drop);
        }
      }

      // generate code for jump table

      pool = new_pool_constant ((uint4)address_size * (uint4)(last_value - first_value + 1), (uint)address_size);
      pool_offset = 0;

      // generate label table entries
      i = table2[0].first;
      for (n=first_value; n<=last_value; n++)
      {
        if (i < (uint)table'length && table[i].value == n)   // case constant exists
        {
          if (table[i].f^.label_nr_drop == 0)     // alternative has no label yet
          {
            table[i].f^.label_nr_drop    = get_new_near_label_nr ();
            table[i].f^.label_nr_no_drop = get_new_near_label_nr ();
          }
          store_integer (pool, pool_offset, table[i].f^.label_nr_no_drop, (uint)address_size);
          pool_offset += (uint)address_size;
          i++;
        }
        else   // case constant does not exist
        {
          store_integer (pool, pool_offset, label_default_no_drop, (uint)address_size);
          pool_offset += (uint)address_size;
        }
      }

      mark_jumptable (pool);
      put_code (P_JUMP_4);
      put_int8 (serial_nr_of_pool_cte (pool));
    }
    else   // single value
    {
      if (min < first_value || max > last_value)
      {
        if (type_info.size <= 4)
        {
          put_code (P_SWITCH_CMP_U4);
          put_int4 ((int4)first_value);
        }
        else
        {
          put_code (P_SWITCH_CMP_8);
          put_int8 (first_value);
        }
        put_byte ((byte)CMP_NOT_EQUAL);
        put_int4 (label_default_drop);
      }


      i = table2[0].first;
      if (table[i].f^.label_nr_drop == 0)     // alternative has no label yet
      {
        table[i].f^.label_nr_drop    = get_new_near_label_nr ();
        table[i].f^.label_nr_no_drop = get_new_near_label_nr ();
      }

      if (table[i].f^.code_generated)  // code was already generated earlier
      {
        if (type_info.size <= 4)
          put_code (P_DROP_4);
        else
          put_code (P_DROP_8);

        put_code (P_GOTO);
        put_int4 (table[i].f^.label_nr_no_drop);
      }
      else        // generate code for this alternative
      {
        put_code (P_NEAR_LABEL);
        put_int4 (table[i].f^.label_nr_drop);

        if (type_info.size <= 4)
          put_code (P_DROP_4);
        else
          put_code (P_DROP_8);

        put_code (P_NEAR_LABEL);
        put_int4 (table[i].f^.label_nr_no_drop);

        // generate code

        any_jumps_to_label_break = false;  // is not used
        
        unreachable = generate_code_for_region
                             (func_id,
                              e,
                              table[i].f^.inner^.entities.first,
                              ref frame,
                              label_exit,
                              outer_label_continue,
                              ref any_jumps_to_label_break,
                              ref any_jumps_to_label_continue);
        if (!unreachable)
        {
          put_code (P_GOTO);
          put_int4 (label_exit);
        }

        table[i].f^.code_generated = true;
      }
    }
  }
}

/*******************************************************************************************/

public
void generate_code_for_switch_statement (    PENTITY    e,
                                             wstring    func_id,
                                         ref FRAME_INFO frame,
                                             int        outer_label_continue,
                                         ref bool       any_jumps_to_label_continue)
{
  PEXPRESSION       exp;
  PENTITY           type;
  INTEGER_TYPE      int_typ;
  uint4             count, i, j1, j2, k;
  CASE_NODE[]^      table;
  FLOW_ALTERNATIVE^ f;
  CASE_CONSTANT^    l, list;
  INODE[]^          table2;
  bool              found;
  int               label_default_drop, label_default_no_drop, label_exit;
  bool              unreachable, any_jumps_to_label_break;
  int8              diff;

  exp = e^.the_switch_statement.value;
  type = complete_type_of (exp^.base_type_or_null);

  if (is_constant_exp (exp))
  {
    int8 value;

    if (exp^.kind == A_CONST_ENUMERATION_VALUE)
      value = exp^.const_enumeration_value_info.value;
    else
      value = exp^.const_integer_value_info.value;

    // find the matching alternative

    l = null;
    f = e^.the_switch_statement.first_alt;
    
    while (f != null)
    {
      list = f^.cte_list;
      while (list != null)
      {
        if (list^.value == value)
        {
          l = list;
          break;
        }
        list = list^.next;
      }
      if (l != null)    // value found
        break;

      f = f^.next;
    }

    if (l == null)   // value not found
      f = e^.the_switch_statement.last_alt;  // set f to last default case


    // generate code for branch f

    label_exit = get_new_near_label_nr ();

    any_jumps_to_label_break = false;   // not used anyway
    
    generate_code_for_region (    func_id,
                                  e,
                                  f^.inner^.entities.first,
                              ref frame,
                                  label_exit,
                                  outer_label_continue,
                              ref any_jumps_to_label_break,
                              ref any_jumps_to_label_continue);

    put_code (P_NEAR_LABEL);
    put_int4 (label_exit);

    return;
  }

  generate_p_location (e^.the_switch_statement.loc);

  generate_code_for_discrete_expression (exp, ref frame);
  if (type == type_bool)
    put_code (P_CONV_BOOL_INT);

  switch (type^.kind)
  {
    case AN_ENUMERATION_TYPE:
      int_typ = type^.the_enumeration_type.base;
      break;

    case AN_INTEGER_TYPE:
      int_typ = type^.the_integer_type.type;
      break;

    default:
      fatal_compiler_error0 ("switch(1)");
      return;
  }

  {
    ref INTEGER_INFO type_info = INTEGER_DATA[(uint)int_typ];

    count = e^.the_switch_statement.count;

    if (count == 0)    // only 1 default branch
    {
      if (type_info.size <= 4)
        put_code (P_DROP_4);
      else
        put_code (P_DROP_8);

      // generate code for default branch

      label_exit = get_new_near_label_nr ();

      any_jumps_to_label_break = false;   // not used anyway

      generate_code_for_region (    func_id,
                                    e,
                                    e^.the_switch_statement.last_alt^.inner^.entities.first,
                                ref frame,
                                    label_exit,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);

      put_code (P_NEAR_LABEL);
      put_int4 (label_exit);

      return;
    }


    // load case constants into table

    table = new CASE_NODE [count];

    i = 0;
    f = e^.the_switch_statement.first_alt;
    while (f != null)
    {
      l = f^.cte_list;
      while (l != null)
      {
        table^[i].value = l^.value;
        table^[i].f     = f;
        i++;
        l = l^.next;
      }

      f = f^.next;
    }


    // sort table by constant value

    Sort.sort (ref table^);


    // allocate inode table

    table2 = new INODE [count];


    // fill inode table with jump table ranges

    k = 0;    // index into table2

    i = 0;
    while (i < count)
    {
      const uint MIN    = 4;   // minimum nb cases in jump table
      const uint FACTOR = 4;   // allow 1/FACTOR full jump table

      j1 = i;

      // advance j1 until reaching MIN cases with close values

      found = false;

      for (;;)
      {
        if (count - j1 < MIN)  // less than MIN cases left
          break;

        diff = table^[j1+MIN-1].value - table^[j1].value;
        if (diff >= 0 && diff <= FACTOR*MIN)   // found MIN cases with close values
        {
          found = true;
          break;
        }

        j1++;
      }

      if (found)
      {
        j2 = j1 + (MIN-1);

        // expand the table upwards
        while (j1 > i)
        {
          diff = table^[j2].value - table^[j1-1].value;
          if (diff < 0 || diff > FACTOR*(j2-(j1-1)+1))
            break;
          j1--;
        }

        // expand the table downwards
        while (j2+1 < count)
        {
          diff = table^[j2+1].value - table^[j1].value;
          if (diff < 0 || diff > FACTOR*((j2+1)-j1+1))
            break;
          j2++;
        }

        // expand the table upwards again
        while (j1 > i)
        {
          diff = table^[j2].value - table^[j1-1].value;
          if (diff < 0 || diff > FACTOR*(j2-(j1-1)+1))
            break;
          j1--;
        }

        // we have an interval j1 .. j2 for a jump table

        // add individual values, from [i to j1[

        while (i < j1)
        {
          table2^[k].first = i;
          table2^[k].last  = i;
          k++;
          i++;
        }

        // add jump table [j1 to j2]

        table2^[k].first = j1;
        table2^[k].last  = j2;
        k++;

        i = j2+1;
      }
      else  // no jump table found
      {
        // add individual values, from [i to count[

        while (i < count)
        {
          table2^[k].first = i;
          table2^[k].last  = i;
          k++;
          i++;
        }

        i = count;
      }
    }


    label_default_drop    = get_new_near_label_nr ();
    label_default_no_drop = get_new_near_label_nr ();
    label_exit            = get_new_near_label_nr ();


    // be sure value is in lowest CPU register
    if (type_info.size <= 4)
      put_code (P_SYNC_4);
    else
      put_code (P_SYNC_8);


    // table2 has k elements

    generate_switch_flow (    type_info.min,
                              type_info.max, 
                              type_info, 
                              table2^ [0 : k], 
                              table^,
                              label_default_drop, 
                              label_default_no_drop, 
                              label_exit,
                              e, 
                              func_id, 
                          ref frame, 
                              outer_label_continue, 
                          ref any_jumps_to_label_continue);


    // generate code for all branches where it hasn't been done;
    // this includes the default branch.

    f = e^.the_switch_statement.first_alt;
    while (f != null)
    {
      if (!f^.code_generated)
      {
        if (!f^.is_default)
        {
          put_code (type_info.size <= 4 ? P_SHADOW_4 : P_SHADOW_8);

          put_code (P_NEAR_LABEL);
          put_int4 (f^.label_nr_drop);

          put_code (type_info.size <= 4 ? P_DROP_4 : P_DROP_8);

          put_code (P_NEAR_LABEL);
          put_int4 (f^.label_nr_no_drop);

          // generate code

          any_jumps_to_label_break = false;   // not used anyway

          unreachable = generate_code_for_region
                               (    func_id,
                                    e,
                                    f^.inner^.entities.first,
                                ref frame,
                                    label_exit,
                                    outer_label_continue,
                                ref any_jumps_to_label_break,
                                ref any_jumps_to_label_continue);
          if (!unreachable)
          {
            put_code (P_GOTO);
            put_int4 (label_exit);
          }
        }
        else      // default case
        {
          put_code (type_info.size <= 4 ? P_SHADOW_4 : P_SHADOW_8);

          put_code (P_NEAR_LABEL);
          put_int4 (label_default_drop);

          put_code (type_info.size <= 4 ? P_DROP_4 : P_DROP_8);

          put_code (P_NEAR_LABEL);
          put_int4 (label_default_no_drop);

          // generate code

          any_jumps_to_label_break = false;   // not used anyway

          generate_code_for_region
                    (    func_id,
                         e,
                         f^.inner^.entities.first,
                     ref frame,
                         label_exit,
                         outer_label_continue,
                     ref any_jumps_to_label_break,
                     ref any_jumps_to_label_continue);

          put_code (P_NEAR_LABEL);
          put_int4 (label_exit);
        }
      }

      f = f^.next;
    }


    free table2;
    free table;
  }
}

/*******************************************************************************************/
