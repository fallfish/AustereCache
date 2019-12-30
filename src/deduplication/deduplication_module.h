#ifndef __DEDUP_H__
#define __DEDUP_H__
#include <memory>
#include "common/common.h"
#include "metadata/metadata_module.h"
#include "compression/compression_module.h"
#include "chunking/chunk_module.h"

namespace cache {
  class DeduplicationModule {
    DeduplicationModule();
   public:
    // check whether chunk c is duplicate or not
    // return deduplication flag: duplicate_write, duplicate_content, or not duplicate
    static void dedup(Chunk &chunk);
    static void lookup(Chunk &chunk);

   private:
  };
}



#endif
