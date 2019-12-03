//
// Created by 王秋平 on 12/3/19.
//

#ifndef SSDDUP_DARCLBAINDEX_H
#define SSDDUP_DARCLBAINDEX_H

#include "cacheDedupCommon.h"
namespace cache {
    class DARCLBAIndex {
    public:
        friend class DARCFPIndex;

        friend class CDARCFPIndex;

        enum EntryLocation {
            INVALID, IN_T1, IN_T2, IN_B1, IN_B2, IN_B3
        };

        struct FP {
            FP() {
              memset(v_, 0, 20);
              listId_ = INVALID;
            }

            uint8_t v_[20]{};
            /**
             * list_id:
             */
            EntryLocation listId_;
            std::list<uint64_t>::iterator it_;
        };

        DARCLBAIndex();

        static DARCLBAIndex &getInstance();

        void init(uint32_t p, uint32_t x);

        bool lookup(uint64_t lba, uint8_t *ca);

        void adjust_adaptive_factor(uint64_t lba);

        void promote(uint64_t lba);

        void update(uint64_t lba, uint8_t *fp);

        void check_metadata_cache(uint64_t lba);

        void manage_metadata_cache(uint64_t lba);

        void replace_in_metadata_cache(uint64_t lba);

        /**
         * Check if the list id is correct
         */
        void check_list_id_consistency();

        /**
         * Check if there is no reference (HIT) of CA in T_1 and T_2
         */
        void check_zero_reference(uint8_t *ca);

        void getFingerprints(std::set<Fingerprint> &v);

        std::map<uint64_t, FP> mp_;
        std::list<uint64_t> t1_, t2_, b1_, b2_, b3_;
        uint32_t p_, x_;
        uint32_t capacity_;

        void moveFromAToB(EntryLocation from, EntryLocation to);
    };
}

#endif //SSDDUP_DARCLBAINDEX_H
