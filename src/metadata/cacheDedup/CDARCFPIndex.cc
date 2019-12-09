//
// Created by 王秋平 on 12/3/19.
//

#include <common/stats.h>
#include "DARCLBAIndex.h"
#include "CDARCFPIndex.h"
#include "manage/DirtyList.h"
#include "manage/managemodule.h"

namespace cache {
    CDARCFPIndex::CDARCFPIndex() {}

    CDARCFPIndex &CDARCFPIndex::getInstance()
    {
      static CDARCFPIndex instance;
      return instance;
    }
    void CDARCFPIndex::init()
    {
      capacity_ = Config::getInstance().getCacheDeviceSize() / Config::getInstance().getWriteBufferSize();
      weuAllocator_.init();
    }

    // Need to carefully deal with stale entries after a WEU eviction
    // one WEU contains mixed "invalid" and "valid" LBAs_
    // for those valid LBAs_, we have no way but evict them in the following procedure
    // or a sweep and clean procedure.
    bool CDARCFPIndex::lookup(uint8_t *fp, uint32_t &weuId, uint32_t &offset, uint32_t &len) {
      Fingerprint _fp;
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

    void CDARCFPIndex::reference(uint64_t lba, uint8_t *fp)
    {
      Fingerprint _fp;
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

    void CDARCFPIndex::dereference(uint64_t lba, uint8_t *fp)
    {
      Fingerprint _fp;
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

    uint32_t CDARCFPIndex::update(
      uint64_t lba, uint8_t *fp, uint32_t &weuId,
      uint32_t &offset, uint32_t len)
    {
      uint32_t evicted_weu_id = ~0;
      Fingerprint _fp;
      DP _dp;
      memcpy(_fp.v_, fp, Config::getInstance().getFingerprintLength());
      if (mp_.find(_fp) != mp_.end()) {
        return evicted_weu_id;
      }

      if (weuAllocator_.isCurrentWEUFull(len) && weuReferenceCount_.size() == capacity_) {
        // current cache is full, evict an old entry and
        // allocate its ssd location to the new one
        while (zeroReferenceList_.size() == 0) {
          DARCLBAIndex::getInstance().manage_metadata_cache(lba);
        }
        uint32_t weu_id = zeroReferenceList_.back();
        zeroReferenceList_.pop_back();
        weuReferenceCount_.erase(weu_id);

        if (Config::getInstance().getCacheMode() == tWriteBack) {
          DirtyList::getInstance().addEvictedChunk(ManageModule::getInstance().weuToCachedataLocation_[weu_id],
                                                   Config::getInstance().getWriteBufferSize());
        }
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

    void CDARCFPIndex::dumpStats()
    {
      std::set<Fingerprint> fingerprints;
      DARCLBAIndex::getInstance().getFingerprints(fingerprints);
      uint32_t nInvalidFingerprints = 0;
      uint32_t nTotalFingerprints = 0;
      Fingerprint arr;
      for (auto pr : mp_) {
        if (weuReferenceCount_.find(pr.second.weuId_) == weuReferenceCount_.end()) continue;
        nTotalFingerprints += 1;
        memcpy(arr.v_, pr.first.v_, 20);
        if (fingerprints.find(arr) == fingerprints.end()) {
          nInvalidFingerprints += 1;
        }
      }
      printf("Zero Referenced WEUs: %lu, Total WEUs: %lu\n", zeroReferenceList_.size(), weuReferenceCount_.size());
      printf("Zero Referenced Fingerprints: %u, Total Fingerprints: %u\n", nInvalidFingerprints, nTotalFingerprints);
      fingerprints.clear();
    }

    void CDARCFPIndex::clearObsolete()
    {
      std::vector<Fingerprint> invalidFingerprints;
      for (auto &pr : mp_) {
        if (weuAllocator_.hasRecycled(pr.second.weuId_)) {
          invalidFingerprints.push_back(pr.first);
        }
      }
      for (auto &fingerprint : invalidFingerprints) {
        mp_.erase(fingerprint);
      }
      weuAllocator_.evictedWEUIds.clear();
    }
}
