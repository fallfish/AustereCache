#include "cache_policy.h"
#include "bucket.h"
#include "index.h"
#include "common/stats.h"
#include "common/config.h"
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

  CAClockExecutor::CAClockExecutor(Bucket *bucket, std::shared_ptr<Bucket> clock, uint32_t *clockPtr) :
    CachePolicyExecutor(bucket),
    clock_(std::move(clock)),
    clockPtr_(clockPtr)
  {}

  CAClockExecutor::~CAClockExecutor()
  {
  }

  void LRUExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy)
  {
    uint32_t n_slots = _bucket->getnSlots();
    uint32_t k = _bucket->getKey(slotId);
    uint64_t v = _bucket->getValue(slotId);
    // Move each slot to the tail
    for ( ; slotId < n_slots - nSlotsToOccupy; ++slotId) {
      _bucket->setInvalid(slotId);
      _bucket->setKey(slotId,
                      _bucket->getKey(slotId + nSlotsToOccupy));
      _bucket->setValue(slotId,
                        _bucket->getValue(slotId + nSlotsToOccupy));
      if (_bucket->isValid(slotId + nSlotsToOccupy)) {
        _bucket->setValid(slotId);
      }
    }
    // Store the promoted slots to the front
    for ( ; slotId < n_slots; ++slotId) {
      _bucket->setKey(slotId, k);
      _bucket->setValue(slotId, v);
      _bucket->setValid(slotId);
    }
  }

  // Only LBA Index would call this function
  // LBA signature only takes one slot.
  // So there is no need to care about the entry may take contiguous slots.
  void LRUExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex)
  {
    for (uint32_t slot_id = 0; slot_id < _bucket->getnSlots(); ++slot_id) {
      if (!_bucket->isValid(slot_id)) continue;

      uint32_t size; uint64_t ssd_location, metadata_location;// useless variables
      bool valid = false;
      uint64_t fp_hash = _bucket->getValue(slot_id);
      if (fpIndex != nullptr)
        valid = fpIndex->lookup(fp_hash, size, ssd_location, metadata_location);
      // if the slot has no mappings in ca index, it is an empty slot
      if (!valid) {
        _bucket->setKey(slot_id, 0), _bucket->setValue(slot_id, 0);
        _bucket->setInvalid(slot_id);
      }
    }
  }

  uint32_t LRUExecutor::allocate(uint32_t nSlotsToOccupy)
  {
    uint32_t slot_id = 0, n_slots_available = 0,
             n_slots = _bucket->getnSlots();
    for ( ; slot_id < n_slots; ++slot_id) {
      if (n_slots_available == nSlotsToOccupy)
        break;
      // find an empty slot
      if (!_bucket->isValid(slot_id)) {
        ++n_slots_available;
      } else {
        n_slots_available = 0;
      }
    }

    if (n_slots_available < nSlotsToOccupy) {
      slot_id = 0;
      // Evict Least Recently Used slots
      for ( ; slot_id < n_slots; ) {
        if (slot_id >= nSlotsToOccupy) break;
        if (!_bucket->isValid(slot_id)) { ++slot_id; continue; }

        uint32_t k = _bucket->getKey(slot_id);
        while (slot_id < n_slots
            && _bucket->getKey(slot_id) == k) {
          Stats::getInstance()->add_lba_index_eviction_caused_by_capacity();
          _bucket->setInvalid(slot_id);
          _bucket->setKey(slot_id, 0);
          _bucket->setValue(slot_id, 0);
          slot_id++;
        }
      }
    }

    return slot_id - nSlotsToOccupy;
  }

  void CAClockExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy)
  {

    for (uint32_t slot_id_ = slotId;
        slot_id_ < slotId + nSlotsToOccupy;
        ++slot_id_) {
      incClock(slot_id_);
    }
  }

  void CAClockExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex)
  {
  }

  uint32_t CAClockExecutor::allocate(uint32_t nSlotsToOccupy)
  {
    uint32_t slot_id = 0, n_slots_available = 0,
             n_slots = _bucket->getnSlots();
    // check whether there is a contiguous space
    for ( ; slot_id < n_slots; ++slot_id) {
      if (n_slots_available == nSlotsToOccupy)
        break;
      // find an empty slot
      if (!_bucket->isValid(slot_id)) {
        ++n_slots_available;
      } else {
        n_slots_available = 0;
      }
    }

    if (n_slots_available < nSlotsToOccupy) {
      // No contiguous space, need to evict previous slot
      slot_id = *clockPtr_;
      // adjust the pointer to the head of an object (chunk) rather than in the middle
      // E.g., a previous compressibility-3 chunk was evicted, and a compressibility-4 chunk
      //       was inserted in-place, which leaves pointer the middle of compressibility-4 chunk
      //       and cause a false clock deference.
      if (slot_id > 0 && _bucket->isValid(slot_id)
          && _bucket->getKey(slot_id - 1) == _bucket->getKey(slot_id)) {
        uint32_t k = _bucket->getKey(slot_id);
        while (slot_id < n_slots 
            && k == _bucket->getKey(slot_id)) {
          ++slot_id;
        }
      }
      n_slots_available = 0;

      while (1) {
        if (n_slots_available >= nSlotsToOccupy) break;
        if (slot_id == n_slots) {
          n_slots_available = 0;
          slot_id = 0;
        }
        if (!_bucket->isValid(slot_id)) {
          ++n_slots_available;
          ++slot_id;
          continue;
        }
        // to allocate
        uint32_t k = _bucket->getKey(slot_id);
        uint32_t c = getClock(slot_id);
        bool evicted = false;
        uint32_t slot_id_begin = slot_id;
        while (slot_id < n_slots
            && k == _bucket->getKey(slot_id)) {
          if (getClock(slot_id) == 0) {
            evicted = true;
            _bucket->setInvalid(slot_id);
            ++n_slots_available;
          } else {
            decClock(slot_id);
            n_slots_available = 0;
          }
          ++slot_id;
        }
        if (evicted) {
#ifdef WRITE_BACK_CACHE
          DirtyList::getInstance()->add_evicted_block(
              /* Compute ssd location of the evicted data */
              /* Actually, full FP and address is sufficient. */
                (_bucket->get_bucket_id() * _bucket->get_n_slots() + slot_id_begin) * 1LL *
                (Config::getInstance()->get_sector_size() +
                 Config::getInstance()->get_metadata_size()),
                slot_id - slot_id_begin
              );
#endif
          Stats::getInstance()->add_fp_index_eviction_caused_by_capacity();
        }
      }
      *clockPtr_ = slot_id;
    }

    for (uint32_t slot_id_ = slot_id - nSlotsToOccupy;
        slot_id_ < slot_id; ++slot_id_) {
      initClock(slot_id_);
    }

    return slot_id - nSlotsToOccupy;
  }

  inline void CAClockExecutor::initClock(uint32_t index) {
    clock_->setValue(index, 1);
  }
  inline uint32_t CAClockExecutor::getClock(uint32_t index) {
    return clock_->getValue(index);
  }
  inline void CAClockExecutor::incClock(uint32_t index) {
    uint32_t v = clock_->getValue(index);
    if (v != (1 << Config::getInstance()->getnBitsPerClock()) - 1) {
      clock_->setValue(index, v + 1);
    }
  }
  inline void CAClockExecutor::decClock(uint32_t index) {
    uint32_t v = clock_->getValue(index);
    if (v != 0) {
      clock_->setValue(index, v - 1);
    }
  }

  CachePolicy::CachePolicy() {}

  LRU::LRU() {}

  CAClock::CAClock(uint32_t nSlotsPerBucket, uint32_t nBuckets) : CachePolicy() {
    nSlotsPerBucket_ = nSlotsPerBucket;
    nBytesPerBucket_ = (Config::getInstance()->getnBitsPerClock() * nSlotsPerBucket + 7) / 8;
    clock_ = std::make_unique< uint8_t[] >(nBytesPerBucket_ * nBuckets);
    clockPtr_ = 0;
  }

  std::shared_ptr<CachePolicyExecutor> LRU::getExecutor(Bucket *bucket)
  { 
    return std::make_shared<LRUExecutor>(bucket);
  }

  // TODO: Check whether there is memory leak when destructing
  std::shared_ptr<CachePolicyExecutor> CAClock::getExecutor(Bucket *bucket)
  { 
    return std::make_shared<CAClockExecutor>(
          bucket, std::move(getBucket(bucket->getBucketId())),
          &clockPtr_);
  }
}
