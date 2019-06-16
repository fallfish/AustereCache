#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdint>
#include "common/config.h"

namespace cache {

// 512 bytes
struct Metadata {
  uint8_t _ca[128];
  uint64_t _lbas[37]; // 4 * 32
  uint32_t _num_lbas;
  uint8_t _[84];
};

enum LookupResult {
  WRITE_DUP_WRITE, WRITE_DUP_CONTENT, WRITE_NOT_DUP,
  READ_HIT, READ_NOT_HIT, LOOKUP_UNKNOWN
};

enum VerificationResult {
  BOTH_LBA_AND_CA_VALID, ONLY_CA_VALID, ONLY_LBA_VALID, BOTH_LBA_AND_CA_NOT_VALID, VERIFICATION_UNKNOWN
};

struct Chunk {
    uint64_t _addr;
    uint32_t _len;
    uint8_t *_buf;
    uint8_t *_compressed_buf;

    uint32_t _lba_hash;
    uint32_t _ca_hash;
    uint8_t  _ca[Config::ca_length];
    bool     _has_ca;

    uint32_t _compress_level; // compression level: 1, 2, 3, 4 * 8k
    uint64_t _ssd_location;
    Metadata _metadata;
    LookupResult _lookup_result;


    void fingerprinting();
    inline bool is_end() { return _len == 0; }
    bool is_partial();
    Chunk construct_read_chunk(uint8_t *buf);
    void merge_write(const Chunk &c);
    void merge_read(const Chunk &c);
    void compute_lba_hash();
};


}
#endif //__COMMON_H__
