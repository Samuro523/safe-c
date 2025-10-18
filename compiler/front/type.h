
// type.h : type category tests

use entities;

/***************************************************************************/

bool is_open_type    (PENTITY t);
bool is_jagged_type  (PENTITY t);
bool is_packed_type  (PENTITY type);
bool is_limited_type (PENTITY t);
bool is_unsafe_type  (PENTITY type);

/***************************************************************************/

bool g_all_full_types_are_visible;   // set to true by code generator

/***************************************************************************/

// resolves renamed, incomplete and opaque types when their full type is visible.
// returns null if t is null.
PENTITY complete_type_of (PENTITY t);

bool is_constant_exp (PEXPRESSION e);

PENTITY base_type_of (PENTITY type);

int mul_int4 (int4 a, int4 b, out int4 r);

// check if type is not renamed/incomplete/generic/opaque
bool is_type_having_size (PENTITY type);

// returns 0 if OK, -1 if bad type, -2 if size is too large
int size_and_alignment_of_type (PENTITY type, out int psize, out int alignment);

// returns 0 if OK, -1 if bad type, -2 if size is too large, -3 if other constraint
// runtime constraints are allowed but they return a jagged pointer + constraint size
int size_and_alignment_of_constant_size_exp (PEXPRESSION exp, out int psize, out int alignment);

/***************************************************************************/

struct FIELD_DATA
{
  bool ispacked;

  int  field_offset;
  int  field_size;

  int  struct_size;
  int  struct_alignment;

  bool invalid_type;
  bool size_overflow;
}

void begin_struct (out FIELD_DATA data,
                       PENTITY    stru);     // struct_type entity

void skip_to_valid_field (ref PENTITY pfield,
                          out PENTITY ptype,
                              uint4 discriminant_value);

void begin_field (ref FIELD_DATA data,
                      PENTITY    field);   // field or varying field

void end_field (ref FIELD_DATA data);

void end_struct (ref FIELD_DATA data);

/***************************************************************************/

bool type_has_circular_dependences (PENTITY s,         // struct or union
                                    PENTITY type,      // field's type to be tested
                                    bool    in_variant);

/***************************************************************************/
