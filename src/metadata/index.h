/* File: metadata/index.h
 * Description:
 *   This file contains declarations of our designed LBAIndex and FPIndex.
 *
 *   1. Each Index instance manages the memory of bucket slots mappings, bucket valid bits,
 *      and mutexes, and corresponding cache policy functions.
 *   2. Bucket access is in the form of functions with pointers to slots, valid bits, and mutex.
 *      Index implements a getBucketManipulator function that wraps and returns a bucket manipulator.
 *   3. Index exposes lookup, promote, and update for caller to query/update the index structure,
 *      it also expose mutex lock and unlock for concurrency control.
 */
#ifndef __INDEX_H__
#define __INDEX_H__
#include <iostream>
#include <cstring>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <list>
#include "bucket.h"
#include "cachePolicies/CachePolicy.h"
#include "common/config.h"
#include "metadata/cacheDedup/cacheDedupCommon.h"
namespace cache {
  class Index {
    public:
      Index();
      ~Index() = default;

      //virtual bool lookup(uint8_t *key, uint8_t *value) = 0;
      //virtual void set(uint8_t *key, uint8_t *value) = 0;

      void setCachePolicy(std::unique_ptr<CachePolicy> cachePolicy);
    protected:
      uint32_t nBitsPerSlot_{}, nSlotsPerBucket_{},
               nBitsPerKey_{}, nBitsPerValue_{},
               nBytesPerBucket_{}, nBuckets_{},
               nBytesPerBucketForValid_{};
      std::unique_ptr< uint8_t[] > data_;
      std::unique_ptr< uint8_t[] > valid_;
      std::unique_ptr< CachePolicy > cachePolicy_;
      std::unique_ptr< std::mutex[] > mutexes_;
  };

  class FPIndex;
  class LBAIndex : Index {
    public:
      explicit LBAIndex(std::shared_ptr<FPIndex> fpIndex);
      ~LBAIndex();
      bool lookup(uint64_t lbaHash, uint64_t &fpHash);
      void promote(uint64_t lbaHash);
      uint64_t update(uint64_t lbaHash, uint64_t fpHash);
      std::unique_ptr<std::lock_guard<std::mutex>> lock(uint64_t lbaHash);

      std::unique_ptr<LBABucket> getLBABucket(uint32_t bucketId)
      {
        return std::move(std::make_unique<LBABucket>(
            nBitsPerKey_, nBitsPerValue_, nSlotsPerBucket_,
            data_.get() + nBytesPerBucket_ * bucketId,
            valid_.get() + nBytesPerBucketForValid_ * bucketId,
            cachePolicy_.get(), bucketId));
      }

      void getFingerprints(std::set<uint64_t> &fpSet);
    private:
      std::shared_ptr<FPIndex> fpIndex_;
  };

  class FPIndex : Index {
    public:
      // n_bits_per_key = 12, n_bits_per_value = 4
      FPIndex();
      ~FPIndex();
      bool lookup(uint64_t fpHash, uint32_t &compressedLevel, uint64_t &cachedataLocation, uint64_t &metadataLocation);
      void promote(uint64_t fpHash);
      void update(uint64_t fpHash, uint32_t compressedLevel, uint64_t &cachedataLocation, uint64_t &metadataLocation);
      std::unique_ptr<std::lock_guard<std::mutex>> lock(uint64_t fpHash);

      void getFingerprints(std::set<uint64_t> &fpSet);

      std::unique_ptr<FPBucket> getFPBucket(uint32_t bucketId) {
        return std::move(std::make_unique<FPBucket>(
            nBitsPerKey_, nBitsPerValue_, nSlotsPerBucket_,
            data_.get() + nBytesPerBucket_ * bucketId,
            valid_.get() + nBytesPerBucketForValid_ * bucketId,
            cachePolicy_.get(), bucketId));
      }
      static uint64_t computeCachedataLocation(uint32_t bucketId, uint32_t slotId);
      static uint64_t computeMetadataLocation(uint32_t bucketId, uint32_t slotId);

      void reference(uint64_t fpHash);
      void dereference(uint64_t fpHash);
      std::map<uint64_t, uint32_t> referenceMap_;

      //std::map< uint8_t [], std::pair<uint32_t> > collide_fingeprints;
  };
}
#endif
