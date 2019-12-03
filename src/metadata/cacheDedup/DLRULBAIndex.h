//
// Created by 王秋平 on 12/3/19.
//

#ifndef SSDDUP_DLRULBAINDEX_H
#define SSDDUP_DLRULBAINDEX_H

#include "cacheDedupCommon.h"
namespace cache {
    class DLRULBAIndex {
    public:
        struct FP {
            FP() { memset(v_, 0, 20); }

            uint8_t v_[20]{};
            std::list<uint64_t>::iterator it_;
        };

        DLRULBAIndex();
        explicit DLRULBAIndex(uint32_t capacity);
        static DLRULBAIndex &getInstance();
        void init();
        bool lookup(uint64_t lba, uint8_t *fp);
        void promote(uint64_t lba);
        bool update(uint64_t lba, uint8_t *fp, uint8_t *oldFP);
        void check_no_reference_to_fp(uint8_t *fp);
        uint32_t capacity_;
    private:
        std::map <uint64_t, FP> mp_; // mapping from lba to ca and list iter
        std::list <uint64_t> list_;
    };

}
#endif //SSDDUP_DLRULBAINDEX_H
