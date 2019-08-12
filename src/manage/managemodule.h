#ifndef __MANAGEMODULE_H__
#define __MANAGEMODULE_H__

#include "common/common.h"
#include "managemodule.h"
#include "io/iomodule.h"
#include "chunk/chunkmodule.h"
#include "compression/compressionmodule.h"
#include "metadata/metadatamodule.h"

namespace cache {

class ManageModule {
 public:
  ManageModule(std::shared_ptr<IOModule> io_module, std::shared_ptr<MetadataModule> metadata_module);
  int read(Chunk &c);
  int write(Chunk &c);
  void update_metadata(Chunk &c);
  inline void sync() { _io_module->sync(); }
 private:
  std::shared_ptr<IOModule> _io_module;
  std::shared_ptr<MetadataModule> _metadata_module;
  bool preprocess_write_cache(
      Chunk &c, uint32_t &device_no,
      uint64_t &addr, uint8_t *&buf, uint32_t &len);
  bool preprocess_write_primary(
      Chunk &c, uint32_t &device_no,
      uint64_t &addr, uint8_t *&buf, uint32_t &len);
  void preprocess_read(Chunk &c, uint32_t &device_no, uint64_t &addr, uint8_t *&buf, uint32_t &len);

#if defined(CDARC)
  // maintain WEU to SSD locations
  std::map<uint32_t, uint64_t> _weu_to_ssd_location;
  uint32_t _weu_size;
  uint32_t _current_weu_id;
  uint64_t _current_ssd_location;
#else
#endif
};

}
#endif //__MANAGEMODULE_H__
