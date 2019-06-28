#ifndef __SSDDUP_H__
#define __SSDDUP_H__
#include <cstdint>
#include <memory>
#include "chunk/chunkmodule.h"
#include "deduplication/deduplicationmodule.h"
#include "compression/compressionmodule.h"
#include "manage/managemodule.h"
#include "utils/thread_pool.h"
#include <set>
#include <string>
#include <mutex>

namespace cache {
class SSDDup {
 public:
  SSDDup();
  ~SSDDup();
  void read(uint64_t addr, void *buf, uint32_t len);
  void write(uint64_t addr, void *buf, uint32_t len);
  void read_mt(uint64_t addr, void *buf, uint32_t len);
  void TEST_write(int device, uint64_t addr, void *buf, uint32_t len);
  void TEST_read(int device, uint64_t addr, void *buf, uint32_t len);
  inline void reset_stats() { _stats->reset(); }
 private:
  void internal_read(Chunk &c, bool update_metadata);
  void internal_write(Chunk &c);
  std::unique_ptr<ChunkModule> _chunk_module;
  std::unique_ptr<DeduplicationModule> _deduplication_module;
  std::unique_ptr<CompressionModule> _compression_module;
  std::unique_ptr<ManageModule> _manage_module;

  // Statistics
  std::unique_ptr<Stats> _stats;

  std::unique_ptr<ThreadPool> _thread_pool;
  std::mutex _mutex;
};
}


#endif
