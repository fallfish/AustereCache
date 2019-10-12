#ifndef __CACHE_POLICY_H__
#define __CACHE_POLICY_H__

#include <memory>
#include "bucket.h"
namespace cache {
  class FPIndex;

  struct CachePolicyExecutor {
    CachePolicyExecutor(Bucket *bucket);

    virtual void promote(uint32_t slotId, uint32_t nSlotsToOccupy = 1) = 0;
    virtual uint32_t allocate(uint32_t nSlotsToOccupy = 1) = 0;
    virtual void clearObsolete(std::shared_ptr<FPIndex> fpIndex) = 0;

    Bucket *_bucket;
  };

  struct LRUExecutor : public CachePolicyExecutor {
    LRUExecutor(Bucket *bucket);

    void promote(uint32_t slotId, uint32_t nSlotsToOccupy = 1);
    // Only LBA Index would call this function
    // LBA signature only takes one slot.
    // So there is no need to care about the entry may take contiguous slots.
    void clearObsolete(std::shared_ptr<FPIndex> fpIndex);
    uint32_t allocate(uint32_t nSlotsToOccupy = 1);
  };

  struct CAClockExecutor : public CachePolicyExecutor {
    CAClockExecutor(Bucket *bucket, std::shared_ptr<Bucket> clock, uint32_t *clockPtr);
    ~CAClockExecutor();

    void promote(uint32_t slotId, uint32_t nSlotsToOccupy = 1);
    void clearObsolete(std::shared_ptr<FPIndex> fpIndex);
    uint32_t allocate(uint32_t nSlotsToOccupy = 1);

    inline void initClock(uint32_t index);
    inline uint32_t getClock(uint32_t index);
    inline void incClock(uint32_t index);
    inline void decClock(uint32_t index);

    std::shared_ptr<Bucket> clock_;
    uint32_t *clockPtr_;
  };


  class CachePolicy {
    public:
      virtual std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) = 0;

      CachePolicy();
  };

  class LRU : public CachePolicy {
    public:
      LRU();
        
      std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket);
  };

  /**
   * @brief CAClock - Compression-Aware Clock Cache Eviction Algorithm
   *        The allocation should take care of compressibility of each
   *        new item.
   */
  class CAClock : public CachePolicy {
    public:
      CAClock(uint32_t nSlotsPerBucket, uint32_t nBuckets);

      std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket);

      std::shared_ptr<Bucket> getBucket(uint32_t bucketId) {
        return std::make_shared<Bucket>(
            0, 2, nSlotsPerBucket_,
            clock_.get() + nBytesPerBucket_ * bucketId,
            nullptr,
            nullptr, bucketId);
      }


      std::unique_ptr<uint8_t []> clock_;
      uint32_t nSlotsPerBucket_;
      uint32_t nBytesPerBucket_;
      uint32_t clockPtr_;
  };
}
#endif
