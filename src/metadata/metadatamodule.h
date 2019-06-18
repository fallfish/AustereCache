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
  MetadataModule(std::shared_ptr<IOModule> io_module);
  void lookup(Chunk &c, bool write_path);
  void update(Chunk &c);

  std::unique_ptr<LBAIndex> _lba_index;
  std::shared_ptr<CAIndex> _ca_index;
  std::unique_ptr<MetaVerification> _meta_verification;
  std::unique_ptr<MetaJournal> _meta_journal;
 private:
  void lookup_write_path(Chunk &c);
  void lookup_read_path(Chunk &c);
};

}

#endif
