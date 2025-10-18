
// complete.c

from std use strings;
use lex, entities, ../error, tokens;

//----------------------------------------------------------------------------------------

// called for function bodies and block statements

public
void check_local_completion (PENTITY first)
{
  PENTITY e;

  e = first;
  while (e != null)
  {
    if (e^.kind == AN_INCOMPLETE_TYPE)
    {
      if (e^.the_incomplete_type.full_type == null)
      {
        char msg[64+MAX_IDENTIFIER_LENGTH];
        sprintf (out msg, "missing full type declaration for incomplete type %S", e^.identifier_or_null^);
        semantic_error (msg, token.pos);
      }
    }

    e = e^.next;
  }
}

//----------------------------------------------------------------------------------------

public
bool is_body_required (PENTITY first)
{
  PENTITY e;

  e = first;
  while (e != null)
  {
    if (e^.kind == AN_INCOMPLETE_TYPE && e^.the_incomplete_type.full_type == null)
      return true;

    if (e^.kind == AN_OPAQUE_TYPE)
      return true;

    if (e^.kind == A_PACKAGE_DECLARATION && e^.the_package_declaration.is_body_required)
      return true;

    if (e^.kind == A_FUNCTION_DECLARATION &&
        e^.the_function_declaration.to_type^.the_function_pointer_type.extern_dll_or_null == null &&
        !e^.the_function_declaration.to_type^.the_function_pointer_type.is_syscall)
      return true;

    e = e^.next;
  }

  return false;
}

//----------------------------------------------------------------------------------------

// called by unit bodies and package bodies
// to check the interface and body parts
// uses current token position

public
void check_global_completion (PENTITY first)
{
  PENTITY e;
  char    msg[64+MAX_IDENTIFIER_LENGTH];

  e = first;
  while (e != null)
  {
    if (e^.kind == AN_INCOMPLETE_TYPE && e^.the_incomplete_type.full_type == null)
    {
      sprintf (out msg, "missing full type declaration for incomplete type %S", e^.identifier_or_null^);
      semantic_error (msg, token.pos);
    }

    if (e^.kind == AN_OPAQUE_TYPE && e^.the_opaque_type.full_type == null)
    {
      sprintf (out msg, "missing full type declaration for opaque type %S", e^.identifier_or_null^);
      semantic_error (msg, token.pos);
    }

    if (e^.kind == A_PACKAGE_DECLARATION &&
        e^.the_package_declaration.is_body_required &&
        e^.the_package_declaration.to_package_body_or_null == null)
    {
      sprintf (out msg, "missing body for package %S", e^.identifier_or_null^);
      semantic_error (msg, token.pos);
    }

    if (e^.kind == A_FUNCTION_DECLARATION &&
        e^.the_function_declaration.to_type^.the_function_pointer_type.extern_dll_or_null == null &&
        !e^.the_function_declaration.to_type^.the_function_pointer_type.is_syscall &&
        e^.the_function_declaration.to_function_body_or_null == null)
    {
      sprintf (out msg, "missing body for function %S", e^.identifier_or_null^);
      semantic_error (msg, token.pos);
    }

    e = e^.next;
  }
}

//----------------------------------------------------------------------------------------
