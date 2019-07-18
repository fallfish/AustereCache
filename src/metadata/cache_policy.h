#ifndef __CACHE_POLICY_H__
#define __CACHE_POLICY_H__

#include <memory>
#include "bucket.h"
namespace cache {
  class CAIndex;

  struct CachePolicyExecutor {
    CachePolicyExecutor(Bucket *bucket);

    virtual void promote(uint32_t slot_id, uint32_t n_slots_to_occupy = 1) = 0;
    virtual uint32_t allocate(uint32_t n_slots_to_occupy = 1) = 0;
    virtual void clear_obsoletes(std::shared_ptr<CAIndex> ca_index) = 0;

    Bucket *_bucket;
  };

  struct LRUExecutor : public CachePolicyExecutor {
    LRUExecutor(Bucket *bucket);

    void promote(uint32_t slot_id, uint32_t n_slots_to_occupy = 1);
    // Only LBA Index would call this function
    // LBA signature only takes one slot.
    // So there is no need to care about the entry may take contiguous slots.
    void clear_obsoletes(std::shared_ptr<CAIndex> ca_index);
    uint32_t allocate(uint32_t n_slots_to_occupy = 1);
  };

  struct CAClockExecutor : public CachePolicyExecutor {
    CAClockExecutor(Bucket *bucket, std::unique_ptr<Bucket> clock, uint32_t *clock_ptr);

    void promote(uint32_t slot_id, uint32_t n_slots_occupied = 1);
    void clear_obsoletes(std::shared_ptr<CAIndex> ca_index);
    uint32_t allocate(uint32_t n_slots_to_occupy = 1);

    inline void init_clock(uint32_t index);
    inline uint32_t get_clock(uint32_t index);
    inline void inc_clock(uint32_t index);
    inline void dec_clock(uint32_t index);

    std::unique_ptr<Bucket> _clock;
    uint32_t *_clock_ptr;
  };

      


  class CachePolicy {
    public:
      virtual CachePolicyExecutor* get_executor(Bucket *bucket) = 0;

      CachePolicy();
  };

  class LRU : public CachePolicy {
    public:
      LRU();
        
      CachePolicyExecutor* get_executor(Bucket *bucket);
  };

  /**
   * @brief CAClock - Compression-Aware Clock Cache Eviction Algorithm
   *        The allocation should take care of compressibility of each
   *        new item.
   */
  class CAClock : public CachePolicy {
    public:
      CAClock(uint32_t n_slots_per_bucket, uint32_t n_buckets);

      CachePolicyExecutor* get_executor(Bucket *bucket);

      std::unique_ptr< Buckets > _clock;
      uint32_t _clock_ptr;
  };
}
#endif
