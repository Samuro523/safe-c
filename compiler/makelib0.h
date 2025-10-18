
// makelib0.h

const uint4 LIB_VERSION = 26;
const uint4 MAGIC       = 0x7F65B103;

packed struct HEADER
{
  uint4 magic;
  uint4 version;      // LIB_VERSION
  uint4 offset_dir;   // offset of item table, almost at the end of the file (does not support files larger than 4 GB)
  uint4 dummy;
}

packed struct NODE_INFO
{
  int   unit_name_length;
  uint4 offset_in_file;  // does not support files larger than 4 GB
  uint4 packed_size;     // size in file
  int   compression;     // 0
  uint4 unpacked_size;   // size when uncompressed
}
