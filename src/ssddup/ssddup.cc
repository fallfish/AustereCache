#include "ssddup.h"
#include "common/env.h"
#include "common/config.h"

#include "manage/DirtyList.h"
#include "metadata/cacheDedup/CDARCFPIndex.h"

#include <unistd.h>

#include <vector>
#include <thread>
#include <cassert>
#include <csignal>
#include <chrono>

#include <malloc.h>

namespace cache {
    SSDDup::SSDDup()
    {
      double vm, rss;
      dumpMemoryUsage(vm, rss);
      std::cout << "VM: " << vm << "; RSS: " << rss << std::endl;
      std::cout << sizeof(Metadata) << std::endl;
      IOModule::getInstance().addCacheDevice(Config::getInstance().getCacheDeviceName());
      IOModule::getInstance().addPrimaryDevice(Config::getInstance().getPrimaryDeviceName());
      threadPool_ = std::make_unique<AThreadPool>(Config::getInstance().getMaxNumLocalThreads());
    }

    SSDDup::~SSDDup() {
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

    void SSDDup::read(uint64_t addr, void *buf, uint32_t len)
    {
      if (Config::getInstance().isMultiThreadingEnabled()) {
        readMultiThread(addr, buf, len);
      } else {
        readSingleThread(addr, buf, len);
      }
    }

    /**
     * (Commented by jhli)
     * Read using multi-thread
     */
    void SSDDup::readMultiThread(uint64_t addr, void *buf, uint32_t len)
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
     * (Commented by jhli)
     * Read using single-thread
     */
    void SSDDup::readSingleThread(uint64_t addr, void *buf, uint32_t len)
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

    void SSDDup::writeMultiThread(uint64_t addr, void *buf, uint32_t len)
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


    void SSDDup::writeSingleThread(uint64_t addr, void *buf, uint32_t len)
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

    void SSDDup::write(uint64_t addr, void *buf, uint32_t len)
    {
      if (Config::getInstance().isMultiThreadingEnabled()) {
        writeMultiThread(addr, buf, len);
      } else {
        writeSingleThread(addr, buf, len);
      }
    }
}
