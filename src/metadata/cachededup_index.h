/* File: metadata/cachededup_index.h
 * Description:
 *   This file contains declarations of three deduplication-aware cache eviction 
 *   algorithms introduced in CacheDedup, FAST '16.
 *   1. D-LRU algorithm (class DLRU_SourceIndex, class DLRU_FingerprintIndex)
 *   2. D-ARC algorithm (class DARC_SourceIndex, class DARC_FingerprintIndex)
 *   3. CD-ARC algorithm (class CDARC_FingerprintIndex)
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
#include "common/config.h"

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
    void recycle(uint64_t location) {
      freeLocation_ = location;
    }
    uint64_t allocate() {
      uint64_t allocatedLocation = -1;
      if (nextLocation_ != capacity_) {
        allocatedLocation = nextLocation_;
        nextLocation_ += chunkSize_;
      } else {
        allocatedLocation = freeLocation_;
      }
      return allocatedLocation;
    }
  };

  class DLRU_SourceIndex {
    public:
      struct FP {
        FP() { memset(v_, 0, 20); }
        uint8_t v_[20];
        std::list<uint64_t>::iterator it_;
      };

      DLRU_SourceIndex();
      static DLRU_SourceIndex& getInstance();
      void init(uint32_t capacity);

      bool lookup(uint64_t lba, uint8_t *fp);
      void promote(uint64_t lba);
      void update(uint64_t lba, uint8_t *fp);

    private:
      static DLRU_SourceIndex instance;
      uint32_t capacity_;
      std::map<uint64_t, FP> mp_; // mapping from lba to ca and list iter
      std::list<uint64_t> list_;
  };

  /*
   * This is a deduplication-only design, so the address allocation to the
   * data cache is bound to be aligned with chunk size.
   * When the cache is not full, we use a self-incremental counter to maintain
   * the free addresses; if the cache is full, the evicted address will
   * be a new free address and it will be allocated to the new entry immediately.
   * Note: for a write cache, there must be a temporary location to accomodate
   * those dirty data, so overprovisioning is needed, and we cannot allocate newly
   * evicted ssd location to newly inserted data, otherwise, we have to copy
   * data to another position.
   */
  class DLRU_FingerprintIndex {
    public:
      struct FP {
        FP() { memset(v_, 0, 20); }
        uint8_t v_[20];
        bool operator<(const FP &fp) const {
          return memcmp(v_, fp.v_, 20) < 0;
        }
      };
      struct DP {
        uint64_t cachedataLocation_;
        std::list<FP>::iterator it_;
      };

      DLRU_FingerprintIndex();
      static DLRU_FingerprintIndex& getInstance();
      void init(uint32_t cap);


      bool lookup(uint8_t *fp, uint64_t &cachedataLocation);
      void promote(uint8_t *fp);
      void update(uint8_t *fp, uint64_t &cachedataLocation);
    private:
      static DLRU_FingerprintIndex instance;
      uint32_t capacity_;
      std::map<FP, DP> mp_; // mapping from FP to list
      std::list<FP> list_;
      SpaceAllocator spaceAllocator_;
  };

  class DARC_SourceIndex {
    public:
      friend class DARC_FingerprintIndex;
      
      struct FP {
        FP() {
          memset(v_, 0, 20);
          listId_ = 0;
        }

        uint8_t v_[20];
        /**
         * list_id: 
         */
        uint8_t listId_;
        std::list<uint64_t>::iterator it_;
      };
      DARC_SourceIndex();
      static DARC_SourceIndex& getInstance();
      void init(uint32_t cap, uint32_t p, uint32_t x);

      bool lookup(uint64_t lba, uint8_t *ca);
      void adjust_adaptive_factor(uint64_t lba);
      void promote(uint64_t lba);
      void update(uint64_t lba, uint8_t *ca);
      void check_metadata_cache(uint64_t lba);
      void manage_metadata_cache(uint64_t lba);
      void replace_in_metadata_cache(uint64_t lba);

      /**
       * Check if the list id is correct
       */
      void check_list_id_consistency() {
        std::vector<uint64_t> lbas2;
        for (auto lba : t1_) {
          if (mp_[lba].listId_ != 0) {
            lbas2.push_back(lba);
          }
        }
        for (auto lba : t2_) {
          if (mp_[lba].listId_ != 1) {
            std::cout << "Should be in " << (int)mp_[lba].listId_ << " while in t2" << std::endl;
            lbas2.push_back(lba);
          }
        }
        if (lbas2.size() != 0) {
          for (auto lba : lbas2) {
            std::cout << (int)mp_[lba].listId_ << std::endl;
          }
          assert(0);
        }
      }

      /**
       * Check if there is no reference (HIT) of CA in T_1 and T_2
       */
      void check_zero_reference(uint8_t *ca) {
        std::vector<uint64_t> lbas;
        std::vector<uint64_t> lbas2;
        for (auto lba : t1_) {
          if (memcmp(mp_[lba].v_, ca, Config::getInstance()->getFingerprintLength()) == 0) {
            lbas.push_back(lba);
          }
          if (mp_[lba].listId_ != 0) {
            lbas2.push_back(lba);
          }
        }
        for (auto lba : t2_) {
          if (memcmp(mp_[lba].v_, ca, Config::getInstance()->getFingerprintLength()) == 0) {
            lbas.push_back(lba);
          }
          if (mp_[lba].listId_ != 1) {
            std::cout << "Should be in " << (int)mp_[lba].listId_ << " while in t2" << std::endl;
            lbas2.push_back(lba);
          }
        }
        assert(lbas.size() == 0);
        if (lbas2.size() != 0) {
          for (auto lba : lbas2) {
            std::cout << (int)mp_[lba].listId_ << std::endl;
          }
        }
        assert(lbas2.size() == 0);
      }
    private:
      static DARC_SourceIndex instance;
      std::map<uint64_t, FP> mp_;
      std::list<uint64_t> t1_, t2_, b1_, b2_, b3_;
      uint32_t capacity_, p_, x_;
  };

  class DARC_FingerprintIndex {
    public:
      friend class DARC_SourceIndex;


      struct FP {
        FP() { memset(v_, 0, sizeof(v_)); }
        uint8_t v_[20];
        bool operator<(const FP &fp) const {
          return memcmp(v_, fp.v_, 20) < 0;
        }
        bool operator==(const FP &fp) const {
          for (uint32_t i = 0; i < 20 / 4; ++i) {
            if (((uint32_t*)v_)[i] != ((uint32_t*)fp.v_)[i])
              return false;
          }
          return true;
        }
      };
      struct DP {
        uint64_t cachedataLocation_;
        uint32_t referenceCount_;
        std::list<FP>::iterator zeroReferenceListIt_;
      };

      DARC_FingerprintIndex();
      static DARC_FingerprintIndex &getInstance();
      void init(uint32_t cap);
      bool lookup(uint8_t *fp, uint64_t &cachedataLocation);
      void reference(uint64_t lba, uint8_t *fp);
      void deference(uint64_t lba, uint8_t *fp);
      void update(uint64_t lba, uint8_t *fp, uint64_t &cachedataLocation);
    private:
      static DARC_FingerprintIndex instance;
      std::map<FP, DP> mp_;
      std::list<FP> zeroReferenceList_;
      SpaceAllocator spaceAllocator_;
      uint32_t capacity_;
  };
 
  struct WEUAllocator {
    uint32_t weuId_ = 0;
    uint32_t weuSize_;
    uint32_t currentOffset;
    
    WEUAllocator() {}

    void init()
    {
      // 2 MiB weu size
      weuSize_ = Config::getInstance()->getWriteBufferSize();
      currentOffset = 0;
    }
    
    std::set<uint32_t> evictedWEUIds;

    void clearEvictedWEUIds() {
      evictedWEUIds.clear();
    }

    bool hasRecycled(uint32_t weuId) {
      return evictedWEUIds.find(weuId) != evictedWEUIds.end();
    }

    inline void recycle(uint32_t weuId) {
      evictedWEUIds.insert(weuId);
    }

    inline bool isCurrentWEUFull(uint32_t length) {
      return (length + currentOffset > weuSize_);
    }

    void allocate(uint32_t &weuId, uint32_t &offset, uint32_t length)
    {
      if (length + currentOffset > weuSize_)
      {
        weuId_ += 1;
        currentOffset = 0;
      }
      weuId = weuId_;
      offset = currentOffset;
      currentOffset += length;
    }
  };

  class CDARC_FingerprintIndex {
    public:
      friend class DARC_SourceIndex;

      struct FP {
        FP() { memset(v_, 0, sizeof(v_)); }
        uint8_t v_[20];
        bool operator<(const FP &fp) const {
          return memcmp(v_, fp.v_, 20) < 0;
        }
        bool operator==(const FP &fp) const {
          for (uint32_t i = 0; i < 20 / 4; ++i) {
            if (((uint32_t*)v_)[i] != ((uint32_t*)fp.v_)[i])
              return false;
          }
          return true;
        }
      };
      struct DP {
        uint32_t weuId_;
        uint32_t offset_;
        uint32_t len_;
      };

      CDARC_FingerprintIndex();
      static CDARC_FingerprintIndex &getInstance();
      void init(uint32_t cap);
      bool lookup(uint8_t *fp, uint32_t &weuId, uint32_t &offset, uint32_t &len);
      void reference(uint64_t lba, uint8_t *fp);
      void deference(uint64_t lba, uint8_t *fp);
      uint32_t update(uint64_t lba, uint8_t *fp, uint32_t &weuId,
          uint32_t &offset, uint32_t length);
    private:
      static CDARC_FingerprintIndex instance;
      std::map<FP, DP> mp_;
      std::map<uint32_t, uint32_t> weuReferenceCount_; // reference count for each weu
      std::list<uint32_t> zeroReferenceList_; // weu_ids
      WEUAllocator weuAllocator_;
      uint32_t capacity_;
  };
}
#endif
