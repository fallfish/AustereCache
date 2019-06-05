#include <cstring>
#include "bucket.h"

namespace cache {
  Bucket::Bucket(uint32_t n_bits_per_item, uint32_t n_items, uint32_t n_total_bytes) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_bits_per_item(n_bits_per_key + n_bits_per_value), _n_items(n_items),
    _n_total_bytes(n_total_bytes),
    _data(std::make_unique<uint8_t[]>(n_total_bytes))
  {} 
  Bucket::~Bucket() {}

  LBABucket::LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    Bucket(n_bits_per_key, n_bits_per_value, n_items, 
      ((n_bits_per_key + n_bits_per_value) * n_items + 7) / 8),
    _lru_pos(0)
  {
    memset(_data.get(), 0, _n_total_bytes);
  }

  LBABucket::~LBABucket() {}

  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  int LBABucket::find(uint8_t *key, uint8_t *value) {
    for (uint32_t i = 0; i < _n_items; i++) {
      uint32_t b = i * _n_bits_per_item;
      uint32_t e = b + _n_bits_per_key;
      uint32_t v = 0;
      bits_extract(b, e, v);
      if (v == *(uint32_t*)key) {
        if (value != nullptr) {
          b = e;
          e = b + _n_bits_per_value;
          v = 0;
          bits_extract(b, e, v);
          *(uint32_t*)value = v;
        }
        return i;
      }
    }
    return -1;
  }
  
  void LBABucket::insert(uint8_t *key, uint8_t *value) {
    int index = find(key, nullptr);
    if (index == -1) {
      index = _lru_pos;
      uint32_t b = index * _n_bits_per_item;
      uint32_t e = b + _n_bits_per_key;
      bits_encode(b, e, *(uint32_t*)key);
      //uint32_t k = 0;
      //bits_extract(b, e, k);
      //std::cout << *(uint32_t*)key << " " << k << std::endl;
      _lru_pos = (_lru_pos + 1 == _n_items) ? 0 : _lru_pos + 1;
    }
    uint32_t b = index * _n_bits_per_item + _n_bits_per_key;
    uint32_t e = b + _n_bits_per_value;
    bits_encode(b, e, *(uint32_t*)value);
    //uint32_t v = 0;
    //bits_extract(b, e, v);
    //std::cout << *(uint32_t*)value << " " << v << std::endl;
  }

  CABucket::CABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    Bucket(n_bits_per_key + n_bits_per_value, n_items, 
      ((n_bits_per_key + n_bits_per_value) * n_items + 7) / 8),
    _lru_pos(0)
  {
    memset(_data.get(), 0, _n_total_bytes);
  }

  CABucket::~CABucket() {}

  /*
   * memory is byte addressable
   * alignment issue needs to be dealed for each element
   *
   */
  int CABucket::find(uint32_t key, uint32_t &value)
  {
    for (uint32_t i = 0; i < _n_items; i++) {
      uint32_t b = i * _n_bits_per_item;
      uint32_t e = b + _n_bits_per_key;
      uint32_t k = 0;
      bits_extract(b, e, k);
      if (k == key) {
        b = e; e = (i + 1) * _n_bits_per_item;
        bits_extract(b, e, value);
        return i;
      }
    }
    return -1;
  }

  uint32_t CABucket::find(uint32_t key)
  {
    for (uint32_t i = 0; i < _n_items; i++) {
      uint32_t b = i * _n_bits_per_item;
      uint32_t e = b + _n_bits_per_key;
      uint32_t k = 0;
      bits_extract(b, e, k);
      if (k == key) {
        return i;
      }
    }
    return ~((uint32_t)0);
  }

  uint32_t CAIndex::find_non_occupied_position(uint32_t size)
  {
    uint32_t b, e;
    b = 0; e = b;
    // check whether there is a contiguous space
    while (b < _n_elems_per_bucket) {
      uint32_t v;
      do {
        v = get(e);
      } while (invalid(v) && (++e) && 
          (e < _n_elems_per_bucket) && (e - b < size));
      if (e - b >= size) break;
      if (e < _n_elems_per_bucket) {
        e += comp_code_to_size(comp_code(v));
      }
      b = e;
    }
    if (e - b < size) {
      // evict old elems until there is a contiguous space
      b = 0; e = b;
      do {
        if (e == _n_elems_per_bucket) {
          e = 0; b = 0;
        }
        v = get(e);
        // decrease the clock bit
        if (valid(v)) {
          clock_decrease(v);
          set(e, v);
        } 
        if (valid(v)) {
          e += comp_code_to_size(comp_code(v));
          b = e;
        } else {
          e += 1;
        }
      } while (e - b < size);
    }
    return b;
  }
  
  void CABucket::update(uint32_t key, uint32_t value)
  {
    uint32_t v;
    uint32_t index = find(key, v);
    uint32_t b, e;
    if (index != ~((uint32_t)0)) {
      if (comp_size(v) != size_to_comp_size(value)) {
        set_v(index, 0);
      } else {
        return ;
      }
    }
    value = (value << 3) & 1;
    set_k(index, key);
    set_v(index, value);
  }
}
