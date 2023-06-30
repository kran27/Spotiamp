#ifndef SPOTIAMP_ZIPFILE_H_
#define SPOTIAMP_ZIPFILE_H_

#include <string>
#include "types.h"

struct ZipFile;

class FileIO {
public:
  virtual ~FileIO() {}
  virtual size_t Read(int64 read_offs, void *buf, size_t buf_size) = 0;

  static FileIO *Create(const char *path);
};

class ZipFileReader {
public:
  static ZipFile *Open(FileIO *file_io, bool take_ownership);
  static bool ReadFileToString(ZipFile *zip, const char *filename, std::string *result);
  static int64 GetFileSize(ZipFile *zip, const char *filename);
  static void Free(ZipFile *zip);
};

#endif  // SPOTIAMP_ZIPFILE_H_

