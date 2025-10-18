
// memory.c

#if WINDOWS
  use win/windows;
#endif  

#if ANDROID
  use android/bionic;
#endif

  
#begin unsafe

#if WINDOWS

public byte *malloc (uint size)
{
  return (byte*)HeapAlloc (GetProcessHeap(), 4, size);
}

public void freem (byte *p)
{
  assert HeapFree (GetProcessHeap(), 0, p) != 0;
}

public uint heapsize2 (byte *p)
{
  return (uint)HeapSize (GetProcessHeap(), 0, p);
}

public byte *realloc (byte *p, uint size)
{
  byte *n = malloc (size);
  uint old_size = heapsize2 (p);
  n[0 : old_size] = p[0 : old_size];
  freem (p);
  return n;
}

#endif


#if ANDROID

public byte *malloc (uint size)
{
  return (byte*)bionic.malloc (size);
}

public void freem (byte *p)
{
  bionic.freem (p);
}

public uint heapsize2 (byte *p)
{
  return malloc_usable_size (p);
}

public byte *realloc (byte *p, uint size)
{
  return (byte*)bionic.realloc (p, size);
}

#endif


#end unsafe
