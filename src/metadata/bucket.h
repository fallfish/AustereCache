/* File: metadata/bucket.h
 * Description:
 *   This file contains declarations of our designed LBABucket and FPBucket.
 *
 *   1. A base class Bucket holds bit-level manipulators to the bucket data (slots)
 *      and valid bits, and contains a cache policy implementations.
 *   2. LBABucket and FPBucket expose bucket-level lookup, promote, and lookup
 *      to the caller.
 *   3. In the current implementation, buckets **do not hold memory**.
 *      The ownership of the memory of all slots belongs to Index, which instantiate
 *      a bucket manipulator with the corresponding memory.
 */
#ifndef __BUCKET_H__
#define __BUCKET_H__
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include "bitmap.h"
namespace cache {
  // Bucket is an abstraction of multiple key-value pairs (mapping)
  class FPIndex;
  class CachePolicy;
  class CachePolicyExecutor;
  class Bucket {
    public:
      Bucket(uint32_t nBitsPerKey, uint32_t nBitsPerValue, uint32_t nSlots,
          uint8_t *data, uint8_t *valid, CachePolicy *cachePolicy, uint32_t slotId);
      virtual ~Bucket();


      inline void initKey(uint32_t index, uint32_t &b, uint32_t &e)
      {
        b = index * nBitsPerSlot_;
        e = b + nBitsPerKey_;
      }
      inline uint32_t getKey(uint32_t index)
      {
        uint32_t b, e;
        initKey(index, b, e);
        return data_.getBits(b, e);
      }
      inline void setKey(uint32_t index, uint32_t v)
      {
        uint32_t b, e;
        initKey(index, b, e);
        data_.storeBits(b, e, v);
      }
      inline void initValue(uint32_t index, uint32_t &b, uint32_t &e)
      {
        b = index * nBitsPerSlot_ + nBitsPerKey_;
        e = b + nBitsPerValue_;
      }
      inline uint64_t getValue(uint32_t index)
      {
        //uint32_t b, e; initValue(index, b, e);
        //return data_.getBits(b, e);
        uint32_t b, e;
        initValue(index, b, e);
        uint64_t v = 0;
        if (e - b > 32) {
          v = data_.getBits(b, b + 32);
          v |= (uint64_t) data_.getBits(b + 32, e) << 32u;
        } else {
          v = data_.getBits(b, e);
        }
        return v;
      }
      inline void setValue(uint32_t index, uint64_t v)
      {
        //uint32_t b, e; initValue(index, b, e);
        //data_.storeBits(b, e, v);
        uint32_t b, e;
        initValue(index, b, e);
        if (e - b > 32) {
          data_.storeBits(b, b + 32, v & 0xffffffff);
          data_.storeBits(b + 32, e, v >> 32u);
        } else {
          data_.storeBits(b, e, v);
        }
      }
      inline uint32_t get32bits(uint32_t index)
      {
        return data_.get32bits(index);
      }
      inline void set32bits(uint32_t index, uint32_t v)
      {
        data_.set32bits(index, v);
      }

      inline bool isValid(uint32_t index) {
        return valid_.get(index);
      }
      inline void setValid(uint32_t index) { valid_.set(index); }
      inline void setInvalid(uint32_t index) { valid_.clear(index); }
      inline uint32_t getValid32bits(uint32_t index) { return valid_.get32bits(index); }
      inline void setValid32bits(uint32_t index, uint32_t v) { valid_.set32bits(index, v); }

      inline uint32_t getnSlots() { return nSlots_; }
      inline uint32_t getBucketId() { return bucketId_; }
      inline void setEvictedSignature(uint64_t signature) {
        evictedSignature_ = signature;
      }

      Bitmap::Manipulator data_;
      Bitmap::Manipulator valid_;
      CachePolicyExecutor* cachePolicyExecutor_;
      uint32_t nBitsPerSlot_, nSlots_,
               nBitsPerKey_, nBitsPerValue_;
      uint32_t bucketId_;
      uint64_t evictedSignature_ = ~0ull;
  };

  /**
   *  @brief: LBABucket: store multiple entries
   *                     (lba signature -> fp hash) pair
   *
   *          layout:
   *          the key-value table (Bucket : Bitmap) (16 + 16 + x)  * 128 bits
   *            key: 16 bit lba hash prefix (signature)
   *            value: (16 + x) bit fp hash (2^x = the number of Fp buckets)
   *            -------------------------------------------
   *            | 16 bit signature | (16 + x) bit fp hash |
   *            -------------------------------------------
   */
  class LBABucket : public Bucket {
    public:
      LBABucket(uint32_t nBitsPerKey, uint32_t nBitsPerValue, uint32_t nSlots,
          uint8_t *data, uint8_t *valid, CachePolicy *cachePolicy, uint32_t bucketId) :
        Bucket(nBitsPerKey, nBitsPerValue, nSlots, data, valid, cachePolicy, bucketId)
      {
      }
      /**
       * @brief Lookup the given lba signature and store the ca hash result into fp_hash
       *
       * @param lbaSignature, lba signature, default 12 bit to achieve < 1% error rate
       * @param fpHash, ca hash, used to lookup ca index
       *
       * @return ~0 if the lba signature does not exist, otherwise the corresponding index
       */
      uint32_t lookup(uint32_t lbaSignature, uint64_t &fpHash);
      void promote(uint32_t lbaSignature);
      /**
       * @brief Update the lba index structure
       *        In the eviction procedure, we firstly check whether old
       *        entries has been invalid because of evictions in ca_index.
       *        LRU evict kicks in only after evicting old obsolete mappings.
       *
       * @param lbaSignature
       * @param fingerprintHash
       * @param fingerprintIndex used to evict obsolete entries that has been evicted in ca_index
       */
      uint64_t update(uint32_t lbaSignature, uint64_t fingerprintHash, std::shared_ptr<FPIndex> fingerprintIndex);

      void getFingerprints(std::set<uint64_t> &fpSet);
  };

  /**
   * @brief FPBucket: store multiple (ca signature -> cache device data pointer) pair
   *          Note: data pointer is computed according to alignment rather than a value
   *                to save memory usage and meantime nicely align with cache device blocks
   *        
   *        layout:
   *        1. the key-value table (Bucket : Bitmap) 16 * 128 (bits) = 256 bytes
   *          key: 16 bit fp hash value prefix
   *          value: 0 bit
   *          --------------------
   *          | 16 bit signature |
   *          --------------------
   *        2. valid bits - _valid (Bitmap) 128 bits = 16 bytes
   */
  class FPBucket : public Bucket {
    public:
      FPBucket(uint32_t nBitsPerKey, uint32_t nBitsPerValue, uint32_t nSlots,
          uint8_t *data, uint8_t *valid, CachePolicy *cachePolicy, uint32_t bucketId) :
        Bucket(nBitsPerKey, nBitsPerValue, nSlots, data, valid, cachePolicy, bucketId)
      {
      }

      /**
       * @brief Lookup the given ca signature and store the space (compression level) to size
       *
       * @param fpSignature
       * @param size
       *
       * @return ~0 if the lba signature does not exist, otherwise the corresponding index
       */
      uint32_t lookup(uint64_t fpSignature, uint32_t &nSlotsOccupied);

      void promote(uint64_t fpSignature);
      /**
       * @brief Update the lba index structure
       *
       * @param lba_sig
       * @param size
       */
      uint32_t update(uint64_t fpSignature, uint32_t nSlotsToOccupy);
      // Delete an entry for a certain ca signature
      // This is required for hit but verification-failed chunk.
      void evict(uint64_t fpSignature);

      void recover(uint64_t fpSignature, uint32_t slotId, uint32_t nSlotsOccupied);

      void getFingerprints(std::set<uint64_t> &fpSet);
  };
}
#endif
