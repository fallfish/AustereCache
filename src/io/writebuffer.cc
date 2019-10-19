#include "writebuffer.h"
#include <csignal>

namespace cache {
    void WriteBuffer::recycle(uint32_t off, uint32_t len) {
      // insert into the free list, keeping the order by offset
      for (auto iter = freeList_.begin(); true; ++iter) {
        if (iter == freeList_.end() || iter->off_ > off) {
          freeList_.insert(iter, Entry(~0ull, off, len));
          break;
        }
      }

      // merge contiguous entries
      std::vector<std::list<Entry>::iterator> iters;
      for (auto iter = freeList_.begin(); iter != freeList_.end();) {
        auto _iter = iter; //

        ++iter;
        while (iter != freeList_.end()
               && iter->off_ == _iter->off_ + _iter->len_) {
          _iter->len_ += iter->len_;
          iters.push_back(iter);
          ++iter;
        }
      }

      for (auto iter : iters) {
        freeList_.erase(iter);
      }
    }

    std::vector<WriteBuffer::Entry> WriteBuffer::flush() {
      std::vector<Entry> toFlush;
      while (nWriters_ != 0);
      std::lock_guard<std::mutex> l(readMutex_);
      while (nReaders_ != 0);

      for (auto &p : index_) {
        // stats related
        toFlush.emplace_back(p);
      }

      index_.clear();
      freeList_.clear();
      freeList_.emplace_back(~0ull, 0, bufferSize_);

      return toFlush;
    }

    bool WriteBuffer::allocate(uint32_t &off, uint32_t len) {
      for (auto & entry : freeList_) {
        if (entry.len_ >= len) {
          off = entry.off_;
          entry.off_ += len;
          entry.len_ -= len;
          return true;
        }
      }
      // current buffer is full, try to recycle previous overwritten entries
      {
        //std::cout << "Before: " << index_.size() << std::endl;
        std::lock_guard<std::mutex> l(readMutex_);
        std::vector<std::vector<Entry>::reverse_iterator> overwritten_entries;
        for (auto iter1 = index_.begin(); iter1 != index_.end(); ) {
          uint64_t beg1 = iter1->addr_, end1 = beg1 + iter1->len_;
          bool shouldErase = false;
          auto iter2 = iter1; ++iter2;
          for ( ; iter2 != index_.end(); ++iter2) {
            uint64_t beg2 = iter2->addr_, end2 = beg2 + iter2->len_;
            if ( (beg1 <= beg2 && end1 > beg2)
                || (beg1 < end2 && end1 >= end2)
                || (beg1 >= beg2 && end1 <= end2)) {
              shouldErase = true;
              break;
            }
          }
          if (shouldErase) {
            recycle(iter1->off_, iter1->len_);
            iter1 = index_.erase(iter1);
          } else {
            ++iter1;
          }
        }
        for (auto &entry : freeList_) {
          if (entry.len_ >= len) {
            off = entry.off_;
            entry.off_ += len;
            entry.len_ -= len;
            return true;
          }
        }
      }

      return false;
    }

    std::pair<uint32_t, uint32_t> WriteBuffer::prepareWrite(uint64_t addr, uint32_t len) {
      uint32_t currentIndex = ~0u, offset = 0;
      {
        std::lock_guard<std::mutex> l(writeMutex_);

        if (!allocate(offset, len)) {
          return std::make_pair(currentIndex, offset);
        }

        currentIndex = index_.size();
        index_.emplace_back(~0ull, 0, 0);

        nWriters_.fetch_add(1);
      }
      return std::make_pair(currentIndex, offset);
    }

    void WriteBuffer::commitWrite(uint64_t addr, uint32_t offset, uint32_t len, uint32_t currentIndex) {
      index_[currentIndex] = Entry(addr, offset, len);
      nWriters_.fetch_sub(1);
    }

    std::pair<uint32_t, uint32_t> WriteBuffer::prepareRead(uint64_t addr, uint32_t len) {
      uint32_t index = ~0u, offset = 0;
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
        nReaders_.fetch_add(1);
      }
      return std::make_pair(index, offset);
    }

    void WriteBuffer::commitRead() {
      nReaders_.fetch_sub(1);
    }
}
