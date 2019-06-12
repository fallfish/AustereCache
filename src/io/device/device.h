#ifndef __DEVICE_H__
#define __DEVICE_H__
#include <cstdint>
#include "metadata/index.h"

namespace cache {

class Device {
 public:
  virtual ~Device();
  virtual uint32_t read(uint8_t* buf, uint32_t len, uint32_t offset)  = 0;
  virtual uint32_t write(uint8_t* buf, uint32_t len, uint32_t offset) = 0;
 protected:
  int _handler;
  void *_device_buffer;
};

class BlockDevice : public Device {
 public:
  ~BlockDevice();
  uint32_t read(uint8_t* buf, uint32_t len, uint32_t offset);
  uint32_t write(uint8_t* buf, uint32_t len, uint32_t offset);
  uint32_t open(char *filename, uint32_t size);
 private:

};

}


#endif
