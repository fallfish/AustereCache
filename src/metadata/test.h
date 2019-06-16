//#include <iostream>
#ifndef __METADATA_TEST_H__
#define __METADATA_TEST_H__
#include <cstdint>
#include <list>
#include <common/config.h>
#include "metadata/index.h"
#include "metadata/bitmap.h"
#include "metadata/bucket.h"

class ClockIndex;
class LRUCache {
 public:
  LRUCache() {}
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

  void update(uint32_t key, uint32_t value,
    std::shared_ptr<ClockIndex> clock_index);
 private:
  std::vector<std::pair<uint32_t, uint32_t>> _data;
  uint32_t _size;
};

class ClockCache {
 public:
  ClockCache() {}
  ClockCache(uint32_t size) : _size(size), _clock_ptr(0) {
    _data.resize(_size);
    for (uint32_t i = 0; i < size; i++)
      _data[i] = Item();
  }

  uint32_t lookup(uint32_t key, uint32_t &value)
  {
    uint32_t index = 0;
    for ( ; index < _size; ++index) {
      if (_data[index]._clock != 0 && _data[index]._key == key) {
        value = _data[index]._value;
        return index;
      }
    }
    return ~(uint32_t)0;
  }

  void update(uint32_t key, uint32_t value)
  {
    uint32_t v;
    uint32_t index = lookup(key, v);
    if (index != ~(uint32_t)0) {
      //std::cout << "collide in clockcache: " << index << 
        //" size_: " << v << " size: " << value << std::endl;
      if (v == value) {
        if (_data[index]._clock != 3)
          ++_data[index]._clock;
        return ;
      } else {
        _data[index] = Item();
      }
    }
    index = 0;
    // find contiguous space from the begining
    uint32_t space = 0;
    while (index < _size) {
      if (_data[index]._clock == 0) {
        ++space;
        ++index;
      } else {
        space = 0;
        index += _data[index]._value;
      }
      if (space == value) {
        break;
      }
    }
//    std::cout << "ClockCache::space: " << space << " index: " << index << " value: " << value
//      << " _size: " << _size << std::endl;
    // eviction needed
    if (space < value) {
      space = 0;
      index = _clock_ptr;
      while (1) {
        if (index == _size) {
          index = 0;
          space = 0;
        }
        if (_data[index]._clock != 0) {
          //std::cout << "ClockCache::clock_dec:"
            //<< " index: " << index
            //<< " value prev: " << _data[index]._clock;
          --_data[index]._clock;
          //std::cout 
            //<< " value after: " << _data[index]._clock << std::endl;
        }

        if (_data[index]._clock != 0) {
          space = 0;
          index += _data[index]._value;
        } else {
          ++space;
          ++index;
        }
        if (space == value) break;
      }
      _clock_ptr = index;
    }
    index -= space;
    _data[index] = Item(key, value);
    //std::cout << "ClockCache::clock_ptr: " << _clock_ptr << std::endl;
    //std::cout << "return index in clockcache: " << index << std::endl;

    //std::cout << "ClockCache::update" << std::endl;
    //for (uint32_t i = 0; i < _size; i++) {
      //if (_data[i]._clock != 0) {
        //std::cout << "index: " << i << " ca_sig: " << _data[i]._key
          //<< " size: " << _data[i]._value << " clock: " << _data[i]._clock
          //<< std::endl;
      //}
    //}
  }


 private:
  struct Item {
    uint32_t _key;
    uint32_t _value;
    uint32_t _clock;
    bool _valid;

    Item() : _valid(false), _key(0), _value(0), _clock(0) {}
    Item(uint32_t key, uint32_t value) :
      _valid(true), _key(key), _value(value), _clock(1) {}
    Item(const Item &i) :
      _valid(i._valid), _key(i._key), _value(i._value), _clock(i._clock) {}
  };
  std::vector<Item> _data;
  uint32_t _size;
  uint32_t _clock_ptr;
};

TEST(Index, Bitmap)
{
  srand(0);
  uint32_t n_bits = 2000;
  uint32_t bit = 13;
  cache::Bitmap bm(n_bits);
  std::map<uint32_t, uint32_t> m;
  for (int i = 0; i < n_bits; i += bit) {
    uint32_t v = rand() & ((1 << bit) - 1);
    bm.store_bits(i, i + bit, v);
    m[i] = v;
  }
  for (int i = 0; i < n_bits; i += bit) {
    //std::cout << i << std::endl;
    uint32_t v = bm.get_bits(i, i + bit);
    EXPECT_EQ(v, m[i]);
  }
}

TEST(Index, LBABucket)
{
  srand(0);
  uint32_t n_bits_lba_sig = 12;
  uint32_t n_bits_ca_hash = 22;
  cache::LBABucket bucket(n_bits_lba_sig, n_bits_ca_hash, 32);
  LRUCache cache(32);
  std::map<uint32_t, uint32_t> map_bucket;
  for (uint32_t i = 0; i < 0; i++) {
    uint32_t op = rand() % 2;
    if (op == 1) {
      uint32_t lba_sig = rand() & ((1 << n_bits_lba_sig) - 1);
      uint32_t ca_hash = rand() % ((1 << n_bits_ca_hash) - 1);
      bucket.update(lba_sig, ca_hash, nullptr);
      cache.update(lba_sig, ca_hash);
      map_bucket[lba_sig] = ca_hash;
    } else {
      uint32_t lba_sig = rand() & ((1 << n_bits_lba_sig) - 1);
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

TEST(Index, CABucket)
{
  srand(0);
  uint32_t n_bits_ca_sig = 12;
  cache::CABucket ca_bucket(n_bits_ca_sig, 4, 32);
  ClockCache cache(32);
  std::map<uint32_t, uint32_t> map_bucket;
  for (uint32_t i = 0; i < 0; i++) {
    uint32_t op = rand() % 2;
    if (op == 0) {
      uint32_t ca_sig = rand() % ((1 << n_bits_ca_sig) - 1);
      uint32_t size = (rand() & 3) + 1;
      //std::cout << "update: " << ca_sig << " " << size << std::endl;
      ca_bucket.update(ca_sig, size);
      cache.update(ca_sig, size);
      map_bucket[ca_sig] = size;
    } else {
      uint32_t ca_sig = rand() % ((1 << n_bits_ca_sig) - 1);
      uint32_t size_ca = 0, size_cache = 0;
      uint32_t lookup_ca = -1, lookup_clock = -1;
      lookup_ca = ca_bucket.lookup(ca_sig, size_ca);
      lookup_clock = cache.lookup(ca_sig, size_cache);
      //std::cout << "lookup: " << ca_sig << " " << size_ca 
        //<< " " << size_cache << " " << map_bucket[ca_sig] << std::endl;
      EXPECT_EQ(lookup_ca, lookup_clock);
      EXPECT_EQ(size_ca, size_cache);
      if (lookup_ca != -1) {
        EXPECT_EQ(size_ca, map_bucket[ca_sig]);
      }
    }
  }
}

class LRUIndex {
 public:
  LRUIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
           uint32_t n_buckets, uint32_t n_items_per_bucket,
           std::shared_ptr<ClockIndex> clock_index) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_buckets(n_buckets), _n_items_per_bucket(n_items_per_bucket),
    _clock_index(clock_index)
  {
    for (uint32_t index = 0; index < _n_buckets; ++index) {
      _buckets[index] = LRUCache(_n_items_per_bucket);
    }
  }

  bool lookup(uint32_t lba_hash, uint32_t &ca_hash)
  {
    uint32_t bucket_no = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    return _buckets[bucket_no].lookup(signature, ca_hash) != ~((uint32_t)0);
  }

  void update(uint32_t lba_hash, uint32_t ca_hash)
  {
    uint32_t bucket_no = lba_hash >> _n_bits_per_key;
    uint32_t signature = lba_hash & ((1 << _n_bits_per_key) - 1);
    _buckets[bucket_no].update(signature, ca_hash, _clock_index);
  }


 private:
  LRUCache _buckets[1024];
  uint32_t _n_bits_per_key, _n_bits_per_value,
    _n_buckets, _n_items_per_bucket;
  std::shared_ptr<ClockIndex> _clock_index;
};
class ClockIndex {
 public:
  // n_bits_per_key = 12, n_bits_per_value = 4
  ClockIndex(uint32_t n_bits_per_key, uint32_t n_bits_per_value,
          uint32_t n_buckets, uint32_t n_items_per_bucket) :
    _n_bits_per_key(n_bits_per_key), _n_bits_per_value(n_bits_per_value),
    _n_buckets(n_buckets), _n_items_per_bucket(n_items_per_bucket)
  {
    for (uint32_t index = 0; index < _n_buckets; ++index) {
      _buckets[index] = ClockCache(_n_items_per_bucket);
    }
  }

  uint32_t compute_ssd_location(uint32_t bucket_no, uint32_t index)
  {
    // 8192 is chunk size, while 512 is the metadata size
    return (bucket_no * _n_items_per_bucket + index) * (Config::metadata_size + Config::sector_size);
  }

  bool lookup(uint32_t ca_hash, uint32_t &size, uint32_t &ssd_location)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key;
    uint32_t signature = ca_hash & ((1 << _n_bits_per_key) - 1);
    uint32_t value;
    uint32_t index = _buckets[bucket_no].lookup(signature, value);
    if (index == ~((uint32_t)0)) return false;
    ssd_location = compute_ssd_location(bucket_no, index);

    return true;
  }

  void update(uint32_t ca_hash, uint32_t size, uint64_t &ssd_location)
  {
    uint32_t bucket_no = ca_hash >> _n_bits_per_key;
    uint32_t signature = ca_hash & ((1 << _n_bits_per_key) - 1);

//     find contiguous spaces size can fit in
    _buckets[bucket_no].update(signature, size);
    uint32_t index = _buckets[bucket_no].lookup(signature, size);
//    std::cout << "ClockIndex: " << std::endl;
//    std::cout << "bucket_no: " << bucket_no << " index: "
//      << index << " size: " << size << std::endl;
    ssd_location = compute_ssd_location(bucket_no, index);
  }

 private:
  ClockCache _buckets[1024];
  uint32_t _n_bits_per_key, _n_bits_per_value,
    _n_buckets, _n_items_per_bucket;
};

void LRUCache::update(uint32_t key, uint32_t value,
                      std::shared_ptr<ClockIndex> clock_index)
{
  uint32_t v;
  uint32_t index = lookup(key, v);
  if (index != ~(uint32_t)0) {
  } else {
    uint32_t pos = 0;
    index = 0;
    while (index < _size) {
      uint32_t v, ssd_location;
      bool result = clock_index->lookup(_data[index].second, v, ssd_location);
      if (!result) {
//        std::cout << "LRUCache::evict: " << _data[index].second << std::endl;
        _data[index] = std::make_pair(0, 0);
        pos = index;
      }
      ++index;
    }
    index = pos;
  }
  while (index < _size - 1) {
    _data[index] = _data[index + 1];
    ++index;
  }
  _data[_size - 1] = std::make_pair(key, value);
}

TEST(Index, Index) {
  uint32_t n_buckets = 1024;
  std::shared_ptr<cache::CAIndex> ca_index =
    std::make_shared<cache::CAIndex>(12, 4, n_buckets, 32);
  std::unique_ptr<cache::LBAIndex> lba_index =
    std::make_unique<cache::LBAIndex>(12, 22, n_buckets, 32, ca_index);
  std::shared_ptr<ClockIndex> clock_index =
    std::make_shared<ClockIndex>(12, 4, n_buckets, 32);
  std::unique_ptr<LRUIndex> lru_index =
    std::make_unique<LRUIndex>(12, 22, n_buckets, 32, clock_index);

  srand(0);
  for (uint32_t i = 0; i < 0; i++) {

    uint32_t op = rand() % 2;
    if (op == 0) {
      uint32_t lba_sig = rand() & ((1 << 22) - 1);
      uint32_t ca_sig = rand() & ((1 << 22) - 1);
      uint32_t size = (rand() & 3) + 1;
      uint64_t ssd_location_ca = 0;
      uint64_t ssd_location_clock = 0;
      {
        clock_index->update(ca_sig, size, ssd_location_clock);
        lru_index->update(lba_sig, ca_sig);
      }
      {
        ca_index->update(ca_sig, size, ssd_location_ca);
        lba_index->update(lba_sig, ca_sig);
      }
      EXPECT_EQ(ssd_location_ca, ssd_location_clock);
//      std::cout << "lba_sig: " << lba_sig << " ca_sig: " << ca_sig << std::endl;
    } else {
      uint32_t lba_sig = rand() & ((1 << 22) - 1);
//      std::cout << "lba_sig: " << lba_sig << std::endl;
      uint32_t ca_sig_clock = 0, ca_sig_ca = 0;
      uint32_t size_ca = 0, size_clock = 0;
      uint64_t ssd_location_ca = 0;
      uint64_t ssd_location_clock = 0;
      {
        lru_index->lookup(lba_sig, ca_sig_clock);
        ca_index->lookup(ca_sig_clock, size_clock, ssd_location_clock);
      }
      {
        lba_index->lookup(lba_sig, ca_sig_ca);
        ca_index->lookup(ca_sig_ca, size_ca, ssd_location_ca);
      }
      EXPECT_EQ(ca_sig_ca, ca_sig_clock);
      EXPECT_EQ(size_ca, size_clock);
      EXPECT_EQ(ssd_location_ca, ssd_location_clock);
    }
  }

}
#endif
