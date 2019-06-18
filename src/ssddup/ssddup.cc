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
  std::cout << sizeof(Chunk) << std::endl;
  _chunk_module = std::make_unique<ChunkModule>();
  std::shared_ptr<IOModule> io_module = std::make_shared<IOModule>();
  //io_module->add_cache_device("./ramdisk/cache_device");
  io_module->add_cache_device("/dev/sda");
  io_module->add_primary_device("/dev/sdb");
  std::shared_ptr<MetadataModule> metadata_module =
    std::make_shared<MetadataModule>(io_module);
  _deduplication_module = std::make_unique<DeduplicationModule>(metadata_module);
  _compression_module = std::make_unique<CompressionModule>();
  _manage_module = std::make_unique<ManageModule>(io_module, metadata_module);
}

void SSDDup::read(uint64_t addr, void *buf, uint32_t len)
{
  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);
  alignas(512) Chunk c;

  while ( chunker.next(c) ) {
    alignas(512) uint8_t temp_buffer[Config::chunk_size];
    if (c.is_partial()) {
      alignas(512) Chunk read_chunk = c.construct_read_chunk(temp_buffer);
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
  static uint32_t n_access = 0;
  static uint32_t n_hits = 0;
  n_access++;

  // construct compressed buffer for chunk c
  // When the cache is hit, this is used to store the data
  // retrieved from ssd 
  uint8_t compressed_buf[Config::chunk_size];
  c._compressed_buf = compressed_buf;

  // look up index and find deduplication
  _deduplication_module->deduplicate(c, false);

  // read from ssd or hdd according to lookup result
  _manage_module->read(c);

  if (c._lookup_result == READ_HIT) {
    _compression_module->decompress(c);
    //n_hits ++;
  } else if (update_metadata && c._lookup_result == READ_NOT_HIT) {
    _compression_module->compress(c);
    c.fingerprinting();
    _manage_module->write(c);
  }

  if (c._lookup_result == READ_HIT) {
    //std::cout << "n_hits: " << n_hits << " n_access: " << n_access << std::endl;
  }
}

void SSDDup::write(uint64_t addr, void *buf, uint32_t len)
{
  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);
  alignas(512) Chunk c;

  while ( chunker.next(c) ) {
    // this read_buffer resides in the application stack
    // and will be wrapped by read_chunk
    // to avoid memory allocation overhead
    // it is small 8K/32K memory overhead
    alignas(512) uint8_t temp_buffer[Config::chunk_size];
    if (c.is_partial()) {
      // read-modify-write is introduced
      Chunk read_chunk = c.construct_read_chunk(temp_buffer);
      // read
      internal_read(read_chunk, /* update_index = */ false);

      // solve_conflict and construct new write chunk with
      // the read_buffer as content
      // modify
      c.merge_write(read_chunk);
    }
    c._compressed_buf = temp_buffer;
    internal_write(c);
  }
}

void SSDDup::internal_write(Chunk &c)
{
  c.fingerprinting();
  _deduplication_module->deduplicate(c, true);
  if (c._lookup_result == WRITE_NOT_DUP)
    _compression_module->compress(c);
  _manage_module->write(c);
}

void SSDDup::write_TEST(uint64_t addr, void *buf, uint32_t len)
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
    c.fingerprinting();
    char fp_char_array[Config::ca_length + 1];
    fp_char_array[Config::ca_length] = '\0';
    memcpy(fp_char_array, c._ca, Config::ca_length);
    std::string fp(fp_char_array);
    if (_fingerprints.count(fp) != 0) {
      std::cout << "Collide fingerprint!" << std::endl;
    } else
      _fingerprints.insert(fp);
  }
}
}
