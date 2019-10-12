#include "iomodule.h"
#include "device/device.h"
#include "common/config.h"
#include "utils/utils.h"
#include <csignal>
#include <memory>

namespace cache {

IOModule::IOModule()
{
#if defined(CDARC)
  weu_.buf_ = new uint8_t[Config::getInstance()->getWriteBufferSize()];
  weu_.len_ = Config::getInstance()->getWriteBufferSize();
  writeBuffer_ = nullptr;
#else
  if (Config::getInstance()->getWriteBufferSize() != 0) {
    std::cout << "Write Buffer init" << std::endl;
    writeBuffer_ = new WriteBuffer(Config::getInstance()->getWriteBufferSize());
    writeBuffer_->threadPool_ = std::make_unique<ThreadPool>(Config::getInstance()->getMaxNumGlobalThreads());
  } else {
    writeBuffer_ = nullptr;
  }
#endif
}

IOModule::~IOModule()
{
  if (writeBuffer_ != nullptr) {
    delete writeBuffer_;
  }
#if defined(CDARC)
  free(weu_.buf_);
#endif
}

uint32_t IOModule::addCacheDevice(char *filename)
{
  // a temporary size for cache device
  // 32 MiB cache device
  uint64_t size = Config::getInstance()->getCacheDeviceSize();
  cacheDevice_ = std::make_unique<BlockDevice>();
  cacheDevice_->_direct_io = Config::getInstance()->isDirectIOEnabled();
  cacheDevice_->open(filename, size);
  if (writeBuffer_ != nullptr)
    writeBuffer_->cacheDevice_ = cacheDevice_.get();
  return 0;
}

uint32_t IOModule::addPrimaryDevice(char *filename)
{
  // a temporary size for primary device
  // 128 MiB primary device
  uint64_t size = Config::getInstance()->getPrimaryDeviceSize();
  primaryDevice_ = std::make_unique<BlockDevice>();
  primaryDevice_->_direct_io = Config::getInstance()->isDirectIOEnabled();
  primaryDevice_->open(filename, size);
  return 0;
}

uint32_t IOModule::read(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len)
{
  uint32_t ret = 0;
  if (deviceType == PRIMARY_DEVICE) {
    BEGIN_TIMER();
    ret = primaryDevice_->read(addr, (uint8_t*)buf, len);
    END_TIMER(io_hdd);
    Stats::getInstance()->add_bytes_read_from_primary_disk(len);
  } else if (deviceType == CACHE_DEVICE) {
    BEGIN_TIMER();
    if (writeBuffer_ != nullptr) {
      ret = writeBuffer_->read(addr, (uint8_t*)buf, len);
    } else {
      ret = cacheDevice_->read(addr, (uint8_t*)buf, len);
      Stats::getInstance()->add_bytes_read_from_cache_disk(len);
    }
    END_TIMER(io_ssd);
#if defined(CDARC)
  } else if (deviceType == 2) {
    weu_.read(addr, (uint8_t*)buf, len);
#endif
  }
  Stats::getInstance()->add_io_request(deviceType, 1, len);
  return ret;
}

uint32_t IOModule::write(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len)
{
  if (deviceType == PRIMARY_DEVICE) {
    BEGIN_TIMER();
    primaryDevice_->write(addr, (uint8_t*)buf, len);
      Stats::getInstance()->add_bytes_written_to_primary_disk(len);
    END_TIMER(io_hdd);
  } else if (deviceType == CACHE_DEVICE) {
    BEGIN_TIMER();
    if (writeBuffer_ != nullptr) {
      writeBuffer_->write(addr, (uint8_t*)buf, len);
    } else {
      cacheDevice_->write(addr, (uint8_t*)buf, len);
      Stats::getInstance()->add_bytes_written_to_cache_disk(len);
    }
    END_TIMER(io_ssd);
#if defined(CDARC)
  } else if (deviceType == 2) {
    weu_.write(addr, (uint8_t*)buf, len);
#endif
  }
  Stats::getInstance()->add_io_request(deviceType, 0, len);
}

void IOModule::flush(uint64_t addr)
{
#if defined(CDARC)
  cacheDevice_->write(addr, weu_.buf_, weu_.len_);
#endif
}

}
