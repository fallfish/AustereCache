#include "index.h"
namespace cache {

  Index::Index(uint32_t key_len, uint32_t value_len,
      uint32_t n_buckets, uint32_t n_elems_in_bucket) :
    _key_len(key_len), _value_len(value_len),
    _n_buckets(n_buckets)
  {};

  LBAIndex::LBAIndex(uint32_t key_len, uint32_t value_len,
      uint32_t n_buckets, uint32_t n_elems_in_bucket) :
    Index(key_len, value_len, n_buckets, n_elems_in_bucket)
  {
    _mapping.resize(n_buckets);
    for (int i = 0; i < n_buckets; i++) {
      std::unique_ptr<Bucket> bucket(
        std::move(std::make_unique<LBABucket>(
          key_len + value_len, n_elems_in_bucket))
        );
      _mapping[i] = std::move(bucket);
    }
  }

  bool LBAIndex::lookup(uint8_t *key, uint8_t *value)
  {
    uint32_t hash = *(uint32_t*)key;
    uint32_t bucket_no = hash >> (32 - _n_buckets);
    uint32_t signature = hash & ((1 << 12) - 1);
    return _mapping[bucket_no]->find(
        (uint8_t*)&signature, _key_len, value) != -1;
  }

  void LBAIndex::set(uint8_t *key, uint8_t *value)
  {
    uint32_t hash = *(uint32_t*)key;
    uint32_t bucket_no = hash >> (32 - _n_buckets);
    uint32_t signature = hash & ((1 << 12) - 1);
    _mapping[bucket_no]->insert(
        (uint8_t*)&signature, _key_len, value);
  }

  CAIndex::CAIndex(uint32_t key_len, uint32_t value_len,
      uint32_t n_buckets, uint32_t n_elems_in_bucket) :
    Index(key_len, value_len, n_buckets, n_elems_in_bucket)
  {
    _mapping.resize(n_buckets);
    for (int i = 0; i < n_buckets; i++) {
      std::unique_ptr<Bucket> bucket(
        std::move(std::make_unique<LBABucket>(
          key_len + value_len, n_elems_in_bucket))
        );
      _mapping[i] = std::move(bucket);
    }
  }

  bool CAIndex::lookup(uint8_t *key, uint8_t *value)
  {
    uint32_t bucket_no = *(uint16_t*)(key + 2) & (_n_buckets - 1);
    return _mapping[bucket_no]->find(
        key, _key_len, value) != -1;
  }

  void CAIndex::set(uint8_t *key, uint8_t *value)
  {
    uint32_t bucket_no = *(uint16_t*)(key + 2) & (_n_buckets - 1);
    //std::cout << "bucket_no: " << bucket_no << std::endl;
    _mapping[bucket_no]->insert(
        key, _key_len, value);
  }
}
