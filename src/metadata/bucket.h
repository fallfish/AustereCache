#ifndef __BUCKET_H__
#define __BUCKET_H__
#include <cstdint>
#include <iostream>
#include <memory>
//#include "index.h"
namespace cache {
  // Bucket is an abstraction of multiple key-value pairs (mapping)
  // bit level bucket
  class CAIndex;
  class Bucket {
    public:
      Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items);
      virtual ~Bucket();
      //virtual int find(uint32_t key, uint32_t value) = 0;
      //virtual void update(uint32_t key, uint32_t value) = 0;

    protected:
      inline void bits_extract(uint32_t b, uint32_t e, uint32_t &v) {
        uint8_t shift = 0;
        if (b & 7) {
          v |= _data[b >> 3] >> (b & 7);
          shift += 8 - (b & 7);
          b = ((b >> 3) + 1) << 3;
        }
        while (b < e) {
          v |= _data[b >> 3] << shift;
          shift += 8;
          b += 8;
        }
        if (b > e) {
          v &= (1 << (shift - b + e)) - 1;
        }
      }
      inline void bits_encode(uint32_t b, uint32_t e, uint32_t v) {
        if (b & 7) {
          _data[b >> 3] &= ((1 << (b & 7)) - 1);
          _data[b >> 3] |= (v & ((1 << (8 - (b & 7))) - 1)) << (b & 7);
          v >>= 8 - (b & 7);
          b = ((b >> 3) + 1) << 3;
        }
        while (b + 8 <= e) {
          _data[b >> 3] = v & 0xff;
          v >>= 8;
          b += 8;
        }
        if (b < e) {
          _data[b >> 3] &= ~((1 << (e - b)) - 1);
          _data[b >> 3] |= v & ((1 << (e - b)) - 1);
        }
      }
      inline void init_k(uint32_t index, uint32_t &b, uint32_t &e) {
        b = index * _n_bits_per_item;
        e = b + _n_bits_per_key;
      }
      inline uint32_t get_k(uint32_t index) { 
        uint32_t b, e, v = 0; init_k(index, b, e);
        bits_extract(b, e, v);
        return v;
      }
      inline void set_k(uint32_t index, uint32_t v) {
        uint32_t b, e; init_k(index, b, e);
        bits_encode(b, e, v);
      }
      inline void init_v(uint32_t index, uint32_t &b, uint32_t &e) {
        b = index * _n_bits_per_item + _n_bits_per_key;
        e = b + _n_bits_per_value;
      }
      inline uint32_t get_v(uint32_t index) { 
        uint32_t b, e, v = 0; init_v(index, b, e);
        bits_extract(b, e, v);
        return v;
      }
      inline void set_v(uint32_t index, uint32_t v) {
        uint32_t b, e; init_v(index, b, e);
        bits_encode(b, e, v);
      }
      uint32_t _n_bits_per_item, _n_items, _n_total_bytes,
               _n_bits_per_key, _n_bits_per_value;
      std::unique_ptr< uint8_t[] > _data;
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
      //int find(uint32_t key);
      //void set(uint32_t index, uint32_t key, uint32_t value);
    private:
      inline uint32_t size_to_comp_code(uint32_t size) { return size - 1; }
      inline uint32_t comp_code_to_size(uint32_t comp_code) { return comp_code + 1; }
      inline uint32_t v_to_comp_code(uint32_t v) { return v >> 2; }
      inline uint32_t size_to_v(uint32_t size) { 
        return (size_to_comp_code(size) << 2) & 1;
      }
      inline uint32_t v_to_size(uint32_t v) { 
        return comp_code_to_size(v_to_comp_code(v));
      }
      inline void clock_inc(uint32_t &v) { v = (v == 3) ? v : v + 1; }
      inline void clock_dec(uint32_t &v) { v = v - 1; }
      inline bool valid(uint32_t v) { return v & 3; }
      uint32_t find_non_occupied_position(uint32_t size);
  };
}
#endif
