#include "meta_journal.h"
namespace cache {

MetaJournal::MetaJournal() = default;

void MetaJournal::addUpdate(const cache::Chunk &c)
{
  if (Config::getInstance().isJournalEnabled()) {
    uint8_t record[16];
    if (c.dedupResult_ == DUP_CONTENT || c.dedupResult_ == NOT_DUP) {
      memcpy(record, &c.addr_, 8);
      memcpy(record + 8, &c.cachedataLocation_, 8);
      IOModule::getInstance().write(DeviceType::JOURNAL, 0, record, 16);
    }
  }
}
}
