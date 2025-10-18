
// dllnames.h

uint4 insert_dll_name (string dll, string func);

// never free dll and func, they are shared and always stay allocated.
void get_dll_name (uint4 nr, out string^ dll, out string^ func);
