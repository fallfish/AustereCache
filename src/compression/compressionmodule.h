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
  void compress_TEST(const char *src, char *dest, int len);

 private:
  bool compressability_check(Chunk &c) { return true; }
  char _compressed_buf[Config::chunk_size];
};
}

#endif //__COMPRESSIONMODULE_H__
