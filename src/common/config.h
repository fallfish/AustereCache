#ifndef __CONFIG_H__
#define __CONFIG_H__
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <cassert>
namespace cache {
    struct Fingerprint {
      Fingerprint() {
        memset(v_, 0, sizeof(v_));
      }
      Fingerprint(uint8_t *v) {
        memcpy(v_, v, 20);
      }
      uint8_t v_[20]{};
      bool operator<(const Fingerprint &fp) const {
        return memcmp(v_, fp.v_, 20) < 0;
      }
      bool operator==(const Fingerprint &fp) const {
        for (uint32_t i = 0; i < 20 / 4; ++i) {
          if (((uint32_t*)v_)[i] != ((uint32_t*)fp.v_)[i])
            return false;
        }
        return true;
      }
    };

    enum CachePolicyEnum {
        // For ACDC
          tRecencyAwareLeastReferenceCount, tLRU,
        // For DLRU
          tNormal,
    };

    enum CacheModeEnum {
        tWriteThrough, tWriteBack
    };

    class Config
    {
    public:
        static Config& getInstance() {
          static Config instance;
          return instance;
        }

        void release() {
          lba2Fingerprints_.clear();
        }

        // getters
        uint32_t getChunkSize() { return chunkSize_; }
        uint32_t getSubchunkSize() { return subchunkSize_; }
        uint32_t getMetadataSize() { return metadataSize_; }
        uint32_t getFingerprintLength() { return fingerprintLen_; }
        uint64_t getPrimaryDeviceSize() { return primaryDeviceSize_; }
        uint64_t getCacheDeviceSize() { return cacheDeviceSize_; }
        uint64_t getWorkingSetSize() { return workingSetSize_; }

        uint32_t getnBitsPerLbaSignature() { return nBitsPerLbaSignature_; }
        uint32_t getnBitsPerFpSignature() { return nBitsPerFpSignature_; }

        uint32_t getnLBASlotsPerBucket() { return nSlotsPerLbaBucket_; }
        uint32_t getnFPSlotsPerBucket() { return nSlotsPerFpBucket_; }

        uint32_t getnBitsPerLbaBucketId() { return 32 - __builtin_clz(getnLbaBuckets()); }
        uint32_t getnBitsPerFpBucketId() { return 32 - __builtin_clz(getnFpBuckets()); }

        uint32_t getnLbaBuckets() {
          if (lbaAmplifier_ == 0) {
            return workingSetSize_ / (chunkSize_ * nSlotsPerLbaBucket_);
          } else {
            return std::min(workingSetSize_ / (chunkSize_ * nSlotsPerLbaBucket_),
                            (uint64_t)(cacheDeviceSize_ / (chunkSize_ * nSlotsPerLbaBucket_) * lbaAmplifier_));
          }
        }
        uint32_t getnFpBuckets() {
          return cacheDeviceSize_ / (subchunkSize_ * nSlotsPerFpBucket_);
        }

        // For CacheDedup
        uint32_t getnSourceIndexEntries() {
          if (lbaAmplifier_ == 0) {
            return workingSetSize_ / chunkSize_;
          } else {
            return std::min(workingSetSize_ / chunkSize_, (uint64_t)(cacheDeviceSize_ / chunkSize_ * lbaAmplifier_));
          }
        }
        float getLBAAmplifier() {
          return lbaAmplifier_;
        }

        // For RecencyAware - the boundary of core slots and shadow slots
        uint32_t getLBASlotSeperator() {
          return std::max((uint32_t)(getnLBASlotsPerBucket() - getnLBASlotsPerBucket() * coreSlotsSize_), 1u);
        }
        uint32_t getMaxSubchunks() { return chunkSize_ / subchunkSize_; }

        // CachePolicy:
        //
        CachePolicyEnum getCachePolicyForFPIndex() {
          return cachePolicyForFPIndex_;
        }

        uint32_t getMaxNumGlobalThreads() { return maxNumGlobalThreads_; }

        char *getCacheDeviceName() { return cacheDeviceName_; }
        char *getPrimaryDeviceName() { return primaryDeviceName_; }

        uint32_t getWeuSize() { return weuSize_; }

        // setters
        void setFingerprintLength(uint32_t ca_length) { fingerprintLen_ = ca_length; }
        void setPrimaryDeviceSize(uint64_t primary_device_size) { primaryDeviceSize_ = primary_device_size; }
        void setCacheDeviceSize(uint64_t cache_device_size) { cacheDeviceSize_ = cache_device_size; }
        void setWorkingSetSize(uint64_t working_set_size) { workingSetSize_ = working_set_size; }

        void setLBAAmplifier(float v) {
          lbaAmplifier_ = v;
        }

        void setCachePolicyForFPIndex(CachePolicyEnum v) {
          cachePolicyForFPIndex_ = v;
        }

        void setnBitsPerFpSignature (uint32_t v) { nBitsPerFpSignature_ = v; }
        void setnSlotsPerFpBucket(uint32_t v) { nSlotsPerFpBucket_ = v; }
        void setnBitsPerLbaSignature (uint32_t v) { nBitsPerLbaSignature_ = v; }
        void setnSlotsPerLbaBucket(uint32_t v) { nSlotsPerLbaBucket_ = v; }
        void setSubchunkSize(uint32_t v) { subchunkSize_ = v; }
        void setChunkSize(uint32_t v) { chunkSize_ = v; }
        void setnThreads(uint32_t v) { maxNumGlobalThreads_ = v; }

        void setCacheDeviceName(char *cache_device_name) { cacheDeviceName_ = cache_device_name; }
        void setPrimaryDeviceName(char *primary_device_name) { primaryDeviceName_ = primary_device_name; }

        void setWeuSize(uint32_t v) { weuSize_ = v; }

        // Functionality enabler
        void enableMultiThreading(bool v) { enableMultiThreading_ = v; }
        void enableDirectIO(bool v) { enableDirectIO_ = v; }
        void enableFakeIO(bool v) { enableFakeIO_ = v; }
        void enableSynthenticCompression(bool v) { enableSynthenticCompression_ = v; }
        void enableTraceReplay(bool v) { enableTraceReplay_ = v; }
        void enableSketchRF(bool v) { enableSketchRF_ = v; }
        void enableCompactCachePolicy(bool v) { enableCompactCachePolicy_ = v; }
        void setCacheMode(CacheModeEnum v) { cacheMode_ = v; }

        bool isMultiThreadingEnabled() { return enableMultiThreading_; }
        bool isDirectIOEnabled() { return enableDirectIO_; }
        bool isFakeIOEnabled() { return enableFakeIO_; }
        bool isTraceReplayEnabled() { return enableTraceReplay_; }
        bool isSynthenticCompressionEnabled() { return enableSynthenticCompression_; }
        bool isSketchRFEnabled() { return enableSketchRF_; }
        bool isCompactCachePolicyEnabled() { return enableCompactCachePolicy_; }
        CacheModeEnum getCacheMode() { return cacheMode_; }

        void setFingerprint(uint64_t lba, char *fingerprint) {
          std::lock_guard<std::mutex> lock(mutex_);
          Fingerprint fp((uint8_t*)fingerprint);
          lba2Fingerprints_[lba] = fp;
        }

        void getFingerprint(uint64_t lba, char *fingerprint) {
          std::lock_guard<std::mutex> lock(mutex_);
          if (lba2Fingerprints_.find(lba) != lba2Fingerprints_.end()) {
            memcpy(fingerprint, lba2Fingerprints_[lba].v_, sizeof(char) * fingerprintLen_);
            lba2Fingerprints_.erase(lba);
          }
        }

    private:
        Config() {
          // Initialize default configuration
          chunkSize_ = 8192 * 4;
          subchunkSize_ = 8192;
          metadataSize_ = 512;
          fingerprintLen_ = 20;
          primaryDeviceSize_ = 1024 * 1024 * 1024LL * 20;
          cacheDeviceSize_ = 1024 * 1024 * 1024LL * 20;
          workingSetSize_ = 1024 * 1024 * 1024LL * 4;

          nBitsPerLbaSignature_ = 16;
          nSlotsPerLbaBucket_ = 128;
          nBitsPerFpSignature_ = 16;
          nSlotsPerFpBucket_ = 128;
          lbaAmplifier_ = 4.0;

          enableMultiThreading_ = false;
          maxNumGlobalThreads_ = 8;
        }

        uint32_t chunkSize_; // 8k size chunk
        uint32_t subchunkSize_; // 8k size sector
        uint32_t metadataSize_; // 512 byte size chunk
        uint32_t fingerprintLen_; // fingerprint length, 20 bytes if using SHA1

        // Each bucket has 32 slots. Each index has nBuckets_ buckets,
        // Each slot represents one chunk 32K.
        // To store all lbas without eviction
        // nBuckets_ = primary_storage_size / 32K / 32 = 512
        uint32_t nBitsPerLbaSignature_;
        uint32_t nSlotsPerLbaBucket_;
        float lbaAmplifier_;
        double coreSlotsSize_ = 0.5;

        uint32_t nBitsPerFpSignature_;
        uint32_t nSlotsPerFpBucket_;

#if defined(DLRU)
        CachePolicyEnum cachePolicyForFPIndex_ = CachePolicyEnum::tNormal;
#else
        CachePolicyEnum cachePolicyForFPIndex_ = CachePolicyEnum::tRecencyAwareLeastReferenceCount;
#endif

        bool enableSketchRF_ = true;

        // Multi threading related
        uint32_t maxNumGlobalThreads_ = 8;
        bool     enableMultiThreading_;

        // io related
        char *primaryDeviceName_;
        char *cacheDeviceName_;
        uint64_t primaryDeviceSize_;
        uint64_t workingSetSize_;
        uint64_t cacheDeviceSize_;
        uint32_t weuSize_ = 0;


        // Trace replay related
        bool enableDirectIO_ = false;
        bool enableFakeIO_ = true;
        bool enableSynthenticCompression_ = false;
        bool enableTraceReplay_ = true;
        CacheModeEnum cacheMode_ = tWriteThrough;

        bool enableCompactCachePolicy_ = true;

        // Used when replaying trace, for each request, we would fill in the fingerprint value
        // specified in the trace rather than the computed one.
        std::map<uint64_t, Fingerprint> lba2Fingerprints_;
        std::mutex mutex_;
    };

}

#endif
