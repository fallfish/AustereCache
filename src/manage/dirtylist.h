#ifndef __DIRTY_LIST__
#define __DIRTY_LIST__

#include "compression/compression_module.h"
#include "io/io_module.h"

#include <map>
#include <list>
#include <condition_variable>

namespace cache {

  class DirtyList {

    public:
      DirtyList();
      void setCompressionModule(std::shared_ptr<CompressionModule> compressionModule);

      static DirtyList& getInstance();

      struct EvictedBlock {
        uint64_t cachedataLocation_;
        uint32_t len_;
      };


      void addLatestUpdate(uint64_t lba, uint64_t cachedataLocation, uint32_t len);
      void addEvictedChunk(uint64_t cachedataLocation, uint32_t len);
      void flush();
      void flushOneLba(uint64_t lba, uint64_t cachedataLocation, Metadata &metadata);
      void flushOneBlock(uint64_t cachedataLocation, uint32_t len);

      void shutdown();

    private:
      // logical block address to cache data pointer and length
      std::map<uint64_t, std::pair<uint64_t, uint32_t>> latestUpdates_;
      std::map<uint64_t, uint64_t> latestUpdatesMetadataLocations_;
      std::list<EvictedBlock> evictedBlocks_;
      std::shared_ptr<CompressionModule> compressionModule_;
      uint64_t size_;
      std::mutex mutex_;
      std::mutex listMutex_;
      std::condition_variable condVar_;
      bool shutdown_;
  };
}

#endif
