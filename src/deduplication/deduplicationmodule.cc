#include "deduplicationmodule.h"
#include "common/stats.h"
#include "utils/utils.h"

namespace cache {

  DeduplicationModule::DeduplicationModule(
      std::shared_ptr<MetadataModule> metadata_module) :
    _metadata_module(metadata_module)
    {}

  void DeduplicationModule::dedup(Chunk &c)
  {
    BEGIN_TIMER();
    _metadata_module->dedup(c);
    END_TIMER(dedup);
  }

  void DeduplicationModule::lookup(Chunk &c)
  {
    BEGIN_TIMER();
    _metadata_module->lookup(c);
    END_TIMER(lookup);
  }
}
