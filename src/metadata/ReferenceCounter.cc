//
// Created by 王秋平 on 10/26/19.
//

#include "ReferenceCounter.h"
#include "common/config.h"
#include <cassert>
#include <cstring>

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
}
