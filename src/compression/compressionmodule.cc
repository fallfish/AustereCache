#include "compressionmodule.h"
#include "common/config.h"
#include "lz4.h"

#include <iostream>
#include <cstring>

namespace cache {

void CompressionModule::compress(Chunk &c)
{
#if defined(CDARC)
  c._compressed_len = LZ4_compress_default(
      (const char*)c._buf, (char*)c._compressed_buf,
      c._len, c._len - 1);
  if (c._compressed_len == 0) {
    c._compressed_len = c._len;
    c._compressed_buf = c._buf;
  }
#else
  c._compressed_len = LZ4_compress_default(
      (const char*)c._buf, (char*)c._compressed_buf,
      c._len, c._len * 0.75);
  double compress_ratio = c._compressed_len * 1.0 / c._len;
  if (compress_ratio > 0.75 || c._compressed_len == 0) {
    c._compress_level = 3;
    c._compressed_buf = c._buf;
  } else {
    if (compress_ratio > 0.5) {
      c._compress_level = 2;
    } else if (compress_ratio > 0.25) {
      c._compress_level = 1;
    } else {
      c._compress_level = 0;
    }
  }
#endif
  return ;
}

void CompressionModule::decompress(Chunk &c)
{
#if defined(CDARC)
  if (c._compressed_len != c._len)
#else
  if (c._compressed_len != 0)
#endif
    LZ4_decompress_safe((const char*)c._compressed_buf, (char*)c._buf,
        c._compressed_len, c._len);
  return ;
}

void CompressionModule::decompress(uint8_t *compressed_buf, uint8_t *buf, uint8_t compressed_len, uint8_t original_len)
{
  LZ4_decompress_safe((const char*)compressed_buf, (char*)buf,
      compressed_len, original_len);
  return ;
}

void CompressionModule::compress_TEST(const char *src, char *dest, int len, int &compressibility)
{
  int compressed_len = LZ4_compress_default(src, dest, len, 40000);
  compressibility = (compressed_len * 1.0 / 32768 < 0.25) ? 1 :
    (compressed_len * 1.0 / 32768 < 0.5)  ? 2 :
    (compressed_len * 1.0 / 32768 < 0.75) ? 3 : 4;
  //std::cout << "compressible: " << LZ4_compressBound(32768) << std::endl;
  //std::cout << "original len: " << len << std::endl;
  //std::cout << "compressed_len: " << compressed_len << std::endl;
  //std::cout << compressed_len * 1.0 / 32768 << std::endl;
}

}
