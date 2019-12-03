//
// Created by 王秋平 on 12/3/19.
//

#include <common/stats.h>
#include <manage/DirtyList.h>
#include "BucketDLRUFPIndex.h"
#include "BucketDLRULBAIndex.h"

namespace cache {
    CacheDedupFPBucket::CacheDedupFPBucket() = default;

    uint32_t CacheDedupFPBucket::lookup(uint8_t *fp)
    {
      Fingerprint _fp(fp);
      for (int i = 0; i < capacity_; ++i) {
        if (valid_[i] && keys_[i] == _fp) {
          return i;
        }
      }
      return ~0u;
    }

    void CacheDedupFPBucket::promote(uint32_t slotId)
    {
      list_.remove(slotId);
      list_.push_front(slotId);
    }

    uint32_t CacheDedupFPBucket::update(uint8_t *fp)
    {
      Fingerprint _fp(fp);
      uint32_t slotId = lookup(fp);
      if (slotId != ~0u) {
        promote(slotId);
      } else {
        // Not support write back currently
        if (list_.size() == capacity_) {
          slotId = list_.back();
          Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
        } else {
          for (int i = 0; i < capacity_; ++i) {
            if (!valid_[i]) {
              slotId = i;
              break;
            }
          }
        }

        memcpy(keys_[slotId].v_, fp, Config::getInstance().getFingerprintLength());
        promote(slotId);
      }

      return slotId;
    }

    CacheDedupFPBucket::CacheDedupFPBucket(uint32_t capacity) {
      capacity_ = capacity;
    }

    BucketizedDLRUFPIndex::BucketizedDLRUFPIndex() {
      nSlotsPerBucket_ = Config::getInstance().getnFPSlotsPerBucket();
      nBuckets_ = Config::getInstance().getnFpBuckets();
      buckets_ = new CacheDedupFPBucket *[nBuckets_];
      std::cout << "Number of Fingerprint buckets: " << nBuckets_ << std::endl;
      for (int i = 0; i < nBuckets_; ++i) {
        buckets_[i] = new CacheDedupFPBucket(nSlotsPerBucket_);
      }
    }

    BucketizedDLRUFPIndex BucketizedDLRUFPIndex::getInstance() {
      static BucketizedDLRUFPIndex instance;
      return instance;
    }

    bool BucketizedDLRUFPIndex::lookup(uint8_t *fp, uint64_t &cachedataLocation) {
      uint32_t bucketId = computeBucketId(fp);
      uint32_t slotId = buckets_[bucketId]->update(fp);
      cachedataLocation = (bucketId * nSlotsPerBucket_ + slotId) * 1LL * Config::getInstance().getChunkSize();
      return slotId != ~0u;
    }

    void BucketizedDLRUFPIndex::promote(uint8_t *fp) {
    }

    void BucketizedDLRUFPIndex::update(uint8_t *fp, uint64_t &cachedataLocation) {
      uint32_t bucketId = computeBucketId(fp);
      uint32_t slotId = buckets_[bucketId]->update(fp);
      cachedataLocation = (bucketId * nSlotsPerBucket_ + slotId) * 1LL * Config::getInstance().getChunkSize();
    }

    void BucketizedDLRUFPIndex::reference(uint8_t *fp) {
    }

    void BucketizedDLRUFPIndex::dereference(uint8_t *fp) {
    }

    uint32_t BucketizedDLRUFPIndex::computeBucketId(uint8_t *fp) {
      uint32_t hash;
      MurmurHash3_x86_32(fp, Config::getInstance().getFingerprintLength(), 2, &hash);
      return hash % nBuckets_;
    }
}

