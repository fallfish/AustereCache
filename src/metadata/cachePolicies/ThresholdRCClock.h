//
// Created by 王秋平 on 12/4/19.
//

#ifndef SSDDUP_THRESHOLDRCCLOCK_H
#define SSDDUP_THRESHOLDRCCLOCK_H

#include "CAClock.h"

namespace cache {
    struct ThresholdRCClockExecutor : public CAClockExecutor {
        ThresholdRCClockExecutor(
          Bucket *bucket, std::shared_ptr<Bucket> clock,
          uint32_t *clockPtr, uint32_t threshold);

        void promote(uint32_t slotId, uint32_t nSlotsOccupied) override;
        uint32_t allocate(uint32_t nSlotsToOccupy) override;

        uint32_t threshold_;
    };


    class ThresholdRCClock : public CAClock {
    public:
        ThresholdRCClock(
          uint32_t nSlotsPerBucket,
          uint32_t nBuckets, uint32_t threshold);

        std::shared_ptr<CachePolicyExecutor> getExecutor(Bucket *bucket) override;

        uint32_t threshold_;
    };


}
class ThresholdRCClock {

};


#endif //SSDDUP_THRESHOLDRCCLOCK_H
