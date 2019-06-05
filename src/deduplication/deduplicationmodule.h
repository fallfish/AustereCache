#ifndef __DEDUP_H__
#define __DEDUP_H__
#include <memory>
#include "metadata/metadatamodule.h"
#include "chunk/chunkmodule.h"

namespace cache {
  class DeduplicationModule {
    DeduplicationModule(MetadataModule *metadata_module);
    // check whether chunk c is duplicate or not
    // return deduplication flag: duplicate_write, duplicate_content, or not duplicate
    uint32_t deduplicate(Chunk &c);

   private:
    std::shared_ptr<MetadataModule> _metadata_module;
  };
}



#endif
