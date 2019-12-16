//
//

#include <common/stats.h>
#include "BucketAwareLRU.h"
#include "common/config.h"
#include "metadata/ReferenceCounter.h"
namespace cache {

    BucketAwareLRUExecutor::BucketAwareLRUExecutor(Bucket *bucket) :
      CachePolicyExecutor(bucket)
    {}

    void BucketAwareLRUExecutor::promote(uint32_t slotId, uint32_t nSlotsToOccupy)
    {
      uint32_t prevSlotId = slotId;
      uint32_t nSlots = bucket_->getnSlots();
      uint32_t k = bucket_->getKey(slotId);
      uint64_t v = bucket_->getValue(slotId);
      if (Config::getInstance().getCachePolicyForFPIndex() ==
          CachePolicyEnum::tRecencyAwareLeastReferenceCount) {
        if (prevSlotId < Config::getInstance().getLBASlotSeperator()) {
          ReferenceCounter::getInstance().reference(v);
          if (bucket_->isValid(Config::getInstance().getLBASlotSeperator())) {
            ReferenceCounter::getInstance().dereference(bucket_->getValue(Config::getInstance().getLBASlotSeperator()));
          }
        }
      }
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
    void BucketAwareLRUExecutor::clearObsolete(std::shared_ptr<FPIndex> fpIndex)
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

    uint32_t BucketAwareLRUExecutor::allocate(uint32_t nSlotsToOccupy)
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

    BucketAwareLRU::BucketAwareLRU() = default;
    CachePolicyExecutor* BucketAwareLRU::getExecutor(Bucket *bucket)
    {
      return new BucketAwareLRUExecutor(bucket);
    }

}
