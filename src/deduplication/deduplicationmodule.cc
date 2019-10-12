#include "deduplicationmodule.h"
#include "common/stats.h"
#include "utils/utils.h"

namespace cache {

  DeduplicationModule::DeduplicationModule(
      std::shared_ptr<MetadataModule> metadata_module) :
    metadataModule_(metadata_module)
    {}

  void DeduplicationModule::dedup(Chunk &chunk)
  {
    BEGIN_TIMER();
    metadataModule_->dedup(chunk);
    END_TIMER(dedup);
  }

  void DeduplicationModule::lookup(Chunk &chunk)
  {
    BEGIN_TIMER();
    metadataModule_->lookup(chunk);
    END_TIMER(lookup);
  }
}
