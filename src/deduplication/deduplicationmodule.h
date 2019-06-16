#ifndef __DEDUP_H__
#define __DEDUP_H__
#include <memory>
#include "common/common.h"
#include "metadata/metadatamodule.h"
#include "chunk/chunkmodule.h"

namespace cache {
  class DeduplicationModule {
   public:
    DeduplicationModule(std::shared_ptr<MetadataModule> metadata_module);
    // check whether chunk c is duplicate or not
    // return deduplication flag: duplicate_write, duplicate_content, or not duplicate
    void deduplicate(Chunk &c, bool is_write_path);

   private:
    std::shared_ptr<MetadataModule> _metadata_module;
  };
}



#endif
