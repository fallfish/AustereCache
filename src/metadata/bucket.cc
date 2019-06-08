#include <cstring>
#include "bucket.h"
#include "index.h"

namespace cache {
  Bucket::Bucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_item(n_bits_per_key + n_bits_per_value), _n_items(n_items),
    _n_total_bytes(((n_bits_per_key + n_bits_per_value) * n_items + 7) / 8),
    _data(std::make_unique<uint8_t[]>(_n_total_bytes))
  {} 
  Bucket::~Bucket() {}

  LBABucket::LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    Bucket(n_bits_per_key, n_bits_per_value, n_items)
  {
    memset(_data.get(), 0, _n_total_bytes);
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
      //uint32_t k = get_k(i);
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

    //std::cout << lba_sig << " " << ca_hash << std::endl;
    //lba_sig = get_k(31);
    //ca_hash = get_v(31);
    //std::cout << lba_sig << " " << ca_hash << std::endl;
    //std::cout << std::endl;
  }

  CABucket::CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    Bucket(n_bits_per_key, n_bits_per_value, n_items)
  {
    memset(_data.get(), 0, _n_total_bytes);
  }

  CABucket::~CABucket() {}

  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  int CABucket::lookup(uint32_t ca_sig, uint32_t &size)
  {
    for (uint32_t i = 0; i < _n_items; i++) {
      uint32_t k = get_k(i);
      if (k == ca_sig) {
        size = v_to_size(get_v(i));
        return i;
      }
    }
    return ~((uint32_t)0);
  }

  uint32_t CABucket::find_non_occupied_position(uint32_t size)
  {
    uint32_t b, e, v;
    b = 0; e = b;
    // check whether there is a contiguous space
    while (b < _n_items) {
      do {
        v = get_v(e);
      } while (!valid(v) && (++e) && 
          (e < _n_items) && (e - b < size));
      if (e - b >= size) break;
      if (e < _n_items) {
        e += v_to_size(v);
      }
      b = e;
    }
    if (e - b < size) {
      // evict old elems until there is a contiguous space
      b = 0; e = b;
      do {
        if (e == _n_items) {
          e = 0; b = 0;
        }
        v = get_v(e);
        // decrease the clock bit
        if (valid(v)) {
          clock_dec(v);
          set_v(e, v);
        } 
        if (valid(v)) {
          e += comp_code_to_size(v_to_comp_code(v));
          b = e;
        } else {
          e += 1;
        }
      } while (e - b < size);
    }
    return b;
  }
  
  void CABucket::update(uint32_t ca_sig, uint32_t size)
  {
    uint32_t value;
    uint32_t index = lookup(ca_sig, value);
    if (index != ~((uint32_t)0)) {
      if (v_to_comp_code(value) != size_to_comp_code(size)) {
        set_v(index, 0);
      } else {
        clock_inc(value);
        set_v(index, value);
        return ;
      }
    }
    index = find_non_occupied_position(size);
    value = size_to_v(size);
    set_k(index, ca_sig);
    set_v(index, value);
  }
}
