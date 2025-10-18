#begin unsafe
/* Copyright (C) 2007 Jean-Marc Valin

   File: os_support.h
   This is the (tiny) OS abstraction layer. Aside from math.h, this is the
   only place where system headers are allowed.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

use opus_types;

/** Opus wrapper for malloc(). To do your own dynamic allocation, all you need to do is replace this function and opus_free */

byte *opus_alloc (size_t size);

/** Same as celt_alloc(), except that the area is only needed inside a CELT call (might cause problem with wideband though) */

byte *opus_alloc_scratch (size_t size);

/** Opus wrapper for free(). To do your own dynamic allocation, all you need to do is replace this function and opus_alloc */

void opus_free (byte *ptr);

char *malloc (int size);
void afree (byte *p);

/** Copy n bytes of memory from src to dst. The 0* term provides compile-time type checking  */

void memcpy (byte* target, byte *source, int size);

/** Copy n bytes of memory from src to dst, allowing overlapping regions. The 0* term
    provides compile-time type checking */

void memmove (byte* target, byte *source, int size);

/** Set n elements of dst to zero, starting at address s */

void memset (char *p, int value, int size);

/*#ifdef __GNUC__
#pragma GCC poison printf sprintf
#pragma GCC poison malloc free realloc calloc
#endif*/


#end unsafe
