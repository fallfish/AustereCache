#ifndef SSDDUP_REFERENCECOUNTER_H
#define SSDDUP_REFERENCECOUNTER_H


#include <map>
#include <common/config.h>
#include <cstring>
#include <mutex>

namespace cache {

  class MapReferenceCounter {

    std::map<uint64_t, uint32_t> counters_;

    public:
    bool clear();
    uint32_t query(uint64_t key);
    bool reference(uint64_t key);
    bool dereference(uint64_t key);

    static MapReferenceCounter& getInstance() {
      static MapReferenceCounter instance;
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

  class SketchReferenceCounter {
    uint8_t *sketch_;
    uint32_t width_, height_;
    std::map<uint32_t, uint16_t> mp_;

    SketchReferenceCounter();
    public:
      void clear();
      uint32_t query(uint64_t key);
      void reference(uint64_t key);
      void dereference(uint64_t key);
      static SketchReferenceCounter& getInstance() {
        static SketchReferenceCounter instance;
        return instance;
      }
  };

  class ReferenceCounter {
    public:
      ReferenceCounter() {}
      static ReferenceCounter& getInstance() {
        static ReferenceCounter instance;
        return instance;
      }
      uint32_t query(uint64_t key) {
        std::lock_guard<std::mutex> lock(rfMutex_);
        if (Config::getInstance().isSketchRFEnabled()) {
          return SketchReferenceCounter::getInstance().query(key);
        } else {
          return MapReferenceCounter::getInstance().query(key);
        }
      }

      void reference(uint64_t key) {
        std::lock_guard<std::mutex> lock(rfMutex_);
        if (Config::getInstance().isSketchRFEnabled()) {
          SketchReferenceCounter::getInstance().reference(key);
        } else {
          MapReferenceCounter::getInstance().reference(key);
        }
      }

      void dereference(uint64_t key) {
        std::lock_guard<std::mutex> lock(rfMutex_);
        if (Config::getInstance().isSketchRFEnabled()) {
          SketchReferenceCounter::getInstance().dereference(key);
        } else {
          MapReferenceCounter::getInstance().dereference(key);
        }
      }
      std::mutex rfMutex_;
  };
}
#endif //SSDDUP_REFERENCECOUNTER_H
