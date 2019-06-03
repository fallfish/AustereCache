#include "ssddup.h"
#include "common/config.h"
#include "chunk/chunkmodule.h"
#include "deduplication/deduplicationmodule.h"

namespace cache {
  void SSDDup::read(uint32_t addr, uint32_t length, uint8_t *buf)
  {
    Chunker chunker = _chunk_module.create_chunker(addr, length, buf);
    Chunk c;

    while ( chunker.next(c) ) {
      uint8_t read_buffer[Config::chunk_size];
      if (c.is_partial()) {
        Chunk read_chunk = c.construct_read_chunk(read_buffer);
        //internal_read(read_chunk, true);
        // solve conflict
        // (copy the content read to the destination buffer)
        //c.solve_conflict(read_chunk);
      } else {
        //internal_read(read_chunk, true);
      }
    }
  }

  void SSDDup::internal_read(const Chunk &c, bool update_metadata)
  {
    //DeduplicationModule deduplication_module(c);

  }

  void SSDDup::write(uint32_t addr, uint32_t length, uint8_t *buf)
  {

    Chunker chunker = _chunk_module.create_chunker(addr, length, buf);
    Chunk c;

    while ( chunker.next(c) ) {
      // this read_buffer resides in the application stack
      // and will be wrapped by read_chunk
      // to avoid memory allocation overhead
      // it is small 8K/32K memory overhead
      uint8_t read_buffer[Config::chunk_size];
      if (c.is_partial()) {
        // read-modify-write is introduced
        Chunk read_chunk = c.construct_read_chunk(read_buffer);
        // read
        internal_read(read_chunk, /* update_index = */ false);

        // solve_conflict and construct new write chunk with
        // the read_buffer as content
        // modify
        //c.solve_conflict(read_chunk);

        // write
        //internal_write(write_chunk, true);
      } else {
        //internal_write(write_chunk, true);
      }
    }
  }

  void SSDDup::internal_write(const Chunk &c, bool update_metadata)
  {
  }
}
