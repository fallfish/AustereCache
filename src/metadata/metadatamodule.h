#ifndef __METADATAMODULE_H__
#define __METADATAMODULE_H__
#include "chunk/chunkmodule.h"
#include "index.h"
//#include "metaverfication.h"
//#include "metajournal.h"

namespace cache {

  // 512 bytes
  class Metadata {
    uint8_t ca[256];
    uint32_t lbas[64]; // 4 * 32
  };

  class MetadataModule {
   public:
    // initialize all submodules and start the journalling thread
    MetadataModule();
    void lookup(const Chunk &c, bool write_path);
    void update(const Chunk &c);

    //std::unique_ptr<MetaJournal> _meta_journal;
    std::unique_ptr<LBAIndex> _lba_index;
    std::unique_ptr<CAIndex> _ca_index;
    //std::unique_ptr<MetaVerification> _meta_verfication;
   private:
    void lookup_write_path(const Chunk &c);
    void lookup_read_path(const Chunk &c);
  };
}

#endif
