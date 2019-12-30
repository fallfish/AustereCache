#include <manage/dirtylist.h>
#include "austere_cache.h"

#if defined(CACHE_DEDUP) && (defined(DLRU) || defined(DARC) || defined(BUCKETDLRU))
namespace cache {
  void AustereCache::internalRead(Chunk &chunk)
  {
    DeduplicationModule::lookup(chunk);
    Stats::getInstance().addReadLookupStatistics(chunk);
    // printf("TEST: %s, manageModule_ read\n", __func__);
    ManageModule::getInstance().read(chunk);

    if (chunk.lookupResult_ == NOT_HIT) {
      Stats::getInstance().add_total_bytes_written_to_ssd(chunk.len_);
      chunk.computeFingerprint();
      DeduplicationModule::dedup(chunk);
      ManageModule::getInstance().updateMetadata(chunk);
      if (chunk.dedupResult_ == NOT_DUP) {
        ManageModule::getInstance().write(chunk);
      }

      Stats::getInstance().add_read_post_dedup_stat(chunk);
    } else {
      ManageModule::getInstance().updateMetadata(chunk);
    }
  }

  void AustereCache::internalWrite(Chunk &chunk)
  {
    Stats::getInstance().add_total_bytes_written_to_ssd(chunk.len_);
    chunk.computeFingerprint();
    DeduplicationModule::dedup(chunk);
    ManageModule::getInstance().updateMetadata(chunk);
    ManageModule::getInstance().write(chunk);

    if (Config::getInstance().getCacheMode() == tWriteBack) {
      DirtyList::getInstance().addLatestUpdate(chunk.addr_, chunk.cachedataLocation_, chunk.len_);
    }
    Stats::getInstance().add_write_stat(chunk);
  }
}
#endif
