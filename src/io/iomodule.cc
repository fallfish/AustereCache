#include "iomodule.h"
#include "device/device.h"
#include "common/config.h"
#include <memory>

namespace cache {

IOModule::IOModule()
{
}

uint32_t IOModule::add_cache_device(char *filename)
{
  // a temporary size for cache device
  // 32 MiB cache device
  uint64_t size = Config::cache_device_size;
  _cache_device = std::make_unique<BlockDevice>();
  _cache_device->open(filename, size);
  return 0;
}

uint32_t IOModule::add_primary_device(char *filename)
{
  // a temporary size for primary device
  // 128 MiB primary device
  uint64_t size = Config::primary_device_size;
  _primary_device = std::make_unique<BlockDevice>();
  _primary_device->open(filename, size);
  return 0;
}

uint32_t IOModule::read(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  if (device == 0) {
    return _primary_device->read(addr, (uint8_t*)buf, len);
  } else {
    return _cache_device->read(addr, (uint8_t*)buf, len);
  }
}

uint32_t IOModule::write(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  if (device == 0) {
    return _primary_device->write(addr, (uint8_t*)buf, len);
  } else {
    return _cache_device->write(addr, (uint8_t*)buf, len);
  }
}

}