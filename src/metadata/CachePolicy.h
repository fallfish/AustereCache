#ifndef __CACHE_POLICY_H__
#define __CACHE_POLICY_H__

#include <memory>
#include <list>
#include <vector>
#include "bucket.h"
namespace cache {
    class FPIndex;

    struct CachePolicyExecutor {
        explicit CachePolicyExecutor(Bucket *bucket);

        virtual void promote(uint32_t slotId, uint32_t nSlotsToOccupy = 1) = 0;
        virtual uint32_t allocate(uint32_t nSlotsToOccupy = 1) = 0;
        virtual void clearObsolete(std::shared_ptr<FPIndex> fpIndex) = 0;

        Bucket *bucket_;
    };

    struct BucketAwareLRUExecutor : public CachePolicyExecutor {
        explicit BucketAwareLRUExecutor(Bucket *bucket);

        void promote(uint32_t slotId, uint32_t nSlotsToOccupy) override;
        // Only LBA Index would call this function
        // LBA signature only takes one slot.
        // So there is no need to care about the entry may take contiguous slots.
        void clearObsolete(std::shared_ptr<FPIndex> fpIndex) override;
        uint32_t allocate(uint32_t nSlotsToOccupy) override;
    };

    struct CAClockExecutor : public CachePolicyExecutor {
        CAClockExecutor(Bucket *bucket, std::shared_ptr<Bucket> clock, uint32_t *clockPtr);
        ~CAClockExecutor();

        void promote(uint32_t slotId, uint32_t nSlotsOccupied) override;
        void clearObsolete(std::shared_ptr<FPIndex> fpIndex) override;
        uint32_t allocate(uint32_t nSlotsToOccupy) override;

        inline void initClock(uint32_t index);
        inline uint32_t getClock(uint32_t index);
        inline void incClock(uint32_t index);
        inline void decClock(uint32_t index);

        std::shared_ptr<Bucket> clock_;
        uint32_t *clockPtr_;
    };

    struct ThresholdRCClockExecutor : public CAClockExecutor {
        ThresholdRCClockExecutor(
          Bucket *bucket, std::shared_ptr<Bucket> clock,
          uint32_t *clockPtr, uint32_t threshold);

        void promote(uint32_t slotId, uint32_t nSlotsOccupied) override;
        uint32_t allocate(uint32_t nSlotsToOccupy) override;

        uint32_t threshold_;
    };

    struct LeastReferenceCountExecutor : public CachePolicyExecutor {
        explicit LeastReferenceCountExecutor(Bucket *bucket);

        void promote(uint32_t slotId, uint32_t nSlotsToOccupy) override;
        // Only LBA Index would call this function
        // LBA signature only takes one slot.
        // So there is no need to care about the entry may take contiguous slots.
        void clearObsolete(std::shared_ptr<FPIndex> fpIndex) override;
        uint32_t allocate(uint32_t nSlotsToOccupy) override;
    };


    class CachePolicy {
    public:
        virtual std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) = 0;

        CachePolicy();
    };

    class BucketAwareLRU : public CachePolicy {
    public:
        BucketAwareLRU();

        std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) override;
    };

    /**
     * @brief CAClock - Compression-Aware Clock Cache Eviction Algorithm
     *        The allocation should take care of compressibility of each
     *        new item.
     */
    class CAClock : public CachePolicy {
    public:
        CAClock(uint32_t nSlotsPerBucket, uint32_t nBuckets);

        std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) override;
        std::shared_ptr<Bucket> getBucket(uint32_t bucketId);


        std::unique_ptr<uint8_t []> clock_;
        uint32_t nSlotsPerBucket_;
        uint32_t nBytesPerBucket_;
        uint32_t clockPtr_;
    };

    class LeastReferenceCount : public CachePolicy {
    public:
        LeastReferenceCount();

        std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) override;
    };

    class ThresholdRCClock : public CAClock {
    public:
        ThresholdRCClock(
          uint32_t nSlotsPerBucket,
          uint32_t nBuckets, uint32_t threshold);

        std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) override;

        uint32_t threshold_;
    };

    class LRUExecutor : public CachePolicyExecutor {
      public:
        explicit LRUExecutor(Bucket *bucket, std::list<uint32_t> *list);

        std::list<uint32_t> *list_;

        uint32_t allocate(uint32_t nSlotsToOccupy);
        void clearObsolete(std::shared_ptr<FPIndex> fpIndex);

        void promote(uint32_t slotId, uint32_t nSlotsToOccupy);
    };

    // LRU policy that uses a list to maintain
    class LRU : public CachePolicy {
        LRU();
        std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) override;
        std::unique_ptr<std::list<uint32_t> []> lists_;
    };
}
#endif
