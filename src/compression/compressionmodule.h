#ifndef __COMPRESSIONMODULE_H__
#define __COMPRESSIONMODULE_H__

#include "common/common.h"
#include "chunk/chunkmodule.h"

namespace cache {

class CompressionModule {
 public:
  CompressionModule() {}
  void compress(Chunk &c, LookupResult lookup_result) {
    // only chunk with lookup_result WRITE_NOT_DUP or READ_NOT_HIT needs compressed
    c._compressed_buf = c._buf;
    c._compress_level = 4;
    return ;
  }
  void decompress(Chunk &c, LookupResult lookup_result) {
    // only chunk with lookup_result READ_HIT needs compressed
    c._buf = c._compressed_buf;
    c._compress_level = 4;
    return ;
  }
 private:
  bool compressability_check(Chunk &c) { return true; }
};
}

#endif //__COMPRESSIONMODULE_H__
