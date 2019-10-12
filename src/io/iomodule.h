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
  uint32_t addCacheDevice(char *filename);
  uint32_t addPrimaryDevice(char *filename);
  uint32_t read(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len);
  uint32_t write(DeviceType deviceType, uint64_t addr, void *buf, uint32_t len);
  void flush(uint64_t addr);
  inline void sync() { primaryDevice_->sync(); cacheDevice_->sync(); }
 private:
  // Currently, we assume that only one cache and one primary
  std::unique_ptr< BlockDevice > primaryDevice_;
  std::unique_ptr< BlockDevice > cacheDevice_;
  Stats *stats_;

  struct WriteBuffer {
    WriteBuffer(uint32_t buffer_size) {
      bufferSize_ = buffer_size;
#ifdef DIRECT_IO
      posix_memalign(reinterpret_cast<void **>(&buf_), 512, buffer_size);
#else
      buf_ = (uint8_t*)malloc(buffer_size);
#endif
      nWriters_ = 0;
      nReaders_ = 0;
      currentOffset_ = 0;
      freeList_.push_back(Entry(~0, 0, buffer_size));
    }

    struct Entry {
      Entry(uint64_t addr, uint32_t off, uint32_t len) :
        addr_(addr), off_(off), len_(len) {}
      uint64_t addr_;
      uint32_t off_;
      uint32_t len_;
    };
    std::vector<Entry> index_;
    std::list<Entry> freeList_;
    uint32_t bufferSize_;
    uint32_t currentOffset_;

    std::mutex writeMutex_;
    std::atomic<uint32_t> nWriters_;
    std::mutex readMutex_;
    std::atomic<uint32_t> nReaders_;

    BlockDevice* cacheDevice_;
    uint8_t *buf_;
    std::unique_ptr<ThreadPool> threadPool_;

    inline void recycle(uint32_t off, uint32_t len) {
      // insert into the free list, keeping the order by offset
      for (auto it = freeList_.begin() ; true; ++it) {
        if (it == freeList_.end() || it->off_ > off) {
          freeList_.insert(it, Entry(~0, off, len));
          break;
        }
      }

      // merge contiguous entries
      std::vector<std::list<Entry>::iterator> its_;
      for (auto it = freeList_.begin(); it != freeList_.end(); ) {
        uint32_t off_ = it->off_, len_ = it->len_;
        auto it_ = it;

        ++it;
        while (it != freeList_.end()
            && it->off_ == off_ + len_) {
          len_ += it->len_;
          its_.push_back(it);
          ++it;
        }

        it_->len_ = len_;
      }

      for (auto it : its_) {
        freeList_.erase(it);
      }
    }

    inline void flush() {
      uint64_t total_bytes_to_cache_disk = 0;
      while (nWriters_ != 0) ;
      std::lock_guard<std::mutex> l(readMutex_);
      while (nReaders_ != 0) ;

      std::vector<int> threads;
      int thread_id;
      for (auto &p : index_) {
        // stats related
        if (p.addr_ != ~0) { total_bytes_to_cache_disk += p.len_; }

        if (Config::getInstance()->isDirectIOEnabled() == 0) {
          // no direct io, written into memory, no multithreading
          if (p.addr_ != ~0) {
            cacheDevice_->write(p.addr_, buf_ + p.off_, p.len_);
          }
        } else {
          if (threads.size() == Config::getInstance()->getMaxNumLocalThreads()) {
            threadPool_->wait_and_return_threads(threads);
          }
          if (p.addr_ != ~0) {
            while ( (thread_id = threadPool_->allocate_thread()) == -1) {
              threadPool_->wait_and_return_threads(threads);
            }
            threadPool_->get_thread(thread_id) = std::make_shared<std::thread>(
                [&]() {
                cacheDevice_->write(p.addr_, buf_ + p.off_, p.len_);
                });
            threads.push_back(thread_id);
          }
        }
      }
      if (Config::getInstance()->isDirectIOEnabled() == 1)
        threadPool_->wait_and_return_threads(threads);

      index_.clear();
      freeList_.clear();
      freeList_.push_back(Entry(~0, 0, bufferSize_));

      // add stats
      Stats::getInstance()->add_bytes_written_to_cache_disk(total_bytes_to_cache_disk);
    }

    inline void allocate(uint32_t &off, uint32_t len) {
      for (auto entry = freeList_.begin(); entry != freeList_.end(); ++entry) {
        if (entry->len_ >= len) {
          off = entry->off_;
          entry->off_ += len;
          entry->len_ -= len;
          return ;
        }
      }
      // current buffer is full, try to recycle previous overwritten entries
      {
        std::lock_guard<std::mutex> l(readMutex_);
        std::vector<int> overwritten_entries;
        for (int i = index_.size() - 1; i >= 0; --i) {
          uint64_t addr_ = index_[i].addr_, len_ = index_[i].len_;
          if (addr_ == ~0) continue;
          for (int j = i - 1; j >= 0; --j) {
            if ( index_[j].addr_ != ~0
                && ((index_[j].addr_ <= addr_ && index_[j].addr_ + index_[j].len_ > addr_)
                || (index_[j].addr_ < addr_ + len_ && index_[j].addr_ + index_[j].len_ >= addr_ + len_)
                || (index_[j].addr_ >= addr_ && index_[j].addr_ + index_[j].len_ <= addr_ + len_)) ) {
              overwritten_entries.push_back(j);
              index_[j].addr_ = ~0;
            }
          }
        }

        std::sort(overwritten_entries.begin(), overwritten_entries.end());
        for (auto i = overwritten_entries.rbegin(); i != overwritten_entries.rend(); ++i) {
          recycle(index_[*i].off_, index_[*i].len_);
          index_.erase(index_.begin() + *i);
        }

        for (auto entry = freeList_.begin(); entry != freeList_.end(); ++entry) {
          if (entry->len_ >= len) {
            off = entry->off_;
            entry->off_ += len;
            entry->len_ -= len;
            return ;
          }
        }
      }
      flush();

      off = freeList_.begin()->off_;
      freeList_.begin()->off_ += len;
      freeList_.begin()->len_ -= len;
    }

    uint32_t write(uint64_t addr, uint8_t *buf, uint32_t len) {
      uint32_t offset = 0, current_index = ~0;
      {
        std::lock_guard<std::mutex> l(writeMutex_);

        allocate(offset, len);
        current_index = index_.size();
        index_.push_back(Entry(~0, 0, 0));

        nWriters_.fetch_add(1);
      }
 
      memcpy(buf_ + offset, buf, len);

      {
        index_[current_index] = Entry(addr, offset, len);
        nWriters_.fetch_sub(1);
        // add stats
        Stats::getInstance()->add_bytes_written_to_write_buffer(len);
      }

      return 0;
    }

    uint32_t read(uint64_t addr, uint8_t *buf, uint32_t len) {
      uint32_t index = -1, offset = 0;
      uint32_t res;
      {
        std::lock_guard<std::mutex> l(readMutex_);
        for (int i = index_.size() - 1; i >= 0; --i) {
          if (index_[i].addr_ == addr && index_[i].len_ == len) {
            index = i;
            offset = index_[i].off_;
            break;
          }
        }
        if (index != -1) {
          nReaders_.fetch_add(1);
        }
      }

      if (index != -1) {
        memcpy(buf, buf_ + offset, len);
        nReaders_.fetch_sub(1);
        // add stats
        Stats::getInstance()->add_bytes_read_from_write_buffer(len);
      } else {
        res = cacheDevice_->read(addr, buf, len);
        // add stats
        Stats::getInstance()->add_bytes_read_from_cache_disk(len);
      }
    }
  } *writeBuffer_;

#if defined(CDARC)
  struct {
    uint8_t *_buf;
    uint32_t len_;
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
