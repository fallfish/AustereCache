#include "ssddup.h"
#include "common/config.h"

#include <vector>
#include <thread>
#include <cassert>

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
  _thread_pool = std::make_unique<ThreadPool>(Config::get_configuration().get_max_num_global_threads());
}

SSDDup::~SSDDup() {
  _stats->dump();
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


void SSDDup::read(uint64_t addr, void *buf, uint32_t len)
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

    // read from ssd or hdd according to the lookup result
    _manage_module->read(c);


    if (c._lookup_result == READ_HIT) {
      // hit the cache
      if (c._compressed_len != 0) {
        // if data is compressed
        _compression_module->decompress(c);
      } else {
        // if data is not compressed, assign
        // the original buffer to compressed buffer
        // to update metadata
        c._compressed_buf = c._buf;
      }
    }
    
    if (c._verification_result != VERIFICATION_UNKNOWN)
      _stats->add_metadata_read();

    // In the read-modify-write path, we don't
    // update metadata, because the content will
    // be modified later.
    //update_metadata = false;
    if (update_metadata) {
      if (c._lookup_result == READ_NOT_HIT) {
        _compression_module->compress(c);
        c.fingerprinting();
      }
      _manage_module->update_metadata(c);
      if (c._lookup_result == READ_NOT_HIT
          // if the fetched ssd data duplicated, (i.e., ca not valid)
          // we have no need to write it to the cache
          && (c._verification_result == BOTH_LBA_AND_CA_NOT_VALID
            || c._verification_result == ONLY_LBA_VALID)) {
        // write compressed data into cache device
        _manage_module->write(c);

        _stats->add_cache_data_write();
      }

      if (c._lookup_result == READ_NOT_HIT) {
        _stats->add_metadata_read();
        if (c._verification_result == ONLY_CA_VALID)
          _stats->add_metadata_write();
      }
    }
  }

  if (c._lookup_result == READ_HIT) {
    _stats->add_cache_data_read();
  } else {
    _stats->add_primary_data_read();
  }

  _stats->add_compress_level(c._compress_level - 1);
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
    c._ca_bucket_lock.reset();
    c._lba_bucket_lock.reset();
  }
}

void SSDDup::internal_write(Chunk &c)
{
  _stats->add_internal_write_request();
  _stats->add_internal_write_bytes(c._len);

  {
    c.fingerprinting();
    _deduplication_module->deduplicate(c, true);
    if (c._lookup_result == WRITE_NOT_DUP) {
      _compression_module->compress(c);
    }
    _manage_module->update_metadata(c);
    _manage_module->write(c);
  }

  _stats->add_lookup_result(c._lookup_result);
  _stats->add_lba_hit(c._lba_hit);
  _stats->add_ca_hit(c._ca_hit);
}

}
