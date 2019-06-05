#include "metadatamodule.h"

namespace cache {
  uint32_t MetadataModule::lookup(const Chunk &c, bool has_ca)
  {
    if (with_ca) {
      return lookup_with_ca(c);
    } else {
      return lookup_without_ca(c);
    }
  }

  uint32_t MetadataModule::lookup_write_path(const Chunk &c)
  {
    uint32_t ca_hash;
    bool lba_hit = false, ca_hit = false;
    uint32_t ssd_location, valid = 0;
    lba_hit = _lba_index->lookup(&c.lba_hash, &ca_hash);
    ca_hit = _ca_index->lookup(&c.ca_hash, &ssd_location);

    if (ca_hash == c.ca_hash && ca_hit && lba_hit) {
      // there is three types of validation
      // 1. lba valid and ca valid, valid = 2
      // 2. ca valid but lba not valid, valid = 1
      // 3. ca not valid, valid = 0
      valid = _meta_verification->verify(c.lba, c.ca, ssd_location);
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

    lba_hit = _lba_index->lookup(&c.lba_hash, &ca_hash);
    if (lba_hit) {
      ca_hit = _ca_index->lookup(&ca_hash, &ssd_location);
      if (ca_hit) {
        valid = _meta_verification->verify(c.lba, ssd_location);
      }
    }

    c.ssd_loaction = ssd_loaction;
    return (lba_hit && ca_hit && valid);
  }

  uint32_t MetadataModule::update(const Chunk &c)
  {
    uint32_t ssd_loaction;
    _ca_index->update(&c.ca_hash, &c.compressed_factor, &ssd_location);    
    _lba_index->update(&lba_hash, &c.ca_hash);

    c.ssd_loaction = ssd_location; 
  }
}
