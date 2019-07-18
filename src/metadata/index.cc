#include "index.h"
#include "cache_policy.h"
#include "common/config.h"
namespace cache {

  Index::Index(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_slots_per_bucket) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_buckets(n_buckets), _n_slots_per_bucket(n_slots_per_bucket)
  {}

  LBAIndex::LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_slots_per_bucket,
      std::shared_ptr<CAIndex> ca_index) :
    Index(n_bits_per_key, n_bits_per_value, n_buckets, n_slots_per_bucket),
    _ca_index(ca_index)
  {
    _buckets = std::make_unique<LBABuckets>(n_bits_per_key, n_bits_per_value, n_slots_per_bucket, n_buckets);
    _buckets->set_cache_policy(std::move(std::make_unique<LRU>()));
  }

  bool LBAIndex::lookup(uint32_t lba_hash, uint32_t &ca_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    return _buckets->get_lba_bucket(bucket_id)->lookup(signature, ca_hash) != ~((uint32_t)0);
  }

  void LBAIndex::promote(uint32_t lba_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    _buckets->get_lba_bucket(bucket_id)->promote(signature);
  }

  void LBAIndex::update(uint32_t lba_hash, uint32_t ca_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    _buckets->get_lba_bucket(bucket_id)->update(signature, ca_hash, _ca_index);
  }

  CAIndex::CAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_buckets, uint32_t n_slots_per_bucket) :
    Index(n_bits_per_key, n_bits_per_value, n_buckets, n_slots_per_bucket)
  {
    _buckets = std::move(
          std::make_unique<CABuckets>(
            n_bits_per_key, n_bits_per_value, n_slots_per_bucket, n_buckets)
          );
    _buckets->set_cache_policy(std::move(std::make_unique<CAClock>(n_slots_per_bucket, n_buckets)));
  }

  uint32_t CAIndex::compute_ssd_location(uint32_t bucket_id, uint32_t slot_id)
  {
    // 8192 is chunk size, while 512 is the metadata size
    return (bucket_id * _n_slots_per_bucket + slot_id) *
      (Config::get_configuration().get_sector_size() + 
       Config::get_configuration().get_metadata_size());
  }

  bool CAIndex::lookup(uint32_t ca_hash, uint32_t &compressibility_level, uint64_t &ssd_location)
  {
    uint32_t bucket_id = ca_hash >> _n_bits_per_key,
             signature = ca_hash & ((1 << _n_bits_per_key) - 1),
             n_slots_occupied = 0;
    uint32_t index = _buckets->get_ca_bucket(bucket_id)->lookup(signature, n_slots_occupied);
    if (index == ~((uint32_t)0)) return false;

    compressibility_level = n_slots_occupied - 1;
    ssd_location = compute_ssd_location(bucket_id, index);

    return true;
  }

  void CAIndex::promote(uint32_t ca_hash)
  {
    uint32_t bucket_id = ca_hash >> _n_bits_per_key,
             signature = ca_hash & ((1 << _n_bits_per_key) - 1);
    _buckets->get_ca_bucket(bucket_id)->promote(signature);
  }

  void CAIndex::update(uint32_t ca_hash, uint32_t compressibility_level, uint64_t &ssd_location)
  {
    uint32_t bucket_id = ca_hash >> _n_bits_per_key,
             signature = ca_hash & ((1 << _n_bits_per_key) - 1),
             n_slots_to_occupy = compressibility_level + 1;

    uint32_t slot_id = _buckets->get_ca_bucket(bucket_id)->update(signature, n_slots_to_occupy);
    ssd_location = compute_ssd_location(bucket_id, slot_id);
  }

  std::unique_ptr<std::lock_guard<std::mutex>> LBAIndex::lock(uint32_t lba_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          _buckets->get_mutex(bucket_id)));
  }

  void LBAIndex::unlock(std::unique_ptr<std::lock_guard<std::mutex>>)
  {
  }

  std::unique_ptr<std::lock_guard<std::mutex>> CAIndex::lock(uint32_t ca_hash)
  {
    uint32_t bucket_id = ca_hash >> _n_bits_per_key;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          _buckets->get_mutex(bucket_id)));
  }

  void CAIndex::unlock(std::unique_ptr<std::lock_guard<std::mutex>>)
  {
  }
}
