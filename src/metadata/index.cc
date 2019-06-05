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

  bool CAIndex::lookup(uint32_t ca_hash, uint32_t &size, uint32_t &ssd_location)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key;
    uint32_t signature = ca_hash & ((1 << _n_bits_per_key) - 1);
    uint32_t index = _mapping[bucket_no]->find((uint8_t*)&signature, size);
    offset = 0;
    for (uint32_t i = 0; i < index; i++) {
      offset += _mapping[bucket_no]->get(i);
    }
    ssd_location = bucket_no * 1024 * 1024 + offset * 32 * 1024;
  }

  void CAIndex::update(uint32_t ca_hash, uint32_t size, uint32_t &ssd_location)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key;
    uint32_t signature = ca_hash & ((1 << _n_bits_per_key) - 1);
    uint32_t value = (size << 3) & (0 << 1);
    _mapping[bucket_no]->insert((uint8_t*)&signature, );
  }
}
