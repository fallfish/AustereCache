//
// Created by 王秋平 on 10/18/19.
//

#ifndef SSDDUP_WRITEBUFFER_H
#define SSDDUP_WRITEBUFFER_H

#include <cstdint>
#include <vector>
#include <list>
#include <mutex>
#include <algorithm>
#include "common/stats.h"

namespace cache {
    class WriteBuffer {
    public:
        explicit WriteBuffer(uint32_t buffer_size) {
          bufferSize_ = buffer_size;
          nWriters_ = 0;
          nReaders_ = 0;
          currentOffset_ = 0;
          freeList_.emplace_back(~0ull, 0, buffer_size);
        }

        struct Entry {
            Entry(uint64_t addr, uint32_t off, uint32_t len) :
              addr_(addr), off_(off), len_(len) {}

            uint64_t addr_;
            uint32_t off_;
            uint32_t len_;
        };

        std::vector<Entry> index_{};
        std::list<Entry> freeList_{};
        uint32_t bufferSize_{};
        uint32_t currentOffset_{};

        std::mutex writeMutex_{};
        std::atomic<uint32_t> nWriters_{};
        std::mutex readMutex_{};
        std::atomic<uint32_t> nReaders_{};

        void recycle(uint32_t off, uint32_t len);
        std::vector<WriteBuffer::Entry> flush();
        bool allocate(uint32_t &off, uint32_t len);
        std::pair<uint32_t, uint32_t> prepareWrite(uint64_t addr, uint32_t len);
        uint32_t commitWrite(uint64_t addr, uint32_t offset, uint32_t len, uint32_t currentIndex);
        std::pair<uint32_t, uint32_t> prepareRead(uint64_t addr, uint32_t len);
        uint32_t commitRead();
    };
}
#endif //SSDDUP_WRITEBUFFER_H
