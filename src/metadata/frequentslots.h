#ifndef __FREQUENT_SLOTS_H__
#define __FREQUENT_SLOTS_H__

#include "common/common.h"
#include <memory>
#include <exception>
#include <mutex>

namespace cache {

class FrequentSlots {
 public:
  FrequentSlots() = default;
  static FrequentSlots& getInstance() {
    static FrequentSlots instance;
    return instance;
  }
  void clear() {
    fpHash2Lbas_.clear();
  }

  void allocate(uint64_t fpHash)
  {
    std::lock_guard<std::mutex> l(mutex_);
    if (fpHash2Lbas_.find(fpHash) != fpHash2Lbas_.end()) {
      fpHash2Lbas_[fpHash].clear();
    }
  }

  bool query(uint64_t fpHash, uint64_t lba)
  {
    std::lock_guard<std::mutex> l(mutex_);
    if (fpHash2Lbas_.find(fpHash) != fpHash2Lbas_.end()) {
      return fpHash2Lbas_[fpHash].find(lba) != fpHash2Lbas_[fpHash].end();
    }
  }

  void add(uint64_t fpHash, uint64_t lba) {
    std::lock_guard<std::mutex> l(mutex_);
    fpHash2Lbas_[fpHash].insert(lba);
  }

  void remove(uint64_t fpHash) {
    std::lock_guard<std::mutex> l(mutex_);
    if (fpHash2Lbas_.find(fpHash) != fpHash2Lbas_.end())
      fpHash2Lbas_.erase(fpHash);
  }

 private:
  std::map<uint64_t, std::set<uint64_t>> fpHash2Lbas_;
  std::mutex mutex_;
};

}


#endif

