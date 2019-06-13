#ifndef __DEVICE_H__
#define __DEVICE_H__
#include <cstdint>
#include <sys/stat.h>
#include "metadata/index.h"

namespace cache {

class Device {
 public:
  virtual ~Device();
  virtual int read(uint64_t addr, uint8_t* buf, uint32_t len)  = 0;
  virtual int write(uint64_t addr, uint8_t* buf, uint32_t len) = 0;
 protected:
  int _fd;
  void *_device_buffer;
  uint64_t _size;
};

class BlockDevice : public Device {
 public:
  BlockDevice();
  ~BlockDevice();
  int read(uint64_t addr, uint8_t* buf, uint32_t len);
  int write(uint64_t addr, uint8_t* buf, uint32_t len);
  int open(char *filename, uint64_t size);
 private:
  int open_new_device(char *filename, uint64_t size);
  int open_existing_device(char *filename, uint64_t size, struct stat *statbuf);
  int get_size(int fd);
};

//class MemoryBlockDevice : public Device {
// public:
//  memoryblockdevice();
//  ~memoryblockdevice();
//  int read(uint64_t addr, uint8_t* buf, uint32_t len);
//  int write(uint64_t addr, uint8_t* buf, uint32_t len);
//  int open(uint64_t size);
//};

}


#endif
