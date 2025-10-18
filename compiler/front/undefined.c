
// undefined.c : detect un-initialized local variables, out parameters, unreachable statements

from std use bintree, strings;
use ../error, ../common;
use type, lex, entities, typedecl;

//--------------------------------------------------------------------------------

// evaluation mode for expressions

enum EVAL_MODE
{
  EVAL_READ,    // read object
  EVAL_WRITE,   // write to object
  EVAL_OADDR,   // don't read or write object, but evaluate object address (read array indexes)
};

//--------------------------------------------------------------------------------

// content of a btree entry

enum STATE
{
  UNDEFINED,
  INITIALIZED,   // has received a value
  READ,          // was read at least once
  TERMINATED,    // an error was generated, so ignore this entity.
};

struct UNDEFINED_NODE
{
  PENTITY  e;      // local variable or parameter entity.
  STATE    state;  // UNDEFINED, INITIALIZED, READ or TERMINATED.
}

//--------------------------------------------------------------------------------

package P = new BALANCED_BINARY_TREE (ELEMENT => UNDEFINED_NODE, USER_INFO => bool);

//--------------------------------------------------------------------------------

// list of btrees (there's one btree per scope level)

struct TREE_LIST
{
  BINARY_TREE tree;
  int         level;   // 0 means .next is null
  TREE_LIST^  next;
}

//--------------------------------------------------------------------------------

// function for comparing two entries, for sorting

int sw_compare (bool^ user, UNDEFINED_NODE data1, UNDEFINED_NODE data2)
{
  _unused user;
  if (data1.e^.nr < data2.e^.nr)
    return -1;
  if (data1.e^.nr > data2.e^.nr)
    return +1;
  return 0;
}

//--------------------------------------------------------------------------------

// create a new node in btree list

TREE_LIST^ new_node (TREE_LIST^ previous)
{
  TREE_LIST^ p;

  p = new TREE_LIST;

  create_btree (out p^.tree, null, sw_compare);
  p^.level = (previous == null) ? 0 : (previous^.level + 1);
  p^.next = previous;

  return p;
}

//--------------------------------------------------------------------------------

// remove one node from bree list

void remove_node (ref TREE_LIST^ p)
{
  TREE_LIST^ t;

  t = p;
  p = t^.next;

  close_btree (ref t^.tree);
  free t;
}

//--------------------------------------------------------------------------------

void free_list (TREE_LIST^ p)
{
  TREE_LIST^ t, q;

  t = p;
  while (t != null)
  {
    q = t^.next;
    close_btree (ref t^.tree);
    free t;
    t = q;
  }
}

//--------------------------------------------------------------------------------

TREE_LIST^ clone_list (TREE_LIST^ p)
{
  TREE_LIST^     t, prev;
  uint2          mode;
  int            rc;
  UNDEFINED_NODE n;

  if (p == null)
    return null;

  prev = clone_list (p^.next);

  t = new_node (prev);

  mode = BT_FIRST;
  clear n;
  for (;;)
  {
    rc = retrieve_btree (p^.tree, ref n, mode);
    if (rc == BT_KEY_NOT_FOUND)
      break;
    if (rc < 0)
      fatal_compiler_error ("clone_list : retrieve() failed", token.pos);

    rc = insert_btree (ref t^.tree, n);
    if (rc < 0)
      fatal_compiler_error ("clone_list : insert() failed", token.pos);

    mode = BT_LARGER;
  }

  return t;
}

//--------------------------------------------------------------------------------

// merge only common levels

void merge_list (TREE_LIST^ master,   // nodes will be updated
                 TREE_LIST^ slave)    // nodes will be read
{
  TREE_LIST^     m, s;
  uint2          mode;
  int            rc;
  UNDEFINED_NODE nm, ns;

  m = master;
  s = slave;

  if (m == null || s == null)
    return;

  while (m^.level > s^.level)
    m = m^.next;

  while (s^.level > m^.level)
    s = s^.next;

  while (m != null)
  {
    mode = BT_FIRST;
    clear ns;
    for (;;)
    {
      rc = retrieve_btree (s^.tree, ref ns, mode);   // loop on slave tree entities
      if (rc == BT_KEY_NOT_FOUND)
        break;
      if (rc < 0)
        fatal_compiler_error ("merge_list : retrieve1() failed", token.pos);

      clear nm;
      nm.e = ns.e;
      rc = retrieve_btree (m^.tree, ref nm, BT_EQUAL); // find same entity in master tree
      if (rc == BT_KEY_NOT_FOUND)
        break;
      if (rc < 0)
        fatal_compiler_error ("merge_list : retrieve2() failed", token.pos);

//  UNDEFINED
//  INITIALIZED   // has received a value
//  READ          // was read at least once
//  TERMINATED    // an error was generated, so ignore this entity.

      if (ns.state == nm.state)   // identical state
        ;
      else if (ns.state == INITIALIZED && nm.state == READ)    // conditionnally READ is considered as READ
        nm.state = READ;
      else if (ns.state == READ && nm.state == INITIALIZED)
        nm.state = READ;
      else if (ns.state == TERMINATED || nm.state == TERMINATED)  // any of both gave an error message
        nm.state = TERMINATED;
      else
        nm.state = UNDEFINED;   // at least one of both is UNDEFINED, the other is INITIALIZED or READ.

      rc = update_btree (ref m^.tree, nm);
      if (rc < 0)
        fatal_compiler_error ("merge_list : update() failed", token.pos);

      mode = BT_LARGER;
    }

    m = m^.next;
    s = s^.next;
  }
}

//--------------------------------------------------------------------------------

void assign_list (TREE_LIST^ master,   // nodes will be updated
                  TREE_LIST^ slave)    // nodes will be read
{
  TREE_LIST^     m, s;
  uint2          mode;
  int            rc;
  UNDEFINED_NODE nm, ns;

  m = master;
  s = slave;

  if (m == null || s == null)
    return;

  while (m^.level > s^.level)
    m = m^.next;

  while (s^.level > m^.level)
    s = s^.next;

  while (m != null)   // loop on common master-slave levels
  {
    mode = BT_FIRST;
    clear ns;
    for (;;)
    {
      rc = retrieve_btree (s^.tree, ref ns, mode);   // loop on slave tree entities
      if (rc == BT_KEY_NOT_FOUND)
        break;
      if (rc < 0)
        fatal_compiler_error ("assign_list : retrieve1() failed", token.pos);

      clear nm;
      nm.e = ns.e;
      rc = retrieve_btree (m^.tree, ref nm, BT_EQUAL); // find same entity in master tree
      if (rc == BT_KEY_NOT_FOUND)
        break;
      if (rc < 0)
        fatal_compiler_error ("assign_list : retrieve2() failed", token.pos);

      if (ns.state != nm.state)   // not identical state
      {
        nm.state = ns.state;
        rc = update_btree (ref m^.tree, nm);
        if (rc < 0)
          fatal_compiler_error ("assign_list : update() failed", token.pos);
      }

      mode = BT_LARGER;
    }

    m = m^.next;
    s = s^.next;
  }
}

//--------------------------------------------------------------------------------

void terminate_list (TREE_LIST^ master)   // set all state of last level to TERMINATED
{
  TREE_LIST^     m;
  uint2          mode;
  int            rc;
  UNDEFINED_NODE nm;

  m = master;

  if (m != null)   // loop on common master-slave levels
  {
    mode = BT_FIRST;
    clear nm;
    for (;;)
    {
      rc = retrieve_btree (m^.tree, ref nm, mode);   // loop on slave tree entities
      if (rc == BT_KEY_NOT_FOUND)
        break;
      if (rc < 0)
        fatal_compiler_error ("terminate_list : retrieve1() failed", token.pos);

      if (nm.state != TERMINATED)
      {
        nm.state = TERMINATED;
        rc = update_btree (ref m^.tree, nm);
        if (rc < 0)
          fatal_compiler_error ("terminate_list : update() failed", token.pos);
      }

      mode = BT_LARGER;
    }
  }
}

//--------------------------------------------------------------------------------

// insert entity in first node of btree list, with state undefined

void insert_object (TREE_LIST^ p, PENTITY e, STATE s)
{
  int            rc;
  UNDEFINED_NODE n;

  clear n;
  n.e = e;
  n.state = s;

  rc = insert_btree (ref p^.tree, n);
  if (rc < 0)
    fatal_compiler_error ("insert_object", token.pos);
}

//--------------------------------------------------------------------------------

STATE get_object (TREE_LIST^ list, PENTITY e)
{
  TREE_LIST^     p;
  UNDEFINED_NODE n;
  int            rc;

  p = list;
  while (p != null)
  {
    clear n;
    n.e = e;
    rc = retrieve_btree (p^.tree, ref n, BT_EQUAL);
    if (rc == 0)
      return n.state;

    if (rc != BT_KEY_NOT_FOUND)
      fatal_compiler_error ("get_object", token.pos);

    p = p^.next;
  }

  fatal_compiler_error ("get_object3", token.pos);
  return TERMINATED;
}

//--------------------------------------------------------------------------------

void set_object (TREE_LIST^ list, PENTITY e, STATE state)
{
  TREE_LIST^     p;
  UNDEFINED_NODE n;
  int            rc;

  p = list;
  while (p != null)
  {
    clear n;
    n.e = e;
    rc = retrieve_btree (p^.tree, ref n, BT_EQUAL);
    if (rc == 0)
    {
      n.state = state;

      rc = update_btree (ref p^.tree, n);
      if (rc < 0)
        fatal_compiler_error ("set_object1", token.pos);

      return;
    }

    if (rc != BT_KEY_NOT_FOUND)
      fatal_compiler_error ("set_object2", token.pos);

    p = p^.next;
  }

  fatal_compiler_error ("set_object3", token.pos);
}

//--------------------------------------------------------------------------------
void detect_in_exp (PEXPRESSION   exp,
                    EVAL_MODE     eval_mode,
                    bool          override_pos,
                    TEXT_POSITION pos,
                    TREE_LIST^    list);
//--------------------------------------------------------------------------------

// perform eval_mode on any local variable or parameter that's in any btree of list.
// list will not be modified by the function.
// Instead, two secondary boolean lists will be computed,
//  which in simple cases can just be clones of the primary list.

void detect_in_bool_exp (    PEXPRESSION   exp,
                             EVAL_MODE     eval_mode,
                             bool          override_pos,
                             TEXT_POSITION pos,
                             TREE_LIST^    list,
                         out TREE_LIST^[2] bool_list)   // secondary lists for (false, true) result
{
  if (exp == null)
  {
    bool_list = {clone_list (list),
                 clone_list (list)};
    return;
  }

  switch (exp^.kind)
  {
    case AN_OPERATOR_VALUE:
      switch (exp^.operator_value_info.op)
      {
        case OP_CONDITIONAL_TEST:
          {
            TREE_LIST^[2] local_bool_list;
            TREE_LIST^[2] branch1_bool_list, branch2_bool_list;

            detect_in_bool_exp (    exp^.operator_value_info.arg[0],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    list,
                                out local_bool_list);

            detect_in_bool_exp (    exp^.operator_value_info.arg[1],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    local_bool_list[1],   // condition was true
                                out branch1_bool_list);

            detect_in_bool_exp (    exp^.operator_value_info.arg[2],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    local_bool_list[0],   // condition was false
                                out branch2_bool_list);

            merge_list (branch1_bool_list[0], branch2_bool_list[0]);   // merge both false results
            merge_list (branch1_bool_list[1], branch2_bool_list[1]);   // merge both true results

            free_list (branch2_bool_list[0]);
            free_list (branch2_bool_list[1]);

            bool_list = branch1_bool_list;
          }
          break;

        case OP_SHORT_CIRCUIT_AND:
          {
            TREE_LIST^[2] branch1_bool_list, branch2_bool_list;

            detect_in_bool_exp (    exp^.operator_value_info.arg[0],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    list,
                                out branch1_bool_list);

            detect_in_bool_exp (    exp^.operator_value_info.arg[1],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    branch1_bool_list[1],   // first argument was true
                                out branch2_bool_list);

            merge_list (branch2_bool_list[0], branch1_bool_list[0]);
            free_list (branch1_bool_list[0]);

            bool_list = branch2_bool_list;
          }
          break;

        case OP_SHORT_CIRCUIT_OR:
          {
            TREE_LIST^[2] branch1_bool_list, branch2_bool_list;

            detect_in_bool_exp (    exp^.operator_value_info.arg[0],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    list,
                                out branch1_bool_list);

            detect_in_bool_exp (    exp^.operator_value_info.arg[1],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    branch1_bool_list[0],   // first argument was false
                                out branch2_bool_list);

            merge_list (branch2_bool_list[1], branch1_bool_list[1]);
            free_list (branch1_bool_list[1]);

            bool_list = branch2_bool_list;
          }
          break;

        case OP_BOOL_NOT:     // bool -> bool
          {
            TREE_LIST^[2] local_bool_list;

            detect_in_bool_exp (    exp^.operator_value_info.arg[0],
                                    EVAL_READ,
                                    override_pos,
                                    pos,
                                    list,
                                out local_bool_list);

            bool_list = {local_bool_list[1], local_bool_list[0]};  // reverse chain
          }
          break;

        default:
          {
            detect_in_exp (exp, eval_mode, override_pos, pos, list);
            bool_list = {clone_list (list),
                         clone_list (list)};
          }
          break;
      }
      break;

    case A_QUALIFIED_EXPRESSION:
      detect_in_bool_exp (    exp^.qualified_expression_info.value,
                              EVAL_READ,
                              override_pos,
                              pos,
                              list,
                          out bool_list);
      break;

    default:
      detect_in_exp (exp, eval_mode, override_pos, pos, list);
      bool_list = {clone_list (list),
                   clone_list (list)};
      break;
  }
}

//--------------------------------------------------------------------------------

// perform eval_mode on any local variable or parameter that's in any btree of list

void detect_in_exp (PEXPRESSION   exp,
                    EVAL_MODE     eval_mode,
                    bool          override_pos,
                    TEXT_POSITION pos,
                    TREE_LIST^    list)
{
  int     i;
  PENTITY e;

  if (exp == null)
    return;

  switch (exp^.kind)
  {
    case AN_OPERATOR_VALUE:
      switch (exp^.operator_value_info.op)
      {
        case OP_CONDITIONAL_TEST:
        case OP_SHORT_CIRCUIT_AND:
        case OP_SHORT_CIRCUIT_OR:
          {
            TREE_LIST^[2] local_bool_list;

            detect_in_bool_exp (    exp,
                                    eval_mode,
                                    override_pos,
                                    pos,
                                    list,
                                out local_bool_list);   // secondary list for (false, true) result

            merge_list (local_bool_list[0], local_bool_list[1]);
            assign_list (list, local_bool_list[0]);

            free_list (local_bool_list[0]);
            free_list (local_bool_list[1]);
          }
          break;

        case OP_ADDRESS_OF:         // operator '&' is considered as write+read access
          {
            detect_in_exp (exp^.operator_value_info.arg[0],
                           EVAL_WRITE,
                           override_pos,
                           pos,
                           list);

            detect_in_exp (exp^.operator_value_info.arg[0],
                           EVAL_READ,
                           override_pos,
                           pos,
                           list);
          }
          break;

        case OP_LENGTH:
        case OP_SIZE:
          {
            detect_in_exp (exp^.operator_value_info.arg[0],
                           EVAL_OADDR,       // prefix is used, but no read or write access
                           override_pos,
                           pos,
                           list);
          }
          break;

        default:
          for (i=0; i<3; i++)
          {
            detect_in_exp (exp^.operator_value_info.arg[i],
                           eval_mode,
                           override_pos,
                           pos,
                           list);
          }
          break;
      }
      break;

    case A_RUN_CALL:
      detect_in_exp (exp^.run_call_info.function_call,
                     eval_mode,
                     override_pos,
                     pos,
                     list);
      break;

    case A_FUNCTION_VALUE:
      break;

    case A_FUNCTION_CALL:
      {
        LIST_OF_EXPRESSIONS^ p;

        detect_in_exp (exp^.function_call_info.func,
                       EVAL_READ,
                       override_pos,
                       pos,
                       list);

        // before function call, the out parameters are not read (only any indexes are)
        p = exp^.function_call_info.param;
        while (p != null)
        {
          detect_in_exp (p^.exp,
                         p^.e^.the_parameter.mode == MODE_OUT ? EVAL_OADDR : EVAL_READ,
                         override_pos,
                         pos,
                         list);
          p = p^.next;
        }

        // after function call, the out parameters are considered initialized
        p = exp^.function_call_info.param;
        while (p != null)
        {
          if (p^.e^.the_parameter.mode == MODE_OUT)
          {
            detect_in_exp (p^.exp,
                           EVAL_WRITE,
                           override_pos,
                           pos,
                           list);
          }
          p = p^.next;
        }
      }
      break;

    case A_DISCRIMINANT_VALUE:
      detect_in_exp (exp^.discriminant_value_info.prefix,
                     EVAL_OADDR,      // reading the discriminant does not read the object
                     override_pos,
                     pos,
                     list);
      break;

    case AN_UNC_ARRAY_AGGREGATE:
      detect_in_exp (exp^.unc_array_aggregate_info.element,
                     EVAL_READ,
                     override_pos,
                     pos,
                     list);
      break;


    case AN_AGGREGATE_VALUE:
      {
        LIST_OF_EXPRESSIONS^ p;

        p = exp^.aggregate_value_info.list;
        while (p != null)
        {
          detect_in_exp (p^.exp,
                         EVAL_READ,
                         override_pos,
                         pos,
                         list);
          p = p^.next;
        }
      }
      break;


    case A_QUALIFIED_EXPRESSION:
      detect_in_exp (exp^.qualified_expression_info.value,
                     EVAL_READ,
                     override_pos,
                     pos,
                     list);
      break;


    case AN_ARRAY_QUALIFIED_EXPRESSION:
      detect_in_exp (exp^.array_qualified_expression_info.length,
                     EVAL_READ,
                     override_pos,
                     pos,
                     list);
      detect_in_exp (exp^.array_qualified_expression_info.value,
                     EVAL_READ,
                     override_pos,
                     pos,
                     list);
      break;

    case A_STRUCT_QUALIFIED_EXPRESSION:
      detect_in_exp (exp^.struct_qualified_expression_info.discriminant,
                     EVAL_READ,
                     override_pos,
                     pos,
                     list);
      detect_in_exp (exp^.struct_qualified_expression_info.value,
                     EVAL_READ,
                     override_pos,
                     pos,
                     list);
      break;

    case AN_ALLOCATOR:
      detect_in_exp (exp^.allocator_info.value,
                     EVAL_READ,
                     override_pos,
                     pos,
                     list);
      break;


    case A_GLOBAL_VARIABLE_OBJECT:
      break;


    case A_LOCAL_VARIABLE_OBJECT:

      e = exp^.local_variable_object_info.pobject;

      if (eval_mode == EVAL_READ)
        e^.the_local_variable.is_read = true;

      if (eval_mode == EVAL_WRITE)
        e^.the_local_variable.is_written = true;

      switch (get_object (list, e))
      {
        case UNDEFINED:
          if (eval_mode == EVAL_READ)    // read, and the variable is undefined !
          {
            char str[256];

            sprintf (out str, "local variable '%.64S' used without having been initialized",
                     e^.identifier_or_null^);

            semantic_error (str,
                            override_pos ? pos : exp^.local_variable_object_info.pos);

            set_object (list, e, TERMINATED);
          }
          else if (eval_mode == EVAL_WRITE)    // write, and the variable is undefined.
          {
            set_object (list, e, INITIALIZED);
          }
          break;

        case INITIALIZED:
          if (eval_mode == EVAL_READ)
          {
            set_object (list, e, READ);
          }
          break;

        default:
          break;
      }
      break;


    case A_REFERENCE_OBJECT:

      detect_in_exp (exp^.reference_object_info.pobject^.the_reference.name,
                     eval_mode,
                     true,
                     override_pos ? pos : exp^.reference_object_info.pos,
                     list);
      break;


    case A_PARAMETER_OBJECT:

      e = exp^.parameter_object_info.pobject;

      switch (get_object (list, e))
      {
        case UNDEFINED:
          if (eval_mode == EVAL_READ)    // read, and the parameter is undefined !
          {
            char str[256];

            sprintf (out str, "parameter '%.64S' used without having been initialized",
                     e^.identifier_or_null^);

            semantic_error (str,
                            override_pos ? pos : exp^.parameter_object_info.pos);

            set_object (list, e, TERMINATED);
          }
          else if (eval_mode == EVAL_WRITE)    // write, and the parameter is undefined.
          {
            set_object (list, e, INITIALIZED);
          }
          break;

        case INITIALIZED:
          if (eval_mode == EVAL_READ)
          {
            set_object (list, e, READ);
          }
          break;

        default:
          break;
      }
      break;


    case AN_ARRAY_ELEMENT_OBJECT:
      {
        PENTITY   prefix_type;
        EVAL_MODE mode;

        mode = eval_mode;

        prefix_type = exp^.array_element_object_info.prefix^.base_type_or_null;
        if (prefix_type != null && prefix_type^.kind == AN_UNSAFE_POINTER_TYPE)
        {
          // prefix is an unsafe pointer
          mode = EVAL_READ;
        }
        else  // an array
        {
          if (mode == EVAL_WRITE)   // don't propagate write on parts of the full variable
            mode = EVAL_OADDR;
        }

        detect_in_exp (exp^.array_element_object_info.prefix,
                       mode,
                       override_pos,
                       pos,
                       list);

        detect_in_exp (exp^.array_element_object_info.index,
                       EVAL_READ,          // always READ the index
                       override_pos,
                       pos,
                       list);
      }
      break;


    case AN_ARRAY_SLICE_OBJECT:
      {
        PENTITY   prefix_type;
        EVAL_MODE mode;

        mode = eval_mode;

        prefix_type = exp^.array_slice_object_info.prefix^.base_type_or_null;
        if (prefix_type != null && prefix_type^.kind == AN_UNSAFE_POINTER_TYPE)
        {
          // prefix is an unsafe pointer
          mode = EVAL_READ;
        }
        else  // an array
        {
          if (mode == EVAL_WRITE)   // don't propagate write on parts of the full variable
            mode = EVAL_OADDR;
        }

        detect_in_exp (exp^.array_slice_object_info.prefix,
                       mode,
                       override_pos,
                       pos,
                       list);

        detect_in_exp (exp^.array_slice_object_info.index,
                       EVAL_READ,          // always READ the index
                       override_pos,
                       pos,
                       list);

        detect_in_exp (exp^.array_slice_object_info.length,
                       EVAL_READ,          // always READ the length
                       override_pos,
                       pos,
                       list);
      }
      break;


    case A_STRUCT_FIELD_OBJECT:
      {
        EVAL_MODE mode;

        mode = eval_mode;
        if (mode == EVAL_WRITE)   // don't propagate write on parts of the full variable
          mode = EVAL_OADDR;

        detect_in_exp (exp^.struct_field_object_info.prefix,
                       mode,
                       override_pos,
                       pos,
                       list);
      }
      break;

    case A_DEREFERENCED_OBJECT:
      detect_in_exp (exp^.dereferenced_object_info.prefix,
                     EVAL_READ,         // evaluate the pointer, always in read
                     override_pos,
                     pos,
                     list);
      break;


    case AN_UNSAFE_DEREFERENCED_OBJECT:
      detect_in_exp (exp^.unsafe_dereferenced_object_info.unsafe_ptr_value,
                     EVAL_READ,         // evaluate the pointer, always in read
                     override_pos,
                     pos,
                     list);
      break;


    case AN_ATTR_BYTE_OBJECT:
      detect_in_exp (exp^.attr_byte_object_info.prefix,
                     eval_mode,
                     override_pos,
                     pos,
                     list);
      break;


    case A_BOXED_OBJECT:
      detect_in_exp (exp^.boxed_object_info.parameter,
                     eval_mode,
                     override_pos,
                     pos,
                     list);
      break;


    case AN_UNBOXED_OBJECT:
      detect_in_exp (exp^.unboxed_object_info.parameter,
                     eval_mode,
                     override_pos,
                     pos,
                     list);
      break;


    case A_BOXED_ARRAY_OBJECT:
      {
        LIST_OF_EXPRESSIONS^ p;
        p = exp^.boxed_array_object_info.list;
        while (p != null)
        {
          detect_in_exp (p^.exp,
                         eval_mode,
                         override_pos,
                         pos,
                         list);
          p = p^.next;
        }
      }
      break;

    default:
      break;
  }
}

//--------------------------------------------------------------------------------

// display warnings for never referenced constants/local variables/ref/parameters

void generate_warning_for_unused_items (PREGION r)
{
  PENTITY e;
  char    str[256];

  e = r^.entities.first;

  while (e != null)
  {
    switch (e^.kind)
    {
      case A_CONSTANT:
        if (!e^.the_constant.is_used)
        {
          sprintf (out str, "constant '%.64S' is never used", e^.identifier_or_null^);
          warning (str, e^.the_constant.pos);
        }
        break;

      case A_LOCAL_VARIABLE:
        if (!e^.the_local_variable.is_used)
        {
          sprintf (out str, "local variable '%.64S' is never used", e^.identifier_or_null^);
          warning (str, e^.the_local_variable.pos);
        }
        else if (e^.the_local_variable.is_written && (!e^.the_local_variable.is_read))
        {
          sprintf (out str, "local variable '%.64S' is never read", e^.identifier_or_null^);
          warning (str, e^.the_local_variable.pos);
        }
        break;

      case A_REFERENCE:
        if (!e^.the_reference.is_used)
        {
          sprintf (out str, "reference '%.64S' is never used", e^.identifier_or_null^);
          warning (str, e^.the_reference.pos);
        }
        break;

      case A_PARAMETER:
        if ((!e^.the_parameter.is_used) &&
            e^.the_parameter.mode != MODE_OUT)   // out parameter already has its error message
        {                                        // when it's not initialized.
          sprintf (out str, "parameter '%.64S' is never used", e^.identifier_or_null^);
          warning (str, e^.the_parameter.pos);
        }
        break;

      default:
        break;
    }

    e = e^.next;
  }
}

//--------------------------------------------------------------------------------

void output_errors_for_undefined_out_parameters (TREE_LIST^ list)
{
  TREE_LIST^     l;
  uint2          mode;
  int            rc;
  UNDEFINED_NODE n;

  l = list;
  while (l^.level > 0)       // skip to level 0
    l = l^.next;

  mode = BT_FIRST;
  clear n;
  for (;;)
  {
    rc = retrieve_btree (l^.tree, ref n, mode);
    if (rc == BT_KEY_NOT_FOUND)
      break;
    if (rc < 0)
      fatal_compiler_error ("output_out_param : retrieve() failed", token.pos);

    if (n.e^.kind == A_PARAMETER && n.e^.the_parameter.mode == MODE_OUT &&
        n.state == UNDEFINED && !n.e^.the_parameter.init_error)
    {
      char str[256];
      sprintf (out str, "out parameter '%S' does not receive a value in all flow paths", n.e^.identifier_or_null^);
      semantic_error (str, n.e^.the_parameter.pos);
      n.e^.the_parameter.init_error = true;    // used to avoid repeating the error message
    }

    mode = BT_LARGER;
  }
}

//--------------------------------------------------------------------------------

bool is_empty_or_unused_statement_sequence (PENTITY e_list)
{
  PENTITY e = e_list;

  while (e != null)
  {
    if (e^.kind != AN_UNUSED_STATEMENT)
      return false;
    e = e^.next;
  }

  return true;
}

//--------------------------------------------------------------------------------

void detect_unused_statement (PENTITY    e,
                              TREE_LIST^ list)
{
  PENTITY n = e^.the_unused_statement.item;

  if (n^.kind == A_LOCAL_VARIABLE)
  {
    n^.the_local_variable.is_read = true;

    switch (get_object (list, n))
    {
      case UNDEFINED:   // read, and the variable is undefined !
        {
          char str[256];

          sprintf (out str, "local variable '%.64S' used without having been initialized", n^.identifier_or_null^);
          semantic_error (str, n^.the_local_variable.pos);

          set_object (list, n, TERMINATED);
        }
        break;

      case INITIALIZED:
        set_object (list, n, UNDEFINED);
        break;

      case READ:     // was read before
        {
          char str[256];

          sprintf (out str, "_unused not allowed for local variable '%.64S'", n^.identifier_or_null^);
          semantic_error (str, n^.the_local_variable.pos);

          set_object (list, n, TERMINATED);
        }
        break;

      default:
        break;
    }
  }
  else
  {
    n^.the_parameter.is_used = true;

    switch (get_object (list, n))
    {
      case UNDEFINED:   // read, and the parameter is undefined !
        {
          char str[256];

          sprintf (out str, "parameter '%.64S' used without having been initialized", n^.identifier_or_null^);

          semantic_error (str, n^.the_parameter.pos);

          set_object (list, n, TERMINATED);
        }
        break;

      case INITIALIZED:
        set_object (list, n, UNDEFINED);
        break;

      case READ:     // was read before
        {
          char str[256];

          sprintf (out str, "_unused not allowed for parameter '%.64S'", n^.identifier_or_null^);
          semantic_error (str, n^.the_parameter.pos);

          set_object (list, n, TERMINATED);
        }
        break;

      default:
        break;
    }
  }
}

//--------------------------------------------------------------------------------

void process_any_following_unused_statements (PENTITY    e_list,
                                              TREE_LIST^ list)
{
  PENTITY e = e_list;
  while (e != null && e^.kind == AN_UNUSED_STATEMENT)
  {
    detect_unused_statement (e, list);
    e = e^.next;
  }
}

//--------------------------------------------------------------------------------

// returns true if code after statement is unreachable

bool detect_in_region (    PREGION    r,
                           TREE_LIST^ list,
                       ref TREE_LIST^ pbreak_list,
                       ref TREE_LIST^ pcontinue_list,
                           bool       are_active_statements_following_in_outer_block)
{
  PENTITY    e;
  TREE_LIST^ l2, l3;

  e = r^.entities.first;

  while (e != null)
  {
    switch (e^.kind)
    {
      case A_LOCAL_VARIABLE:
        if (e^.the_local_variable.initial_value_or_null == null)
        {
          insert_object (list, e, UNDEFINED);       // add local variable to tree list
        }
        else
        {
          detect_in_exp (e^.the_local_variable.initial_value_or_null,
                         EVAL_READ,
                         false,
                         token.pos,
                         list);

          insert_object (list, e, INITIALIZED);       // add local variable to tree list
        }
        break;

      case A_REFERENCE:
        detect_in_exp (e^.the_reference.name,
                       EVAL_OADDR,   // name need not be initialized, but indexes or slices must be
                       false,
                       token.pos,
                       list);
        break;

      case A_CLEAR_STATEMENT:
        detect_in_exp (e^.the_clear_statement.name,
                       EVAL_WRITE,
                       false,
                       token.pos,
                       list);
        break;

      case AN_ASSIGNMENT_STATEMENT:
        detect_in_exp (e^.the_assignment_statement.name,
                       EVAL_OADDR,    // first evaluate name in mode none (for all indexes)
                       false,
                       token.pos,
                       list);

        detect_in_exp (e^.the_assignment_statement.value,
                       EVAL_READ,
                       false,
                       token.pos,
                       list);

        if (e^.the_assignment_statement.op != _ASSIGN)
        {
          detect_in_exp (e^.the_assignment_statement.name,
                         EVAL_READ,
                         false,
                         token.pos,
                         list);
        }
        else
        {
          detect_in_exp (e^.the_assignment_statement.name,
                         EVAL_WRITE,
                         false,
                         token.pos,
                         list);
        }
        break;

      case A_PRE_OR_POSTFIX_STATEMENT:
        detect_in_exp (e^.the_pre_or_postfix_statement.name,
                       EVAL_READ,
                       false,
                       token.pos,
                       list);
        break;

      case A_FUNCTION_CALL_STATEMENT:
        detect_in_exp (e^.the_function_call_statement.name,
                       EVAL_READ,
                       false,
                       token.pos,
                       list);
        break;

      case A_RETURN_STATEMENT:
        detect_in_exp (e^.the_return_statement.value,
                       EVAL_READ,
                       false,
                       token.pos,
                       list);

        // output error messages for undefined 'out' parameters
        output_errors_for_undefined_out_parameters (list);

        // flag any following statements as unreachable
        if ((!is_empty_or_unused_statement_sequence (e^.next)) || are_active_statements_following_in_outer_block)
          warning ("statements following return are unreachable", e^.the_return_statement.pos);

        process_any_following_unused_statements (e^.next, list);

        return true;  // unreachable


      case A_BREAK_STATEMENT:          // leaves switch, for, while (can break out of 'block' or 'if')
        if (pbreak_list == null)
          pbreak_list = clone_list (list);
        else
          merge_list (pbreak_list, list);

        // flag any following statements as unreachable
        if (e^.next != null || are_active_statements_following_in_outer_block)
          warning ("statements following break are unreachable", e^.the_break_statement.pos);

        return true;  // unreachable


      case A_CONTINUE_STATEMENT:
        if (pcontinue_list == null)
          pcontinue_list = clone_list (list);
        else
          merge_list (pcontinue_list, list);

        // flag any following statements as unreachable
        if (e^.next != null || are_active_statements_following_in_outer_block)
          warning ("statements following continue are unreachable", e^.the_continue_statement.pos);

        return true;  // unreachable


      case A_FREE_STATEMENT:
        detect_in_exp (e^.the_free_statement.value,
                       EVAL_READ,
                       false,
                       token.pos,
                       list);
        break;

      case AN_ASSERT_STATEMENT:
        detect_in_exp (e^.the_assert_statement.value,
                       EVAL_READ,
                       false,
                       token.pos,
                       list);
        break;

      case AN_ABORT_STATEMENT:

        // flag any following statements as unreachable
        if (e^.next != null || are_active_statements_following_in_outer_block)
          warning ("statements following abort are unreachable", e^.the_abort_statement.pos);

        return true;   // unreachable


      case A_SLEEP_STATEMENT:
        detect_in_exp (e^.the_sleep_statement.value,
                       EVAL_READ,
                       false,
                       token.pos,
                       list);
        break;


      case AN_UNUSED_STATEMENT:
        detect_unused_statement (e, list);
        break;

      case A_BLOCK_STATEMENT:
        {
          bool unreachable;

          // add new node in tree list
          TREE_LIST^ list2 = new_node (list);

          // and call this function recursively
          unreachable =
             detect_in_region (    e^.the_block_statement.inner,
                                   list2,
                               ref pbreak_list,
                               ref pcontinue_list,
                                   are_active_statements_following_in_outer_block =>
                                         !is_empty_or_unused_statement_sequence (e^.next));

          // display warnings for never referenced constants/local variables/ref/parameters (check entities)
          generate_warning_for_unused_items (e^.the_block_statement.inner);

          // remove last node
          remove_node (ref list2);   // list2 becomes list

          if (unreachable)
          {
            process_any_following_unused_statements (e^.next, list);
            return true;
          }
        }
        break;


      case AN_IF_STATEMENT:
        {
          bool unreachable1, unreachable2;
          TREE_LIST^[2] local_bool_list;

          detect_in_bool_exp (    e^.the_if_statement.condition,
                                  EVAL_READ,
                                  false,
                                  token.pos,
                                  list,
                              out local_bool_list);

          unreachable1 = detect_in_region (e^.the_if_statement.true_branch, local_bool_list[1], ref pbreak_list, ref pcontinue_list, false);
          unreachable2 = detect_in_region (e^.the_if_statement.false_branch, local_bool_list[0], ref pbreak_list, ref pcontinue_list, false);

          if (unreachable1 == true && unreachable2 == false)
          {
            assign_list (list, local_bool_list[0]); // keep local_bool_list[0]
          }
          else if (unreachable1 == false && unreachable2 == true)
          {
            assign_list (list, local_bool_list[1]); // keep local_bool_list[1]
          }
          else if (unreachable1 == false && unreachable2 == false)
          {
            merge_list (local_bool_list[0], local_bool_list[1]);
            assign_list (list, local_bool_list[0]);
          }
          else     // both are unreachable
          {
            terminate_list (list);
          }

          free_list (local_bool_list[0]);
          free_list (local_bool_list[1]);

          if (unreachable1 && unreachable2)
            return true;
        }
        break;


      case A_SWITCH_STATEMENT:
        {
          FLOW_ALTERNATIVE^ alt;
          TREE_LIST^        break_list;
          bool              unreachable, all_unreachable;

          detect_in_exp (e^.the_switch_statement.value,
                         EVAL_READ,
                         false,
                         token.pos,
                         list);

          l2 = null;    // final list

          all_unreachable = true;

          alt = e^.the_switch_statement.first_alt;
          while (alt != null)
          {
            l3 = clone_list (list);    // list at the end of this case

            break_list = null;

            unreachable = detect_in_region (alt^.inner, l3, ref break_list, ref pcontinue_list, false);
            all_unreachable &= (unreachable && break_list == null);

            if (!unreachable)
            {
              merge_list (l3, break_list);
              free_list (break_list);
            }
            else
            {
              free_list (l3);
              l3 = break_list;     // can be null
            }

            if (l3 != null)   // state at the end of case (can be null if unreachable)
            {
              merge_list (l3, l2);
              free_list (l2);
              l2 = l3;
            }

            alt = alt^.next;
          }

          if (all_unreachable)
          {
            terminate_list (list);
            return true;
          }

          assign_list (list, l2);
        }
        break;

      case A_WHILE_STATEMENT:
        {
          PEXPRESSION   exp;
          TREE_LIST^[2] local_bool_list;
          TREE_LIST^    break_list, continue_list;

          exp = e^.the_while_statement.condition;

          detect_in_bool_exp (    exp,
                                  EVAL_READ,
                                  false,
                                  token.pos,
                                  list,
                              out local_bool_list);

          break_list = null;
          continue_list = null;
          detect_in_region (e^.the_while_statement.inner, local_bool_list[1], ref break_list, ref continue_list, false);

          free_list (continue_list);   // continue_list is ignored

          if (exp^.kind == A_CONST_ENUMERATION_VALUE &&
              exp^.const_enumeration_value_info.value == 1)  // true (we can only leave with a break)
          {
            if (break_list == null)
            {
              terminate_list (list);

              free_list (local_bool_list[0]);
              free_list (local_bool_list[1]);

              if (e^.next != null || are_active_statements_following_in_outer_block)
                warning ("statements following 'while' statement are unreachable", e^.the_while_statement.pos);

              return true;   // unreachable
            }
            else
            {
              // remove some nodes of break_list
              while (break_list^.level > list^.level)
                remove_node (ref break_list);

              assign_list (list, break_list);   // after while statement : state like at break statements
            }
          }
          else   // anything else than true constant
          {
            assign_list (list, local_bool_list[0]);
          }

          free_list (local_bool_list[0]);
          free_list (local_bool_list[1]);
          free_list (break_list);
        }
        break;


      case A_FOR_STATEMENT:
        {
          PEXPRESSION   exp;
          TREE_LIST^[2] local_bool_list;
          TREE_LIST^    break_list, continue_list;

          detect_in_region (e^.the_for_statement.pre, list, ref pbreak_list, ref pcontinue_list, false);

          detect_in_region (e^.the_for_statement.exp_region, list, ref pbreak_list, ref pcontinue_list, false);

          exp = e^.the_for_statement.condition;   // can be null

          detect_in_bool_exp (    exp,
                                  EVAL_READ,
                                  false,
                                  token.pos,
                                  list,
                              out local_bool_list);

          break_list = null;
          continue_list = null;

          detect_in_region (e^.the_for_statement.inner, local_bool_list[1], ref break_list, ref continue_list, false);

          merge_list (local_bool_list[1], continue_list);
          free_list (continue_list);

          detect_in_region (e^.the_for_statement.post, local_bool_list[1], ref pbreak_list, ref pcontinue_list, false);

          if (exp == null ||
              (exp^.kind == A_CONST_ENUMERATION_VALUE &&
               exp^.const_enumeration_value_info.value == 1))  // true (we can only leave with a break)
          {
            if (break_list == null)
            {
              terminate_list (list);

              free_list (local_bool_list[0]);
              free_list (local_bool_list[1]);

              if (e^.next != null || are_active_statements_following_in_outer_block)
                warning ("statements following 'for' statement are unreachable", e^.the_for_statement.pos);

              return true;   // unreachable
            }
            else
            {
              // remove some nodes of break_list
              while (break_list^.level > list^.level)
                remove_node (ref break_list);

              assign_list (list, break_list);   // after for statement : state like at break statements
            }
          }
          else   // anything else than true constant
          {
            assign_list (list, local_bool_list[0]);
          }

          free_list (local_bool_list[0]);
          free_list (local_bool_list[1]);
          free_list (break_list);
        }
        break;

      default:
        break;
    }

    e = e^.next;
  }

  return false;
}

//--------------------------------------------------------------------------------

void add_parameters_to_list (PREGION parameters, TREE_LIST^ list)
{
  PENTITY e;

  e = parameters^.entities.first;

  while (e != null)
  {
    switch (e^.kind)
    {
      case A_PARAMETER:
        if (e^.the_parameter.mode != MODE_OUT)    // mode IN or REF
        {
          insert_object (list, e, INITIALIZED);
        }
        else if (!types_are_equal (base_type_of (e^.the_parameter.type), type_array_of_object))
        {
          insert_object (list, e, UNDEFINED);
        }
        else
        {
          // out parameter of type object[] is considered as initialized,
          // it must not receive a complete value.
          // example: int sscanf (string buffer, string format, out object[] arg);
          insert_object (list, e, INITIALIZED);
        }
        break;

      default:
        break;
    }

    e = e^.next;
  }
}

//--------------------------------------------------------------------------------

// called for each function body, after it was syntactically checked

public
void detect_uninitialized_variables (PREGION parameters, PREGION inner)
{
  TREE_LIST^ list, break_list, continue_list;

  // create a tree list node for function's local scope (level 0)
  list = new_node (null);

  // add parameters to list
  add_parameters_to_list (parameters, list);

  break_list = null;
  continue_list = null;

  if (detect_in_region (inner, list, ref break_list, ref continue_list, false) == false)  // reachable
  {
    // output error messages for undefined 'out' parameters
    output_errors_for_undefined_out_parameters (list);
  }

  // free list
  remove_node (ref list);

  // display warnings for never referenced constants/local variables/ref/parameters (check entities)
  generate_warning_for_unused_items (parameters);
  generate_warning_for_unused_items (inner);
}

//--------------------------------------------------------------------------------
