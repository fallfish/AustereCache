#include "austere_cache.h"
#include "common/env.h"
#include "common/config.h"

#include "manage/dirtylist.h"
#include "metadata/cachededup/cdarc_fpindex.h"
 

#include <unistd.h>

#include <vector>
#include <thread>
#include <cassert>
#include <csignal>
#include <chrono>

#include <malloc.h>

namespace cache {
    AustereCache::AustereCache()
    {
      IOModule::getInstance().addCacheDevice(Config::getInstance().getCacheDeviceName());
      IOModule::getInstance().addPrimaryDevice(Config::getInstance().getPrimaryDeviceName());
    }

    AustereCache::~AustereCache() {
      Stats::getInstance().dump();
      Stats::getInstance().release();
      Config::getInstance().release();
      if (Config::getInstance().getCacheMode() == tWriteBack) {
        DirtyList::getInstance().shutdown();
      }
#ifdef CDARC
      CDARCFPIndex::getInstance().clearObsolete();
#endif
      // trim all released memory
      malloc_trim(0);
      double vm, rss;
      dumpMemoryUsage(vm, rss);
      std::cout << std::fixed << "VM: " << vm << "; RSS: " << rss << std::endl;
    }

    void AustereCache::read(uint64_t addr, void *buf, uint32_t len)
    {
      Stats::getInstance().setCurrentRequestType(0);
      Chunker chunker = ChunkModule::getInstance().createChunker(addr, buf, len);

      alignas(512) Chunk chunk;
      while (chunker.next(chunk)) {
        internalRead(chunk);
        chunk.fpBucketLock_.reset();
        chunk.lbaBucketLock_.reset();
      }
    }

    void AustereCache::write(uint64_t addr, void *buf, uint32_t len)
    {
      Stats::getInstance().setCurrentRequestType(1);
      Chunker chunker = ChunkModule::getInstance().createChunker(addr, buf, len);
      alignas(512) Chunk c;

      while ( chunker.next(c) ) {
        internalWrite(c);
        c.fpBucketLock_.reset();
        c.lbaBucketLock_.reset();
      }
    }
}
