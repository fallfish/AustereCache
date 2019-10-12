#ifndef __COMPRESSIONMODULE_H__
#define __COMPRESSIONMODULE_H__

#include "common/common.h"
#include "chunk/chunkmodule.h"

namespace cache {

class CompressionModule {
 public:
  CompressionModule() {}
  void compress(Chunk &chunk);
  void decompress(Chunk &chunk);
  void decompress(uint8_t *compressedBuf, uint8_t *buf, uint32_t compressedLen, uint32_t originalLen);
};
}

#endif //__COMPRESSIONMODULE_H__
