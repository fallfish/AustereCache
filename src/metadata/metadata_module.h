#ifndef __METADATAMODULE_H__
#define __METADATAMODULE_H__
#include "common/common.h"
#include "chunking/chunk_module.h"
#include "index.h"
#include "meta_verification.h"
#include "meta_journal.h"

namespace cache {

class MetadataModule {
 public:
  static MetadataModule& getInstance();
  ~MetadataModule();
  void dedup(Chunk &chunk);
  void lookup(Chunk &chunk);
  void update(Chunk &chunk);
  void dumpStats();
  void recoverLbaIndex(uint64_t lbaHash, uint64_t fpHash);
  void recoverFpIndex(uint64_t fpHash, uint64_t cachedataLocation, uint32_t nSlotsOccupied);
  void recover();

  std::shared_ptr<LBAIndex> lbaIndex_;
  std::shared_ptr<FPIndex> fpIndex_;
  std::unique_ptr<MetaVerification> metaVerification_;
  std::unique_ptr<MetaJournal> metaJournal_;
 private:
  MetadataModule();
};

}

#endif
