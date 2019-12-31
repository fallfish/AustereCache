#include "io_module.h"
#include "device/device.h"
#include "common/config.h"
#include "utils/utils.h"
#include <csignal>
#include <memory>

namespace cache {

IOModule::IOModule()
{
  if (Config::getInstance().getWriteBufferSize() != 0) {
    std::cout << "Write Buffer init" << std::endl;
    posix_memalign(reinterpret_cast<void **>(&inMemBuffer_.buf_), 512, Config::getInstance().getWriteBufferSize());
    inMemBuffer_.len_ = Config::getInstance().getWriteBufferSize();
  } else {
    inMemBuffer_.len_ = 0;
  }

  if (Config::getInstance().getWriteBufferSize() != 0) {
    writeBuffer_ = std::make_unique<WriteBuffer>(
      Config::getInstance().getWriteBufferSize());
  }
}

IOModule::~IOModule() = default;

IOModule &IOModule::getInstance() {
  static IOModule ioModule;
  return ioModule;
}


uint32_t IOModule::addCacheDevice(char *filename)
{
  // a temporary size for cache device
  // 32 MiB cache device
  uint64_t size = Config::getInstance().getCacheDeviceSize();
  cacheDevice_ = std::make_unique<BlockDevice>();
  cacheDevice_->_direct_io = Config::getInstance().isDirectIOEnabled();
  cacheDevice_->open(filename, size + 1ull * Config::getInstance().getnFpBuckets() * Config::getInstance().getnFPSlotsPerBucket() * Config::getInstance().getMetadataSize());
  return 0;
}

uint32_t IOModule::addPrimaryDevice(char *filename)
{
  // a temporary size for primary device
  // 128 MiB primary device
  uint64_t size = Config::getInstance().getPrimaryDeviceSize();
  primaryDevice_ = std::make_unique<BlockDevice>();
  primaryDevice_->_direct_io = Config::getInstance().isDirectIOEnabled();
  primaryDevice_->open(filename, size);
  return 0;
}

uint32_t IOModule::read(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len)
{
  uint32_t ret = 0;
  if (deviceType == PRIMARY_DEVICE) {
    BEGIN_TIMER();
    ret = primaryDevice_->read(addr, static_cast<uint8_t *>(buf), len);
    END_TIMER(io_hdd);
    Stats::getInstance().add_bytes_read_from_hdd(len);
  } else if (deviceType == CACHE_DEVICE) {
    if (len == 512) {
      Stats::getInstance().add_metadata_bytes_read_from_ssd(512);
    }

    BEGIN_TIMER();
    Stats::getInstance().add_bytes_read_from_ssd(len);
    ret = cacheDevice_->read(addr, static_cast<uint8_t *>(buf), len);
    END_TIMER(io_ssd);
  } else if (deviceType == IN_MEM_BUFFER) {
    inMemBuffer_.read(addr, static_cast<uint8_t *>(buf), len);
  }
  return ret;
}

uint32_t IOModule::write(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len)
{
  if (deviceType == PRIMARY_DEVICE) {
    BEGIN_TIMER();
    primaryDevice_->write(addr, (uint8_t*)buf, len);
    END_TIMER(io_hdd);
    Stats::getInstance().add_bytes_written_to_hdd(len);
  } else if (deviceType == CACHE_DEVICE) {
    if (len == 512) {
      Stats::getInstance().add_metadata_bytes_written_to_ssd(512);
    }
    BEGIN_TIMER();
    Stats::getInstance().add_bytes_written_to_ssd(len);
    cacheDevice_->write(addr, (uint8_t *) buf, len);
    END_TIMER(io_ssd);
  } else if (deviceType == IN_MEM_BUFFER) {
    inMemBuffer_.write(addr, (uint8_t*)buf, len);
  } else if (deviceType == JOURNAL) {
    std::lock_guard<std::mutex> l(mutex_);
    if (journalOffset_ + len >= 512) {
      journalOffset_ = 8;
      journal_[0] = journalId_++;
      cacheDevice_->write(journalDiskOffset_ + journalDiskStart_, journal_, 512);
      journalDiskOffset_ += 512;
      if (journalDiskOffset_ >= journalSize_) {
        journalDiskOffset_ = 0;
      }
    }
    memcpy(journal_ + journalOffset_, buf, len);
    journalOffset_ += len;
  }
  return 0;
}

void IOModule::flush(uint64_t addr, uint64_t bufferOffset, uint32_t len)
{
  Stats::getInstance().add_bytes_written_to_ssd(len);
  cacheDevice_->write(addr, inMemBuffer_.buf_ + bufferOffset, len);
}

}
