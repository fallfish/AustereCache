#ifndef __BUCKET_H__
#define __BUCKET_H__
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include "bitmap.h"
namespace cache {
  // Bucket is an abstraction of multiple key-value pairs (mapping)
  // bit level bucket
  class CAIndex;
  class Bucket {
    public:
      Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots);
      virtual ~Bucket();
      std::mutex &get_mutex() { return _mutex; }

      inline void init_k(uint32_t index, uint32_t &b, uint32_t &e) {
        b = index * _n_bits_per_slot;
        e = b + _n_bits_per_key;
      }
      inline uint32_t get_k(uint32_t index) { 
        uint32_t b, e; init_k(index, b, e);
        return _data->get_bits(b, e);
      }
      inline void set_k(uint32_t index, uint32_t v) {
        uint32_t b, e; init_k(index, b, e);
        _data->store_bits(b, e, v);
      }
      inline void init_v(uint32_t index, uint32_t &b, uint32_t &e) {
        b = index * _n_bits_per_slot + _n_bits_per_key;
        e = b + _n_bits_per_value;
      }
      inline uint32_t get_v(uint32_t index) { 
        uint32_t b, e; init_v(index, b, e);
        return _data->get_bits(b, e);
      }
      inline void set_v(uint32_t index, uint32_t v) {
        uint32_t b, e; init_v(index, b, e);
        _data->store_bits(b, e, v);
      }
     protected:
      uint32_t _n_bits_per_slot, _n_slots, _n_total_bytes,
               _n_bits_per_key, _n_bits_per_value;
      std::unique_ptr< Bitmap > _data;
      std::mutex _mutex;
  };

  /**
   *  @brief: LBABucket: store multiple entries
   *                     (lba signature -> ca hash) pair
   *
   *          layout:
   *          the key-value table (Bucket : Bitmap) (12 + 12 + x)  * 32 bits 
   *            key: 12 bit lba hash prefix (signature)
   *            value: (12 + x) bit ca hash
   *            -------------------------------------------
   *            | 12 bit signature | (12 + x) bit ca hash |
   *            -------------------------------------------
   */
  class LBABucket : public Bucket {
    public:
      LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots);
      ~LBABucket();
      /**
       * @brief Lookup the given lba signature and store the ca hash result into ca_hash
       *
       * @param lba_sig, lba signature, default 12 bit to achieve < 1% error rate
       * @param ca_hash, ca hash, used to lookup ca index
       *
       * @return ~0 if the lba signature does not exist, otherwise the corresponding index
       */
      uint32_t lookup(uint32_t lba_sig, uint32_t &ca_hash);
      /**
       * @brief Update the lba index structure
       *        In the eviction procedure, we firstly check whether old
       *        entries has been invalid because of evictions in ca_index.
       *        LRU evict kicks in only after evicting old obsolete mappings.
       *
       * @param lba_sig
       * @param ca_hash
       * @param ca_index used to evict obselete entries that has been evicted in ca_index
       */
      void update(uint32_t lba_sig, uint32_t ca_hash, std::shared_ptr<CAIndex> ca_index);

    private:
      void advance(uint32_t index);
      uint32_t find_non_occupied_position(std::shared_ptr<CAIndex> ca_index);
      std::unique_ptr<Bitmap> _valid;
  };

  /**
   * @brief CABucket: store multiple (ca signature -> cache device data pointer) pair
   *          Note: data pointer is computed according to alignment rather than a value
   *                to save memory usage and meantime nicely align with cache device blocks
   *        
   *        layout:
   *        1. the key-value table (Bucket : Bitmap) 12 * 32 = 384 bits (regardless of alignment)
   *          key: 12 bit ca hash value prefix
   *          value: 0 bit
   *          --------------------
   *          | 12 bit signature |
   *          --------------------
   *        2. valid bits - _valid (Bitmap) 32 bits
   *        3. clock bits - _clock (Bucket : Bitmap) 2 * 32 bits (key_len : 0, value_len : 2)
   *        4. space bits - _space (Bucket : Bitmap) 2 * 32 bits (key_len : 0, value_len : 2)
   *           0, 1, 2, 3 represents the compression level 0, 1, 2, 3
   *           (also called size of an item)
   *           helps to compute a data pointer to the cache device
   */
  class CABucket : public Bucket {
    public:
      CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots);
      ~CABucket();

      /**
       * @brief Lookup the given ca signature and store the space (compression level) to size
       *
       * @param ca_sig
       * @param size
       *
       * @return ~0 if the lba signature does not exist, otherwise the corresponding index
       */
      uint32_t lookup(uint32_t ca_sig, uint32_t &size);

      /**
       * @brief Update the lba index structure
       *
       * @param lba_sig
       * @param size
       */
      uint32_t update(uint32_t ca_sig, uint32_t size);
      // Delete an entry for a certain ca signature
      // This is required for hit but verification-failed chunk.
      void erase(uint32_t ca_sig);
    private:
      inline uint32_t get_space(uint32_t index) {
        return _space->get_v(index);
      }
      inline void set_space(uint32_t index, uint32_t space) {
        _space->set_v(index, space);
      }

      inline void init_clock(uint32_t index) {
        _clock->set_v(index, 1);
      }
      inline uint32_t get_clock(uint32_t index) {
        return _clock->get_v(index);
      }
      inline void inc_clock(uint32_t index) {
        uint32_t v = _clock->get_v(index);
        if (v != 3) {
          _clock->set_v(index, v + 1);
        }
      }
      inline void dec_clock(uint32_t index) { 
        uint32_t v = _clock->get_v(index);
        if (v != 0) {
          _clock->set_v(index, v - 1);
        }
      }

      inline bool is_valid(uint32_t index) { return _valid->get(index); }
      inline void set_valid(uint32_t index) { _valid->set(index); }
      inline void set_invalid(uint32_t index) { _valid->clear(index); }

      uint32_t find_non_occupied_position(uint32_t size);

      uint32_t _clock_ptr;
      std::unique_ptr<Bitmap> _valid;
      std::unique_ptr<Bucket> _clock;
      std::unique_ptr<Bucket> _space;
  };

  
  /**
   * @brief BlockBucket enlarges the number of slots.
   *        This is to address the random write problem. Inside one bucket,
   *        slots are grouped into "erase" blocks. Write (Persist to SSD)
   *        and Eviction is based on units of one block.
   *
   *        layout:
   *        1. Firstly we have an indexing structure from (ca signature) -> (index)
   *           This indexing structure needs to be fast:
   *           a. Binary Search Tree - O(logN) update/delete/lookup
   *           b. Sorted Array - O(N) update/delete, O(logN) lookup
   *           c. SkipList - O(logN)
   *           d. Cuckoo Hash - O(1)
   *           This indexing structure needs to be memory efficient:
   *           a. Binary Search Tree, If not balanced, the memory provision needs to be considered
   *           b. Sorted Array, no additional memory overhead
   *           c. SkipList - cannot be pointerless
   *           d. Cuckoo Hash - good candidate
   *        2. Memory Overhead (mainly inside the indexing structure)
   *           For a bucket with 0.5 Million slots (19 bits), 
   *           the signature length should be 28 bits (9 bits to tolerant the collision 1/512),
   *           the indexing pointer should be 19 bits,
   *           the compressibility level costs 2 bits,
   *           the reference count is 0.5 bits,
   *           the bitmap for validation is 1 bit
   *           in total 50.5 bits + some additional overhead for indexing structure. ~8 bytes
   *
   */
  //class BlockBucket : Bucket {
    //public:
      //BlockBucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots);
      //~BlockBucket();

      /**
       * @brief Lookup the given ca signature and store the space (compression level) to size
       *
       * @param ca_sig
       * @param size
       *
       * @return ~0 if the lba signature does not exist, otherwise the corresponding index
       */
      //int lookup(uint32_t ca_sig, uint32_t &size);

      /**
       * @brief Update the lba index structure
       *
       * @param lba_sig
       * @param size
       */
      //void update(uint32_t ca_sig, uint32_t size);
    //private:
      //// An in-memory buffer for one block updates


      //uint32_t _current_block_no;
      //uint32_t _n_slots_per_block;
      //std::unique_ptr<uint16_t []> _reference_counts;
      //BitmapCuckooHashTable _index;
      //std::shared_ptr<IOModule> _io_module;

      //// anxiliary indexing structure to solve collisions
      //// from full CA to slot number
      //std::map<uint8_t [], uint32_t> _anxiliary_list;
  //};
}
#endif
