#include "metajournal.h"
namespace cache {

MetaJournal::MetaJournal() = default;

void MetaJournal::addUpdate(const cache::Chunk &c)
{
  // pack and write lba, ca, and lookup_result into a log
}

}
