#include "LRU.h"

namespace cache {

    LRUExecutor::LRUExecutor(
        Bucket *bucket, std::list<uint32_t> *list,
        std::map<uint32_t, std::list<uint32_t>::iterator> *slotId2listPosition) :
      CachePolicyExecutor(bucket) {
      list_ = list;
      slotId2listPosition_ = slotId2listPosition;
    }

    void LRUExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy) {
      if (slotId2listPosition_->find(slotId) != slotId2listPosition_->end()) {
        list_->erase((*slotId2listPosition_)[slotId]);
      }
      list_->push_front(slotId);
      (*slotId2listPosition_)[slotId] = list_->begin();
      if (list_->size() > 32)
        std::cout << list_->size() << std::endl;
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
      return new LRUExecutor(bucket, &lists_[bucket->getBucketId()], &slotId2listPosition_[bucket->getBucketId()]);
    }

    LRU::LRU(uint32_t nBuckets) {
      lists_ = std::make_unique<std::list<uint32_t>[]>(nBuckets);
      slotId2listPosition_ = std::make_unique< std::map<uint32_t, std::list<uint32_t>::iterator>[] >(nBuckets);
    }
}
