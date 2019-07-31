#include "ssddup.h"
#include "common/env.h"
#include "common/config.h"

#include <unistd.h>

#include <vector>
#include <thread>
#include <cassert>
#include <chrono>

namespace cache {
SSDDup::SSDDup()
{
  double vm, rss;
  process_mem_usage(vm, rss);
  std::cout << "VM: " << vm << "; RSS: " << rss << std::endl;
  _chunk_module = std::make_unique<ChunkModule>();
  std::shared_ptr<IOModule> io_module = std::make_shared<IOModule>();
  io_module->add_cache_device(Config::get_configuration().get_cache_device_name());
  io_module->add_primary_device(Config::get_configuration().get_primary_device_name());
  _compression_module = std::make_shared<CompressionModule>();
  std::shared_ptr<MetadataModule> metadata_module =
    std::make_shared<MetadataModule>(io_module, _compression_module);
  _deduplication_module = std::make_unique<DeduplicationModule>(metadata_module);
  _manage_module = std::make_unique<ManageModule>(io_module, metadata_module);
  _stats = Stats::get_instance();
  _thread_pool = std::make_unique<ThreadPool>(Config::get_configuration().get_max_num_global_threads());

  std::cout << sizeof(Metadata) << std::endl;
}

SSDDup::~SSDDup() {
  _stats->dump();
  double vm, rss;
  process_mem_usage(vm, rss);
  std::cout << "VM: " << vm << "; RSS: " << rss << std::endl;
}

void SSDDup::read(uint64_t addr, void *buf, uint32_t len)
{
  if (Config::get_configuration().get_multi_thread()) {
    read_mt(addr, buf, len);
  } else {
    read_singlethread(addr, buf, len);
  }
}

void SSDDup::read_mt(uint64_t addr, void *buf, uint32_t len)
{
  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);

  std::vector<int> threads;
  uint64_t tmp_addr;
  uint8_t *tmp_buf;
  uint32_t tmp_len;
  int thread_id;

  while (chunker.next(tmp_addr, tmp_buf, tmp_len)) {
    if (threads.size() == Config::get_configuration().get_max_num_local_threads()) {
      _thread_pool->wait_and_return_threads(threads);
    }
    while ( (thread_id = _thread_pool->allocate_thread()) == -1) {
      _thread_pool->wait_and_return_threads(threads);
    }
    _thread_pool->get_thread(thread_id) = std::make_shared<std::thread>(
        [this, tmp_addr, tmp_buf, tmp_len]() {
          read(tmp_addr, tmp_buf, tmp_len);
          });
    threads.push_back(thread_id);
  }
  _thread_pool->wait_and_return_threads(threads);
}


void SSDDup::read_singlethread(uint64_t addr, void *buf, uint32_t len)
{
  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);

  alignas(512) Chunk c;
  while (chunker.next(c)) {
    alignas(512) uint8_t temp_buffer[Config::get_configuration().get_chunk_size()];
    _stats->add_read_request();
    _stats->add_read_bytes(c._len);
    if (!c.is_aligned()) {
      c.preprocess_unaligned(temp_buffer);
      internal_read(c, true);
      c.merge_read();
    } else {
      internal_read(c, true);
    }
    c._ca_bucket_lock.reset();
    c._lba_bucket_lock.reset();
  }
}


void SSDDup::write(uint64_t addr, void *buf, uint32_t len)
{

  Chunker chunker = _chunk_module->create_chunker(addr, buf, len);
  alignas(512) Chunk c;

  while ( chunker.next(c) ) {
    //std::cout << c._addr << " " << c._len << std::endl;
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
    internal_write(c);
    c._ca_bucket_lock.reset();
    c._lba_bucket_lock.reset();
  }
}

#ifdef CACHE_DEDUP
//#ifdef DLRU
void SSDDup::internal_read(Chunk &c, bool update_metadata)
{
  _stats->add_internal_read_request();
  _stats->add_internal_read_bytes(c._len);

  _deduplication_module->lookup(c);
  _manage_module->read(c);

  if (update_metadata) {
    if (c._lookup_result == NOT_HIT) {
      c.fingerprinting();
      _deduplication_module->dedup(c);
      _manage_module->update_metadata(c);
      if (c._dedup_result == NOT_DUP) {
        _manage_module->write(c);
      }
    } else {
      _manage_module->update_metadata(c);
    }
  }

  if (c._lookup_result == HIT) {
    _stats->add_cache_data_read();
  } else {
    _stats->add_primary_data_read();
  }

  _stats->add_compress_level(c._compress_level);
  _stats->add_lookup_result(c._lookup_result);
  _stats->add_lba_hit(c._lba_hit);
  _stats->add_ca_hit(c._ca_hit);
}
void SSDDup::internal_write(Chunk &c)
{
  _stats->add_lba_hit(c._lba_hit);
  _stats->add_ca_hit(c._ca_hit);
  c.fingerprinting();
  _deduplication_module->dedup(c);
  _manage_module->update_metadata(c);
  _manage_module->write(c);
  _stats->add_dedup_result(c._dedup_result);
}
//#else

//#endif

#else
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

    // look up index
    _deduplication_module->lookup(c);
    // read from ssd or hdd according to the lookup result
    _manage_module->read(c);

    if (c._lookup_result == HIT) {
      // hit the cache
      if (c._compressed_len != 0) {
        // if data is compressed
        _compression_module->decompress(c);
      }
    }
    
    if (c._verification_result != VERIFICATION_UNKNOWN)
      _stats->add_metadata_read();

    // In the read-modify-write path, we don't
    // re-dedup the data read from HDD or update metadata
    // since the content will be modified later.
    if (update_metadata) {
      if (c._lookup_result == NOT_HIT) {
        // dedup the data, the same procedure as in the write
        // process.
        _compression_module->compress(c);
        c.fingerprinting();
        _deduplication_module->dedup(c);
        _manage_module->update_metadata(c);
        if (c._dedup_result == NOT_DUP) {
          // write compressed data into cache device
          _manage_module->write(c);
          _stats->add_cache_data_write();
        }
      } else {
        _manage_module->update_metadata(c);
      }

      if (c._lookup_result == NOT_HIT) {
        _stats->add_metadata_read();
        if (c._verification_result == ONLY_CA_VALID)
          _stats->add_metadata_write();
      }
    }
  }

  if (c._lookup_result == HIT) {
    _stats->add_cache_data_read();
  } else {
    _stats->add_primary_data_read();
  }

  _stats->add_compress_level(c._compress_level);
  _stats->add_lookup_result(c._lookup_result);
  _stats->add_lba_hit(c._lba_hit);
  _stats->add_ca_hit(c._ca_hit);
}
void SSDDup::internal_write(Chunk &c)
{
  c._lookup_result = LOOKUP_UNKNOWN;
  alignas(512) uint8_t temp_buffer[Config::get_configuration().get_chunk_size()];
  c._compressed_buf = temp_buffer;
  _stats->add_internal_write_request();
  _stats->add_internal_write_bytes(c._len);

  {
    c.fingerprinting();
    _deduplication_module->dedup(c);
    if (c._dedup_result == NOT_DUP) {
      _compression_module->compress(c);
    }
    _manage_module->update_metadata(c);
    _manage_module->write(c);
  }

  if (c._dedup_result == NOT_DUP || c._dedup_result == DUP_CONTENT)
    _stats->add_primary_data_write();
  _stats->add_lba_hit(c._lba_hit);
  _stats->add_ca_hit(c._ca_hit);
  _stats->add_dedup_result(c._dedup_result);
}

#endif
}
