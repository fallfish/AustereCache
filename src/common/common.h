#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <iostream>
#include "common/config.h"
#include "utils/utils.h"
#include <atomic>
#define DIRECT_IO

namespace cache {

/**
 * @brief Metadata is an on-ssd data structure storing Full-CA and Full-LBAs
 *        When a chunk matches both LBA index and CA index for prefix matching,
 *        metadata is fetched from SSD to verify if the chunk is duplicate or not.
 */
struct Metadata {
  uint8_t  _ca[20];
  uint8_t  _strong_ca[20];
  uint32_t _reference_count;
  uint64_t _lbas[37]; // 4 * 32
  uint32_t _num_lbas;
  // If the data is compressed, the compressed_len is valid, otherwise, it is 0.
  uint32_t _compressed_len;
  // 512 bytes alignment for DMA access
  uint8_t _[76 + 80];
};

enum DedupResult {
  DUP_WRITE, DUP_CONTENT, NOT_DUP, DEDUP_UNKNOWN
};

enum LookupResult {
  HIT, NOT_HIT, LOOKUP_UNKNOWN
};

enum VerificationResult {
  BOTH_LBA_AND_CA_VALID, ONLY_CA_VALID, ONLY_LBA_VALID, BOTH_LBA_AND_CA_NOT_VALID, VERIFICATION_UNKNOWN
};

/*
 * The basic read/write unit.
 * An object of class Chunk is passed along the data path of a single request.
 * Members:
 *   1. address, buffer, length of data to be read/write
 *   2. (original) address, buffer, length of the requested data
 *     Here the buffer refers to the one passed in the corresponding request.
 *     Note: The write data or read buffer of the original request is managed
 *           by the caller. It would be efficient if we can manage to pass
 *           the original buffer to the underlying I/O request. However,
 *           in cases where the request is not well aligned, we need to firstly
 *           do the aligned I/O request and read or write data with a newly
 *           created temporary buffer.
 *   3. content address and its hash, logical block address hash
 *      ssd location (data pointer)
 *      These members are Deduplication and index related
 *   4. lookup_result (write dup, read hit, etc.), verification_result (lba valid, ca valid, etc.)
 *   5. lba_hit, ca_hit, for performance statistics
 *   6. compressed_buf, compressed_len, compress_level
 *      Compression related.
 *      Compress_level varies from [0, 1, 2, 3]
 *      which specifies 25%, 50%, 75%, 100% compression ratio, respectively.
 *
 *   7. (CacheDedup-DARC-specific) weu_id, weu_offset, and evicted_weu_id from the decision of the DARC_FingerprintIndex.
 **/

struct Chunk {
    Metadata _metadata;

    uint64_t _addr;
    uint32_t _len;
    uint8_t *_buf;

    uint8_t *_compressed_buf;
    uint32_t _compressed_len;
    uint32_t _compress_level; // compression level: 0, 1, 2, 3 representing 1, 2, 3, 4 * 8 KiB

    // For unaligned read/write request
    // We store the original request here
    // and merge the aligned data
    uint64_t _original_addr;
    uint32_t _original_len;
    uint8_t *_original_buf;

    uint8_t  _ca[20];
    uint8_t  _strong_ca[20];
    uint32_t _lba_hash;
    uint32_t _ca_hash;
    bool     _has_ca;

    uint64_t _ssd_location;
    uint64_t _metadata_location;

    bool _lba_hit;
    bool _ca_hit;
    DedupResult _dedup_result;
    LookupResult _lookup_result;
    VerificationResult _verification_result;

    // For multithreading, indexing update must be serialized
    // Bucket-level locks are used to guarantee the consistency of index
    std::unique_ptr<std::lock_guard<std::mutex>> _lba_bucket_lock;
    std::unique_ptr<std::lock_guard<std::mutex>> _ca_bucket_lock;

#ifdef CDARC
    uint32_t _weu_id;
    uint32_t _weu_offset;
    uint32_t _evicted_weu_id;
#endif

    Chunk() {}
    Chunk(const Chunk &c) {
      _addr = c._addr;
      _len = c._len;
      _buf = c._buf;

      _has_ca = false;
      _lba_hit = false;
      _ca_hit = false;
      _verification_result = VERIFICATION_UNKNOWN;
      _lookup_result = LOOKUP_UNKNOWN;

      compute_lba_hash();
    }
    /**
     * @brief compute fingerprint of current chunk.
     *        Require: a write chunk, address is aligned
     */
    void fingerprinting();
    void compute_strong_ca();
    void compute_lba_hash();
    void TEST_fingerprinting();
    void TEST_compute_lba_hash();
    inline bool is_end() { return _len == 0; }
    inline bool is_aligned() { 
#ifdef DIRECT_IO
      return _len == Config::get_configuration().get_chunk_size()
        && (long long)_buf % 512 == 0;
#else
      return _len == Config::get_configuration().get_chunk_size();
#endif
    }
    
    /**
     * @brief If the chunk is unaligned, we must pre-read the corresponding
     *        aligned area.
     *        Buf is provided here as the _buf, while the previous _buf, _addr, _length
     *        given from the caller is moved to _original_buf
     *        A well-aligned chunk for read is created after preprocess_unaligned.
     *
     * @param buf
     */
    void preprocess_unaligned(uint8_t *buf);

    /**
     * @brief In the write path, if the original chunk is not aligned, after read
     *        the corresponding aligned area, we need to merge the content with
     *        the requested content, and continue the write process.
     *
     */
    void merge_write();

    /**
     * @brief In the read path, if the original chunk is not aligned, after read
     *        the corresponding aligned area, we need to copy the needed content
     *        for the temporary buffer to the buffer of the caller.
     */
    void merge_read();
};


/*
 * class Stats is used to statistic in the data path.
 * It is a singleton class with std::atomic to ensure
 * thread-safe.
 */
struct Stats {
  // store number of each lookup results
  // write_dup_write, write_dup_content, write_not_dup,
  // read_hit, read_not_hit
  std::atomic<int> _n_lookup_results[3];
  std::atomic<int> _n_dedup_results[4];
  // store number of writes and reads
  // aligned write, aligned read,
  // write request, read request
  // Note: unaligned write requests would trigger read-modify-write
  std::atomic<int> _n_requests[4];
  // store number of bytes of write and read
  // aligned write bytes, aligned read bytes
  // requested write bytes, requested read bytes
  std::atomic<uint64_t> _total_bytes[4];
  // lba hit, ca hit
  std::atomic<int> _index_hit[2];
  // compression level
  std::atomic<int> _compress_level[4];
  // number of metadata write, number of data write
  // number of metadata read, number of data read
  std::atomic<int> _n_data_write_read[6];

  // stats in write buffer
  std::atomic<uint64_t> _n_bytes_written_to_write_buffer;
  std::atomic<uint64_t> _n_bytes_written_to_cache_disk;
  std::atomic<uint64_t> _n_bytes_written_to_primary_disk;
  std::atomic<uint64_t> _n_bytes_read_from_write_buffer;
  std::atomic<uint64_t> _n_bytes_read_from_cache_disk;
  std::atomic<uint64_t> _n_bytes_read_from_primary_disk;
  inline void add_bytes_written_to_write_buffer(uint64_t v) { _n_bytes_written_to_write_buffer.fetch_add(v, std::memory_order_relaxed); }
  inline void add_bytes_written_to_cache_disk(uint64_t v) {   _n_bytes_written_to_cache_disk  .fetch_add(v, std::memory_order_relaxed); }
  inline void add_bytes_written_to_primary_disk(uint64_t v) { _n_bytes_written_to_primary_disk.fetch_add(v, std::memory_order_relaxed); }
  inline void add_bytes_read_from_write_buffer(uint64_t v) {  _n_bytes_read_from_write_buffer .fetch_add(v, std::memory_order_relaxed); }
  inline void add_bytes_read_from_cache_disk(uint64_t v) {    _n_bytes_read_from_cache_disk   .fetch_add(v, std::memory_order_relaxed); }
  inline void add_bytes_read_from_primary_disk(uint64_t v) {  _n_bytes_read_from_primary_disk .fetch_add(v, std::memory_order_relaxed); }

  static Stats* get_instance() {
    static Stats instance;
    return &instance;
  }
  Stats() {
    reset();
  }

  inline void add_write_request() { _n_requests[0].fetch_add(1, std::memory_order_relaxed); }
  inline void add_write_bytes(uint32_t bytes) { _total_bytes[0].fetch_add(bytes, std::memory_order_relaxed); }
  inline void add_read_request() { _n_requests[1].fetch_add(1, std::memory_order_relaxed); }
  inline void add_read_bytes(uint32_t bytes) { _total_bytes[1].fetch_add(bytes, std::memory_order_relaxed); }
  inline void add_internal_write_request() { _n_requests[2].fetch_add(1, std::memory_order_relaxed); }
  inline void add_internal_write_bytes(uint32_t bytes) { _total_bytes[2].fetch_add(1, std::memory_order_relaxed); }
  inline void add_internal_read_request() { _n_requests[3].fetch_add(1, std::memory_order_relaxed); }
  inline void add_internal_read_bytes(uint32_t bytes) { _total_bytes[3].fetch_add(bytes, std::memory_order_relaxed); }

  inline void add_lookup_result(LookupResult lookup_result)
  {
    _n_lookup_results[lookup_result].fetch_add(1, std::memory_order_relaxed);
  }
  inline void add_dedup_result(DedupResult dedup_result)
  {
    _n_dedup_results[dedup_result].fetch_add(1, std::memory_order_relaxed);
  }

  inline void add_compress_level(int compress_level) 
  {
    _compress_level[compress_level].fetch_add(1, std::memory_order_relaxed);
  }

  inline void add_metadata_write()
  {
    _n_data_write_read[0].fetch_add(1, std::memory_order_relaxed);
  }
  inline void add_metadata_read()
  {
    _n_data_write_read[1].fetch_add(1, std::memory_order_relaxed);
  }
  inline void add_cache_data_read()
  {
    _n_data_write_read[2].fetch_add(1, std::memory_order_relaxed);
  }
  inline void add_cache_data_write()
  {
    _n_data_write_read[3].fetch_add(1, std::memory_order_relaxed);
  }
  inline void add_primary_data_read()
  {
    _n_data_write_read[4].fetch_add(1, std::memory_order_relaxed);
  }
  inline void add_primary_data_write()
  {
    _n_data_write_read[5].fetch_add(1, std::memory_order_relaxed);
  }


  inline void add_lba_hit(bool lba_hit) {
    _index_hit[0].fetch_add(lba_hit, std::memory_order_relaxed);
  }
  inline void add_ca_hit(bool ca_hit) {
    _index_hit[1].fetch_add(ca_hit, std::memory_order_relaxed);
  }

  void reset()
  {
    for (int i = 0; i < 6; i++) _n_lookup_results[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < 4; i++) _n_requests[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < 4; i++) _total_bytes[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < 2; i++) _index_hit[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < 4; i++) _compress_level[i].store(0, std::memory_order_relaxed);
    for (int i = 0; i < 6; i++) _n_data_write_read[i].store(0, std::memory_order_relaxed);
    _n_bytes_written_to_write_buffer .store(0, std::memory_order_relaxed);
    _n_bytes_written_to_cache_disk   .store(0, std::memory_order_relaxed);
    _n_bytes_written_to_primary_disk .store(0, std::memory_order_relaxed);
    _n_bytes_read_from_write_buffer  .store(0, std::memory_order_relaxed);
    _n_bytes_read_from_cache_disk    .store(0, std::memory_order_relaxed);
    _n_bytes_read_from_primary_disk  .store(0, std::memory_order_relaxed);
  }

  void dump()
  {
  std::cout
    << "Number of write request: " << _n_requests[0] << std::endl
    << "Number of write bytes: " <<   _total_bytes[0] << std::endl
    << "Number of read request: " <<  _n_requests[1] << std::endl
    << "Number of read bytes: " <<    _total_bytes[1] << std::endl
    << "Number of internal write request: " << _n_requests[2] << std::endl
    << "Number of internal write bytes: " <<   _total_bytes[2] << std::endl
    << "Number of internal read request: " <<  _n_requests[3] << std::endl
    << "Number of internal read bytes : " <<   _total_bytes[3] << std::endl
    << "Number of read hit: " << _n_lookup_results[0] << std::endl
    << "Number of read miss: " << _n_lookup_results[1] << std::endl
    << "Number of lba hit: " << _index_hit[0] << std::endl
    << "Number of ca hit: " << _index_hit[1] << std::endl
    << "Number of compressiblity levels: 1: " << _compress_level[0] 
    << ", 2: " << _compress_level[1] 
    << ", 3: " << _compress_level[2] 
    << ", 4: " << _compress_level[3] << std::endl
    //<< "Number of metadata read/write, cache disk read/write, primary disk read/write: " << _n_data_write_read[0]
    //<< ", " << _n_data_write_read[1]
    //<< ", " << _n_data_write_read[2]
    //<< ", " << _n_data_write_read[3]
    //<< ", " << _n_data_write_read[4]
    //<< ", " << _n_data_write_read[5] << std::endl
    << "Number of bytes read for write buffer: " << _n_bytes_read_from_write_buffer << std::endl
    << "Number of bytes written to write buffer: " << _n_bytes_written_to_write_buffer << std::endl
    << "Number of bytes read from cache disk: " << _n_bytes_read_from_cache_disk << std::endl
    << "Number of bytes written to cache disk: " << _n_bytes_written_to_cache_disk << std::endl
    << "Number of bytes read from primary disk: " << _n_bytes_read_from_primary_disk << std::endl
    << "Number of bytes written to primary disk: " << _n_bytes_written_to_primary_disk << std::endl;

    std::cout << "hit ratio: " << _n_lookup_results[0] * 1.0 / (_n_lookup_results[0] + _n_lookup_results[1]) * 100 << "%" << std::endl;
    std::cout << "dup ratio: " << _n_dedup_results[1] * 1.0 / (_n_dedup_results[0] + _n_dedup_results[1] + _n_dedup_results[2]) * 100 << "%" << std::endl;
  }
};

}
#endif //__COMMON_H__
