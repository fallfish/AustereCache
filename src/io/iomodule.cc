#include "iomodule.h"
#include "device/device.h"
#include "common/config.h"
#include "utils/utils.h"
#include <csignal>
#include <memory>

namespace cache {

IOModule::IOModule()
{
  if (Config::getInstance()->getWriteBufferSize() != 0) {
    std::cout << "Write Buffer init" << std::endl;
#ifdef DIRECT_IO
    posix_memalign(reinterpret_cast<void **>(&inMemBuffer_.buf_), 512, Config::getInstance()->getWriteBufferSize());
#else
    buf_ = (uint8_t*)malloc(buffer_size);
#endif
    inMemBuffer_.len_ = Config::getInstance()->getWriteBufferSize();
  } else {
    inMemBuffer_.len_ = 0;
  }



}

IOModule::~IOModule() = default;

uint32_t IOModule::addCacheDevice(char *filename)
{
  // a temporary size for cache device
  // 32 MiB cache device
  uint64_t size = Config::getInstance()->getCacheDeviceSize();
  cacheDevice_ = std::make_unique<BlockDevice>();
  cacheDevice_->_direct_io = Config::getInstance()->isDirectIOEnabled();
  cacheDevice_->open(filename, size);
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
    ret = cacheDevice_->read(addr, (uint8_t*)buf, len);
    Stats::getInstance()->add_bytes_read_from_cache_disk(len);
    END_TIMER(io_ssd);
  } else if (deviceType == IN_MEM_BUFFER) {
    inMemBuffer_.read(addr, (uint8_t*)buf, len);
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
    cacheDevice_->write(addr, (uint8_t*)buf, len);
    Stats::getInstance()->add_bytes_written_to_cache_disk(len);
    END_TIMER(io_ssd);
  } else if (deviceType == IN_MEM_BUFFER) {
    inMemBuffer_.write(addr, (uint8_t*)buf, len);
  }
  Stats::getInstance()->add_io_request(deviceType, 0, len);
}

void IOModule::flush(uint64_t addr, uint64_t bufferOffset, uint32_t len)
{
  if (len == ~0u) len = inMemBuffer_.len_;
  cacheDevice_->write(addr, inMemBuffer_.buf_ + bufferOffset, len);
}

}
