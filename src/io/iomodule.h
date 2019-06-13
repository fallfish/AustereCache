#ifndef __IOMODULE_H__
#define __IOMODULE_H__
#include <cstdint>
#include "device/device.h"
namespace cache {

class IOModule {
 public:
  IOModule();
  uint32_t add_cache_device(char *filename);
  uint32_t add_primary_device(char *filename);
  uint32_t read(uint32_t device, uint64_t addr, void *buf, uint32_t len);
  uint32_t write(uint32_t device, uint64_t addr, void *buf, uint32_t len);
 private:
  // Currently, we assume that only one cache and one primary
  std::unique_ptr< BlockDevice > _cache_device;
  std::unique_ptr< BlockDevice > _primary_device;
};

}

#endif //__IOMODULE_H__
