#include "iomodule.h"
#include "device/device.h"
#include "common/config.h"
#include "utils/utils.h"
#include <memory>

namespace cache {

IOModule::IOModule()
{
  if (Config::get_configuration().get_write_buffer_size() != 0) {
    _write_buffer = new WriteBuffer(Config::get_configuration().get_write_buffer_size());
    _write_buffer->_thread_pool = std::make_unique<ThreadPool>(Config::get_configuration().get_max_num_global_threads());
  } else {
    _write_buffer = nullptr;
  }
}

IOModule::~IOModule()
{
  if (_write_buffer != nullptr) {
    delete _write_buffer;
  }
}

uint32_t IOModule::add_cache_device(char *filename)
{
  // a temporary size for cache device
  // 32 MiB cache device
  uint64_t size = Config::get_configuration().get_cache_device_size();
  _cache_device = std::make_unique<BlockDevice>();
  _cache_device->_direct_io = Config::get_configuration().get_direct_io();
  _cache_device->open(filename, size);
  if (_write_buffer != nullptr)
    _write_buffer->_cache_device = _cache_device.get();
  return 0;
}

uint32_t IOModule::add_primary_device(char *filename)
{
  // a temporary size for primary device
  // 128 MiB primary device
  uint64_t size = Config::get_configuration().get_primary_device_size();
  _primary_device = std::make_unique<BlockDevice>();
  _primary_device->_direct_io = Config::get_configuration().get_direct_io();
  _primary_device->open(filename, size);
  return 0;
}

uint32_t IOModule::read(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  uint32_t ret = 0;
  if (device == 0) {
    ret = _primary_device->read(addr, (uint8_t*)buf, len);
    Stats::get_instance()->add_bytes_written_to_primary_disk(len);
  } else {
    if (_write_buffer != nullptr) {
      ret = _write_buffer->read(addr, (uint8_t*)buf, len);
    } else {
      ret = _cache_device->read(addr, (uint8_t*)buf, len);
    }
  }
  return ret;
}

uint32_t IOModule::write(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  if (device == 0) {
    _primary_device->write(addr, (uint8_t*)buf, len);
    Stats::get_instance()->add_bytes_read_from_primary_disk(len);
  } else {
    if (_write_buffer != nullptr) {
      _write_buffer->write(addr, (uint8_t*)buf, len);
    } else {
      _cache_device->write(addr, (uint8_t*)buf, len);
    }
  }
}

}
