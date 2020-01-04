#ifndef __METAJOURNAL_H__
#define __METAJOURNAL_H__

#include "chunking/chunk_module.h"
#include "io/io_module.h"

namespace cache {

class MetaJournal {
 public:
  MetaJournal();
  void addUpdate(const Chunk &c);
  void recover();
  FILE *journal_;
};
}

#endif //__METAJOURNAL_H
