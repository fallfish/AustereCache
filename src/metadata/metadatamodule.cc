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
  MetadataModule::MetadataModule(std::shared_ptr<IOModule> ioModule,
      std::shared_ptr<CompressionModule> compressionModule) {
    Config *config = Config::getInstance();
    uint32_t fp_signature_len = config->getnBitsPerFPSignature(),
             fp_bucket_no_len = config->getnBitsPerFPBucketId(),
             lba_signature_len = config->getnBitsPerLBASignature(),
             lba_bucket_no_len = config->getnBitsPerLBABucketId();
    uint32_t ca_slots_per_bucket = config->getnFPSlotsPerBucket(),
             lba_slots_per_bucket = config->getnLBASlotsPerBucket();
    fpIndex_ = std::make_shared<FPIndex>(
      fp_signature_len, 4, ca_slots_per_bucket, (1 << fp_bucket_no_len));
    lbaIndex_ = std::make_unique<LBAIndex>(
      lba_signature_len, (fp_signature_len + fp_bucket_no_len),
      lba_slots_per_bucket, (1 << lba_bucket_no_len), fpIndex_);
    // metaVerification_ and metaJournal_ should
    // hold a shared_ptr to ioModule_
    metaVerification_ = std::make_unique<MetaVerification>(ioModule, compressionModule);
    metaJournal_ = std::make_unique<MetaJournal>(ioModule);
    std::cout << "Number of LBA buckets: " << (1 << lba_bucket_no_len) << std::endl;
    std::cout << "Number of FP buckets: " << (1 << fp_bucket_no_len) << std::endl;
#ifdef CACHE_DEDUP
#if defined(DLRU)
    DLRU_SourceIndex::get_instance().init(lba_slots_per_bucket * (1 << lba_bucket_no_len));
    DLRU_FingerprintIndex::get_instance().init(ca_slots_per_bucket * (1 << fp_bucket_no_len));
    std::cout << "SourceIndex capacity: " << (lba_slots_per_bucket * (1 << lba_bucket_no_len)) << std::endl;
    std::cout << "FingerprintIndex capacity: " << (ca_slots_per_bucket * (1 << fp_bucket_no_len)) << std::endl;
#elif defined(DARC)
    DARC_SourceIndex::get_instance().init(lba_slots_per_bucket * (1 << lba_bucket_no_len), 0, 0);
    DARC_FingerprintIndex::get_instance().init(ca_slots_per_bucket * (1 << fp_bucket_no_len));
    std::cout << "SourceIndex capacity: " << (lba_slots_per_bucket * (1 << lba_bucket_no_len)) << std::endl;
    std::cout << "FingerprintIndex capacity: " << (ca_slots_per_bucket * (1 << fp_bucket_no_len)) << std::endl;
#elif defined(CDARC)
    DARC_SourceIndex::get_instance().init(lba_slots_per_bucket * (1 << lba_bucket_no_len), 0, 0);
    CDARC_FingerprintIndex::get_instance().init((1 << fp_bucket_no_len));
    std::cout << "SourceIndex capacity: " << (lba_slots_per_bucket * (1 << lba_bucket_no_len)) << std::endl;
    std::cout << "FingerprintIndex capacity: " << (1 << fp_bucket_no_len) << std::endl;
#endif
#endif
  }

#ifdef CACHE_DEDUP
#if defined(DLRU)
  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = DLRU_FingerprintIndex::get_instance().lookup(c.fingerprint_, c.cachedataLocation_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DLRU_SourceIndex::get_instance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = DLRU_FingerprintIndex::get_instance().lookup(c.fingerprint_, c.cachedataLocation_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    DLRU_SourceIndex::get_instance().update(c.addr_, c.fingerprint_);
    DLRU_FingerprintIndex::get_instance().update(c.fingerprint_, c.cachedataLocation_);
  }
#elif defined(DARC)
  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = DARC_FingerprintIndex::get_instance().lookup(c.fingerprint_, c.cachedataLocation_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DARC_SourceIndex::get_instance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = DARC_FingerprintIndex::get_instance().lookup(c.fingerprint_, c.cachedataLocation_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    DARC_SourceIndex::get_instance().adjust_adaptive_factor(c.addr_);
    DARC_FingerprintIndex::get_instance().update(c.addr_, c.fingerprint_, c.cachedataLocation_);
    DARC_SourceIndex::get_instance().update(c.addr_, c.fingerprint_);
  }
#elif defined(CDARC)
  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = CDARC_FingerprintIndex::get_instance().lookup(c.fingerprint_, c.weuId_, c._weu_offset, c.compressedLen_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DARC_SourceIndex::get_instance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = CDARC_FingerprintIndex::get_instance().lookup(c.fingerprint_, c.weuId_, c._weu_offset, c.compressedLen_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    DARC_SourceIndex::get_instance().adjust_adaptive_factor(c.addr_);
    c._evicted_weu_id = CDARC_FingerprintIndex::get_instance().update(c.addr_, c.fingerprint_, c.weuId_, c._weu_offset, c.compressedLen_);
    DARC_SourceIndex::get_instance().update(c.addr_, c.fingerprint_);
  }
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

  void MetadataModule::dedup(Chunk &c)
  {
    uint64_t fp_hash = ~0LL;
    if (c.lbaBucketLock_.get() == nullptr) {
      c.lbaBucketLock_ = std::move(lbaIndex_->lock(c.lbaHash_));
    }
    c.hitLBAIndex_ = lbaIndex_->lookup(c.lbaHash_, fp_hash) && (fp_hash == c.fingerprintHash_);

    if (c.fpBucketLock_.get() == nullptr) {
      c.fpBucketLock_ = std::move(fpIndex_->lock(c.fingerprintHash_));
    }
    c.hitFPIndex_ = fpIndex_->lookup(c.fingerprintHash_, c.compressedLevel_, c.cachedataLocation_, c.metadataLocation_);

    if (c.hitFPIndex_) {
      c.verficationResult_ = metaVerification_->verify(c);
    }

    if (c.hitFPIndex_ && c.hitLBAIndex_
        && c.verficationResult_ == BOTH_LBA_AND_FP_VALID) {
      // duplicate write
      c.dedupResult_ = DUP_WRITE;
    } else if (c.hitFPIndex_ &&
               (c.verficationResult_ == ONLY_FP_VALID ||
                c.verficationResult_ == BOTH_LBA_AND_FP_VALID)){
      // duplicate content
      c.dedupResult_ = DUP_CONTENT;
    } else {
      // not duplicate
      c.dedupResult_ = NOT_DUP;
    }
  }

  void MetadataModule::lookup(Chunk &c)
  {
    bool hitLBAIndex = false, hitFPIndex = false;

    // Obtain LBA bucket lock
    c.lbaBucketLock_ = std::move(lbaIndex_->lock(c.lbaHash_));
    c.hitLBAIndex_ = lbaIndex_->lookup(c.lbaHash_, c.fingerprintHash_);
    if (c.hitLBAIndex_) {
      c.fpBucketLock_ = std::move(fpIndex_->lock(c.fingerprintHash_));
      c.hitFPIndex_ = fpIndex_->lookup(c.fingerprintHash_, c.compressedLevel_, c.cachedataLocation_, c.metadataLocation_);
      if (c.hitFPIndex_) {
        c.verficationResult_ = metaVerification_->verify(c);
      }
    }

    if (c.verficationResult_ == VerificationResult::ONLY_LBA_VALID) {
      c.lookupResult_ = HIT;
    } else {
      c.fpBucketLock_.reset();
      assert(c.fpBucketLock_.get() == nullptr);
      c.lookupResult_ = NOT_HIT;
    }
  }

  void MetadataModule::update(Chunk &c)
  {
    // If a chunk hits lba and ca, it would check validity in the metadata.
    // If the ca is not valid, it means a new chunk and the previous one
    // should be evicted.

    BEGIN_TIMER();
    if (c.lookupResult_ == HIT) {
      fpIndex_->promote(c.fingerprintHash_);
      lbaIndex_->promote(c.lbaHash_);
    } else {
      if (c.dedupResult_ == DUP_CONTENT || c.dedupResult_ == DUP_WRITE) {
        fpIndex_->promote(c.fingerprintHash_);
      } else {
        fpIndex_->update(c.fingerprintHash_, c.compressedLevel_, c.cachedataLocation_, c.metadataLocation_);
      }
      if (c.hitLBAIndex_) {
        lbaIndex_->promote(c.lbaHash_);
      } else {
        lbaIndex_->update(c.lbaHash_, c.fingerprintHash_);
      }
    }

      metaJournal_->addUpdate(c);

    END_TIMER(update_index);

    // Cases when an on-ssd metadata update is needed
    // 1. DUP_CONTENT (update lba lists in the on-ssd metadata)
    // 2. NOT_DUP (write a new metadata and a new cached data)
    if ( (c.dedupResult_ == DUP_CONTENT && c.verficationResult_ != BOTH_LBA_AND_FP_VALID) ||
        c.dedupResult_ == NOT_DUP) {
      metaVerification_->update(c);
    }

    return ;
  }
#endif
}
