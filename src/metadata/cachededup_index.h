#include <cstring>
#include <map>
#include <list>

namespace cache {
  class DLRU_MetadataCache {
    public:
      struct CA {
        CA() { memset(_v, 0, 20); }
        uint8_t _v[20];
        std::list<uint64_t>::iterator _it;
      };

      DLRU_MetadataCache();
      static DLRU_MetadataCache& get_instance();
      void init(uint32_t capacity);

      bool lookup(uint64_t lba, uint8_t *ca);
      void promote(uint64_t lba);
      void update(uint64_t lba, uint8_t *ca);

    private:
      static DLRU_MetadataCache instance;
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
  class DLRU_DataCache {
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

      DLRU_DataCache();
      static DLRU_DataCache& get_instance();
      void init(uint32_t cap);


      bool lookup(uint8_t *ca, uint64_t &ssd_data_pointer);
      void promote(uint8_t *ca);
      void update(uint8_t *ca, uint64_t &ssd_data_pointer);
    private:
      static DLRU_DataCache instance;
      uint32_t _capacity;
      uint64_t _current_free_data_pointer;
      std::map<CA, DP> _mp; // mapping from CA to list
      std::list<CA> _list;
  };

  class DARC_MetadataCache {
    public:
      friend class DARC_DataCache;
      
      struct CA {
        CA() {
          memset(_v, 0, sizeof(_v));
          _list_id = 0;
        }

        uint8_t _v[20];
        uint8_t _list_id;
        std::list<uint64_t>::iterator _it;
      };
      DARC_MetadataCache();
      static DARC_MetadataCache& get_instance();
      void init(uint32_t cap, uint32_t p, uint32_t x);

      bool lookup(uint64_t lba, uint8_t *ca);
      void adjust_adaptive_factor(uint8_t list_id);
      void promote(uint64_t lba);
      void update(uint64_t lba, uint8_t *ca);
      void check_metadata_cache(uint64_t lba);
      void manage_metadata_cache(uint64_t lba);
      void replace_in_metadata_cache(uint64_t lba);
    private:
      static DARC_MetadataCache instance;
      std::map<uint64_t, CA> _mp;
      std::list<uint64_t> _t1, _t2, _b1, _b2, _b3;
      uint32_t _capacity, _p, _x;
  };

  class DARC_DataCache {
    public:
      friend class DARC_MetadataCache;


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
      };

      DARC_DataCache();
      static DARC_DataCache &get_instance();
      void init(uint32_t cap);
      bool lookup(uint8_t *ca, uint64_t &ssd_data_pointer);
      void reference(uint64_t lba, uint8_t *ca);
      void deference(uint64_t lba, uint8_t *ca);
      void update(uint64_t lba, uint8_t *ca, uint64_t &ssd_data_pointer);
    private:
      static DARC_DataCache instance;
      std::map<CA, DP> _mp;
      std::list<CA> _zero_reference_list;
      uint32_t _capacity;
      uint64_t _current_free_data_pointer;
  };
}
