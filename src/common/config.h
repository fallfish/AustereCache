#ifndef __CONFIG_H__
#define __CONFIG_H__
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <iostream>
namespace cache {

    enum CachePolicyEnum {
        // For ACDC
          tCAClock, tGarbageAwareCAClock, tLeastReferenceCount, tRecencyAwareLeastReferenceCount, tLRU,
        // For DLRU
          tNormal, tGarbageAware,
    };

    class Config
    {
    public:
        static Config& getInstance() {
          static Config instance;
          return instance;
        }

        void release() {
          free(dataOfCurrentChunk_);
        }

        // getters
        uint32_t getChunkSize() { return chunkSize_; }
        uint32_t getSectorSize() { return sectorSize_; }
        uint32_t getMetadataSize() { return metadataSize_; }
        uint32_t getFingerprintLength() { return fingerprintLen_; }
        uint32_t getStrongFingerprintLength() { return strongFingerprintLen_; }
        uint64_t getPrimaryDeviceSize() { return primaryDeviceSize_; }
        uint64_t getCacheDeviceSize() { return cacheDeviceSize_; }
        uint64_t getWorkingSetSize() { return workingSetSize_; }

        uint32_t getnBitsPerLbaSignature() { return nBitsPerLbaSignature_; }
        uint32_t getnBitsPerFpSignature() { return nBitsPerFpSignature_; }

        uint32_t getnBitsPerLbaBucketId() {
          return 32 - __builtin_clz(getnLbaBuckets());
        }
        uint32_t getnLbaBuckets() {
          if (lbaAmplifier_ == 0) {
            return workingSetSize_ / (chunkSize_ * nSlotsPerLbaBucket_);
          } else {
            return std::min(workingSetSize_ / (chunkSize_ * nSlotsPerLbaBucket_),
                            (uint64_t)(cacheDeviceSize_ / (chunkSize_ * nSlotsPerLbaBucket_) * lbaAmplifier_));
          }
        }
        uint32_t getnSourceIndexEntries() {
          if (lbaAmplifier_ == 0) {
            return workingSetSize_ / chunkSize_;
          } else {
            return std::min(workingSetSize_ / chunkSize_, (uint64_t)(cacheDeviceSize_ / chunkSize_ * lbaAmplifier_));
          }
        }
        uint32_t getnBitsPerFpBucketId() {
          return 32 - __builtin_clz(getnFpBuckets());
        }
        uint32_t getnFpBuckets() {
          return cacheDeviceSize_ / (sectorSize_ * nSlotsPerFpBucket_);
        }

        float getLBAAmplifier() {
          return lbaAmplifier_;
        }

        uint32_t getLBASlotSeperator() {
          return
            (uint32_t)((32 - cacheDeviceSize_ / (uint64_t)chunkSize_ /
                             (uint64_t)getnLbaBuckets()) / 4);
          // 25% core list
        }

        uint32_t getnLBASlotsPerBucket() { return nSlotsPerLbaBucket_; }
        uint32_t getnFPSlotsPerBucket() { return nSlotsPerFpBucket_; }
        uint32_t getnBitsPerClock() { return nBitsPerClock_; }
        uint32_t getClockStartValue() { return clockStartValue_; }
        uint32_t getCompressionLevels() { return chunkSize_ / sectorSize_; }

        // CachePolicy:
        //
        CachePolicyEnum getCachePolicyForFPIndex() {
          return cachePolicyForFPIndex_;
        }

        uint32_t getMaxNumGlobalThreads() { return maxNumGlobalThreads_; }
        uint32_t getMaxNumLocalThreads() { return maxNumLocalThreads_; }

        char *getCacheDeviceName() { return cacheDeviceName_; }
        char *getPrimaryDeviceName() { return primaryDeviceName_; }

        uint32_t getFingerprintAlg() { return fingerprintAlgorithm_; }
        uint32_t getFingerprintMode() { return fingerprintMode_; }

        uint32_t getWriteBufferSize() { return writeBufferSize_; }

        bool isMultiThreadEnabled() { return enableMultiThreads_; }


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
        void setSectorSize(uint32_t v) { sectorSize_ = v; }
        void setChunkSize(uint32_t v) { chunkSize_ = v; }

        void setCacheDeviceName(char *cache_device_name) { cacheDeviceName_ = cache_device_name; }
        void setPrimaryDeviceName(char *primary_device_name) { primaryDeviceName_ = primary_device_name; }

        void setWriteBufferSize(uint32_t v) { writeBufferSize_ = v; }

        void enableDirectIO(bool v) { enableDirectIO_ = v; }
        void enableFakeIO(bool v) { enableFakeIO_ = v; }
        void enableSynthenticCompression(bool v) { enableSynthenticCompression_ = v; }
        void enableReplayFIU(bool v) { enableReplayFIU_ = v; }
        void enableSketchRF(bool v) { enableSketchRF_ = v; }
        void enableSmartDedupPolicy(bool v) { enableSmartDedupPolicy_ = v; }
        bool isDirectIOEnabled() { return enableDirectIO_; }
        bool isFakeIOEnabled() { return enableFakeIO_; }
        bool isReplayFIUEnabled() { return enableReplayFIU_; }
        bool isSynthenticCompressionEnabled() { return enableSynthenticCompression_; }
        bool isSketchRFEnabled() { return enableSketchRF_; }
        bool isSmartDedupPolicyEnabled() { return enableSmartDedupPolicy_; }

        void setFingerprintAlgorithm(uint32_t v) {
          if (v == 0) setFingerprintLength(20);
          else setFingerprintLength(16);
          fingerprintAlgorithm_ = v;
        }
        void setFingerprintMode(uint32_t v) {
          if (v == 0) setFingerprintAlgorithm(0);
          else setFingerprintAlgorithm(1);
          fingerprintMode_ = v;
        }
        char *getCurrentFingerprint() {
          return fingerprintOfCurrentChunk_;
        }
        void setCurrentFingerprint(char *fingerprint) {
          memcpy(fingerprintOfCurrentChunk_, fingerprint, fingerprintLen_);
        }
        char *getCurrentData() {
          return dataOfCurrentChunk_;
        }
        void setCurrentData(char *data, int len) {
          if (len > chunkSize_) len = chunkSize_;
          memcpy(dataOfCurrentChunk_, data, len);
        }
        int getCurrentCompressedLen() {
          return compressedLenOfCurrentChunk_;
        }
        void setCurrentCompressedLen(int compressedLen) {
          compressedLenOfCurrentChunk_ = compressedLen;
        }

        bool currentEvictionIsRewrite = false;
    private:
        Config() {
          // Initialize default configuration
          // Conf format from caller to be imported
          chunkSize_ = 8192 * 4;
          sectorSize_ = 8192;
          metadataSize_ = 512;
          fingerprintLen_ = 16;
          strongFingerprintLen_ = 20;
          primaryDeviceSize_ = 1024 * 1024 * 1024LL * 20;
          cacheDeviceSize_ = 1024 * 1024 * 1024LL * 20;
          workingSetSize_ = 1024 * 1024 * 1024LL * 4;

          // Each bucket has 32 slots. Each index has nBuckets_ buckets,
          // Each slot represents one chunk 32K.
          // To store all lbas without eviction
          // nBuckets_ = primary_storage_size / 32K / 32 = 512
          nBitsPerLbaSignature_ = 12;
          nSlotsPerLbaBucket_ = 32;
          nBitsPerFpSignature_ = 12;
          nSlotsPerFpBucket_ = 32;
          nBitsPerClock_ = 2;
          clockStartValue_ = 1;
          lbaAmplifier_ = 1.0;

          enableMultiThreads_ = false;
          maxNumGlobalThreads_ = 32;
          maxNumLocalThreads_ = 8;

          // io related
          cacheDeviceName_ = "./ramdisk/cache_device";
          primaryDeviceName_ = "./primary_device";

          // NORMAL_DIST_COMPRESSION related
          dataOfCurrentChunk_ = (char*)malloc(sizeof(char) * chunkSize_);
          setFingerprintMode(0);
          setFingerprintAlgorithm(1);
        }

        uint32_t chunkSize_; // 8k size chunk
        uint32_t sectorSize_; // 8k size sector
        uint32_t metadataSize_; // 512 byte size chunk
        uint32_t fingerprintLen_; // fingerprint length, 20 bytes if using SHA1
        uint32_t strongFingerprintLen_; // content address (fingerprint) length, 20 bytes if using SHA1

        // Each bucket has 32 slots. Each index has nBuckets_ buckets,
        // Each slot represents one chunk 32K.
        // To store all lbas without eviction
        // nBuckets_ = primary_storage_size / 32K / 32 = 512
        uint32_t nBitsPerLbaSignature_;
        uint32_t nSlotsPerLbaBucket_;
        float lbaAmplifier_;

        uint32_t nBitsPerFpSignature_;
        uint32_t nSlotsPerFpBucket_;
        // bits for CLOCK policy
        uint32_t nBitsPerClock_;
        uint32_t clockStartValue_;
#if defined(DLRU)
        CachePolicyEnum cachePolicyForFPIndex_ = CachePolicyEnum::tNormal;
#else
        CachePolicyEnum cachePolicyForFPIndex_ = CachePolicyEnum::tRecencyAwareLeastReferenceCount;
#endif
        bool enableSketchRF_ = true;

        // Multi threading related
        uint32_t maxNumGlobalThreads_;
        uint32_t maxNumLocalThreads_;
        bool     enableMultiThreads_;

        // io related
        char *primaryDeviceName_;
        char *cacheDeviceName_;
        uint64_t primaryDeviceSize_;
        uint64_t workingSetSize_;
        uint64_t cacheDeviceSize_;
        uint32_t writeBufferSize_ = 0;

        // Trace replay related
        bool enableDirectIO_ = false;
        bool enableFakeIO_ = true;
        bool enableSynthenticCompression_ = false;
        bool enableReplayFIU_ = true;

        // chunk algorithm
        // 0 - Strong hash (SHA1), 1 - Weak hash (MurmurHash3)
        uint32_t fingerprintAlgorithm_{};
        // 0 - Strong hash, 1 - Weak hash + memcmp, 2 - Weak hash + Strong hash
        // If mode is set to 1 or 2, the fingerprintAlgorithm_ field must be 1
        uint32_t fingerprintMode_{};

        // Used when replaying FIU trace, for each request, we would fill in the fingerprint value
        // specified in the trace rather than the computed one.
        char fingerprintOfCurrentChunk_[20]{};

        // Used when using NORMAL_DIST_COMPRESSION
        char* dataOfCurrentChunk_;
        int compressedLenOfCurrentChunk_{};
        bool enableSmartDedupPolicy_ = true;
    };

}

#endif
