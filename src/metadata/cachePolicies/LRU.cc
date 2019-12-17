#include "LRU.h"

namespace cache {

    LRUExecutor::LRUExecutor(
        Bucket *bucket, std::list<uint32_t> *list) :
      CachePolicyExecutor(bucket) {
      list_ = list;
    }

    void LRUExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy) {
      list_->remove(slotId);
      list_->push_front(slotId);
    }

    void LRUExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex) {

    }

    uint32_t LRUExecutor::allocate(uint32_t nSlotsToOccupy) {
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
        slotId = list_->back();
        uint32_t key = bucket_->getKey(slotId);
        while (nSlotsAvailable < nSlotsToOccupy && slotId < nSlots) {
          if (bucket_->isValid(slotId)
              && key == bucket_->getKey(slotId)) {
            nSlotsAvailable += 1;
          }
          ++slotId;
        }
      }

      return slotId - nSlotsToOccupy;
    }

    CachePolicyExecutor* LRU::getExecutor(Bucket *bucket) {
      return new LRUExecutor(bucket, &lists_[bucket->getBucketId()]);
    }

    LRU::LRU(uint32_t nBuckets) {
      lists_ = std::make_unique<std::list<uint32_t>[]>(nBuckets);
    }
}
