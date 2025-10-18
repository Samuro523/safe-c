
// exeoute.h : asm independant executable image output

/*********************************************************************************/

void exe_init_image (uint initial_size);

/*********************************************************************************/

#begin unsafe
byte* exe_ptr(uint ptr);
#end unsafe

/*********************************************************************************/

uint4 exe_current_ptr ();
void exe_set_current_ptr (uint4 ptr);
void exec_advance_ptr (uint ofs);

/*********************************************************************************/

// make sure place is allocated
void probe (uint4 size);

/*********************************************************************************/

void align_at (uint4 mod);

/*********************************************************************************/

void write_vector (int8 value);   // 4 or 8 bytes, depending on address size
void exe_write_byte (uint value);
void exe_write_int1 (int value);
void exe_write_int2 (int value);
void exe_write_int4 (int4 value);
void exe_write_int8 (int8 value);

// returns the image offset at which the sequence was stored
uint4 exe_write_byte_sequence (byte[] sequence, uint4 align);

void exe_move_byte_sequence (uint origin, uint destination, uint size);

void exe_patch (uint ptr, byte[] sequence);

/*********************************************************************************/

void exe_save (string exec_filename);

/*********************************************************************************/
