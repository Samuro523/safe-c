
// exeoute.c : asm independant executable image output

from std use files;
use error, goptions;

/************************************************************************/

byte[]^ g_image;
uint    g_image_index;
uint    g_size;

/************************************************************************/

public void exe_init_image (uint initial_size)
{
  g_image = new byte[initial_size];
}

/************************************************************************/

#begin unsafe
public byte* exe_ptr (uint ptr)
{
  return &g_image^[ptr];
}
#end unsafe

/************************************************************************/

public uint4 exe_current_ptr ()
{
  return g_image_index;
}

/************************************************************************/

public void exe_set_current_ptr (uint4 ptr)
{
  assert ptr <= g_image^'size;
  g_image_index = ptr;
}

/************************************************************************/

public void exec_advance_ptr (uint ofs)
{
  exe_set_current_ptr (g_image_index + ofs);
}

/************************************************************************/

public
void probe (uint4 size)
{
  byte[]^ new_image;
  uint4   new_size;

  if (g_image^'size - g_image_index < size)   // not enough memory left
  {
    new_size = g_image^'size;
    while (new_size - g_image_index < size)   // double allocated size until enough space
    {
      if (new_size >= 0x80000000)  // 2GB
        fatal_out_of_memory_error ("probe1");
      new_size <<= 1;
    }

    new_image = new byte[new_size];
    new_image^[0 : g_image^'size] = g_image^;

    free g_image;    // free old image

    g_image = new_image;
  }
}

/************************************************************************/

public
void align_at (uint4 mod)  // ex: mod = PAGE
{
  probe (mod);
  while ((g_image_index & (mod-1)) > 0)
    g_image^[g_image_index++] = 0;

  if (g_size < g_image_index)
    g_size = g_image_index;
}

/************************************************************************/

public
void write_vector (int8 value)
{
  probe ((uint)address_size);
  if (address_size == 4)
  {
    uint4 u = (uint4)value;
    g_image^[g_image_index:4] = u'byte;
  }
  else
    g_image^[g_image_index:8] = value'byte;
  g_image_index += (uint)address_size;


  if (g_size < g_image_index)
    g_size = g_image_index;
}

/************************************************************************/

public void exe_write_byte (uint value)
{
  byte b = (byte)value;
  probe (1);
  g_image^[g_image_index] = b;
  g_image_index++;

  if (g_size < g_image_index)
    g_size = g_image_index;
}

/************************************************************************/

public void exe_write_int1 (int value)
{
  int1 i = (int1)value;
  probe (1);
  g_image^[g_image_index:1] = i'byte;
  g_image_index++;

  if (g_size < g_image_index)
    g_size = g_image_index;
}

/************************************************************************/

public void exe_write_int2 (int value)
{
  int2 i = (int2)value;
  probe (2);
  g_image^[g_image_index:2] = i'byte;
  g_image_index += 2;

  if (g_size < g_image_index)
    g_size = g_image_index;
}

/************************************************************************/

public void exe_write_int4 (int4 value)
{
  probe (4);
  g_image^[g_image_index:4] = value'byte;
  g_image_index += 4;

  if (g_size < g_image_index)
    g_size = g_image_index;
}

/************************************************************************/

public void exe_write_int8 (int8 value)
{
  probe (8);
  g_image^[g_image_index:8] = value'byte;
  g_image_index += 8;

  if (g_size < g_image_index)
    g_size = g_image_index;
}

/************************************************************************/

// returns the image offset at which the sequence was stored

public uint4 exe_write_byte_sequence (byte[] sequence, uint4 align)
{
  uint4 ofs;

  align_at (align);
  ofs = g_image_index;

  probe (sequence'size);
  g_image^[ofs : sequence'size] = sequence;
  g_image_index += sequence'size;

  if (g_size < g_image_index)
    g_size = g_image_index;

  return ofs;
}

/************************************************************************/

public
void exe_move_byte_sequence (uint origin, uint destination, uint size)
{
  g_image^[destination : size] = g_image^[origin : size];
}

/************************************************************************/

public
void exe_patch (uint ptr, byte[] sequence)
{
  g_image^[ptr : sequence'size] = sequence;
}

/************************************************************************/

public void exe_save (string exec_filename)
{
  int fd;

  fd = create (exec_filename);
  if (fd < 0)
  {
    fatal_compiler_error0 ("cannot save exe file on disk");
  }

  if (write (fd, g_image^[0:g_size]) != (int)g_size)
  {
    close (fd);
    delete_file (exec_filename);
    fatal_compiler_error0 ("cannot write exe file to disk");
  }

  if (close (fd) < 0)
  {
    delete_file (exec_filename);
    fatal_compiler_error0 ("cannot close exe file");
  }
}

/************************************************************************/
