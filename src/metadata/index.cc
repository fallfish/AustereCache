#include "index.h"
#include "common/config.h"
namespace cache {

  Index::Index(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_items_per_bucket) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_buckets(n_buckets), _n_items_per_bucket(n_items_per_bucket)
  {};

  LBAIndex::LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_items_per_bucket,
      std::shared_ptr<CAIndex> ca_index) :
    Index(n_bits_per_key, n_bits_per_value, n_buckets, n_items_per_bucket),
    _ca_index(ca_index)
  {
    _buckets.resize(n_buckets);
    for (int i = 0; i < n_buckets; i++) {
      _buckets[i] = std::move(
          std::make_shared<LBABucket>(
            n_bits_per_key, n_bits_per_value, n_items_per_bucket)
          );
      _buckets[i]->set_cache_policy(
          std::move(std::make_unique<LRU>(_buckets[i])));
    }
  }

  bool LBAIndex::lookup(uint32_t lba_hash, uint32_t &ca_hash)
  {
    uint32_t bucket_no = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    return _buckets[bucket_no]->lookup(signature, ca_hash) != ~((uint32_t)0);
  }

  void LBAIndex::promote(uint32_t lba_hash)
  {
    uint32_t bucket_no = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    _buckets[bucket_no]->promote(signature);
  }

  void LBAIndex::update(uint32_t lba_hash, uint32_t ca_hash)
  {
    uint32_t bucket_no = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    _buckets[bucket_no]->update(signature, ca_hash, _ca_index);
  }

  CAIndex::CAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_items_per_bucket) :
    Index(n_bits_per_key, n_bits_per_value, n_buckets, n_items_per_bucket)
  {
    _buckets.resize(n_buckets);
    for (int i = 0; i < n_buckets; i++) {
      _buckets[i] = std::move(
          std::make_unique<CABucket>(
            n_bits_per_key, n_bits_per_value, n_items_per_bucket)
          );
      _buckets[i]->set_cache_policy(
          std::move(std::make_unique<CAClock>(_buckets[i])));
    }
  }

  uint32_t CAIndex::compute_ssd_location(uint32_t bucket_no, uint32_t slot_id)
  {
    // 8192 is chunk size, while 512 is the metadata size
    return (bucket_no * _n_items_per_bucket + slot_id) *
      (Config::get_configuration().get_sector_size() + 
       Config::get_configuration().get_metadata_size());
  }

  bool CAIndex::lookup(uint32_t ca_hash, uint32_t &compressibility_level, uint64_t &ssd_location)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key,
             signature = ca_hash & ((1 << _n_bits_per_key) - 1),
             n_slots_occupied = 0;
    uint32_t index = _buckets[bucket_no]->lookup(signature, n_slots_occupied);
    if (index == ~((uint32_t)0)) return false;

    compressibility_level = n_slots_occupied - 1;
    ssd_location = compute_ssd_location(bucket_no, index);

    return true;
  }

  void CAIndex::promote(uint32_t ca_hash)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key,
             signature = ca_hash & ((1 << _n_bits_per_key) - 1);
    _buckets[bucket_no]->promote(signature);
  }

  void CAIndex::update(uint32_t ca_hash, uint32_t compressibility_level, uint64_t &ssd_location)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key,
             signature = ca_hash & ((1 << _n_bits_per_key) - 1),
             n_slots_to_occupy = compressibility_level + 1;

    uint32_t slot_id = _buckets[bucket_no]->update(signature, n_slots_to_occupy);
    ssd_location = compute_ssd_location(bucket_no, slot_id);
  }

  std::unique_ptr<std::lock_guard<std::mutex>> LBAIndex::lock(uint32_t lba_hash)
  {
    uint32_t bucket_no = lba_hash >> _n_bits_per_key;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          _buckets[bucket_no]->get_mutex()));
  }

  void LBAIndex::unlock(std::unique_ptr<std::lock_guard<std::mutex>>)
  {
  }

  std::unique_ptr<std::lock_guard<std::mutex>> CAIndex::lock(uint32_t ca_hash)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          _buckets[bucket_no]->get_mutex()));
  }

  void CAIndex::unlock(std::unique_ptr<std::lock_guard<std::mutex>>)
  {
  }
}
