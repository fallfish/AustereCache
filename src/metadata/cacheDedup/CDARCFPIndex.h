//
// Created by 王秋平 on 12/3/19.
//

#ifndef SSDDUP_CDARCFPINDEX_H
#define SSDDUP_CDARCFPINDEX_H
#include "cacheDedupCommon.h"
namespace cache {
    class CDARCFPIndex {
    public:
        struct DP {
            uint32_t weuId_;
            uint32_t offset_;
            uint32_t len_;
        };

        CDARCFPIndex();

        static CDARCFPIndex &getInstance();

        void init();
        bool lookup(uint8_t *fp, uint32_t &weuId, uint32_t &offset, uint32_t &len);
        void reference(uint64_t lba, uint8_t *fp);
        void dereference(uint64_t lba, uint8_t *fp);
        uint32_t update(uint64_t lba, uint8_t *fp, uint32_t &weuId,
                        uint32_t &offset, uint32_t length);
        void dumpStats();
        uint32_t capacity_;

    private:
        std::map<Fingerprint, DP> mp_;
        std::map<uint32_t, uint32_t> weuReferenceCount_; // reference count for each weu
        std::list<uint32_t> zeroReferenceList_; // weu_ids
        WEUAllocator weuAllocator_;
    };
}
#endif //SSDDUP_CDARCFPINDEX_H
