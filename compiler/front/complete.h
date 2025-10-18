
// complete.h : check for completition of incomplete & opaque types, and bodies.

use entities;

//----------------------------------------------------------------------------------------

// called for function bodies and block statements

void check_local_completion (PENTITY first);

//----------------------------------------------------------------------------------------

bool is_body_required (PENTITY first);

//----------------------------------------------------------------------------------------

// called by unit bodies and package bodies
// to check the interface and body parts
// uses current token position

void check_global_completion (PENTITY first);

//----------------------------------------------------------------------------------------
