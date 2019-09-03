#ifndef __CACHE_POLICY_H__
#define __CACHE_POLICY_H__

#include <memory>
#include "bucket.h"
namespace cache {
  class FPIndex;

  struct CachePolicyExecutor {
    CachePolicyExecutor(Bucket *bucket);

    virtual void promote(uint32_t slot_id, uint32_t n_slots_to_occupy = 1) = 0;
    virtual uint32_t allocate(uint32_t n_slots_to_occupy = 1) = 0;
    virtual void clear_obsoletes(std::shared_ptr<FPIndex> ca_index) = 0;

    Bucket *_bucket;
  };

  struct LRUExecutor : public CachePolicyExecutor {
    LRUExecutor(Bucket *bucket);

    void promote(uint32_t slot_id, uint32_t n_slots_to_occupy = 1);
    // Only LBA Index would call this function
    // LBA signature only takes one slot.
    // So there is no need to care about the entry may take contiguous slots.
    void clear_obsoletes(std::shared_ptr<FPIndex> ca_index);
    uint32_t allocate(uint32_t n_slots_to_occupy = 1);
  };

  struct CAClockExecutor : public CachePolicyExecutor {
    CAClockExecutor(Bucket *bucket, std::shared_ptr<Bucket> clock, uint32_t *clock_ptr);
    ~CAClockExecutor();

    void promote(uint32_t slot_id, uint32_t n_slots_occupied = 1);
    void clear_obsoletes(std::shared_ptr<FPIndex> ca_index);
    uint32_t allocate(uint32_t n_slots_to_occupy = 1);

    inline void init_clock(uint32_t index);
    inline uint32_t get_clock(uint32_t index);
    inline void inc_clock(uint32_t index);
    inline void dec_clock(uint32_t index);

    std::shared_ptr<Bucket> _clock;
    uint32_t *_clock_ptr;
  };


  class CachePolicy {
    public:
      virtual std::shared_ptr<CachePolicyExecutor> get_executor(Bucket *bucket) = 0;

      CachePolicy();
  };

  class LRU : public CachePolicy {
    public:
      LRU();
        
      std::shared_ptr<CachePolicyExecutor> get_executor(Bucket *bucket);
  };

  /**
   * @brief CAClock - Compression-Aware Clock Cache Eviction Algorithm
   *        The allocation should take care of compressibility of each
   *        new item.
   */
  class CAClock : public CachePolicy {
    public:
      CAClock(uint32_t n_slots_per_bucket, uint32_t n_buckets);

      std::shared_ptr<CachePolicyExecutor> get_executor(Bucket *bucket);

      std::shared_ptr<Bucket> get_bucket(uint32_t bucket_id) {
        return std::make_shared<Bucket>(
            0, 2, _n_slots_per_bucket,
            _clock.get() + _n_bytes_per_bucket * bucket_id, 
            nullptr,
            nullptr, bucket_id);
      }


      std::unique_ptr<uint8_t []> _clock;
      uint32_t _n_slots_per_bucket;
      uint32_t _n_bytes_per_bucket;
      uint32_t _clock_ptr;
  };
}
#endif
