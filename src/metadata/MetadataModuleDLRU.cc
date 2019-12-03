#ifdef DLRU

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
      DLRU_SourceIndex::getInstance().init();
      DLRU_FingerprintIndex::getInstance().init();
      std::cout << "SourceIndex capacity: " <<  DLRU_SourceIndex::getInstance().capacity_ << std::endl;
      std::cout << "FingerprintIndex capacity: " << DLRU_FingerprintIndex::getInstance().capacity_ << std::endl;
    }

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
    uint8_t oldFP[20];
    bool evicted = DLRU_SourceIndex::getInstance().update(c.addr_, c.fingerprint_, oldFP);
    if (evicted) {
      if (Config::getInstance().getCachePolicyForFPIndex() == CachePolicyEnum::tGarbageAware) {
        DLRU_FingerprintIndex::getInstance().dereference(oldFP);
      }
    }
    if (Config::getInstance().getCachePolicyForFPIndex() == CachePolicyEnum::tGarbageAware) {
      DLRU_FingerprintIndex::getInstance().reference(c.fingerprint_);
    }
    DLRU_FingerprintIndex::getInstance().update(c.fingerprint_, c.cachedataLocation_);
  }
}

#endif
