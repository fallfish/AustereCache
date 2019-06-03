#ifndef __SSDDUP_H__
#define __SSDDUP_H__
#include <cstdint>
#include "chunk/chunkmodule.h"

namespace cache {
  class SSDDup {
   public:
    void read(uint32_t addr, uint32_t length, uint8_t *buf);
    void internal_read(const Chunk &c, bool update_metadata);
    void write(uint32_t addr, uint32_t length, uint8_t *buf);
    void internal_write(const Chunk &c, bool update_metadata);
   private:
    ChunkModule _chunk_module;
  };
}


#endif
