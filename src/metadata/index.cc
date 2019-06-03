#include "index.h"
namespace cache {

  Index::Index(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_elems_in_bucket) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_buckets(n_buckets)
  {};

  LBAIndex::LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_elems_in_bucket) :
    Index(n_bits_per_key, n_bits_per_value, n_buckets, n_elems_in_bucket)
  {
    _mapping.resize(n_buckets);
    for (int i = 0; i < n_buckets; i++) {
      std::unique_ptr<Bucket> bucket(
        std::move(std::make_unique<LBABucket>(
          n_bits_per_key, n_bits_per_value, n_elems_in_bucket))
        );
      _mapping[i] = std::move(bucket);
    }
  }

  bool LBAIndex::lookup(uint8_t *key, uint8_t *value)
  {
    uint32_t hash = *(uint32_t*)key;
    uint32_t bucket_no = hash >> _n_bits_per_key;
    uint32_t signature = hash & ((1 << _n_bits_per_key) - 1);
    return _mapping[bucket_no]->find((uint8_t*)&signature, value) != -1;
  }

  void LBAIndex::set(uint8_t *key, uint8_t *value)
  {
    uint32_t hash = *(uint32_t*)key;
    uint32_t bucket_no = hash >> _n_bits_per_key;
    uint32_t signature = hash & ((1 << _n_bits_per_key) - 1);
    _mapping[bucket_no]->insert((uint8_t*)&signature, value);
  }

  CAIndex::CAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_elems_in_bucket) :
    Index(n_bits_per_key, n_bits_per_value, n_buckets, n_elems_in_bucket)
  {
    _mapping.resize(n_buckets);
    for (int i = 0; i < n_buckets; i++) {
      std::unique_ptr<Bucket> bucket(
        std::move(std::make_unique<LBABucket>(
          n_bits_per_key, n_bits_per_value, n_elems_in_bucket))
        );
      _mapping[i] = std::move(bucket);
    }
  }

  bool CAIndex::lookup(uint8_t *key, uint8_t *value)
  {
    uint32_t hash = *(uint32_t*)key;
    uint32_t bucket_no = hash >> _n_bits_per_key;
    uint32_t signature = hash & ((1 << _n_bits_per_key) - 1);
    return _mapping[bucket_no]->find((uint8_t*)&signature, value) != -1;
  }

  void CAIndex::set(uint8_t *key, uint8_t *value)
  {
    uint32_t hash = *(uint32_t*)key;
    uint32_t bucket_no = hash >> _n_bits_per_key;
    uint32_t signature = hash & ((1 << _n_bits_per_key) - 1);
    _mapping[bucket_no]->insert((uint8_t*)&signature, value);
  }
}
