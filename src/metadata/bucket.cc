#include <cstring>
#include "bitmap.h"
#include "bucket.h"
#include "index.h"

namespace cache {
  Bucket::Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_item(n_bits_per_key + n_bits_per_value), _n_items(n_items),
    _n_total_bytes(((n_bits_per_key + n_bits_per_value) * n_items + 7) / 8),
    _bits(std::make_unique<Bitmap>((n_bits_per_key + n_bits_per_value) * n_items))
  {} 

  Bucket::~Bucket() {}

  LBABucket::LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    Bucket(n_bits_per_key, n_bits_per_value, n_items)
  {
  }

  LBABucket::~LBABucket() {}

  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  uint32_t LBABucket::lookup(uint32_t lba_sig, uint32_t &ca_hash) {
    for (uint32_t index = 0; index < _n_items; index++) {
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
    for (uint32_t i = index; i < _n_items - 1; i++) {
      set_k(i, get_k(i + 1));
      set_v(i, get_v(i + 1));
    }
    set_k(_n_items - 1, k);
    set_v(_n_items - 1, v);
  }

  uint32_t LBABucket::find_non_occupied_position(std::shared_ptr<CAIndex> ca_index)
  {
    uint32_t position = 0;
    bool valid = true;
    for (uint32_t i = 0; i < _n_items; i++) {
      uint32_t v = get_v(i);
      uint32_t size, ssd_location; // useless variables
      if (ca_index != nullptr)
        valid = ca_index->lookup(v, size, ssd_location);
      if (!valid) {
        set_k(i, 0), set_v(i, 0);
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
      index = find_non_occupied_position(ca_index);
      set_k(index, lba_sig);
      set_v(index, ca_hash);
    }
    advance(index);

  }

  CABucket::CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    Bucket(n_bits_per_key, n_bits_per_value, n_items)
  {
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
    uint32_t index = 0;
    while (index < _n_items) {
      uint32_t k = get_k(index);
      uint32_t v = get_v(index);
      if (valid(v)) {
        if (k == ca_sig) {
          size = v_to_size(v);
          return index;
        }
      }
      ++index;
    }
    return ~((uint32_t)0);
  }

  uint32_t CABucket::find_non_occupied_position(uint32_t size)
  {
    uint32_t index = 0, v, space = 0;
    // check whether there is a contiguous space
    while (index < _n_items) {
      v = get_v(index);
      if (valid(v)) {
        space = 0;
        index += v_to_size(v);
      } else {
        ++space;
        ++index;
      }
      if (space == size) break;
    }
    //std::cout << "CABucket::space: " << space << " index: " << index << " size: " << size << std::endl;
    if (space < size) {
      space = 0;
      index = _clock_ptr;
      while (1) {
        if (index == _n_items) {
          space = 0;
          index = 0;
        }
        v = get_v(index);
        if (valid(v)) {
          //std::cout << "CABucket::clock_dec:"
            //<< " index: " << index
            //<< " value prev: " << (v & 3);
          clock_dec(v);
          set_v(index, v);
          //std::cout 
            //<< " value after: " << (v & 3) << std::endl;
        }
        if (valid(v)) {
          space = 0;
          index += v_to_size(v);
        } else {
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
      //std::cout << "collide in cabucket: " << index << 
        //" size_: " << size_ << " size: " << size << std::endl;
      if (size_ == size) {
        value = get_v(index);
        clock_inc(value);
        set_v(index, value);
        return ;
      } else {
        set_k(index, 0);
        set_v(index, 0);
      }
    }
    index = find_non_occupied_position(size);
    //std::cout << "return index in cabucket: " << index << std::endl;
    value = size_to_v(size);
    set_k(index, ca_sig);
    set_v(index, value);
    //std::cout << "CABucket::update" << std::endl;
    //for (uint32_t i = 0; i < _n_items; i++) {
      //uint32_t k = get_k(i), v = get_v(i);
      //if (valid(v)) {
        //std::cout << "index: " << i << " ca_sig: " << k
          //<< " size: " << v_to_size(v) << " clock: " << (v & 3)
          //<< std::endl;
      //}
    //}
    //set_k(6, 3);
    //set_v(6, (2 << 2) | 1);
    //std::cout << "233333333: " << get_k(6) << std::endl;
    ////set_k(5, 7);
    //set_v(5, (1 << 2) | 1);
    //std::cout << "233333333: " << get_k(6) << std::endl;
  }
}
