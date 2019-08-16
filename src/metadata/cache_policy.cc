#include "cache_policy.h"
#include "bucket.h"
#include "index.h"
#include "common/stats.h"
#include "manage/dirty_list.h"

#include <memory>
#include <csignal>



namespace cache {
  CachePolicyExecutor::CachePolicyExecutor(Bucket *bucket) :
    _bucket(bucket)
  {}

  LRUExecutor::LRUExecutor(Bucket *bucket) :
    CachePolicyExecutor(bucket)
  {}

  CAClockExecutor::CAClockExecutor(Bucket *bucket, std::shared_ptr<Bucket> clock, uint32_t *clock_ptr) :
    CachePolicyExecutor(bucket),
    _clock(std::move(clock)),
    _clock_ptr(clock_ptr)
  {}

  CAClockExecutor::~CAClockExecutor()
  {
  }

  void LRUExecutor::promote(uint32_t slot_id, uint32_t n_slots_to_occupy)
  {
    uint32_t n_slots = _bucket->get_n_slots();
    uint32_t k = _bucket->get_k(slot_id);
    uint32_t v = _bucket->get_v(slot_id);
    // Move each slot to the tail
    for ( ; slot_id < n_slots - n_slots_to_occupy; ++slot_id) {
      _bucket->set_k(slot_id, 
          _bucket->get_k(slot_id + n_slots_to_occupy));
      _bucket->set_v(slot_id, 
          _bucket->get_v(slot_id + n_slots_to_occupy));
      _bucket->set_invalid(slot_id);
      if (_bucket->is_valid(slot_id + n_slots_to_occupy)) {
        _bucket->set_valid(slot_id);
      }
    }
    // Store the promoted slots to the front
    for ( ; slot_id < n_slots; ++slot_id) {
      _bucket->set_k(slot_id, k);
      _bucket->set_v(slot_id, v);
      _bucket->set_valid(slot_id);
    }
  }

  // Only LBA Index would call this function
  // LBA signature only takes one slot.
  // So there is no need to care about the entry may take contiguous slots.
  void LRUExecutor::clear_obsoletes(std::shared_ptr<CAIndex> ca_index)
  {
    for (uint32_t slot_id = 0; slot_id < _bucket->get_n_slots(); ++slot_id) {
      if (!_bucket->is_valid(slot_id)) continue;

      uint32_t size; uint64_t ssd_location; // useless variables
      bool valid = false;
      uint32_t ca_hash = _bucket->get_v(slot_id);
      if (ca_index != nullptr)
        valid = ca_index->lookup(ca_hash, size, ssd_location);
      // if the slot has no mappings in ca index, it is an empty slot
      if (!valid) {
        _bucket->set_k(slot_id, 0), _bucket->set_v(slot_id, 0);
        _bucket->set_invalid(slot_id);
      }
    }
  }

  uint32_t LRUExecutor::allocate(uint32_t n_slots_to_occupy)
  {
    uint32_t slot_id = 0, n_slots_available = 0,
             n_slots = _bucket->get_n_slots();
    for ( ; slot_id < n_slots; ++slot_id) {
      if (n_slots_available == n_slots_to_occupy)
        break;
      // find an empty slot
      if (!_bucket->is_valid(slot_id)) {
        ++n_slots_available;
      } else {
        n_slots_available = 0;
      }
    }

    if (n_slots_available < n_slots_to_occupy) {
      slot_id = 0;
      // Evict Least Recently Used slots
      for ( ; slot_id < n_slots; ) {
        if (slot_id >= n_slots_to_occupy) break;
        if (!_bucket->is_valid(slot_id)) { ++slot_id; continue; }

        uint32_t k = _bucket->get_k(slot_id);
        while (slot_id < n_slots
            && _bucket->get_k(slot_id) == k) {
          Stats::get_instance()->add_lba_index_eviction_caused_by_capacity();
          _bucket->set_invalid(slot_id);
          _bucket->set_k(slot_id, 0);
          _bucket->set_v(slot_id, 0);
          slot_id++;
        }
      }
    }

    return slot_id - n_slots_to_occupy;
  }

  void CAClockExecutor::promote(uint32_t slot_id, uint32_t n_slots_occupied)
  {

    for (uint32_t slot_id_ = slot_id;
        slot_id_ < slot_id + n_slots_occupied;
        ++slot_id_) {
      inc_clock(slot_id_);
    }
  }

  void CAClockExecutor::clear_obsoletes(std::shared_ptr<CAIndex> ca_index)
  {
  }

  uint32_t CAClockExecutor::allocate(uint32_t n_slots_to_occupy)
  {
    uint32_t slot_id = 0, n_slots_available = 0,
             n_slots = _bucket->get_n_slots();
    // check whether there is a contiguous space
    for ( ; slot_id < n_slots; ++slot_id) {
      if (n_slots_available == n_slots_to_occupy)
        break;
      // find an empty slot
      if (!_bucket->is_valid(slot_id)) {
        ++n_slots_available;
      } else {
        n_slots_available = 0;
      }
    }

    if (n_slots_available < n_slots_to_occupy) {
      // No contiguous space, need to evict previous slot
      slot_id = *_clock_ptr;
      // adjust the pointer to the head of an object (chunk) rather than in the middle
      // E.g., a previous compressibility-3 chunk was evicted, and a compressibility-4 chunk
      //       was inserted in-place, which leaves pointer the middle of compressibility-4 chunk
      //       and cause a false clock deference.
      if (slot_id > 0 && _bucket->is_valid(slot_id)
          && _bucket->get_k(slot_id - 1) == _bucket->get_k(slot_id)) {
        uint32_t k = _bucket->get_k(slot_id);
        while (slot_id < n_slots 
            && k == _bucket->get_k(slot_id)) {
          ++slot_id;
        }
      }
      n_slots_available = 0;

      while (1) {
        if (n_slots_available >= n_slots_to_occupy) break;
        if (slot_id == n_slots) {
          n_slots_available = 0;
          slot_id = 0;
        }
        if (!_bucket->is_valid(slot_id)) {
          ++n_slots_available;
          ++slot_id;
          continue;
        }
        // to allocate
        uint32_t k = _bucket->get_k(slot_id);
        uint32_t c = get_clock(slot_id);
        bool evicted = false;
        uint32_t slot_id_begin = slot_id;
        while (slot_id < n_slots
            && k == _bucket->get_k(slot_id)) {
          if (get_clock(slot_id) == 0) {
            evicted = true;
            _bucket->set_invalid(slot_id);
            ++n_slots_available;
          } else {
            dec_clock(slot_id);
            n_slots_available = 0;
          }
          ++slot_id;
        }
        if (evicted) {
#ifdef WRITE_BACK_CACHE
          DirtyList::get_instance()->add_evicted_block(
              /* Compute ssd location of the evicted data */
              /* Actually, full CA and address is sufficient. */
                0,  
                (_bucket->get_bucket_id() * _bucket->get_n_slots() + slot_id_begin) * 1LL *
                (Config::get_configuration().get_sector_size() + 
                 Config::get_configuration().get_metadata_size()),
                slot_id - slot_id_begin
              );
#endif
          Stats::get_instance()->add_ca_index_eviction_caused_by_capacity();
        }
      }
      *_clock_ptr = slot_id;
    }

    for (uint32_t slot_id_ = slot_id - n_slots_to_occupy;
        slot_id_ < slot_id; ++slot_id_) {
      init_clock(slot_id_);
    }

    return slot_id - n_slots_to_occupy;
  }

  inline void CAClockExecutor::init_clock(uint32_t index) {
    _clock->set_v(index, 1);
  }
  inline uint32_t CAClockExecutor::get_clock(uint32_t index) {
    return _clock->get_v(index);
  }
  inline void CAClockExecutor::inc_clock(uint32_t index) {
    uint32_t v = _clock->get_v(index);
    if (v != 3) {
      _clock->set_v(index, v + 1);
    }
  }
  inline void CAClockExecutor::dec_clock(uint32_t index) { 
    uint32_t v = _clock->get_v(index);
    if (v != 0) {
      _clock->set_v(index, v - 1);
    }
  }

  CachePolicy::CachePolicy() {}

  LRU::LRU() {}

  CAClock::CAClock(uint32_t n_slots_per_bucket, uint32_t n_buckets) : CachePolicy() {
    _n_slots_per_bucket = n_slots_per_bucket;
    _n_bytes_per_bucket = (2 * n_slots_per_bucket + 7) / 8;
    _clock = std::make_unique< uint8_t[] >(_n_bytes_per_bucket * n_buckets);
    _clock_ptr = 0;
  }

  std::shared_ptr<CachePolicyExecutor> LRU::get_executor(Bucket *bucket)
  { 
    return std::make_shared<LRUExecutor>(bucket);
  }

  // TODO: Check whether there is memory leak when destructing
  std::shared_ptr<CachePolicyExecutor> CAClock::get_executor(Bucket *bucket)
  { 
    return std::make_shared<CAClockExecutor>(
          bucket, std::move(get_bucket(bucket->get_bucket_id())),
          &_clock_ptr);
  }
}
