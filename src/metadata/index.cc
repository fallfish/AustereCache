#include "index.h"

#include <utility>
#include "cache_policy.h"
#include "common/config.h"
#include "common/stats.h"
namespace cache {

  Index::Index(uint32_t nBitsPerKey, uint32_t nBitsPerValue,
      uint32_t nSlotsPerBucket, uint32_t nBuckets) :
    nBitsPerKey_(nBitsPerKey), nBitsPerValue_(nBitsPerValue),
    nBitsPerSlot_(nBitsPerKey + nBitsPerValue), nSlotsPerBucket_(nSlotsPerBucket),
    nBytesPerBucket_(((nBitsPerKey + nBitsPerValue) * nSlotsPerBucket + 7) / 8),
    nBytesPerBucketForValid_((1 * nSlotsPerBucket + 7) / 8),
    nBuckets_(nBuckets),
    data_(std::make_unique<uint8_t[]>(nBytesPerBucket_ * nBuckets)),
    valid_(std::make_unique<uint8_t[]>(nBytesPerBucketForValid_ * nBuckets)),
    mutexes_(std::make_unique<std::mutex[]>(nBuckets))
  {}

  LBAIndex::~LBAIndex() = default;
  FPIndex::~FPIndex() = default;

  void Index::setCachePolicy(std::unique_ptr<CachePolicy> cachePolicy)
  { 
    cachePolicy_ = std::move(cachePolicy);
  }

  LBAIndex::LBAIndex(uint32_t nBitsPerKey, uint32_t nBitsPerValue,
      uint32_t nSlotsPerBucket, uint32_t nBuckets,
      std::shared_ptr<FPIndex> fpIndex) :
    Index(nBitsPerKey, nBitsPerValue, nSlotsPerBucket, nBuckets),
    fpIndex_(std::move(fpIndex))
  {
    setCachePolicy(std::move(std::make_unique<LRU>()));
  }

  bool LBAIndex::lookup(uint64_t lbaHash, uint64_t &fpHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    uint32_t signature = lbaHash & ((1u << nBitsPerKey_) - 1);
    Stats::getInstance()->add_lba_index_bucket_hit(bucketId);
    return getLBABucket(bucketId)->lookup(signature, fpHash) != ~((uint32_t)0);
  }

  void LBAIndex::promote(uint64_t lbaHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    uint32_t signature = lbaHash & ((1u << nBitsPerKey_) - 1);
    getLBABucket(bucketId)->promote(signature);
  }

  void LBAIndex::update(uint64_t lbaHash, uint64_t fpHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    uint32_t signature = lbaHash & ((1u << nBitsPerKey_) - 1);
    getLBABucket(bucketId)->update(signature, fpHash, fpIndex_);

    Stats::getInstance()->add_lba_bucket_update(bucketId);
  }

  FPIndex::FPIndex(uint32_t nBitsPerKey, uint32_t nBitsPerValue,
      uint32_t nSlotsPerBucket, uint32_t nBuckets) :
    Index(nBitsPerKey, nBitsPerValue, nSlotsPerBucket, nBuckets)
  {
    cachePolicy_ = std::move(std::make_unique<CAClock>(nSlotsPerBucket, nBuckets));
  }

  uint64_t FPIndex::computeCachedataLocation(uint32_t bucketId, uint32_t slotId)
  {
    // 8192 is chunk size, while 512 is the metadata size
    return (bucketId * Config::getInstance()->getnFPSlotsPerBucket() + slotId) * 1ull *
             Config::getInstance()->getSectorSize() + (uint64_t)(1ull << Config::getInstance()->getnBitsPerFPBucketId()) *
      Config::getInstance()->getnFPSlotsPerBucket() * Config::getInstance()->getMetadataSize();
  }

  uint64_t FPIndex::computeMetadataLocation(uint32_t bucketId, uint32_t slotId)
  {
    return (bucketId * Config::getInstance()->getnFPSlotsPerBucket() + slotId) * 1ull * Config::getInstance()->getMetadataSize();
  }

  bool FPIndex::lookup(uint64_t fpHash, uint32_t &compressedLevel, uint64_t &cachedataLocation, uint64_t &metadataLocation)
  {
    uint32_t bucketId = fpHash >> nBitsPerKey_,
             signature = fpHash & ((1u << nBitsPerKey_) - 1),
             nSlotsOccupied = 0;
    uint32_t index = getFPBucket(bucketId)->lookup(signature, nSlotsOccupied);
    if (index == ~0u) return false;

    compressedLevel = nSlotsOccupied - 1;
    cachedataLocation = computeCachedataLocation(bucketId, index);
    metadataLocation = computeMetadataLocation(bucketId, index);

    Stats::getInstance()->addFPBucketLookup(bucketId);
    return true;
  }

  void FPIndex::promote(uint64_t fpHash)
  {
    uint32_t bucketId = fpHash >> nBitsPerKey_,
             signature = fpHash & ((1u << nBitsPerKey_) - 1);
    getFPBucket(bucketId)->promote(signature);
  }

  void FPIndex::update(uint64_t fpHash, uint32_t compressedLevel, uint64_t &cachedataLocation, uint64_t &metadataLocation)
  {
    uint32_t bucketId = fpHash >> nBitsPerKey_,
             signature = fpHash & ((1u << nBitsPerKey_) - 1),
             nSlotsToOccupy = compressedLevel + 1;

    Stats::getInstance()->add_fp_bucket_update(bucketId);
    uint32_t slotId = getFPBucket(bucketId)->update(signature, nSlotsToOccupy);
    cachedataLocation = computeCachedataLocation(bucketId, slotId);
    metadataLocation = computeMetadataLocation(bucketId, slotId);
  }

  std::unique_ptr<std::lock_guard<std::mutex>> LBAIndex::lock(uint64_t lbaHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          mutexes_[bucketId]));
  }

  std::unique_ptr<std::lock_guard<std::mutex>> FPIndex::lock(uint64_t fpHash)
  {
    uint32_t bucketId = fpHash >> nBitsPerKey_;
    return std::move(
        std::make_unique<std::lock_guard<std::mutex>>(
          mutexes_[bucketId]));
  }

}
