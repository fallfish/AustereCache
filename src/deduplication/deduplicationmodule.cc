#include "deduplicationmodule.h"

namespace cache {

  DeduplicationModule::DeduplicationModule(std::shared_ptr<MetadataModule> metadata_module) :
    _metadata_module(metadata_module)
    {}

  LookupResult DeduplicationModule::deduplicate(Chunk &c, bool is_write_path)
  {
    return _metadata_module->lookup(c, is_write_path);
  }

}
