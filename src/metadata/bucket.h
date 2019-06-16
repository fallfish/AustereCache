#ifndef __BUCKET_H__
#define __BUCKET_H__
#include <cstdint>
#include <iostream>
#include <memory>
#include "bitmap.h"
//#include "index.h"
namespace cache {
  // Bucket is an abstraction of multiple key-value pairs (mapping)
  // bit level bucket
  class CAIndex;
  class Bucket {
    public:
      Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items);
      virtual ~Bucket();
    protected:
      inline void init_k(uint32_t index, uint32_t &b, uint32_t &e) {
        b = index * _n_bits_per_item;
        e = b + _n_bits_per_key;
      }
      inline uint32_t get_k(uint32_t index) { 
        uint32_t b, e; init_k(index, b, e);
        return _bits->get_bits(b, e);
      }
      inline void set_k(uint32_t index, uint32_t v) {
        uint32_t b, e; init_k(index, b, e);
        _bits->store_bits(b, e, v);
      }
      inline void init_v(uint32_t index, uint32_t &b, uint32_t &e) {
        b = index * _n_bits_per_item + _n_bits_per_key;
        e = b + _n_bits_per_value;
      }
      inline uint32_t get_v(uint32_t index) { 
        uint32_t b, e; init_v(index, b, e);
        return _bits->get_bits(b, e);
      }
      inline void set_v(uint32_t index, uint32_t v) {
        uint32_t b, e; init_v(index, b, e);
        _bits->store_bits(b, e, v);
      }
      uint32_t _n_bits_per_item, _n_items, _n_total_bytes,
               _n_bits_per_key, _n_bits_per_value;
      std::unique_ptr< Bitmap > _bits;
  };

  /*
   * LBABucket: store multiple entries
   *            (lba signature -> ca signature + ca bucket index) pair
   *   find (ca signature + ca bucket index) given lba signature
   *   insert lba signature to (ca signature + ca bucket index)
   */
  class LBABucket : public Bucket {
    public:
      LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items);
      ~LBABucket();
      uint32_t lookup(uint32_t lba_sig, uint32_t &ca_hash);
      // In the eviction procedure, we firstly check whether old
      // entries has been invalid because of evictions in ca_index.
      // LRU evict kicks in only after evicting old obsolete mappings.
      void update(uint32_t lba_sig, uint32_t ca_hash, std::shared_ptr<CAIndex> ca_index);

    private:
      void advance(uint32_t index);
      uint32_t find_non_occupied_position(std::shared_ptr<CAIndex> ca_index);
      std::unique_ptr<Bitmap> _valid_bitmap;
  };

  /*
   * CABucket: store multiple (ca signature -> ca bucket) pair
   */
  class CABucket : public Bucket {
    public:
      CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items);
      ~CABucket();

      int lookup(uint32_t ca_sig, uint32_t &size);
      void update(uint32_t ca_sig, uint32_t size);
      void erase();
    private:
      inline uint32_t size_to_comp_code(uint32_t size) { return size - 1; }
      inline uint32_t comp_code_to_size(uint32_t comp_code) { return comp_code + 1; }
      inline uint32_t v_to_comp_code(uint32_t v) { return v >> 2; }
      inline uint32_t size_to_v(uint32_t size) { 
        return (size_to_comp_code(size) << 2) | 1;
      }
      inline uint32_t v_to_size(uint32_t v) { 
        return comp_code_to_size(v_to_comp_code(v));
      }
      inline void clock_inc(uint32_t &v) { v = ((v & 3) == 3) ? v : v + 1; }
      inline void clock_dec(uint32_t &v) { v = ((v & 3) == 0) ? v : v - 1; }
      inline bool valid(uint32_t v) { return v & 3; }
      uint32_t find_non_occupied_position(uint32_t size);
      uint32_t _clock_ptr;
  };
}
#endif
