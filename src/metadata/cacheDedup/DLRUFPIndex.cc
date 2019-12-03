#include <manage/DirtyList.h>
#include "DLRUFPIndex.h"

namespace cache {
    DLRUFPIndex::DLRUFPIndex() {}
    DLRUFPIndex& DLRUFPIndex::getInstance()
    {
      static DLRUFPIndex instance;
      return instance;
    }

    void DLRUFPIndex::init()
    {
      capacity_ = Config::getInstance().getCacheDeviceSize() / Config::getInstance().getChunkSize();
      spaceAllocator_.capacity_ = capacity_ * Config::getInstance().getChunkSize();
      spaceAllocator_.nextLocation_ = 0;
      spaceAllocator_.chunkSize_ = Config::getInstance().getChunkSize();
    }

    bool DLRUFPIndex::lookup(uint8_t *fp, uint64_t &cachedataLocation)
    {
      Fingerprint _fp;
      memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
      auto it = mp_.find(_fp);

      if (it == mp_.end()) {
        return false;
      } else {
        cachedataLocation = it->second.cachedataLocation_;
        return true;
      }
    }

    void DLRUFPIndex::promote(uint8_t *fp)
    {
      Fingerprint _fp;
      memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
      auto it = mp_.find(_fp);
      assert(it != mp_.end());
      list_.erase(it->second.it_);
      list_.push_front(_fp);

      // renew the iterator stored in the mapping
      it->second.it_ = list_.begin();
    }

    void DLRUFPIndex::update(uint8_t *fp, uint64_t &cachedataLocation)
    {
      static uint32_t updateCount = 0;
      Fingerprint _fp;
      DP _dp;
      if (lookup(fp, _dp.cachedataLocation_)) {
        promote(fp);
        return ;
      }

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

    DLRUFPIndex::DLRUFPIndex(uint32_t capacity) {
      capacity_ = capacity;
      spaceAllocator_.capacity_ = capacity_ * Config::getInstance().getChunkSize();
      spaceAllocator_.nextLocation_ = 0;
      spaceAllocator_.chunkSize_ = Config::getInstance().getChunkSize();
    }

    void DLRUFPIndex::reference(uint8_t *fp) {
      Fingerprint _fp;
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

    void DLRUFPIndex::dereference(uint8_t *fp) {
      Fingerprint _fp;
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
}
