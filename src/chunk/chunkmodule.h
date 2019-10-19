#ifndef __CHUNK_H__
#define __CHUNK_H__
#include <cstdint>
#include "common/config.h"
#include "common/common.h"

namespace cache {

  class Chunker {
   public:
    // initialize a chunking iterator
    // read is possibly triggered to deal with unaligned situation
    Chunker(uint64_t addr, void *buf, uint32_t len);
    // obtain next chunk, addr, len, and buf
    bool next(Chunk &c);
    bool next(uint64_t &addr, uint8_t *&buf, uint32_t &len);
   protected:
    uint64_t addr_;
    uint8_t *buf_;
    uint32_t len_;
    uint32_t chunkSize_;
  };

  /**
   * (Commented by jhli)
   * A factory of class "Chunker"
   */
  class ChunkModule {
  private:
      ChunkModule();
   public:
      static ChunkModule& getInstance();
     Chunker createChunker(uint64_t addr, void *buf, uint32_t len);
  };
}

#endif
