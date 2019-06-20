#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include "common/config.h"

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

    uint8_t  _ca[Config::ca_length];
    uint32_t _lba_hash;
    uint32_t _ca_hash;
    bool     _has_ca;

    uint64_t _ssd_location;

    bool _lba_hit;
    bool _ca_hit;
    LookupResult _lookup_result;
    VerificationResult _verification_result;

    std::unique_ptr<std::lock_guard<std::mutex>> _lba_bucket_lock;
    std::unique_ptr<std::lock_guard<std::mutex>> _ca_bucket_lock;

    void fingerprinting();
    inline bool is_end() { return _len == 0; }
    inline bool is_aligned() { return _len == Config::chunk_size; }
    void preprocess_unaligned(uint8_t *buf);
    void merge_write();
    void merge_read();
    void compute_lba_hash();
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
    memset(this, 0, sizeof(this));
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
