#ifndef __METAJOURNAL_H__
#define __METAJOURNAL_H__

#include "chunk/chunkmodule.h"

namespace cache {

class MetaJournal {
 public:
  void add_update(const Chunk &c);
 private:
//  std::shared_ptr<IOModule> _io_module;
};
}

#endif //__METAJOURNAL_H
