#ifdef DARC

#include <metadata/cachededup/DARCFPIndex.h>
#include <metadata/cachededup/DARCLBAIndex.h>
#include "common/env.h"
#include "metadata_module.h"
#include "metajournal.h"
 
#include "utils/utils.h"

namespace cache {
    MetadataModule& MetadataModule::getInstance() {
      static MetadataModule instance;
      return instance;
    }
    MetadataModule::~MetadataModule() {
      std::cout << "Dup ratio: " << DARCLBAIndex::getInstance().getDupRatio() << std::endl;
    }

    MetadataModule::MetadataModule() {
      DARCLBAIndex::getInstance().init(0, 0);
      DARCFPIndex::getInstance().init();
      std::cout << "SourceIndex capacity: " << DARCLBAIndex::getInstance().capacity_ << std::endl;
      std::cout << "FingerprintIndex capacity: " << DARCFPIndex::getInstance().capacity_ << std::endl;
    }

    void MetadataModule::dedup(Chunk &c)
    {
      c.hitFPIndex_ = DARCFPIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
      if (c.hitFPIndex_)
        c.dedupResult_ = DUP_CONTENT;
      else
        c.dedupResult_ = NOT_DUP;
    }
    void MetadataModule::lookup(Chunk &c)
    {
      c.hitLBAIndex_ = DARCLBAIndex::getInstance().lookup(c.addr_, c.fingerprint_);
      if (c.hitLBAIndex_) {
        c.hitFPIndex_ = DARCFPIndex::getInstance().lookup(c.fingerprint_, c.cachedataLocation_);
      }
      if (c.hitLBAIndex_ && c.hitFPIndex_)
        c.lookupResult_ = HIT;
      else
        c.lookupResult_ = NOT_HIT;
    }
    void MetadataModule::update(Chunk &c)
    {
      DARCLBAIndex::getInstance().adjust_adaptive_factor(c.addr_);
      DARCFPIndex::getInstance().update(c.addr_, c.fingerprint_, c.cachedataLocation_);
      DARCLBAIndex::getInstance().update(c.addr_, c.fingerprint_);
    }
}
#endif
