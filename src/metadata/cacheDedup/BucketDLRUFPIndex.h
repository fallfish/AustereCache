
#ifndef SSDDUP_BUCKETDLRUFPINDEX_H
#define SSDDUP_BUCKETDLRUFPINDEX_H

#include "cacheDedupCommon.h"
namespace cache {
    class CacheDedupFPBucket {
    public:
        CacheDedupFPBucket();
        explicit CacheDedupFPBucket(uint32_t capacity);

        uint32_t lookup(uint8_t *fp);
        void promote(uint32_t slotId);
        uint32_t update(uint8_t *fp);

        uint32_t capacity_{};
    private:
        std::vector<Fingerprint> keys_;
        std::vector<bool> valid_;
        std::list<uint32_t> list_;
    };

    class BucketizedDLRUFPIndex {
    public:
        BucketizedDLRUFPIndex();
        static BucketizedDLRUFPIndex getInstance();

        bool lookup(uint8_t *fp, uint64_t &cachedataLocation);

        void promote(uint8_t *fp);

        void update(uint8_t *fp, uint64_t &cachedataLocation);

        void reference(uint8_t *fp);

        void dereference(uint8_t *fp);

        uint32_t computeBucketId(uint8_t *fp);
        uint32_t nSlotsPerBucket_;
        uint32_t nBuckets_;
        CacheDedupFPBucket **buckets_;
    };
}
#endif //SSDDUP_BUCKETDLRUFPINDEX_H
