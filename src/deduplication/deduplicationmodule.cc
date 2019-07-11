#include "deduplicationmodule.h"

namespace cache {

  DeduplicationModule::DeduplicationModule(
      std::shared_ptr<MetadataModule> metadata_module) :
    _metadata_module(metadata_module)
    {}

  void DeduplicationModule::dedup(Chunk &c)
  {
    _metadata_module->dedup(c);
  }

  void DeduplicationModule::lookup(Chunk &c)
  {
    _metadata_module->lookup(c);
  }

}
