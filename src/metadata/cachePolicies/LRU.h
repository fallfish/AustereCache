//
// Created by 王秋平 on 12/4/19.
//

#ifndef SSDDUP_LRU_H
#define SSDDUP_LRU_H

#include "CachePolicy.h"
#include <list>
namespace cache {

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

#endif //SSDDUP_LRU_H
