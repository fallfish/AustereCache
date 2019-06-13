#ifndef __METAJOURNAL_H__
#define __METAJOURNAL_H__

#include "chunk/chunkmodule.h"
#include "io/iomodule.h"

namespace cache {

class MetaJournal {
 public:
  MetaJournal(std::shared_ptr<IOModule> io_module);
  void add_update(const Chunk &c);
 private:
  std::shared_ptr<IOModule> _io_module;
};
}

#endif //__METAJOURNAL_H
