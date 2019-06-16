#ifndef __SSDDUP_H__
#define __SSDDUP_H__
#include <cstdint>
#include <memory>
#include "chunk/chunkmodule.h"
#include "deduplication/deduplicationmodule.h"
#include "compression/compressionmodule.h"
#include "manage/managemodule.h"
#include <set>
#include <string>

namespace cache {
class SSDDup {
 public:
  SSDDup();
  void read(uint64_t addr, void *buf, uint32_t len);
  void write(uint64_t addr, void *buf, uint32_t len);
  void write_TEST(uint64_t addr, void *buf, uint32_t len);
 private:
  void internal_read(Chunk &c, bool update_metadata);
  void internal_write(Chunk &c);
  std::unique_ptr<ChunkModule> _chunk_module;
  std::unique_ptr<DeduplicationModule> _deduplication_module;
  std::unique_ptr<CompressionModule> _compression_module;
  std::unique_ptr<ManageModule> _manage_module;

  // For TEST
  std::set<std::string> _fingerprints;
};
}


#endif
