#include "stdafx.h"
#include "zipfile.h"
#include "types.h"

#include <string>
#include <vector>
#include "tinfl.h"


struct ZipFileName {
  std::string name;
  uint32 file_offset;
  uint32 compress_size;
  uint32 decompress_size;
  uint16 method;
};

struct ZipFile {
  //String filename;
  std::vector<ZipFileName> list;
  FileIO *file_io;
  bool owns_file_io;
  ZipFile(FileIO *file_io, bool owns_file_io) : file_io(file_io), owns_file_io(owns_file_io) {}
};

// TODO: reading and writing of these structures should be made endianness-safe,
// even though windows is the only platform that uses ZipFileReader so far.
#if defined(_WIN32)
#pragma pack(push, 1)
#endif
struct ZipLocalFileHeader {
  uint32 sig;
  uint16 ver;
  uint16 flags;
  uint16 method;
  uint16 mod_time;
  uint16 mod_date;
  uint32 crc;
  uint32 compress_size;
  uint32 decompress_size;
  uint16 filename_len;
  uint16 extra_len;
};
struct ZipDescriptor {
  uint32 crc;
  uint32 compress_size;
  uint32 decompress_size;
};
#if defined(_WIN32)
#pragma pack(pop)
#endif

static bool ZipFile_ReadDirectoryInt(ZipFile *zip) {
  ZipLocalFileHeader hdr;
  char filename[256];
  uint32 offs = 0;
  // Read the zipfile
  while (zip->file_io->Read(offs, &hdr, sizeof(hdr)) == sizeof(hdr) && hdr.sig == 0x04034B50) {
    offs += sizeof(hdr);
    if (hdr.filename_len > sizeof(filename)) return false;
    if (zip->file_io->Read(offs, filename, hdr.filename_len) != hdr.filename_len) return false;
    offs += hdr.filename_len + hdr.extra_len;
    // Verify that the flags are ok.
    // Only deflate is supported
    if (hdr.flags & ((1 << 0) | (1<<3))) return false;
    if (hdr.method == 0) {
      if (hdr.compress_size != hdr.decompress_size) return false;
    } else if (hdr.method != 8) {
      return false;
    }
    // prevent inf loop by using neg size
    if (hdr.compress_size & 0x80000000) return false;
    {
      zip->list.resize(zip->list.size() + 1);
      ZipFileName &zfn = zip->list.back();
      zfn.compress_size = hdr.compress_size;
      zfn.decompress_size = hdr.decompress_size;
      zfn.name.assign(filename, hdr.filename_len);
      zfn.file_offset = offs;
      zfn.method = hdr.method;
    }
    offs += hdr.compress_size;
  }
  return true;
}

int64 ZipFileReader::GetFileSize(ZipFile *zip, const char *filename) {
  ZipFileName *zn;

  for(size_t i=0;;i++) {
    if (i == zip->list.size()) return -1;
    zn = &zip->list[i];
    if (stricmp(zn->name.c_str(), filename) == 0) break;
  }
  return zn->decompress_size;
}

extern "C" size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);


bool ZipFileReader::ReadFileToString(ZipFile *zip, const char *filename, std::string *result) {
  ZipFileName *zn;
  for(size_t i=0;;i++) {
    if (i == zip->list.size()) goto error;
    zn = &zip->list[i];
    if (stricmp(zn->name.c_str(), filename) == 0) break;
  }
  if ( (zn->compress_size|zn->decompress_size) >= 64*1024*1024) goto error;
  {
    result->resize(zn->compress_size);
    if (zip->file_io->Read(zn->file_offset, &(*result)[0], zn->compress_size) != zn->compress_size) goto error;
    if (zn->method == 8) {
      std::string tmp;
      std::swap(tmp, *result);
      result->resize(zn->decompress_size);
      uint32 destlen = zn->decompress_size;
      size_t r = tinfl_decompress_mem_to_mem(&(*result)[0], destlen, tmp.c_str(), tmp.size(), 0);
      if ((int)r < 0) goto error;
    }
    return true;
  }
error:
  result->clear();
  return false;
}

ZipFile *ZipFileReader::Open(FileIO *file_io, bool take_ownership) {
  ZipFile *zf = new ZipFile(file_io, take_ownership);
  ZipFile_ReadDirectoryInt(zf);
  return zf;
}

void ZipFileReader::Free(ZipFile *zip) {
  if (zip->owns_file_io)
    delete zip->file_io;
  delete zip;
}

