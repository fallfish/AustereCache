#include "writebuffer.h"

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

      std::vector<int> threads;
      int thread_id;
      for (auto &p : index_) {
        // stats related
        if (p.addr_ != ~0ull) {
          toFlush.emplace_back(p);
        }
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
        std::lock_guard<std::mutex> l(readMutex_);
        std::vector<uint32_t> overwritten_entries;
        for (int i = index_.size() - 1; i >= 0; --i) {
          uint64_t addr_ = index_[i].addr_, len_ = index_[i].len_;
          if (addr_ == ~0ull) continue;
          for (int j = i - 1; j >= 0; --j) {
            if (index_[j].addr_ != ~0ull
                && ((index_[j].addr_ <= addr_ && index_[j].addr_ + index_[j].len_ > addr_)
                    || (index_[j].addr_ < addr_ + len_ && index_[j].addr_ + index_[j].len_ >= addr_ + len_)
                    || (index_[j].addr_ >= addr_ && index_[j].addr_ + index_[j].len_ <= addr_ + len_))) {
              overwritten_entries.push_back(j);
              index_[j].addr_ = ~0ull;
            }
          }
        }

        std::sort(overwritten_entries.begin(), overwritten_entries.end());
        for (auto i = overwritten_entries.rbegin(); i != overwritten_entries.rend(); ++i) {
          recycle(index_[*i].off_, index_[*i].len_);
          index_.erase(index_.begin() + *i);
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
      uint32_t offset = 0, currentIndex = ~0u;
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

    uint32_t WriteBuffer::commitWrite(uint64_t addr, uint32_t offset, uint32_t len, uint32_t currentIndex) {
      index_[currentIndex] = Entry(addr, offset, len);
      nWriters_.fetch_sub(1);
    }

    std::pair<uint32_t, uint32_t> WriteBuffer::prepareRead(uint64_t addr, uint32_t len) {
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
        nReaders_.fetch_add(1);
      }
      return std::make_pair(index, offset);
    }

    uint32_t WriteBuffer::commitRead() {
      nReaders_.fetch_sub(1);
    }
}
