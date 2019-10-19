#include "deduplicationmodule.h"
#include "common/stats.h"
#include "utils/utils.h"

namespace cache {

  DeduplicationModule::DeduplicationModule() = default;

  void DeduplicationModule::dedup(Chunk &chunk)
  {
    BEGIN_TIMER();
    MetadataModule::getInstance().dedup(chunk);
    END_TIMER(dedup);
  }

  void DeduplicationModule::lookup(Chunk &chunk)
  {
    BEGIN_TIMER();
    MetadataModule::getInstance().lookup(chunk);
    END_TIMER(lookup);
  }

}
