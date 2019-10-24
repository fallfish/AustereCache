#include "common/env.h"
#include "metadatamodule.h"
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
#ifdef CACHE_DEDUP
#if defined(DLRU)
    DLRU_SourceIndex::getInstance().init();
    DLRU_FingerprintIndex::getInstance().init();
    std::cout << "SourceIndex capacity: " <<  DLRU_SourceIndex::getInstance().capacity_ << std::endl;
    std::cout << "FingerprintIndex capacity: " << DLRU_FingerprintIndex::getInstance().capacity_ << std::endl;
#elif defined(DARC)
    DARC_SourceIndex::getInstance().init(0, 0);
    DARC_FingerprintIndex::getInstance().init();
    std::cout << "SourceIndex capacity: " << DARC_SourceIndex::getInstance().capacity_ << std::endl;
    std::cout << "FingerprintIndex capacity: " <<  DARC_FingerprintIndex::getInstance().capacity_ << std::endl;
#elif defined(CDARC)
    DARC_SourceIndex::getInstance().init(0, 0);
    CDARC_FingerprintIndex::getInstance().init();
    std::cout << "SourceIndex capacity: " <<  DARC_SourceIndex::getInstance().capacity_ << std::endl;
    std::cout << "FingerprintIndex capacity: " << CDARC_FingerprintIndex::getInstance().capacity_ << std::endl;
#elif defined(BUCKETDLRU)

#endif

#else
    fpIndex_ = std::make_shared<FPIndex>();
    lbaIndex_ = std::make_unique<LBAIndex>(fpIndex_);
    // metaVerification_ and metaJournal_ should
    // hold a shared_ptr to ioModule_
    metaVerification_ = std::make_unique<MetaVerification>();
    metaJournal_ = std::make_unique<MetaJournal>();
    std::cout << "Number of LBA buckets: " << Config::getInstance().getnLBABuckets() << std::endl;
    std::cout << "Number of FP buckets: " << Config::getInstance().getnFPBuckets() << std::endl;
#endif
  }

#ifdef CACHE_DEDUP
#if defined(DLRU)
  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = DLRU_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DLRU_SourceIndex::getInstance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = DLRU_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    DLRU_SourceIndex::getInstance().update(c.addr_, c.fingerprint_);
    DLRU_FingerprintIndex::getInstance().update(c.fingerprint_, c.cachedataLocation_);
  }
#elif defined(DARC)
  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = DARC_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DARC_SourceIndex::getInstance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = DARC_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    DARC_SourceIndex::getInstance().adjust_adaptive_factor(c.addr_);
    DARC_FingerprintIndex::getInstance().update(c.addr_, c.fingerprint_, c.cachedataLocation_);
    DARC_SourceIndex::getInstance().update(c.addr_, c.fingerprint_);
  }
#elif defined(CDARC)
  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = CDARC_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.weuId_, c.weuOffset_, c.compressedLen_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DARC_SourceIndex::getInstance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = CDARC_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.weuId_, c.weuOffset_, c.compressedLen_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    DARC_SourceIndex::getInstance().adjust_adaptive_factor(c.addr_);
    c.evictedWEUId_ = CDARC_FingerprintIndex::getInstance().update(c.addr_, c.fingerprint_, c.weuId_, c.weuOffset_, c.compressedLen_);
    //printf("%d %d %d %d\n", c.evictedWEUId_, c.weuId_, c.weuOffset_, c.compressedLen_);
    DARC_SourceIndex::getInstance().update(c.addr_, c.fingerprint_);
  }
#else
  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = BucketizedDLRU_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = BucketizedDLRU_SourceIndex::getInstance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = BucketizedDLRU_FingerprintIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    BucketizedDLRU_SourceIndex::getInstance().update(c.addr_, c.fingerprint_);
    BucketizedDLRU_FingerprintIndex::getInstance().update(c.fingerprint_, c.cachedataLocation_);
  }
#endif
#endif
#else
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
      // deduplication focus on the fingerprint part
      if (chunk.dedupResult_ == DUP_CONTENT || chunk.dedupResult_ == DUP_WRITE) {
        fpIndex_->promote(chunk.fingerprintHash_);
      } else {
        fpIndex_->update(chunk.fingerprintHash_, chunk.compressedLevel_, chunk.cachedataLocation_, chunk.metadataLocation_);
      }
      // Note that for a cache-candidate chunk, hitLBAIndex indicates both hit in the LBA index and fpHash match
      if (chunk.hitLBAIndex_) {
        lbaIndex_->promote(chunk.lbaHash_);
      } else {
        lbaIndex_->update(chunk.lbaHash_, chunk.fingerprintHash_);
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

#endif
}
