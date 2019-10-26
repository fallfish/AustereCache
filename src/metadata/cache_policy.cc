#include "cache_policy.h"
#include "bucket.h"
#include "index.h"
#include "common/stats.h"
#include "common/config.h"
#include "manage/dirty_list.h"
#include "ReferenceCounter.h"

#include <memory>
#include <csignal>

namespace cache {
  CachePolicyExecutor::CachePolicyExecutor(Bucket *bucket) :
    bucket_(bucket)
  {}

  LRUExecutor::LRUExecutor(Bucket *bucket) :
    CachePolicyExecutor(bucket)
  {}

  CAClockExecutor::CAClockExecutor(Bucket *bucket, std::shared_ptr<Bucket> clock, uint32_t *clockPtr) :
    CachePolicyExecutor(bucket),
    clock_(std::move(clock)),
    clockPtr_(clockPtr)
  {}

  CAClockExecutor::~CAClockExecutor() = default;

  void LRUExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy)
  {
    uint32_t nSlots = bucket_->getnSlots();
    uint32_t k = bucket_->getKey(slotId);
    uint64_t v = bucket_->getValue(slotId);
    // Move each slot to the tail
    for ( ; slotId < nSlots - nSlotsToOccupy; ++slotId) {
      bucket_->setInvalid(slotId);
      bucket_->setKey(slotId, bucket_->getKey(slotId + nSlotsToOccupy));
      bucket_->setValue(slotId, bucket_->getValue(slotId + nSlotsToOccupy));
      if (bucket_->isValid(slotId + nSlotsToOccupy)) {
        bucket_->setValid(slotId);
      }
    }
    // Store the promoted slots to the front (higher slotId is the front)
    for ( ; slotId < nSlots; ++slotId) {
      bucket_->setKey(slotId, k);
      bucket_->setValue(slotId, v);
      bucket_->setValid(slotId);
    }
  }

  // Only LBA Index would call this function
  // LBA signature only takes one slot.
  // So there is no need to care about the entry may take contiguous slots.
  void LRUExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex)
  {
    for (uint32_t slotId = 0; slotId < bucket_->getnSlots(); ++slotId) {
      if (!bucket_->isValid(slotId)) continue;

      uint32_t size; uint64_t cachedataLocation, metadataLocation;// dummy variables
      bool valid = false;
      uint64_t fpHash = bucket_->getValue(slotId);
      if (fpIndex != nullptr)
        valid = fpIndex->lookup(fpHash, size, cachedataLocation, metadataLocation);
      // if the slot has no mappings in ca index, it is an empty slot
      if (!valid) {
        bucket_->setKey(slotId, 0), bucket_->setValue(slotId, 0);
        bucket_->setInvalid(slotId);
      }
    }
  }

  uint32_t LRUExecutor::allocate(uint32_t nSlotsToOccupy)
  {
    uint32_t slotId = 0, nSlotsAvailable = 0,
             nSlots = bucket_->getnSlots();
    for ( ; slotId < nSlots; ++slotId) {
      if (nSlotsAvailable == nSlotsToOccupy)
        break;
      // find an empty slot
      if (!bucket_->isValid(slotId)) {
        ++nSlotsAvailable;
      } else {
        nSlotsAvailable = 0;
      }
    }

    if (nSlotsAvailable < nSlotsToOccupy) {
      slotId = 0;
      // Evict Least Recently Used slots
      for ( ; slotId < nSlots; ) {
        if (slotId >= nSlotsToOccupy) break;
        if (!bucket_->isValid(slotId)) { ++slotId; continue; }

        uint32_t key = bucket_->getKey(slotId);
        bucket_->setEvictedSignature(bucket_->getValue(slotId));
        while (slotId < nSlots
               && bucket_->getKey(slotId) == key) {
          Stats::getInstance().add_lba_index_eviction_caused_by_capacity();
          bucket_->setInvalid(slotId);
          bucket_->setKey(slotId, 0);
          bucket_->setValue(slotId, 0);
          slotId++;
        }
      }
    }

    return slotId - nSlotsToOccupy;
  }

  void CAClockExecutor::promote(uint32_t slotId, uint32_t nSlotsOccupied)
  {

    for (uint32_t _slotId = slotId;
         _slotId < slotId + nSlotsOccupied;
         ++_slotId) {
      incClock(_slotId);
    }
  }

  void CAClockExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex)
  {
  }

  uint32_t CAClockExecutor::allocate(uint32_t nSlotsToOccupy)
  {
    uint32_t slotId = 0, nSlotsAvailable = 0,
             nSlots = bucket_->getnSlots();
    // check whether there is a contiguous space
    for ( ; slotId < nSlots; ++slotId) {
      if (nSlotsAvailable == nSlotsToOccupy)
        break;
      // find an empty slot
      if (!bucket_->isValid(slotId)) {
        ++nSlotsAvailable;
      } else {
        nSlotsAvailable = 0;
      }
    }

    // try to evict those zero referenced fingerprint first
    if (nSlotsAvailable < nSlotsToOccupy) {
      nSlotsAvailable = 0;
      slotId = 0;
      for ( ; slotId < nSlots; ) {
        // to allocate
        if (nSlotsAvailable >= nSlotsToOccupy) break;
        if (!bucket_->isValid(slotId)) {
          ++nSlotsAvailable;
          ++slotId;
          continue;
        }
        uint32_t key = bucket_->getKey(slotId);
        bool shouldEvict = ReferenceCounter::getInstance().query((bucket_->bucketId_ << bucket_->nBitsPerKey_) || key);
        uint32_t slot_id_begin = slotId;
        while (slotId < nSlots
               && key == bucket_->getKey(slotId)) {
          if (shouldEvict) {
            bucket_->setInvalid(slotId);
            ++nSlotsAvailable;
          }
          ++slotId;
        }
#ifdef WRITE_BACK_CACHE
        DirtyList::getInstance().addEvictedChunk(
          /* Compute ssd location of the evicted data */
          /* Actually, full FP and address is sufficient. */
          FPIndex::computeCachedataLocation(bucket_->getBucketId(), slot_id_begin),
          (slotId - slot_id_begin) * Config::getInstance().getSectorSize()
        );
#endif
        Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
      }
    }

    if (nSlotsAvailable < nSlotsToOccupy) {
      // No contiguous space, need to evict previous slot
      slotId = *clockPtr_;
      // adjust the pointer to the head of an object (chunk) rather than in the middle
      // E.g., a previous compressibility-3 chunk was evicted, and a compressibility-4 chunk
      //       was inserted in-place, which leaves pointer the middle of compressibility-4 chunk
      //       and cause a false clock dereference.
      if (slotId > 0 && bucket_->isValid(slotId)
          && bucket_->getKey(slotId - 1) == bucket_->getKey(slotId)) {
        uint32_t k = bucket_->getKey(slotId);
        while (slotId < nSlots
               && k == bucket_->getKey(slotId)) {
          ++slotId;
        }
      }
      nSlotsAvailable = 0;

      while (true) {
        if (nSlotsAvailable >= nSlotsToOccupy) break;
        if (slotId == nSlots) {
          nSlotsAvailable = 0;
          slotId = 0;
        }
        if (!bucket_->isValid(slotId)) {
          ++nSlotsAvailable;
          ++slotId;
          continue;
        }
        // to allocate
        uint32_t key = bucket_->getKey(slotId);
        bool evicted = false;
        uint32_t slot_id_begin = slotId;
        while (slotId < nSlots
               && key == bucket_->getKey(slotId)) {
          if (getClock(slotId) == 0) {
            evicted = true;
            bucket_->setInvalid(slotId);
            ++nSlotsAvailable;
          } else {
            decClock(slotId);
            nSlotsAvailable = 0;
          }
          ++slotId;
        }
        if (evicted) {
#ifdef WRITE_BACK_CACHE
          DirtyList::getInstance().addEvictedChunk(
              /* Compute ssd location of the evicted data */
              /* Actually, full FP and address is sufficient. */
                FPIndex::computeCachedataLocation(bucket_->getBucketId(), slot_id_begin),
                (slotId - slot_id_begin) * Config::getInstance().getSectorSize()
              );
#endif
          Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
        }
      }
      *clockPtr_ = slotId;
    }

    for (uint32_t _slotId = slotId - nSlotsToOccupy;
         _slotId < slotId; ++_slotId) {
      initClock(_slotId);
    }

    return slotId - nSlotsToOccupy;
  }

  inline void CAClockExecutor::initClock(uint32_t index) {
    clock_->setValue(index, 1);
  }
  inline uint32_t CAClockExecutor::getClock(uint32_t index) {
    return clock_->getValue(index);
  }
  inline void CAClockExecutor::incClock(uint32_t index) {
    uint32_t v = clock_->getValue(index);
    if (v != (1u << Config::getInstance().getnBitsPerClock()) - 1) {
      clock_->setValue(index, v + 1);
    }
  }
  inline void CAClockExecutor::decClock(uint32_t index) {
    uint32_t v = clock_->getValue(index);
    if (v != 0) {
      clock_->setValue(index, v - 1);
    }
  }

  CachePolicy::CachePolicy() = default;
  LRU::LRU() = default;

  CAClock::CAClock(uint32_t nSlotsPerBucket, uint32_t nBuckets) : CachePolicy() {
    nSlotsPerBucket_ = nSlotsPerBucket;
    nBytesPerBucket_ = (Config::getInstance().getnBitsPerClock() * nSlotsPerBucket + 7) / 8;
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

  std::shared_ptr<Bucket> CAClock::getBucket(uint32_t bucketId) {
    return std::make_shared<Bucket>(
      0, 2, nSlotsPerBucket_,
      clock_.get() + nBytesPerBucket_ * bucketId,
      nullptr,
      nullptr, bucketId);
  }
}
