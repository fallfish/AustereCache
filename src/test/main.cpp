#include <iostream>
#include "index/index.h"
#include "index/bitmap.h"
#include "index/bucket.h"
#include "device/device.h"
#include "gtest/gtest.h"

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
  std::map<uint32_t, uint32_t> mp;
  cache::LBABucket bucket(8, 1024);
  for (int i = 0; i < 1024; i++) {
    uint32_t key = rand();
    uint32_t value = rand();
    bucket.insert((uint8_t*)&key, 4, (uint8_t*)&value);
    mp[key] = value;
    uint32_t _value;
    bucket.find((uint8_t*)&key, 4, (uint8_t*)&_value);
    EXPECT_EQ(_value, value);
  }

  int count = 0;
  for (auto pr : mp) {
    uint32_t key = pr.first;
    uint32_t value;
    if (bucket.find((uint8_t*)&key, 4, (uint8_t*)&value) != 1) {
      count += 1;
      EXPECT_EQ(value, pr.second);
    }
  }
  //std::cout << count << std::endl;
}

TEST(Index, LBAIndex)
{
  srand(0);
  std::map<uint32_t, uint32_t> mp;
  cache::LBAIndex index(2, 4, 16, 1024);
  for (int i = 0; i < 16 * 1024; i++) {
    uint32_t key = rand();
    uint32_t value = rand();
    index.set((uint8_t*)&key, (uint8_t*)&value);
    mp[key] = value;
    uint32_t _value;
    if (index.lookup((uint8_t*)&key, (uint8_t*)&_value)) {
      EXPECT_EQ(_value, value);
    }
  }

  int count = 0, count2 = 0;
  for (auto pr : mp) {
    uint32_t key = pr.first;
    uint32_t value;
    if (index.lookup((uint8_t*)&key, (uint8_t*)&value)) {
      count += 1;
      if (value == pr.second)
        count2 += 1;
      //EXPECT_EQ(*(uint32_t*)value, pr.second);
    }
  }
}

TEST(Device, BlockDevice) {
  cache::BlockDevice bdevice;
  bdevice.open("./tmp");
  //char arr[512] = {'a'};
  //memset(arr, 'a', 511);
  //arr[511] = '\0';
  //bdevice.write(0, 512, (uint8_t*)arr);
  //char res[512];
  //res[511] = '\0';
  //bdevice.read(0, 512, (uint8_t*)res);
  //std::cout << res << std::endl;
  
}
