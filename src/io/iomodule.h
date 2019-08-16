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
#include "common/env.h"
#include "common/common.h"
#include "common/stats.h"
#include "device/device.h"
#include "utils/thread_pool.h"

namespace cache {

class IOModule {
 public:
  IOModule();
  ~IOModule();
  uint32_t add_cache_device(char *filename);
  uint32_t add_primary_device(char *filename);
  uint32_t read(uint32_t device, uint64_t addr, void *buf, uint32_t len);
  uint32_t write(uint32_t device, uint64_t addr, void *buf, uint32_t len);
  void flush(uint64_t addr);
  inline void sync() { _primary_device->sync(); _cache_device->sync(); }
 private:
  // Currently, we assume that only one cache and one primary
  std::unique_ptr< BlockDevice > _primary_device;
  std::unique_ptr< BlockDevice > _cache_device;
  Stats *_stats;

  struct WriteBuffer {
    WriteBuffer(uint32_t buffer_size) {
      _buffer_size = buffer_size;
#ifdef DIRECT_IO
      _buffer = (uint8_t*)aligned_alloc(512, buffer_size);
#else
      _buffer = (uint8_t*)malloc(buffer_size);
#endif
      _n_writers = 0;
      _n_readers = 0;
      _current_offset = 0;
      _free_list.push_back(Entry(~0, 0, buffer_size));
    }

    struct Entry {
      Entry(uint64_t addr, uint32_t off, uint32_t len) :
        _addr(addr), _off(off), _len(len) {}
      uint64_t _addr;
      uint32_t _off;
      uint32_t _len;
    };
    std::vector<Entry> _index;
    std::list<Entry> _free_list;
    uint32_t _buffer_size;
    uint32_t _current_offset;

    std::mutex _write_mutex;
    std::atomic<uint32_t> _n_writers;
    std::mutex _read_mutex;
    std::atomic<uint32_t> _n_readers;

    BlockDevice* _cache_device;
    uint8_t *_buffer;
    std::unique_ptr<ThreadPool> _thread_pool;

    inline void recycle(uint32_t off, uint32_t len) {
      // insert into the free list, keeping the order by offset
      for (auto it = _free_list.begin() ; true; ++it) {
        if (it == _free_list.end() || it->_off > off) {
          _free_list.insert(it, Entry(~0, off, len));
          break;
        }
      }

      // merge contiguous entries
      std::vector<std::list<Entry>::iterator> its_;
      for (auto it = _free_list.begin(); it != _free_list.end(); ) {
        uint32_t off_ = it->_off, len_ = it->_len;
        auto it_ = it;

        ++it;
        while (it != _free_list.end()
            && it->_off == off_ + len_) {
          len_ += it->_len;
          its_.push_back(it);
          ++it;
        }

        it_->_len = len_;
      }

      for (auto it : its_) {
        _free_list.erase(it);
      }
    }

    inline void flush() {
      uint64_t total_bytes_to_cache_disk = 0;
      while (_n_writers != 0) ;
      std::lock_guard<std::mutex> l(_read_mutex);
      while (_n_readers != 0) ;

      std::vector<int> threads;
      int thread_id;
      for (auto &p : _index) {
        // stats related
        if (p._addr != ~0) { total_bytes_to_cache_disk += p._len; }

        if (Config::get_configuration().get_direct_io() == 0) {
          // no direct io, written into memory, no multithreading
          if (p._addr != ~0) {
            _cache_device->write(p._addr, _buffer + p._off, p._len);
          }
        } else {
          if (threads.size() == Config::get_configuration().get_max_num_local_threads()) {
            _thread_pool->wait_and_return_threads(threads);
          }
          if (p._addr != ~0) {
            while ( (thread_id = _thread_pool->allocate_thread()) == -1) {
              _thread_pool->wait_and_return_threads(threads);
            }
            _thread_pool->get_thread(thread_id) = std::make_shared<std::thread>(
                [&]() {
                _cache_device->write(p._addr, _buffer + p._off, p._len);
                });
            threads.push_back(thread_id);
          }
        }
      }
      if (Config::get_configuration().get_direct_io() == 1)
        _thread_pool->wait_and_return_threads(threads);

      _index.clear();
      _free_list.clear();
      _free_list.push_back(Entry(~0, 0, _buffer_size));

      // add stats
      Stats::get_instance()->add_bytes_written_to_cache_disk(total_bytes_to_cache_disk);
    }

    inline void allocate(uint32_t &off, uint32_t len) {
      for (auto entry = _free_list.begin(); entry != _free_list.end(); ++entry) {
        if (entry->_len >= len) {
          off = entry->_off;
          entry->_off += len;
          entry->_len -= len;
          return ;
        }
      }
      // current buffer is full, try to recycle previous overwritten entries
      {
        std::lock_guard<std::mutex> l(_read_mutex);
        std::vector<int> overwritten_entries;
        for (int i = _index.size() - 1; i >= 0; --i) {
          uint32_t addr_ = _index[i]._addr, len_ = _index[i]._len;
          if (addr_ == ~0) continue;
          for (int j = i - 1; j >= 0; --j) {
            if ( _index[j]._addr != ~0
                && ((_index[j]._addr <= addr_ && _index[j]._addr + _index[j]._len > addr_)
                || (_index[j]._addr < addr_ + len_ && _index[j]._addr + _index[j]._len >= addr_ + len_)
                || (_index[j]._addr >= addr_ && _index[j]._addr + _index[j]._len <= addr_ + len_)) ) {
              overwritten_entries.push_back(j);
              _index[j]._addr = ~0;
            }
          }
        }

        std::sort(overwritten_entries.begin(), overwritten_entries.end());
        for (auto i = overwritten_entries.rbegin(); i != overwritten_entries.rend(); ++i) {
          recycle(_index[*i]._off, _index[*i]._len);
          _index.erase(_index.begin() + *i);
        }

        for (auto entry = _free_list.begin(); entry != _free_list.end(); ++entry) {
          if (entry->_len >= len) {
            off = entry->_off;
            entry->_off += len;
            entry->_len -= len;
            return ;
          }
        }
      }
      flush();

      off = _free_list.begin()->_off;
      _free_list.begin()->_off += len;
      _free_list.begin()->_len -= len;
    }

    uint32_t write(uint64_t addr, uint8_t *buf, uint32_t len) {
      uint32_t offset = 0, current_index = ~0;
      {
        std::lock_guard<std::mutex> l(_write_mutex);

        allocate(offset, len);
        current_index = _index.size();
        _index.push_back(Entry(~0, 0, 0));

        _n_writers.fetch_add(1);
      }
 
      memcpy(_buffer + offset, buf, len);

      {
        _index[current_index] = Entry(addr, offset, len);
        _n_writers.fetch_sub(1);
        // add stats
        Stats::get_instance()->add_bytes_written_to_write_buffer(len);
      }

      return 0;
    }

    uint32_t read(uint64_t addr, uint8_t *buf, uint32_t len) {
      uint32_t index = -1, offset = 0;
      uint32_t res;
      {
        std::lock_guard<std::mutex> l(_read_mutex);
        for (int i = _index.size() - 1; i >= 0; --i) {
          if (_index[i]._addr == addr && _index[i]._len == len) {
            index = i;
            offset = _index[i]._off;
            break;
          }
        }
        if (index != -1) {
          _n_readers.fetch_add(1);
        }
      }

      if (index != -1) {
        memcpy(buf, _buffer + offset, len);
        _n_readers.fetch_sub(1);
        // add stats
        Stats::get_instance()->add_bytes_read_from_write_buffer(len);
      } else {
        res = _cache_device->read(addr, buf, len);
        // add stats
        Stats::get_instance()->add_bytes_read_from_cache_disk(len);
      }
    }
  } *_write_buffer;

#if defined(CDARC)
  struct {
    uint8_t *_buf;
    uint32_t _len;
    void read(uint64_t addr, uint8_t *buf, uint32_t len)
    {
      memcpy(buf, _buf + addr, len);
    }
    void write(uint64_t addr, uint8_t *buf, uint32_t len)
    {
      memcpy(_buf + addr, buf, len);
    }
  } _weu;
#endif
};

}

#endif //__IOMODULE_H__
