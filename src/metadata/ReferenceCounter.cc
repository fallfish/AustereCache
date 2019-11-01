//
// Created by 王秋平 on 10/26/19.
//

#include "ReferenceCounter.h"
#include "common/config.h"
#include "utils/MurmurHash3.h"
#include "bitmap.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <csignal>

namespace cache {
  bool ReferenceCounter::clear() {
    counters_.clear();
  }

  bool ReferenceCounter::query(uint64_t key) {
    return counters_.find(key) == counters_.end();
  }

  bool ReferenceCounter::reference(uint64_t key) {
    if (counters_.find(key) == counters_.end()) {
      counters_[key] = 0;
    }
    counters_[key] += 1;
  }

  bool ReferenceCounter::dereference(uint64_t key) {
    assert(counters_.find(key) != counters_.end());
    counters_[key] -= 1;
    if (counters_[key] == 0) {
      counters_.erase(key);
    }
  }

  bool FullReferenceCounter::clear() {
    counters_.clear();
  }

  bool FullReferenceCounter::query(uint8_t * _key) {
    Key key(_key);
    return counters_.find(key) == counters_.end();
  }

  bool FullReferenceCounter::reference(uint8_t * _key) {
    Key key(_key);
    if (counters_.find(key) == counters_.end()) {
      counters_[key] = 0;
    }
    counters_[key] += 1;
  }

  bool FullReferenceCounter::dereference(uint8_t * _key) {
    Key key(_key);
    assert(counters_.find(key) != counters_.end());
    counters_[key] -= 1;
    if (counters_[key] == 0) {
      counters_.erase(key);
    }
  }

  SketchReferenceCounter::SketchReferenceCounter() {
    height_ = 4;
    width_ = Config::getInstance().getnLBABuckets() * Config::getInstance().getnLBASlotsPerBucket();
    sketch_ = new uint8_t[4 * 4 * width_ / 8];
    memset(sketch_, 0, 4 * 4 * width_ / 8);
  }

  void SketchReferenceCounter::clear() {}

  bool SketchReferenceCounter::query(uint64_t key) {
    uint32_t minVal = ~0u;
    uint32_t hashVal = 0;
//      MurmurHash3_x86_32(&key, 8, 0 * 100 + 7, &hashVal);
//      uint32_t bucketId = key % width_;
//      minVal = sketch_[0][bucketId];
    for (int i = 0; i < height_; ++i) {
      MurmurHash3_x86_32(&key, 8, i * 1003 + 7, &hashVal);
      uint32_t bucketId = i * width_ + hashVal % width_;
      uint32_t overflowValue = 0;
      uint32_t countValue = Bitmap::Manipulator(sketch_).getBits(bucketId * 4, bucketId * 4 + 4);
      if (mp_.find(bucketId) != mp_.end()) {
        overflowValue = mp_[bucketId];
      }
      minVal = (countValue + overflowValue < minVal) ? countValue + overflowValue : minVal;
    }
    return minVal == 0;
  }

  void SketchReferenceCounter::reference(uint64_t key) {
    uint32_t minVal = ~0u;
    uint32_t hashVal = 0;
    uint32_t width = width_;
    uint32_t maxValPerSlot = 32;
    for (int i = 0; i < height_; ++i) {
      MurmurHash3_x86_32(&key, 8, i * 1003 + 7, &hashVal);
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
      MurmurHash3_x86_32(&key, 8, i * 1003 + 7, &hashVal);
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
