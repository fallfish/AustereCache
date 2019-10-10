#ifndef __FREQUENT_SLOTS_H__
#define __FREQUENT_SLOTS_H__

#include "common/common.h"
#include <memory>
#include <exception>

namespace cache {

class FrequentSlots {
 public:
  FrequentSlots() {}

  void allocate(uint64_t fp_hash)
  {
    for (auto &reverse_mapping : _slots) {
      if (reverse_mapping.get() == nullptr) {
        reverse_mapping = std::make_unique<ReverseMapping>();
        reverse_mapping->_fp_hash = fp_hash;
        return ;
      }
    }
    std::unique_ptr<ReverseMapping> reverse_mapping =
      std::make_unique<ReverseMapping>();
    reverse_mapping->_fp_hash = fp_hash;
    _slots.push_back(std::move(reverse_mapping));
  }

  bool query(uint64_t fp_hash, uint64_t lba)
  {
    for (auto &reverse_mapping : _slots) {
      if (reverse_mapping.get() == nullptr) continue;
      if (reverse_mapping->_fp_hash == fp_hash) { 
        for (auto l : reverse_mapping->_lbas) {
          if (l == lba)
            return true;
        }
        return false;
      }
    }
    return false;
  }

  void add(uint64_t fp_hash, uint64_t lba) {
    for (auto &reverse_mapping : _slots) {
      if (reverse_mapping.get() == nullptr) continue;
      if (reverse_mapping->_fp_hash == fp_hash) {
        reverse_mapping->_lbas.push_back(lba);
      }
    }
  }

  void remove(uint64_t fp_hash) {
    for (auto &reverse_mapping : _slots) {
      if (reverse_mapping.get() == nullptr) continue;
      if (reverse_mapping->_fp_hash == fp_hash) {
        reverse_mapping.reset();
      }
    }
  }

 private:
  struct ReverseMapping {
    uint64_t _fp_hash;
    std::vector<uint64_t> _lbas;
  };
  std::vector<std::unique_ptr<ReverseMapping>> _slots;
};

}


#endif

