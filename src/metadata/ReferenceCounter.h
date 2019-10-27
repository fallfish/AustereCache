#ifndef SSDDUP_REFERENCECOUNTER_H
#define SSDDUP_REFERENCECOUNTER_H


#include <map>
#include <common/config.h>
#include <cstring>

namespace cache {
  class ReferenceCounter {

    std::map<uint64_t, uint32_t> counters_;

    public:
    bool clear();
    bool query(uint64_t key);
    bool reference(uint64_t key);
    bool dereference(uint64_t key);

    static ReferenceCounter& getInstance() {
      static ReferenceCounter instance;
      return instance;
    }
  };

  class FullReferenceCounter {
    struct Key {
      uint8_t value_[20]{};
      Key() { memset(value_, 0, 20); }
      Key(uint8_t *value) { 
        memset(value_, 0, 20);
        memcpy(value_, value, Config::getInstance().getFingerprintLength());
      }
      bool operator<(const Key &key) const {
        return memcmp(value_, key.value_, 20) < 0;
      }
      bool operator==(const Key &key) const {
        return memcmp(value_, key.value_, 20) == 0;
      }
      bool operator=(uint8_t *value) {
        memcpy(value_, value, Config::getInstance().getFingerprintLength());
      }
    };
    std::map<Key, uint32_t> counters_;

    public:
    bool clear();
    bool query(uint8_t * key);
    bool reference(uint8_t * key);
    bool dereference(uint8_t * key);

    static FullReferenceCounter& getInstance() {
      static FullReferenceCounter instance;
      return instance;
    }
  };
}
#endif //SSDDUP_REFERENCECOUNTER_H
