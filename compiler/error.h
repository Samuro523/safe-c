
// error.h

use common;

void set_current_error_source_file (string lib, string file, int key);

int g_unit_key;  // used by entities.c when creating new entities

// error in configuration file
void config_error (string s);

void warning          (string s, TEXT_POSITION pos);
void lexical_error    (string s, TEXT_POSITION pos);
void syntax_error     (string s, TEXT_POSITION pos);
void semantic_error   (string s, TEXT_POSITION pos);

void resource_error (string resource_filename, string msg, int line_nr, int col);

void error_set_code_generator_unit_key (int key);
void error_set_code_generator_source_line (int line);
void code_generator_error (string s);

void stop_compilation ();

// error in compiler logic
void fatal_compiler_error (string s, TEXT_POSITION pos);
void fatal_compiler_error0 (string s);

void fatal_out_of_memory_error (string s);

long nb_of_compilation_errors ();
long nb_of_compilation_warnings ();

void get_current_source_filename (out char[528] filename);
