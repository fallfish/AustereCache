#ifndef __IOMODULE_TEST_H__
#define __IOMODULE_TEST_H__
#include <cstdint>
#include "iomodule.h"
#include "device/device.h"
#include "fcntl.h"

bool compare_array(void *a, void *b, uint32_t length)
{
  for (uint32_t i = 0; i < length; i++) {
   if (((char*)a)[i] != ((char*)b)[i]) return false;
  }
  return true;
}

TEST(IOModule, BlockDevice)
{
  srand(0);
  cache::BlockDevice block_device;
  uint64_t size = 1024 * 1024;
  uint64_t block_size = 512;
  char buf[size];

  block_device.open("test", size);
  for (uint32_t i = 0; i < size; i++)
    buf[i] = rand() & 255;
  for (uint64_t addr = 0; addr < size; addr += block_size) {
    block_device.write(addr, (uint8_t*)buf + addr, block_size);
  }
  char read_buf[size];
  for (uint64_t addr = 0; addr < size; addr += block_size) {
    block_device.read(addr, (uint8_t*)read_buf + addr, block_size);

    EXPECT_EQ(compare_array(buf + addr, read_buf + addr, block_size), true);
  }
}

TEST(IOModule, MemoryBlockDevice)
{

}
#endif //IOMODULE_TEST_H
