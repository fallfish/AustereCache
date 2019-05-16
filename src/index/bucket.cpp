#include <cstring>
#include "bucket.h"

namespace cache {
  Buclet::Bucket(uint32_t n_bits_per_item, uint32_t n_items, uint32_t n_total_bytes) :
    _n_bits_per_item(n_bits_per_item), _n_items(n_items),
    _n_total_bytes(n_total_bytes),
    _data(std::make_unique<uint8_t[]>(alignas(16) n_total_bytes)
  {} 
  Bucket::~Bucket() {}

  LBABucket::LBABucket(uint32_t n_bits_per_item, uint32_t n_items) :
    Bucket(n_bits_per_item, n_items, (n_bits_per_item * n_items + 7) / 8))
  {
    _lru_pos = 0;
    memset(_data.get(), 0, _n_total_bytes);
  }

  LBABucket::~LBABucket() {}

  int LBABucket::find(uint8_t *key, uint32_t n_bits_per_key, uint8_t *value) {
    for (int i = 0; i < _n_items; i++) {
    }
  }
  
  void LBABucket::insert(uint8_t *key, uint32_t len, uint8_t *value) {
    int index = find(key, len, nullptr);
    if (index != -1) {
      //std::cout << "index: " << index << std::endl;
      memcpy(_data.get() + index * _width + len, value, _width - len);
    } else {
      memcpy(_data.get() + _lru_pos * _width, key, len);
      memcpy(_data.get() + _lru_pos * _width + len, value, _width - len);
      _lru_pos = (_lru_pos + 1 == _num) ? 0 : _lru_pos + 1;
    }
  }

  CABucket::CABucket(uint32_t width, uint32_t num) :
    Bucket(width, num, (1 + width) * num)
  {
    memset(_data.get(), 0, (1 + width) * num);
  }
  CABucket::~CABucket() {}

  int CABucket::find(uint8_t *key, uint32_t len, uint8_t *value) {
    for (int index = 0; index < _num; index++) {
      if (memcmp(_data.get() + index * _width, key, len) == 0) {
        if (value != nullptr) {
          
          memcpy(value, _data.get() + index * _width + len, _width - len);
        }
        return index;
      }
    }
    return -1;
  }
  
  void CABucket::insert(uint8_t *key, uint32_t len, uint8_t *value) {
    int index = find(key, len, value);
    if (index != -1) {
      memcpy(_data.get() + index * _width + len, value, _width - len);
    } else {
      //memcpy(_data + lru_pos * _width, key, len);
      //memcpy(_data + lru_pos * _width + len, value, _width - len);
      //lru_pos = (lru_pos + 1 == num) ? 0 : lru_pos + 1;
    }
  }
}
