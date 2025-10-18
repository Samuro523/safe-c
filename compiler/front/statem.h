
// statem.h

use entities, ../common;

// 'prefix' can either be null,
// or denote an already parsed expanded name that is not a type_name.

void parse_statements (TEXT_POSITION pos,
                       PENTITY       prefix);
