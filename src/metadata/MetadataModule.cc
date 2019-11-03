#include "common/env.h"
#include "MetadataModule.h"
#include "metaverification.h"
#include "metajournal.h"
#include "frequentslots.h"
#include "common/config.h"
#include "common/stats.h"
#include "utils/utils.h"
#include <cassert>

namespace cache {
  MetadataModule& MetadataModule::getInstance() {
    static MetadataModule instance;
    return instance;
  }

  MetadataModule::MetadataModule() {
    fpIndex_ = std::make_shared<FPIndex>();
    lbaIndex_ = std::make_unique<LBAIndex>(fpIndex_);
    // metaVerification_ and metaJournal_ should
    // hold a shared_ptr to ioModule_
    metaVerification_ = std::make_unique<MetaVerification>();
    metaJournal_ = std::make_unique<MetaJournal>();
    std::cout << "Number of LBA buckets: " << Config::getInstance().getnLBABuckets() << std::endl;
    std::cout << "Number of FP buckets: " << Config::getInstance().getnFPBuckets() << std::endl;
  }

  // Note:
  // For read request, the chunk should already obtain the lba
  // lock in the lookup procedure. If the chunk didn't hit a
  // cached data (NOT HIT), it goes into the dedup to check the
  // deduplication of the fetched data where lba need not be
  // checked (The content must not be DUP_WRITE). We can use
  // the lock status (the chunk has not obtained a ca lock)
  // to only do the ca verification.

  void MetadataModule::dedup(Chunk &chunk)
  {
    uint64_t fpHash = ~0ull;
    if (chunk.lbaBucketLock_ == nullptr) {
      chunk.lbaBucketLock_ = std::move(lbaIndex_->lock(chunk.lbaHash_));
    }
    chunk.hitLBAIndex_ = lbaIndex_->lookup(chunk.lbaHash_, fpHash) && (fpHash == chunk.fingerprintHash_);

    if (chunk.fpBucketLock_ == nullptr) {
      chunk.fpBucketLock_ = std::move(fpIndex_->lock(chunk.fingerprintHash_));
    }
    chunk.hitFPIndex_ = fpIndex_->lookup(chunk.fingerprintHash_, chunk.compressedLevel_, chunk.cachedataLocation_, chunk.metadataLocation_);

    if (chunk.hitFPIndex_) {
      chunk.verficationResult_ = metaVerification_->verify(chunk);
    }

    if (chunk.hitFPIndex_ && chunk.hitLBAIndex_
        && chunk.verficationResult_ == BOTH_LBA_AND_FP_VALID) {
      // duplicate write
      chunk.dedupResult_ = DUP_WRITE;
    } else if (chunk.hitFPIndex_ &&
               (chunk.verficationResult_ == ONLY_FP_VALID ||
                chunk.verficationResult_ == BOTH_LBA_AND_FP_VALID)){
      // duplicate content
      chunk.dedupResult_ = DUP_CONTENT;
    } else {
      // not duplicate
      chunk.dedupResult_ = NOT_DUP;
    }
  }

  void MetadataModule::lookup(Chunk &chunk)
  {
    // Obtain LBA bucket lock
    chunk.lbaBucketLock_ = std::move(lbaIndex_->lock(chunk.lbaHash_));
    chunk.hitLBAIndex_ = lbaIndex_->lookup(chunk.lbaHash_, chunk.fingerprintHash_);
    if (chunk.hitLBAIndex_) {
      chunk.fpBucketLock_ = std::move(fpIndex_->lock(chunk.fingerprintHash_));
      chunk.hitFPIndex_ = fpIndex_->lookup(chunk.fingerprintHash_, chunk.compressedLevel_, chunk.cachedataLocation_, chunk.metadataLocation_);
      if (chunk.hitFPIndex_) {
        chunk.verficationResult_ = metaVerification_->verify(chunk);
      }
    }

    if (chunk.verficationResult_ == VerificationResult::ONLY_LBA_VALID) {
      chunk.compressedLen_ = chunk.metadata_.compressedLen_;
      chunk.lookupResult_ = HIT;
    } else {
      chunk.fpBucketLock_.reset();
      assert(chunk.fpBucketLock_.get() == nullptr);
      chunk.lookupResult_ = NOT_HIT;
    }
  }

  void MetadataModule::update(Chunk &chunk)
  {
    BEGIN_TIMER();
    if (chunk.lookupResult_ == HIT) {
      fpIndex_->promote(chunk.fingerprintHash_);
      lbaIndex_->promote(chunk.lbaHash_);
    } else {
      uint64_t removedFingerprintHash = ~0ull;
      // Note that for a cache-candidate chunk, hitLBAIndex indicates both hit in the LBA index and fpHash match
      // deduplication focus on the fingerprint part
      if (chunk.hitLBAIndex_) {
        lbaIndex_->promote(chunk.lbaHash_);
      } else {
        removedFingerprintHash = lbaIndex_->update(chunk.lbaHash_, chunk.fingerprintHash_);
        fpIndex_->reference(chunk.fingerprintHash_);
        if (removedFingerprintHash != ~0ull && removedFingerprintHash != chunk.fingerprintHash_) {
          fpIndex_->dereference(removedFingerprintHash);
        }
      }
      if (chunk.dedupResult_ == DUP_CONTENT || chunk.dedupResult_ == DUP_WRITE) {
        fpIndex_->promote(chunk.fingerprintHash_);
      } else {
        fpIndex_->update(chunk.fingerprintHash_, chunk.compressedLevel_, chunk.cachedataLocation_, chunk.metadataLocation_);
      }
    }

    metaJournal_->addUpdate(chunk);

    END_TIMER(update_index);

    // Cases when an on-ssd metadata update is needed
    // 1. DUP_CONTENT (update lba lists in the on-ssd metadata if the lba is not in the list)
    // 2. NOT_DUP (write a new metadata and a new cached data)
    if ((chunk.dedupResult_ == DUP_CONTENT && chunk.verficationResult_ != BOTH_LBA_AND_FP_VALID) ||
        chunk.dedupResult_ == NOT_DUP) {
      metaVerification_->update(chunk);
    }
  }
}
