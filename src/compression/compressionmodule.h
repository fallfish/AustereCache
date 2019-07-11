#ifndef __COMPRESSIONMODULE_H__
#define __COMPRESSIONMODULE_H__

#include "common/common.h"
#include "chunk/chunkmodule.h"

namespace cache {

class CompressionModule {
 public:
  CompressionModule() {}
  void compress(Chunk &c);
  void decompress(Chunk &c);
  void decompress(uint8_t *_compressed_buf, uint8_t *_buf, uint8_t compressed_len, uint8_t original_len);
  void compress_TEST(const char *src, char *dest, int len, int &compressibility);
};
}

#endif //__COMPRESSIONMODULE_H__
