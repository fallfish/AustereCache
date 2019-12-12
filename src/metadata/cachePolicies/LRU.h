//
// Created by 王秋平 on 12/4/19.
//

#ifndef SSDDUP_LRU_H
#define SSDDUP_LRU_H

#include "CachePolicy.h"
#include <list>
#include <map>
namespace cache {

    class LRUExecutor : public CachePolicyExecutor {
    public:
        explicit LRUExecutor(Bucket *bucket, std::list<uint32_t> *list,
            std::map<uint32_t, std::list<uint32_t>::iterator> *slotId2listPosition);

        std::list<uint32_t> *list_;
        std::map<uint32_t, std::list<uint32_t>::iterator> *slotId2listPosition_;

        uint32_t allocate(uint32_t nSlotsToOccupy);
        void clearObsolete(std::shared_ptr<FPIndex> fpIndex);

        void promote(uint32_t slotId, uint32_t nSlotsToOccupy);
    };

    // LRU policy that uses a list to maintain
    class LRU : public CachePolicy {
      public:
        LRU(uint32_t nBuckets);
        CachePolicyExecutor* getExecutor(Bucket *bucket) override;
        std::unique_ptr<std::list<uint32_t> []> lists_;
        std::unique_ptr<std::map<uint32_t, std::list<uint32_t>::iterator> []> slotId2listPosition_;
    };
}

#endif //SSDDUP_LRU_H
