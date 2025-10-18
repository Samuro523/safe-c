
// elf.h

#begin unsafe

// load resource from elf library, referenced in libsafe.rc
bool load_ressource (int name, int typ, out byte* ptr, out uint size);

#end unsafe
