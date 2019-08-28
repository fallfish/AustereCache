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
    uint64_t _capacity;
    uint64_t _current_location;
    uint64_t _free_location;
    uint64_t _chunk_size;
    void recycle(uint64_t location) {
      _free_location = location;
    }
    uint64_t allocate() {
      uint64_t allocated_location = -1;
      if (_current_location != _capacity) {
        allocated_location = _current_location;
        _current_location += _chunk_size;
      } else {
        allocated_location = _free_location;
      }
      return allocated_location;
    }
  };

  class DLRU_SourceIndex {
    public:
      struct CA {
        CA() { memset(_v, 0, 20); }
        uint8_t _v[20];
        std::list<uint64_t>::iterator _it;
      };

      DLRU_SourceIndex();
      static DLRU_SourceIndex& get_instance();
      void init(uint32_t capacity);

      bool lookup(uint64_t lba, uint8_t *ca);
      void promote(uint64_t lba);
      void update(uint64_t lba, uint8_t *ca);

    private:
      static DLRU_SourceIndex instance;
      uint32_t _capacity;
      std::map<uint64_t, CA> _mp; // mapping from lba to ca and list iter
      std::list<uint64_t> _list;
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
      struct CA {
        CA() { memset(_v, 0, 20); }
        uint8_t _v[20];
        bool operator<(const CA &ca) const {
          return memcmp(_v, ca._v, 20) < 0;
        }
      };
      struct DP {
        uint64_t _ssd_data_pointer;
        std::list<CA>::iterator _it;
      };

      DLRU_FingerprintIndex();
      static DLRU_FingerprintIndex& get_instance();
      void init(uint32_t cap);


      bool lookup(uint8_t *ca, uint64_t &ssd_data_pointer);
      void promote(uint8_t *ca);
      void update(uint8_t *ca, uint64_t &ssd_data_pointer);
    private:
      static DLRU_FingerprintIndex instance;
      uint32_t _capacity;
      std::map<CA, DP> _mp; // mapping from CA to list
      std::list<CA> _list;
      SpaceAllocator _space_allocator;
  };

  class DARC_SourceIndex {
    public:
      friend class DARC_FingerprintIndex;
      
      struct CA {
        CA() {
          memset(_v, 0, 20);
          _list_id = 0;
        }

        uint8_t _v[20];
        /**
         * list_id: 
         */
        uint8_t _list_id;
        std::list<uint64_t>::iterator _it;
      };
      DARC_SourceIndex();
      static DARC_SourceIndex& get_instance();
      void init(uint32_t cap, uint32_t p, uint32_t x);

      bool lookup(uint64_t lba, uint8_t *ca);
      void adjust_adaptive_factor(uint32_t lba);
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
        for (auto lba : _t1) {
          if (_mp[lba]._list_id != 0) {
            lbas2.push_back(lba);
          }
        }
        for (auto lba : _t2) {
          if (_mp[lba]._list_id != 1) {
            std::cout << "Should be in " << (int)_mp[lba]._list_id << " while in t2" << std::endl;
            lbas2.push_back(lba);
          }
        }
        if (lbas2.size() != 0) {
          for (auto lba : lbas2) {
            std::cout << (int)_mp[lba]._list_id << std::endl;
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
        for (auto lba : _t1) {
          if (memcmp(_mp[lba]._v, ca, Config::get_configuration().get_ca_length()) == 0) {
            lbas.push_back(lba);
          }
          if (_mp[lba]._list_id != 0) {
            lbas2.push_back(lba);
          }
        }
        for (auto lba : _t2) {
          if (memcmp(_mp[lba]._v, ca, Config::get_configuration().get_ca_length()) == 0) {
            lbas.push_back(lba);
          }
          if (_mp[lba]._list_id != 1) {
            std::cout << "Should be in " << (int)_mp[lba]._list_id << " while in t2" << std::endl;
            lbas2.push_back(lba);
          }
        }
        assert(lbas.size() == 0);
        if (lbas2.size() != 0) {
          for (auto lba : lbas2) {
            std::cout << (int)_mp[lba]._list_id << std::endl;
          }
        }
        assert(lbas2.size() == 0);
      }
    private:
      static DARC_SourceIndex instance;
      std::map<uint64_t, CA> _mp;
      std::list<uint64_t> _t1, _t2, _b1, _b2, _b3;
      uint32_t _capacity, _p, _x;
  };

  class DARC_FingerprintIndex {
    public:
      friend class DARC_SourceIndex;


      struct CA {
        CA() { memset(_v, 0, sizeof(_v)); }
        uint8_t _v[20];
        bool operator<(const CA &ca) const {
          return memcmp(_v, ca._v, 20) < 0;
        }
        bool operator==(const CA &ca) const {
          for (uint32_t i = 0; i < 20 / 4; ++i) {
            if (((uint32_t*)_v)[i] != ((uint32_t*)ca._v)[i])
              return false;
          }
          return true;
        }
      };
      struct DP {
        uint64_t _ssd_data_pointer;
        uint32_t _reference_count;
        std::list<CA>::iterator _zero_reference_list_it;
      };

      DARC_FingerprintIndex();
      static DARC_FingerprintIndex &get_instance();
      void init(uint32_t cap);
      bool lookup(uint8_t *ca, uint64_t &ssd_data_pointer);
      void reference(uint64_t lba, uint8_t *ca);
      void deference(uint64_t lba, uint8_t *ca);
      void update(uint64_t lba, uint8_t *ca, uint64_t &ssd_data_pointer);
    private:
      static DARC_FingerprintIndex instance;
      std::map<CA, DP> _mp;
      std::list<CA> _zero_reference_list;
      SpaceAllocator _space_allocator;
      uint32_t _capacity;
  };
 
  struct WEUAllocator {
    uint32_t _weu_id = 0;
    uint32_t _weu_size;
    uint32_t _current_offset;
    
    WEUAllocator() {}

    void init()
    {
      // 2 MiB weu size
      _weu_size = Config::get_configuration().get_write_buffer_size();
      _current_offset = 0;
    }
    
    std::set<uint32_t> evicted_weu_ids;

    void clear_evicted_weu_ids() {
      evicted_weu_ids.clear();
    }

    bool has_recycled(uint32_t weu_id) {
      return evicted_weu_ids.find(weu_id) != evicted_weu_ids.end();
    }

    inline void recycle(uint32_t weu_id) {
      evicted_weu_ids.insert(weu_id);
    }

    inline bool is_full(uint32_t length) {
      return (length + _current_offset > _weu_size);
    }

    void allocate(uint32_t &weu_id, uint32_t &offset, uint32_t length)
    {
      if (length + _current_offset > _weu_size)
      {
        _weu_id += 1;
        _current_offset = 0;
      }
      weu_id = _weu_id;
      offset = _current_offset;
      _current_offset += length;
    }
  };

  class CDARC_FingerprintIndex {
    public:
      friend class DARC_SourceIndex;

      struct CA {
        CA() { memset(_v, 0, sizeof(_v)); }
        uint8_t _v[20];
        bool operator<(const CA &ca) const {
          return memcmp(_v, ca._v, 20) < 0;
        }
        bool operator==(const CA &ca) const {
          for (uint32_t i = 0; i < 20 / 4; ++i) {
            if (((uint32_t*)_v)[i] != ((uint32_t*)ca._v)[i])
              return false;
          }
          return true;
        }
      };
      struct DP {
        uint32_t _weu_id;
        uint32_t _offset;
        uint32_t _len;
      };

      CDARC_FingerprintIndex();
      static CDARC_FingerprintIndex &get_instance();
      void init(uint32_t cap);
      bool lookup(uint8_t *ca, uint32_t &weu_id, uint32_t &offset, uint32_t &len);
      void reference(uint64_t lba, uint8_t *ca);
      void deference(uint64_t lba, uint8_t *ca);
      uint32_t update(uint64_t lba, uint8_t *ca, uint32_t &weu_id,
          uint32_t &offset, uint32_t length);
    private:
      static CDARC_FingerprintIndex instance;
      std::map<CA, DP> _mp;
      std::map<uint32_t, uint32_t> _weu_reference_count; // reference count for each weu
      std::list<uint32_t> _zero_reference_list; // weu_ids
      WEUAllocator _weu_allocator;
      uint32_t _capacity;
  };
}
#endif
