
// dbginfo : write debug info into file

void dbg_header ();
void dbg_new_unit (int unit);
void dbg_store_line (int line, int ip);
void dbg_store_unit_name (int unit, string library, string source);
void move_dbg_lines (int start_address, int rip_offset);
void move_all_ip (int rip_offset);
void dbg_save (string debug_filename);
