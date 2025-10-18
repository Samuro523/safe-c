
// glibpaths.h

void load_global_library_paths (string ini_filename);

void find_library (    string    unit_name,          // "std"
                   out char[260] library_filename);  // "std.lib" or "c:/libraries/std.lib"
