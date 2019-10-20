#ifndef __CONFIG_H__
#define __CONFIG_H__ 
#include <cstdint>
#include <cstring>
#include <cstdlib>
namespace cache {

class Config
{
 public:
  static Config& getInstance() {
    static Config instance;
    return instance;
  }

  // getters
  uint32_t getChunkSize() { return chunkSize_; }
  uint32_t getSectorSize() { return sectorSize_; }
  uint32_t getMetadataSize() { return metadataSize_; }
  uint32_t getFingerprintLength() { return fingerprintLen_; }
  uint32_t getStrongFingerprintLength() { return strongFingerprintLen_; }
  uint64_t getPrimaryDeviceSize() { return primaryDeviceSize_; }
  uint64_t getCacheDeviceSize() { return cacheDeviceSize_; }

  uint32_t getnBitsPerLBASignature() { return nBitsPerLBASignature_; }
  uint32_t getnBitsPerFPSignature() { return nBitsPerFPSignature_; }

  uint32_t getnBitsPerLBABucketId() {
    return 32 - __builtin_clz(getnLBABuckets());
  }
  uint32_t getnLBABuckets() {
    return cacheDeviceSize_ / (chunkSize_ * nLBASlotsPerBucket_) * lbaAmplifier_;
  }
  uint32_t getnBitsPerFPBucketId() {
    return 32 - __builtin_clz(getnFPBuckets());
  }
  uint32_t getnFPBuckets() {
    return cacheDeviceSize_ / (sectorSize_ * nFPSlotsPerBucket_);
  }

  uint32_t getLBAAmplifier() {
    return lbaAmplifier_;
  }
  uint32_t getnLBASlotsPerBucket() { return nLBASlotsPerBucket_; }
  uint32_t getnFPSlotsPerBucket() { return nFPSlotsPerBucket_; }
  uint32_t getnBitsPerClock() { return nBitsPerClock_; }


    uint32_t getMaxNumGlobalThreads() { return maxNumGlobalThreads_; }
  uint32_t getMaxNumLocalThreads() { return maxNumLocalThreads_; }

  char *getCacheDeviceName() { return cacheDeviceName_; }
  char *getPrimaryDeviceName() { return primaryDeviceName_; }
  bool isDirectIOEnabled() { return enableDirectIO_; }

  uint32_t getFingerprintAlg() { return fingerprintAlgorithm_; }
  uint32_t getFingerprintMode() { return fingerprintMode_; }

  uint32_t getWriteBufferSize() { return writeBufferSize_; }
  
  bool isMultiThreadEnabled() { return enableMultiThreads_; }


  // setters
  void setFingerprintLength(uint32_t ca_length) { fingerprintLen_ = ca_length; }
  void setPrimaryDeviceSize(uint64_t primary_device_size) { primaryDeviceSize_ = primary_device_size; }
  void setCacheDeviceSize(uint64_t cache_device_size) { cacheDeviceSize_ = cache_device_size; }

  void setLBAAmplifier(uint32_t v) {
    lbaAmplifier_ = v;
  }

  void setCacheDeviceName(char *cache_device_name) { cacheDeviceName_ = cache_device_name; }
  void setPrimaryDeviceName(char *primary_device_name) { primaryDeviceName_ = primary_device_name; }
  void enableDirectIO(bool v) { enableDirectIO_ = v; }

  void setWriteBufferSize(uint32_t v) { writeBufferSize_ = v; }

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

    // Each bucket has 32 slots. Each index has nBuckets_ buckets,
    // Each slot represents one chunk 32K.
    // To store all lbas without eviction
    // nBuckets_ = primary_storage_size / 32K / 32 = 512
    nBitsPerLBASignature_ = 12;
    nLBASlotsPerBucket_ = 32;
    nBitsPerFPSignature_ = 12;
    nFPSlotsPerBucket_ = 32;
    nBitsPerClock_ = 2;
    lbaAmplifier_ = 1u;

    enableMultiThreads_ = false;
    maxNumGlobalThreads_ = 32;
    maxNumLocalThreads_ = 8;

    // io related
    cacheDeviceName_ = "./cache_device";
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
  uint32_t nBitsPerLBASignature_;
  uint32_t nLBASlotsPerBucket_;
  uint32_t lbaAmplifier_;

  uint32_t nBitsPerFPSignature_;
  uint32_t nFPSlotsPerBucket_;
  // bits for CLOCK policy
  uint32_t nBitsPerClock_;

  // Multi threading related
  uint32_t maxNumGlobalThreads_;
  uint32_t maxNumLocalThreads_;
  bool     enableMultiThreads_;

  // io related
  char *primaryDeviceName_;
  char *cacheDeviceName_;
  uint64_t primaryDeviceSize_;
  uint64_t cacheDeviceSize_;
  uint32_t writeBufferSize_ = 0;
  bool enableDirectIO_ = false;

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
};

}

#endif
