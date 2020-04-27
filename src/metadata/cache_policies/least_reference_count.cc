#include <metadata/reference_counter.h>
#include <common/stats.h>
#include <manage/dirtylist.h>
#include "least_reference_count.h"
 

namespace cache {
    LeastReferenceCountExecutor::LeastReferenceCountExecutor(Bucket *bucket)
      : CachePolicyExecutor(bucket) {}

    void LeastReferenceCountExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy) {}

    void LeastReferenceCountExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex) {}

    uint32_t LeastReferenceCountExecutor::allocate(uint32_t nSlotsToOccupy)
    {

      std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>> slotsToReferenceCounts;
      uint32_t slotId = 0, nSlotsAvailable = 0,
        nSlots = bucket_->getnSlots();

      for (slotId = 0; slotId < nSlots; ) {
        if (!bucket_->isValid(slotId)) {
          ++slotId;
          continue;
        }

        uint32_t nSlotsOccupied = 0;
        uint32_t slotId_ = slotId;
        uint64_t key = bucket_->getKey(slotId);
        uint64_t bucketId = bucket_->bucketId_;
        uint64_t fpHash = (bucketId << Config::getInstance().getnBitsPerFpSignature()) | key;
        uint32_t refCount = ReferenceCounter::getInstance().query(fpHash);
        while (slotId < nSlots && bucket_->isValid(slotId)
               && key == bucket_->getKey(slotId)) {
          ++slotId;
          nSlotsOccupied += 1;
        }

        slotsToReferenceCounts.emplace_back(slotId_, std::make_pair(refCount, nSlotsOccupied));
      }

      std::sort(slotsToReferenceCounts.begin(),
          slotsToReferenceCounts.end(),
          [](auto &left, auto &right) {
          uint32_t refCount1 = left.second.first, refCount2 = right.second.first;
          uint32_t nOccupied1 = left.second.second, nOccupied2 = right.second.second;
          return refCount1 < refCount2;
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

        if (Config::getInstance().getCacheMode() == tWriteBack) {
          DirtyList::getInstance().addEvictedChunk(
            /* Compute ssd location of the evicted data */
            /* Actually, full Fingerprint and address is sufficient. */
            FPIndex::computeCachedataLocation(bucket_->getBucketId(), slotsToReferenceCounts[0].first),
            (slotId - slotsToReferenceCounts[0].first) * Config::getInstance().getSubchunkSize()
          );
        }

 
        slotsToReferenceCounts.erase(slotsToReferenceCounts.begin());
      }

      return slotId - nSlotsAvailable;
    }

    LeastReferenceCount::LeastReferenceCount() = default;

    CachePolicyExecutor* LeastReferenceCount::getExecutor(Bucket *bucket) {
      return new LeastReferenceCountExecutor(bucket);
    }
}
