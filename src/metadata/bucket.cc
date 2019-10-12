#include <utility>

#include <cstring>
#include <cassert>
#include "bitmap.h"
#include "bucket.h"
#include "index.h"
#include "cache_policy.h"
#include "common/stats.h"
#include "manage/dirty_list.h"
#include <csignal>

#include "validators.h"

namespace cache {


  Bucket::Bucket(uint32_t nBitsPerKey, uint32_t nBitsPerValue, uint32_t nSlots,
      uint8_t *data, uint8_t *valid, CachePolicy *cachePolicy, uint32_t slotId) :
    nBitsPerKey_(nBitsPerKey), nBitsPerValue_(nBitsPerValue),
    nBitsPerSlot_(nBitsPerKey + nBitsPerValue), nSlots_(nSlots),
    data_(data), valid_(valid), bucketId_(slotId)
  {
    if (cachePolicy != nullptr) {
      cachePolicy_ = std::move(cachePolicy->getExecutor(this));
    } else {
      cachePolicy_ = nullptr;
    }
  }

  Bucket::~Bucket()
  {
  }

  uint32_t LBABucket::lookup(uint32_t lbaSignature, uint64_t &fpHash) {
    for (uint32_t slotId = 0; slotId < nSlots_; slotId++) {
      if (!isValid(slotId)) continue;
      uint32_t _lbaSignature = getKey(slotId);
      if (_lbaSignature == lbaSignature) {
        fpHash = getValue(slotId);
        return slotId;
      }
    }
    return ~((uint32_t)0);
  }

  void LBABucket::promote(uint32_t lbaSignature) {
    uint64_t fingerprintHash = 0;
    uint32_t slotId = lookup(lbaSignature, fingerprintHash);
    //assert(slotId != ~((uint32_t)0));
    cachePolicy_->promote(slotId);
  }

  void LBABucket::update(uint32_t lbaSignature, uint64_t fingerprintHash, std::shared_ptr<FPIndex> fingerprintIndex) {
    uint64_t _fingerprintHash = 0;
    uint32_t slotId = lookup(lbaSignature, _fingerprintHash);
    if (slotId != ~((uint32_t)0)) {
      if (fingerprintHash == getValue(slotId)) {
        promote(lbaSignature);
        return ;
      }
      Stats::getInstance()->add_lba_index_eviction_caused_by_collision();
      setInvalid(slotId);
    }

    // If is full, clear all obsoletes first
    // Warning: Number of slots is 32, so a uint32_t comparison is efficient.
    //          Any other design with larger number of slots should change this
    //          one.
    if (getValid32bits(0) == ~(uint32_t)0) {
      cachePolicy_->clearObsolete(std::move(fingerprintIndex));
    }
    slotId = cachePolicy_->allocate();
    setKey(slotId, lbaSignature);
    setValue(slotId, fingerprintHash);
    setValid(slotId);
    cachePolicy_->promote(slotId);
  }


  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  uint32_t FPBucket::lookup(uint32_t fpSignature, uint32_t &nSlotsOccupied)
  {
    uint32_t slotId = 0;
    nSlotsOccupied = 0;
    for ( ; slotId < nSlots_; ) {
      if (!isValid(slotId)
          || fpSignature != getKey(slotId)) {
        ++slotId;
        continue;
      }
      while (slotId < nSlots_
          && isValid(slotId)
          && fpSignature == getKey(slotId)) {
        ++nSlotsOccupied;
        ++slotId;
      }
      return slotId - nSlotsOccupied;
    }
    return ~((uint32_t)0);
  }


  void FPBucket::promote(uint32_t fpSignature)
  {
    uint32_t slot_id = 0, compressibility_level = 0, n_slots_occupied;
    slot_id = lookup(fpSignature, n_slots_occupied);
    //assert(slot_id != ~((uint32_t)0));

    cachePolicy_->promote(slot_id, n_slots_occupied);
  }

  uint32_t FPBucket::update(uint32_t fpSignature, uint32_t nSlotsToOccupy)
  {
    uint32_t nSlotsOccupied = 0;
    uint32_t slotId = lookup(fpSignature, nSlotsOccupied);
    if (slotId != ~((uint32_t)0)) {
      // TODO: Add it into the evicted data of dirty list 
      Stats::getInstance()->add_fp_index_eviction_caused_by_collision();
#ifdef WRITE_BACK_CACHE
      DirtyList::getInstance()->add_evicted_block(
            /* Compute ssd location of the evicted data */
            /* Actually, full FP and address is sufficient. */
            (bucketId_ * nSlots_ + slotId) * 1LL *
            (Config::getInstance()->get_sector_size() +
             Config::getInstance()->get_metadata_size()),
            nSlotsOccupied
          );
#endif
          
      for (uint32_t _slotId = slotId;
          _slotId < slotId + nSlotsOccupied;
          ++_slotId) {
        setInvalid(_slotId);
      }
    }

    slotId = cachePolicy_->allocate(nSlotsToOccupy);
    for (uint32_t _slotId = slotId;
        _slotId < slotId + nSlotsToOccupy;
        ++_slotId) {
      setKey(_slotId, fpSignature);
      setValid(_slotId);
    }

    return slotId;
  }

  void FPBucket::erase(uint32_t fpSignature)
  {
    for (uint32_t index = 0; index < nSlots_; index++) {
      if (getKey(index) == fpSignature) {
        setInvalid(index);
      }
    }
  }
}
