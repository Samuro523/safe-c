
// package.h

void parse_package_declaration_or_package_instantiation_or_package_body (    bool body_allowed, 
                                                                         ref bool body_was_parsed);
                 
void parse_generic_package_declaration ();

void instantiate_all_pending_package_bodies ();
