#include "metadatamodule.h"
#include "metaverification.h"
#include "metajournal.h"
#include "frequentslots.h"
#include "common/config.h"
#include <cassert>

namespace cache {
  MetadataModule::MetadataModule(std::shared_ptr<IOModule> io_module) {
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
    _meta_verification = std::make_unique<MetaVerification>(io_module);
    _meta_journal = std::make_unique<MetaJournal>(io_module);
  }

  void MetadataModule::lookup(Chunk &c, bool write_path)
  {
    if (write_path) {
      lookup_write_path(c);
    } else {
      lookup_read_path(c);
    }
  }

  void MetadataModule::lookup_write_path(Chunk &c)
  {
    uint32_t ca_hash;
    c._lba_bucket_lock = std::move(_lba_index->lock(c._lba_hash));
    c._lba_hit = _lba_index->lookup(c._lba_hash, ca_hash);
    c._ca_bucket_lock = std::move(_ca_index->lock(c._ca_hash));
    c._ca_hit = _ca_index->lookup(c._ca_hash, c._compress_level, c._ssd_location);

    if (c._ca_hit) {
      c._verification_result = _meta_verification->verify(c);
    }

    if (c._ca_hit && c._lba_hit && c._ca_hash == ca_hash
        && c._verification_result == BOTH_LBA_AND_CA_VALID) {
      // duplicate write
      c._lookup_result = LookupResult::WRITE_DUP_WRITE;
    } else if (c._ca_hit &&
               (c._verification_result == ONLY_CA_VALID ||
                c._verification_result == BOTH_LBA_AND_CA_VALID)){
      // duplicate content
      c._lookup_result = LookupResult::WRITE_DUP_CONTENT;
    } else {
      // not duplicate
      c._lookup_result = LookupResult::WRITE_NOT_DUP;
    }
  }

  void MetadataModule::lookup_read_path(Chunk &c)
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
      c._lookup_result = LookupResult::READ_HIT;
    } else {
      c._ca_bucket_lock.reset();
      assert(c._ca_bucket_lock.get() == nullptr);
      c._lookup_result = LookupResult::READ_NOT_HIT;
    }
  }

  void MetadataModule::update(Chunk &c)
  {
    //if (c._ca_bucket_lock.get() == nullptr) {
      //c._ca_bucket_lock = std::move(_ca_index->lock(c._ca_hash));
    //}

    // If chunk hits lba and ca, it would check validity in the metadata
    // If the ca is not valid, it means a new chunk and the previous one
    // should be evicted.

    _ca_index->update(c._ca_hash, c._compress_level, c._ssd_location);
    _lba_index->update(c._lba_hash, c._ca_hash);

    _meta_journal->add_update(c);

    // Cases when metadata update is needed
    // for write path:
    // 1. Write_Dup_Content
    // 2. Write_Not_Dup
    // for read path:
    // 1. Read_Not_Hit
    // The thing is "We always need to cache an item when it's
    // not in the cache".
    if (c._lookup_result == WRITE_DUP_CONTENT ||
        c._lookup_result == WRITE_NOT_DUP ||
        c._lookup_result == READ_NOT_HIT) {
      _meta_verification->update(c);
    }

    return ;
  }
}
