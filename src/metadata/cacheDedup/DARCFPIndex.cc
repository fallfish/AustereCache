
#include <manage/DirtyList.h>
#include "DARCFPIndex.h"
#include "DARCLBAIndex.h"
#include "common/stats.h"

namespace cache {
    DARCFPIndex::DARCFPIndex() = default;
    DARCFPIndex &DARCFPIndex::getInstance()
    {
      static DARCFPIndex instance;
      return instance;
    }
    void DARCFPIndex::init()
    {
      capacity_ = Config::getInstance().getCacheDeviceSize() / Config::getInstance().getChunkSize();
      spaceAllocator_.capacity_ = capacity_ * Config::getInstance().getChunkSize();
      spaceAllocator_.nextLocation_ = 0;
      spaceAllocator_.chunkSize_ = Config::getInstance().getChunkSize();
    }

    bool DARCFPIndex::lookup(uint8_t *fp, uint64_t &cachedataLocation) {
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

    void DARCFPIndex::reference(uint64_t lba, uint8_t *fp)
    {
      Fingerprint _fp;
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

    void DARCFPIndex::dereference(uint64_t lba, uint8_t *ca)
    {
      Fingerprint _fp;
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
        //DARCLBAIndex::getInstance().check_zero_reference(_fp.v_);
      }
      // Deference a fingerprint index meaning a LBA index entry has been removed from T1 or T2
      Stats::getInstance().add_lba_index_eviction_caused_by_capacity();
    }

    void DARCFPIndex::update(uint64_t lba, uint8_t *fp, uint64_t &cachedataLocation)
    {
      Fingerprint _fp;
      DP _dp;
      memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
      if (mp_.find(_fp) != mp_.end()) {
        return ;
      }

      if (mp_.size() == capacity_) {
        // current cache is full, evict an old entry and
        // allocate its ssd location to the new one
        while (zeroReferenceList_.size() == 0) {
          DARCLBAIndex::getInstance().manage_metadata_cache(lba);
        }
        _fp = zeroReferenceList_.back();
        zeroReferenceList_.pop_back();

        spaceAllocator_.recycle(mp_[_fp].cachedataLocation_);
        if (Config::getInstance().getCacheMode() == tWriteBack) {
          DirtyList::getInstance().addEvictedChunk(mp_[_fp].cachedataLocation_,
                                                   Config::getInstance().getChunkSize());
        }
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
}
