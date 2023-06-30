#ifndef TINFL_H_
#define TINFL_H_

#include "types.h"
#include "config.h"

#if TSP_WITH_GZIP

// Max size of LZ dictionary.
#define TINFL_LZ_DICT_SIZE 32768

// Size of an instance of the tinfl decompressor
#if defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
#define TINFL_INSTANCE_SIZE 11000
#else
#define TINFL_INSTANCE_SIZE 10988
#endif

// Decompression flags used by tinfl_decompress().
// TINFL_FLAG_PARSE_ZLIB_HEADER: If set, the input has a valid zlib header and ends with an adler32 checksum (it's a valid zlib stream). Otherwise, the input is a raw deflate stream.
// TINFL_FLAG_HAS_MORE_INPUT: If set, there are more input bytes available beyond the end of the supplied input buffer. If clear, the input buffer contains all remaining input.
// TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF: If set, the output buffer is large enough to hold the entire decompressed stream. If clear, the output buffer is at least the size of the dictionary (typically 32KB).
// TINFL_FLAG_COMPUTE_ADLER32: Force adler-32 checksum computation of the decompressed bytes.
enum {
  TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
  TINFL_FLAG_HAS_MORE_INPUT = 2,
  TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
  TINFL_FLAG_COMPUTE_ADLER32 = 8,
  TINFL_FLAG_PARSE_GZIP_HEADER = 16
};

//void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags);

// tinfl_decompress_mem_to_mem() decompresses a block in memory to another block in memory.
// Returns TINFL_DECOMPRESS_MEM_TO_MEM_FAILED on failure, or the number of bytes written on success.
#define TINFL_DECOMPRESS_MEM_TO_MEM_FAILED ((size_t)(-1))
//size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);

// tinfl_decompress_mem_to_callback() decompresses a block in memory to an internal 32KB buffer, and a user provided callback function will be called to flush the buffer.
// Returns 1 on success or 0 on failure.
typedef int (*tinfl_put_buf_func_ptr)(const void* pBuf, int len, void *pUser);
//int tinfl_decompress_mem_to_callback(const void *pIn_buf, size_t *pIn_buf_size, tinfl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags);

// This is the state required for decompression
typedef struct TspGunzip {
  // Before calling TspGunzipDoWork, set this to the input buffer
  const char *input;
  size_t input_size;

  // After TspGunzipDoWork, this will be set with to the decompressed data
  const char *output;
  size_t output_size;

  // Current offset into dictionary
  size_t dict_ofs;
  // tinfl dictionary
  byte dictionary[TINFL_LZ_DICT_SIZE];
  // tinfl instance
  byte tinfl[TINFL_INSTANCE_SIZE];
} TspGunzip;

// Initialize gunzipper
void TspGunzipInit(TspGunzip *gz);

// Decompress bytes. Returns < 0 on failure, 0 if more work left, or 1 if done.
int TspGunzipDoWork(TspGunzip *gz);

#endif  // TSP_WITH_GZIP

#endif

