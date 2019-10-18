#ifndef __FREQUENT_SLOTS_H__
#define __FREQUENT_SLOTS_H__

#include "common/common.h"
#include <memory>
#include <exception>

namespace cache {

class FrequentSlots {
 public:
  FrequentSlots() = default;

  void allocate(uint64_t fpHash)
  {
    for (auto &reverseMapping : slots_) {
      if (reverseMapping == nullptr) {
        reverseMapping = std::make_unique<ReverseMapping>();
        reverseMapping->fpHash_ = fpHash;
        return ;
      }
    }
    std::unique_ptr<ReverseMapping> reverseMapping =
      std::make_unique<ReverseMapping>();
    reverseMapping->fpHash_ = fpHash;
    slots_.push_back(std::move(reverseMapping));
  }

  bool query(uint64_t fpHash, uint64_t lba)
  {
    for (auto &reverseMapping : slots_) {
      if (reverseMapping == nullptr) continue;
      if (reverseMapping->fpHash_ == fpHash) {
        for (auto l : reverseMapping->lbas_) {
          if (l == lba)
            return true;
        }
        return false;
      }
    }
    return false;
  }

  void add(uint64_t fpHash, uint64_t lba) {
    for (auto &reverseMapping : slots_) {
      if (reverseMapping == nullptr) continue;
      if (reverseMapping->fpHash_ == fpHash) {
        reverseMapping->lbas_.push_back(lba);
      }
    }
  }

  void remove(uint64_t fpHash) {
    for (auto &reverseMapping : slots_) {
      if (reverseMapping == nullptr) continue;
      if (reverseMapping->fpHash_ == fpHash) {
        reverseMapping.reset();
      }
    }
  }

 private:
  struct ReverseMapping {
    uint64_t fpHash_{};
    std::vector<uint64_t> lbas_;
  };
  std::vector<std::unique_ptr<ReverseMapping>> slots_;
};

}


#endif

