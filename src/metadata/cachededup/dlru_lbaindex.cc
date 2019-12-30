#include <common/stats.h>
#include "dlru_lbaindex.h"
namespace cache {
    DLRULBAIndex::DLRULBAIndex() {}
    DLRULBAIndex::DLRULBAIndex(uint32_t capacity) {
      capacity_ = capacity;
    }

    DLRULBAIndex& DLRULBAIndex::getInstance()
    {
      static DLRULBAIndex instance;
      return instance;
    }

    void DLRULBAIndex::init()
    {
      capacity_ = Config::getInstance().getnSourceIndexEntries();
    }

    /**
     * Check whether the address/index is stored in the cache
     */
    bool DLRULBAIndex::lookup(uint64_t lba, uint8_t *fp) {
      auto it = mp_.find(lba);
      if (it == mp_.end()) return false;
      else {
        memcpy(fp, it->second.v_, Config::getInstance().getFingerprintLength());
        return true;
      }
    }

    /**
     * move the accessed index to the front
     */
    void DLRULBAIndex::promote(uint64_t lba) {
      auto it = mp_.find(lba)->second.it_;
      list_.erase(it);
      list_.push_front(lba);
      // renew the iterator stored in the mapping
      mp_[lba].it_ = list_.begin();
    }

    /**
     * move
     */
    bool DLRULBAIndex::update(uint64_t lba, uint8_t *fp, uint8_t *oldFP) {
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

    void DLRULBAIndex::check_no_reference_to_fp(uint8_t *fp) {
      for (auto pr : mp_) {
        if (memcmp(fp, pr.second.v_, Config::getInstance().getFingerprintLength()) == 0) {
          assert(0);
        }
      }
    }
}
