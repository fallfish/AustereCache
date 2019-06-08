#include "metadatamodule.h"

namespace cache {
  MetadataModule::MetadataModule() {
    _ca_index = std::make_shared<CAIndex>(12, 4, 1024, 32);
    _lba_index = std::make_unique<LBAIndex>(12, 12, 1024, 32);
    _meta_verification = std::make_unique<MetaVerfication>();
  }
  uint32_t MetadataModule::lookup(const Chunk &c, bool write_path)
  {
    if (write_path) {
      return lookup_write_path(c);
    } else {
      return lookup_read_path(c);
    }
  }

  uint32_t MetadataModule::lookup_write_path(const Chunk &c)
  {
    uint32_t ca_hash;
    bool lba_hit = false, ca_hit = false;
    uint32_t ssd_location, valid = 0;
    lba_hit = _lba_index->lookup(&c._lba_hash, &ca_hash);
    ca_hit = _ca_index->lookup(&c._ca_hash, &ssd_location);

    if (ca_hash == c.ca_hash && ca_hit && lba_hit) {
      // there is three types of validation
      // 1. lba valid and ca valid, valid = 2
      // 2. ca valid but lba not valid, valid = 1
      // 3. ca not valid, valid = 0
      valid = _meta_verification->verify(c._lba, c._ca, ssd_location);
    }

    c.ssd_loaction = ssd_loaction;
    if (ca_hit && lba_hit && valid == 2) {
      // duplicate write
      return 2;
    } else if (ca_hit && valid == 1) {
      // duplicate content
      return 1;
    } else {
      // not duplicate
      return 0;
    }
  }

  uint32_t MetadataModule::lookup_read_path(const Chunk &c)
  {
    uint32_t ca_hash;
    bool lba_hit = false, ca_hit = false;
    uint32_t ssd_location, valid = 0;

    lba_hit = _lba_index->lookup(&c._lba_hash, &ca_hash);
    if (lba_hit) {
      ca_hit = _ca_index->lookup(&ca_hash, &ssd_location);
      if (ca_hit) {
        valid = _meta_verification->verify(c._lba, ssd_location);
      }
    }

    c._ssd_loaction = ssd_location; 
    return (lba_hit && ca_hit && valid);
  }

  uint32_t MetadataModule::update(const Chunk &c)
  {
    _ca_index->update(&c._ca_hash, c._size, c._ssd_location);    
    _lba_index->update(&lba_hash, c._ca_hash);

    return 0;
  }
}
