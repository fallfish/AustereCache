#include <cstring>
#include "bucket.h"

namespace cache {
  Bucket::Bucket(uint32_t n_bits_per_item, uint32_t n_items, uint32_t n_total_bytes) :
    _n_bits_per_item(n_bits_per_item), _n_items(n_items),
    _n_total_bytes(n_total_bytes),
    _data(std::make_unique<uint8_t[]>(n_total_bytes))
  {} 
  Bucket::~Bucket() {}

  LBABucket::LBABucket(uint32_t n_bits_per_key, uint32_t n_bits_per_value, uint32_t n_items) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    Bucket(n_bits_per_key + n_bits_per_value, n_items, 
      ((n_bits_per_key + n_bits_per_value) * n_items + 7) / 8),
    _lru_pos(0)
  {
    memset(_data.get(), 0, _n_total_bytes);
  }

  LBABucket::~LBABucket() {}

  inline void Bucket::bits_extract(uint32_t b, uint32_t e, uint32_t &v) {
    uint8_t shift = 0;
    if (b & 7) {
      v |= _data[b >> 3] >> (b & 7);
      shift += 8 - (b & 7);
      b = ((b >> 3) + 1) << 3;
    }
    while (b < e) {
      v |= _data[b >> 3] << shift;
      shift += 8;
      b += 8;
    }
    if (b > e) {
      v &= (1 << (shift - b + e)) - 1;
    }
  }
  inline void Bucket::bits_encode(uint32_t b, uint32_t e, uint32_t v) {
    if (b & 7) {
      _data[b >> 3] &= ((1 << (b & 7)) - 1);
      _data[b >> 3] |= (v & ((1 << (8 - (b & 7))) - 1)) << (b & 7);
      v >>= 8 - (b & 7);
      b = ((b >> 3) + 1) << 3;
    }
    while (b + 8 <= e) {
      _data[b >> 3] = v & 0xff;
      v >>= 8;
      b += 8;
    }
    if (b < e) {
      _data[b >> 3] &= ~((1 << (e - b)) - 1);
      _data[b >> 3] |= v & ((1 << (e - b)) - 1);
    }
  }
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
  int CABucket::find(uint8_t *key, uint8_t *value) {
    for (uint32_t i = 0; i < _n_items; i++) {
      uint32_t b = i * _n_bits_per_item;
      uint32_t e = b + _n_bits_per_key;
      uint32_t v = 0;
      bits_extract(b, e, v);
      if (v == *(uint32_t*)key) {
        if (value != nullptr) {
          b = e;
          e = (i + 1) * _n_bits_per_item;
          bits_extract(b, e, v);
          *(uint32_t*)value = v;
        }
        return i;
      }
    }
    return -1;
  }
  
  void CABucket::insert(uint8_t *key, uint8_t *value) {
    int index = find(key, nullptr);
    if (index == -1) {
      //std::cout << "index: " << index << std::endl;
      index = _lru_pos;
      uint32_t b = index * _n_bits_per_item;
      uint32_t e = b + _n_bits_per_key;
      bits_encode(b, e, *(uint32_t*)key);
      _lru_pos = (_lru_pos + 1 == _n_items) ? 0 : _lru_pos + 1;
    }
    uint32_t b = index * _n_bits_per_item + _n_bits_per_key;
    uint32_t e = b + _n_bits_per_value;
    bits_encode(b, e, *(uint32_t*)value);
  }
}
