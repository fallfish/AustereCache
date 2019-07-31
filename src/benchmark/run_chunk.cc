#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"
#include "ssddup/ssddup.h"
#include "workload_conf.h"

#include <vector>
#include <thread>
#include <atomic>

namespace cache {

class RunChunkModule {
 public:
  RunChunkModule() {
    _multi_thread = 0;
    _num_workers = 1;
  }
  ~RunChunkModule() {
    free(_original_data);
  }

  void parse_argument(int argc, char **argv)
  {
    char *param, *value;
    double wr_ratio = 0;
    for (int i = 1; i < argc; i += 2) {
      param = argv[i];
      value = argv[i + 1];
      if (value == nullptr) {
        std::cout << "RunChunkModule::parse_argument invalid argument!" << std::endl;
        print_help();
        exit(1);
      }
      if (strcmp(param, "--trace") == 0) {
        // read workload configuration
        int fd = open(value, O_RDONLY);
        int n = read(fd, &_workload_conf, sizeof(cache::WorkloadConfiguration));
        if (n != sizeof(cache::WorkloadConfiguration)) {
          std::cout << "RunChunkModule:parse_argument read configuration failed!" << std::endl;
          exit(1);
        }

        // read working set
#ifdef DIRECT_IO
        _original_data = (char *)aligned_alloc(512, _workload_conf._working_set_size);
#else
        _original_data = (char *)malloc(_workload_conf._working_set_size);
#endif
        n = read(fd, _original_data, _workload_conf._working_set_size);
        if (n != _workload_conf._working_set_size) {
          std::cout << "RunChunkModule: read working set failed!" << std::endl; 
          exit(1);
        }
      } else if (strcmp(param, "--wr-ratio") == 0) {
        wr_ratio = atof(value);
      } else if (strcmp(param, "--ca-bits") == 0) {
        Config::get_configuration().set_ca_bucket_no_len(atoi(value));
      } else if (strcmp(param, "--multi-thread") == 0) {
        _multi_thread = atoi(value);
      } else if (strcmp(param, "--num-workers") == 0) {
        _num_workers = atoi(value);
      }
    }

    if (_original_data == nullptr) {
      std::cout << "RunChunkModule::parse_argument no input file given!" << std::endl;
      print_help();
      exit(1);
    }
    _workload_conf._wr_ratio = wr_ratio;
    _workload_conf.print_current_parameters();
    _chunk_module = std::make_unique<ChunkModule>();
  }

  void warm_up()
  {
  }

  void work()
  {
    Config::get_configuration().set_fingerprint_algorithm(1);
    Chunker chunker = _chunk_module->create_chunker(0, _original_data, _workload_conf._working_set_size);
    Chunk chunk;
    while (chunker.next(chunk)) {
      chunk.fingerprinting();
    }
  }

 private: 
  void print_help()
  {
    std::cout << "--wr-ratio: the ratio of write chunks and read chunks" << std::endl
      << "--trace: input workload file" << std::endl;
  }

  char *_original_data;
  int _multi_thread;
  std::unique_ptr<ChunkModule> _chunk_module;

 public:
  WorkloadConfiguration _workload_conf; 
  int _num_workers;
};

}
int main(int argc, char **argv)
{
  srand(0);
  cache::RunChunkModule run_chunkmodule;
  run_chunkmodule.parse_argument(argc, argv);

  std::atomic<uint64_t> total_bytes(run_chunkmodule._workload_conf._working_set_size);
  int elapsed = 0;
  PERF_FUNCTION(elapsed, run_chunkmodule.work);
  std::cout << (double)total_bytes / (1024 * 1024) << std::endl;
  std::cout << elapsed << " ms" << std::endl;
  std::cout << "Throughput: " << (double)total_bytes / elapsed << " MBytes/s" << std::endl;
}

