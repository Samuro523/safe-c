
// decl.h

use entities, ../common;

//=================================================================================

void parse_global_declarations (bool is_body);

//=================================================================================

// 'out_prefix' is an "out" parameter that can either be null,
// or denote an already parsed expanded name that is not a type_name.

void parse_local_declarations (out TEXT_POSITION pos,
                               out PENTITY       out_prefix);

//=================================================================================
