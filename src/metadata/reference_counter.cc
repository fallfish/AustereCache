
#include "reference_counter.h"
#include "common/config.h"
#include "utils/xxhash.h"
#include "bitmap.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <csignal>

namespace cache {
  bool MapReferenceCounter::clear() {
    counters_.clear();
  }

  uint32_t MapReferenceCounter::query(uint64_t key) {
    if (counters_.find(key) == counters_.end()) {
      return 0;
    } else {
      return counters_[key];
    }
  }

  bool MapReferenceCounter::reference(uint64_t key) {
    if (counters_.find(key) == counters_.end()) {
      counters_[key] = 0;
    }
    counters_[key] += 1;
  }

  bool MapReferenceCounter::dereference(uint64_t key) {
    assert(counters_.find(key) != counters_.end());
    counters_[key] -= 1;
    if (counters_[key] == 0) {
      counters_.erase(key);
    }
  }

  SketchReferenceCounter::SketchReferenceCounter() {
    height_ = 4;
    width_ = Config::getInstance().getnLbaBuckets() * Config::getInstance().getnLBASlotsPerBucket();
    sketch_ = new uint8_t[4 * 4 * width_ / 8];
    memset(sketch_, 0, 4 * 4 * width_ / 8);
  }

  void SketchReferenceCounter::clear() {}

  uint32_t SketchReferenceCounter::query(uint64_t key) {
    uint32_t minVal = ~0u;
    uint32_t hashVal = 0;
    for (int i = 0; i < height_; ++i) {
      hashVal = XXH32(&key, 8, i * 1003 + 7);
      uint32_t bucketId = i * width_ + hashVal % width_;
      uint32_t overflowValue = 0;
      uint32_t countValue = Bitmap::Manipulator(sketch_).getBits(bucketId * 4, bucketId * 4 + 4);
      if (mp_.find(bucketId) != mp_.end()) {
        overflowValue = mp_[bucketId];
      }
      minVal = (countValue + overflowValue < minVal) ? countValue + overflowValue : minVal;
    }
    return minVal;
  }

  void SketchReferenceCounter::reference(uint64_t key) {
    uint32_t minVal = ~0u;
    uint32_t hashVal = 0;
    uint32_t width = width_;
    for (int i = 0; i < height_; ++i) {
      hashVal = XXH32(&key, 8, i * 1003 + 7);
      uint32_t bucketId = i * width_ + hashVal % width_;
      uint32_t countValue = Bitmap::Manipulator(sketch_).getBits(bucketId * 4, bucketId * 4 + 4);
      if (countValue == 15) {
        if (mp_.find(bucketId) == mp_.end()) {
          mp_[bucketId] = 1;
        } else {
          mp_[bucketId] += 1;
        }
      } else {
        Bitmap::Manipulator(sketch_).storeBits(bucketId * 4, bucketId * 4 + 4, countValue + 1);
      }
    }
  }

  void SketchReferenceCounter::dereference(uint64_t key) {
    uint32_t minVal = ~0u;
    uint32_t hashVal = 0;
    uint32_t width = width_;
    uint32_t a = mp_.size();
    for (int i = 0; i < height_; ++i) {
      hashVal = XXH32(&key, 8, i * 1003 + 7);
      uint32_t bucketId = i * width_ + hashVal % width_;
      if (mp_.find(bucketId) != mp_.end()) {
        mp_[bucketId] -= 1;
        if (mp_[bucketId] == 0) {
          mp_.erase(bucketId);
        }
      } else {
        uint32_t countValue = Bitmap::Manipulator(sketch_).getBits(bucketId * 4, bucketId * 4 + 4);
        Bitmap::Manipulator(sketch_).storeBits(bucketId * 4, bucketId * 4 + 4, countValue - 1);
      }
    }
  }
}
