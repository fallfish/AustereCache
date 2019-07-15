#include "cache_policy.h"
#include "index.h"

namespace cache {

  CachePolicy::CachePolicy(std::shared_ptr<Bucket> bucket):
    _bucket(bucket)
  {
  }

  LRU::LRU(std::shared_ptr<Bucket> bucket) : CachePolicy(bucket) {}

  void LRU::promote(uint32_t slot_id, uint32_t n_slots_to_occupy)
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
  void LRU::clear_obsoletes(std::shared_ptr<CAIndex> ca_index)
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

  uint32_t LRU::allocate(uint32_t n_slots_to_occupy)
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
          _bucket->set_invalid(slot_id);
          _bucket->set_k(slot_id, 0);
          _bucket->set_v(slot_id, 0);
          slot_id++;
        }
      }
    }

    return slot_id - n_slots_to_occupy;
  }

  CAClock::CAClock(std::shared_ptr<Bucket> bucket) : CachePolicy(bucket) {
    _clock = std::make_unique< Bucket >(0, 2, bucket->get_n_slots());
    _clock_ptr = 0;

  }
  void CAClock::promote(uint32_t slot_id, uint32_t n_slots_occupied)
  {
    for (uint32_t slot_id_ = slot_id;
        slot_id_ < slot_id + n_slots_occupied;
        ++slot_id_) {
      inc_clock(slot_id_);
    }
  }

  void CAClock::clear_obsoletes(std::shared_ptr<CAIndex> ca_index)
  {
  }

  uint32_t CAClock::allocate(uint32_t n_slots_to_occupy)
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
      slot_id = _clock_ptr;
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
        while (slot_id < n_slots
            && k == _bucket->get_k(slot_id)) {
          if (get_clock(slot_id) == 0) {
            _bucket->set_invalid(slot_id);
          } else {
            dec_clock(slot_id);
          }
          ++slot_id;
        }
        n_slots_available = 0;
      }
      _clock_ptr = slot_id;
    }

    for (uint32_t slot_id_ = slot_id - n_slots_to_occupy;
        slot_id_ < slot_id; ++slot_id_) {
      init_clock(slot_id_);
    }

    //std::cout << slot_id << " " << n_slots_to_occupy << std::endl;

    return slot_id - n_slots_to_occupy;
  }

  inline void CAClock::init_clock(uint32_t index) {
    _clock->set_v(index, 1);
  }
  inline uint32_t CAClock::get_clock(uint32_t index) {
    return _clock->get_v(index);
  }
  inline void CAClock::inc_clock(uint32_t index) {
    uint32_t v = _clock->get_v(index);
    if (v != 3) {
      _clock->set_v(index, v + 1);
    }
  }
  inline void CAClock::dec_clock(uint32_t index) { 
    uint32_t v = _clock->get_v(index);
    if (v != 0) {
      _clock->set_v(index, v - 1);
    }
  }

}
