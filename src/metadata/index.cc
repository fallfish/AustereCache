#include "index.h"

#include <utility>
#include "common/config.h"
#include "common/stats.h"
#include "reference_counter.h"
#include "cache_policies/lru.h"
#include "cache_policies/bucket_aware_lru.h"
#include "cache_policies/least_reference_count.h"

namespace cache {

  Index::Index() = default;

  LBAIndex::~LBAIndex() = default;
  FPIndex::~FPIndex() = default;

  void Index::setCachePolicy(std::unique_ptr<CachePolicy> cachePolicy)
  { 
    cachePolicy_ = std::move(cachePolicy);
  }

  LBAIndex::LBAIndex(std::shared_ptr<FPIndex> fpIndex):
    fpIndex_(std::move(fpIndex))
  {
    nBitsPerKey_ = Config::getInstance().getnBitsPerLbaSignature();
    nBitsPerValue_ = Config::getInstance().getnBitsPerFpSignature() + Config::getInstance().getnBitsPerFpBucketId();
    nSlotsPerBucket_ = Config::getInstance().getnLBASlotsPerBucket();
    nBuckets_ = Config::getInstance().getnLbaBuckets();

    nBytesPerBucket_ = ((nBitsPerKey_ + nBitsPerValue_) * nSlotsPerBucket_ + 7) / 8;
    nBytesPerBucketForValid_ = (1 * nSlotsPerBucket_ + 7) / 8;
    data_ = std::make_unique<uint8_t[]>(nBytesPerBucket_ * nBuckets_ + 1);
    valid_ = std::make_unique<uint8_t[]>(nBytesPerBucketForValid_ * nBuckets_ + 1);
    if (Config::getInstance().isMultiThreadingEnabled()) {
      mutexes_ = std::make_unique<std::mutex[]>(nBuckets_);
    }

    if (Config::getInstance().isCompactCachePolicyEnabled()) {
      setCachePolicy(std::move(std::make_unique<BucketAwareLRU>()));
    } else {
      // setting the cache policy for fp index as LRU means that we disable the acdc cache policy
      setCachePolicy(std::move(std::make_unique<LRU>(nBuckets_)));
    }
  }

  bool LBAIndex::lookup(uint64_t lbaHash, uint64_t &fpHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    uint32_t signature = lbaHash & ((1u << nBitsPerKey_) - 1);
    return getLBABucket(bucketId)->lookup(signature, fpHash) != ~((uint32_t)0);
  }

  void LBAIndex::promote(uint64_t lbaHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    uint32_t signature = lbaHash & ((1u << nBitsPerKey_) - 1);
    getLBABucket(bucketId)->promote(signature);
  }

  // If the request modify an existing LBA, return the previous fingerprint
  uint64_t LBAIndex::update(uint64_t lbaHash, uint64_t fpHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    uint32_t signature = lbaHash & ((1u << nBitsPerKey_) - 1);
    uint64_t evictedFPHash = getLBABucket(bucketId)->update(signature, fpHash, fpIndex_);

    return evictedFPHash;
  }

  FPIndex::FPIndex()
  {
    nBitsPerKey_ = Config::getInstance().getnBitsPerFpSignature();
    nBitsPerValue_ = 4;
    nSlotsPerBucket_ = Config::getInstance().getnFPSlotsPerBucket();
    nBuckets_ = Config::getInstance().getnFpBuckets();

    nBytesPerBucket_ = ((nBitsPerKey_ + nBitsPerValue_) * nSlotsPerBucket_ + 7) / 8;
    nBytesPerBucketForValid_ = (1 * nSlotsPerBucket_ + 7) / 8;
    data_ = std::make_unique<uint8_t[]>(nBytesPerBucket_ * nBuckets_ + 1);
    valid_ = std::make_unique<uint8_t[]>(nBytesPerBucketForValid_ * nBuckets_ + 1);
    if (Config::getInstance().isMultiThreadingEnabled()) {
      mutexes_ = std::make_unique<std::mutex[]>(nBuckets_);
    }

    if (Config::getInstance().isCompactCachePolicyEnabled()) {
      cachePolicy_ = std::move(std::make_unique<LeastReferenceCount>());
    } else {
      cachePolicy_ = std::move(std::make_unique<LRU>(nBuckets_));
    }
  }

  uint64_t FPIndex::computeCachedataLocation(uint32_t bucketId, uint32_t slotId)
  {
    return (bucketId * Config::getInstance().getnFPSlotsPerBucket() + slotId) *
      1ull * Config::getInstance().getSectorSize() + 1ull *
      Config::getInstance().getnFpBuckets() *
      Config::getInstance().getnFPSlotsPerBucket() *
      Config::getInstance().getMetadataSize();
  }

  uint64_t FPIndex::computeMetadataLocation(uint32_t bucketId, uint32_t slotId)
  {
    return (bucketId * Config::getInstance().getnFPSlotsPerBucket() + slotId)
      * 1ull * Config::getInstance().getMetadataSize();
  }

  uint64_t FPIndex::cachedataLocationToMetadataLocation(uint64_t cachedataLocation)
  {
    return (cachedataLocation - 1ull *
        Config::getInstance().getnFPSlotsPerBucket() *
        Config::getInstance().getMetadataSize() *
        Config::getInstance().getnFpBuckets()) /
      Config::getInstance().getSectorSize() *
      Config::getInstance().getMetadataSize();
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

    uint32_t slotId = getFPBucket(bucketId)->update(signature, nSlotsToOccupy);
    cachedataLocation = computeCachedataLocation(bucketId, slotId);
    metadataLocation = computeMetadataLocation(bucketId, slotId);
  }

  std::unique_ptr<std::lock_guard<std::mutex>> LBAIndex::lock(uint64_t lbaHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    if (Config::getInstance().isMultiThreadingEnabled()) {
      return std::move(
          std::make_unique<std::lock_guard<std::mutex>>(
            mutexes_[bucketId]));
    } else {
      return nullptr;
    }
  }

  void LBAIndex::getFingerprints(std::set<uint64_t> &fpSet) {
    for (uint32_t i = 0; i < nBuckets_; ++i) {
      getLBABucket(i)->getFingerprints(fpSet);
    }
  }
  void FPIndex::getFingerprints(std::set<uint64_t> &fpSet) {
    for (uint32_t i = 0; i < nBuckets_; ++i) {
      getFPBucket(i)->getFingerprints(fpSet);
    }
  }

  std::unique_ptr<std::lock_guard<std::mutex>> FPIndex::lock(uint64_t fpHash)
  {
    uint32_t bucketId = fpHash >> nBitsPerKey_;
    if (Config::getInstance().isMultiThreadingEnabled()) {
      return std::move(
          std::make_unique<std::lock_guard<std::mutex>>(
            mutexes_[bucketId]));
    } else {
      return nullptr;
    }
  }

  void FPIndex::recover(uint64_t fpHash, uint64_t cachedataLocation, uint32_t nSlotsOccupied) {
    uint32_t bucketId = fpHash >> nBitsPerKey_,
             signature = fpHash & ((1u << nBitsPerKey_) - 1);
    uint32_t slotId = cachedataLocationToMetadataLocation(cachedataLocation) %
      (Config::getInstance().getMetadataSize() *
       Config::getInstance().getnFPSlotsPerBucket()) /
      Config::getInstance().getMetadataSize();
    getFPBucket(bucketId)->recover(signature, slotId, nSlotsOccupied);
  }

  void FPIndex::reference(uint64_t fpHash) {
    ReferenceCounter::getInstance().reference(fpHash);
  }
  void FPIndex::dereference(uint64_t fpHash) {
    ReferenceCounter::getInstance().dereference(fpHash);
  }

}
