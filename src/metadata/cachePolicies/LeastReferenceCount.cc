#include <metadata/ReferenceCounter.h>
#include <common/stats.h>
#include "LeastReferenceCount.h"

namespace cache {
    LeastReferenceCountExecutor::LeastReferenceCountExecutor(Bucket *bucket)
      : CachePolicyExecutor(bucket) {}

    void LeastReferenceCountExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy) {}

    void LeastReferenceCountExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex) {}

    uint32_t LeastReferenceCountExecutor::allocate(uint32_t nSlotsToOccupy)
    {

      std::vector<std::pair<uint32_t, uint32_t>> slotsToReferenceCounts;
      uint32_t slotId = 0, nSlotsAvailable = 0,
        nSlots = bucket_->getnSlots();

      for (slotId = 0; slotId < nSlots; ) {
        if (!bucket_->isValid(slotId)) {
          ++slotId;
          continue;
        }
        uint64_t key = bucket_->getKey(slotId);
        uint64_t bucketId = bucket_->bucketId_;
        uint64_t fpHash = (bucketId << Config::getInstance().getnBitsPerFpSignature()) | key;
        uint32_t refCount = ReferenceCounter::query(fpHash);

        slotsToReferenceCounts.emplace_back(slotId, refCount);
        while (slotId < nSlots && bucket_->isValid(slotId)
               && key == bucket_->getKey(slotId)) {
          ++slotId;
        }
      }
      std::sort(slotsToReferenceCounts.begin(),
                slotsToReferenceCounts.end(),
                [](auto &left, auto &right) {
                    return left.second < right.second;
                });

      while (true) {
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
#ifdef WRITE_BACK_CACHE
        DirtyList::getInstance().addEvictedChunk(
        /* Compute ssd location of the evicted data */
        /* Actually, full Fingerprint and address is sufficient. */
        FPIndex::computeCachedataLocation(bucket_->getBucketId(), slotsToReferenceCounts[0].first),
        (slotId - slotsToReferenceCounts[0].first) * Config::getInstance().getSectorSize()
      );
#endif

        Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
        slotsToReferenceCounts.erase(slotsToReferenceCounts.begin());
      }

      return slotId - nSlotsAvailable;
    }

    LeastReferenceCount::LeastReferenceCount() = default;

    std::shared_ptr<CachePolicyExecutor> LeastReferenceCount::getExecutor(Bucket *bucket) {
      return std::make_shared<LeastReferenceCountExecutor>(bucket);
    }

}
