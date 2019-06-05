#ifndef __METADATAMODULE_H__
#define __METADATAMODULE_H__
#include "chunk/chunkmodule.h"
#include "index.h"
#include "metaverfication.h"
//#include "metajournal.h"

namespace cache {

  class MetadataModule {
   public:
    // initialize all submodules and start the journalling thread
    MetadataModule();
    void lookup(const Chunk &c, bool write_path);
    void update();

    //std::unique_ptr<MetaJournal> _meta_journal;
    std::unique_ptr<LBAIndex> _lba_index;
    std::unique_ptr<CAIndex> _ca_index;
    std::unique_ptr<MetaVerification> _meta_verfication;
   private:
    lookup_write_path(const Chunk &c);
    lookup_read_path(const Chunk &c);
  };
}

#endif
