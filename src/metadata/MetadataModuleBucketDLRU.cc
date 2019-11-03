#ifdef BUCKETDLRU

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

    MetadataModule::MetadataModule() = default;

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
    uint8_t oldFP[20];
    memset(oldFP, 0, sizeof(oldFP));
    bool evicted = BucketizedDLRU_SourceIndex::getInstance().update(c.addr_, c.fingerprint_, oldFP);
    if (evicted) {
      BucketizedDLRU_FingerprintIndex::getInstance().dereference(oldFP);
    }
    BucketizedDLRU_FingerprintIndex::getInstance().reference(c.fingerprint_);
    BucketizedDLRU_FingerprintIndex::getInstance().update(c.fingerprint_, c.cachedataLocation_);
  }
}

#endif
