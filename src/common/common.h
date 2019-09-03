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
    uint32_t _fp_hash;
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
    std::unique_ptr<std::lock_guard<std::mutex>> _fp_bucket_lock;

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
      return _len == Config::get_configuration()->get_chunk_size()
        && (long long)_buf % 512 == 0;
#else
      return _len == Config::get_configuration()->get_chunk_size();
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

}
#endif //__COMMON_H__
