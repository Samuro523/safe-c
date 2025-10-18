
// type.c : type category tests

from std use arithm;
use ../goptions, entities;

/*****************************************************************************/

// resolves renamed, incomplete, opaque and generic types when their full type is visible.
// returns null if t is null.

public
PENTITY complete_type_of (PENTITY t)
{
  PENTITY p;

  if (t == null)    // error already given
    return null;

  p = t;

  for (;;)
  {
    if (p^.kind == A_RENAMED_TYPE)
    {
      p = p^.the_renamed_type.actual_type;
    }
    else if (p^.kind == AN_INCOMPLETE_TYPE &&
             p^.the_incomplete_type.full_type != null &&
             (p^.the_incomplete_type.is_full_type_visible || g_all_full_types_are_visible))
    {
      p = p^.the_incomplete_type.full_type;
    }
    else if (p^.kind == AN_OPAQUE_TYPE &&
             p^.the_opaque_type.full_type != null &&
             (p^.the_opaque_type.is_full_type_visible || g_all_full_types_are_visible))
    {
      p = p^.the_opaque_type.full_type;
    }
    else if (p^.kind == A_GENERIC_TYPE &&
             p^.the_generic_type.actual_type != null)
    {
      p = p^.the_generic_type.actual_type;
    }
    else
    {
      break;
    }
  }

  return p;
}

//=================================================================================

// same as complete_type_of() but all full types are visible

PENTITY override_complete_type_of (PENTITY t)
{
  bool    save_override;
  PENTITY t2;

  save_override = g_all_full_types_are_visible;   // save flag

  g_all_full_types_are_visible = true;

  t2 = complete_type_of (t);

  g_all_full_types_are_visible = save_override;   // restore flag

  return t2;
}

//=================================================================================

public
bool is_open_type (PENTITY t)
{
  PENTITY p;

  p = complete_type_of (t);

  return (p^.kind == AN_OPEN_ARRAY_TYPE ||
          (p^.kind == A_STRUCT_TYPE && p^.the_struct_type.is_open_type));
}

//=================================================================================

PENTITY renamed_type_of (PENTITY t)
{
  PENTITY p;

  p = t;

  while (p^.kind == A_RENAMED_TYPE)
    p = p^.the_renamed_type.actual_type;

  return p;
}

//=================================================================================

// a jagged type is an array or struct type where any inner element/field type is
// an open type.

// recursive types can be introduced by :
// 1) incomplete types
// 2) opaque types
// 3) generic types?
// 4) self-recursive open structs

// note: a jagged type can be open or non-open.

public
bool is_jagged_type (PENTITY t)
{
  PENTITY p, e, elem_type;


  // handle only renamed types, and no forward reference types !
  // -> leave incomplete/opaque/generic types "as is" (they are never jagged)

  p = renamed_type_of (t);


  if (p^.kind == AN_ARRAY_TYPE)
  {
    return is_jagged_type (p^.the_array_type.open_array);
  }
  else if (p^.kind == A_CONSTRAINED_STRUCT_TYPE)
  {
    return is_jagged_type (p^.the_constrained_struct_type.open_struct);
  }
  else if (p^.kind == AN_OPEN_ARRAY_TYPE)
  {
    elem_type = p^.the_open_array_type.element;
    return is_open_type (elem_type) || is_jagged_type (elem_type);
  }
  else if (p^.kind == A_STRUCT_TYPE)
  {
    e = p^.the_struct_type.fields^.entities.first;
    while (e != null)
    {
      if (e^.kind == A_FIELD)
      {
        // risk of circular references :
        // handle only renamed types, and no forward reference types !
        // -> leave incomplete/opaque/generic types "as is" (they are never jagged)

        elem_type = renamed_type_of (e^.the_field.type);
        if (elem_type == p ||          // self-reference : avoid infinite recursion
            is_open_type (elem_type) ||
            is_jagged_type (elem_type))
          return true;
      }
      else if (e^.kind == A_VARYING_FIELD)
      {
        // risk of circular references :
        // handle only renamed types, and no forward reference types !
        // -> leave incomplete/opaque/generic types "as is" (they are never jagged)

        elem_type = renamed_type_of (e^.the_varying_field.type);
        if (elem_type == p ||          // self-reference : avoid infinite recursion
            is_open_type (elem_type) ||
            is_jagged_type (elem_type))
          return true;
      }

      e = e^.next;
    }

    return false;
  }
  else
  {
    return false;
  }
}

//=================================================================================

public
bool is_limited_type (PENTITY t)
{
  PENTITY p, e, elem_type;


  p = complete_type_of (t);


  if (p^.kind == AN_ARRAY_TYPE)
  {
    return is_limited_type (p^.the_array_type.open_array);
  }
  else if (p^.kind == A_CONSTRAINED_STRUCT_TYPE)
  {
    return is_limited_type (p^.the_constrained_struct_type.open_struct);
  }
  else if (p^.kind == AN_OPEN_ARRAY_TYPE)
  {
    return is_limited_type (p^.the_open_array_type.element);
  }
  else if (p^.kind == A_STRUCT_TYPE)
  {
    e = p^.the_struct_type.fields^.entities.first;
    while (e != null)
    {
      if (e^.kind == A_FIELD)
      {
        elem_type = complete_type_of (e^.the_field.type);
        if (is_limited_type (elem_type))
          return true;
      }
      else if (e^.kind == A_VARYING_FIELD)
      {
        elem_type = complete_type_of (e^.the_varying_field.type);
        if (is_limited_type (elem_type))
          return true;
      }

      e = e^.next;
    }

    return false;
  }
  else if (p^.kind == AN_OPAQUE_TYPE)
  {
    return p^.the_opaque_type.is_limited;
  }
  else
  {
    return false;
  }
}

//=================================================================================

// A packed struct can only contain fields of packed types.
// Enumeration, integer, floating-point and unsafe pointer types are always packed.
// Array types are packed if the element type is packed, non-open and non-jagged.
// Struct types are packed if they feature the keyword "packed";
//  all their fields must then have packed, non-open and non-jagged types.
// Pointer, function pointer, opaque, incomplete, generic and jagged types are never packed.

// note: a packed type can be open or non-open.

public
bool is_packed_type (PENTITY type)
{
  PENTITY t;

  t = complete_type_of (type);

  if (t == null)
    return false;

  if (t^.kind == AN_INTEGER_TYPE || t^.kind == A_FLOAT_TYPE || t^.kind == AN_ENUMERATION_TYPE ||
      t^.kind == AN_UNSAFE_POINTER_TYPE || t^.kind == A_UNION_TYPE)
    return true;

  if (t^.kind == AN_ARRAY_TYPE)
    return is_packed_type (t^.the_array_type.open_array);

  if (t^.kind == A_CONSTRAINED_STRUCT_TYPE)
    return is_packed_type (t^.the_constrained_struct_type.open_struct);

  if (t^.kind == AN_OPEN_ARRAY_TYPE)
  {
    PENTITY elem_type;
    elem_type = t^.the_open_array_type.element;
    return is_packed_type (elem_type) && (!is_open_type (elem_type)) && (!is_jagged_type (elem_type));
  }

  if (t^.kind == A_STRUCT_TYPE)
    return t^.the_struct_type.is_packed;

  return false;
}

//=================================================================================

public
bool is_unsafe_type (PENTITY type)
{
  PENTITY t;

  t = complete_type_of (type);

  if (t == null)
    return false;

  if (t^.kind == AN_ARRAY_TYPE)
    return is_unsafe_type (t^.the_array_type.open_array);

  if (t^.kind == A_CONSTRAINED_STRUCT_TYPE)
    return is_unsafe_type (t^.the_constrained_struct_type.open_struct);

  if (t^.kind == AN_UNSAFE_POINTER_TYPE)
    return true;

  if (t^.kind == AN_OPEN_ARRAY_TYPE)
    return is_unsafe_type (t^.the_open_array_type.element);

  if (t^.kind == A_STRUCT_TYPE)
    return t^.the_struct_type.is_unsafe;

  if (t^.kind == A_UNION_TYPE)
    return t^.the_union_type.is_unsafe;

  return false;
}

//=================================================================================

public
bool is_constant_exp (PEXPRESSION e)
{
  return e^.kind == A_CONST_ENUMERATION_VALUE ||
         e^.kind == A_CONST_INTEGER_VALUE ||
         e^.kind == A_CONST_FLOAT_VALUE ||
         e^.kind == A_CONST_NULL_VALUE ||
         e^.kind == A_POOL_CONSTANT;
}

/*****************************************************************************/

public
PENTITY base_type_of (PENTITY type)
{
  PENTITY t;

  t = complete_type_of (type);

  if (t == null)
    return null;

  if (t^.kind == AN_ARRAY_TYPE)
  {
    t = complete_type_of (t^.the_array_type.open_array);
  }
  else if (t^.kind == A_CONSTRAINED_STRUCT_TYPE)
  {
    t = complete_type_of (t^.the_constrained_struct_type.open_struct);
  }

  return t;
}

/*****************************************************************************/

// returns 0 if OK or -1 if overflow

int add_int4 (int4 a, int4 b, out int4 r)
{
  int4 c;

  c = a + b;

  if (((a < 0) ^ (b >= 0)) && ((c < 0) ^ (a < 0)))
  {
    r = 0;
    return -1;
  }

  r = c;
  return 0;
}

/***************************************************************************/

// returns 0 if OK or -1 if overflow

public
int mul_int4 (int4 a, int4 b, out int4 r)
{
  int4 c;

  c = a * b;

  if (b != 0 && (c / b) != a)
  {
    r = 0;
    return -1;
  }

  r = c;
  return 0;
}

/***************************************************************************/

// returns 0 if OK or -1 if overflow

int align_at (ref int psize, int align)
{
  int offset;

  offset = psize % align;

  if (offset > 0)
  {
    if (add_int4 (psize, align-offset, out psize) < 0)
      return -1;
  }

  return 0;
}

/***************************************************************************/

public
void begin_struct (out FIELD_DATA data,
                       PENTITY    stru)      // struct or union type entity
{
  PENTITY ot;

  ot = complete_type_of (stru);

  clear data;
  data.ispacked = ot^.kind != A_STRUCT_TYPE || ot^.the_struct_type.is_packed;
  data.struct_alignment = 1;
}

/***************************************************************************/

public
void skip_to_valid_field (ref PENTITY pfield,
                          out PENTITY ptype,
                              uint4   discriminant_value)
{
  PENTITY e;

  ptype = type_int;

  e = pfield;

  while (e != null)
  {
    if (e^.kind == A_FIELD)
    {
      ptype = e^.the_field.type;
      break;
    }

    if (e^.kind == A_VARYING_FIELD && e^.the_varying_field.discriminant_value == discriminant_value)
    {
      ptype = e^.the_varying_field.type;
      break;
    }

    e = e^.next;
  }

  pfield = e;
}

/*****************************************************************************/

public
void begin_field (ref FIELD_DATA data,
                      PENTITY    field)      // field or varying field
{
  PENTITY t;
  int     align, rc;

  if (field^.kind == A_FIELD)
  {
    t = field^.the_field.type;
  }
  else
  {
    t = field^.the_varying_field.type;
  }

  rc = size_and_alignment_of_type (t, out data.field_size, out align);

  if (rc == -1)
    data.invalid_type = true;
  else if (rc == -2)
    data.size_overflow = true;
  else
  {
    if (!data.ispacked)
    {
      if (align_at (ref data.struct_size, align) < 0)
        data.size_overflow = true;

      data.struct_alignment = max (data.struct_alignment, align);
    }
  }

  data.field_offset = data.struct_size;
}

/***************************************************************************/

public
void end_field (ref FIELD_DATA data)
{
  if (add_int4 (data.struct_size, data.field_size, out data.struct_size) < 0)
    data.size_overflow = true;
}

/***************************************************************************/

public
void end_struct (ref FIELD_DATA data)
{
  if (!data.ispacked)
  {
    if (align_at (ref data.struct_size, data.struct_alignment) < 0)
      data.size_overflow = true;
  }
}

/*****************************************************************************/

// returns 0 if OK, -1 if bad type, -2 if size is too large

public
int size_and_alignment_of_type (PENTITY type, out int psize, out int alignment)
{
  PENTITY t, field, ot;
  int     fsize, falign, rc;
  uint4   dis, nb_elem;

  if (type == null)
  {
    psize = 0;
    alignment = 1;
    return -1;
  }

  t = complete_type_of (type);

  switch (t^.kind)
  {
    case AN_INTEGER_TYPE:
      psize = INTEGER_DATA[(uint)t^.the_integer_type.type].size;
      alignment = psize;
      break;

    case A_FLOAT_TYPE:
      psize = FLOAT_DATA[(uint)t^.the_float_type.type].size;
      alignment = psize;
      break;

    case AN_ENUMERATION_TYPE:
      psize = INTEGER_DATA[(uint)t^.the_enumeration_type.base].size;
      alignment = psize;
      break;

    case AN_OPEN_ARRAY_TYPE:         // open type
      psize = address_size + 4;      // address(4 or 8) + length(int4)
      alignment = address_size;
      align_at (ref psize, alignment);   // possibly 4 filler bytes
      break;

    case AN_ARRAY_TYPE:
      ot = complete_type_of (t^.the_array_type.open_array);
      nb_elem = t^.the_array_type.length;
      rc = size_and_alignment_of_type (ot^.the_open_array_type.element, out fsize, out falign);
      if (rc < 0)
      {
        psize = 0;
        alignment = 1;
        return rc;
      }

      alignment = 1;

      if (mul_int4 (fsize, (int)nb_elem, out psize) < 0)
        return -2;

      alignment = falign;
      break;

    case A_STRUCT_TYPE:       // can be open, can be packed
      if (t^.the_struct_type.is_open_type)   // open (never packed)
      {
        psize = address_size + 4;      // address(4 or 8) + discriminant(int4)
        alignment = address_size;
        align_at (ref psize, alignment);   // possibly 4 filler bytes
      }
      else
      {
        FIELD_DATA data;

        begin_struct (out data, t);

        field = t^.the_struct_type.fields^.entities.first;
        while (field != null)
        {
          if (field^.kind == A_FIELD)
          {
            begin_field (ref data, field);
            end_field (ref data);
          }

          field = field^.next;
        }

        end_struct (ref data);

        psize = data.struct_size;
        alignment = data.struct_alignment;

        if (data.invalid_type)
          return -1;
        if (data.size_overflow)
          return -2;
      }
      break;

    case A_CONSTRAINED_STRUCT_TYPE:
      ot = complete_type_of (t^.the_constrained_struct_type.open_struct);
      dis = t^.the_constrained_struct_type.discriminant_value;

      {
        FIELD_DATA data;

        begin_struct (out data, ot);

        field = ot^.the_struct_type.fields^.entities.first;
        while (field != null)
        {
          if (field^.kind == A_FIELD ||
              (field^.kind == A_VARYING_FIELD &&
                field^.the_varying_field.discriminant_value == dis))
          {
            begin_field (ref data, field);
            end_field (ref data);
          }

          field = field^.next;
        }

        end_struct (ref data);

        psize = data.struct_size;
        alignment = data.struct_alignment;

        if (data.invalid_type)
          return -1;
        if (data.size_overflow)
          return -2;
      }
      break;

    case A_UNION_TYPE:
      psize = 0;
      alignment = 1;
      field = t^.the_union_type.fields^.entities.first;
      while (field != null)
      {
        if (field^.kind == A_FIELD)
        {
          if (size_and_alignment_of_type (field^.the_field.type, out fsize, out falign) < 0)
            return -1;

          if (fsize > psize)
            psize = fsize;
          if (falign > alignment)
            alignment = falign;
        }

        field = field^.next;
      }
      break;

    case A_POINTER_TYPE:
    case A_FUNCTION_POINTER_TYPE:
    case AN_UNSAFE_POINTER_TYPE:
    case A_NULL_POINTER_TYPE:  // only used internally for 'null' literal expressions
      psize = address_size;
      alignment = psize;
      break;

/*
    case A_RENAMED_TYPE:
    case AN_INCOMPLETE_TYPE:
    case AN_OPAQUE_TYPE:
    case A_GENERIC_TYPE:
*/
    default:
      psize = 0;
      alignment = 1;
      return -1;
  }

  return 0;
}

/*****************************************************************************/

// check if type is not renamed/incomplete/generic/opaque

public
bool is_type_having_size (PENTITY type)
{
  int rc, size, align;

  // returns 0 if OK, -1 if bad type, -2 if size is too large
  rc = size_and_alignment_of_type (type, out size, out align);

  _unused size;
  _unused align;

  return (rc == 0);
}

/*****************************************************************************/

// returns 0 if OK, -1 if bad type, -2 if size is too large, -3 if other constraint
// runtime constraints are allowed but they return a jagged pointer + constraint size

public
int size_and_alignment_of_constant_size_exp (PEXPRESSION exp, out int psize, out int alignment)
{
  PENTITY t, field;
  int     fsize, falign, rc;
  uint4   dis, nb_elem;

  t = exp^.base_type_or_null;

  if (t == null)
  {
    psize = 0;
    alignment = 1;
    return -1;
  }

  t = complete_type_of (t);

  switch (t^.kind)
  {
    case AN_OPEN_ARRAY_TYPE:        // open type (address + length)
      if (exp^.constraint.kind == CONSTANT_CONSTRAINT)
      {
        psize = 0;
        alignment = 1;

        nb_elem = exp^.constraint.value;
        rc = size_and_alignment_of_type (t^.the_open_array_type.element, out fsize, out falign);
        if (rc < 0)
          return rc;

        if (mul_int4 (fsize, (int)nb_elem, out psize) < 0)
          return -2;

        alignment = falign;
      }
      else if (exp^.constraint.kind == RUNTIME_CONSTRAINT)
      {
        return size_and_alignment_of_type (t, out psize, out alignment);
      }
      else
      {
        psize = 0;
        alignment = 1;
        return -3;  // other constraint
      }
      break;

    case A_STRUCT_TYPE:       // can be open, can be packed

      if (!t^.the_struct_type.is_open_type)   // not open
        return size_and_alignment_of_type (t, out psize, out alignment);

      if (exp^.constraint.kind == CONSTANT_CONSTRAINT)
      {
        dis = exp^.constraint.value;

        {
          FIELD_DATA data;

          begin_struct (out data, t);

          field = t^.the_struct_type.fields^.entities.first;
          while (field != null)
          {
            if (field^.kind == A_FIELD ||
                (field^.kind == A_VARYING_FIELD &&
                  field^.the_varying_field.discriminant_value == dis))
            {
              begin_field (ref data, field);
              end_field (ref data);
            }

            field = field^.next;
          }

          end_struct (ref data);

          psize = data.struct_size;
          alignment = data.struct_alignment;

          if (data.invalid_type)
            return -1;
          if (data.size_overflow)
            return -2;
        }
      }
      else if (exp^.constraint.kind == RUNTIME_CONSTRAINT)
      {
        return size_and_alignment_of_type (t, out psize, out alignment);
      }
      else
      {
        psize = 0;
        alignment = 1;
        return -3;  // other constraint
      }
      break;

    default:
      return size_and_alignment_of_type (t, out psize, out alignment);
  }

  return 0;
}

/*****************************************************************************/

package CIRCULAR

  struct CIRCULAR_NODE
  {
    PENTITY        e;
    CIRCULAR_NODE^ next;
  }

  typedef CIRCULAR_NODE^ CIRCULAR_LIST;

end CIRCULAR;

/*****************************************************************************/

bool exists_in_list (PENTITY e, CIRCULAR_LIST l)
{
  CIRCULAR_LIST n = l;
  while (n != null)
  {
    if (n^.e == e)
      return true;
    n = n^.next;
  }

  return false;
}

/*****************************************************************************/

// test all of type t's components to see if any is present

bool exists_in_type (    PENTITY       t,            // any field type to be tested for circularities
                         bool          in_variant,
                         CIRCULAR_LIST head,         // list of forbiden types we must not find
                     ref CIRCULAR_LIST ptail)        // last element of list
{
  PENTITY t2, f;

  t2 = override_complete_type_of (t);       // full field type to be tested for circularities

  switch (t2^.kind)
  {
    case AN_ARRAY_TYPE:
      return exists_in_type (t2^.the_array_type.open_array, in_variant, head, ref ptail);

    case AN_OPEN_ARRAY_TYPE:
      return exists_in_type (t2^.the_open_array_type.element, in_variant, head, ref ptail);

    case A_STRUCT_TYPE:
      if (in_variant && t2^.the_struct_type.is_open_type)  // allowed anyway only for jagged constants
        return false;

      if (exists_in_list (t2, head))    // found in list -> circularity
        return true;

      ptail = new CIRCULAR_NODE ' {t2, null};

      f = t2^.the_struct_type.fields^.entities.first;
      while (f != null)
      {
        if (f^.kind == A_FIELD)
        {
          if (exists_in_type (f^.the_field.type, false, head, ref ptail^.next))
          {
            free ptail;
            ptail = null;
            return true;
          }
        }
        else if (f^.kind == A_VARYING_FIELD)
        {
          if (exists_in_type (f^.the_varying_field.type, true, head, ref ptail^.next))
          {
            free ptail;
            ptail = null;
            return true;
          }
        }

        f = f^.next;
      }

      free ptail;
      ptail = null;

      return false;

    case A_CONSTRAINED_STRUCT_TYPE:    // force in_variant=false
      return exists_in_type (t2^.the_constrained_struct_type.open_struct, false, head, ref ptail);

    case A_UNION_TYPE:
      if (exists_in_list (t2, head))    // union type to be tested found in list -> circularity
        return true;

      ptail = new CIRCULAR_NODE ' {t2, null};

      f = t2^.the_union_type.fields^.entities.first;
      while (f != null)
      {
        if (f^.kind == A_FIELD)
        {
          if (exists_in_type (f^.the_field.type, false, head, ref ptail^.next))
          {
            free ptail;
            ptail = null;
            return true;
          }
        }

        f = f^.next;
      }

      free ptail;
      ptail = null;

      return false;

    default:
      return false;   // suppose no circularities for RENAMED, INCOMPLETE, OPAQUE, GENERIC.
  }
}

/*****************************************************************************/

// called in analysis phase for each field of a struct or union.
// test if a field or field's component contains the surrounding struct or union type

public
bool type_has_circular_dependences (PENTITY s,         // struct or union type
                                    PENTITY type,      // any field type to be tested
                                    bool    in_variant)
{
  CIRCULAR_LIST list;
  bool          b;

  list = new CIRCULAR_NODE ' {override_complete_type_of (s), null};

  b = exists_in_type (type, in_variant, list, ref list^.next);

  free (list);

  return b;
}

/*****************************************************************************/
