//
// Created by 王秋平 on 12/4/19.
//

#ifndef SSDDUP_CACLOCK_H
#define SSDDUP_CACLOCK_H


#include "CachePolicy.h"
namespace cache {
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


        std::unique_ptr<uint8_t[]> clock_;
        uint32_t nSlotsPerBucket_;
        uint32_t nBytesPerBucket_;
        uint32_t clockPtr_;
    };
}


#endif //SSDDUP_CACLOCK_H
