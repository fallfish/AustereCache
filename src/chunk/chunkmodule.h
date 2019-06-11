#ifndef __CHUNK_H__
#define __CHUNK_H__
#include <cstdint>
#include "common/config.h"

namespace cache {

  struct Chunk {
    uint32_t _addr;
    uint32_t _len;
    uint32_t _mode; // read, write, or read-modify-write
    //uint32_t _fingerprint;
    uint8_t *_buf;

    uint32_t _lba_hash;
    uint32_t _ca_hash;
    uint8_t  _ca[Config::ca_length];

    uint32_t _size; // compression level: 1, 2, 3, 4 * 8k
    uint32_t _ssd_location;


    void fingerprinting();
    inline bool is_end() { return _len == 0; }
    bool is_partial();
    Chunk construct_read_chunk(uint8_t *buf);
    void merge_write(const Chunk &c);
    void merge_read(const Chunk &c);
  };

  class Chunker {
   public:
    // initialize a chunking iterator
    // read is possibly triggered to deal with unaligned situation
    Chunker(uint8_t *buf, uint32_t len, uint32_t addr);
    // obtain next chunk, addr, len, and buf
    bool next(Chunk &c);
   protected:
    uint32_t _addr;
    uint32_t _len;
    uint8_t *_buf;
    uint32_t _chunk_size;
    uint32_t _mode;
  };

  class ChunkModule {
   public:
     ChunkModule();
     Chunker create_chunker(uint32_t addr, uint32_t len, uint8_t *buf);
  };
}

#endif
