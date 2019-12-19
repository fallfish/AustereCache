
#ifndef SSDDUP_BUCKETDLRULBAINDEX_H
#define SSDDUP_BUCKETDLRULBAINDEX_H
#include "cacheDedupCommon.h"
namespace cache {
    class CacheDedupLBABucket {
    public:

        CacheDedupLBABucket();
        explicit CacheDedupLBABucket(uint32_t capacity);
        void init();
        uint32_t lookup(uint64_t lba, uint8_t *fp); // return slotId
        void promote(uint64_t lba);
        bool update(uint64_t lba, uint8_t *fp, uint8_t *oldFP);
        uint32_t capacity_{};
    private:
        std::vector<uint64_t> keys_;
        std::vector<Fingerprint> values_;
        std::vector<bool> valid_;
        std::list<uint64_t> list_;
    };
    class BucketizedDLRULBAIndex {
    public:
        BucketizedDLRULBAIndex();

        void promote(uint64_t lba);

        bool update(uint64_t lba, uint8_t *fp, uint8_t *oldFP);

        uint32_t computeBucketId(uint64_t lba);

        uint32_t nSlotsPerBucket_{};
        uint32_t nBuckets_{};
        CacheDedupLBABucket **buckets_{};

        static BucketizedDLRULBAIndex &getInstance() {
          static BucketizedDLRULBAIndex instance;
          return instance;
        }

        bool lookup(uint64_t lba, uint8_t *fp);
    };
}
#endif //SSDDUP_BUCKETDLRULBAINDEX_H
