#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include "common/config.h"
#include "utils/utils.h"

namespace cache {

// 512 bytes
struct Metadata {
  uint8_t _ca[128];
  uint64_t _lbas[37]; // 4 * 32
  uint32_t _num_lbas;
  uint32_t _compressed_len; // if the data is compressed, the compressed_len is valid, otherwise, it is 0.
  uint8_t _[80];
};

enum LookupResult {
  WRITE_DUP_WRITE, WRITE_DUP_CONTENT, WRITE_NOT_DUP,
  READ_HIT, READ_NOT_HIT, LOOKUP_UNKNOWN
};

enum VerificationResult {
  BOTH_LBA_AND_CA_VALID, ONLY_CA_VALID, ONLY_LBA_VALID, BOTH_LBA_AND_CA_NOT_VALID, VERIFICATION_UNKNOWN
};

/*
 * The basic read/write unit.
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
 *      Compress_level varies from [1, 2, 3, 4]
 *      which specifies 25%, 50%, 75%, 100% compression ratio, respectively.
 *
 **/

struct Chunk {
    Metadata _metadata;
    uint64_t _addr;
    uint32_t _len;
    uint8_t *_buf;

    uint8_t *_compressed_buf;
    uint32_t _compressed_len;
    uint32_t _compress_level; // compression level: 1, 2, 3, 4 * 8k

    // For unaligned read/write request
    // We store the original request here
    // and merge the aligned data
    uint64_t _original_addr;
    uint32_t _original_len;
    uint8_t *_original_buf;

    uint8_t  _ca[128]; // the largest possible content address length for ca
    uint32_t _lba_hash;
    uint32_t _ca_hash;
    bool     _has_ca;

    uint64_t _ssd_location;

    bool _lba_hit;
    bool _ca_hit;
    LookupResult _lookup_result;
    VerificationResult _verification_result;

    // For multithreading, indexing update must be serialized
    // Bucket-level locks are used to guarantee the consistency of index
    std::unique_ptr<std::lock_guard<std::mutex>> _lba_bucket_lock;
    std::unique_ptr<std::lock_guard<std::mutex>> _ca_bucket_lock;

    // compute finger print of current chunk.
    // Require: a write chunk, address is aligned
    void fingerprinting();
    void compute_lba_hash();
    void TEST_fingerprinting();
    void TEST_compute_lba_hash();
    inline bool is_end() { return _len == 0; }
    inline bool is_aligned() { return _len == Config::get_configuration().get_chunk_size(); }
    // If the chunk is unaligned, we must pre-read the corresponding
    // aligned area.
    // Buf is provided here as the _buf, while the previous _buf, _addr, _length
    // given from the caller is moved to _original_buf
    // A well-aligned chunk for read is created after preprocess_unaligned.
    void preprocess_unaligned(uint8_t *buf);

    // In the write path, if the original chunk is not aligned, after read
    // the corresponding aligned area, we need to merge the content with
    // the requested content, and continue the write process.
    void merge_write();

    // In the read path, if the original chunk is not aligned, after read
    // the corresponding aligned area, we need to copy the needed content
    // for the temporary buffer to the buffer of the caller.
    void merge_read();
};

struct Stats {
  // store number of each lookup results
  // write_dup_write, write_dup_content, write_not_dup,
  // read_hit, read_not_hit
  int _n_lookup_results[6];
  // store number of writes and reads
  // aligned write, aligned read,
  // write request, read request
  // Note: unaligned write requests would trigger read-modify-write
  int _n_requests[4];
  // store number of bytes of write and read
  // aligned write bytes, aligned read bytes
  // requested write bytes, requested read bytes
  int _total_bytes[4];
  // lba hit, ca hit
  int _index_hit[2];

  Stats() {
    memset(this, 0, sizeof(Stats));
  }

  inline void add_write_request() { _n_requests[0]++; }
  inline void add_write_bytes(uint32_t bytes) { _total_bytes[0] += bytes; }
  inline void add_read_request() { _n_requests[1]++; }
  inline void add_read_bytes(uint32_t bytes) { _total_bytes[1] += bytes; }
  inline void add_internal_write_request() { _n_requests[2]++; }
  inline void add_internal_write_bytes(uint32_t bytes) { _total_bytes[2] += bytes; }
  inline void add_internal_read_request() { _n_requests[3]++; }
  inline void add_internal_read_bytes(uint32_t bytes) { _total_bytes[3] += bytes; }

  inline void add_lookup_result(LookupResult lookup_result)
  {
    _n_lookup_results[lookup_result]++;
  }

  inline void add_lba_hit(bool lba_hit) {
    _index_hit[0] += lba_hit;
  }
  inline void add_ca_hit(bool ca_hit) {
    _index_hit[1] += ca_hit;
  }

};

}
#endif //__COMMON_H__
