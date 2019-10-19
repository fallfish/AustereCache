#ifndef __METAJOURNAL_H__
#define __METAJOURNAL_H__

#include "chunk/chunkmodule.h"
#include "io/iomodule.h"

namespace cache {

class MetaJournal {
 public:
  MetaJournal();
  void addUpdate(const Chunk &c);
};
}

#endif //__METAJOURNAL_H
