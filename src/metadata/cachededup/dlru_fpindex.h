#ifndef AUSTERECACHE_DLRUFPINDEX_H
#define AUSTERECACHE_DLRUFPINDEX_H

#include "common.h"
/*
 * This is a deduplication-only design, so the address allocation to the
 * data cache is bound to be aligned with chunk size.
 * When the cache is not full, we use a self-incremental counter to maintain
 * the free addresses; if the cache is full, the evicted address will
 * be a new free address and it will be allocated to the new entry immediately.
 */
namespace cache {
  class DLRUFPIndex {
    public:
      struct DP {
        uint64_t cachedataLocation_{};
        uint32_t referenceCount_{};
        std::list<Fingerprint>::iterator it_;
        std::list<Fingerprint>::iterator zeroReferenceListIter_;
      };

      DLRUFPIndex();

      explicit DLRUFPIndex(uint32_t capacity);

      static DLRUFPIndex &getInstance();

      void init();

      void reference(uint8_t *fp);

      void dereference(uint8_t *fp);

      bool lookup(uint8_t *fp, uint64_t &cachedataLocation);

      void promote(uint8_t *fp);

      void update(uint8_t *fp, uint64_t &cachedataLocation);

      uint32_t capacity_;
    private:
      std::map<Fingerprint, DP> mp_; // mapping from Fingerprint to list
      std::list<Fingerprint> list_;
      std::list<Fingerprint> zeroReferenceList_;
      SpaceAllocator spaceAllocator_;
  };
}
#endif //AUSTERECACHE_DLRUFPINDEX_H
