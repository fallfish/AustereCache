#include "metaverification.h"
#include "cache_policy.h"
#include <cstring>

namespace cache {
  MetaVerification::MetaVerification()
  {
    frequentSlots_ = std::make_unique<FrequentSlots>();
  }

  VerificationResult MetaVerification::verify(Chunk &chunk)
  {
    // read metadata from ioModule_
    uint64_t &lba = chunk.addr_;
    auto &fingerprint = chunk.fingerprint_;
    uint64_t &metadataLocation = chunk.metadataLocation_;
    Metadata &metadata = chunk.metadata_;

    IOModule::getInstance().read(CACHE_DEVICE, metadataLocation, &metadata, 512);

    // check lba
    bool validLBA = false;

    for (uint32_t i = 0; i < metadata.numLBAs_; i++) {
      if (i >= MAX_NUM_LBAS_PER_CACHED_CHUNK) break;
      if (metadata.LBAs_[i] == chunk.addr_) {
        validLBA = true;
        break;
      }
    }
    if (!validLBA && metadata.numLBAs_ > MAX_NUM_LBAS_PER_CACHED_CHUNK) {
      frequentSlots_->query(chunk.fingerprintHash_, lba);
    }

    // check fingerprint
    // fingerprint is valid only fingerprint is valid and also
    // the content is the same by memcmp
    bool validFingerprint = false;
    // Compute SHA1 and Compare Full fingerprint
    if (chunk.hasFingerprint_ && memcmp(
        metadata.fingerprint_, chunk.fingerprint_,
        Config::getInstance().getFingerprintLength()) == 0)
      validFingerprint = true;

    // To release computation burden for fingerprint computation
    // We come up with three methods to compare the fingerprint value
    // Method 1: Compute a strong fingerprint (SHA1) which could be slow
    // Method 2: Compute a weak fingerprint (xxhash).
    //           To avoid higher collision rate,
    //           when matched, fetch real data from the cache, decompress,
    //           and do memcmp.
    // Method 3: Compute a weak fingerprint.
    //           To avoid higher collision rate,
    //           when matched, compute a strong fingerprint (SHA1)
    //           and compare again against the strong fingerprint.
    {
      if (validFingerprint) {
        uint32_t chunkSize = Config::getInstance().getChunkSize(),
                 metadataSize = Config::getInstance().getMetadataSize(),
                 sectorSize = Config::getInstance().getSectorSize();
        uint32_t fingerprintMode = Config::getInstance().getFingerprintMode();

        if (fingerprintMode == 1) {
        // Method 1: use weak hash + memcmp + decompression
          uint8_t compressedData[chunkSize];
          uint8_t decompressedData[chunkSize];
          // Fetch compressed data, decompress, and compare data content
          if (metadata.compressedLen_ != 0) {
            IOModule::getInstance().read(CACHE_DEVICE, chunk.cachedataLocation_, compressedData,
              (metadata.compressedLen_ + sectorSize - 1) / sectorSize * sectorSize);
            CompressionModule::decompress(compressedData, decompressedData,
              metadata.compressedLen_, chunkSize);
          } else {
            IOModule::getInstance().read(CACHE_DEVICE, chunk.cachedataLocation_,
              decompressedData, chunkSize);
          }
          validFingerprint = (memcmp(decompressedData, chunk.buf_, chunkSize) == 0);
        } else if (fingerprintMode == 2) {
        // Method 2: use weak hash + strong hash
          chunk.computeStrongFingerprint();
          validFingerprint = (memcmp(chunk.strongFingerprint_, metadata.strongFingerprint_,
                Config::getInstance().getStrongFingerprintLength()) == 0);
        }
      }
    }

    if (validLBA && validFingerprint)
      return BOTH_LBA_AND_FP_VALID;
    else if (validLBA)
      return ONLY_LBA_VALID;
    else if (validFingerprint)
      return ONLY_FP_VALID;
    else
      return BOTH_LBA_AND_FP_NOT_VALID;
  }

  void MetaVerification::update(Chunk &chunk)
  {
    uint64_t &lba = chunk.addr_;
    auto &ca = chunk.fingerprint_;
    uint64_t &metadataLocation = chunk.metadataLocation_;
    Metadata &metadata = chunk.metadata_;

    // The data has been cached (duplicate);
    // We update the chunk metadata
    if (chunk.dedupResult_ == DUP_CONTENT) {
      if (metadata.numLBAs_ >= MAX_NUM_LBAS_PER_CACHED_CHUNK) {
        // On-disk chunk_metadata cannot accommodate the new LBA
        if (metadata.numLBAs_ == MAX_NUM_LBAS_PER_CACHED_CHUNK)
          frequentSlots_->allocate(chunk.fingerprintHash_);
        frequentSlots_->add(chunk.fingerprintHash_, chunk.addr_);
      } else {
        metadata.LBAs_[metadata.numLBAs_] = chunk.addr_;
      }
      metadata.numLBAs_++;
      IOModule::getInstance().write(CACHE_DEVICE, metadataLocation, &chunk.metadata_, 512);
    } else if (chunk.dedupResult_ == NOT_DUP) {
    // The data has not been cached before;
    // We need to create a new chunk metadata
      frequentSlots_->remove(chunk.fingerprintHash_);
      memset(&metadata, 0, sizeof(metadata));
      memcpy(metadata.fingerprint_, chunk.fingerprint_, Config::getInstance().getFingerprintLength());
      metadata.LBAs_[0] = chunk.addr_;
      metadata.numLBAs_ = 1;
      metadata.compressedLen_ = chunk.compressedLen_;
      if (Config::getInstance().getFingerprintMode() == 2) { // Weak hash + Strong hash
        chunk.computeStrongFingerprint();
        memcpy(metadata.strongFingerprint_, chunk.strongFingerprint_, Config::getInstance().getStrongFingerprintLength());
      }
      IOModule::getInstance().write(CACHE_DEVICE, metadataLocation, &chunk.metadata_, 512);
    }
  }
}
