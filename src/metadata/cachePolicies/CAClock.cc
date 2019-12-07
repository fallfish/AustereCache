#include "CAClock.h"
#include "common/stats.h"

namespace cache {
    CAClockExecutor::CAClockExecutor(Bucket *bucket, std::shared_ptr<Bucket> clock, uint32_t *clockPtr) :
      CachePolicyExecutor(bucket),
      clock_(std::move(clock)),
      clockPtr_(clockPtr)
    {}

    CAClockExecutor::~CAClockExecutor() = default;

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
      uint32_t nEvicts = 0;
      // check whether there is a contiguous space
      // try to evict those zero referenced fingerprint first
      for ( ; slotId < nSlots; ++slotId) {
        if (nSlotsAvailable >= nSlotsToOccupy)
          break;
        // find an empty slot
        if (!bucket_->isValid(slotId)) {
          ++nSlotsAvailable;
        } else {
          nSlotsAvailable = 0;
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
              /* Actually, full Fingerprint and address is sufficient. */
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

    void CAClockExecutor::initClock(uint32_t index) {
      clock_->setValue(index, Config::getInstance().getClockStartValue());
    }
    uint32_t CAClockExecutor::getClock(uint32_t index) {
      return clock_->getValue(index);
    }
    void CAClockExecutor::incClock(uint32_t index) {
      uint32_t v = clock_->getValue(index);
      if (v != (1u << Config::getInstance().getnBitsPerClock()) - 1) {
        clock_->setValue(index, v + 1);
      }
    }
    void CAClockExecutor::decClock(uint32_t index) {
      uint32_t v = clock_->getValue(index);
      if (v != 0) {
        clock_->setValue(index, v - 1);
      }
    }

    CAClock::CAClock(uint32_t nSlotsPerBucket, uint32_t nBuckets) : CachePolicy() {
      nSlotsPerBucket_ = nSlotsPerBucket;
      nBytesPerBucket_ = (Config::getInstance().getnBitsPerClock() * nSlotsPerBucket + 7) / 8;
      clock_ = std::make_unique< uint8_t[] >(nBytesPerBucket_ * nBuckets);
      clockPtr_ = 0;
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
        0, Config::getInstance().getnBitsPerClock(), nSlotsPerBucket_,
        clock_.get() + nBytesPerBucket_ * bucketId,
        nullptr,
        nullptr, bucketId);
    }
}
