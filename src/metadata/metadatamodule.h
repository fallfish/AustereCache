#ifndef __METADATAMODULE_H__
#define __METADATAMODULE_H__
#include "common/common.h"
#include "chunk/chunkmodule.h"
#include "index.h"
#include "metaverification.h"
#include "metajournal.h"

namespace cache {

class MetadataModule {
 public:
  // initialize all submodules and start the journalling thread
  MetadataModule(std::shared_ptr<IOModule> ioModule,
      std::shared_ptr<CompressionModule> compressionModule);
  void dedup(Chunk &c);
  void lookup(Chunk &c);
  void update(Chunk &c);

  std::unique_ptr<LBAIndex> lbaIndex_;
  std::shared_ptr<FPIndex> fpIndex_;
  std::unique_ptr<MetaVerification> metaVerification_;
  std::unique_ptr<MetaJournal> metaJournal_;
 private:
  std::mutex mutex_;
};

}

#endif
