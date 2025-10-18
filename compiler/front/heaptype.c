
// heaptype.c

from std use arithm, bintree, strings;
use entities, type, ../error;

/*****************************************************************************/

struct NODE
{
  int4    unique_type;
  int8[]^ signature;
}

/*****************************************************************************/

package P = new BALANCED_BINARY_TREE (ELEMENT => NODE, USER_INFO => bool);

BINARY_TREE heap_type_btree;
int4        unique_type_serial_nr;

/*****************************************************************************/

int compare (bool^ user, NODE a, NODE b)
{
  int len = min (a.signature^'length, b.signature^'length);
  int rc;

  _unused user;

  rc = memcmp (a.signature^[0 : len], b.signature^[0 : len]);
  if (rc != 0)
    return rc;

  if (a.signature^'length > b.signature^'length)
    return +1;
  if (a.signature^'length < b.signature^'length)
    return -1;
  return 0;
}

/**********************************************************************************/

// signature format :
//
// int8   entity nr
// uint4  AN_ARRAY_TYPE,
// uint4  A_CONSTRAINED_STRUCT_TYPE,
// [ (-1) AN_OPEN_ARRAY_TYPE,
// ^ (-2) A_POINTER_TYPE,
// * (-3) AN_UNSAFE_POINTER_TYPE,
//   (-4) A_FUNCTION_POINTER_TYPE
//        . callback(y/n)
//        . extern_dll(y/n)
//        . is_entry(y/n)
//        . return type
//        . { (-5) A_PARAMETER
//                 . mode (in/out/ref)
//                 . type
//          }
//   (-6) END_FUNCTION

/*****************************************************************************/

int signature_length (PENTITY subtype)
{
  PENTITY     e;
  ENTITY_KIND kind;
  int         count;

  e = complete_type_of (subtype);
  kind = e^.kind;

  if (kind == AN_INTEGER_TYPE || kind == A_FLOAT_TYPE || kind == AN_ENUMERATION_TYPE ||
      kind == A_STRUCT_TYPE   || kind == A_UNION_TYPE || kind == A_VOID_TYPE)
  {
    return 1;
  }

  if (kind == AN_ARRAY_TYPE)
    return 1 + signature_length (e^.the_array_type.open_array);

  if (kind == A_CONSTRAINED_STRUCT_TYPE)
    return 1 + signature_length (e^.the_constrained_struct_type.open_struct);

  if (kind == AN_OPEN_ARRAY_TYPE)
    return 1 + signature_length (e^.the_open_array_type.element);

  if (kind == A_POINTER_TYPE)
    return 1 + signature_length (e^.the_pointer_type.designated_type);

  if (kind == AN_UNSAFE_POINTER_TYPE)
    return 1 + signature_length (e^.the_unsafe_pointer_type.designated_type);

  if (kind != A_FUNCTION_POINTER_TYPE)
    fatal_compiler_error0 ("signature_length(1)");

  count = 1 + 1 + 1 + 1    // -4 + callback(y/n) + extern_dll(y/n) + is_entry(y/n)
        + signature_length(e^.the_function_pointer_type.return_type);  // + return_type

  e = e^.the_function_pointer_type.parameters^.entities.first;
  while (e != null)
  {
    if (e^.kind == A_PARAMETER)
    {
      // -5(PAR), mode, type
      count += (1 + 1 + signature_length(e^.the_parameter.type));
    }
    e = e^.next;
  }

  count++;     // -6 (END_FUNC)

  return count;
}

/*****************************************************************************/

int fill_signature (PENTITY subtype, out int8 signature[])
{
  PENTITY     e;
  ENTITY_KIND kind;
  int         index;

  clear signature;
  
  e = complete_type_of (subtype);
  kind = e^.kind;

  if (kind == AN_INTEGER_TYPE || kind == A_FLOAT_TYPE || kind == AN_ENUMERATION_TYPE ||
      kind == A_STRUCT_TYPE   || kind == A_UNION_TYPE || kind == A_VOID_TYPE)
  {
    signature[0] = e^.nr;
    return 1;
  }

  if (kind == AN_ARRAY_TYPE)
  {
    signature[0] = e^.the_array_type.length;
    return 1 + fill_signature (e^.the_array_type.open_array, out signature[1 : signature'length-1]);
  }

  if (kind == A_CONSTRAINED_STRUCT_TYPE)
  {
    signature[0] = e^.the_constrained_struct_type.discriminant_value;
    return 1 + fill_signature (e^.the_constrained_struct_type.open_struct, out signature[1 : signature'length-1]);
  }

  if (kind == AN_OPEN_ARRAY_TYPE)
  {
    signature[0] = -1;
    return 1 + fill_signature (e^.the_open_array_type.element, out signature[1 : signature'length-1]);
  }

  if (kind == A_POINTER_TYPE)
  {
    signature[0] = -2;
    return 1 + fill_signature (e^.the_pointer_type.designated_type, out signature[1 : signature'length-1]);
  }

  if (kind == AN_UNSAFE_POINTER_TYPE)
  {
    signature[0] = -3;
    return 1 + fill_signature (e^.the_unsafe_pointer_type.designated_type, out signature[1 : signature'length-1]);
  }

  if (kind != A_FUNCTION_POINTER_TYPE)
    fatal_compiler_error0 ("fill_signature(1)");

  signature[0] = -4;
  signature[1] = (int8)e^.the_function_pointer_type.is_callback;
  signature[2] = (int8)(e^.the_function_pointer_type.extern_dll_or_null != null);
  signature[3] = (int8)e^.the_function_pointer_type.is_entry;

  index = 4;

  index += fill_signature (e^.the_function_pointer_type.return_type, out signature[index : signature'length-index]);

  e = e^.the_function_pointer_type.parameters^.entities.first;
  while (e != null)
  {
    if (e^.kind == A_PARAMETER)
    {
      signature[index++] = -5;  // PAR
      signature[index++] = (int8)e^.the_parameter.mode;

      index += fill_signature (e^.the_parameter.type, out signature[index : signature'length-index]);
    }
    e = e^.next;
  }

  signature[index++] = -6;  // END_FUNC

  return index;
}

/*****************************************************************************/

public
void init_heap_type_btree ()
{
  create_btree (out heap_type_btree, null, compare);
}

/**********************************************************************************/

public
int heap_object_type_unique_nr (PENTITY subtype)
{
  int     signature_len;
  int8[]^ signature;
  NODE    n;
  int     rc;

  signature_len = signature_length (subtype);

  signature = new int8 [ signature_len ];

  if (fill_signature (subtype, out signature^) != signature_len)
    fatal_out_of_memory_error ("heap_object_type_unique_nr(2)");

#if 0
{
  int i;
  printf ("signature is : ");
  for (i=0; i<signature_len; i++)
    printf (" %d", signature^[i]);
  printf ("\n");
}
#endif

  clear n;
  n.signature = signature;

  rc = retrieve_btree (heap_type_btree, ref n, BT_EQUAL);
  if (rc == 0)
  {
    free signature;
    return n.unique_type;
  }

  if (rc != BT_KEY_NOT_FOUND)
    fatal_compiler_error0 ("heap_object_type_unique_nr(3)");

  n.signature        = signature;
  n.unique_type      = ++unique_type_serial_nr;

  rc = insert_btree (ref heap_type_btree, n);
  if (rc < 0)
    fatal_compiler_error0 ("heap_object_type_unique_nr(2)");

  return unique_type_serial_nr;
}

/*****************************************************************************/
