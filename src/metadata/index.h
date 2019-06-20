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
    /*
     *
     *
     */
    public:
      LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_items_per_bucket, std::shared_ptr<CAIndex> ca_index);
      bool lookup(uint32_t lba_hash, uint32_t &ca_hash);
      void update(uint32_t lba_hash, uint32_t ca_hash);
      std::unique_ptr<std::lock_guard<std::mutex>> lock(uint32_t lba_hash);
      void unlock(std::unique_ptr<std::lock_guard<std::mutex>>);
    private:
      std::shared_ptr<CAIndex> _ca_index;
      std::vector< std::unique_ptr<LBABucket> > _buckets;
  };

  class CAIndex : Index {
    public:
      // n_bits_per_key = 12, n_bits_per_value = 4
      CAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_items_per_bucket);
      bool lookup(uint32_t ca_hash, uint32_t &size, uint64_t &ssd_location);
      void update(uint32_t ca_hash, uint32_t size, uint64_t &ssd_location);
      void erase(uint32_t ca_hash);
      std::unique_ptr<std::lock_guard<std::mutex>> lock(uint32_t ca_hash);
      void unlock(std::unique_ptr<std::lock_guard<std::mutex>>);

    private:
      uint32_t compute_ssd_location(uint32_t bucket_no, uint32_t index);
      std::vector< std::unique_ptr<CABucket> > _buckets;
  };
}
#endif
