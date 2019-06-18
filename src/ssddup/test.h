#ifndef __SSDDUP_TEST_H__
#define __SSDDUP_TEST_H__

#include <fstream>
#include "ssddup.h"

TEST(SSDDup, SSDDup)
{
  srand(0);
  cache::SSDDup ssddup;
  uint64_t size = 1024 * 1024 * 512;
  char *test = (char *)aligned_alloc(512, size);
  char *_test = (char *)aligned_alloc(512, size);

  std::ifstream file("input", std::ios::binary | std::ios::ate);
  size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (file.read(test, size))
  {
    /* worked! */
  }

  std::cout << size << std::endl;
  ssddup.write(0, test, size);
  uint64_t total_bytes = 0;
  for (uint32_t i = 0; i < 5000; i++) {
    std::cout << "read: " << i << std::endl;

    uint64_t begin = rand() % size;
    uint64_t end = begin + rand() % (1024 * 128);
    if (end >= size) end = size - 1;
    if (begin == end) continue;
//    uint32_t op = rand() % 2;
    uint32_t op = 1;

    if (op == 0) {
      for (uint32_t i = begin; i < end; i++) {
        test[i] = rand() & 0xff;
      }
      ssddup.write(begin, test + begin, end - begin);
    } else {
      ssddup.read(begin, _test + begin, end - begin);
      total_bytes += end - begin;
      EXPECT_EQ(compare_array(test + begin, _test + begin, end - begin), true);
    }

//    std::cout << "begin: " << begin << " end: " << end << " total: " << end - begin << std::endl;
  }
  std::cout << (double)total_bytes / (1024 * 1024) << std::endl;
  free(test);
  free(_test);
}


#endif //__SSDDUP_TEST_H__
