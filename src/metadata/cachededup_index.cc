#include "cachededup_index.h"
#include "common/config.h"
#include "common/stats.h"
#include "manage/dirty_list.h"

#include <iostream>
#include <cassert>
#include <csignal>

namespace cache {
  DLRU_SourceIndex::DLRU_SourceIndex() {}
  DLRU_SourceIndex::DLRU_SourceIndex(uint32_t capacity) {
    capacity_ = capacity;
  }

  DLRU_SourceIndex& DLRU_SourceIndex::getInstance()
  {
    static DLRU_SourceIndex instance;
    return instance;
  }

  void DLRU_SourceIndex::init()
  {
    capacity_ = Config::getInstance().getnSourceIndexEntries();
  }

  /**
   * (Comment by jhli) Check whether the address/index is stored in the cache
   */
  bool DLRU_SourceIndex::lookup(uint64_t lba, uint8_t *fp) {
    auto it = mp_.find(lba);
    if (it == mp_.end()) return false;
    else {
      memcpy(fp, it->second.v_, Config::getInstance().getFingerprintLength());
      return true;
    }
  }

  /**
   * (Comment by jhli) move the accessed index to the front
   */
  void DLRU_SourceIndex::promote(uint64_t lba) {
    auto it = mp_.find(lba)->second.it_;
    list_.erase(it);
    list_.push_front(lba);
    // renew the iterator stored in the mapping
    mp_[lba].it_ = list_.begin();
  }

  /**
   * (Comment by jhli) move 
   */
  bool DLRU_SourceIndex::update(uint64_t lba, uint8_t *fp, uint8_t *oldFP) {
    bool evicted = false;
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
    if (mp_.find(lba) != mp_.end()) {
      memcpy(oldFP, mp_[lba].v_, Config::getInstance().getFingerprintLength());
      evicted = true;

      list_.erase(mp_[lba].it_);
      list_.push_front(lba);
      _fp.it_ = list_.begin();
    } else {
      if (list_.size() == capacity_) {
        uint64_t lba_ = list_.back();
        list_.pop_back();

        memcpy(oldFP, mp_[lba_].v_, Config::getInstance().getFingerprintLength());
        evicted = true;

        mp_.erase(lba_);
        Stats::getInstance().add_lba_index_eviction_caused_by_capacity();
      }
      list_.push_front(lba);
      _fp.it_ = list_.begin();
    }
    mp_[lba] = _fp;

    return evicted;
  }


    DLRU_FingerprintIndex::DLRU_FingerprintIndex() {}
  DLRU_FingerprintIndex& DLRU_FingerprintIndex::getInstance()
  {
    static DLRU_FingerprintIndex instance;
    return instance;
  }

  void DLRU_FingerprintIndex::init()
  {
    capacity_ = Config::getInstance().getCacheDeviceSize() / Config::getInstance().getChunkSize();
    spaceAllocator_.capacity_ = capacity_ * Config::getInstance().getChunkSize();
    spaceAllocator_.nextLocation_ = 0;
    spaceAllocator_.chunkSize_ = Config::getInstance().getChunkSize();
  }

  bool DLRU_FingerprintIndex::lookup(uint8_t *fp, uint64_t &cachedataLocation)
  {
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
    auto it = mp_.find(_fp);

    if (it == mp_.end()) {
      return false;
    } else {
      cachedataLocation = it->second.cachedataLocation_;
      return true;
    }
  }

  void DLRU_FingerprintIndex::promote(uint8_t *fp)
  {
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
    auto it = mp_.find(_fp);
    assert(it != mp_.end());
    list_.erase(it->second.it_);
    list_.push_front(_fp);

    // renew the iterator stored in the mapping
    it->second.it_ = list_.begin();
  }

  void DLRU_FingerprintIndex::update(uint8_t *fp, uint64_t &cachedataLocation)
  {
    static uint32_t updateCount = 0;
    FP _fp;
    DP _dp;
    if (lookup(fp, _dp.cachedataLocation_)) {
      promote(fp);
      return ;
    }
    updateCount += 1;

    if (list_.size() == capacity_) {

      bool inRecencyList = true;
      bool find = false;
      // current cache is full, evict an old entry and
      // allocate its ssd location to the new one
      do {
        if (zeroReferenceList_.empty()) {
          _fp = list_.back();
          list_.pop_back();
          inRecencyList = true;
        } else {
          _fp = zeroReferenceList_.back();
          zeroReferenceList_.pop_back();
          inRecencyList = false;
        }
      } while (mp_.find(_fp) == mp_.end());
      // assign the evicted free ssd location to the newly inserted data
#if defined(WRITE_BACK_CACHE)
      DirtyList::getInstance().addEvictedChunk(mp_[_fp].cachedataLocation_,
          Config::getInstance().getChunkSize());
#endif
      if (inRecencyList) {
        if (mp_[_fp].zeroReferenceListIter_ != zeroReferenceList_.end()) {
          zeroReferenceList_.erase(mp_[_fp].zeroReferenceListIter_);
        }
      } else {
        list_.erase(mp_[_fp].it_);
      }
//#endif
      spaceAllocator_.recycle(mp_[_fp].cachedataLocation_);
      mp_.erase(_fp);
      Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
    }
    _dp.cachedataLocation_ = spaceAllocator_.allocate();

    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
    list_.push_front(_fp);
    _dp.it_ = list_.begin();
    _dp.zeroReferenceListIter_ = zeroReferenceList_.end();
    _dp.referenceCount_ = 1;

    mp_[_fp] = _dp;
    cachedataLocation = _dp.cachedataLocation_;
  }

  DLRU_FingerprintIndex::DLRU_FingerprintIndex(uint32_t capacity) {
    capacity_ = capacity;
    spaceAllocator_.capacity_ = capacity_ * Config::getInstance().getChunkSize();
    spaceAllocator_.nextLocation_ = 0;
    spaceAllocator_.chunkSize_ = Config::getInstance().getChunkSize();
  }

  DARC_SourceIndex::DARC_SourceIndex() {}

  DARC_SourceIndex& DARC_SourceIndex::getInstance()
  {
    static DARC_SourceIndex instance;
    return instance;
  }

  void DARC_SourceIndex::init(uint32_t p, uint32_t x)
  {
    capacity_ = Config::getInstance().getnSourceIndexEntries();
    p_ = p;
    x_ = x; // capacity_;
  }

  bool DARC_SourceIndex::lookup(uint64_t lba, uint8_t *ca)
  {
    auto it = mp_.find(lba);
    if (it == mp_.end()) {
      return false;
    } else {
      memcpy(ca, it->second.v_, Config::getInstance().getFingerprintLength());
      return true;
    }
  }

  void DARC_SourceIndex::adjust_adaptive_factor(uint64_t lba)
  {
    auto it = mp_.find(lba);
    if (it == mp_.end()) return ;
    if (it->second.listId_ == IN_B1) {
      if (p_ < capacity_) ++p_;
    } else if (it->second.listId_ == IN_B2) {
      if (p_ > 0) --p_;
    }
  }

  void DARC_SourceIndex::promote(uint64_t lba)
  {
  }

  void DARC_SourceIndex::moveFromAToB(EntryLocation from, EntryLocation to)
  {
    uint64_t lba = 0;
    if (from == IN_T1) {
      lba = t1_.back();
      t1_.pop_back();
    } else if (from == IN_T2) {
      lba = t2_.back();
      t2_.pop_back();
    } else if (from == IN_B1) {
      lba = b1_.back();
      b1_.pop_back();
    } else if (from == IN_B2) {
      lba = b2_.back();
      b2_.pop_back();
    } else if (from == IN_B3) {
      lba = b3_.back();
      b3_.pop_back();
    }

    auto it = mp_.find(lba);
    assert(it != mp_.end());
    if (from == IN_T1 || from == IN_T2) {
      // dec reference count
#ifdef CDARC
      CDARC_FingerprintIndex::getInstance().dereference(lba, it->second.v_);
#else
      DARC_FingerprintIndex::getInstance().dereference(lba, it->second.v_);
#endif
    }
    if (to == IN_T1 || to == IN_T2) {
#ifdef CDARC
      CDARC_FingerprintIndex::getInstance().reference(lba, it->second.v_);
#else
      DARC_FingerprintIndex::getInstance().reference(lba, it->second.v_);
#endif
    }

    it->second.listId_ = to;
    if (to == IN_T1) {
      t1_.push_front(lba);
      it->second.it_ = t1_.begin();
    } else if (to == IN_T2) {
      t2_.push_front(lba);
      it->second.it_ = t2_.begin();
    } else if (to == IN_B1) {
      b1_.push_front(lba);
      it->second.it_ = b1_.begin();
    } else if (to == IN_B2) {
      b2_.push_front(lba);
      it->second.it_ = b2_.begin();
    } else if (to == IN_B3) {
      b3_.push_front(lba);
      it->second.it_ = b3_.begin();
    }
  }

  void DARC_SourceIndex::update(uint64_t lba, uint8_t *fp)
  {
    EntryLocation result = INVALID;
    FP _fp;
    auto iter = mp_.find(lba); // lookup

    if (iter == mp_.end()) {
      result = INVALID;
    } else {
      result = iter->second.listId_;
    }

    if (result == IN_B1 || result == IN_B2) {
      check_metadata_cache(lba);
      iter = mp_.find(lba);
      if (iter == mp_.end()) {
        result = INVALID;
      } else {
        result = iter->second.listId_;
      }
    }

    enum {
      INSERT_TO_T1, INSERT_TO_T2
    } action;

    if (result == INVALID) {
      check_metadata_cache(lba);
      action = INSERT_TO_T1;
    } else {
      if (result == IN_T1) {
        t1_.erase(iter->second.it_);
        action = INSERT_TO_T2;
      } else if (result == IN_T2) {
        t2_.erase(iter->second.it_);
        action = INSERT_TO_T2;
      } else if (result == IN_B1) {
        b1_.erase(iter->second.it_);
        action = INSERT_TO_T2;
      } else if (result == IN_B2) {
        b2_.erase(iter->second.it_);
        action = INSERT_TO_T2;
      } else if (result == IN_B3) {
        b3_.erase(iter->second.it_);
        action = INSERT_TO_T1;
      }
      if (result == IN_T1 || result == IN_T2) {
#ifdef CDARC
        CDARC_FingerprintIndex::getInstance().dereference(lba, iter->second.v_);
#else
        DARC_FingerprintIndex::getInstance().dereference(lba, iter->second.v_);
#endif
      }
    }

    if (action == INSERT_TO_T1) // push to t1
    {
      t1_.push_front(lba);
      _fp.it_ = t1_.begin();
      _fp.listId_ = IN_T1;
    } else {
      t2_.push_front(lba);
      _fp.it_ = t2_.begin();
      _fp.listId_ = IN_T2;
    }

    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
    mp_[lba] = _fp;
    // add reference count
#ifdef CDARC
    CDARC_FingerprintIndex::getInstance().reference(lba, fp);
#else
    DARC_FingerprintIndex::getInstance().reference(lba, fp);
#endif
  }

  void DARC_SourceIndex::check_metadata_cache(uint64_t lba)
  {
    if (t1_.size() + t2_.size() + b3_.size() == capacity_ + x_) {
      if (b3_.empty()) {
        manage_metadata_cache(lba);
      }
      if (!b3_.empty()) {
        uint64_t lba_ = b3_.back();
        b3_.pop_back();
        mp_.erase(lba_);
      }
    }
  }

  /*
   * if |T1| + |B1| >= cap
   * -- if |T1| < cap, move LRU from B1 to B3, Do replace
   * -- else, move LRU from T1 to B1, then LRU from B1 to B3
   * else
   * -- if |B1| + |B2| > cap, move LRU from B2 to B3
   * -- Do replace
   */
  void DARC_SourceIndex::manage_metadata_cache(uint64_t lba)
  {
    uint64_t lba_ = 0;
    bool shouldReplace = true;
    bool moveToB3 = false;
    if (t1_.size() + b1_.size() >= capacity_) {
      if (t1_.size() >= capacity_) {
        moveFromAToB(IN_T1, IN_B1);
        shouldReplace = false;
      }
      // move LRU from b1 to b3
      moveFromAToB(IN_B1, IN_B3);
    } else {
      if (b1_.size() + b2_.size() >= capacity_) {
        // move LRU from b2 to b3
        moveFromAToB(IN_B2, IN_B3);
      }
    }

    if (shouldReplace) {
      replace_in_metadata_cache(lba);
    }
  }

  void DARC_SourceIndex::replace_in_metadata_cache(uint64_t lba)
  {
    uint64_t lba_;
    auto tmp_ = mp_.find(lba);
    if (!t1_.empty() &&
        (t1_.size() > p_ ||
         (t1_.size() == p_ && (mp_.find(lba) != mp_.end() && mp_.find(lba)->second.listId_ == IN_B2))
         || t2_.empty())
       ) {
      moveFromAToB(IN_T1, IN_B1);
    } else {
      moveFromAToB(IN_T2, IN_B2);
    }
  }

  DARC_FingerprintIndex::DARC_FingerprintIndex() = default;
  DARC_FingerprintIndex &DARC_FingerprintIndex::getInstance()
  {
    static DARC_FingerprintIndex instance;
    return instance;
  }
  void DARC_FingerprintIndex::init()
  {
    capacity_ = Config::getInstance().getCacheDeviceSize() / Config::getInstance().getChunkSize();
    spaceAllocator_.capacity_ = capacity_ * Config::getInstance().getChunkSize();
    spaceAllocator_.nextLocation_ = 0;
    spaceAllocator_.chunkSize_ = Config::getInstance().getChunkSize();
  }

  bool DARC_FingerprintIndex::lookup(uint8_t *fp, uint64_t &cachedataLocation) {
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());

    auto it = mp_.find(_fp);
    if (it == mp_.end()) {
      return false;
    } else {
      cachedataLocation = it->second.cachedataLocation_;
      return true;
    }
  }

  void DARC_FingerprintIndex::reference(uint64_t lba, uint8_t *fp)
  {
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());

    auto it = mp_.find(_fp);
    if (it == mp_.end()) {
      std::cout << "reference an empty entry" << std::endl;
      exit(0);
    }
    if ( (it->second.referenceCount_ += 1) == 1) {
      if (it->second.zeroReferenceListIt_ != zeroReferenceList_.end()) {
        zeroReferenceList_.erase(it->second.zeroReferenceListIt_);
        it->second.zeroReferenceListIt_ = zeroReferenceList_.end();
      }
    }
  }

  void DARC_FingerprintIndex::dereference(uint64_t lba, uint8_t *ca)
  {
    FP _fp;
    memcpy(_fp.v_, ca, Config::getInstance().getFingerprintLength());

    auto it = mp_.find(_fp);
    if (it == mp_.end()) {
      assert(0);
      std::cout << "dereference an empty entry" << std::endl;
      exit(0);
    }
    if ((it->second.referenceCount_ -= 1) == 0) {
      zeroReferenceList_.push_front(_fp);
      it->second.zeroReferenceListIt_ = zeroReferenceList_.begin();
      //DARC_SourceIndex::getInstance().check_zero_reference(_fp.v_);
    }
    // Deference a fingerprint index meaning a LBA index entry has been removed from T1 or T2
    Stats::getInstance().add_lba_index_eviction_caused_by_capacity();
  }

  void DARC_FingerprintIndex::update(uint64_t lba, uint8_t *fp, uint64_t &cachedataLocation)
  {
    FP _fp;
    DP _dp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
    if (mp_.find(_fp) != mp_.end()) {
      return ;
    }

    if (mp_.size() == capacity_) {
      // current cache is full, evict an old entry and
      // allocate its ssd location to the new one
      while (zeroReferenceList_.size() == 0) {
        DARC_SourceIndex::getInstance().manage_metadata_cache(lba);
      }
      _fp = zeroReferenceList_.back();
      zeroReferenceList_.pop_back();

      spaceAllocator_.recycle(mp_[_fp].cachedataLocation_);
#if defined(WRITE_BACK_CACHE)
      DirtyList::getInstance().addEvictedChunk(mp_[_fp].cachedataLocation_,
          Config::getInstance().getChunkSize());
#endif
      mp_.erase(_fp);

      memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
      Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
    }

    _dp.cachedataLocation_ = spaceAllocator_.allocate();
    _dp.referenceCount_ = 0;
    _dp.zeroReferenceListIt_ = zeroReferenceList_.end();

    mp_[_fp] = _dp;
    cachedataLocation = _dp.cachedataLocation_;
  }

  CDARC_FingerprintIndex::CDARC_FingerprintIndex() {
    capacity_ = Config::getInstance().getCacheDeviceSize() / Config::getInstance().getWriteBufferSize();
    weuAllocator_.init();
  }

  CDARC_FingerprintIndex &CDARC_FingerprintIndex::getInstance()
  {
    static CDARC_FingerprintIndex instance;
    return instance;
  }
  void CDARC_FingerprintIndex::init()
  {
    capacity_ = Config::getInstance().getCacheDeviceSize() / Config::getInstance().getWriteBufferSize();
    weuAllocator_.init();
  }

  // Need to carefully deal with stale entries after a WEU eviction
  // one WEU contains mixed "invalid" and "valid" LBAs_
  // for those valid LBAs_, we have no way but evict them in the following procedure
  // or a sweep and clean procedure.
  bool CDARC_FingerprintIndex::lookup(uint8_t *fp, uint32_t &weuId, uint32_t &offset, uint32_t &len) {
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());

    auto it = mp_.find(_fp);
    if (it == mp_.end()) {
      return false;
    } else {
      weuId = it->second.weuId_;
      if (weuAllocator_.hasRecycled(weuId)) {
        mp_.erase(it);
        return false;
      }
      offset = it->second.offset_;
      len = it->second.len_;
      return true;
    }
  }

  void CDARC_FingerprintIndex::reference(uint64_t lba, uint8_t *fp)
  {
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());

    auto it = mp_.find(_fp);
    if (it == mp_.end()) {
      std::cout << "reference an empty entry" << std::endl;
      exit(0);
    }

    uint32_t weu_id = it->second.weuId_;

    if (weuReferenceCount_.find(weu_id) == weuReferenceCount_.end()) {
      weuReferenceCount_[weu_id] = 0;
    }
    
    if ( (weuReferenceCount_[weu_id] += 1) == 1 ) {
      zeroReferenceList_.remove(weu_id);
    }
  }

  void CDARC_FingerprintIndex::dereference(uint64_t lba, uint8_t *fp)
  {
    FP _fp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());

    auto it = mp_.find(_fp);
    if (it == mp_.end()) {
      std::cout << "dereference an empty entry" << std::endl;
      exit(0);
    }

    uint32_t weuId = it->second.weuId_;
    if ( (weuReferenceCount_[weuId] -= 1) == 0 ) {
      zeroReferenceList_.push_front(weuId);
    }
    // Deference a fingerprint index meaning a LBA index entry has been removed from T1 or T2
    Stats::getInstance().add_lba_index_eviction_caused_by_capacity();
  }

  uint32_t CDARC_FingerprintIndex::update(
      uint64_t lba, uint8_t *fp, uint32_t &weuId,
      uint32_t &offset, uint32_t len)
  {
    uint32_t evicted_weu_id = ~0;
    FP _fp;
    DP _dp;
    memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
    if (mp_.find(_fp) != mp_.end()) {
      return evicted_weu_id;
    }

    if (weuAllocator_.isCurrentWEUFull(len) && weuReferenceCount_.size() == capacity_) {
      // current cache is full, evict an old entry and
      // allocate its ssd location to the new one
      while (zeroReferenceList_.size() == 0) {
        DARC_SourceIndex::getInstance().manage_metadata_cache(lba);
      }
      uint32_t weu_id = zeroReferenceList_.back();
      zeroReferenceList_.pop_back();
      weuReferenceCount_.erase(weu_id);

      weuAllocator_.recycle(weu_id);
      evicted_weu_id = weu_id;
      Stats::getInstance().add_fp_index_eviction_caused_by_capacity();
    }
    _dp.len_ = len;
    weuAllocator_.allocate(_dp.weuId_, _dp.offset_, _dp.len_);

    mp_[_fp] = _dp;
    weuId = _dp.weuId_;
    offset = _dp.offset_;
    
    return evicted_weu_id;
  }


}
