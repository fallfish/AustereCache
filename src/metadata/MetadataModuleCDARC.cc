#ifdef CDARC
#include "common/env.h"
#include "MetadataModule.h"
#include "metajournal.h"
#include "frequentslots.h"
#include "utils/utils.h"

namespace cache {
    MetadataModule& MetadataModule::getInstance() {
      static MetadataModule instance;
      return instance;
    }

    MetadataModule::MetadataModule() {
      DARC_SourceIndex::getInstance().init(0, 0);
      CDARC_FingerprintIndex::getInstance().init();
      std::cout << "SourceIndex capacity: " <<  DARC_SourceIndex::getInstance().capacity_ << std::endl;
      std::cout << "FingerprintIndex capacity: " << CDARC_FingerprintIndex::getInstance().capacity_ << std::endl;
    }

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
}
#endif
