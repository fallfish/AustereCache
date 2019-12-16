//
//

#ifndef SSDDUP_LEASTREFERENCECOUNT_H
#define SSDDUP_LEASTREFERENCECOUNT_H
#include "CachePolicy.h"
namespace cache {
    struct LeastReferenceCountExecutor : public CachePolicyExecutor {
        explicit LeastReferenceCountExecutor(Bucket *bucket);

        void promote(uint32_t slotId, uint32_t nSlotsToOccupy) override;
        // Only LBA Index would call this function
        // LBA signature only takes one slot.
        // So there is no need to care about the entry may take contiguous slots.
        void clearObsolete(std::shared_ptr<FPIndex> fpIndex) override;
        uint32_t allocate(uint32_t nSlotsToOccupy) override;
    };


    class LeastReferenceCount : public CachePolicy {
    public:
        LeastReferenceCount();

        CachePolicyExecutor* getExecutor(Bucket *bucket) override;
    };
}

#endif //SSDDUP_LEASTREFERENCECOUNT_H
