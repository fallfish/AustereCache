#ifdef DLRU

#include "common/env.h"
#include "metadata_module.h"
#include "meta_journal.h"
 
#include "common/config.h"
#include "common/stats.h"
#include "utils/utils.h"
#include <cassert>
#include "metadata/cachededup/dlru_lbaindex.h"
#include "metadata/cachededup/dlru_fpindex.h"

namespace cache {
  MetadataModule& MetadataModule::getInstance() {
    static MetadataModule instance;
    return instance;
  }

  MetadataModule::MetadataModule() {
    DLRULBAIndex::getInstance().init();
    DLRUFPIndex::getInstance().init();
    std::cout << "SourceIndex capacity: " << DLRULBAIndex::getInstance().capacity_ << std::endl;
    std::cout << "FingerprintIndex capacity: " << DLRUFPIndex::getInstance().capacity_ << std::endl;
  }

  MetadataModule::~MetadataModule() = default;

  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = DLRUFPIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    if (c.hitFPIndex_)
      c.dedupResult_ = DUP_CONTENT;
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DLRULBAIndex::getInstance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = DLRUFPIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    uint8_t oldFP[20];
    DLRULBAIndex::getInstance().update(c.addr_, c.fingerprint_, oldFP);
    DLRUFPIndex::getInstance().update(c.fingerprint_, c.cachedataLocation_);
  }

}

#endif
