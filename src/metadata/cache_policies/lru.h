//
//

#ifndef AUSTERECACHE_LRU_H
#define AUSTERECACHE_LRU_H

#include "cache_policy.h"
#include <list>
#include <map>
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
      public:
        LRU(uint32_t nBuckets);
        CachePolicyExecutor* getExecutor(Bucket *bucket) override;
        std::unique_ptr<std::list<uint32_t> []> lists_;
    };
}

#endif //AUSTERECACHE_LRU_H
