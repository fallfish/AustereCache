#ifndef __METADATAMODULE_H__
#define __METADATAMODULE_H__
#include "common.h"
#include "chunk/chunkmodule.h"
#include "index.h"
#include "metaverification.h"
#include "metajournal.h"

namespace cache {

class MetadataModule {
 public:
  // initialize all submodules and start the journalling thread
  MetadataModule();
  LookupResult lookup(Chunk &c, bool write_path);
  void update(Chunk &c, LookupResult lookup_result);

  std::unique_ptr<LBAIndex> _lba_index;
  std::shared_ptr<CAIndex> _ca_index;
  std::unique_ptr<MetaVerification> _meta_verification;
  std::unique_ptr<MetaJournal> _meta_journal;
 private:
  LookupResult lookup_write_path(Chunk &c);
  LookupResult lookup_read_path(Chunk &c);
};

}

#endif
