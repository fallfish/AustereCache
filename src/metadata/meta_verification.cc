#include "meta_verification.h"
#include "manage/dirtylist.h"
#include <cstring>

namespace cache {
  MetaVerification::MetaVerification(std::shared_ptr<LBAIndex> lbaIndex)
  {
    lbaIndex_ = std::move(lbaIndex);
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
      if (metadata.LBAs_[i] == chunk.addr_) {
        validLBA = true;
        break;
      }
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
      if (metadata.numLBAs_ == MAX_NUM_LBAS_PER_CACHED_CHUNK) {
        if (Config::getInstance().getCacheMode() == tWriteBack) {
          DirtyList::getInstance().flushOneLba(metadata.LBAs_[metadata.nextEvict_], 
              chunk.cachedataLocation_, metadata);
        }
        metadata.LBAs_[metadata.nextEvict_++] = chunk.addr_;
        if (metadata.nextEvict_ == MAX_NUM_LBAS_PER_CACHED_CHUNK) {
          metadata.nextEvict_ = 0;
        }
      } else {
        metadata.LBAs_[metadata.numLBAs_] = chunk.addr_;
        metadata.numLBAs_++;
      }

      IOModule::getInstance().write(CACHE_DEVICE, metadataLocation, &chunk.metadata_, 512);
    } else if (chunk.dedupResult_ == NOT_DUP) {
    // The data has not been cached before;
    // We need to create a new chunk metadata
      memset(&metadata, 0, sizeof(metadata));
      memcpy(metadata.fingerprint_, chunk.fingerprint_, Config::getInstance().getFingerprintLength());
      metadata.LBAs_[0] = chunk.addr_;
      metadata.numLBAs_ = 1;
      metadata.nextEvict_ = 0;
      metadata.compressedLen_ = chunk.compressedLen_;
      IOModule::getInstance().write(CACHE_DEVICE, metadataLocation, &chunk.metadata_, 512);
    }
  }
}
