#include <cstdlib>
#include <iostream>
#include <cstring>
namespace cache {
//namespace benchmark {

class TraceGenerator {
 public:
 // Parameters that need consideration
 // 1. Workload mode: write-read ratio
 // 2. Compressibility: 0 - 25%, 25% - 50%, 50% - 75%, 75% - not-compressible, mixed
 // 3. Duplication: all zero (all duplicate), all random (not duplicate), mixed
 // 4. Working set: KiB, MiB, GiB
 // 5. Cache device size: KiB, MiB, GiB
  TraceGenerator() {
    {
      // set default values
      _wr_ratio = 0;
      _compressiblity = 0;
      _duplication_ratio = 0;
      _working_set = 1024 * 1024; // 1MiB
      _cache_device_size = 1024 * 32;
    }
    _chunk_size = 32768;
    alloc_aligned_memory((void**)&_buf[0], 512, _chunk_size);
    alloc_aligned_memory((void**)&_buf[1], 512, _chunk_size);
    alloc_aligned_memory((void**)&_buf[2], 512, _chunk_size);
    alloc_aligned_memory((void**)&_buf[3], 512, _chunk_size);
    int seed = 0;
    srand(0);
    for (int i = 0; i < _chunk_size; i++) {
      _buf[0][i]= ((i + seed) & (0xff - 7));
      _buf[1][i]= ((i + seed) & (0xff - i / 256 ));
      _buf[2][i]= ((i + seed) & (0xff - i * i / 256 ));
      _buf[3][i]= ((i + seed) & (0xff - rand() / 256 ));
    }
  }

  void generate_trace()
  {
    FILE *f = fopen(_output_file, "w");
    
  }

  void parse_config(int argc, char **argv)
  {
    char *param_name;
    for (int i = 1; i < argc; i += 2) {
      char *param_name = argv[i];
      char *value = argv[i + 1];
      if (value == nullptr) {
        std::cout << "invalid parameters" << std::endl;
        print_help();
        exit(-1);
      }
      if (strcmp(value, "-h") == 0) {
        print_help();
        exit(0);
      }
      if (strcmp(param_name, "--wr-ratio") == 0) {
        _wr_ratio = atof(value);
      } else if (strcmp(param_name, "--compressbility") == 0) {
        // 0, 1, 2, 3, (from non-compressible to high-compressible)
        // 4 (mixed)
        _compressiblity = atoi(value);
      } else if (strcmp(param_name, "--duplication-ratio") == 0) {
        // 0 means no-duplicate chunks, -1 means all zero
        _duplication_ratio = atoi(value);
      } else if (strcmp(param_name, "--working-set") == 0) {
        int len = strlen(value);
        char tmp_str[len];
        memcpy(tmp_str, value, len - 1);
        tmp_str[len - 1] = '\0';
        double tmp = 1.0;
        switch (value[len-1]) {
          case 'G':
            tmp *= 1024;
          case 'M':
            tmp *= 1024;
          case 'K':
            tmp *= 1024;
        }
        _working_set = (uint64_t) (tmp * atof(tmp_str));
        std::cout << _working_set << std::endl;
      } else if (strcmp(param_name, "--cache-device-size") == 0) {
        int len = strlen(value);
        char tmp_str[len];
        memcpy(tmp_str, value, len - 1);
        tmp_str[len - 1] = '\0';
        double tmp = 1.0;
        switch (value[len-1]) {
          case 'G':
            tmp *= 1024;
          case 'M':
            tmp *= 1024;
          case 'K':
            tmp *= 1024;
        }
        _cache_device_size = (uint64_t) (tmp * atof(tmp_str));
      } else if (strcmp(param_name, "--output-file") == 0) {
        memcpy(_output_file, value, strlen(value));
        _output_file[strlen(value) - 1] = '\0';
      } else {
        std::cout << "invalid parameters" << std::endl;
        print_help();
        exit(-1);
      }
    }
  }

  void print_help()
  {
    std::cout << "--wr-ratio: the ratio of write chunks and read chunks" << std::endl
      << "--compressiblity: the compressiblity of the workload" << std::endl
      << "--duplication-ratio: the level of duplicate chunks" << std::endl
      << "--working-set: the size of whole data set" << std::endl
      << "--cache-device-size: the size of cache device" << std::endl;
  }

  ~TraceGenerator() {}

 private:
  int alloc_aligned_memory(void **buf, size_t align, size_t len)
  {
    int ret = posix_memalign(buf, align, len);
    if (ret < 0) {
      std::cout << "TraceGenerator::alloc_aligned_memory " 
        << strerror(errno) << std::endl;
    }
    return ret;
  }
  uint32_t _chunk_size;
  char *_buf[4];
  double _wr_ratio;
  int _compressiblity;
  int _duplication_ratio;
  int _working_set;
  int _cache_device_size;
  char _output_file[100];
};

//}
}

int main(int argc, char **argv)
{
  cache::TraceGenerator trace_generator;
  trace_generator.parse_config(argc, argv);

  return 0;
}
