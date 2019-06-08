#ifndef __INDEX_H__
#define __INDEX_H__
#include <iostream>
#include <cstring>
#include <vector>
#include <memory>
#include "bucket.h"
namespace cache {
  class Index {
    public:
      Index(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_items_per_bucket);

      //virtual bool lookup(uint8_t *key, uint8_t *value) = 0;
      //virtual void set(uint8_t *key, uint8_t *value) = 0;
    protected:
      uint32_t _n_bits_per_key, _n_bits_per_value;
      uint32_t _n_buckets, _n_items_per_bucket;
  };

  class CAIndex;
  class LBAIndex : Index {
    public:
      LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_items_per_bucket, std::shared_ptr<CAIndex> ca_index);
      bool lookup(uint32_t lba_hash, uint32_t &ca_hash);
      void update(uint32_t lba_hash, uint32_t ca_hash);
    private:
      std::shared_ptr<CAIndex> _ca_index;
      std::vector< std::unique_ptr<LBABucket> > _buckets;
  };

  class CAIndex : Index {
    /*
     * layout:
     * key: 12 bit ca hash value prefix
     * value: 4 bit
     *   2 bit compression_code (0, 1, 2, 3 representing space occupation)
     *   (metadata and compression flag could take several bits of space)
     *   2 bit representing the clock bits
     *   (with 0 as invalid state, 1 - 3 as valid)
     * --------------------------------------------------------
     * | 12 bit signature | 2 bit space code | 2 bit validity |
     * --------------------------------------------------------
     */
    public:
      // n_bits_per_key = 12, n_bits_per_value = 4
      CAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_items_per_bucket);
      bool lookup(uint32_t ca_hash, uint32_t &size, uint32_t &ssd_location);
      void update(uint32_t ca_hash, uint32_t size, uint32_t &ssd_location);
    private:
      uint32_t compute_ssd_location(uint32_t bucket_no, uint32_t index);
      std::vector< std::unique_ptr<CABucket> > _buckets;
  };
}
#endif
