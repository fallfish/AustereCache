#include "deduplicationmodule.h"

namespace cache {

  DeduplicationModule::DeduplicationModule(MetadataModule *metadata_module) :
    _metadata_module(metadata_module)
    {}

  uint32_t DeduplicationModule::deduplicate(Chunk &c)
  {
    //_metadata_module->lookup(c);
    return 0;
  }

}