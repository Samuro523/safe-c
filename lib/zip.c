
// zip.c

use calendar, crc, files, memory, strings, utf;
use pack/pack, pack/unpack;

#if WINDOWS
  use win/windows;
#endif

/***********************************************************************/

package ZIP_CONTAINER_FORMAT

  packed struct ZIP_LOCAL_FILE_HEADER
  {
    uint4 local_file_header_signature;     // 0x04034b50
    uint2 min_version_needed_to_extract;   // 0x14(=20) for ZIP, 0x2D(=45) for ZIP64
    uint2 general_purpose_bit_flag;        // 0 or (1<<3 if size not known when writing this header)(1<<11 if filename is in utf-8)
    uint2 compression_method;              // 0 = none, 8 = deflate, 9 = deflate64
    uint2 file_last_modification_time;     // bits 0-4 : seconds/2, bits 5-10 : minutes, bits 11-15 : hours
    uint2 file_last_modification_date;     // bits 0-4 : day (1-31), bits 5-8 : month (1-12), bits 9-15 : year-1980
    uint4 crc32_of_uncompressed_data;
    uint4 compressed_size;                 // or 0 for later, or 0xffffffff for ZIP64
    uint4 uncompressed_size;               // or 0 for later, or 0xffffffff for ZIP64
    uint2 filename_length;
    uint2 extra_field_length;
    // followed by filename (in utf8, without leading drive nor slash) and extra_field
  }

  packed struct ZIP64_EXTRA_FIELD_CHUNK  // if present, denotes ZIP64 format
  {
    uint2 header_ID;       // 0x0001
    uint2 size_of_chunk;   // (8, 16, 24 or 28) depends on fields below  (usually 16)
    int8  uncompressed_size;
    int8  compressed_size;
    int8  offset_of_local_file_header;    // relative to disk, see field below
    uint4	disk_of_local_file_header;
  }


  // Zero-byte files and directories MUST NOT generate compressed data.

  // This descriptor MUST exist if bit 3 of the general purpose bit flag is set.
  // This descriptor SHOULD be used only when it was not possible to seek.
  packed struct ZIP_OPTIONAL_DATA_DESCRIPTOR_HEADER
  {
    uint4 signature;                         // 0x08074b50
  }
  packed struct ZIP_OPTIONAL_DATA_DESCRIPTOR
  {
    uint4 crc32_of_uncompressed_data;
    uint4 compressed_size;
    uint4 uncompressed_size;
  }
  packed struct ZIP64_OPTIONAL_DATA_DESCRIPTOR  // used if preceding ZIP_EXTRA_FIELD_CHUNK_64
  {
    uint4 crc32_of_uncompressed_data;
    int8  compressed_size;
    int8  uncompressed_size;
  }

  packed struct ZIP_CENTRAL_DIRECTORY_HEADER
  {
    uint4 central_directory_file_header_signature;  // 0x02014b50
    uint2 version_made_by;                          // 0x14(=20) for ZIP, 0x2D(=45) for ZIP64
    uint2 minimum_version_needed_to_extract;        // 0x14(=20) for ZIP, 0x2D(=45) for ZIP64
    uint2 general_purpose_bit_flag;                 // 0 or (1<<3 if size not known when writing this header)(1<<11 if filename is in utf-8)
    uint2 compression_method;              // 0 = none, 8 = deflate, 9 = deflate64
    uint2 file_last_modification_time;     // bits 0-4 : seconds/2, bits 5-10 : minutes, bits 11-15 : hours
    uint2 file_last_modification_date;     // bits 0-4 : day (1-31), bits 5-8 : month (1-12), bits 9-15 : year-1980
    uint4 crc32_of_uncompressed_data;
    uint4 compressed_size;                 // or 0xffffffff for ZIP64
    uint4 uncompressed_size;               // or 0xffffffff for ZIP64
    uint2 filename_length;
    uint2 extra_field_length;
    uint2	file_comment_length;
    uint2 disk_of_local_file_header;       // or 0xffff for ZIP64
    uint2 internal_file_attributes;        // 0 for binary, 1 for text
    uint4 external_file_attributes;        // 0x20 (archive)
    uint4 offset_of_local_file_header;     // or 0xffffffff for ZIP64   (relative to disk_of_local_file_header)
    // followed by filename (in utf8), extra_field, and file_comment.
  }


  packed struct ZIP64_END_OF_CENTRAL_DIRECTORY
  {
    uint4 end_of_central_directory_signature;       // 0x06064b50
    int8  size_of_the_EOCD64_minus_12;              // (0x2C)
    uint2 version_made_by;                          // 0x14(=20) for ZIP, 0x2D(=45) for ZIP64
    uint2 minimum_version_needed_to_extract;        // 0x14(=20) for ZIP, 0x2D(=45) for ZIP64
    uint4 number_of_this_disk;                      // (0)
    uint4 disk_where_central_directory_starts;      // (0)
    int8  number_of_central_directory_records_on_this_disk;  // (1)
    int8  total_number_of_central_directory_records;   // (1)
    int8  size_of_central_directory;                // (0x61)
    int8  offset_of_start_of_central_directory;     // relative to disk_where_central_directory_starts
    // followed by comment (up to the size of EOCD64)
  }


  packed struct ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR
  {
    uint4 end_of_central_directory_locator_signature;       // 0x07064b50
    uint4 disk_of_zip64_end_of_central_directory;           // (0)
    int8  offset_of_zip64_end_of_central_directory;         // (relative to disk, see field above)
    uint4 total_number_of_disks;
  }

  packed struct ZIP_END_OF_CENTRAL_DIRECTORY
  {
    uint4 end_of_central_directory_signature;                 // 0x06054b50
    uint2 number_of_this_disk;                                // or 0xffff for ZIP64
    uint2 disk_where_central_directory_starts;                // or 0xffff for ZIP64
    uint2 number_of_central_directory_records_on_this_disk;   // or 0xffff for ZIP64
    uint2 total_number_of_central_directory_records;          // or 0xffff for ZIP64
    uint4 size_of_central_directory;                          // or 0xffffffff for ZIP64
    uint4 offset_of_start_of_central_directory;               // relative to start of archive (or 0xffffffff for ZIP64)
    uint2 comment_length;
    // followed by comment
  }

end ZIP_CONTAINER_FORMAT;

/***********************************************************************/

// declarations for packing

const bool FORCE_ZIP64 = false;

typedef byte[]^ ENTRY;   // data to be stored in central directory

struct ZIP_CONTAINER
{
  int      fd;
  ENTRY[]^ list;
  bool     ZIP64;
}

struct COMPRESS_INFO
{
  long bytes_written;
  long uncompressed_size;
  int  in_fd;
  int  out_fd;
  uint crc32;     // crc on read bytes
}

package PZ = new zip.PACK (USER_INFO => COMPRESS_INFO);

//------------------------------------------------------------------------------------

// declarations for unpacking

packed struct FILE_INFO
{
  long  filename_pos;
  uint2 filename_length;
  bool  filename_in_utf8;
  bool  filename_in_msdos;

  uint2 file_last_modification_time;     // bits 0-4 : seconds/2, bits 5-10 : minutes, bits 11-15 : hours
  uint2 file_last_modification_date;     // bits 0-4 : day (1-31), bits 5-8 : month (1-12), bits 9-15 : year-1980

  long offset_of_local_file_header;
  long compressed_size;
  uint2 compression_method;              // 0 = none, 8 = deflate, 9 = deflate64

  long uncompressed_size;
  uint4 crc32_of_uncompressed_data;
}

struct UNZIP_CONTAINER
{
  int          fd;
  long         in_pos;
  long         in_size;

  int          out_fd;
  long         out_pos;
  long         out_size;
  uint         crc32;

  FILE_INFO[]^ list;
}

package UNPK = new zip.UNPACK (USER_INFO => UNZIP_CONTAINER);

//------------------------------------------------------------------------------------------
// create and write container
//------------------------------------------------------------------------------------------

public int zip_create_container (out ZIP_CONTAINER zip, string zip_filename)
{
  clear zip;

  zip.fd = create (zip_filename);
  if (zip.fd < 0)
    return zip.fd;

  return 0;
}

//------------------------------------------------------------------------------------

public int wzip_create_container (out ZIP_CONTAINER zip, wstring zip_filename)
{
  clear zip;

  zip.fd = wcreate (zip_filename);
  if (zip.fd < 0)
    return zip.fd;

  return 0;
}

//------------------------------------------------------------------------------------

bool is_valid_relative_filename (wstring filename, out wstring fixed_filename)
{
  int len, i;

  clear fixed_filename;

  len = wstrlen(filename);
  if (len == 0 || len > fixed_filename'length)  // empty or too long
    return false;

  for (i=0; i<len; i++)
  {
    wchar c = filename[i];

    if (c == L'\\')
      c = L'/';

    if (c == L':')
      return false;  // no drive allowed

    fixed_filename[i] = c;
  }

  if (filename[0] <= (wchar)32 || filename[0] == L'/')    // no leading space or slash
    return false;

  return true;
}

//------------------------------------------------------------------------------------

int compress_read (ref COMPRESS_INFO info, out byte[] buffer)
{
  int rc;
  rc = files.read (info.in_fd, out buffer);
  if (rc > 0)
    update_crc (ref info.crc32, buffer[0 : rc]);
  return rc;
}

//------------------------------------------------------------------------------------

int compress_write (ref COMPRESS_INFO info, byte[] buffer)
{
  info.bytes_written += buffer'size;
  if (info.bytes_written >= info.uncompressed_size)   // compressed size >= uncompressed ?  return error
  {
    info.bytes_written -= buffer'size;
    return -1;
  }

  return files.write (info.out_fd, buffer);
}

//------------------------------------------------------------------------------------

public int wzip_add_file (ref ZIP_CONTAINER zip, wstring file, wstring name, bool always_store = false)
{
  int   fd, rc, fixed_filename_length, utf_filename_length;
  long  uncompressed_size, start_pos, data_pos, end_pos;
  wchar fixed_filename[260];
  char  utf_filename[3*260];
  bool  ZIP64;

  if (!is_valid_relative_filename (name, out fixed_filename))
    return -1;

  fd = wopen (file);
  if (fd < 0)
    return fd;

  uncompressed_size = filesize (fd);
  if (uncompressed_size < 0)
  {
    close (fd);
    return -1;
  }

  start_pos = lseek (zip.fd, offset => 0, mode => SEEK_CUR);

  ZIP64 = FORCE_ZIP64 ||
          (uncompressed_size > 4*1024*1024*1024 - 512) ||
          (start_pos > 4*1024*1024*1024 - 512);
  zip.ZIP64 |= ZIP64;

  fixed_filename_length = wstrlen(fixed_filename);

  utf . utf16_to_utf8 (fixed_filename, fixed_filename_length, out utf_filename, out utf_filename_length);

  {
    ZIP_LOCAL_FILE_HEADER   header;
    ZIP64_EXTRA_FIELD_CHUNK extra_chunk;
    long                    clock;
    calendar.DATE_TIME      datetime;

    clear header, extra_chunk;

    header.local_file_header_signature  = 0x04034b50;
    header.min_version_needed_to_extract = (uint2)(ZIP64 ? 0x2D : 0x14);
    header.general_purpose_bit_flag = 1 << 11;     // filename is always stored in utf-8 (avoids dos conversion when unzipping)

    get_ftime (fd, out clock);
    clock_to_datetime (clock, 0, out datetime);  // gmt
    header.file_last_modification_time = (uint2)((datetime.sec >> 1) + (datetime.min << 5) + (datetime.hour << 11));
    header.file_last_modification_date = (uint2)(datetime.day + (datetime.month << 5) + ((datetime.year - 1980) << 9));

    header.uncompressed_size  = ZIP64 ? 0xffffffff : (uint)uncompressed_size;
    header.filename_length    = (uint2)utf_filename_length;
    header.extra_field_length = (uint2)(ZIP64 ? 20 : 0);

    if (ZIP64)
    {
      extra_chunk.header_ID         = 0x0001;
      extra_chunk.size_of_chunk     = 16;
      extra_chunk.uncompressed_size = uncompressed_size;
    }


    // write data

    data_pos = lseek (zip.fd, offset => header'size + header.filename_length + header.extra_field_length, mode => SEEK_CUR);
    if (data_pos < 0)
    {
      close (fd);
      return -1;
    }

    {
      COMPRESS_INFO info;

      clear info;
      info.bytes_written     = 0L;
      info.uncompressed_size = uncompressed_size;
      info.in_fd             = fd;
      info.out_fd            = zip.fd;

      if (always_store)
        rc = PK_WRITE_ERROR;
      else
        rc = PZ.pack (ref info, compress_read, compress_write);

      if (rc == 0)   // compressed file was stored
      {
        end_pos = data_pos + info.bytes_written;

        header.compression_method         = 8;   // deflate
        header.compressed_size            = ZIP64 ? 0xffffffff : (uint)info.bytes_written;
        header.crc32_of_uncompressed_data = info.crc32;
        extra_chunk.compressed_size       = info.bytes_written;
      }
      else if (rc == PK_WRITE_ERROR)   // compressed size is too large, exceeds uncompressed size.
      {
        // we need to store the file as uncompressed.
        long rest;
        uint crc;
        byte buf[4096];

        if (lseek (fd,     offset => 0,        mode => SEEK_SET) < 0 ||
            lseek (zip.fd, offset => data_pos, mode => SEEK_SET) < 0)
        {
          close (fd);
          return -1;
        }

        clear buf;
        rest = uncompressed_size;
        crc = 0;

        while (rest > 0)
        {
          int chunk = (rest >= buf'length) ? buf'length : (int)rest;
          ref byte[] buffer_chunk = buf[0 : chunk];

          if (read (fd, out buffer_chunk) != chunk ||
              write (zip.fd, buffer_chunk) != chunk)
          {
            close (fd);
            return -1;
          }

          update_crc (ref crc, buffer_chunk);
          rest -= chunk;
        }

        end_pos = data_pos + uncompressed_size;

        header.compression_method         = 0;   // uncompressed
        header.compressed_size            = ZIP64 ? 0xffffffff : (uint)uncompressed_size;
        extra_chunk.compressed_size       = uncompressed_size;
        header.crc32_of_uncompressed_data = crc;
      }
      else
      {
        close (fd);
        return -1;
      }
    }

    if (close (fd) < 0)
      return -1;

    if (lseek (zip.fd, offset => start_pos, mode => SEEK_SET) < 0)
      return -1;

    if (write (zip.fd, header) != (int)header'size)
      return -1;

    if (write (zip.fd, utf_filename[0 : header.filename_length]) != (int)header.filename_length)
      return -1;

    if (write (zip.fd, extra_chunk'byte[0 : header.extra_field_length]) != (int)header.extra_field_length)
      return -1;

    if (lseek (zip.fd, offset => end_pos, mode => SEEK_SET) < 0)
      return -1;



    {
      ZIP_CENTRAL_DIRECTORY_HEADER  dir;
      ENTRY                         p;

      clear dir;
      dir.central_directory_file_header_signature = 0x02014b50;
      dir.version_made_by                         = header.min_version_needed_to_extract;
      dir.minimum_version_needed_to_extract       = header.min_version_needed_to_extract;
      dir.general_purpose_bit_flag                = header.general_purpose_bit_flag;
      dir.compression_method                      = header.compression_method;
      dir.file_last_modification_time             = header.file_last_modification_time;
      dir.file_last_modification_date             = header.file_last_modification_date;
      dir.crc32_of_uncompressed_data              = header.crc32_of_uncompressed_data;
      dir.compressed_size                         = header.compressed_size;
      dir.uncompressed_size                       = header.uncompressed_size;
      dir.filename_length                         = header.filename_length;
      dir.extra_field_length                      = header.extra_field_length;

//    dir.file_comment_length = 0;
//    dir.disk_of_local_file_header = 0;
//    dir.internal_file_attributes = 0;
//    dir.external_file_attributes = 0;

      if (start_pos <= 4*1024*1024*1024 - 512)
      {
        dir.offset_of_local_file_header = (uint)start_pos;
      }
      else    // we need to store offset in 8 bytes
      {
        dir.offset_of_local_file_header = 0xffffffff;
        dir.extra_field_length += 8;
        extra_chunk.size_of_chunk += 8;
        extra_chunk.offset_of_local_file_header = start_pos;
      }

      p = new byte[dir'size + dir.filename_length + dir.extra_field_length];
      p^[0        : dir'size                                    ] = dir'byte;
      p^[dir'size : dir.filename_length                         ] = utf_filename[0 : dir.filename_length] ' byte;
      p^[dir'size + dir.filename_length : dir.extra_field_length] = extra_chunk'byte[0 : dir.extra_field_length];

      if (zip.list == null)
      {
        zip.list = new ENTRY [1] ' {p};
      }
      else
      {
        ENTRY[]^ old_list;

        old_list = zip.list;
        zip.list = new ENTRY [old_list^'length + 1];
        zip.list^[0 : old_list^'length] = old_list^;
        zip.list^[old_list^'length] = p;
        free old_list;
      }
    }
  }

  return 0;
}

//------------------------------------------------------------------------------------

void wcstrcpy (out wstring dest, string src)
{
  int i;

  clear dest;

  for (i=0; i<src'length && src[i] != nul; i++)
    dest[i] = (wchar)(uint)src[i];
}

//----------------------------------------------------------------------------

public int zip_add_file (ref ZIP_CONTAINER zip, string file, string name, bool always_store = false)
{
  wchar wname[MAX_FILENAME_LENGTH];
  wchar wfile[MAX_FILENAME_LENGTH];
  wcstrcpy (out wname, name);
  wcstrcpy (out wfile, file);
  return wzip_add_file (ref zip, wfile, wname, always_store);
}

//------------------------------------------------------------------------------------

public int zip_close_container (ref ZIP_CONTAINER zip)
{
  int  ret            = 0;
  long pos            = lseek (zip.fd, offset => 0, mode => SEEK_CUR);
  uint nb_entries     = (uint)((zip.list == null) ? 0 : zip.list^'length);
  long total_dir_size = 0;

  if (nb_entries > 0)
  {
    ref ENTRY[] entries = zip.list^;
    uint i;
    for (i=0; i<nb_entries; i++)
    {
      {
        ref byte[] bin = entries[i]^;
        if (write (zip.fd, bin) != bin'length)
          ret = -1;
        total_dir_size += bin'length;
      }
      free entries[i];
    }
  }


  if (FORCE_ZIP64 || nb_entries > 65535 || pos + total_dir_size > 4*1024*1024*1024 - 512)
    zip.ZIP64 = true;


  if (nb_entries > 0)
  {
    if (zip.ZIP64)
    {
      {
        ZIP64_END_OF_CENTRAL_DIRECTORY r;

        clear r;
        r.end_of_central_directory_signature               = 0x06064b50;
        r.size_of_the_EOCD64_minus_12                      = r'size - 12;
        r.version_made_by                                  = 0x2D;
        r.minimum_version_needed_to_extract                = 0x2D;
        r.number_of_this_disk                              = 0;
        r.disk_where_central_directory_starts              = 0;
        r.number_of_central_directory_records_on_this_disk = nb_entries;
        r.total_number_of_central_directory_records        = r.number_of_central_directory_records_on_this_disk;
        r.size_of_central_directory                        = total_dir_size;
        r.offset_of_start_of_central_directory             = pos;

        if (write (zip.fd, r) != (int)r'size)
          ret = -1;
      }

      {
        ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR r;

        clear r;
        r.end_of_central_directory_locator_signature  = 0x07064b50;
        r.disk_of_zip64_end_of_central_directory      = 0;
        r.offset_of_zip64_end_of_central_directory    = pos + total_dir_size;
        r.total_number_of_disks                       = 1;

        if (write (zip.fd, r) != (int)r'size)
          ret = -1;
      }
    }
  }


  {
    ZIP_END_OF_CENTRAL_DIRECTORY r;

    clear r;

    if (zip.ZIP64)
    {
      r.end_of_central_directory_signature               = 0x06054b50;
      r.number_of_this_disk                              = 0xffff;
      r.disk_where_central_directory_starts              = 0xffff;
      r.number_of_central_directory_records_on_this_disk = 0xffff;
      r.total_number_of_central_directory_records        = 0xffff;
      r.size_of_central_directory                        = 0xffffffff;
      r.offset_of_start_of_central_directory             = 0xffffffff;
      r.comment_length                                   = 0;
    }
    else
    {
      r.end_of_central_directory_signature               = 0x06054b50;
      r.number_of_this_disk                              = 0;
      r.disk_where_central_directory_starts              = 0;
      r.number_of_central_directory_records_on_this_disk = (uint2)nb_entries;
      r.total_number_of_central_directory_records        = r.number_of_central_directory_records_on_this_disk;
      r.size_of_central_directory                        = (uint4)total_dir_size;
      r.offset_of_start_of_central_directory             = (uint4)pos;
      r.comment_length                                   = 0;
    }

    if (write (zip.fd, r) != (int)r'size)
      ret = -1;
  }

  free zip.list;

  if (close (zip.fd) < 0)
    ret = -1;

  clear zip;

  return ret;
}

//------------------------------------------------------------------------------------------
// open and read container
//------------------------------------------------------------------------------------------

public int unzip_close_container (ref UNZIP_CONTAINER uzip)
{
  int rc = 0;

  free uzip.list;

  if (uzip.fd > 0)
    rc = close (uzip.fd);
  else
    rc = UNPK_CLOSE_FAILED;

  clear uzip;

  return rc;
}

//------------------------------------------------------------------------------------------

int read_entry (ref UNZIP_CONTAINER uzip, ref long pos, out FILE_INFO e)
{
  ZIP_CENTRAL_DIRECTORY_HEADER rec;

  clear e;

  clear rec;
  if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
      read (uzip.fd, out rec) != (int)rec'size ||
      rec.central_directory_file_header_signature != 0x02014b50)
  {
    return UNPK_READ_ENTRY_FAILED;
  }
  pos += rec'size;

  e.filename_pos                = pos;
  e.filename_length             = rec.filename_length;
  e.filename_in_utf8            = (rec.general_purpose_bit_flag & (1<<11)) != 0;
  e.filename_in_msdos           = (rec.version_made_by >> 8) == 0;
  e.file_last_modification_time = rec.file_last_modification_time;
  e.file_last_modification_date = rec.file_last_modification_date;
  e.offset_of_local_file_header = rec.offset_of_local_file_header;
  e.compressed_size             = rec.compressed_size;
  e.compression_method          = rec.compression_method;
  e.uncompressed_size           = rec.uncompressed_size;
  e.crc32_of_uncompressed_data  = rec.crc32_of_uncompressed_data;

  pos += rec.filename_length;

  // scan extra chunks

  {
    int extra_len_rest;
    extra_len_rest = rec.extra_field_length;
    while (extra_len_rest >= 4)     // at least ID and size
    {
      ZIP64_EXTRA_FIELD_CHUNK chunk;

      clear chunk;

      if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
          read (uzip.fd, out chunk'byte[0:4]) != 4)
      {
        return UNPK_SEEK_READ_FAILED;
      }
      extra_len_rest -= 4;
      pos +=  4;

      if (chunk.header_ID == 0x0001)   // ZIP64_EXTRA_FIELD_CHUNK
      {
        uint rest = chunk.size_of_chunk;
        if (rest > (uint)extra_len_rest)
          rest = (uint)extra_len_rest;
        if (rest > ZIP64_EXTRA_FIELD_CHUNK'size - 4)
          rest = ZIP64_EXTRA_FIELD_CHUNK'size - 4;

        if (read (uzip.fd, out chunk'byte[4:rest]) != (int)rest)
        {
          return UNPK_SEEK_READ_FAILED;
        }

        if (chunk.size_of_chunk >= 8)
        {
          e.uncompressed_size = chunk.uncompressed_size;
          if (chunk.size_of_chunk >= 16)
          {
            e.compressed_size = chunk.compressed_size;
            if (chunk.size_of_chunk >= 24)
              e.offset_of_local_file_header = chunk.offset_of_local_file_header;
          }
        }

        extra_len_rest -= (int)rest;
        pos += rest;
      }
      else   // some unknown chunk
      {
        uint rest = chunk.size_of_chunk;
        if (rest > (uint)extra_len_rest)
          rest = (uint)extra_len_rest;
        extra_len_rest -= (int)rest;
        pos += rest;
      }
    }

    pos += extra_len_rest;
  }

  pos += rec.file_comment_length;

  return 0;
}

//------------------------------------------------------------------------------------------

public int unzip_open_container (out UNZIP_CONTAINER uzip, wstring zip_filename)
{
  long size;

  clear uzip;

  uzip.fd = wopen (zip_filename);
  if (uzip.fd < 0)
    return uzip.fd;

  size = filesize (uzip.fd);

  {
    ZIP_END_OF_CENTRAL_DIRECTORY end_rec;
    long pos;

    pos = size - end_rec'size;

    clear end_rec;
    if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
        read (uzip.fd, out end_rec) != (int)end_rec'size)
    {
      unzip_close_container (ref uzip);
      return UNPK_SEEK_READ_FAILED;
    }

    if (end_rec.end_of_central_directory_signature != 0x06054b50)   // not found
    {
      // there can be a comment field, upto 64K large, at the end of the file
      byte buffer[65536+4];
      uint bufsize = buffer'size;
      int  i;

      pos -= 65536;
      if (pos < 0)
        pos = 0;
      if (bufsize > size)
        bufsize = (uint)size;

      clear buffer;
      if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
          read (uzip.fd, out buffer[0:bufsize]) != (int)bufsize)
      {
        unzip_close_container (ref uzip);
        return UNPK_SEEK_READ_FAILED;
      }

      for (i=65536; i>=0; i++)  // scan upwards
      {
        const uint SIG = 0x06054b50;
        if (memcmp (buffer[i:4], SIG'byte) == 0)   // found
          break;
      }

      if (i < 0)   // not found
      {
        unzip_close_container (ref uzip);
        return UNPK_NOT_ZIP;
      }

      pos += i;

      clear end_rec;

      if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
          read (uzip.fd, out end_rec) != (int)end_rec'size)
      {
        unzip_close_container (ref uzip);
        return UNPK_SEEK_READ_FAILED;
      }
    }

    if (end_rec.end_of_central_directory_signature != 0x06054b50)   // not found
    {
      unzip_close_container (ref uzip);
      return UNPK_NOT_ZIP;
    }

    {
      long directory_offset = end_rec.offset_of_start_of_central_directory;
      uint directory_count  = end_rec.total_number_of_central_directory_records;

      if (directory_offset == 0xffffffff || directory_count == 0xffff)
      {
        {
          ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR locator;

          clear locator;

          pos -= locator'size;

          if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
              read (uzip.fd, out locator) != (int)locator'size ||
              locator.end_of_central_directory_locator_signature != 0x07064b50)
          {
            unzip_close_container (ref uzip);
            return UNPK_SEEK_READ_FAILED;
          }

          pos = locator.offset_of_zip64_end_of_central_directory;
        }


        {
          ZIP64_END_OF_CENTRAL_DIRECTORY end64;

          clear end64;
          if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
              read (uzip.fd, out end64) != (int)end64'size ||
              end64.end_of_central_directory_signature != 0x06064b50)
          {
            unzip_close_container (ref uzip);
            return UNPK_SEEK_READ_FAILED;
          }

          if (end64.total_number_of_central_directory_records >= int'max)   // loading all directory consumes too much ram
          {
            unzip_close_container (ref uzip);
            return UNPK_TOO_MANY_ITEMS;
          }

          directory_offset = end64.offset_of_start_of_central_directory;
          directory_count  = (uint)end64.total_number_of_central_directory_records;
        }
      }

      pos = directory_offset;
      uzip.list = new FILE_INFO [directory_count];

      {
        uint i;
        for (i=0; i<directory_count; i++)
        {
          int rc = read_entry (ref uzip, ref pos, out uzip.list^[i]);
          if (rc < 0)
          {
            unzip_close_container (ref uzip);
            return rc;
          }
        }
      }
    }
  }

  return 0;
}

//------------------------------------------------------------------------------------------

public int unzip_number_of_files (UNZIP_CONTAINER uzip)
{
  if (uzip.list == null)
    return 0;
  return uzip.list^'length;
}

//------------------------------------------------------------------------------------------

public int unzip_get_file_size (ref UNZIP_CONTAINER uzip, int index, out long size)
{
  if (uzip.list == null || index < 0 || index >= uzip.list^'length)
  {
    size = 0L;
    return UNPK_BAD_INDEX;
  }

  size = uzip.list^[index].uncompressed_size;
  return 0;
}

//------------------------------------------------------------------------------------------

// 0 = none(store), 8 = deflate, 9 = deflate64
public int unzip_get_compression_method (ref UNZIP_CONTAINER uzip, int index, out int compression_method)
{
  if (uzip.list == null || index < 0 || index >= uzip.list^'length)
  {
    compression_method = 0;
    return UNPK_BAD_INDEX;
  }

  compression_method = uzip.list^[index].compression_method;
  return 0;
}

//------------------------------------------------------------------------------------------

public int unzip_get_file_datetime (ref UNZIP_CONTAINER uzip, int index, out DATE_TIME datetime)
{
  clear datetime;

  if (uzip.list == null || index < 0 || index >= uzip.list^'length)
    return UNPK_BAD_INDEX;

  {
    ref FILE_INFO f = uzip.list^[index];

    datetime.sec  = (int1)((f.file_last_modification_time & 31) << 1);     // 5 bits
    datetime.min  = (int1)(((f.file_last_modification_time >> 5) & 63));   // 6 bits
    datetime.hour = (int1)(((f.file_last_modification_time >> 11) & 31));  // 5 bits
    
    datetime.day   = (int1)(f.file_last_modification_date & 31);                  // 5 bits
    datetime.month = (int1)(((f.file_last_modification_date >> 5) & 15));         // 4 bits
    datetime.year  = (int2)(1980 + ((f.file_last_modification_date >> 9) & 127)); // 7 bits
  }    
  
  return 0;
}

//------------------------------------------------------------------------------------------

int io_read1 (ref UNZIP_CONTAINER uzip,
              out byte[]          buffer)
{
  uint len = buffer'size;
  if (uzip.in_pos + len > uzip.in_size)
    len = (uint)(uzip.in_size - uzip.in_pos);
  uzip.in_pos += len;
  clear buffer;
  return read (uzip.fd, out buffer[0 : len]);
}

//------------------------------------------------------------------------------------------

int io_write1 (ref UNZIP_CONTAINER uzip,
                   byte[]          buffer)
{
  uint len = buffer'size;
  if (uzip.out_pos + len > uzip.out_size)
    len = (uint)(uzip.out_size - uzip.out_pos);
  uzip.out_pos += len;
  if (len > 0)
    update_crc (ref uzip.crc32, buffer[0:len]);
  return write (uzip.out_fd, buffer[0:len]);
}

//------------------------------------------------------------------------------------------

// supports store, deflate & deflate64

public int unzip_extract_file (ref UNZIP_CONTAINER uzip, int index, wstring filename)
{
  if (uzip.list == null || index < 0 || index >= uzip.list^'length)
    return UNPK_BAD_INDEX;

  {
    FILE_INFO             f = uzip.list^[index];
    long                  pos;
    ZIP_LOCAL_FILE_HEADER rec;

    pos = f.offset_of_local_file_header;

    clear rec;
    if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
        read (uzip.fd, out rec) != (int)rec'size)
    {
      unzip_close_container (ref uzip);
      return UNPK_SEEK_READ_FAILED;
    }
    pos += rec'size + rec.filename_length + rec.extra_field_length;
    if (lseek (uzip.fd, pos, SEEK_SET) < 0)
    {
      unzip_close_container (ref uzip);
      return UNPK_SEEK_READ_FAILED;
    }

    uzip.crc32 = 0;

    if (f.compression_method == 0 ||  // store
        f.uncompressed_size == 0)     // zero-byte files MUST NOT generate compressed data !!
    {
      int fd, rc;

      fd = wcreate (filename);
      if (fd < 0)
      {
        unzip_close_container (ref uzip);
        return UNPK_CREATE_FILE_FAILED;
      }

      {
        long rest = f.uncompressed_size;
        uint chunk;
        byte buffer[8192];

        clear buffer;
        while (rest > 0)
        {
          chunk = buffer'size;
          if (chunk > rest)
            chunk = (uint)rest;

          if (read (uzip.fd, out buffer[0:chunk]) != (int)chunk ||
              write (fd, buffer[0:chunk]) != (int)chunk)
          {
            close (fd);
            wdelete_file (filename);
            unzip_close_container (ref uzip);
            return UNPK_SEEK_READ_WRITE_FAILED;
          }

          update_crc (ref uzip.crc32, buffer[0:chunk]);

          rest -= chunk;
        }
      }

      rc = close (fd);
      if (rc < 0)
      {
        wdelete_file (filename);
        unzip_close_container (ref uzip);
        return UNPK_CLOSE_FAILED;
      }
    }
    else if (f.compression_method == 8 || f.compression_method == 9)  // deflate or deflate64
    {
      int        rc;
      READ_AHEAD extra;

      uzip.out_fd = wcreate (filename);
      if (uzip.out_fd < 0)
      {
        unzip_close_container (ref uzip);
        return UNPK_CREATE_FILE_FAILED;
      }

      uzip.in_pos = 0;
      uzip.in_size = f.compressed_size;
      uzip.out_pos = 0;
      uzip.out_size = f.uncompressed_size;

      /* note: unpack() reads data in large blocks and will probably read  */
      /*       past the compression part. The data read but not used by    */
      /*       unpack will be returned in the structure 'extra'.           */

      rc = UNPK.unpack (ref uzip, io_read1, io_write1, out extra, deflate_64 => f.compression_method == 9);
      if (rc < 0)
      {
        close (uzip.out_fd);
        wdelete_file (filename);
        unzip_close_container (ref uzip);
        return rc;
      }

      _unused extra;

      rc = close (uzip.out_fd);
      if (rc < 0)
      {
        wdelete_file (filename);
        unzip_close_container (ref uzip);
        return UNPK_CLOSE_FAILED;
      }

      if (uzip.out_pos != uzip.out_size)
      {
        wdelete_file (filename);
        unzip_close_container (ref uzip);
        return UNPK_BAD_FILESIZE;
      }
    }
    else
    {
      unzip_close_container (ref uzip);
      return UNPK_UNSUPPORTED_COMPRESSION_METHOD;
    }

    if (uzip.crc32 != f.crc32_of_uncompressed_data)
    {
      wdelete_file (filename);
      unzip_close_container (ref uzip);
      return UNPK_BAD_CRC;
    }
  }

  return 0;
}

//------------------------------------------------------------------------------------------

public int unzip_get_filename (ref UNZIP_CONTAINER uzip, int index, out wstring(260) filename)
{
  clear filename;
  
  if (uzip.list == null || index < 0 || index >= uzip.list^'length)
    return UNPK_BAD_INDEX;

  {
    FILE_INFO             f = uzip.list^[index];
    long                  pos;
    ZIP_LOCAL_FILE_HEADER rec;
    char  utf_filename[3*260];
    wchar wfilename[6*260];

    pos = f.offset_of_local_file_header;

    clear rec;
    if (lseek (uzip.fd, pos, SEEK_SET) < 0 ||
        read (uzip.fd, out rec) != (int)rec'size)
    {
      return UNPK_SEEK_READ_FAILED;
    }
    pos += rec'size;
    
    _unused rec;
    
    if (lseek (uzip.fd, pos, SEEK_SET) < 0)
    {
      return UNPK_SEEK_READ_FAILED;
    }

    if (f.filename_in_utf8)
    {
      int target_length;
      
      clear utf_filename;
      if ((int)f.filename_length > utf_filename'length ||
          read (uzip.fd, out utf_filename[0 : f.filename_length]) != (int)f.filename_length)
      {
        return UNPK_SEEK_READ_FAILED;
      }

      utf8_to_utf16 (utf_filename, f.filename_length, out wfilename, out target_length);
      
      wstrncpy (out filename, wfilename[0:target_length], filename'length);
    }
    else  // in ansi or dos
    { 
      int target_length;
      
      clear utf_filename;
      
      if (f.filename_length > 260 ||
          read (uzip.fd, out utf_filename[0 : f.filename_length]) != (int)f.filename_length)
      {
        return UNPK_SEEK_READ_FAILED;
      }

#if WINDOWS
      if (f.filename_in_msdos)
      {
#begin unsafe     
        target_length = MultiByteToWideChar 
                                (CodePage       => 437,
                                 dwFlags        => MB_PRECOMPOSED,
                                 lpMultiByteStr => &utf_filename,
                                 cbMultiByte    => f.filename_length,
                                 lpWideCharStr  => &wfilename,
                                 cchWideChar    => wfilename'length);
        if (target_length == 0)
        {
          return UNPK_SEEK_READ_FAILED;
        }
#end unsafe
      }
      else
#endif      
      {
        ascii_to_utf16 (utf_filename, f.filename_length, out wfilename, out target_length);
      }
      
      wstrncpy (out filename, wfilename[0:target_length], filename'length);
    }
  }
  
  return 0;
}

//-------------------------------------------------------------------------
// 1) non-standard format to compress/uncompress
//-------------------------------------------------------------------------

#begin unsafe

package MEM

  struct DATA
  {
    byte* Rbuffer;
    int   Rsize;
    int   Rofs;
    byte* Wbuffer;
    int   Wsize;
    int   Wofs;
    bool  manage_Wbuffer;   // true = our layer allocated Wbuffer and will manage it in io_write(), false = it's a user buffer.
  }

  package P1 = new PACK   (USER_INFO => DATA);
  package P2 = new UNPACK (USER_INFO => DATA);

end MEM;

/***********************************************************************/

int io_read (ref DATA   info,
             out byte[] buffer)
{
  int len;
  byte *p = &buffer;
  len = info.Rsize - info.Rofs;
  if (len > buffer'length)
    len = buffer'length;
  p[0:len] = info.Rbuffer[info.Rofs:len];
  info.Rofs += len;
  return len;
}

/***********************************************************************/

int io_write (ref DATA info,
              byte[]   buffer)
{
  int len = buffer'length;

  if (len > info.Wsize - info.Wofs)   // not enough space in buffer
  {
    if (info.manage_Wbuffer)
    {
      while (len > info.Wsize - info.Wofs)  // not enough space in buffer
      {
        info.Wsize <<= 1;   // double buffer size
        info.Wbuffer = memory.realloc (info.Wbuffer, (uint)info.Wsize);
      }
    }
    else   // no buffer management, just write what you can
    {
      len = info.Wsize - info.Wofs;
    }
  }

  info.Wbuffer[info.Wofs:len] = buffer[0:len];
  info.Wofs += len;
  return len;
}

/***********************************************************************/

public void compress_block (    byte[] block_in,
                            out byte[] block_out,
                            out int    block_out_size)
{
  DATA d;

  d = {Rbuffer => &block_in,
       Rsize   => block_in'length,
       Rofs    => 0,

       Wbuffer => &block_out,
       Wsize   => block_out'length,
       Wofs    => 1,

       manage_Wbuffer => false};

  if (P1.pack (ref d, io_read, io_write) < 0 || d.Wofs > 1+block_in'length)
  {
    block_out[0] = 0;   /* no compression */
    block_out[1:block_in'length] = block_in[0:block_in'length];
    d.Wofs = 1+block_in'length;
  }
  else
  {
    block_out[0] = 1;   /* compression */
  }

  block_out_size = d.Wofs;
}

/***********************************************************************/

public void decompress_block (    byte[] block_in,
                              out byte[] block_out,
                              out int block_out_size)
{
  DATA d;
  READ_AHEAD extra;

  byte* p = &block_out;
  _unused p;

  if (block_in[0] == 0)   /* no compression */
  {
    block_out[0:block_in'length-1] = block_in[1:block_in'length-1];
    block_out_size = block_in'length-1;
    return;
  }

  d = {Rbuffer => &block_in,
       Rsize   => block_in'length,
       Rofs    => 1,

       Wbuffer => &block_out,
       Wsize   => block_out'length,
       Wofs    => 0,

       manage_Wbuffer => false};

  if (P2.unpack (ref d, io_read, io_write, out extra) < 0)
    d.Wofs = 0;
  _unused extra;

  block_out_size = d.Wofs;
}

/***********************************************************************/

// returns null if decompression failed

public byte[]^ decompress_and_allocate_block (byte[] block_in)
{
  DATA       d;
  READ_AHEAD extra;
  int        size;
  byte[]^    p;

  if (block_in[0] == 0)   // no compression
    return new byte[] ' (block_in[1 : block_in'length-1]);

  size = (block_in'length + 4095) & (-4096);  // allocate at least compressed size, rounded up by multiple of 4K

  d = {Rbuffer => &block_in,
       Rsize   => block_in'length,
       Rofs    => 1,

       Wbuffer => memory.malloc ((uint)size),
       Wsize   => size,
       Wofs    => 0,

       manage_Wbuffer => true};

  if (P2.unpack (ref d, io_read, io_write, out extra) < 0)
  {
    memory.freem (d.Wbuffer);
    return null;
  }
  _unused extra;

  p = new byte[]'(d.Wbuffer[0 : d.Wofs]);
  memory.freem (d.Wbuffer);
  return p;
}

/***********************************************************************/
#end unsafe
/***********************************************************************/

package P

  struct FDATA
  {
    int fin;
    int fout;
  }

  package P3 = new PACK   (USER_INFO => FDATA);
  package P4 = new UNPACK (USER_INFO => FDATA);

end P;

/***********************************************************************/

int fio_read (ref FDATA  f,
              out byte[] buffer)
{
  return read (f.fin, out buffer);
}

/***********************************************************************/

int fio_write (ref FDATA f,
               byte[]    buffer)
{
  return write (f.fout, buffer);
}

/***********************************************************************/

public int compress_file (string source, string target)
{
  FDATA f;
  int   rc;
  const byte flag = 1;  // compress
  
  clear f;

  f.fin = open (source);
  if (f.fin < 0)
    return -1;

  f.fout = create (target);
  if (f.fout < 0)
  {
    close (f.fin);
    return -1;
  }

  if (write (f.fout, flag) != 1)  // write compression flag
  {
    close (f.fin);
    close (f.fout);
    delete_file (target);
    return -1;
  }
  
  rc = P3.pack (ref f, fio_read, fio_write);

  if (close (f.fin) < 0)
    rc = -1;

  if (close (f.fout) < 0)
    rc = -1;

  if (rc < 0)
    delete_file (target);

  return rc;
}

/***********************************************************************/

public int decompress_file (string source, string target)
{
  FDATA f;
  byte  flag;
  int   rc;
  READ_AHEAD extra;
  
  clear f;

  f.fin = open (source);
  if (f.fin < 0)
    return -1;

  f.fout = create (target);
  if (f.fout < 0)
  {
    close (f.fin);
    return -1;
  }

  if (read (f.fin, out flag) != 1)  // read compression flag
  {
    close (f.fin);
    close (f.fout);
    delete_file (target);
    return -1;
  }

  if (flag == 1)
  {
    rc = P4.unpack (ref f, fio_read, fio_write, out extra);
    _unused extra;
  }
  else   // no compression
  {
    byte buffer[4096];
    
    for (;;)
    {
      rc = read (f.fin, out buffer);
      if (rc < 0)
        break;
      if (write (f.fout, buffer) != rc)
      {
        rc = -1;
        break;
      }
      if (rc < buffer'length)
        break;
    }
  }
  
  if (close (f.fin) < 0)
    rc = -1;

  if (close (f.fout) < 0)
    rc = -1;

  if (rc < 0)
    delete_file (target);

  return rc;
}

/***********************************************************************/

package body PACK

  package PACK1 = new GEN_PACK (USER_INFO => USER_INFO);

  //-------------------------------------------------------------------

  public int pack (ref USER_INFO user_info, IO_READ user_read, IO_WRITE user_write)
  {
    return PACK1.pack (ref user_info, user_read, user_write);
  }

  //-------------------------------------------------------------------

end PACK;



package body UNPACK

  package UNPACK1 = new GEN_UNPACK (USER_INFO => USER_INFO);

  //-------------------------------------------------------------------

  public int unpack (ref USER_INFO user_info, 
                         IO_READ   user_read, 
                         IO_WRITE   user_write, 
                     out READ_AHEAD extra,
                         bool       deflate_64 = false)
  {
    return UNPACK1.unpack (ref user_info, user_read, user_write, out extra, deflate_64);
  }

  //-------------------------------------------------------------------

end UNPACK;
