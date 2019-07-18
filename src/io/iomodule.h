#ifndef __IOMODULE_H__
#define __IOMODULE_H__
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include "device/device.h"
namespace cache {

class IOModule {
 public:
  IOModule();
  ~IOModule();
  uint32_t add_cache_device(char *filename);
  uint32_t add_primary_device(char *filename);
  uint32_t read(uint32_t device, uint64_t addr, void *buf, uint32_t len);
  uint32_t write(uint32_t device, uint64_t addr, void *buf, uint32_t len);
 private:
  // Currently, we assume that only one cache and one primary
  std::unique_ptr< BlockDevice > _primary_device;

  struct WriteBuffer {
    WriteBuffer(uint32_t buffer_size) {
      _buffer_size = buffer_size;
      _n_writers = 0;
      _current_position = 0;
    }

    struct Entry {
      Entry(uint64_t addr, uint32_t pos, uint32_t len) :
        _addr(addr), _pos(pos), _len(len) {}
      uint64_t _addr;
      uint32_t _pos;
      uint32_t _len;
    };
    std::vector<Entry> _index;
    uint32_t _buffer_size;
    uint32_t _current_position;
    std::atomic<uint32_t> _n_writers;
    std::mutex _mutex;
    std::unique_ptr< BlockDevice > _cache_device;
    alignas(128) uint8_t _buffer[0];

    uint32_t write(uint64_t addr, uint8_t *buf, uint32_t len) {
      uint64_t position = 0, current_index = 0;
      {
        std::lock_guard<std::mutex> l(_mutex);

        // current buffer is full
        if (len > _buffer_size - _current_position) {
          while (_n_writers != 0) { };

          for (auto &p : _index) {
            _cache_device->write(p._addr, _buffer + p._pos, p._len);
          }
          _cache_device->sync();
          _index.clear();
          _current_position = 0;

        }

        position = _current_position;
        current_index = _index.size();

        _index.push_back(Entry(0, 0, 0));
        _current_position += len;
        _n_writers.fetch_add(1);
      }

      memcpy(_buffer + position, buf, len);

      {
        _index[current_index] = Entry(addr, position, len);
        _n_writers.fetch_sub(1);
      }
      return 0;
    }

    uint32_t read(uint64_t addr, uint8_t *buf, uint32_t len) {
      return _cache_device->read(addr, buf, len);
      //for (auto &p : _index) {
        //if (p._addr == addr && p._len == len) {
          //memcpy(buf, _buf + p.position
        //}
      //}
    }

  } *_write_buffer;

};

}

#endif //__IOMODULE_H__
