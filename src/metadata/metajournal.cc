#include "metajournal.h"
namespace cache {

MetaJournal::MetaJournal(std::shared_ptr<cache::IOModule> ioModule) :
  ioModule_(ioModule)
{}

void MetaJournal::addUpdate(const cache::Chunk &c)
{
  // pack and write lba, ca, and lookup_result into a log
}

}
