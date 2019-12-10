#include <metadata/ReferenceCounter.h>
#include <common/stats.h>
#include <manage/DirtyList.h>
#include "ThresholdRCClock.h"

namespace cache {

  ThresholdRCClockExecutor::ThresholdRCClockExecutor(
      Bucket *bucket, std::shared_ptr<Bucket> clock,
      uint32_t *clockPtr,
      uint32_t threshold
      ) :
    CAClockExecutor(bucket, std::move(clock), clockPtr),
    threshold_(threshold)
  {}

    void ThresholdRCClockExecutor::promote(uint32_t slotId, uint32_t nSlotsOccupied)
    {
      CAClockExecutor::promote(slotId, nSlotsOccupied);
    }

    uint32_t ThresholdRCClockExecutor::allocate(uint32_t nSlotsToOccupy)
    {
      uint32_t slotId = 0, nSlotsAvailable = 0,
        nSlots = bucket_->getnSlots();
      std::vector<std::pair<uint32_t, uint32_t>> slotsToReferenceCounts;
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
        for (slotId = 0; slotId < nSlots; ) {
          if (!bucket_->isValid(slotId)) {
            ++slotId;
            continue;
          }
          uint64_t key = bucket_->getKey(slotId);
          uint64_t bucketId = bucket_->bucketId_;
          uint64_t fpHash = (bucketId << Config::getInstance().getnBitsPerFpSignature()) | key;
          uint32_t refCount = ReferenceCounter::getInstance().query(fpHash);
          if (refCount <= threshold_) {
            slotsToReferenceCounts.emplace_back(slotId, refCount);
          }
          while (slotId < nSlots && bucket_->isValid(slotId)
                 && key == bucket_->getKey(slotId)) {
            ++slotId;
          }
        }
        std::sort(slotsToReferenceCounts.begin(),
                  slotsToReferenceCounts.end(),
                  [this](auto &left, auto &right) {
                      return getClock(left.first) < getClock(right.first);
                  });
      }

      while (!slotsToReferenceCounts.empty()) {
        // check whether there is a contiguous space
        // try to evict those zero referenced fingerprint first
        slotId = 0;
        nSlotsAvailable = 0;
        for (; slotId < nSlots; ++slotId) {
          if (nSlotsAvailable >= nSlotsToOccupy)
            break;
          // find an empty slot
          if (!bucket_->isValid(slotId)) {
            ++nSlotsAvailable;
          } else {
            nSlotsAvailable = 0;
          }
        }

        if (nSlotsAvailable >= nSlotsToOccupy) break;
        // Evict the least RF entry
        slotId = slotsToReferenceCounts[0].first;
        uint64_t key = bucket_->getKey(slotId);
        while (slotId < nSlots && bucket_->isValid(slotId)
               && key == bucket_->getKey(slotId)) {
          bucket_->setInvalid(slotId);
          ++slotId;
        }
        if (Config::getInstance().getCacheMode() == tWriteBack) {
          DirtyList::getInstance().addEvictedChunk(
            /* Compute ssd location of the evicted data */
            /* Actually, full Fingerprint and address is sufficient. */
            FPIndex::computeCachedataLocation(bucket_->getBucketId(), slotsToReferenceCounts[0].first),
            (slotId - slotsToReferenceCounts[0].first) * Config::getInstance().getSectorSize()
          );
        }

        Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
        slotsToReferenceCounts.erase(slotsToReferenceCounts.begin());
      }

      slotId = CAClockExecutor::allocate(nSlotsToOccupy);
      return slotId;
    }


    std::shared_ptr<CachePolicyExecutor> ThresholdRCClock::getExecutor(cache::Bucket *bucket)
    {
      return std::make_shared<ThresholdRCClockExecutor>(
        bucket, std::move(getBucket(bucket->getBucketId())),
        &clockPtr_, threshold_);
    }
    ThresholdRCClock::ThresholdRCClock(uint32_t nSlotsPerBucket, uint32_t nBuckets, uint32_t threshold) :
      CAClock(nSlotsPerBucket, nBuckets),
      threshold_(threshold)
    {}
}
