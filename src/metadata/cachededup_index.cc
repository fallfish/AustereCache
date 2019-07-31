#include "cachededup_index.h"
#include "common/config.h"
#include <iostream>
#include <cassert>


namespace cache {
  DLRU_MetadataCache DLRU_MetadataCache::instance;
  DLRU_DataCache DLRU_DataCache::instance;
  DARC_MetadataCache DARC_MetadataCache::instance;
  DARC_DataCache DARC_DataCache::instance;

  DLRU_MetadataCache::DLRU_MetadataCache() {}
  DLRU_MetadataCache& DLRU_MetadataCache::get_instance()
  {
    return instance;
  }

  void DLRU_MetadataCache::init(uint32_t capacity)
  {
    _capacity = capacity;
  }

  bool DLRU_MetadataCache::lookup(uint64_t lba, uint8_t *ca) {
    auto it = _mp.find(lba);
    if (it == _mp.end()) return false;
    else {
      memcpy(ca, it->second._v, Config::get_configuration().get_ca_length());
      return true;
    }
  }

  void DLRU_MetadataCache::promote(uint64_t lba) {
    auto it = _mp.find(lba)->second._it;
    _list.erase(it);
    _list.push_front(lba);
    // renew the iterator stored in the mapping
    _mp[lba]._it = _list.begin();
  }

  void DLRU_MetadataCache::update(uint64_t lba, uint8_t *ca) {
    CA ca_;
    if (lookup(lba, ca_._v)) {
      promote(lba);
      return ;
    }

    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());
    if (_list.size() == _capacity) {
      uint64_t lba_ = _list.back();
      _list.pop_back();
      _mp.erase(lba_);
    }
    _list.push_front(lba);
    ca_._it = _list.begin();
    _mp[lba] = ca_;
  }

  DLRU_DataCache::DLRU_DataCache() {}
  DLRU_DataCache& DLRU_DataCache::get_instance()
  {
    return instance;
  }

  void DLRU_DataCache::init(uint32_t cap)
  {
    _capacity = cap;
    _current_free_data_pointer = 0;
  }


  bool DLRU_DataCache::lookup(uint8_t *ca, uint64_t &ssd_data_pointer)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());
    auto it = _mp.find(ca_);

    if (it == _mp.end()) {
      return false;
    } else {
      ssd_data_pointer = it->second._ssd_data_pointer;
      return true;
    }
  }

  void DLRU_DataCache::promote(uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());
    auto it = _mp.find(ca_);
    assert(it != _mp.end());
    _list.erase(it->second._it);
    _list.push_front(ca_);

    // renew the iterator stored in the mapping
    it->second._it = _list.begin();
  }

  void DLRU_DataCache::update(uint8_t *ca, uint64_t &ssd_data_pointer)
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
      dp_._ssd_data_pointer = _mp[ca_]._ssd_data_pointer;
      _mp.erase(ca_);
    } else {
      // if the cache is not full, allocate a new ssd address
      dp_._ssd_data_pointer = _current_free_data_pointer;
      _current_free_data_pointer += Config::get_configuration().get_chunk_size();
    }

    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());
    _list.push_front(ca_);
    dp_._it = _list.begin();

    _mp[ca_] = dp_;
    ssd_data_pointer = dp_._ssd_data_pointer;
  }

  DARC_MetadataCache::DARC_MetadataCache() {}

  DARC_MetadataCache& DARC_MetadataCache::get_instance()
  {
    return instance;
  }

  void DARC_MetadataCache::init(uint32_t cap, uint32_t p, uint32_t x)
  {
    _capacity = cap;
    _p = p;
    _x = x;
  }

  bool DARC_MetadataCache::lookup(uint64_t lba, uint8_t *ca)
  {
    auto it = _mp.find(lba);
    if (it == _mp.end()) {
      return false;
    } else {
      memcpy(ca, it->second._v, Config::get_configuration().get_ca_length());
      return true;
    }
  }

  void DARC_MetadataCache::adjust_adaptive_factor(uint8_t list_id)
  {
    if (list_id == 2) {
      if (_p < _capacity) ++_p;
    } else if (list_id == 3) {
      if (_p > 0) --_p;
    }
  }

  void DARC_MetadataCache::promote(uint64_t lba)
  {
  }


  void DARC_MetadataCache::update(uint64_t lba, uint8_t *ca)
  {
    auto it = _mp.find(lba);
    if (it == _mp.end()) {
      CA ca_;
      check_metadata_cache(lba);

      _t1.push_front(lba);

      ca_._it = _t1.begin();
      ca_._list_id = 0;
      memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());

      _mp[lba] = ca_;
      it = _mp.find(lba);
      // add reference count
      DARC_DataCache::get_instance().reference(lba, ca);
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
        if (it->second._list_id == 2) {
          // entry is in B1, move to T2
          _b1.erase(it->second._it);
        } else if (it->second._list_id == 3) {
          // entry is in B2, move to T2
          _b2.erase(it->second._it);
        }
        adjust_adaptive_factor(it->second._list_id);
        // add reference count
        DARC_DataCache::get_instance().reference(lba, it->second._v);
      }

      _t2.push_front(lba);
      it->second._it = _t2.begin();
      it->second._list_id = 1;
    } else if (it->second._list_id == 4) {
      // entry is in B3
      _b3.erase(it->second._it);

      _t1.push_front(lba);
      it->second._it = _t1.begin();
      it->second._list_id = 0;

      // add reference count
      DARC_DataCache::get_instance().reference(lba, ca);
    }

    memcpy(it->second._v, ca, Config::get_configuration().get_ca_length());
  }

  void DARC_MetadataCache::check_metadata_cache(uint64_t lba)
  {
    uint64_t lba_;
    if (_t1.size() + _t2.size() + _b3.size() == _capacity + _x) {
      if (_b3.size() == 0) {
        manage_metadata_cache(lba);
      }
      lba_ = _b3.back();
      _b3.pop_back();
      _mp.erase(lba_);
    }
  }

  void DARC_MetadataCache::manage_metadata_cache(uint64_t lba)
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
          DARC_DataCache::get_instance().deference(lba_, it->second._v);
        } else {
          // move LRU from t1 to b3
          lba_ = _t1.back();
          _t1.pop_back();
          _b3.push_front(lba_);

          auto it = _mp.find(lba_);
          it->second._list_id = 4;
          it->second._it = _b3.begin();
          // dec reference count
          DARC_DataCache::get_instance().deference(lba_, it->second._v);
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

  void DARC_MetadataCache::replace_in_metadata_cache(uint64_t lba)
  {
    uint64_t lba_;
    if (_t1.size() > 0 && 
        (_t1.size() > _p ||
         (_t1.size() == _p && (_mp.find(lba) != _mp.end() && _mp.find(lba)->second._list_id == 1))
        )) {
      // move LRU from T1 to B1
      lba_ = _t1.back();
      _t1.pop_back();
      _b1.push_front(lba_);

      auto it = _mp.find(lba_);
      it->second._list_id = 2;
      it->second._it = _b1.begin();

      // dec reference count
      DARC_DataCache::get_instance().deference(lba_, it->second._v);
    } else {
      // move LRU from T2 to B2
      lba_ = _t2.back();
      _t2.pop_back();
      _b2.push_front(lba_);

      auto it = _mp.find(lba_);
      it->second._list_id = 3;
      it->second._it = _b2.begin();
      // dec reference count
      DARC_DataCache::get_instance().deference(lba_, it->second._v);
    }
  }

  DARC_DataCache::DARC_DataCache() {}
  DARC_DataCache &DARC_DataCache::get_instance()
  {
    return instance;
  }
  void DARC_DataCache::init(uint32_t cap)
  {
    _capacity = cap;
    _current_free_data_pointer = 0;
  }

  bool DARC_DataCache::lookup(uint8_t *ca, uint64_t &ssd_data_pointer) {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      return false;
    } else {
      ssd_data_pointer = it->second._ssd_data_pointer;
      return true;
    }
  }

  void DARC_DataCache::reference(uint64_t lba, uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      std::cout << "reference an empty entry" << std::endl;
      exit(0);
    }
    it->second._reference_count += 1;
    if (it->second._reference_count == 1) {
      _zero_reference_list.remove(ca_);
    }
  }

  void DARC_DataCache::deference(uint64_t lba, uint8_t *ca)
  {
    CA ca_;
    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());

    auto it = _mp.find(ca_);
    if (it == _mp.end()) {
      std::cout << "deference an empty entry" << std::endl;
      exit(0);
    }
    it->second._reference_count -= 1;
    if (it->second._reference_count == 0) {
      _zero_reference_list.push_front(ca_);
    }
  }

  void DARC_DataCache::update(uint64_t lba, uint8_t *ca, uint64_t &ssd_data_pointer)
  {
    int _ = _capacity;
    CA ca_;
    DP dp_; dp_._reference_count = 0;
    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());
    if (_mp.find(ca_) != _mp.end()) {
      return ;
    }

    if (_mp.size() == _capacity) {
      // current cache is full, evict an old entry and
      // allocate its ssd location to the new one
      while (_zero_reference_list.size() == 0) {
        DARC_MetadataCache::get_instance().manage_metadata_cache(lba);
      }
      ca_ = _zero_reference_list.back();
      _zero_reference_list.pop_back();

      dp_._ssd_data_pointer = _mp[ca_]._ssd_data_pointer;
      _mp.erase(ca_);
    } else {
      dp_._ssd_data_pointer = _current_free_data_pointer;
      _current_free_data_pointer += Config::get_configuration().get_chunk_size();
    }

    memcpy(ca_._v, ca, Config::get_configuration().get_ca_length());

    dp_._reference_count = 0;

    _mp[ca_] = dp_;
    ssd_data_pointer = dp_._ssd_data_pointer;
  }
}
