#include "ssddup.h"
#include "common/config.h"
#include "chunk/chunkmodule.h"
#include "deduplication/deduplicationmodule.h"
#include "compression/compressionmodule.h"
#include "metadata/metadatamodule.h"
#include "manage/managemodule.h"

namespace cache {
SSDDup::SSDDup()
{
  _chunk_module = std::make_unique<ChunkModule>();
  std::shared_ptr<IOModule> io_module = std::make_shared<IOModule>();
  io_module->add_cache_device("./cache_device");
  io_module->add_primary_device("./primary_device");
  std::shared_ptr<MetadataModule> metadata_module =
    std::make_shared<MetadataModule>(io_module);
  _deduplication_module = std::make_unique<DeduplicationModule>(metadata_module);
  _compression_module = std::make_unique<CompressionModule>();
  _manage_module = std::make_unique<ManageModule>(io_module, metadata_module);
}

void SSDDup::read(uint64_t addr, void *buf, uint32_t len)
{
  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);
  Chunk c;

  while ( chunker.next(c) ) {
    uint8_t read_buffer[Config::chunk_size];
    if (c.is_partial()) {
      Chunk read_chunk = c.construct_read_chunk(read_buffer);
      internal_read(read_chunk, true);
      // solve conflict
      // (copy the content read to the destination buffer)
      c.merge_read(read_chunk);
    } else {
      internal_read(c, true);
    }
  }
}

void SSDDup::internal_read(Chunk &c, bool update_metadata)
{
  LookupResult lookup_result = _deduplication_module->deduplicate(c, false);
  _manage_module->read(c, lookup_result, update_metadata);
  _compression_module->decompress(c, lookup_result);
}

void SSDDup::write(uint64_t addr, void *buf, uint32_t len)
{

  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);
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
      c.merge_write(read_chunk);
    }
    internal_write(c, true);
  }
}

void SSDDup::internal_write(Chunk &c, bool update_metadata)
{
   LookupResult lookup_result = _deduplication_module->deduplicate(c, true);
   _compression_module->compress(c, lookup_result);
   _manage_module->write(c, lookup_result);
}
}
