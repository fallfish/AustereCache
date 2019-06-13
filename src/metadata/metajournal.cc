#include "metajournal.h"
namespace cache {

MetaJournal::MetaJournal(std::shared_ptr<cache::IOModule> io_module) :
  _io_module(io_module)
{}

void MetaJournal::add_update(const cache::Chunk &c)
{
  // pack and write lba, ca, and lookup_result into a log
}

}
