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
  //io_module->add_cache_device("./ramdisk/cache_device");
  io_module->add_cache_device("/dev/sda");
  io_module->add_primary_device("/dev/sdb");
  //io_module->add_cache_device("./cache_device");
  //io_module->add_primary_device("./primary_device");
  std::shared_ptr<MetadataModule> metadata_module =
    std::make_shared<MetadataModule>(io_module);
  _deduplication_module = std::make_unique<DeduplicationModule>(metadata_module);
  _compression_module = std::make_unique<CompressionModule>();
  _manage_module = std::make_unique<ManageModule>(io_module, metadata_module);
  _stats = std::make_unique<Stats>();
}

SSDDup::~SSDDup() {
  std::cout
    << "Number of write request: " << _stats->_n_requests[0] << std::endl
    << "Number of write bytes: " <<   _stats->_total_bytes[0] << std::endl
    << "Number of read request: " <<  _stats->_n_requests[1] << std::endl
    << "Number of read bytes: " <<    _stats->_total_bytes[1] << std::endl
    << "Number of internal write request: " << _stats->_n_requests[2] << std::endl
    << "Number of internal write bytes: " <<   _stats->_total_bytes[2] << std::endl
    << "Number of internal read request: " <<  _stats->_n_requests[3] << std::endl
    << "Number of internal read bytes : " <<   _stats->_total_bytes[3] << std::endl
    << "Number of read hit: " << _stats->_n_lookup_results[3] << std::endl
    << "Number of read miss: " << _stats->_n_lookup_results[4] << std::endl;
}

void SSDDup::read(uint64_t addr, void *buf, uint32_t len)
{
  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);
  alignas(512) Chunk c;

  while ( chunker.next(c) ) {
    _stats->add_read_request();
    _stats->add_read_bytes(c._len);
    alignas(512) uint8_t temp_buffer[Config::get_configuration().get_chunk_size()];
    if (!c.is_aligned()) {
      c.preprocess_unaligned(temp_buffer);
      internal_read(c, true);
      c.merge_read();
    } else {
      internal_read(c, true);
    }
  }
}

void SSDDup::internal_read(Chunk &c, bool update_metadata)
{
  _stats->add_internal_read_request();
  _stats->add_internal_read_bytes(c._len);

  {
    // construct compressed buffer for chunk c
    // When the cache is hit, this is used to store the data
    // retrieved from ssd 
    alignas(512) uint8_t compressed_buf[Config::get_configuration().get_chunk_size()];
    c._compressed_buf = compressed_buf;

    // look up index and find deduplication
    _deduplication_module->deduplicate(c, false);

    // read from ssd or hdd according to lookup result
    _manage_module->read(c);

    if (c._lookup_result == READ_HIT && c._compressed_len != 0) {
      _compression_module->decompress(c);
    } else if (update_metadata && c._lookup_result == READ_NOT_HIT) {
      _compression_module->compress(c);
      c.fingerprinting();
      _manage_module->write(c);
    }
  }

  _stats->add_lookup_result(c._lookup_result);
  _stats->add_lba_hit(c._lba_hit);
  _stats->add_ca_hit(c._ca_hit);
}

void SSDDup::write(uint64_t addr, void *buf, uint32_t len)
{

  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);
  alignas(512) Chunk c;

  while ( chunker.next(c) ) {
    _stats->add_write_request();
    _stats->add_write_bytes(c._len);
    // this read_buffer resides in the application stack
    // and will be wrapped by read_chunk
    // to avoid memory allocation overhead
    // it is small 8K/32K memory overhead
    alignas(512) uint8_t temp_buffer[Config::get_configuration().get_chunk_size()];
    if (!c.is_aligned()) {
      // read-modify-write is introduced
      c.preprocess_unaligned(temp_buffer);
      // read
      internal_read(c, /* update_index = */ false);

      // modify
      c.merge_write();
    }
    c._compressed_buf = temp_buffer;
    internal_write(c);
  }
}

void SSDDup::internal_write(Chunk &c)
{
  _stats->add_internal_write_request();
  _stats->add_internal_write_bytes(c._len);

  {
    c.fingerprinting();
    _deduplication_module->deduplicate(c, true);
    if (c._lookup_result == WRITE_NOT_DUP)
      _compression_module->compress(c);
    _manage_module->write(c);
  }

  _stats->add_lookup_result(c._lookup_result);
  _stats->add_lba_hit(c._lba_hit);
  _stats->add_ca_hit(c._ca_hit);
}

void SSDDup::TEST_write(int device, uint64_t addr, void *buf, uint32_t len)
{

}

void SSDDup::TEST_read(int device, uint64_t addr, void *buf, uint32_t len)
{

}

}
