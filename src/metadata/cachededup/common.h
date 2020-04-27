/* File: metadata/cachededup_index.h
 * Description:
 *   This file contains declarations of three deduplication-aware cache eviction 
 *   algorithms introduced in CacheDedup, FAST '16.
 *   1. D-BucketAwareLRU algorithm (class DLRULBAIndex, class DLRUFPIndex)
 *   2. D-ARC algorithm (class DARCLBAIndex, class DARCFPIndex)
 *   3. CD-ARC algorithm (class CDARCFPIndex)
 *   CD-ARC incorporates WEU (Write-Evict Unit) structure designed in Nitro, ATC '14
 *   Note: all classes are singletonized.
 *
 *   To ease the implementation of SSD space allocation for FingerprintIndex,
 *   struct SpaceAllocator (WEUAllocator in CD-ARC) is used.
 */
#ifndef __CACHEDEDUP_INDEX__
#define __CACHEDEDUP_INDEX__
#include <iostream>
#include <cstring>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <cassert>
#include "utils/xxhash.h"
#include "common/config.h"
#include <csignal>

namespace cache {
    /*
     * SpaceAllocator allocates contiguous space until the capacity is reached.
     * After that, it allocates recycled space. Each allocation (after full)
     * will firstly trigger an eviction.
     */
    struct SpaceAllocator {
        uint64_t capacity_;
        uint64_t nextLocation_;
        uint64_t freeLocation_;
        uint64_t chunkSize_;
        void recycle(uint64_t location);
        uint64_t allocate();
    };

    struct WEUAllocator {
        uint32_t weuId_ = 0;
        uint32_t weuSize_{};
        uint32_t currentOffset{};

        WEUAllocator() = default;

        void init()
        {
          // 2 MiB weu size
          weuSize_ = Config::getInstance().getWeuSize();
          currentOffset = 0;
        }

        std::set<uint32_t> evictedWEUIds;

        bool hasRecycled(uint32_t weuId);
        void recycle(uint32_t weuId);
        bool isCurrentWEUFull(uint32_t length);
        void allocate(uint32_t &weuId, uint32_t &offset, uint32_t length);
    };
}
#endif
