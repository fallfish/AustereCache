#include "index.h"
#include "cache_policy.h"
#include "common/config.h"
#include "common/stats.h"
namespace cache {

  Index::Index(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_slots_per_bucket, uint32_t n_buckets) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_slot(n_bits_per_key + n_bits_per_value), _n_slots_per_bucket(n_slots_per_bucket),
    _n_data_bytes_per_bucket(((n_bits_per_key + n_bits_per_value) * n_slots_per_bucket + 7) / 8),
    _n_valid_bytes_per_bucket((1 * n_slots_per_bucket + 7) / 8),
    _data(std::make_unique<uint8_t[]>(_n_data_bytes_per_bucket * n_buckets)),
    _valid(std::make_unique<uint8_t[]>(_n_valid_bytes_per_bucket * n_buckets)),
    _mutexes(std::make_unique<std::mutex[]>(n_buckets)),
    _n_buckets(n_buckets)
  {}

  LBAIndex::~LBAIndex() {
    std::cout << "~LBAIndex: " << std::endl;
    std::map<uint32_t, uint32_t> counters;
    for (int i = 0; i < _n_buckets; ++i) {
      uint32_t count = 0;
      for (int j = 0; j < 32; ++j) {
        if (get_lba_bucket(i)->is_valid(j)) {
          count += 1;
        }
      }
      if (counters.find(count) == counters.end()) {
        counters[count] = 0;
      }
      counters[count] += 1;
    }

    int cnt = 0;
    for (auto p : counters) {
      cnt++;
      printf("(%d,%d) ", p.first, p.second);
      if (cnt == 8) printf("\n"), cnt=0;
//      std::cout << p.first << " " << p.second << std::endl;
    }
    printf("\n");
  }
  FPIndex::~FPIndex() {
    std::cout << "~FPIndex: " << std::endl;
    std::map<uint32_t, uint32_t> counters;
    for (int i = 0; i < _n_buckets; ++i) {
      uint32_t count = 0;
      for (int j = 0; j < 32; ++j) {
        if (get_fp_bucket(i)->is_valid(j)) {
          count += 1;
        }
      }
      if (counters.find(count) == counters.end()) {
        counters[count] = 0;
      }
      counters[count] += 1;
    }
    /*
    for (auto p : counters) {
      std::cout << (p.first / 4) << " " << p.second << std::endl;
    }
    */
    int cnt = 0;
    for (auto p : counters) {
      cnt++;
      printf("(%d,%d) ", p.first, p.second);
      if (cnt == 8) printf("\n"), cnt=0;
//      std::cout << p.first << " " << p.second << std::endl;
    }
    printf("\n");
  }
  void Index::set_cache_policy(std::unique_ptr<CachePolicy> cache_policy)
  { 
    _cache_policy = std::move(cache_policy);
  }

  LBAIndex::LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_slots_per_bucket, uint32_t n_buckets,
      std::shared_ptr<FPIndex> ca_index) :
    Index(n_bits_per_key, n_bits_per_value, n_slots_per_bucket, n_buckets),
    _ca_index(ca_index)
  {
    set_cache_policy(std::move(std::make_unique<LRU>()));
  }

  bool LBAIndex::lookup(uint64_t lba_hash, uint64_t &fp_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    Stats::get_instance()->add_lba_index_bucket_hit(bucket_id);
    return get_lba_bucket(bucket_id)->lookup(signature, fp_hash) != ~((uint32_t)0);
  }

  void LBAIndex::promote(uint64_t lba_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    get_lba_bucket(bucket_id)->promote(signature);
  }

  void LBAIndex::update(uint64_t lba_hash, uint64_t fp_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    get_lba_bucket(bucket_id)->update(signature, fp_hash, _ca_index);

    Stats::get_instance()->add_lba_bucket_update(bucket_id);
  }

  FPIndex::FPIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
      uint32_t n_slots_per_bucket, uint32_t n_buckets) :
    Index(n_bits_per_key, n_bits_per_value, n_slots_per_bucket, n_buckets)
  {
    _cache_policy = std::move(std::make_unique<CAClock>(n_slots_per_bucket, n_buckets));
  }

  uint64_t FPIndex::compute_ssd_location(uint32_t bucket_id, uint32_t slot_id)
  {
    // 8192 is chunk size, while 512 is the metadata size
    return (bucket_id * _n_slots_per_bucket + slot_id) * 1LL *
      Config::get_configuration()->get_sector_size() + (uint64_t)_n_buckets *
      _n_slots_per_bucket * Config::get_configuration()->get_metadata_size();
  }

  uint64_t FPIndex::compute_metadata_location(uint32_t bucket_id, uint32_t slot_id)
  {
    return (bucket_id * _n_slots_per_bucket + slot_id) * 1LL * Config::get_configuration()->get_metadata_size();
  }

  bool FPIndex::lookup(uint64_t fp_hash, uint32_t &compressibility_level, uint64_t &ssd_location, uint64_t &metadata_location)
  {
    uint32_t bucket_id = fp_hash >> _n_bits_per_key,
             signature = fp_hash & ((1 << _n_bits_per_key) - 1),
             n_slots_occupied = 0;
    uint32_t index = get_fp_bucket(bucket_id)->lookup(signature, n_slots_occupied);
    if (index == ~((uint32_t)0)) return false;

    compressibility_level = n_slots_occupied - 1;
    ssd_location = compute_ssd_location(bucket_id, index);
    metadata_location = compute_metadata_location(bucket_id, index);

    Stats::get_instance()->add_fp_bucket_lookup(bucket_id);
    return true;
  }

  void FPIndex::promote(uint64_t fp_hash)
  {
    uint32_t bucket_id = fp_hash >> _n_bits_per_key,
             signature = fp_hash & ((1 << _n_bits_per_key) - 1);
    get_fp_bucket(bucket_id)->promote(signature);
  }

  void FPIndex::update(uint64_t fp_hash, uint32_t compressibility_level, uint64_t &ssd_location, uint64_t &metadata_location)
  {
    uint32_t bucket_id = fp_hash >> _n_bits_per_key,
             signature = fp_hash & ((1 << _n_bits_per_key) - 1),
             n_slots_to_occupy = compressibility_level + 1;

    Stats::get_instance()->add_fp_bucket_update(bucket_id);
    uint32_t slot_id = get_fp_bucket(bucket_id)->update(signature, n_slots_to_occupy);
    ssd_location = compute_ssd_location(bucket_id, slot_id);
    metadata_location = compute_metadata_location(bucket_id, slot_id);
  }

  std::unique_ptr<std::lock_guard<std::mutex>> LBAIndex::lock(uint64_t lba_hash)
  {
    uint32_t bucket_id = lba_hash >> _n_bits_per_key;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          _mutexes[bucket_id]));
  }

  void LBAIndex::unlock(std::unique_ptr<std::lock_guard<std::mutex>>)
  {
  }

  std::unique_ptr<std::lock_guard<std::mutex>> FPIndex::lock(uint64_t fp_hash)
  {
    uint32_t bucket_id = fp_hash >> _n_bits_per_key;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          _mutexes[bucket_id]));
  }

  void FPIndex::unlock(std::unique_ptr<std::lock_guard<std::mutex>>)
  {
  }
}
