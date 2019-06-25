#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"
#include "ssddup/ssddup.h"
#include "workload_conf.h"

namespace cache {

class RunSystem {
 public:
  RunSystem() {}
  ~RunSystem() {
    free(_original_data);
    free(_read_data);
  }

  void parse_argument(int argc, char **argv)
  {
    char *param, *value;
    double wr_ratio = 0;
    for (int i = 1; i < argc; i += 2) {
      param = argv[i];
      value = argv[i + 1];
      if (value == nullptr) {
        std::cout << "RunSystem::parse_argument invalid argument!" << std::endl;
        print_help();
        exit(1);
      }
      if (strcmp(param, "--trace") == 0) {
        // read workload configuration
        int fd = open(value, O_RDONLY);
        int n = read(fd, &_workload_conf, sizeof(cache::WorkloadConfiguration));
        if (n != sizeof(cache::WorkloadConfiguration)) {
          std::cout << "RunSystem:parse_argument read configuration failed!" << std::endl;
          exit(1);
        }

        // read working set
#ifdef __APPLE__
        posix_memalign((void**)&_original_data, 512, _workload_conf._working_set_size);
        posix_memalign((void**)&_read_data, 512, _workload_conf._working_set_size);
#else
        _original_data = (char *)aligned_alloc(512, _workload_conf._working_set_size);
        _read_data = (char *)aligned_alloc(512, _workload_conf._working_set_size);
#endif
        n = read(fd, _original_data, _workload_conf._working_set_size);
        if (n != _workload_conf._working_set_size) {
          std::cout << "RunSystem: read working set failed!" << std::endl; 
          exit(1);
        }
      } else if (strcmp(param, "--wr-ratio") == 0) {
        wr_ratio = atof(value);
      }
    }

    if (_original_data == nullptr) {
      std::cout << "RunSystem::parse_argument no input file given!" << std::endl;
      print_help();
      exit(1);
    }
    _workload_conf._wr_ratio = wr_ratio;
    _workload_conf.print_current_parameters();
  }

  void warm_up()
  {
    _ssddup.write(0, _original_data, _workload_conf._working_set_size);
  }

  void work(int n_requests, uint64_t &total_bytes)
  {
    for (uint32_t i = 0; i < n_requests; i++) {
      int _n_chunks = _workload_conf._working_set_size / _workload_conf._chunk_size;
      uint64_t begin = rand() % _n_chunks;
      uint64_t end = begin + rand() % 3;
      if (end >= _n_chunks) end = _n_chunks - 1;


      begin = begin * _workload_conf._chunk_size;
      end = end * _workload_conf._chunk_size;


      //uint64_t begin = rand() % _workload_conf._working_set_size;
      //uint64_t end = begin + rand() % (_workload_conf._chunk_size * 3);
      //if (end >= _workload_conf._working_set_size) end = _workload_conf._working_set_size - 1;
      //if (begin == end) continue;

      int op = 1;
      if (op == 0) {
        modify_chunk(begin, end);
        _ssddup.write(begin, _original_data + begin, end - begin);
      } else {
        total_bytes += end - begin;
        _ssddup.read(begin, _read_data + begin, end - begin);
        if (compare_array(_original_data + begin, _read_data + begin, end - begin) == false) {
          std::cout << "RunSystem:work content not match for address: "
            << begin << ", length: "
            << end - begin << std::endl;
        }
      }
    }
  }

 private: 
  bool compare_array(void *a, void *b, uint32_t length)
  {
    for (uint32_t i = 0; i < length; i++) {
     if (((char*)a)[i] != ((char*)b)[i]) return false;
    }
    return true;
  }

  void print_help()
  {
    std::cout << "--wr-ratio: the ratio of write chunks and read chunks" << std::endl
      << "--trace: input workload file" << std::endl;
  }

  void modify_chunk(uint64_t begin, uint64_t end)
  {
    uint32_t chunk_size = _workload_conf._chunk_size;
    int compressibility = _workload_conf._compressibility;
    int num_unique_chunks = _workload_conf._num_unique_chunks;
    begin = begin / chunk_size;
    end = end / chunk_size;
    int rd = 0;
    for (int i = begin; i <= end; i++) {
      uint64_t addr = i * chunk_size;
      if (compressibility == 4) {
        rd = rand() % 4;

        if (num_unique_chunks == -1) {
          rd = rand() % chunk_size;
          (_original_data + addr)[rd] = rand() % 0xff;
        } else {
          if (num_unique_chunks >= 4) {
            rd = rand() % (num_unique_chunks / 4);
            if ((_original_data + addr)[rd] == 0x1) (_original_data + addr)[rd] = 0x0;
            else (_original_data + addr)[rd] = 0x1;
          }
        }
      } else {
        if (num_unique_chunks == -1) {
          rd = rand() % chunk_size;
          (_original_data + addr)[rd] = rand() % 0xff;
        } else {
          rd = rand() % (num_unique_chunks);
          if ((_original_data + addr)[rd] == 0x1) (_original_data + addr)[rd] = 0x0;
          else (_original_data + addr)[rd] = 0x1;
        }
      }
    }
  }

  char *_original_data, *_read_data;
  WorkloadConfiguration _workload_conf; 
  SSDDup _ssddup;
};

}
int main(int argc, char **argv)
{
  srand(0);
  cache::RunSystem run_system;
  run_system.parse_argument(argc, argv);
  run_system.warm_up();

  uint64_t total_bytes = 0;
  int elapsed = 0;
  PERF_FUNCTION(elapsed, run_system.work, 5000, total_bytes);
  std::cout << (double)total_bytes / (1024 * 1024) << std::endl;
  std::cout << elapsed << " ms" << std::endl;
  std::cout << (double)total_bytes / elapsed << " MBytes/s" << std::endl;
}

