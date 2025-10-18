
// codgen.c : code generator (main)

from std use arithm, strings, tracing;

use front/entities, front/type, front/codexp, front/codstatem, front/heaptype;
use common, error, goptions, pcodes, codout, pool;

//======================================================================================

const uint STACK_ALIGNMENT = 16;    // mandatory for Intel's SSE2 and 64-bit thread stacks
                                    // and for Android's Aarch64

//======================================================================================

ENTITY_LIST^ first_pending_function, last_pending_function;
ENTITY_LIST^ used_global_variables;

//======================================================================================

int near_label_nr;
int func_label_nr = 1024;   // the first 1023 label numbers are reserved for bootstrap code

int func_label_to_init_constants;
int func_label_to_main;

//======================================================================================

// used by code generator layer to create unique labels;
// is reset to zero for each new function.

public int get_new_near_label_nr ()
{
  return ++near_label_nr;
}

//======================================================================================

public int get_new_func_label_nr ()
{
  return func_label_nr++;
}

//======================================================================================

void skip_to_next_param (ref PENTITY pparam)
{
  while (pparam != null && pparam^.kind != A_PARAMETER)
    pparam = pparam^.next;
}

//======================================================================================

// address, integer, float, address+integer

void parameter_sizes (    wstring func_id,
                          PENTITY param,
                      out int     addr_size,
                      out int     int_size,
                      out int     float_size,
                      out int     actual_size)   // actual size of single field (for android)
{
  PENTITY t = complete_type_of (param^.the_parameter.type);

  int_size   = 0;
  float_size = 0;
  addr_size  = 0;
  actual_size = 0;

  switch (t^.kind)
  {
    case AN_INTEGER_TYPE:
      if (param^.the_parameter.mode == MODE_IN)   // simple type
      {
        int size = INTEGER_DATA[(int)t^.the_integer_type.type].size;  // 1, 2, 4 or 8
        actual_size = size;
        int_size += max (size,             // 1, 2, 4 or 8
                         address_size);    // 4 or 8
      }
      else
      {
        addr_size += address_size;
        actual_size = address_size;
      }
      break;

    case A_FLOAT_TYPE:
      if (param^.the_parameter.mode == MODE_IN)   // simple type
      {
        int size = FLOAT_DATA[(int)t^.the_float_type.type].size;  // 4 or 8
        actual_size = size;
        float_size += max (size,           // 4 or 8
                           address_size);  // 4 or 8
      }
      else
      {
        addr_size += address_size;
        actual_size = address_size;
      }
      break;

    case AN_ENUMERATION_TYPE:
      if (param^.the_parameter.mode == MODE_IN)   // simple type
      {
        int size = INTEGER_DATA[(int)t^.the_enumeration_type.base].size;  // 1, 2 or 4
        actual_size = size;
        int_size += max (size,            // 1, 2 or 4
                         address_size);   // 4 or 8
      }
      else
      {
        addr_size += address_size;
        actual_size = address_size;
      }
      break;

    case AN_OPEN_ARRAY_TYPE:      // has a length
      addr_size += address_size;
      int_size  += address_size;  // aligned(length4)
      actual_size = address_size;
      break;

    case A_STRUCT_TYPE:
      addr_size += address_size;
      if (is_open_type (t))         // has a discriminant
        int_size += address_size;   // aligned(discriminant4)
      actual_size = address_size;
      break;

    case AN_ARRAY_TYPE:
    case A_CONSTRAINED_STRUCT_TYPE:
    case A_UNION_TYPE:
      addr_size += address_size;
      actual_size = address_size;
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
      addr_size += address_size;
      actual_size = address_size;
      break;

    default:
    {
      char str[128];
      sprintf (out str, "func %.32S has bad parameter type", func_id);
      fatal_compiler_error0 (str);
    }
    break;
  }
}

//======================================================================================

void intel_assign_offset_to_parameter (wstring func_id, PENTITY param, ref int8 poffset)
{
  int addr_size, int_size, float_size, dummy;

  param^.the_parameter.offset = (int4)poffset;

  parameter_sizes (func_id, param, out addr_size, out int_size, out float_size, out dummy);
  _unused dummy;

  poffset += (int_size + float_size + addr_size);
}

//======================================================================================

void intel_recursive_assign_offset_to_parameter (wstring func_id, PENTITY param, bool is_callback, out int8 poffset)
{
  PENTITY e;

  if (is_callback)     // push right to left
  {
    poffset = address_size * 2;    // first param at : EBP+8 (or RBP+16 for 64-bit)

    e = param;

    for (;;)
    {
      skip_to_next_param (ref e);

      if (e == null)
        break;

      intel_assign_offset_to_parameter (func_id, e, ref poffset);

      e = e^.next;
    }
  }
  else   //  push left to right
  {
    // advance til parameter or null

    e = param;

    skip_to_next_param (ref e);

    if (e == null)
    {
      poffset = address_size * 2;    // last param at : EBP+8 (or RBP+16 for 64-bit)
      return;
    }

    // first, assign offsets to rest of parameters
    intel_recursive_assign_offset_to_parameter (func_id, e^.next, is_callback, out poffset);

    intel_assign_offset_to_parameter (func_id, e, ref poffset);
  }
}

//======================================================================================

void android_assign_offset_to_parameters (wstring func_id, PENTITY param, out int saved_in_registers_size, out int saved_on_stack_size)
{
  PENTITY e;
  int     x_register_size, f_register_size, x_stack_size, f_stack_size;
  int     ofs_reg, ofs_stack, dummy;


  // first loop : computed these 4 variables

  x_register_size = 0;
  f_register_size = 0;
  x_stack_size    = 0;
  f_stack_size    = 0;

  e = param;
  skip_to_next_param (ref e);

  while (e != null)
  {
    int addr_size, int_size, float_size;

    parameter_sizes (func_id, e, out addr_size, out int_size, out float_size, out dummy);
    _unused dummy;

    if (x_register_size + (addr_size + int_size) <= 8*8 &&   // max 8 registers X0 to X7
        f_register_size + float_size             <= 8*8)     // max 8 registers D0 to D7
    {
      x_register_size += (addr_size + int_size);
      f_register_size += float_size;
    }
    else
    {
      x_stack_size += (addr_size + int_size);
      f_stack_size += float_size;
    }

    e = e^.next;
    skip_to_next_param (ref e);
  }


  // second loop : assign parameter offset

  ofs_reg   = -(x_register_size + f_register_size);
  ofs_stack = 0;

  x_register_size = 0;
  f_register_size = 0;
  x_stack_size    = 0;
  f_stack_size    = 0;

  e = param;
  skip_to_next_param (ref e);
  while (e != null)
  {
    int addr_size, int_size, float_size;

    parameter_sizes (func_id, e, out addr_size, out int_size, out float_size, out dummy);
    _unused dummy;

    if (x_register_size + (addr_size + int_size) <= 8*8 &&   // max 8 registers X0 to X7
        f_register_size + float_size             <= 8*8)     // max 8 registers D0 to D7
    {
      e^.the_parameter.offset = ofs_reg;      // relative to FP of previous frame, need to add FP displacement of current frame
      ofs_reg += (int_size + float_size + addr_size);

      x_register_size += (addr_size + int_size);
      f_register_size += float_size;
    }
    else
    {
      e^.the_parameter.offset = ofs_stack;      // relative to FP of previous frame, need to add FP displacement of current frame
      ofs_stack += (addr_size + int_size + float_size);

      x_stack_size += (addr_size + int_size);
      f_stack_size += float_size;
    }

    e = e^.next;
    skip_to_next_param (ref e);
  }

  assert ofs_reg == 0;
  assert ofs_stack == x_stack_size + f_stack_size;

  saved_in_registers_size = x_register_size + f_register_size;
  saved_on_stack_size     = x_stack_size + f_stack_size;
}

//======================================================================================

void android_store_first_parameters_on_stack (wstring func_id, PENTITY param)
{
  PENTITY e;
  int     x_reg, f_reg;

  x_reg = 0;
  f_reg = 0;

  e = param;
  skip_to_next_param (ref e);    // skip to entity A_PARAMETER or null
  while (e != null)
  {
    int ofs = e^.the_parameter.offset;

    if (ofs < 0)   // registers must be stored on stack
    {
      int addr_size, int_size, float_size, actual_size;

      parameter_sizes (func_id, e, out addr_size, out int_size, out float_size, out actual_size);

      while (addr_size > 0)
      {
        put_code (P_SAVE_XREG);
        put_int4 (x_reg);
        put_int4 (ofs);
        put_int4 (actual_size);

        addr_size -= 8;
        x_reg++;
        ofs += 8;
      }

      while (int_size > 0)
      {
        put_code (P_SAVE_XREG);
        put_int4 (x_reg);
        put_int4 (ofs);
        put_int4 (actual_size);

        int_size -= 8;
        x_reg++;
        ofs += 8;
      }

      while (float_size > 0)
      {
        put_code (P_SAVE_FREG);
        put_int4 (f_reg);
        put_int4 (ofs);
        put_int4 (actual_size);  // this is important to save single or double

        float_size -= 8;
        f_reg++;
        ofs += 8;
      }
    }

    e = e^.next;
    skip_to_next_param (ref e);    // skip to entity A_PARAMETER or null
  }
}

//======================================================================================

// count number of small variables of each size

void pass1_count_size (int size, ref int8 tcount[5])
{
  if (size == 1)
  {
    tcount[0]++;
  }
  else if (size == 2)
  {
    tcount[1]++;
  }
  else if (size == 4)
  {
    tcount[2]++;
  }
  else if (size == 8)
  {
    tcount[3]++;
  }
  else if (size == 16)
  {
    tcount[4]++;
  }
}

//======================================================================================

void simple_allocate (ref FRAME_INFO frame,
                          int        align,   // 1, 2, 4, 8, 16
                          int        size)
{
  int4 diff;

  frame.frame_offset -= size;

  // do alignment
  diff = (int4)((frame.param_offset - frame.frame_offset) % align);
  if (diff > 0)
    frame.frame_offset -= (align - diff);
}

//======================================================================================

int allocate_local_chunk (ref FRAME_INFO frame,
                              int        align,   // 1, 2, 4, 8, 16
                              int        size)
{
  if (g_target == INTEL)
  {
    simple_allocate (ref frame, align, size);
  }
  else if (g_target == ANDROID)
  {
    /* so how does this work ?
       first, move up frame_offset at M16

       if saved_on_stack_size + frame_offset <= 4096 - 16
         we need to allocate only 1 page of size (saved_on_stack_size + frame_offset + 16), that includes 16 for (old_fp+ret_addr)

       if saved_on_stack_size + frame_offset > 4096 - 16 && saved_on_stack_size + frame_offset < 4096
         that's not possible

       if saved_on_stack_size + frame_offset >= 4096
         allocate a first_page = (4096 - frame.saved_on_stack_size), that includes 16 for (old_fp+ret_addr)
         allocate extra space = frame_offset - first_page
    
       note that frame_offset is negative in the code below
    */

    if (-frame.frame_offset >= 4096 - frame.saved_on_stack_size)   // already more than 1 page
    {
      simple_allocate (ref frame, align, size);   // just extend
    }
    else   // til now, we allocated less than a full page
    {
      simple_allocate (ref frame, align, size);

      if (-frame.frame_offset > 4096 - 16 - frame.saved_on_stack_size)  // we crossed into 16 bytes at page limit, that's not ok
      {
        frame.frame_offset = -(4096 - frame.saved_on_stack_size);   // so fill up first page (incl. 16 bytes)

        simple_allocate (ref frame, align, size);  // and extend
      }
    }
  }
  else
    abort;

  // update minimum_frame_offset
  if (frame.frame_offset < frame.minimum_frame_offset)
    frame.minimum_frame_offset = frame.frame_offset;

  return (int4)frame.frame_offset;
}

//======================================================================================

// for local addresses : compute an address either from one of the 5 reserved aligned areas,
// or an address-aligned block from poffset

int4 pass2_compute_local_offset (    int4       size,
                                     int4       align0,
                                 ref int8       toffset[5],
                                 ref FRAME_INFO frame)
{
  int i;

  if (size == 0)
    return 0;      // address 0[ebp]

  for (i=0; i<5; i++)
  {
    if (size == (1<<i))   // 1, 2, 4, 8 or 16 bytes
    {
      toffset[i] -= size;
      return (int4)toffset[i];
    }
  }

  // any other size
  {
    int4 align;

    // align variables of size 3 at 4-bytes boundaries
    align = max (align0, 4);

    // align variables 5..7 bytes at 8-bytes boundaries
    if (size >= 4)
      align = max (align, 8);

    // align variables >= 64 bytes at 16-bytes boundaries (for fast copy)
    if (size >= 64)
      align = max (align, 16);

    return allocate_local_chunk (ref frame => frame,
                                     align => align,
                                     size  => size);
  }
}

//======================================================================================

void assign_offsets_to_local_variables_of_region
               (    wstring    func_id,
                    PENTITY    e0,
                ref FRAME_INFO frame)
{
  PENTITY e, t;
  int4    size, align, roffset;
  int     rc, pass, i;
  int8    tcount[5];    // number of variables of sizes : 1, 2, 4, 8 and 16.
  int8    toffset[5];   // offsets for sizes            : 1, 2, 4, 8 and 16.

  clear tcount, toffset;

  for (pass=1; pass<=2; pass++)    // pass 1 : collect tcount, pass 2 : assign offsets.
  {
    e = e0;

    while (e != null)
    {
      switch (e^.kind)
      {
        case A_LOCAL_VARIABLE:

          rc = size_and_alignment_of_type (e^.the_local_variable.type, out size, out align);
          if (rc == -2)
          {
            if (pass == 1)
            {
              char str[128];
              sprintf (out str, "%.32S() : local variable %.32S is too large", func_id, e^.identifier_or_null^);
              code_generator_error (str);
            }

            size = 1;
            align = 1;
          }
          else if (rc < 0)  // should not happen in code generator phase
          {
            char str[128];
            sprintf (out str, "codgen.c: func %.32S has bad local var %.32S", func_id, e^.identifier_or_null^);
            fatal_compiler_error0 (str);
          }

          if (size > 4096)    // local variables larger than 4 KB are allocated on heap
          {
            e^.the_local_variable.allocated_on_heap = true;
            size = address_size;
          }

          if (pass == 1)
          {
            pass1_count_size (size, ref tcount);
          }
          else
          {
            roffset = pass2_compute_local_offset (size, align, ref toffset, ref frame);
            e^.the_local_variable.offset = roffset;
          }
          break;


        case A_REFERENCE:

          t = complete_type_of (e^.the_reference.type);

          switch (t^.kind)
          {
            case AN_INTEGER_TYPE:
            case A_FLOAT_TYPE:
            case AN_ENUMERATION_TYPE:
              size = address_size;
              break;

            case AN_OPEN_ARRAY_TYPE:           // has a length
              size = (address_size * 2);    // address + aligned(length4)
              break;

            case A_STRUCT_TYPE:
              size = address_size;
              if (is_open_type (t))         // has a discriminant
                size += address_size;     // aligned(discriminant4)
              break;

            case AN_ARRAY_TYPE:
            case A_CONSTRAINED_STRUCT_TYPE:
            case A_UNION_TYPE:
              size = address_size;
              break;

            case A_POINTER_TYPE:
            case A_FUNCTION_POINTER_TYPE:
            case AN_UNSAFE_POINTER_TYPE:
              size = address_size;
              break;

            default:
            {
              char str[128];
              sprintf (out str, "codgen.c: func %.32S has bad ref %.32S", func_id, e^.identifier_or_null^);
              fatal_compiler_error0 (str);
              size = 4;
            }
            break;
          }

          // if it references a heap object, reserve a tombstone anchor address
          if (exp_requires_tombstone_anchor (e^.the_reference.name) == _YES)
          {
            size += address_size;
            e^.the_reference.references_heap_object = true;
          }

          if (pass == 1)
          {
            pass1_count_size (size, ref tcount);
          }
          else
          {
            align = address_size;
            roffset = pass2_compute_local_offset (size, align, ref toffset, ref frame);
            e^.the_reference.offset = roffset;
          }
          break;

        default:
          break;
      }

      e = e^.next;
    }


    if (pass == 1)   // assign start offsets for variables of small size.
    {
      for (i=0; i<5; i++)
      {
        if (tcount[i] > 0)    // there are (1<<i)-byte variables
        {
          // align offset at (1<<i)
          align = (1 << i);

          toffset[i] = allocate_local_chunk (ref frame => frame,
                                                 align => align,
                                                 size  => (int)tcount[i] * align)
                     + tcount[i] * align;
        }
      }
    }
  }
}

//======================================================================================

// used for tombstone, aggregate, allocator

public int4 allocate_temp_variable (int4 size, ref FRAME_INFO frame)
{
  int4 align;

  if (size == 0)
    return 0;      // address 0[ebp]

  align = 1;
  if (size > 1)
    align = 2;
  if (size > 2)
    align = 4;
  if (size > 4)
    align = 8;
  if (size > 8)
    align = 16;

  return allocate_local_chunk (ref frame => frame,
                                   align => align,
                                   size  => size);
}

//======================================================================================

public int8 allocate_global_variable (int4 size)
{
  int4 align;
  int8 diff, result;

  if (size == 0)
    return 0;

  if (size > 8)       // size 9..?
    align = 16;
  else if (size > 4)  // size 5..8
    align = 8;
  else if (size > 2)  // size 3..4
    align = 4;
  else if (size > 1)  // size 2
    align = 2;
  else                // size 1
    align = 1;

  diff = (int4)(g_global_offset % align);
  if (diff > 0)
    g_global_offset += (align - diff);

  result = g_global_offset;

  g_global_offset += size;

  return result;
}

//======================================================================================

public void add_global_variable_to_list (PENTITY e)
{
  insert_entity_in_list (e, ref used_global_variables);
}

//======================================================================================

void generate_code_to_initialize_global_variables ()
{
  FRAME_INFO   frame;
  ENTITY_LIST^ list;

  clear frame;
  list = used_global_variables;
  while (list != null)
  {
    generate_code_for_global_variable_declaration (list^.e, ref frame);
    list = list^.next;
  }
}

//======================================================================================

public void add_function_to_pending_functions (PENTITY e)
{
  if (e^.the_function_declaration.code_generated)
    return;

  if (first_pending_function == null)
  {
    insert_entity_in_list (e, ref first_pending_function);
    last_pending_function = first_pending_function;
  }
  else
  {
    insert_entity_in_list (e, ref last_pending_function^.next);
    last_pending_function = last_pending_function^.next;
  }

  e^.the_function_declaration.code_generated = true;
}

//======================================================================================

bool entity_needs_release (PENTITY e)
{
  // deallocate all local variables allocated on heap
  if (e^.kind == A_LOCAL_VARIABLE &&
      e^.the_local_variable.allocated_on_heap)
    return true;

  // undereference all references that are heap objects
  if (e^.kind == A_REFERENCE &&
      e^.the_reference.references_heap_object)
    return true;

  return false;
}

//======================================================================================

void generate_code_for_release (PENTITY e)
{
  // deallocate all local variables allocated on heap
  if (e^.kind == A_LOCAL_VARIABLE)
  {
    put_code (P_LOAD_LOCAL);
    put_int4 (e^.the_local_variable.offset);
    put_code (P_VALUE_ADDR);
    put_code (P_FREE);
    put_byte (1);        // bool true : indicates addr is never null
  }

  // undereference all references that are heap objects
  if (e^.kind == A_REFERENCE)
  {
    PENTITY t;
    int     offset;

    t = complete_type_of (e^.the_reference.type);

    offset = e^.the_reference.offset + address_size;
    if (t^.kind == AN_OPEN_ARRAY_TYPE || (t^.kind == A_STRUCT_TYPE && is_open_type (t)))
      offset += address_size;   // length or discriminant rounded up to address size

    // undereference tombstone at local 'offset'
    put_code (P_UNDEREF);
    put_int4 (offset);
    put_int4 (get_new_near_label_nr());
  }
}

//======================================================================================

bool release_entity_list (PENTITY e_list,
                          bool    b_generate_code)
{
  PENTITY e;
  bool    b_code_was_generated = false;

  for (e=e_list; e!=null; e=e^.next)
  {
    if (e^.kind > FIRST_ENTITY_DENOTING_A_STATEMENT)
      break;

    if (entity_needs_release (e))
    {
      b_code_was_generated = true;
      if (b_generate_code)
        generate_code_for_release (e);
    }
  }

  return b_code_was_generated;
}

//======================================================================================

// returns true if code was generated, false otherwise

public
bool release_objects_in_outer_scope (int     k_typ,
                                     PENTITY e_parent,
                                     bool    b_generate_code)
{
  PENTITY e = e_parent;
  bool    b_code_was_generated = false;
  bool    quit = false;

  while (!quit)
  {
    switch (e^.kind)
    {
      case A_FUNCTION_BODY:
        b_code_was_generated |=
         release_entity_list (e^.the_function_body.inner^.entities.first,
                              b_generate_code);
        quit = true;
        break;

      case A_BLOCK_STATEMENT:
        b_code_was_generated |=
          release_entity_list (e^.the_block_statement.inner^.entities.first,
                               b_generate_code);
        e = e^.the_block_statement.outer;
        break;

      case AN_IF_STATEMENT:
        e = e^.the_if_statement.outer;
        break;

      case A_SWITCH_STATEMENT:
        if ((k_typ & K_SWITCH) != 0)
          quit = true;
        else
          e = e^.the_switch_statement.outer;
        break;

      case A_WHILE_STATEMENT:
        if ((k_typ & K_WHILE) != 0)
          quit = true;
        else
          e = e^.the_while_statement.outer;
        break;

      case A_FOR_STATEMENT:
        if ((k_typ & K_FOR) != 0)
          quit = true;
        else
          e = e^.the_for_statement.outer;
        break;

      default:
        fatal_compiler_error0 ("release_objects_in_outer_scope()");
        break;
    }
  }

  return b_code_was_generated;
}

//======================================================================================

// returns true if code after region is unreachable

public
bool generate_code_for_region (    wstring    func_id,
                                   PENTITY    e_parent,
                                   PENTITY    e_list,
                               ref FRAME_INFO frame,
                                   int        outer_label_break,
                                   int        outer_label_continue,
                               ref bool       any_jumps_to_label_break,
                               ref bool       any_jumps_to_label_continue)
{
  PENTITY e;
  int8    saved_frame_offset;
  bool    unreachable = false;

  assign_offsets_to_local_variables_of_region (func_id, e_list, ref frame);

  saved_frame_offset = frame.frame_offset;

  e = e_list;
  while (e != null)
  {
    switch (e^.kind)
    {
      case A_LOCAL_VARIABLE:  // allocate it if on heap, initialize it
        generate_code_for_local_variable_declaration (e, func_id, ref frame);
        break;

      case A_REFERENCE:       // initialize it
        generate_code_for_reference_declaration (e, func_id, ref frame);
        break;

      case A_CLEAR_STATEMENT:
        generate_code_for_clear_statement (e, func_id, ref frame);
        break;

      case AN_ASSIGNMENT_STATEMENT:
        generate_code_for_assignment_statement (e, func_id, ref frame);
        break;

      case A_PRE_OR_POSTFIX_STATEMENT:
        generate_code_for_pre_or_postfix_statement (e, func_id, ref frame);
        break;

      case A_FUNCTION_CALL_STATEMENT:
        generate_code_for_function_call_statement (e, func_id, ref frame);
        break;

      case A_RETURN_STATEMENT:
        generate_code_for_return_statement (e, func_id, ref frame, e_parent);
        unreachable = true;
        break;

      case A_BREAK_STATEMENT:
        release_objects_in_outer_scope (K_FOR | K_WHILE | K_SWITCH, e_parent, true);
        generate_code_for_break_statement (e, func_id, ref frame, outer_label_break);
        any_jumps_to_label_break = true;
        unreachable = true;
        break;

      case A_CONTINUE_STATEMENT:
        release_objects_in_outer_scope (K_FOR | K_WHILE, e_parent, true);
        generate_code_for_continue_statement (e, func_id, ref frame, outer_label_continue);
        any_jumps_to_label_continue = true;
        unreachable = true;
        break;

      case A_FREE_STATEMENT:
        generate_code_for_free_statement (e, func_id, ref frame);
        break;

      case AN_ABORT_STATEMENT:
        unreachable = generate_code_for_abort_statement (e, func_id, ref frame);
        break;

      case AN_ASSERT_STATEMENT:
        unreachable = generate_code_for_assert_statement (e, func_id, ref frame);
        break;

      case A_SLEEP_STATEMENT:
        generate_code_for_sleep_statement (e, func_id, ref frame);
        break;

      case A_CODE_STATEMENT:
        generate_code_for_code_statement (e, func_id, ref frame);
        break;

      case AN_UNUSED_STATEMENT:
        break;

      case A_BLOCK_STATEMENT:
        unreachable = generate_code_for_region
                (    func_id,
                     e,
                     e^.the_block_statement.inner^.entities.first,
                 ref frame,
                     outer_label_break,
                     outer_label_continue,
                 ref any_jumps_to_label_break,
                 ref any_jumps_to_label_continue);
        break;

      case AN_IF_STATEMENT:
        unreachable = generate_code_for_if_statement
                                       (e, func_id, ref frame,
                                        outer_label_break,
                                        outer_label_continue,
                                        ref any_jumps_to_label_break,
                                        ref any_jumps_to_label_continue);
        break;

      case A_SWITCH_STATEMENT:
        generate_code_for_switch_statement (e, func_id, ref frame,
                                            outer_label_continue,
                                            ref any_jumps_to_label_continue);
        break;

      case A_WHILE_STATEMENT:
        unreachable = generate_code_for_while_statement (e, func_id, ref frame);
        break;

      case A_FOR_STATEMENT:
        unreachable = generate_code_for_for_statement (e, func_id, ref frame);
        break;

      default:
        break;
    }

    check_all_xx_stacks_empty ();

    frame.frame_offset = saved_frame_offset;    // reset frame offset

    if (unreachable)
      break;   // don't generate code for the remaining statements

    e = e^.next;
  }

  if (!unreachable)
    release_entity_list (e_list, true);

  return unreachable;
}

//======================================================================================

// the parameter table is used, in the prolog of a function called by windows
// to store the parameters from registers into the shadow zone.

void generate_parameter_table_for_function (    PENTITY func,   // a_function_declaration
                                            out char    parameter_table[4])
{
  PENTITY func_decl;  // the_function_pointer_type

  parameter_table = "....";

  func_decl = func^.the_function_declaration.to_type;

  if (func_decl^.the_function_pointer_type.is_thread_entry_point ||
      func_decl^.the_function_pointer_type.is_callback)
  {
    int     nr;
    PENTITY param;

    nr = 0;
    param = func_decl^.the_function_pointer_type.parameters^.entities.first;
    skip_to_next_param (ref param);    // skip to entity A_PARAMETER or null

    while (param != null && nr < 4)
    {
      PENTITY     type = complete_type_of (param^.the_parameter.type);
      ENTITY_KIND k    = type^.kind;
      char        datatyp;

      if (param^.the_parameter.mode == MODE_IN &&    // parameter of mode 'in' of simple type
          (k == AN_INTEGER_TYPE || k == A_FLOAT_TYPE            || k == AN_ENUMERATION_TYPE ||
           k == A_POINTER_TYPE  || k == A_FUNCTION_POINTER_TYPE || k == AN_UNSAFE_POINTER_TYPE))
      {
        datatyp = code_datatype (type);
      }
      else
      {
        datatyp = 'a';
      }

      parameter_table[nr] = datatyp;

      nr++;
      param = param^.next;
      skip_to_next_param (ref param);    // skip to entity A_PARAMETER or null
    }
  }
}

//======================================================================================

public
void generate_leave_ret (FRAME_INFO frame)
{
  put_code (P_LEAVE);
  put_code (P_RET);
  put_int4 ((int4)frame.param_offset - address_size*2);
}

//======================================================================================

void generate_code_for_function (PENTITY e)
{
  FRAME_INFO frame;
  int8       diff;
  PENTITY    ebody;
  bool       unreachable;
  bool       dummy=false;
  char       parameter_table[4];
  int        saved_in_registers_size=0;  // for android
  int        saved_callee_registers = 0; // for android
  int        flags;

  if (g_tracing)
  {
    char filename[528];
    get_current_source_filename (out filename);
    trace ("\n");
    trace ("generate pcodes for function '%S' (%s)\n", e^.identifier_or_null^, filename);
  }

  clear frame;


  // assign offsets to parameters

  if (g_target == INTEL)
  {
    intel_recursive_assign_offset_to_parameter
         (    e^.identifier_or_null^,
              e^.the_function_declaration.to_type
               ^.the_function_pointer_type.parameters^.entities.first,
              e^.the_function_declaration.to_type
               ^.the_function_pointer_type.is_callback,
          out frame.param_offset);
  }
  else if (g_target == ANDROID)
  {
    android_assign_offset_to_parameters
         (    e^.identifier_or_null^,
              e^.the_function_declaration.to_type
               ^.the_function_pointer_type.parameters^.entities.first,
          out saved_in_registers_size,
          out frame.saved_on_stack_size);

    // align up to M16
    frame.saved_on_stack_size = (frame.saved_on_stack_size + (int)STACK_ALIGNMENT - 1) & -(int)STACK_ALIGNMENT;

//    frame.param_offset = 0;  // alignment exactly on SP during call
  }
  else
    abort;

  ebody = e^.the_function_declaration.to_function_body_or_null;
  generate_p_location (ebody^.the_function_body.begin_loc);

  put_code (P_FUNC_LABEL);
  put_int4 (e^.the_function_declaration.func_label_nr);

  if (g_target == INTEL)
    generate_parameter_table_for_function (e, out parameter_table);
  else
    parameter_table = "....";

  flags = (int)e^.the_function_declaration.to_type^.the_function_pointer_type.is_thread_entry_point
            + 2*(int)e^.the_function_declaration.to_type^.the_function_pointer_type.is_callback
            + 4*(int)e^.the_function_declaration.to_type^.the_function_pointer_type.is_entry;

  put_code (P_ENTER);
  put_int4 (0);    // placeholder to be filled later
  put_int4 (0);    // placeholder to be filled later
  put_int4 (0);    // placeholder to be filled later
  put_int4 (0);    // placeholder to be filled later
  put_int4 (flags);
  put_byte ((byte)parameter_table[0]); put_byte ((byte)parameter_table[1]);
  put_byte ((byte)parameter_table[2]); put_byte ((byte)parameter_table[3]);

  if (g_target == ANDROID)
  {
    if (ebody^.the_function_body.inner^.entities.first != null &&
        ebody^.the_function_body.inner^.entities.first^.kind == A_CODE_STATEMENT)
    {
      // code statement as function body : don't load parameters
    }
    else
    {
      android_store_first_parameters_on_stack (e^.identifier_or_null^,
                                               e^.the_function_declaration.to_type
                                                ^.the_function_pointer_type.parameters^.entities.first);
    }

    (void)allocate_local_chunk (ref frame => frame,
                                    align => 1,
                                    size  => saved_in_registers_size);

    if (flags != 0)
    {
      saved_callee_registers = allocate_local_chunk (ref frame => frame,
                                                         align => 16,
                                                         size  => 4*8);  // 4 registers : X19-X22
    }
  }

  unreachable = generate_code_for_region
                           (    e^.identifier_or_null^,
                                ebody,
                                ebody^.the_function_body.inner^.entities.first,
                            ref frame,
                                0,
                                0,
                            ref dummy,
                            ref dummy);

  if ((!unreachable) &&
      e^.the_function_declaration.to_type
       ^.the_function_pointer_type.return_type == type_void)
  {
    generate_p_location (ebody^.the_function_body.end_loc);
    put_code (P_RETURN_VOID);
    generate_leave_ret (frame);
  }


  // adjust frame size to keep ESP aligned from frame to frame

  if (address_size == 4)  // align using parameters + locals
  {
    diff = (frame.param_offset - frame.minimum_frame_offset) % STACK_ALIGNMENT;
    if (diff > 0)
      diff = (STACK_ALIGNMENT - diff);
    frame.minimum_frame_offset -= diff;
  }
  else  // 64-bit   - align using locals only (parameters are aligned by caller)
  {
    diff = (- frame.minimum_frame_offset) % STACK_ALIGNMENT;
    if (diff > 0)
      diff = (STACK_ALIGNMENT - diff);
    frame.minimum_frame_offset -= diff;   // negative or zero
  }

  // patch P_ENTER.

  if (g_target == INTEL)
  {
    codout_fix_enter (size                  => (uint)-(int)frame.minimum_frame_offset,
                      freespace             => (uint)diff,
                      block                 => STACK_ALIGNMENT,
                      callee_registers_addr => saved_callee_registers);
  }
  else if (g_target == ANDROID)
  {
    codout_fix_enter (size                  => (uint)-(int)frame.minimum_frame_offset,
                      freespace             => (uint)frame.saved_on_stack_size,  // total size of parameters saved on stack
                      block                 => STACK_ALIGNMENT,
                      callee_registers_addr => saved_callee_registers);
  }

  if (frame.minimum_frame_offset < -2147483647 + 1024)
  {
    char str[128];
    sprintf (out str, "function %.32S has too many/too large local variables", e^.identifier_or_null^);
    code_generator_error (str);
  }

  e^.the_function_declaration.frame_size = (int4)frame.minimum_frame_offset;  // negative value

  if (frame.currently_pushed_offset != 0)
    fatal_compiler_error0 ("pushed_not_zero");
}

//======================================================================================

public
void generate_code (PENTITY                 func,
                    GENERATE_ASM_FOR_PCODES generate_asm_for_pcodes)
{
  g_all_full_types_are_visible = true;
  init_heap_type_btree ();

  // all the application's functions, starting with entry point 'func'

  if (func^.the_function_declaration.func_label_nr == 0)
    func^.the_function_declaration.func_label_nr = get_new_func_label_nr();

  // add function in list if no code was generated yet for it
  add_function_to_pending_functions (func);

  while (first_pending_function != null)
  {
    PENTITY      e;
    ENTITY_LIST^ t;

    t = first_pending_function;
    e = t^.e;
    first_pending_function = first_pending_function^.next;
    if (first_pending_function == null)
      last_pending_function = null;
    free (t);

    error_set_code_generator_unit_key
      (e^.the_function_declaration.to_function_body_or_null
        ^.the_function_body.begin_loc.unit_key);

    near_label_nr = 0;
    codout_reset_pos ();
    reset_pool_backfills ();

    generate_code_for_function (e);

    generate_asm_for_pcodes (is_main   => e == g_func_main,
                             nb_labels => near_label_nr + 1);
  }
}

//======================================================================================

public void generate_code_to_init_global_variables (int                     func_label_to_init_constants,
                                                    GENERATE_ASM_FOR_PCODES generate_asm_for_pcodes,
                                                    int                     stack_size)   // for ANDROID
{
  if (g_tracing)
  {
    trace ("\n");
    trace ("Generate code for init global variables\n");
  }

  near_label_nr = 0;
  codout_reset_pos ();
  reset_pool_backfills ();

  put_code (P_FUNC_LABEL);
  put_int4 (func_label_to_init_constants);

  put_code (P_ENTER);
  put_int4 (0);    // placeholder to be filled later
  put_int4 (0);    // placeholder to be filled later
  put_int4 (0);    // placeholder to be filled later
  put_int4 (0);    // callee save registers
  put_int4 (0);    // flags (is not thread entry point or callback)
  put_byte ((byte)'.'); put_byte ((byte)'.'); put_byte ((byte)'.'); put_byte ((byte)'.');

  // patch P_ENTER.
  codout_fix_enter (0, 0, STACK_ALIGNMENT, 0);

  generate_code_to_initialize_global_variables ();

  put_code (P_INIT_THREADS);
  put_int4 (stack_size);

  put_code (P_RETURN_VOID);
  put_code (P_LEAVE);
  put_code (P_RET);
  put_int4 (0);

  generate_asm_for_pcodes (is_main   => false,
                           nb_labels => near_label_nr + 1);


  // allocate 8 extra BSS bytes so that pushing quadword globals does never cause read error at end of bss section
  if (address_size == 8)
    (void)allocate_global_variable (8);
}

//======================================================================================

public void generate_p_location (LOCATION l)
{
  put_code (P_LOCATION);
  put_int4 (l.unit_key);
  put_int4 (l.source_line);

  error_set_code_generator_unit_key (l.unit_key);
  error_set_code_generator_source_line (l.source_line);
}

//======================================================================================
