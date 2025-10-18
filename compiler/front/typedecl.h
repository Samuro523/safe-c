
// typedecl.h

use entities, ../common;

//==========================================================================================

void check_unsafe_region ();

//==========================================================================================

void parse_semicolon ();

PENTITY parse_type_definition (TEXT_POSITION pos0, PENTITY t0);

// The expanded name of a type_name must denote a type.
// returns null in case of error.
PENTITY parse_type_name ();

//==========================================================================================

void parse_enumeration_declaration ();
void parse_typedef_or_incomplete_declaration ();
void parse_struct_or_opaque_declaration ();
void parse_union_declaration ();
void parse_ref_declaration ();

//==========================================================================================

// when t is non-null, a type definition has been parsed earlier and an identifier follows.
// when t is null, current token is : const or volatile.

void parse_object_declaration (TEXT_POSITION pos0,
                               PENTITY       t0,
                               bool          is_global);

//==========================================================================================

void parse_function_specification (    TEXT_POSITION pos0,
                                       PENTITY       t,
                                       bool          body_allowed,
                                   ref bool          body_was_parsed,
                                       bool          in_typedef,
                                       bool          in_generic_part);

//==========================================================================================

// when t is non-null, a type definition has been parsed earlier and an identifier follows.
// when t is null, current token is : "[", private, inline or void.

void parse_function_declaration_or_function_body (    TEXT_POSITION pos0,
                                                      PENTITY       t,
                                                      bool          body_allowed,
                                                  ref bool          body_was_parsed);

//==========================================================================================

// check if types from function declaration and body match.
// (they can differ due to opaque types becoming complete or redeclaration of arrays/pointers)

bool types_are_equal (PENTITY t1, PENTITY t2);

//==========================================================================================

void create_enumeration_literal_string_table (PENTITY t);

//==========================================================================================
