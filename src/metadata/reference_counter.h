#ifndef AUSTERECACHE_REFERENCECOUNTER_H
#define AUSTERECACHE_REFERENCECOUNTER_H


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
#endif //AUSTERECACHE_REFERENCECOUNTER_H
