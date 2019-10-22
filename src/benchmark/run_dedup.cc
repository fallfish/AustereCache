#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"
#include "deduplication/deduplicationmodule.h"
#include "workload_conf.h"
#include <thread>

namespace cache {

class RunDeduplicationModule {
 public:
  RunDeduplicationModule()
  {
  }

  void parse_argument(int argc, char **argv)
  {
    char *param, *value;
    double wr_ratio = 0;
    for (int i = 1; i < argc; i += 2) {
      param = argv[i];
      value = argv[i + 1];
      if (value == nullptr) {
        std::cout << "RunDeduplicationModule::parse_argument invalid argument!" << std::endl;
        //print_help();
        exit(1);
      }
      if (strcmp(param, "--trace") == 0) {
        // read workload configuration
        int fd = open(value, O_RDONLY);
        int n = read(fd, &_workload_conf, sizeof(cache::WorkloadConfiguration));
        if (n != sizeof(cache::WorkloadConfiguration)) {
          std::cout << "RunDeduplicationModule:parse_argument read configuration failed!" << std::endl;
          exit(1);
        }

        _n_chunks = _workload_conf._working_set_size / _workload_conf._chunk_size;
        // read working set
        posix_memalign(reinterpret_cast<void **>(_chunks), 512, _n_chunks * sizeof(Chunk));
        n = read(fd, _chunks, _n_chunks * sizeof(Chunk));

        if (n != _n_chunks * sizeof(Chunk)) {
          std::cout << "RunDeduplicationModule: read working set failed!" << std::endl; 
          exit(1);
        }
      }
    }
    IOModule::getInstance().addCacheDevice("./ramdisk/cache_device");
    //ioModule_->addCacheDevice("/dev/sda");
  }

  void warm_up()
  {
    for (int i = 0; i < _n_chunks; i++) {
    alignas(512) Chunk c;
      // the lba hash and ca hash should be adapted to the metadata configuration
      // in the trace, the hash value is all full 32 bit hash
      // while in the metadata module, we will have (signature + bucket_no) format
      _chunks[i].lbaHash_ >>= 32 -
                              (Config::getInstance().getnBitsPerLBASignature() + Config::getInstance().getnBitsPerLBABucketId());
      _chunks[i].fingerprintHash_ >>= 32 -
        (Config::getInstance().getnBitsPerFPSignature() + Config::getInstance().getnBitsPerFPBucketId());
      memcpy(&c, _chunks + i, sizeof(Chunk));
      DeduplicationModule::dedup(c);
      MetadataModule::getInstance().update(c);
    }
  }

  void work(int n_requests, int &n_hits, uint64_t &n_total)
  {
    for (int i = 0; i < n_requests; i++) {
      int begin = rand() % _n_chunks;
      int end = begin + rand() % 4;
      if (end >= _n_chunks) end = _n_chunks - 1;
      std::vector<std::thread> threads;
      for (int j = begin; j < end; j++) {
        ++ n_total;
        //threads.push_back(std::thread([&]()
          //{
            alignas(512) Chunk c;
            {
              c.addr_ = _chunks[j].addr_;
              c.lbaHash_ = _chunks[j].lbaHash_;
              c.len_ = _chunks[j].len_;
              c.hasFingerprint_ = false;
              c.verficationResult_ = cache::VERIFICATION_UNKNOWN;
            }
            DeduplicationModule::lookup(c);
            if (c.lookupResult_ == NOT_HIT) {
              memcpy(c.fingerprint_, _chunks[j].fingerprint_, 16);
              c.fingerprintHash_ = _chunks[j].fingerprintHash_;
              c.compressedLevel_ = _chunks[j].compressedLevel_;
              c.hasFingerprint_ = true;
            } else if (c.lookupResult_ == HIT) {
              ++n_hits;
            }
            MetadataModule::getInstance().update(c);
          //}
        //));
      }
      //for (auto &t : threads) {
        //t.join();
      //}
    }
  }
 private:
  Chunk *_chunks;
  WorkloadConfiguration _workload_conf;
  int _n_chunks;
};
}

int main(int argc, char **argv)
{
  cache::RunDeduplicationModule run_dedup;
  run_dedup.parse_argument(argc, argv);
  run_dedup.warm_up();

  int elapsed = 0;
  int n_hits = 0;
  uint64_t n_total = 0;
  PERF_FUNCTION(elapsed, run_dedup.work, 50000, n_hits, n_total);

  uint64_t total_bytes = n_total * 32768;

  std::cout << (double)total_bytes / (1024 * 1024) << std::endl;
  std::cout << elapsed << " ms" << std::endl;
  std::cout << (double)n_total / elapsed * 1000.0 << " Kops/s" << std::endl;
  std::cout << (double)total_bytes / elapsed << " MBytes/s" << std::endl;
  std::cout << "hit_ratio: " << (double)n_hits / (n_total) * 100 << "%" << std::endl;
  std::cout << "total access: " << n_total << std::endl;

}
