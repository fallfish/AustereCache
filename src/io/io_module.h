#ifndef __IOMODULE_H__
#define __IOMODULE_H__
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>
#include <list>
#include <set>
#include <algorithm>
#include <io/writebuffer.h>
#include "common/env.h"
#include "common/common.h"
#include "common/stats.h"
#include "device/device.h"
#include "utils/thread_pool.h"

namespace cache {

class IOModule {
private:
    IOModule();
    ~IOModule();
 public:
    static IOModule& getInstance();
  uint32_t addCacheDevice(char *filename);
  uint32_t addPrimaryDevice(char *filename);
  uint32_t read(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len);
  uint32_t write(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len);
  void flush(uint64_t addr, uint64_t bufferOffset, uint32_t len);
  inline void sync() { primaryDevice_->sync(); cacheDevice_->sync(); }
 private:
  // Currently, we assume that only one cache and one primary
  std::unique_ptr< BlockDevice > primaryDevice_;
  std::unique_ptr< BlockDevice > cacheDevice_;
  Stats *stats_{};

  struct {
    uint8_t *buf_;
    uint32_t len_;
    void read(uint64_t addr, uint8_t *buf, uint32_t len)
    {
      memcpy(buf, buf_ + addr, len);
    }
    void write(uint64_t addr, uint8_t *buf, uint32_t len)
    {
      memcpy(buf_ + addr, buf, len);
    }
  } inMemBuffer_{};

  std::unique_ptr<WriteBuffer> writeBuffer_;
  uint8_t journal_[512];
  uint32_t journalId_ = 0;
  uint32_t journalOffset_ = 0;
  uint64_t journalDiskOffset_ = 0;
  uint64_t journalDiskStart_ = 0;
  uint64_t journalSize_ = 20 * 1024 * 1024u;
  std::mutex mutex_;
};

}

#endif //__IOMODULE_H__
