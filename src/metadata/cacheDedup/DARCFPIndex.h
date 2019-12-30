#ifndef SSDDUP_DARCFPINDEX_H
#define SSDDUP_DARCFPINDEX_H

#include "cacheDedupCommon.h"
namespace cache {
    class DARCFPIndex {
    public:
        struct DP {
            uint64_t cachedataLocation_{};
            uint32_t referenceCount_{}; std::list<Fingerprint>::iterator zeroReferenceListIt_;
            std::list<Fingerprint>::iterator it_;
        };

        DARCFPIndex();

        static DARCFPIndex &getInstance();

        void init();

        void promote(uint8_t *fp);

        bool lookup(uint8_t *fp, uint64_t &cachedataLocation);

        void reference(uint64_t lba, uint8_t *fp);

        void dereference(uint64_t lba, uint8_t *fp);

        void update(uint64_t lba, uint8_t *fp, uint64_t &cachedataLocation);

        uint32_t capacity_{};
        void dumpStats();
    private:
        std::map<Fingerprint, DP> mp_;
        std::list<Fingerprint> zeroReferenceList_;
        std::list<Fingerprint> list_;
        SpaceAllocator spaceAllocator_{};
    };
}
#endif //SSDDUP_DARCFPINDEX_H
