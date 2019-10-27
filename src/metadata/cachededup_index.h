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
#include <utils/MurmurHash3.h>
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
    void recycle(uint64_t location) {
      freeLocation_ = location;
    }
    uint64_t allocate() {
      uint64_t allocatedLocation;
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
        uint8_t v_[20]{};
        std::list<uint64_t>::iterator it_;
      };

      DLRU_SourceIndex();
      explicit DLRU_SourceIndex(uint32_t capacity);
      static DLRU_SourceIndex& getInstance();
      void init();

      bool lookup(uint64_t lba, uint8_t *fp);
      void promote(uint64_t lba);
      bool update(uint64_t lba, uint8_t *fp, uint8_t *oldFP);

    void check_no_reference_to_fp(uint8_t *fp) {
      for (auto pr : mp_) {
        if (memcmp(fp, pr.second.v_, Config::getInstance().getFingerprintLength()) == 0) {
          assert(0);
        }
      }
    }

      uint32_t capacity_;
  private:
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
        uint8_t v_[20]{};
        bool operator<(const FP &fp) const {
          return memcmp(v_, fp.v_, 20) < 0;
        }
        bool operator==(const FP &fp) const {
          return memcmp(v_, fp.v_, 20) == 0;
        }
      };
      struct DP {
        uint64_t cachedataLocation_{};
        uint32_t referenceCount_{};
        std::list<FP>::iterator it_;
        std::list<FP>::iterator zeroReferenceListIter_;
      };

      DLRU_FingerprintIndex();
      explicit DLRU_FingerprintIndex(uint32_t capacity);
      static DLRU_FingerprintIndex& getInstance();
      void init();

      void reference(uint8_t *fp) {
        FP _fp;
        memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
        if (mp_.find(_fp) == mp_.end()) {
          return ;
        }
        mp_[_fp].referenceCount_ += 1;
        if (mp_[_fp].referenceCount_ == 1) {
          if (mp_[_fp].zeroReferenceListIter_ != zeroReferenceList_.end()) {
            zeroReferenceList_.erase(mp_[_fp].zeroReferenceListIter_);
            mp_[_fp].zeroReferenceListIter_ = zeroReferenceList_.end();
          }
        }
      }

      void dereference(uint8_t *fp) {
        FP _fp;
        memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
        if (mp_.find(_fp) == mp_.end()) {
          return ;
        }
        if (mp_[_fp].referenceCount_ != 0) {
          mp_[_fp].referenceCount_ -= 1;
          if (mp_[_fp].referenceCount_ == 0) {
            zeroReferenceList_.push_front(_fp);
            mp_[_fp].zeroReferenceListIter_ = zeroReferenceList_.begin();
          }
        }
      }


      bool lookup(uint8_t *fp, uint64_t &cachedataLocation);
      void promote(uint8_t *fp);
      void update(uint8_t *fp, uint64_t &cachedataLocation);

      uint32_t capacity_;
  private:
      std::map<FP, DP> mp_; // mapping from FP to list
      std::list<FP> list_;
      std::list<FP> zeroReferenceList_;
      SpaceAllocator spaceAllocator_;
  };

  class DARC_SourceIndex {
    public:
      friend class DARC_FingerprintIndex;

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
      DARC_SourceIndex();
      static DARC_SourceIndex& getInstance();
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
      void check_list_id_consistency() {
        std::vector<uint64_t> lbas2;
        for (auto lba : t1_) {
          if (mp_[lba].listId_ != IN_T1) {
            lbas2.push_back(lba);
          }
        }
        int t = 0;
        for (auto lba : t2_) {
          ++t;
          if (mp_[lba].listId_ != IN_T2) {
            std::cout << "index " << t << " " << lba << " " << "Should be in " << (int)mp_[lba].listId_ << " while in t2" << std::endl;
            lbas2.push_back(lba);
          }
        }
        if (!lbas2.empty()) {
          for (auto lba : lbas2) {
            std::cout << lba << " " << (int)mp_[lba].listId_ << std::endl;
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
          if (memcmp(mp_[lba].v_, ca, Config::getInstance().getFingerprintLength()) == 0) {
            lbas.push_back(lba);
          }
          if (mp_[lba].listId_ != IN_T1) {
            lbas2.push_back(lba);
          }
        }
        for (auto lba : t2_) {
          if (memcmp(mp_[lba].v_, ca, Config::getInstance().getFingerprintLength()) == 0) {
            lbas.push_back(lba);
          }
          if (mp_[lba].listId_ != IN_T2) {
            std::cout << "Should be in " << (int)mp_[lba].listId_ << " while in t2" << std::endl;
            lbas2.push_back(lba);
          }
        }
        assert(lbas.empty());
        if (!lbas2.empty()) {
          for (auto lba : lbas2) {
            std::cout << (int)mp_[lba].listId_ << std::endl;
          }
        }
        assert(lbas2.empty());
      }

      uint32_t capacity_;
  public:
      std::map<uint64_t, FP> mp_;
      std::list<uint64_t> t1_, t2_, b1_, b2_, b3_;
      uint32_t p_, x_;

      void moveFromAToB(EntryLocation from, EntryLocation to);
  };

  class DARC_FingerprintIndex {
    public:
      friend class DARC_SourceIndex;

      struct FP {
        FP() { memset(v_, 0, sizeof(v_)); }
        uint8_t v_[20]{};
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
        uint64_t cachedataLocation_{};
        uint32_t referenceCount_{};
        std::list<FP>::iterator zeroReferenceListIt_;
        std::list<FP>::iterator it_;
      };

      DARC_FingerprintIndex();
      static DARC_FingerprintIndex &getInstance();
      void init();
      void promote(uint8_t *fp);
      bool lookup(uint8_t *fp, uint64_t &cachedataLocation);
      void reference(uint64_t lba, uint8_t *fp);
      void dereference(uint64_t lba, uint8_t *fp);
      void update(uint64_t lba, uint8_t *fp, uint64_t &cachedataLocation);

      uint32_t capacity_{};
  private:
      std::map<FP, DP> mp_;
      std::list<FP> zeroReferenceList_;
      std::list<FP> list_;
      SpaceAllocator spaceAllocator_{};
  };
 
  struct WEUAllocator {
    uint32_t weuId_ = 0;
    uint32_t weuSize_{};
    uint32_t currentOffset{};
    
    WEUAllocator() = default;

    void init()
    {
      // 2 MiB weu size
      weuSize_ = Config::getInstance().getWriteBufferSize();
      currentOffset = 0;
    }
    
    std::set<uint32_t> evictedWEUIds;

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
        uint8_t v_[20]{};
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
      void init();
      bool lookup(uint8_t *fp, uint32_t &weuId, uint32_t &offset, uint32_t &len);
      void reference(uint64_t lba, uint8_t *fp);
      void dereference(uint64_t lba, uint8_t *fp);
      uint32_t update(uint64_t lba, uint8_t *fp, uint32_t &weuId,
          uint32_t &offset, uint32_t length);

      uint32_t capacity_;
  private:
      std::map<FP, DP> mp_;
      std::map<uint32_t, uint32_t> weuReferenceCount_; // reference count for each weu
      std::list<uint32_t> zeroReferenceList_; // weu_ids
      WEUAllocator weuAllocator_;
  };


  class BucketizedDLRU_SourceIndex {
  public:
      BucketizedDLRU_SourceIndex() {
        nSlotsPerBucket_ = Config::getInstance().getnLBASlotsPerBucket();
        nBuckets_ = Config::getInstance().getnLBABuckets();
        buckets_ = new DLRU_SourceIndex*[nBuckets_];
        std::cout << "Number of LBA buckets: " << nBuckets_ << std::endl;
        for (int i = 0; i < nBuckets_; ++i) {
          buckets_[i] = new DLRU_SourceIndex(nSlotsPerBucket_);
        }
      }

      void promote(uint64_t lba) {
        buckets_[computeBucketId(lba)]->promote(lba);
      }
      bool update(uint64_t lba, uint8_t *fp, uint8_t *oldFP) {
        return buckets_[computeBucketId(lba)]->update(lba, fp, oldFP);
      }

      uint32_t computeBucketId(uint64_t lba) {
        uint32_t hash;
        MurmurHash3_x86_32(reinterpret_cast<const void *>(&lba), 8, 1, &hash);
        return hash % nBuckets_;
      }

      uint32_t nSlotsPerBucket_;
      uint32_t nBuckets_;
      DLRU_SourceIndex **buckets_;
      static BucketizedDLRU_SourceIndex& getInstance() {
        static BucketizedDLRU_SourceIndex instance;
        return instance;
      }

      bool lookup(uint64_t lba, uint8_t *fp) {
        return buckets_[computeBucketId(lba)]->lookup(lba, fp);
      }
  };

    class BucketizedDLRU_FingerprintIndex {
    public:
        BucketizedDLRU_FingerprintIndex() {
          nSlotsPerBucket_ = Config::getInstance().getnFPSlotsPerBucket() / 4;
          nBuckets_ = Config::getInstance().getnFPBuckets();
          buckets_ = new DLRU_FingerprintIndex*[nBuckets_];
          std::cout << "Number of FP buckets: " << nBuckets_ << std::endl;
          for (int i = 0; i < nBuckets_; ++i) {
            buckets_[i] = new DLRU_FingerprintIndex(nSlotsPerBucket_);
          }
        }

        static BucketizedDLRU_FingerprintIndex getInstance() {
          static BucketizedDLRU_FingerprintIndex instance;
          return instance;
        }

        bool lookup(uint8_t *fp, uint64_t &cachedataLocation) {
          return buckets_[computeBucketId(fp)]->lookup(fp, cachedataLocation);
        }
        void promote(uint8_t *fp) {
          buckets_[computeBucketId(fp)]->promote(fp);
        }
        void update(uint8_t *fp, uint64_t &cachedataLocation) {
          buckets_[computeBucketId(fp)]->update(fp, cachedataLocation);
        }

        void reference(uint8_t *fp) {
          buckets_[computeBucketId(fp)]->reference(fp);
        }
        void dereference(uint8_t *fp) {
          buckets_[computeBucketId(fp)]->dereference(fp);
        }

        uint32_t computeBucketId(uint8_t *fp) {
          uint32_t hash;
          MurmurHash3_x86_32(fp, Config::getInstance().getFingerprintLength(), 2, &hash);
          return hash % nBuckets_;
        }

        uint32_t nSlotsPerBucket_;
        uint32_t nBuckets_;
        DLRU_FingerprintIndex **buckets_;
    };
}
#endif
