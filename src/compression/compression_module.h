#ifndef __COMPRESSIONMODULE_H__
#define __COMPRESSIONMODULE_H__

#include "common/common.h"
#include "chunking/chunk_module.h"

namespace cache {

class CompressionModule {
  CompressionModule() = default;
 public:
  static CompressionModule& getInstance();
  static void compress(Chunk &chunk);
  static void decompress(Chunk &chunk);
  static void decompress(uint8_t *compressedBuf, uint8_t *buf, uint32_t compressedLen, uint32_t originalLen);
};
}

#endif //__COMPRESSIONMODULE_H__
