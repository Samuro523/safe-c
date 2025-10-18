
// memory.h


#begin unsafe

// allocate uninitialized memory, fatal error if out of memory
byte *malloc (uint size);

void freem (byte *p);

// returns size of allocated block (or slighly larger !)
uint heapsize2 (byte *p);

// reallocate larger piece, copy the old content
byte *realloc (byte *p, uint size);

#end unsafe
