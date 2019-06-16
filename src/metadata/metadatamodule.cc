#include "metadatamodule.h"
#include "metaverification.h"
#include "metajournal.h"
#include "common/config.h"

namespace cache {
  MetadataModule::MetadataModule(std::shared_ptr<IOModule> io_module) {
    uint32_t a = Config::ca_signature_len, b = Config::ca_bucket_no_len,
      c = Config::lba_signature_len, d = Config::lba_bucket_no_len;
    _ca_index = std::make_shared<CAIndex>(
      a, 4, (1 << b), 32);
    _lba_index = std::make_unique<LBAIndex>(
      c, (a + b), (1 << d), 32, _ca_index);
    // _meta_verification and _meta_journal should
    // hold a shared_ptr to _io_module
    _meta_verification = std::make_unique<MetaVerification>(io_module);
    _meta_journal = std::make_unique<MetaJournal>(io_module);
  }

  LookupResult MetadataModule::lookup(Chunk &c, bool write_path)
  {
    if (write_path) {
      return lookup_write_path(c);
    } else {
      return lookup_read_path(c);
    }
  }

  LookupResult MetadataModule::lookup_write_path(Chunk &c)
  {
    uint32_t ca_hash;
    bool lba_hit = false, ca_hit = false;
//    uint32_t ssd_location;
    lba_hit = _lba_index->lookup(c._lba_hash, ca_hash);
    ca_hit = _ca_index->lookup(c._ca_hash, c._compress_level, c._ssd_location);

    VerificationResult verification_result = VerificationResult::VERIFICATION_UNKNOWN;
    if (ca_hit) {
      verification_result = _meta_verification->verify(c);
      if (verification_result != BOTH_LBA_AND_CA_VALID &&
          verification_result != ONLY_CA_VALID) {
        _ca_index->erase(c._ca_hash);
      }
    }

    if (ca_hit && lba_hit && verification_result == BOTH_LBA_AND_CA_VALID) {
      // duplicate write
      return LookupResult::WRITE_DUP_WRITE;
    } else if (ca_hit &&
               (verification_result == ONLY_CA_VALID ||
                verification_result == BOTH_LBA_AND_CA_VALID)){
      // duplicate content
      return LookupResult::WRITE_DUP_CONTENT;
    } else {
      // not duplicate
      return LookupResult::WRITE_NOT_DUP;
    }
  }

  LookupResult MetadataModule::lookup_read_path(Chunk &c)
  {
    uint32_t ca_hash;
    bool lba_hit = false, ca_hit = false;
    VerificationResult verification_result = VerificationResult::VERIFICATION_UNKNOWN;

    lba_hit = _lba_index->lookup(c._lba_hash, c._ca_hash);
    if (lba_hit) {
      ca_hit = _ca_index->lookup(c._ca_hash, c._compress_level, c._ssd_location);
      if (ca_hit) {
        verification_result = _meta_verification->verify(c);
      }
    }

    if (verification_result == VerificationResult::ONLY_LBA_VALID) {
      return LookupResult::READ_HIT;
    } else {
      return LookupResult::READ_NOT_HIT;
    }
  }

  void MetadataModule::update(Chunk &c)
  {
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
