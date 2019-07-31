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
#include <unistd.h>
#include <string>
#include <ios>
#include <iostream>
#include <fstream>

namespace cache {
class SSDDup {
 public:
  SSDDup();
  ~SSDDup();
  void read(uint64_t addr, void *buf, uint32_t len);
  void write(uint64_t addr, void *buf, uint32_t len);
  void read_mt(uint64_t addr, void *buf, uint32_t len);
  void read_singlethread(uint64_t addr, void *buf, uint32_t len);
  void TEST_write(int device, uint64_t addr, void *buf, uint32_t len);
  void TEST_read(int device, uint64_t addr, void *buf, uint32_t len);
  inline void reset_stats() { _stats->reset(); }
  inline void dump_stats() { _stats->dump(); }
  inline void sync() { _manage_module->sync(); }
  void process_mem_usage(double& vm_usage, double& resident_set)
  {
    using std::ios_base;
    using std::ifstream;
    using std::string;

    vm_usage     = 0.0;
    resident_set = 0.0;

    // 'file' stat seems to give the most reliable results
    //
    ifstream stat_stream("/proc/self/stat",ios_base::in);

    // dummy vars for leading entries in stat that we don't care about
    //
    string pid, comm, state, ppid, pgrp, session, tty_nr;
    string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    string utime, stime, cutime, cstime, priority, nice;
    string O, itrealvalue, starttime;

    // the two fields we want
    //
    unsigned long vsize;
    long rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
      >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
      >> utime >> stime >> cutime >> cstime >> priority >> nice
      >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

    stat_stream.close();

    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
    vm_usage     = vsize / 1024.0;
    resident_set = rss * page_size_kb;
  }

 private:
  void internal_read(Chunk &c, bool update_metadata);
  void internal_write(Chunk &c);
  std::unique_ptr<ChunkModule> _chunk_module;
  std::unique_ptr<DeduplicationModule> _deduplication_module;
  std::shared_ptr<CompressionModule> _compression_module;
  std::unique_ptr<ManageModule> _manage_module;

  // Statistics
  Stats* _stats;

  std::unique_ptr<ThreadPool> _thread_pool;
  std::mutex _mutex;
};
}


#endif
