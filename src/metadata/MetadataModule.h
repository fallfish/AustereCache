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
  static MetadataModule& getInstance();
  ~MetadataModule();
  // initialize all submodules and start the journalling thread
  void dedup(Chunk &chunk);
  void lookup(Chunk &chunk);
  void update(Chunk &chunk);
  void dumpStats();

  std::unique_ptr<LBAIndex> lbaIndex_;
  std::shared_ptr<FPIndex> fpIndex_;
  std::unique_ptr<MetaVerification> metaVerification_;
  std::unique_ptr<MetaJournal> metaJournal_;
 private:
  MetadataModule();
};

}

#endif
