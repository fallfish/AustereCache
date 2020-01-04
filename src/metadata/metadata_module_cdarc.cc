#ifdef CDARC

#include <metadata/cachededup/darc_lbaindex.h>
#include <metadata/cachededup/cdarc_fpindex.h>
#include "common/env.h"
#include "metadata_module.h"

#include "utils/utils.h"

namespace cache {
  MetadataModule& MetadataModule::getInstance() {
    static MetadataModule instance;
    return instance;
  }
  MetadataModule::~MetadataModule() {
    CDARCFPIndex::getInstance().dumpStats();
  }

  MetadataModule::MetadataModule() {
    DARCLBAIndex::getInstance().init(0, 0);
    CDARCFPIndex::getInstance().init();
    std::cout << "SourceIndex capacity: " << DARCLBAIndex::getInstance().capacity_ << std::endl;
    std::cout << "FingerprintIndex capacity: " << CDARCFPIndex::getInstance().capacity_ << std::endl;
  }

  void MetadataModule::dedup(Chunk &c)
  {
    c.hitFPIndex_ = CDARCFPIndex::getInstance().lookup(c.fingerprint_, c.weuId_, c.weuOffset_, c.compressedLen_);
    if (c.hitFPIndex_) {
      c.dedupResult_ = DUP_CONTENT;
    }
    else
      c.dedupResult_ = NOT_DUP;
  }
  void MetadataModule::lookup(Chunk &c)
  {
    c.hitLBAIndex_ = DARCLBAIndex::getInstance().lookup(c.addr_, c.fingerprint_);
    if (c.hitLBAIndex_) {
      c.hitFPIndex_ = CDARCFPIndex::getInstance().lookup(c.fingerprint_, c.weuId_, c.weuOffset_, c.compressedLen_);
    }
    if (c.hitLBAIndex_ && c.hitFPIndex_)
      c.lookupResult_ = HIT;
    else
      c.lookupResult_ = NOT_HIT;
  }
  void MetadataModule::update(Chunk &c)
  {
    DARCLBAIndex::getInstance().adjust_adaptive_factor(c.addr_);
    c.evictedWEUId_ = CDARCFPIndex::getInstance().update(c.addr_, c.fingerprint_, c.weuId_, c.weuOffset_, c.compressedLen_);
    DARCLBAIndex::getInstance().update(c.addr_, c.fingerprint_);
  }
}
#endif
