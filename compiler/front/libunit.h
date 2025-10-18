
// libunit.h

void init_library_unit_tree ();

// returns -1 if unit was not found in library
int load_library_source (string library, string source_name, out byte[]^ source);

bool is_console_app;    // set to true if console.h is loaded from std
