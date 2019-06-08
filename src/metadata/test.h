//#include <iostream>
#ifndef __METADATA_TEST_H__
#define __METADATA_TEST_H__
#include <cstdint>
#include <list>
#include "metadata/index.h"
#include "metadata/bitmap.h"
#include "metadata/bucket.h"

class LRUCache {
 public:
  LRUCache(uint32_t size) : _size(size)
  {
    _data.resize(size);
    for (uint32_t i = 0; i < size; i++)
      _data[i] = std::make_pair(0, 0);
  }

  uint32_t lookup(uint32_t key, uint32_t &value)
  {
    uint32_t index = 0;
    while (index < _size) {
      if (_data[index].first == key) {
        value = _data[index].second;
        return index;
      }
      ++index;
    }
    return ~(uint32_t)0;
  }

  void update(uint32_t key, uint32_t value)
  {
    uint32_t v;
    uint32_t index = lookup(key, v);
    if (index == ~(uint32_t)0) {
      index = 0;    
    }
    while (index < _size - 1) {
      _data[index] = _data[index + 1];
      ++index;
    }
    _data[_size - 1] = std::make_pair(key, value);
  }
 private:
  std::vector<std::pair<uint32_t, uint32_t>> _data;
  uint32_t _size;
};

TEST(Index, Bitmap)
{
  srand(0);
  cache::Bitmap bm(1000);
  for (int i = 0; i < 100000; i++) {
    int index = rand() % 1000;
    int op = rand() % 2;
    if (op == 0) {
      bm.set(index);
      EXPECT_EQ(bm.get(index), true);
    } else {
      bm.clear(index);
      EXPECT_EQ(bm.get(index), false);
    }
  }
}

TEST(Index, LBABucket)
{
  srand(0);
  cache::LBABucket bucket(12, 22, 32);
  LRUCache cache(32);
  std::map<uint32_t, uint32_t> map_bucket;
  for (uint32_t i = 0; i < 1000000; i++) {
    uint32_t op = rand() % 2;
    if (op == 1) {
      uint32_t lba_sig = rand() & ((1 << 12) - 1);
      uint32_t ca_hash = rand() % ((1 << 22) - 1);
      bucket.update(lba_sig, ca_hash, nullptr);
      cache.update(lba_sig, ca_hash);
      map_bucket[lba_sig] = ca_hash;
    } else {
      uint32_t lba_sig = rand() & ((1 << 12) - 1);
      uint32_t ca_hash = 0, ca_hash_lru = 0;
      uint32_t lookup_bucket = -1, lookup_lru = -1;
      lookup_bucket = bucket.lookup(lba_sig, ca_hash);
      lookup_lru = cache.lookup(lba_sig, ca_hash_lru);
      EXPECT_EQ(lookup_bucket, lookup_lru);
      EXPECT_EQ(ca_hash, ca_hash_lru);
      if (lookup_bucket != -1 && lookup_lru != -1) {
        EXPECT_EQ(ca_hash, map_bucket[lba_sig]);
      }
    }
  }
}

TEST(Index, LBAIndex)
{
}
#endif
