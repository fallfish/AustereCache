#ifndef __DEDUP_H__
#define __DEDUP_H__
#include <memory>
#include "common/common.h"
#include "metadata/metadatamodule.h"
#include "compression/compressionmodule.h"
#include "chunk/chunkmodule.h"

namespace cache {
  class DeduplicationModule {
   public:
    DeduplicationModule(std::shared_ptr<MetadataModule> metadata_module);
    // check whether chunk c is duplicate or not
    // return deduplication flag: duplicate_write, duplicate_content, or not duplicate
    void dedup(Chunk &c);
    void lookup(Chunk &c);

   private:
    std::shared_ptr<MetadataModule> _metadata_module;
  };
}



#endif
