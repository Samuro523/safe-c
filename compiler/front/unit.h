
// unit.h

void init_unit_tree ();

// returns 0 if OK, +1 if file not found
int compile_main_unit (string filename);

void retrieve_library_and_source_names (int key, out string library, out string source);

void list_units_for_debug ();
