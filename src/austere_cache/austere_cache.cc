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
      double vm, rss;
      dumpMemoryUsage(vm, rss);
      std::cout << "VM: " << vm << "; RSS: " << rss << std::endl;
      std::cout << sizeof(Metadata) << std::endl;
      IOModule::getInstance().addCacheDevice(Config::getInstance().getCacheDeviceName());
      IOModule::getInstance().addPrimaryDevice(Config::getInstance().getPrimaryDeviceName());
      threadPool_ = std::make_unique<AThreadPool>(Config::getInstance().getMaxNumLocalThreads());
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
      if (Config::getInstance().isCacheDisabled()) {
        readDirectly(addr, buf, len);
      } else {
        readSingleThread(addr, buf, len);
      }
    }

    /**
     * Read using multi-thread
     */
    void AustereCache::readMultiThread(uint64_t addr, void *buf, uint32_t len)
    {
      Chunker chunker = ChunkModule::getInstance().createChunker(addr, buf, len);

      std::vector<int> threads;
      uint64_t tmp_addr;
      uint8_t *tmp_buf;
      uint32_t tmp_len;

      while (chunker.next(tmp_addr, tmp_buf, tmp_len)) {
        readSingleThread(tmp_addr, tmp_buf, tmp_len);
      }
    }


    /**
     * Read using single-thread
     */
    void AustereCache::readSingleThread(uint64_t addr, void *buf, uint32_t len)
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

    void AustereCache::writeMultiThread(uint64_t addr, void *buf, uint32_t len)
    {
      Chunker chunker = ChunkModule::getInstance().createChunker(addr, buf, len);

      std::vector<int> threads;
      uint64_t tmp_addr;
      uint8_t *tmp_buf;
      uint32_t tmp_len;

      while (chunker.next(tmp_addr, tmp_buf, tmp_len)) {
        //threadPool_->doJob( [this, tmp_addr, tmp_buf, tmp_len]() {
            writeSingleThread(tmp_addr, tmp_buf, tmp_len);
        //});
      }
    }


    void AustereCache::writeSingleThread(uint64_t addr, void *buf, uint32_t len)
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

    void AustereCache::write(uint64_t addr, void *buf, uint32_t len)
    {
      if (Config::getInstance().isCacheDisabled()) {
        writeDirectly(addr, buf, len);
      } else {
        writeSingleThread(addr, buf, len);
      }
    }

    void AustereCache::readDirectly(uint64_t addr, void *buf, uint32_t len) {
      IOModule::getInstance().read(PRIMARY_DEVICE, addr, buf, len);
    }

    void AustereCache::writeDirectly(uint64_t addr, void *buf, uint32_t len) {
      IOModule::getInstance().write(PRIMARY_DEVICE, addr, buf, len);
    }
}
