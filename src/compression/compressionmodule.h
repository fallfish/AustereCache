#ifndef __COMPRESSIONMODULE_H__
#define __COMPRESSIONMODULE_H__

#include "common/common.h"
#include "chunk/chunkmodule.h"

namespace cache {

class CompressionModule {
 public:
  CompressionModule() {}
  void compress(Chunk &c) {
    c._compressed_buf = c._buf;
    c._compress_level = 4;
    return ;
  }
  void decompress(Chunk &c) {
//    c._buf = c._compressed_buf;
    c._compress_level = 4;
    return ;
  }
 private:
  bool compressability_check(Chunk &c) { return true; }
};
}

#endif //__COMPRESSIONMODULE_H__
