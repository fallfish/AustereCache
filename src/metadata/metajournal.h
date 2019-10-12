#ifndef __METAJOURNAL_H__
#define __METAJOURNAL_H__

#include "chunk/chunkmodule.h"
#include "io/iomodule.h"

namespace cache {

class MetaJournal {
 public:
  MetaJournal(std::shared_ptr<IOModule> ioModule);
  void addUpdate(const Chunk &c);
 private:
  std::shared_ptr<IOModule> ioModule_;
};
}

#endif //__METAJOURNAL_H
