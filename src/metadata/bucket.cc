#include <utility>

#include <cstring>
#include "bitmap.h"
#include "bucket.h"
#include "index.h"

namespace cache {
  Bucket::Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_slot(n_bits_per_key + n_bits_per_value), _n_slots(n_slots),
    _n_total_bytes(((n_bits_per_key + n_bits_per_value) * n_slots + 7) / 8),
    _data(std::make_unique<Bitmap>((n_bits_per_key + n_bits_per_value) * n_slots))
  {} 

  Bucket::~Bucket() {}

  LBABucket::LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots) :
    Bucket(n_bits_per_key, n_bits_per_value, n_slots),
    _valid(std::make_unique<Bitmap>(n_slots))
  {
  }

  LBABucket::~LBABucket() {}

  uint32_t LBABucket::lookup(uint32_t lba_sig, uint32_t &ca_hash) {
    for (uint32_t index = 0; index < _n_slots; index++) {
      uint32_t k = get_k(index);
      if (k == lba_sig) {
        ca_hash = get_v(index);
        return index;
      }
    }
    return ~((uint32_t)0);
  }

  void LBABucket::advance(uint32_t index)
  {
    uint32_t k = get_k(index);
    uint32_t v = get_v(index);
    for (uint32_t i = index; i < _n_slots - 1; i++) {
      set_k(i, get_k(i + 1));
      set_v(i, get_v(i + 1));
      _valid->clear(i);
      if (_valid->get(i + 1))
        _valid->set(i);
    }
    set_k(_n_slots - 1, k);
    set_v(_n_slots - 1, v);
    _valid->set(_n_slots - 1);
  }

  uint32_t LBABucket::find_non_occupied_position(std::shared_ptr<CAIndex> ca_index)
  {
    uint32_t position = 0;
    bool valid = true;
    for (uint32_t i = 0; i < _n_slots; i++) {
      uint32_t v = get_v(i);
      if (!_valid->get(i)) continue;
      uint32_t size; uint64_t ssd_location; // useless variables
      // note here that v = 0 can result in too many evictions
      // needs ~15 lookup per update, further optimization needed
      // but correctness not impacted
      if (ca_index != nullptr)
        valid = ca_index->lookup(v, size, ssd_location);
      if (!valid) {
//        std::cout << "LBABucket::evict: " << v << std::endl;
//        if (v == 0) std::cout << "v = 0" << std::endl;
        set_k(i, 0), set_v(i, 0);
        _valid->clear(i);
        position = i;
      }
    }
    return position;
  }

  void LBABucket::update(uint32_t lba_sig, uint32_t ca_hash, std::shared_ptr<CAIndex> ca_index) {
    uint32_t v = 0;
    uint32_t index = lookup(lba_sig, v);
    if (index != ~((uint32_t)0)) {
      if (v != ca_hash) {
        set_v(index, ca_hash);
      }
    } else {
      index = find_non_occupied_position(std::move(ca_index));
      set_k(index, lba_sig);
      set_v(index, ca_hash);
      _valid->set(index);
    }
    advance(index);
    if (_valid->get(0)) {
      uint32_t v = 1;
    }
  }

  CABucket::CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_slots) :
    Bucket(n_bits_per_key, n_bits_per_value, n_slots)
  {
    _valid = std::make_unique< Bitmap >(1 * n_slots);
    _space = std::make_unique< Bucket >(0, 2, n_slots);
    _clock = std::make_unique< Bucket >(0, 2, n_slots);
    _clock_ptr = 0;
  }

  CABucket::~CABucket() {}

  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  int CABucket::lookup(uint32_t ca_sig, uint32_t &size)
  {
    uint32_t index = 0, space = 0;
    while (index < _n_slots) {
      if (is_valid(index)) {
        if (get_k(index) == ca_sig) {
          size = get_space(index);
          return index;
        }
        index += space;
      }
      ++index;
    }
    return ~((uint32_t)0);
  }

  uint32_t CABucket::find_non_occupied_position(uint32_t size)
  {
    uint32_t index = 0, space = 0;
    // check whether there is a contiguous space
    while (index < _n_slots) {
      if (is_valid(index)) {
        space = 0;
        index += get_space(index);
      } else {
        ++space;
        ++index;
      }
      if (space == size) break;
    }
    if (space < size) {
      // No contiguous space, need to evict previous slot
      space = 0;
      index = _clock_ptr;
      while (1) {
        if (index == _n_slots) {
          space = 0;
          index = 0;
        }
        if (is_valid(index) && get_clock(index) != 0) {
          // the slot is valid and clock bits is not zero
          // cannot accommodate new item
          dec_clock(index);
          space = 0;
          index += get_space(index);
        } else {
          set_invalid(index);
          ++space;
          ++index;
        }
        if (space == size) break;
      }
      _clock_ptr = index;
    }
    //std::cout << "CABucket::clock_ptr: " << _clock_ptr << std::endl;
    index -= space;
    return index;
  }
  
  void CABucket::update(uint32_t ca_sig, uint32_t size)
  {
    uint32_t size_ = 0, value = 0;
    uint32_t index = lookup(ca_sig, size_);
    if (index != ~((uint32_t)0)) {
      if (size_ == size) {
        inc_clock(index);
        return ;
      } else {
        set_invalid(index);
      }
    }
    index = find_non_occupied_position(size);
    //std::cout << "return index in cabucket: " << index << std::endl;
    set_k(index, ca_sig);
    init_clock(index);
    set_space(index, size);
    set_valid(index);
  }

  void CABucket::erase(uint32_t ca_sig)
  {
    for (uint32_t index = 0; index < _n_slots; index++) {
      if (get_k(index) == ca_sig) {
        set_invalid(index);
      }
    }
  }
}
