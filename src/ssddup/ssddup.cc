#include "ssddup.h"
#include "common/env.h"
#include "common/config.h"

#include "manage/dirty_list.h"

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
    double vm, rss;
    dumpMemoryUsage(vm, rss);
    std::cout << "VM: " << vm << "; RSS: " << rss << std::endl;
  }

  void SSDDup::read(uint64_t addr, void *buf, uint32_t len)
  {
    if (Config::getInstance().isMultiThreadEnabled()) {
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

      {
        alignas(512) uint32_t buffer[32768];
        memset(buffer, 0, Config::getInstance().getChunkSize());
        //for (uint32_t offset = 0;
             //offset + Config::getInstance().getFingerprintLength() < Config::getInstance().getChunkSize();
             //offset += Config::getInstance().getFingerprintLength()) {
          //memcpy(buffer, Config::getInstance().getCurrentFingerprint(), Config::getInstance().getFingerprintLength());
        //}
        //if (memcmp(buffer, chunk.buf_, 32768) != 0) {
          //std::cout << "Wrong!" << std::endl;
        //}
      }
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

#if defined(CACHE_DEDUP) && (defined(DLRU) || defined(DARC) || defined(BUCKETDLRU))
  void SSDDup::internalRead(Chunk &chunk)
  {
    DeduplicationModule::lookup(chunk);
    Stats::getInstance().addReadLookupStatistics(chunk);
    // printf("TEST: %s, manageModule_ read\n", __func__);
    ManageModule::getInstance().read(chunk);

    if (chunk.lookupResult_ == NOT_HIT) {
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

  void SSDDup::internalWrite(Chunk &chunk)
  {
    chunk.computeFingerprint();
    DeduplicationModule::dedup(chunk);
    ManageModule::getInstance().updateMetadata(chunk);
    ManageModule::getInstance().write(chunk);
#if defined(WRITE_BACK_CACHE)
    DirtyList::getInstance().addLatestUpdate(chunk.addr_, chunk.cachedataLocation_, chunk.len_);
#endif

    Stats::getInstance().add_write_stat(chunk);
  }
#else
// ACDC or CDARC 
// (CDARC needs also compression but no compress level optimization)
// Will deal with it in compression module
  void SSDDup::internalRead(Chunk &chunk)
  {
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
    if (chunk.lookupResult_ == HIT && chunk.compressedLen_ != 0) {
      // hit the cache
      CompressionModule::decompress(chunk);
    }

    if (chunk.lookupResult_ == NOT_HIT) {
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

  void SSDDup::internalWrite(Chunk &chunk)
  {
    chunk.lookupResult_ = LOOKUP_UNKNOWN;
    alignas(512) uint8_t tempBuf[Config::getInstance().getChunkSize()];
    chunk.compressedBuf_ = tempBuf;

    {
      chunk.computeFingerprint();
      DeduplicationModule::dedup(chunk);
      if (chunk.dedupResult_ == NOT_DUP) {
        CompressionModule::compress(chunk);
      }
      ManageModule::getInstance().updateMetadata(chunk);
#if defined(WRITE_BACK_CACHE)
      DirtyList::getInstance().addLatestUpdate(chunk.addr_,
        chunk.cachedataLocation_,
        (chunk.compressedLevel_ + 1) * Config::getInstance().getSectorSize());
#endif
      ManageModule::getInstance().write(chunk);
      Stats::getInstance().add_write_stat(chunk);
    }
  }
#endif
}
