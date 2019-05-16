#ifndef __INDEX_H__
#define __INDEX_H__
#include <iostream>
#include <cstring>
#include <vector>
#include "bitmap.h"
#include "bucket.h"
namespace cache {
  class Index {
    public:
      Index(uint32_t key_len, uint32_t value_len,
          uint32_t n_buckets, uint32_t n_elems_in_bucket);

      virtual bool lookup(uint8_t *key, uint8_t *value) = 0;
      virtual void set(uint8_t *key, uint8_t *value) = 0;

    protected:
      uint32_t _key_len, _value_len, _n_buckets;
      std::vector< std::unique_ptr<Bucket> > _mapping;
  };

  class LBAIndex : Index {
    public:
      LBAIndex(uint32_t key_len, uint32_t value_len,
          uint32_t n_buckets, uint32_t n_elems_in_bucket);
      bool lookup(uint8_t *key, uint8_t *value);
      void set(uint8_t *key, uint8_t *value);
    private:
      int lru_pos = 0;
  };

  class CAIndex : Index {
    public:
      CAIndex(uint32_t key_len, uint32_t value_len,
          uint32_t n_buckets, uint32_t n_elems_in_bucket);
      bool lookup(uint8_t *key, uint8_t *value);
      void set(uint8_t *key, uint8_t *value);
    private:

  };
}
#endif
