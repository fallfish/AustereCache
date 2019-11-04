#include "index.h"

#include <utility>
#include "CachePolicy.h"
#include "common/config.h"
#include "common/stats.h"
#include "ReferenceCounter.h"

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
    if (Config::getInstance().isMultiThreadEnabled()) {
      mutexes_ = std::make_unique<std::mutex[]>(nBuckets_);
    }

    setCachePolicy(std::move(std::make_unique<LRU>()));
  }

  bool LBAIndex::lookup(uint64_t lbaHash, uint64_t &fpHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    uint32_t signature = lbaHash & ((1u << nBitsPerKey_) - 1);
    Stats::getInstance().add_lba_index_bucket_hit(bucketId);
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

    Stats::getInstance().add_lba_bucket_update(bucketId);

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
    if (Config::getInstance().isMultiThreadEnabled()) {
      mutexes_ = std::make_unique<std::mutex[]>(nBuckets_);
    }
    // 0 and 1 means ReferenceCount
    // 2 means CAClock
    if (Config::getInstance().getCachePolicyForFPIndex() == 2) {
      cachePolicy_ = std::move(std::make_unique<CAClock>(nSlotsPerBucket_, nBuckets_));
    } else {
      cachePolicy_ = std::move(std::make_unique<LeastReferenceCount>());
    }
  }

  uint64_t FPIndex::computeCachedataLocation(uint32_t bucketId, uint32_t slotId)
  {
    // 8192 is chunk size, while 512 is the metadata size
    return (bucketId * Config::getInstance().getnFPSlotsPerBucket() + slotId) * 1ull *
             Config::getInstance().getSectorSize() + Config::getInstance().getnFpBuckets() *
                                                     Config::getInstance().getnFPSlotsPerBucket() * Config::getInstance().getMetadataSize();
  }

  uint64_t FPIndex::computeMetadataLocation(uint32_t bucketId, uint32_t slotId)
  {
    return (bucketId * Config::getInstance().getnFPSlotsPerBucket() + slotId) * 1ull * Config::getInstance().getMetadataSize();
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

    Stats::getInstance().addFPBucketLookup(bucketId);
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

    Stats::getInstance().add_fp_bucket_update(bucketId);
    uint32_t slotId = getFPBucket(bucketId)->update(signature, nSlotsToOccupy);
    cachedataLocation = computeCachedataLocation(bucketId, slotId);
    metadataLocation = computeMetadataLocation(bucketId, slotId);
  }

  std::unique_ptr<std::lock_guard<std::mutex>> LBAIndex::lock(uint64_t lbaHash)
  {
    uint32_t bucketId = lbaHash >> nBitsPerKey_;
    if (Config::getInstance().isMultiThreadEnabled()) {
      return std::move(
          std::make_unique<std::lock_guard<std::mutex>>(
            mutexes_[bucketId]));
    } else {
      return nullptr;
    }
  }

  std::unique_ptr<std::lock_guard<std::mutex>> FPIndex::lock(uint64_t fpHash)
  {
    uint32_t bucketId = fpHash >> nBitsPerKey_;
    if (Config::getInstance().isMultiThreadEnabled()) {
      return std::move(
          std::make_unique<std::lock_guard<std::mutex>>(
            mutexes_[bucketId]));
    } else {
      return nullptr;
    }
  }

  void FPIndex::reference(uint64_t fpHash) {
    if (Config::getInstance().getCachePolicyForFPIndex() != 2) {
      if (Config::getInstance().getCachePolicyForFPIndex() == 0) {
        SketchReferenceCounter::getInstance().reference(fpHash);
      } else {
        MapReferenceCounter::getInstance().reference(fpHash);
      }
    }
  }
  void FPIndex::dereference(uint64_t fpHash) {
    if (Config::getInstance().getCachePolicyForFPIndex() != 2) {
      if (Config::getInstance().getCachePolicyForFPIndex() == 0) {
        SketchReferenceCounter::getInstance().dereference(fpHash);
      } else {
        MapReferenceCounter::getInstance().dereference(fpHash);
      }
    }
  }
}
