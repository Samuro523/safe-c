
// overlap.h : check for overlapping memory zones

use entities;

// note: unconstrained array aggregate never overlap with an object because
//       the inner element is first evaluated and stored in a separate area.

// use to test if an assignment can be done in any memory order
bool object_shares_memory_with_expression (PEXPRESSION obj, PEXPRESSION exp);

// used to test if an aggregate can be evaluated "inline"
bool object_used_in_expression (PEXPRESSION obj, PEXPRESSION exp);
