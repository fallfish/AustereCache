#ifndef __INDEX_H__
#define __INDEX_H__
#include <iostream>
#include <cstring>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <list>
#include "bucket.h"
#include "cache_policy.h"
#include "common/config.h"
#include "cachededup_index.h"
namespace cache {
  class Index {
    public:
      Index(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_slots_per_bucket);

      //virtual bool lookup(uint8_t *key, uint8_t *value) = 0;
      //virtual void set(uint8_t *key, uint8_t *value) = 0;

      void set_cache_policy(std::unique_ptr<CachePolicy> cache_policy);
      std::mutex& get_mutex(uint32_t bucket_id) { return _mutexes[bucket_id]; }
    protected:
      uint32_t _n_bits_per_slot, _n_slots_per_bucket, _n_total_bytes,
               _n_bits_per_key, _n_bits_per_value,
               _n_data_bytes_per_bucket, n_buckets,
               _n_valid_bytes_per_bucket;
      std::unique_ptr< uint8_t[] > _data;
      std::unique_ptr< uint8_t[] > _valid;
      std::unique_ptr< CachePolicy > _cache_policy;
      std::unique_ptr< std::mutex[] > _mutexes;
  };

  class CAIndex;
  class LBAIndex : Index {
    /*
     *
     *
     */
    public:
      LBAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_slots_per_bucket, std::shared_ptr<CAIndex> ca_index);
      bool lookup(uint32_t lba_hash, uint32_t &ca_hash);
      void promote(uint32_t lba_hash);
      void update(uint32_t lba_hash, uint32_t ca_hash);
      std::unique_ptr<std::lock_guard<std::mutex>> lock(uint32_t lba_hash);
      void unlock(std::unique_ptr<std::lock_guard<std::mutex>>);

      std::unique_ptr<LBABucket> get_lba_bucket(uint32_t bucket_id)
      {
        return std::move(std::make_unique<LBABucket>(
            _n_bits_per_key, _n_bits_per_value, _n_slots_per_bucket,
            _data.get() + _n_data_bytes_per_bucket * bucket_id, 
            _valid.get() + _n_valid_bytes_per_bucket * bucket_id,
            _cache_policy.get(), bucket_id));
      }
    private:
      std::shared_ptr<CAIndex> _ca_index;
  };

  class CAIndex : Index {
    public:
      // n_bits_per_key = 12, n_bits_per_value = 4
      CAIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_slots_per_bucket);
      bool lookup(uint32_t ca_hash, uint32_t &size, uint64_t &ssd_location);
      void promote(uint32_t ca_hash);
      void update(uint32_t ca_hash, uint32_t size, uint64_t &ssd_location);
      std::unique_ptr<std::lock_guard<std::mutex>> lock(uint32_t ca_hash);
      void unlock(std::unique_ptr<std::lock_guard<std::mutex>>);

      std::unique_ptr<CABucket> get_ca_bucket(uint32_t bucket_id) {
        return std::move(std::make_unique<CABucket>(
            _n_bits_per_key, _n_bits_per_value, _n_slots_per_bucket,
            _data.get() + _n_data_bytes_per_bucket * bucket_id, 
            _valid.get() + _n_valid_bytes_per_bucket * bucket_id,
            _cache_policy.get(), bucket_id));
      }
    private:
      uint32_t compute_ssd_location(uint32_t bucket_no, uint32_t index);
  };

}
#endif
