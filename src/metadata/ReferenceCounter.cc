//
// Created by 王秋平 on 10/26/19.
//

#include "ReferenceCounter.h"

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
