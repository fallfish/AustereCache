#include <common/stats.h>
#include "bucketdlru_lbaindex.h"
namespace cache {
    CacheDedupLBABucket::CacheDedupLBABucket() = default;
    CacheDedupLBABucket::CacheDedupLBABucket(uint32_t capacity) {
      capacity_ = capacity;
      keys_.resize(capacity);
      values_.resize(capacity);
      valid_.resize(capacity);
    }

    void CacheDedupLBABucket::init()
    {
      capacity_ = Config::getInstance().getnSourceIndexEntries();
    }

    /**
     * Check whether the address/index is stored in the cache
     */
    uint32_t CacheDedupLBABucket::lookup(uint64_t lba, uint8_t *fp) {
      for (int i = 0; i < capacity_; ++i) {
        if (valid_[i] && keys_[i] == lba) {
          memcpy(fp, values_[i].v_, Config::getInstance().getFingerprintLength());
          return i;
        }
      }
      return ~0u;
    }

    /**
     * move the accessed index to the front
     */
    void CacheDedupLBABucket::promote(uint64_t lba) {
      list_.remove(lba);
      list_.push_front(lba);
    }

    /**
     * move
     */
    bool CacheDedupLBABucket::update(uint64_t lba, uint8_t *fp, uint8_t *oldFP) {
      bool evicted = false;
      uint32_t slotId = lookup(lba, oldFP);

      if (slotId != ~0u) {
        evicted = true;
        promote(lba);
      } else {
        if (list_.size() == capacity_) {
          uint64_t lba_ = list_.back();
          list_.pop_back();

          uint32_t evictedSlotId = lookup(lba_, oldFP);
          evicted = true;
          valid_[evictedSlotId] = false;

          slotId = evictedSlotId;
 
        } else {
          for (int i = 0; i < capacity_; ++i) {
            if (!valid_[i]) {
              slotId = i;
              break;
            }
          }
        }
        list_.push_front(lba);
      }
      valid_[slotId] = true;
      keys_[slotId] = lba;
      memcpy(values_[slotId].v_, fp, Config::getInstance().getFingerprintLength());

      return evicted;
    }

    BucketizedDLRULBAIndex::BucketizedDLRULBAIndex() {
      nSlotsPerBucket_ = Config::getInstance().getnLBASlotsPerBucket();
      nBuckets_ = Config::getInstance().getnLbaBuckets();
      buckets_ = new CacheDedupLBABucket *[nBuckets_];
      std::cout << "Number of LBA buckets: " << nBuckets_ << std::endl;
      for (int i = 0; i < nBuckets_; ++i) {
        buckets_[i] = new CacheDedupLBABucket(nSlotsPerBucket_);
      }
    }

    void BucketizedDLRULBAIndex::promote(uint64_t lba) {
      buckets_[computeBucketId(lba)]->promote(lba);
    }

    bool BucketizedDLRULBAIndex::lookup(uint64_t lba, uint8_t *fp) {
      return (buckets_[computeBucketId(lba)]->lookup(lba, fp) != ~0u);
    }

    bool BucketizedDLRULBAIndex::update(uint64_t lba, uint8_t *fp, uint8_t *oldFP) {
      return buckets_[computeBucketId(lba)]->update(lba, fp, oldFP);
    }

    uint32_t BucketizedDLRULBAIndex::computeBucketId(uint64_t lba) {
      uint32_t hash = XXH32(reinterpret_cast<const void *>(&lba), 8, 1);
      return hash % nBuckets_;
    }
}
