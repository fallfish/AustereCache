#ifndef __MANAGEMODULE_H__
#define __MANAGEMODULE_H__

#include "common/common.h"
#include "managemodule.h"
#include "io/iomodule.h"
#include "chunk/chunkmodule.h"
#include "compression/compressionmodule.h"
#include "metadata/metadatamodule.h"

namespace cache {

class ManageModule {
 public:
  ManageModule(std::shared_ptr<IOModule> io_module, std::shared_ptr<MetadataModule> metadata_module);
  int read(Chunk &c);
  int write(Chunk &c);
 private:
  std::shared_ptr<IOModule> _io_module;
  std::shared_ptr<MetadataModule> _metadata_module;
};

}
#endif //__MANAGEMODULE_H__
