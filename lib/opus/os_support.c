
use opus_types;

#if WINDOWS
  use ../win/windows;
#endif

#if ANDROID
  use ../android/bionic;
#endif
  

#begin unsafe


public char *malloc (int size)
{
  #if WINDOWS
    return (char*)HeapAlloc (GetProcessHeap(), 4, (uint)size);
  #endif

  #if ANDROID
    return (char*)bionic.malloc ((uint)size);
  #endif
}

public void afree (byte *p)
{
  #if WINDOWS
    assert HeapFree (GetProcessHeap(), 0, p) != 0;
  #endif

  #if ANDROID
    bionic.freem (p);
  #endif
}


public byte *opus_alloc (size_t size)
{
   return (byte*)malloc((int)size);
}

/** Same as celt_alloc(), except that the area is only needed inside a CELT call (might cause problem with wideband though) */

public byte *opus_alloc_scratch (size_t size)
{
   /* Scratch space doesn't need to be cleared */
   return opus_alloc(size);
}

/** Opus wrapper for free(). To do your own dynamic allocation, all you need to do is replace this function and opus_alloc */

public void opus_free (byte *ptr)
{
  afree(ptr);
}


public void memset (char *p, int value, int size)
{
  p[0:size] = {all => (char)value};
}

public void memcpy (byte* target, byte *source, int size)
{
  target[0:size] = source[0:size];
}

public void memmove (byte* target, byte *source, int size)
{
  target[0:size] = source[0:size];
}

#end unsafe