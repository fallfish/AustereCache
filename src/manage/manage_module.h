#ifndef __MANAGEMODULE_H__
#define __MANAGEMODULE_H__

#include "common/common.h"
#include "manage_module.h"
#include "io/io_module.h"
#include "chunking/chunk_module.h"
#include "compression/compression_module.h"
#include "metadata/metadata_module.h"

namespace cache {

class ManageModule {
 public:
  static ManageModule& getInstance();
  int read(Chunk &chunk);
  int write(Chunk &chunk);
  void updateMetadata(Chunk &chunk);
 private:
  ManageModule();
  bool generateCacheWriteRequest(
    Chunk &chunk, DeviceType &deviceType,
    uint64_t &addr, uint8_t *&buf, uint32_t &len);
  bool generatePrimaryWriteRequest(
    Chunk &chunk, DeviceType &deviceType,
    uint64_t &addr, uint8_t *&buf, uint32_t &len);
  void generateReadRequest(Chunk &chunk, DeviceType &deviceType,
    uint64_t &addr, uint8_t *&buf, uint32_t &len);


#if defined(CDARC)
 public:
  // maintain WEU to SSD locations
  std::map<uint32_t, uint64_t> weuToCachedataLocation_;
  uint32_t weuSize_;
  uint32_t currentWEUId_;
  uint64_t currentCachedataLocation_;
#endif
};

}
#endif //__MANAGEMODULE_H__
