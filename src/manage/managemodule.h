#ifndef __MANAGEMODULE_H__
#define __MANAGEMODULE_H__

#include "common/common.h"
#include "managemodule.h"
#include "io/writebuffer.h"
#include "io/iomodule.h"
#include "chunk/chunkmodule.h"
#include "compression/compressionmodule.h"
#include "metadata/metadatamodule.h"

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
  void generateReadRequest(Chunk &chunk, DeviceType &deviceType, uint64_t &addr, uint8_t *&buf, uint32_t &len);


#if defined(CDARC)
  // maintain WEU to SSD locations
  std::map<uint32_t, uint64_t> weuToCachedataLocation_;
  uint32_t weuSize_;
  uint32_t currentWEUId_;
  uint64_t currentCachedataLocation_;
#endif
};

}
#endif //__MANAGEMODULE_H__
