#include "ssddup.h"
#include "common/env.h"
#include "common/config.h"

#include "manage/DirtyList.h"

#include <unistd.h>

#include <vector>
#include <thread>
#include <cassert>
#include <csignal>
#include <chrono>

namespace cache {
  SSDDup::SSDDup()
  {
    double vm, rss;
    dumpMemoryUsage(vm, rss);
    std::cout << "VM: " << vm << "; RSS: " << rss << std::endl;
    std::cout << sizeof(Metadata) << std::endl;
    IOModule::getInstance().addCacheDevice(Config::getInstance().getCacheDeviceName());
    IOModule::getInstance().addPrimaryDevice(Config::getInstance().getPrimaryDeviceName());
    threadPool_ = std::make_unique<ThreadPool>(Config::getInstance().getMaxNumGlobalThreads());
  }

  SSDDup::~SSDDup() {
    Stats::getInstance().dump();
    Stats::getInstance().release();
    Config::getInstance().release();
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
    int thread_id;

    while (chunker.next(tmp_addr, tmp_buf, tmp_len)) {
      if (threads.size() == Config::getInstance().getMaxNumLocalThreads()) {
        threadPool_->wait_and_return_threads(threads);
      }
      while ((thread_id = threadPool_->allocate_thread()) == -1) {
        threadPool_->wait_and_return_threads(threads);
      }
      threadPool_->get_thread(thread_id) = std::make_shared<std::thread>(
          [this, tmp_addr, tmp_buf, tmp_len]() {
          read(tmp_addr, tmp_buf, tmp_len);
          });
      threads.push_back(thread_id);
    }
    threadPool_->wait_and_return_threads(threads);
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


  void SSDDup::write(uint64_t addr, void *buf, uint32_t len)
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
