//
//

#ifndef SSDDUP_BUCKETAWARELRU_H
#define SSDDUP_BUCKETAWARELRU_H

#include "CachePolicy.h"

namespace cache {
    struct BucketAwareLRUExecutor : public CachePolicyExecutor {
        explicit BucketAwareLRUExecutor(Bucket *bucket);

        void promote(uint32_t slotId, uint32_t nSlotsToOccupy) override;

        // Only LBA Index would call this function
        // LBA signature only takes one slot.
        // So there is no need to care about the entry may take contiguous slots.
        void clearObsolete(std::shared_ptr<FPIndex> fpIndex) override;

        uint32_t allocate(uint32_t nSlotsToOccupy) override;
    };

    class BucketAwareLRU : public CachePolicy {
    public:
        BucketAwareLRU();

        CachePolicyExecutor* getExecutor(Bucket *bucket) override;
    };
}

#endif //SSDDUP_BUCKETAWARELRU_H
