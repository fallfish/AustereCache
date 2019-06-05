#ifndef __INDEX_H__
#define __INDEX_H__
#include <iostream>
#include <cstring>
#include <vector>
#include <memory>
#include "bitmap.h"
#include "bucket.h"
namespace cache {
  class Index {
    public:
      Index(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_elems_in_bucket);

      //virtual bool lookup(uint8_t *key, uint8_t *value) = 0;
      //virtual void set(uint8_t *key, uint8_t *value) = 0;

    protected:
      uint32_t _n_bits_per_key, _n_bits_per_value, _n_buckets;
      std::vector< std::unique_ptr<Bucket> > _mapping;
  };

  class LBAIndex : Index {
    public:
      LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_elems_in_bucket);
      bool lookup(uint32_t lba_hash, uint32_t &ca_hash);
      void update(uint32_t lba_hash, uint32_t &ca_hash);
      //void set(uint8_t *key, uint8_t *value);
    private:
      std::shared_ptr<CAIndex> _ca_index;
  };

  class CAIndex : Index {
    public:
      CAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_elems_in_bucket);
      //bool lookup(uint8_t *key, uint8_t *value);
      //void set(uint8_t *key, uint8_t *value);
      bool lookup(uint32_t ca_hash, uint32_t &size, uint32_t &ssd_location);
      void update(uint32_t ca_hash, uint32_t size, uint32_t &ssd_location);
      uint32_t compute_ssd_loaction();
    private:

  };
}
#endif
