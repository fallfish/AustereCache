#ifndef __IOMODULE_H__
#define __IOMODULE_H__
#include <cstdint>
#include "device/device.h"
namespace cache {

class IOModule {
 public:
  IOModule();
  uint32_t add_cache_device(char *filename, uint32_t type);
  uint32_t add_primary_device(char *filename);
  uint32_t read(uint8_t *buf, uint32_t len, uint32_t addr);
  uint32_t write(uint8_t *buf, uint32_t len, uint32_t addr);
 private:
  // Currently, we assume that only one cache and one primary
  std::unique_ptr< Device > _cache_device;
  std::unique_ptr< Device > _primary_device;
};

}

#endif //__IOMODULE_H__
