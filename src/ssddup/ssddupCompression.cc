#include <manage/DirtyList.h>
#include "ssddup.h"

#if defined(ACDC) || defined(CDARC)
namespace cache {
// ACDC or CDARC
// (CDARC needs also compression but no compress level optimization)
// Will deal with it in compression module
    void SSDDup::internalRead(Chunk &chunk) {
      // construct compressed buffer for chunk chunk
      // When the cache is hit, compressedBuf stores the data retrieved from ssd
      alignas(512) uint8_t compressedBuf[Config::getInstance().getChunkSize()];
      chunk.compressedBuf_ = compressedBuf;

      // look up index
      DeduplicationModule::lookup(chunk);
      {
        // record status
        Stats::getInstance().addReadLookupStatistics(chunk);
      }
      // read from ssd or hdd according to the lookup result
      ManageModule::getInstance().read(chunk);
      if (chunk.lookupResult_ == HIT) {
        // hit the cache
        CompressionModule::decompress(chunk);
      }

      if (chunk.lookupResult_ == NOT_HIT) {
        Stats::getInstance().add_total_bytes_written_to_ssd(chunk.len_);
        // dedup the data, the same procedure as in the write
        // process.
        chunk.computeFingerprint();
        CompressionModule::compress(chunk);
        DeduplicationModule::dedup(chunk);
        ManageModule::getInstance().updateMetadata(chunk);
        if (chunk.dedupResult_ == NOT_DUP) {
          // write compressed data into cache device
          ManageModule::getInstance().write(chunk);
        }
        Stats::getInstance().add_read_post_dedup_stat(chunk);
      } else {
        ManageModule::getInstance().updateMetadata(chunk);
      }
    }

    void SSDDup::internalWrite(Chunk &chunk) {
      chunk.lookupResult_ = LOOKUP_UNKNOWN;
      alignas(512) uint8_t tempBuf[Config::getInstance().getChunkSize()];
      chunk.compressedBuf_ = tempBuf;

      Stats::getInstance().add_total_bytes_written_to_ssd(chunk.len_);
      {
        chunk.computeFingerprint();
        DeduplicationModule::dedup(chunk);
        if (chunk.dedupResult_ == NOT_DUP) {
          CompressionModule::compress(chunk);
        }
        ManageModule::getInstance().updateMetadata(chunk);
#ifdef CACHE_DEDUP
        if (Config::getInstance().getCacheMode() == tWriteBack) {
          DirtyList::getInstance().addLatestUpdate(chunk.addr_,
              ((uint64_t)chunk.weuId_ << 32u) | chunk.weuOffset_,
              chunk.compressedLen_);
        }
#endif
        ManageModule::getInstance().write(chunk);
        Stats::getInstance().add_write_stat(chunk);
      }
    }
}
#endif
