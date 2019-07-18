#include "iomodule.h"
#include "device/device.h"
#include "common/config.h"
#include "utils/utils.h"
#include <memory>

namespace cache {

IOModule::IOModule()
{
  _write_buffer = reinterpret_cast<WriteBuffer*>(new uint8_t[sizeof(WriteBuffer) + 1024 * 32 * 32]);
  new (_write_buffer) WriteBuffer(1024 * 32 * 32);
}

IOModule::~IOModule()
{
  delete _write_buffer;
}

uint32_t IOModule::add_cache_device(char *filename)
{
  // a temporary size for cache device
  // 32 MiB cache device
  uint64_t size = Config::get_configuration().get_cache_device_size();
  _write_buffer->_cache_device = std::make_unique<BlockDevice>();
  _write_buffer->_cache_device->open(filename, size);
  return 0;
}

uint32_t IOModule::add_primary_device(char *filename)
{
  // a temporary size for primary device
  // 128 MiB primary device
  uint64_t size = Config::get_configuration().get_primary_device_size();
  _primary_device = std::make_unique<BlockDevice>();
  _primary_device->open(filename, size);
  return 0;
}

uint32_t IOModule::read(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  uint32_t ret = 0;
  if (device == 0) {
    ret = _primary_device->read(addr, (uint8_t*)buf, len);
  } else {
    ret = _write_buffer->read(addr, (uint8_t*)buf, len);
  }
  return ret;
}

uint32_t IOModule::write(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  if (device == 0) {
    return _primary_device->write(addr, (uint8_t*)buf, len);
  } else {
    return _write_buffer->write(addr, (uint8_t*)buf, len);
  }
}

}
