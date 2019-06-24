#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include "common/common.h" // Chunk for generating fingerprints
#include "workload_conf.h"
#include "utils/MurmurHash3.h"
//#include <openssl/sha.h>
namespace cache {
//namespace benchmark {

class TraceGenerator {
 public:
 // Parameters that need consideration
 // 1. Workload mode: write-read ratio
 // 2. Compressibility: 0 - 25%, 25% - 50%, 50% - 75%, 75% - not-compressible, mixed
 // 3. Duplication factor: average occurrences of all chunks, 1 means all unique, number of total chunks means all same, number of unique chunks can be computed as (working_set_size / chunk_size / duplication_factor)
 // 4. Working set: KiB, MiB, GiB
 // 5. Cache device size: KiB, MiB, GiB
 // 6. Output file name
 // 7. Distribution: random
  TraceGenerator() {
    {
      // set default values
      _wr_ratio = 0;
      _compressibility = 0;
      _duplication_factor = 1;
      _working_set_size = 1024 * 1024; // 1MiB
      _cache_device_size = 1024 * 32;
      strcpy(_output_file, "trace");
      _num_unique_chunks = 1.0 * _working_set_size / _chunk_size;
      _fp_alg = 0;
    }
    _chunk_size = 32768;
    alloc_aligned_memory((void**)&_base_chunks[0], 512, _chunk_size);
    alloc_aligned_memory((void**)&_base_chunks[1], 512, _chunk_size);
    alloc_aligned_memory((void**)&_base_chunks[2], 512, _chunk_size);
    alloc_aligned_memory((void**)&_base_chunks[3], 512, _chunk_size);
    int seed = 0;
    srand(0);
    for (int i = 0; i < _chunk_size; i++) {
      _base_chunks[0][i]= ((i + seed) & (0xff - 7));
      _base_chunks[1][i]= ((i + seed) & (0xff - i / 256 ));
      _base_chunks[2][i]= ((i + seed) & (0xff - i * i / 256 ));
      _base_chunks[3][i]= ((i + seed) & (0xff - rand() / 256 ));
    }
    new (&_workload_conf) WorkloadConfiguration(
        _base_chunks, _chunk_size, _wr_ratio, _compressibility,
        _duplication_factor, _working_set_size, _cache_device_size,
        _fp_alg);
  }

  ~TraceGenerator()
  {
    if (_workload_data != nullptr) free(_workload_data);
    if (_chunks != nullptr) free(_chunks);
  }

  void generate_trace()
  {
    int fd = open(_output_file, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
      std::cout << "TraceGen::generate_trace: File " << _output_file << " failed to open!" << std::endl;
      exit(1);
    }

    int n = write(fd, &_workload_conf, sizeof(WorkloadConfiguration));
    n = write(fd, _workload_data, _working_set_size);

    close(fd);
    std::cout << "TraceGen::generate_trace: Generate trace successfully!" << std::endl;
  }

  void generate_chunk()
  {
    char _fingerprint_file[100];
    strcpy(_fingerprint_file, _output_file);
    strcat(_fingerprint_file, "_chunk");
    int fd = open(_fingerprint_file, O_CREAT | O_RDWR, 0666);
    _chunks = reinterpret_cast<Chunk *>(malloc(sizeof(Chunk) * _working_set_size / _chunk_size));
    // compute lba_hash, ca_hash, and ca (fingerprint) for each chunk
    for (uint64_t addr = 0; addr < _working_set_size; addr += _chunk_size) {
      int index = addr / _chunk_size;
      if (_fp_alg == 0) {
        MurmurHash3_x64_128(_workload_data + addr, _chunk_size, 0, _chunks[index]._ca);
        MurmurHash3_x86_32(&addr, 8, 1, &_chunks[index]._lba_hash);
        MurmurHash3_x86_32(_chunks[index]._ca, 16, 2, &_chunks[index]._ca_hash);
      } else {
        //SHA1(reinterpret_cast<const unsigned char*>(_workload_data + addr), _chunk_size, _chunks[index]._ca);
        //MurmurHash3_x86_32(&addr, 8, 1, &_chunks[index]._lba_hash);
        //MurmurHash3_x86_32(_chunks[index]._ca, 20, 2, &_chunks[index]._ca_hash);
      }
    }
    int n = write(fd, &_workload_conf, sizeof(WorkloadConfiguration));
    n = write(fd, _chunks, sizeof(Chunk) * _working_set_size / _chunk_size);
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
      if (strcmp(value, "--h") == 0) {
        print_help();
        exit(0);
      }
      if (strcmp(param_name, "--wr-ratio") == 0) {
        _wr_ratio = atof(value);
      } else if (strcmp(param_name, "--compressibility") == 0) {
        // 0, 1, 2, 3, (from non-compressible to high-compressible)
        // 4 (mixed)
        _compressibility = atoi(value);
      } else if (strcmp(param_name, "--duplication-factor") == 0) {
        // 0 means no-duplicate chunks, -1 means all zero
        _duplication_factor = atoi(value);
      } else if (strcmp(param_name, "--working-set-size") == 0) {
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
        _working_set_size = (uint64_t) (tmp * atof(tmp_str));
        std::cout << _working_set_size << std::endl;
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
        _output_file[strlen(value)] = '\0';
      } else if (strcmp(param_name, "--fp-alg") == 0) {
        if (strcmp(value, "murmurhash3") == 0) {
          _fp_alg = 0;
        } else if (strcmp(value, "sha1") == 0) {
          _fp_alg = 1;
        }
      } else {
        std::cout << "invalid parameters" << std::endl;
        print_help();
        exit(-1);
      }
      _num_unique_chunks = 1.0 * _working_set_size / _chunk_size / _duplication_factor;
      if (_num_unique_chunks == 0) _num_unique_chunks = 1;
    }

    new (&_workload_conf) WorkloadConfiguration(
        _base_chunks, _chunk_size, _wr_ratio, _compressibility,
        _duplication_factor, _working_set_size, _cache_device_size,
        _fp_alg);
    print_current_parameters();
    generate_workload_data();
  }

 private:
  void print_help()
  {
    std::cout << "--wr-ratio: the ratio of write chunks and read chunks" << std::endl
      << "--compressibility: the compressibility of the workload" << std::endl
      << "--duplication-factor: the level of duplicate chunks" << std::endl
      << "--working-set: the size of whole data set" << std::endl
      << "--cache-device-size: the size of cache device" << std::endl
      << "--output-file: file name of generated trace" << std::endl;
  }

  void print_current_parameters()
  {
    std::cout << "--wr-ratio: " << _wr_ratio << std::endl
      << "--compressibility: " << _compressibility << std::endl
      << "--duplication-factor: " << _duplication_factor << std::endl
      << "--working-set-size: " << _working_set_size << std::endl
      << "--cache-device-size: " << _cache_device_size << std::endl
      << "--output-file: " << _output_file << std::endl;
    if (_fp_alg == 0) {
      std::cout << "--fp-algorithm: murmurhash3" << std::endl;
    }
  }

  void generate_workload_data()
  {
    _workload_data = reinterpret_cast<char*>(malloc(_working_set_size));
    for (uint64_t addr = 0; addr < _working_set_size; addr += _chunk_size) {
      int rd;
      if (_compressibility == 4) {
        rd = rand() % 4;
        memcpy(_workload_data + addr, _base_chunks[rd], _chunk_size);

        if (_num_unique_chunks == -1) {
          rd = rand() % _chunk_size;
          (_workload_data + addr)[rd] = rand() % 0xff;
        } else {
          if (_num_unique_chunks >= 4) {
            rd = rand() % (_num_unique_chunks / 4);
            if ((_workload_data + addr)[rd] == 0x1) (_workload_data + addr)[rd] = 0x0;
            else (_workload_data + addr)[rd] = 0x1;
          }
        }
      } else {
        memcpy(_workload_data + addr, _base_chunks[_compressibility], _chunk_size);
        if (_num_unique_chunks == -1) {
          rd = rand() % _chunk_size;
          (_workload_data + addr)[rd] = rand() % 0xff;
        } else {
          rd = rand() % (_num_unique_chunks);
          if ((_workload_data + addr)[rd] == 0x1) (_workload_data + addr)[rd] = 0x0;
          else (_workload_data + addr)[rd] = 0x1;
        }
      }
    }
  }

  int alloc_aligned_memory(void **buf, size_t align, size_t len)
  {
    int ret = posix_memalign(buf, align, len);
    if (ret < 0) {
      std::cout << "TraceGenerator::alloc_aligned_memory " 
        << strerror(errno) << std::endl;
    }
    return ret;
  }
  WorkloadConfiguration _workload_conf;
  uint32_t _chunk_size;
  char *_base_chunks[4];
  double _wr_ratio;
  int _compressibility;
  int _duplication_factor;
  int _num_unique_chunks;
  int _working_set_size;
  int _cache_device_size;
  int _fp_alg;
  char _output_file[100];
  char *_workload_data;
  Chunk *_chunks;
};

//}
}

int main(int argc, char **argv)
{
  cache::TraceGenerator trace_generator;
  trace_generator.parse_config(argc, argv);
  trace_generator.generate_trace();
  trace_generator.generate_chunk();

  return 0;
}
