#include "metadatamodule.h"
#include "metaverification.h"
#include "metajournal.h"
#include "frequentslots.h"
#include "common/config.h"
#include <cassert>

namespace cache {
  MetadataModule::MetadataModule(std::shared_ptr<IOModule> io_module,
      std::shared_ptr<CompressionModule> compression_module) {
    Config &config = Config::get_configuration();
    uint32_t ca_signature_len = config.get_ca_signature_len(), 
             ca_bucket_no_len = config.get_ca_bucket_no_len(),
             lba_signature_len = config.get_lba_signature_len(),
             lba_bucket_no_len = config.get_lba_bucket_no_len();
    uint32_t ca_slots_per_bucket = config.get_ca_slots_per_bucket(),
             lba_slots_per_bucket = config.get_lba_slots_per_bucket();
    _ca_index = std::make_shared<CAIndex>(
      ca_signature_len, 4, (1 << ca_bucket_no_len), ca_slots_per_bucket);
    _lba_index = std::make_unique<LBAIndex>(
      lba_signature_len, (ca_signature_len + ca_bucket_no_len),
      (1 << lba_bucket_no_len), lba_slots_per_bucket, _ca_index);
    // _meta_verification and _meta_journal should
    // hold a shared_ptr to _io_module
    _meta_verification = std::make_unique<MetaVerification>(io_module, compression_module);
    _meta_journal = std::make_unique<MetaJournal>(io_module);
  }

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
    uint32_t ca_hash = ~0;
    if (c._lba_bucket_lock.get() == nullptr) {
      c._lba_bucket_lock = std::move(_lba_index->lock(c._lba_hash));
    }
    c._lba_hit = _lba_index->lookup(c._lba_hash, ca_hash);

    if (c._ca_bucket_lock.get() == nullptr) {
      c._ca_bucket_lock = std::move(_ca_index->lock(c._ca_hash));
    }
    c._ca_hit = _ca_index->lookup(c._ca_hash, c._compress_level, c._ssd_location);

    if (c._ca_hit) {
      c._verification_result = _meta_verification->verify(c);
    }

    if (c._ca_hit && c._lba_hit && c._ca_hash == ca_hash
        && c._verification_result == BOTH_LBA_AND_CA_VALID) {
      // duplicate write
      c._dedup_result = DUP_WRITE;
    } else if (c._ca_hit &&
               (c._verification_result == ONLY_CA_VALID ||
                c._verification_result == BOTH_LBA_AND_CA_VALID)){
      // duplicate content
      c._dedup_result = DUP_CONTENT; 
    } else {
      // not duplicate
      c._dedup_result = NOT_DUP;
    }
  }

  void MetadataModule::lookup(Chunk &c)
  {
    bool lba_hit = false, ca_hit = false;

    c._lba_bucket_lock = std::move(_lba_index->lock(c._lba_hash));
    c._lba_hit = _lba_index->lookup(c._lba_hash, c._ca_hash);
    if (c._lba_hit) {
      c._ca_bucket_lock = std::move(_ca_index->lock(c._ca_hash));
      c._ca_hit = _ca_index->lookup(c._ca_hash, c._compress_level, c._ssd_location);
      if (c._ca_hit) {
        c._verification_result = _meta_verification->verify(c);
      }
    }

    if (c._verification_result == VerificationResult::ONLY_LBA_VALID) {
      c._lookup_result = HIT;
    } else {
      c._ca_bucket_lock.reset();
      assert(c._ca_bucket_lock.get() == nullptr);
      c._lookup_result = NOT_HIT;
    }
  }

  void MetadataModule::update(Chunk &c)
  {
    // If a chunk hits lba and ca, it would check validity in the metadata.
    // If the ca is not valid, it means a new chunk and the previous one
    // should be evicted.

    if (c._ca_hit) {
      _ca_index->promote(c._ca_hash);
    } else {
      _ca_index->update(c._ca_hash, c._compress_level, c._ssd_location);
    }

    if (c._lba_hit) {
      _lba_index->promote(c._lba_hash);
    } else {
      _lba_index->update(c._lba_hash, c._ca_hash);
    }

    _meta_journal->add_update(c);

    // Cases when an on-ssd metadata update is needed
    // 1. DUP_CONTENT (update lba lists in the on-ssd metadata)
    // 2. NOT_DUP (write a new metadata and a new cached data)
    if (c._dedup_result == DUP_CONTENT ||
        c._dedup_result == NOT_DUP) {
      _meta_verification->update(c);
    }

    return ;
  }
}
