#ifndef __CACHE_POLICY_H__
#define __CACHE_POLICY_H__

#include <memory>
namespace cache {
  class Bucket;
  class CAIndex;
  class CachePolicy {
    public:

      virtual void promote(uint32_t slot_id, uint32_t n_slots_to_occupy = 1) = 0;
      virtual uint32_t allocate(uint32_t n_slots_to_occupy = 1) = 0;
      virtual void clear_obsoletes(std::shared_ptr<CAIndex> ca_index) = 0;

    protected:
      CachePolicy(std::shared_ptr<Bucket> bucket);
      std::shared_ptr<Bucket> _bucket;
  };

  class LRU : public CachePolicy {
    public:
      LRU(std::shared_ptr<Bucket> bucket);
        
      void promote(uint32_t slot_id, uint32_t n_slots_to_occupy = 1);
      // Only LBA Index would call this function
      // LBA signature only takes one slot.
      // So there is no need to care about the entry may take contiguous slots.
      void clear_obsoletes(std::shared_ptr<CAIndex> ca_index);
      uint32_t allocate(uint32_t n_slots_to_occupy = 1);

    private:
  };

  /**
   * @brief CAClock - Compression-Aware Clock Cache Eviction Algorithm
   *        The allocation should take care of compressibility of each
   *        new item.
   */
  class CAClock : public CachePolicy {
    public:
      CAClock(std::shared_ptr<Bucket> bucket);

      void promote(uint32_t slot_id, uint32_t n_slots_occupied = 1);
      void clear_obsoletes(std::shared_ptr<CAIndex> ca_index);
      uint32_t allocate(uint32_t n_slots_to_occupy = 1);

    private:
      inline void init_clock(uint32_t index);
      inline uint32_t get_clock(uint32_t index);
      inline void inc_clock(uint32_t index);
      inline void dec_clock(uint32_t index);

      std::unique_ptr<Bucket> _clock;
      uint32_t _clock_ptr;
  };
}
#endif
