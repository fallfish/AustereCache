#include "cachededup_index.h"
#include "common/config.h"
#include "common/stats.h"
#include "manage/dirty_list.h"

#include <iostream>
#include <cassert>
#include <csignal>

namespace cache {
  DLRU_SourceIndex DLRU_SourceIndex::instance;
  DLRU_FingerprintIndex DLRU_FingerprintIndex::instance;
  DARC_SourceIndex DARC_SourceIndex::instance;
  DARC_FingerprintIndex DARC_FingerprintIndex::instance;
  CDARC_FingerprintIndex CDARC_FingerprintIndex::instance;

  DLRU_SourceIndex::DLRU_SourceIndex() {}
  DLRU_SourceIndex& DLRU_SourceIndex::get_instance()
  {
    return instance;
  }

  void DLRU_SourceIndex::init(uint32_t capacity)
  {
    _capacity = capacity;
  }

  /**
   * (Comment by jhli) Check whether the address/index is stored in the cache
   */
  bool DLRU_SourceIndex::lookup(uint64_t lba, uint8_t *ca) {
    auto it = _mp.find(lba);
    if (it == _mp.end()) return false;
    else {
      memcpy(ca, it->second._v, Config::get_configuration()->get_ca_length());
      return true;
    }
  }

  /**
   * (Comment by jhli) move the accessed index to the front
   */
  void DLRU_SourceIndex::promote(uint64_t lba) {
    auto it = _mp.find(lba)->second._it;
    _list.erase(it);
    _list.push_front(lba);
    // renew the iterator stored in the mapping
    _mp[lba]._it = _list.begin();
  }

  /**
   * (Comment by jhli) move 
   */
  void DLRU_SourceIndex::update(uint64_t lba, uint8_t *ca) {
    CA ca_;
    if (lookup(lba, ca_._v)) {
      auto it = _mp.find(lba)->second._it;
      memcpy(_mp[lba]._v, ca, Config::get_configuration()->get_ca_length());
      _list.erase(it);
      _list.push_front(lba);
      // renew the iterator stored in the mapping
      _mp[lba]._it = _list.begin();
      return ;
    }

    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
    if (_list.size() == _capacity) {
      uint64_t lba_ = _list.back();
      _list.pop_back();
      _mp.erase(lba_);
      Stats::get_instance()->add_lba_index_eviction_caused_by_capacity();
    }
    _list.push_front(lba);
    ca_._it = _list.begin();
    _mp[lba] = ca_;
  }

  DLRU_FingerprintIndex::DLRU_FingerprintIndex() {}
  DLRU_FingerprintIndex& DLRU_FingerprintIndex::get_instance()
  {
    return instance;
  }

  void DLRU_FingerprintIndex::init(uint32_t cap)
  {
    _capacity = cap;
    _space_allocator._capacity = cap * Config::get_configuration()->get_chunk_size();
    _space_allocator._current_location = 0;
    _space_allocator._chunk_size = Config::get_configuration()->get_chunk_size();
  }

  bool DLRU_FingerprintIndex::lookup(uint8_t *ca, uint64_t &ssd_data_pointer)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
    auto it = _mp.find(ca_);

    if (it == _mp.end()) {
      return false;
    } else {
      ssd_data_pointer = it->second._ssd_data_pointer;
      return true;
    }
  }

  void DLRU_FingerprintIndex::promote(uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
    auto it = _mp.find(ca_);
    assert(it != _mp.end());
    _list.erase(it->second._it);
    _list.push_front(ca_);

    // renew the iterator stored in the mapping
    it->second._it = _list.begin();
  }

  void DLRU_FingerprintIndex::update(uint8_t *ca, uint64_t &ssd_data_pointer)
  {
    CA ca_;
    DP dp_;
    if (lookup(ca, dp_._ssd_data_pointer)) {
      promote(ca);
      return ;
    }

    if (_list.size() == _capacity) {
      // current cache is full, evict an old entry and
      // allocate its ssd location to the new one
      ca_ = _list.back();
      _list.pop_back();
      // assign the evicted free ssd location to the newly inserted data
      _space_allocator.recycle(_mp[ca_]._ssd_data_pointer);
#if defined(WRITE_BACK_CACHE)
      DirtyList::get_instance()->add_evicted_block(_mp[ca_]._ssd_data_pointer,
          Config::get_configuration()->get_chunk_size());
#endif
      _mp.erase(ca_);
      Stats::get_instance()->add_ca_index_eviction_caused_by_capacity();
    }
    dp_._ssd_data_pointer = _space_allocator.allocate();

    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
    _list.push_front(ca_);
    dp_._it = _list.begin();

    _mp[ca_] = dp_;
    ssd_data_pointer = dp_._ssd_data_pointer;
  }

  DARC_SourceIndex::DARC_SourceIndex() {}

  DARC_SourceIndex& DARC_SourceIndex::get_instance()
  {
    return instance;
  }

  void DARC_SourceIndex::init(uint32_t cap, uint32_t p, uint32_t x)
  {
    _capacity = cap;
    _p = p;
    _x = x;
  }

  bool DARC_SourceIndex::lookup(uint64_t lba, uint8_t *ca)
  {
    auto it = _mp.find(lba);
    if (it == _mp.end()) {
      return false;
    } else {
      memcpy(ca, it->second._v, Config::get_configuration()->get_ca_length());
      return true;
    }
  }

  void DARC_SourceIndex::adjust_adaptive_factor(uint64_t lba)
  {
    auto it = _mp.find(lba);
    if (it == _mp.end()) return ;
    if (it->second._list_id == 2) {
      if (_p < _capacity) ++_p;
    } else if (it->second._list_id == 3) {
      if (_p > 0) --_p;
    }
  }

  void DARC_SourceIndex::promote(uint64_t lba)
  {
  }


  void DARC_SourceIndex::update(uint64_t lba, uint8_t *ca)
  {
    CA ca_;
    auto it = _mp.find(lba);

    if (it == _mp.end()) {
      check_metadata_cache(lba);

      _t1.push_front(lba);
      memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
      ca_._it = _t1.begin();
      ca_._list_id = 0;

      _mp[lba] = ca_;
      // add reference count
#ifdef CDARC
      CDARC_FingerprintIndex::get_instance().reference(lba, ca);
#else
      DARC_FingerprintIndex::get_instance().reference(lba, ca);
#endif
      return ;
    }


    if (it->second._list_id >= 0 && it->second._list_id <= 3) {
      if (it->second._list_id == 0) {
        // entry is in T1 and be referenced again, move to T2
        _t1.erase(it->second._it);
      } else if (it->second._list_id == 1) {
        // entry is in T2, move to the head
        _t2.erase(it->second._it);
      } else {
        check_metadata_cache(lba);
        it = _mp.find(lba);
        if (it == _mp.end()) {
          // if check metadata cache remove current lba from the mp
          _t1.push_front(lba);
          memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
          ca_._it = _t1.begin();
          ca_._list_id = 0;

          _mp[lba] = ca_;
          // add reference count
#ifdef CDARC
          CDARC_FingerprintIndex::get_instance().reference(lba, ca);
#else
          DARC_FingerprintIndex::get_instance().reference(lba, ca);
#endif
          return ;
        } else if (it->second._list_id == 2) {
          // entry is in B1, move to T2
          _b1.erase(it->second._it);
        } else if (it->second._list_id == 3) {
          // entry is in B2, move to T2
          _b2.erase(it->second._it);
        }
      }

      // If the request is a write, and the content has been changed,
      // we must deference the old fingerprint
      //if (Stats::get_instance()->get_current_request_type() == 1
      if ((it->second._list_id == 0 || it->second._list_id == 1)) {
#ifdef CDARC
        CDARC_FingerprintIndex::get_instance().deference(lba, it->second._v);
#else
        DARC_FingerprintIndex::get_instance().deference(lba, it->second._v);
#endif
      }

      _t2.push_front(lba);
      _mp[lba]._list_id = 1;
      _mp[lba]._it = _t2.begin();
    } else if (it->second._list_id == 4) {
      // entry is in B3
      _b3.erase(it->second._it);

      _t1.push_front(lba);
      _mp[lba]._list_id = 0;
      _mp[lba]._it = _t1.begin();
    }

      // add reference count
#ifdef CDARC
    CDARC_FingerprintIndex::get_instance().reference(lba, ca);
#else
    DARC_FingerprintIndex::get_instance().reference(lba, ca);
#endif
    memcpy(_mp[lba]._v, ca, Config::get_configuration()->get_ca_length());
  }

  void DARC_SourceIndex::check_metadata_cache(uint64_t lba)
  {
    uint64_t lba_;
    if (_t1.size() + _t2.size() + _b3.size() == _capacity + _x) {
      if (_b3.size() == 0) {
        manage_metadata_cache(lba);
      }
      if (_b3.size() != 0) {
        lba_ = _b3.back();
        _b3.pop_back();
        _mp.erase(lba_);
      }
    }
  }

  void DARC_SourceIndex::manage_metadata_cache(uint64_t lba)
  {
    uint64_t lba_;
    if (_t1.size() + _b1.size() >= _capacity) {
      if (_t1.size() < _capacity) {
        // move LRU from b1 to b3
        lba_ = _b1.back();
        _b1.pop_back();
        _b3.push_front(lba_);

        auto it = _mp.find(lba_);
        it->second._list_id = 4;
        it->second._it = _b3.begin();

        replace_in_metadata_cache(lba);
      } else {
        if (_b1.size() > 0) {
          // move LRU from b1 to b3
          lba_ = _b1.back();
          _b1.pop_back();
          _b3.push_front(lba_);

          auto it = _mp.find(lba_);
          it->second._list_id = 4;
          it->second._it = _b3.begin();
          // move LRU from t1 to b1
          lba_ = _t1.back();
          _t1.pop_back();
          _b1.push_front(lba_);

          it = _mp.find(lba_);
          it->second._list_id = 2;
          it->second._it = _b1.begin();
          // dec reference count
#ifdef CDARC
          CDARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#else
          DARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#endif
        } else {
          // move LRU from t1 to b3
          lba_ = _t1.back();
          _t1.pop_back();
          _b3.push_front(lba_);

          auto it = _mp.find(lba_);
          it->second._list_id = 4;
          it->second._it = _b3.begin();
          // dec reference count
#ifdef CDARC
          CDARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#else
          DARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#endif
        }
      }
    } else {
      if (_b1.size() + _b2.size() >= _capacity) {
        // move LRU from b2 to b3
        lba_ = _b2.back();
        _b2.pop_back();
        _b3.push_front(lba_);

        auto it = _mp.find(lba_);
        it->second._list_id = 4;
        it->second._it = _b3.begin();
      }
      replace_in_metadata_cache(lba);
    }
  }

  void DARC_SourceIndex::replace_in_metadata_cache(uint64_t lba)
  {
    uint64_t lba_;
    auto tmp_ = _mp.find(lba);
    if (_t1.size() > 0 && 
        (_t1.size() > _p ||
         (_t1.size() == _p && (_mp.find(lba) != _mp.end() && _mp.find(lba)->second._list_id == 3))
        || _t2.size() == 0)) {
      // move LRU from T1 to B1
      lba_ = _t1.back();
      _t1.pop_back();
      _b1.push_front(lba_);

      auto it = _mp.find(lba_);
      it->second._list_id = 2;
      it->second._it = _b1.begin();

      // dec reference count
#ifdef CDARC
      CDARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#else
      DARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#endif
    } else {
      // move LRU from T2 to B2
      lba_ = _t2.back();
      _t2.pop_back();
      _b2.push_front(lba_);

      auto it = _mp.find(lba_);
      it->second._list_id = 3;
      it->second._it = _b2.begin();
      // dec reference count
#ifdef CDARC
      CDARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#else
      DARC_FingerprintIndex::get_instance().deference(lba_, it->second._v);
#endif
    }
  }

  DARC_FingerprintIndex::DARC_FingerprintIndex() {}
  DARC_FingerprintIndex &DARC_FingerprintIndex::get_instance()
  {
    return instance;
  }
  void DARC_FingerprintIndex::init(uint32_t cap)
  {
    _capacity = cap;
    _space_allocator._capacity = cap * Config::get_configuration()->get_chunk_size();
    _space_allocator._current_location = 0;
    _space_allocator._chunk_size = Config::get_configuration()->get_chunk_size();
  }

  bool DARC_FingerprintIndex::lookup(uint8_t *ca, uint64_t &ssd_data_pointer) {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      return false;
    } else {
      ssd_data_pointer = it->second._ssd_data_pointer;
      return true;
    }
  }

  void DARC_FingerprintIndex::reference(uint64_t lba, uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      std::cout << "reference an empty entry" << std::endl;
      exit(0);
    }
    if ( (it->second._reference_count += 1) == 1) {
      if (it->second._zero_reference_list_it != _zero_reference_list.end()) {
        _zero_reference_list.erase(it->second._zero_reference_list_it);
        it->second._zero_reference_list_it = _zero_reference_list.end();
      }
    }
  }

  void DARC_FingerprintIndex::deference(uint64_t lba, uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      assert(0);
      std::cout << "deference an empty entry" << std::endl;
      exit(0);
    }
    if ((it->second._reference_count -= 1) == 0) {
      _zero_reference_list.push_front(ca_);
      it->second._zero_reference_list_it = _zero_reference_list.begin();
      //DARC_SourceIndex::get_instance().check_zero_reference(ca_._v);
    }
    // Deference a fingerprint index meaning a LBA index entry has been removed from T1 or T2
    Stats::get_instance()->add_lba_index_eviction_caused_by_capacity();
  }

  void DARC_FingerprintIndex::update(uint64_t lba, uint8_t *ca, uint64_t &ssd_data_pointer)
  {
    int _ = _capacity;
    CA ca_;
    DP dp_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
    if (_mp.find(ca_) != _mp.end()) {
      return ;
    }

    if (_mp.size() == _capacity) {
      // current cache is full, evict an old entry and
      // allocate its ssd location to the new one
      while (_zero_reference_list.size() == 0) {
        DARC_SourceIndex::get_instance().manage_metadata_cache(lba);
      }
      ca_ = _zero_reference_list.back();
      _zero_reference_list.pop_back();

      _space_allocator.recycle(_mp[ca_]._ssd_data_pointer);
#if defined(WRITE_BACK_CACHE)
      DirtyList::get_instance()->add_evicted_block(_mp[ca_]._ssd_data_pointer,
          Config::get_configuration()->get_chunk_size());
#endif
      _mp.erase(ca_);

      memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
      Stats::get_instance()->add_ca_index_eviction_caused_by_capacity();
    }

    dp_._ssd_data_pointer = _space_allocator.allocate();
    dp_._reference_count = 0;
    dp_._zero_reference_list_it = _zero_reference_list.end();

    _mp[ca_] = dp_;
    ssd_data_pointer = dp_._ssd_data_pointer;
  }

  CDARC_FingerprintIndex::CDARC_FingerprintIndex() {}
  CDARC_FingerprintIndex &CDARC_FingerprintIndex::get_instance()
  {
    return instance;
  }
  void CDARC_FingerprintIndex::init(uint32_t cap)
  {
    _capacity = cap;
    _weu_allocator.init();
  }

  // Need to carefully deal with stale entries after a WEU eviction
  // one WEU contains mixed "invalid" and "valid" LBAs
  // for those valid LBAs, we have no way but evict them in the following procedure
  // or a sweep and clean procedure.
  bool CDARC_FingerprintIndex::lookup(uint8_t *ca, uint32_t &weu_id, uint32_t &offset, uint32_t &len) {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      return false;
    } else {
      weu_id = it->second._weu_id;
      if (_weu_allocator.has_recycled(weu_id)) {
        _mp.erase(it);
        return false;
      }
      offset = it->second._offset;
      len = it->second._len;
      return true;
    }
  }

  void CDARC_FingerprintIndex::reference(uint64_t lba, uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      std::cout << "reference an empty entry" << std::endl;
      exit(0);
    }

    uint32_t weu_id = it->second._weu_id;
    if (_weu_allocator.has_recycled(weu_id)) {
      _mp.erase(it);
      return;
    }
    if (_weu_reference_count.find(weu_id) == _weu_reference_count.end()) {
      _weu_reference_count[weu_id] = 0;
    }
    
    if ( (_weu_reference_count[weu_id] += 1) == 1 ) {
      _zero_reference_list.remove(weu_id);
    }
  }

  void CDARC_FingerprintIndex::deference(uint64_t lba, uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      std::cout << "deference an empty entry" << std::endl;
      exit(0);
    }

    uint32_t weu_id = it->second._weu_id;
    if ( (_weu_reference_count[weu_id] -= 1) == 0 ) {
      _zero_reference_list.push_front(weu_id);
    }
    // Deference a fingerprint index meaning a LBA index entry has been removed from T1 or T2
    Stats::get_instance()->add_lba_index_eviction_caused_by_capacity();
  }

  uint32_t CDARC_FingerprintIndex::update(
      uint64_t lba, uint8_t *ca, uint32_t &weu_id,
      uint32_t &offset, uint32_t len)
  {
    uint32_t evicted_weu_id = ~0;
    CA ca_;
    DP dp_;
    memcpy(ca_._v, ca, Config::get_configuration()->get_ca_length());
    if (_mp.find(ca_) != _mp.end()) {
      return evicted_weu_id;
    }

    if (_weu_allocator.is_full(len) && _weu_reference_count.size() == _capacity) {
      // current cache is full, evict an old entry and
      // allocate its ssd location to the new one
      while (_zero_reference_list.size() == 0) {
        DARC_SourceIndex::get_instance().manage_metadata_cache(lba);
      }
      uint32_t weu_id = _zero_reference_list.back();
      _zero_reference_list.pop_back();
      _weu_reference_count.erase(weu_id);

      _weu_allocator.recycle(weu_id);
      evicted_weu_id = weu_id;
      Stats::get_instance()->add_ca_index_eviction_caused_by_capacity();
    }
    dp_._len = len;
    _weu_allocator.allocate(dp_._weu_id, dp_._offset, dp_._len);

    _mp[ca_] = dp_;
    weu_id = dp_._weu_id;
    offset = dp_._offset;
    
    return evicted_weu_id;
  }
}
