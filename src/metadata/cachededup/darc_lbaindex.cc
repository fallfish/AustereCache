#include "darc_lbaindex.h"
#include "darc_fpindex.h"
#include "cdarc_fpindex.h"

namespace cache {
    DARCLBAIndex::DARCLBAIndex() {}

    DARCLBAIndex& DARCLBAIndex::getInstance()
    {
      static DARCLBAIndex instance;
      return instance;
    }

    void DARCLBAIndex::init(uint32_t p, uint32_t x)
    {
      capacity_ = Config::getInstance().getnSourceIndexEntries() / 2;
      p_ = p;
      x_ = x; // capacity_;
    }

    bool DARCLBAIndex::lookup(uint64_t lba, uint8_t *ca)
    {
      auto it = mp_.find(lba);
      if (it == mp_.end()) {
        return false;
      } else {
        memcpy(ca, it->second.v_, Config::getInstance().getFingerprintLength());
        return true;
      }
    }

    void DARCLBAIndex::adjust_adaptive_factor(uint64_t lba)
    {
      auto it = mp_.find(lba);
      if (it == mp_.end()) return ;
      if (it->second.listId_ == IN_B1) {
        if (p_ < capacity_) ++p_;
      } else if (it->second.listId_ == IN_B2) {
        if (p_ > 0) --p_;
      }
    }

    void DARCLBAIndex::promote(uint64_t lba)
    {
    }

    void DARCLBAIndex::moveFromAToB(EntryLocation from, EntryLocation to)
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
        CDARCFPIndex::getInstance().dereference(lba, it->second.v_);
#else
        DARCFPIndex::getInstance().dereference(lba, it->second.v_);
#endif
      }
      if (to == IN_T1 || to == IN_T2) {
#ifdef CDARC
        CDARCFPIndex::getInstance().reference(lba, it->second.v_);
#else
        DARCFPIndex::getInstance().reference(lba, it->second.v_);
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

    void DARCLBAIndex::update(uint64_t lba, uint8_t *fp)
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
          CDARCFPIndex::getInstance().dereference(lba, iter->second.v_);
#else
          DARCFPIndex::getInstance().dereference(lba, iter->second.v_);
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
      CDARCFPIndex::getInstance().reference(lba, fp);
#else
      DARCFPIndex::getInstance().reference(lba, fp);
#endif
    }

    void DARCLBAIndex::check_metadata_cache(uint64_t lba)
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
     * -- if |T1| < cap, move BucketAwareLRU from B1 to B3, Do replace
     * -- else, move BucketAwareLRU from T1 to B1, then BucketAwareLRU from B1 to B3
     * else
     * -- if |B1| + |B2| > cap, move BucketAwareLRU from B2 to B3
     * -- Do replace
     */
    void DARCLBAIndex::manage_metadata_cache(uint64_t lba)
    {
      uint64_t lba_ = 0;
      bool shouldReplace = true;
      bool moveToB3 = false;
      if (t1_.size() + b1_.size() >= capacity_) {
        if (t1_.size() >= capacity_) {
          moveFromAToB(IN_T1, IN_B1);
          shouldReplace = false;
        }
        // move BucketAwareLRU from b1 to b3
        moveFromAToB(IN_B1, IN_B3);
      } else {
        if (b1_.size() + b2_.size() >= capacity_) {
          // move BucketAwareLRU from b2 to b3
          moveFromAToB(IN_B2, IN_B3);
        }
      }

      if (shouldReplace) {
        replace_in_metadata_cache(lba);
      }
    }

    void DARCLBAIndex::replace_in_metadata_cache(uint64_t lba)
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

    void DARCLBAIndex::check_list_id_consistency() {
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

    void DARCLBAIndex::check_zero_reference(uint8_t *ca) {
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

    void DARCLBAIndex::getFingerprints(std::set<Fingerprint> &v) {
      Fingerprint arr;
      for (auto t : t1_) {
        memcpy(arr.v_, mp_[t].v_, 20);
        v.insert(arr);
      }
      for (auto t : t2_) {
        memcpy(arr.v_, mp_[t].v_, 20);
        v.insert(arr);
      }
      for (auto t : b1_) {
        memcpy(arr.v_, mp_[t].v_, 20);
        v.insert(arr);
      }
      for (auto t : b2_) {
        memcpy(arr.v_, mp_[t].v_, 20);
        v.insert(arr);
      }
      for (auto t : b3_) {
        memcpy(arr.v_, mp_[t].v_, 20);
        v.insert(arr);
      }
    }
}
