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
  _weu._buf = new uint8_t[Config::get_configuration()->get_write_buffer_size()];
  _weu._len = Config::get_configuration()->get_write_buffer_size();
  _write_buffer = nullptr;
#else
  if (Config::get_configuration()->get_write_buffer_size() != 0) {
    std::cout << "Write Buffer init" << std::endl;
    _write_buffer = new WriteBuffer(Config::get_configuration()->get_write_buffer_size());
    _write_buffer->_thread_pool = std::make_unique<ThreadPool>(Config::get_configuration()->get_max_num_global_threads());
  } else {
    _write_buffer = nullptr;
  }
#endif
}

IOModule::~IOModule()
{
  if (_write_buffer != nullptr) {
    delete _write_buffer;
  }
#if defined(CDARC)
  free(_weu._buf);
#endif
}

uint32_t IOModule::add_cache_device(char *filename)
{
  // a temporary size for cache device
  // 32 MiB cache device
  uint64_t size = Config::get_configuration()->get_cache_device_size();
  _cache_device = std::make_unique<BlockDevice>();
  _cache_device->_direct_io = Config::get_configuration()->get_direct_io();
  _cache_device->open(filename, size);
  if (_write_buffer != nullptr)
    _write_buffer->_cache_device = _cache_device.get();
  return 0;
}

uint32_t IOModule::add_primary_device(char *filename)
{
  // a temporary size for primary device
  // 128 MiB primary device
  uint64_t size = Config::get_configuration()->get_primary_device_size();
  _primary_device = std::make_unique<BlockDevice>();
  _primary_device->_direct_io = Config::get_configuration()->get_direct_io();
  _primary_device->open(filename, size);
  return 0;
}

uint32_t IOModule::read(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  uint32_t ret = 0;
  if (device == 0) {
    BEGIN_TIMER();
    ret = _primary_device->read(addr, (uint8_t*)buf, len);
    END_TIMER(io_hdd);
    Stats::get_instance()->add_bytes_read_from_primary_disk(len);
  } else if (device == 1) {
    BEGIN_TIMER();
    if (_write_buffer != nullptr) {
      ret = _write_buffer->read(addr, (uint8_t*)buf, len);
    } else {
      ret = _cache_device->read(addr, (uint8_t*)buf, len);
      Stats::get_instance()->add_bytes_read_from_cache_disk(len);
    }
    END_TIMER(io_ssd);
#if defined(CDARC)
  } else if (device == 2) {
    _weu.read(addr, (uint8_t*)buf, len);
#endif
  }
  Stats::get_instance()->add_io_request(device, 1, len);
  return ret;
}

uint32_t IOModule::write(uint32_t device, uint64_t addr, void *buf, uint32_t len)
{
  if (device == 0) {
    BEGIN_TIMER();
    _primary_device->write(addr, (uint8_t*)buf, len);
    Stats::get_instance()->add_bytes_written_to_primary_disk(len);
    END_TIMER(io_hdd);
  } else if (device == 1) {
    BEGIN_TIMER();
    if (_write_buffer != nullptr) {
      _write_buffer->write(addr, (uint8_t*)buf, len);
    } else {
      _cache_device->write(addr, (uint8_t*)buf, len);
      Stats::get_instance()->add_bytes_written_to_cache_disk(len);
    }
    END_TIMER(io_ssd);
#if defined(CDARC)
  } else if (device == 2) {
    _weu.write(addr, (uint8_t*)buf, len);
#endif
  }
  Stats::get_instance()->add_io_request(device, 0, len);
}

void IOModule::flush(uint64_t addr)
{
#if defined(CDARC)
  _cache_device->write(addr, _weu._buf, _weu._len);
#endif
}

}
