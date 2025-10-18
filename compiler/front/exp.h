
// exp.h

use entities, ../common;

//*****************************************************************************

void parse_expression (    CONTEXT      context,           // never null
                           TEXT_POSITION prefix_position,
                           PENTITY       prefix_name,       // can be null
                       out PEXPRESSION   pout);

void parse_unary_expression (    CONTEXT      context,           // never null
                                 TEXT_POSITION prefix_position,
                                 PENTITY       prefix_name,       // can be null
                             out PEXPRESSION   pout);

//*****************************************************************************

void convert_type_into_context (    PENTITY  type,
                                out CONTEXT context);

//*****************************************************************************

// returns 0 if OK, -1 if types are not compatible
// error message is generated in function

int check_assignment_context_compatibility (CONTEXT target, PEXPRESSION source, TEXT_POSITION pos);

//*****************************************************************************

// error message is generated in function
// used for parameter and ref declaration

int check_ref_parameter_context_compatibility (CONTEXT target, PEXPRESSION source, TEXT_POSITION pos);

//*****************************************************************************

int8 parse_constant_int_or_uint_expression ();

//*****************************************************************************

PEXPRESSION parse_constant_expression ();

//*****************************************************************************

PEXPRESSION parse_constant_expression_with_context (CONTEXT context);

//*****************************************************************************

bool constant_expressions_match (PEXPRESSION e1, PEXPRESSION e2);

//*****************************************************************************

void free_exp (PEXPRESSION e);

//*****************************************************************************

PEXPRESSION dummy_expression ();

//*****************************************************************************
