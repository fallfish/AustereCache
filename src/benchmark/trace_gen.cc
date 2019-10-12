#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include "common/common.h" // Chunk for generating fingerprints
#include "workload_conf.h"
#include "utils/MurmurHash3.h"
#include "utils/gen_zipf.h"
#include "chunk/chunkmodule.h"
#include "compression/compressionmodule.h"

//#include <openssl/sha.h>
namespace cache {
//namespace benchmark {

class TraceGenerator {
 public:
 // Parameters that need consideration
 // 1. Workload mode: write-read ratio
 // 2. Compressibility: 0 - 25%, 25% - 50%, 50% - 75%, 75% - not-compressible, mixed (0, 1, 2, 3, 4)
 // 3. Number of unique chunks
 // 4. Total number of chunks 
 // 6. Output file name
 // 7. Distribution: random, zipf
 // 8. Zipf distribution skewness
  TraceGenerator()
  {
    {
      // set default values
      _compressibility = 0;
      _chunk_size = 32768;
      _num_unique_chunks = 8192;
      _distribution = 0; // 0 indicates uniform distribution, 1 indicates zipf distribution
      _num_chunks = 8192;
      strcpy(_output_file, "trace");
    }
  }

  ~TraceGenerator()
  {
    if (_workload_chunks != nullptr) free(_workload_chunks);
    if (_chunks != nullptr) free(_chunks);
  }

  void generate_trace()
  {
    generate_workload_data();
    _workload_conf = new WorkloadConfiguration(
        _base_chunks, _chunk_size, _num_unique_chunks, _num_chunks,
        _distribution, _skewness);

    int fd = open(_output_file, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
      std::cout << "TraceGen::generate_trace: File " << _output_file << " failed to open!" << std::endl;
      exit(1);
    }

    int n = write(fd, _workload_conf, sizeof(WorkloadConfiguration));
    if (n < 0) {
      std::cout << "Fail to write workload configuration!" << std::endl;
      exit(1);
    }
    n = write(fd, _workload_chunks, (uint64_t)_num_chunks * _chunk_size);
    if (n < 0) {
      std::cout << "Fail to write workload chunks!" << std::endl;
      exit(1);
    }
    n = write(fd, _unique_chunks, (uint64_t)_num_unique_chunks * _chunk_size);
    if (n < 0) {
      std::cout << "Fail to write unique chunks!" << std::endl;
      exit(1);
    }

    close(fd);
    std::cout << "TraceGen::generate_trace: Generate trace successfully!" << std::endl;
  }

  void generate_chunk()
  {
    CompressionModule compression_module;
    ChunkModule chunk_module;
    Chunker chunker = chunk_module.createChunker(0, _workload_chunks, _num_chunks * _chunk_size);
    uint8_t tmp_buf[_chunk_size];

    char _fingerprint_file[100];
    strcpy(_fingerprint_file, _output_file);
    strcat(_fingerprint_file, "-chunk");

    int fd = open(_fingerprint_file, O_CREAT | O_RDWR, 0666);
    _chunks = reinterpret_cast<Chunk *>(malloc(sizeof(Chunk) * _num_chunks));

    // compute lba_hash, fp_hash, compressiblity, and ca (fingerprint) for each chunk
    int index = 0;
    while (chunker.next(_chunks[index])) {
      _chunks[index].computeFingerprint();
      _chunks[index].computeLBAHash();
      _chunks[index].compressedBuf_ = tmp_buf;
      compression_module.compress(_chunks[index]);
      ++ index;
    }

    int n = write(fd, &_workload_conf, sizeof(WorkloadConfiguration));
    n = write(fd, _chunks, sizeof(Chunk) * _num_chunks);
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
      if (strcmp(param_name, "--h") == 0 || strcmp(value, "--h") == 0) {
        print_help();
        exit(0);
      }
      std::cout << param_name << std::endl;
      if (strcmp(param_name, "--chunk-size") == 0) {
        _chunk_size = atoi(value);
      } else if (strcmp(param_name, "--compressibility") == 0) {
        _compressibility = atoi(value);
      } else if (strcmp(param_name, "--num-unique-chunks") == 0) {
        _num_unique_chunks = atoi(value);
      } else if (strcmp(param_name, "--duplication-ratio") == 0) {
        _duplication_ratio = atof(value);
        _num_chunks = _num_unique_chunks * _duplication_ratio;
      } else if (strcmp(param_name, "--output-file") == 0) {
        memcpy(_output_file, value, strlen(value));
        _output_file[strlen(value)] = '\0';
      } else if (strcmp(param_name, "--distribution") == 0) {
        if (strcmp(value, "uniform") == 0) {
          _distribution = 0;
        } else if (strcmp(value, "zipf") == 0) {
          _distribution = 1;
        }
      } else if (strcmp(param_name, "--skewness") == 0) {
        _skewness = atof(value);
        std::cout << _skewness << std::endl;
      } else {
        std::cout << "invalid parameters" << std::endl;
        print_help();
        exit(-1);
      }
    }
  }

 private:
  void print_help()
  {
    std::cout << "--wr-ratio: the ratio of write chunks and read chunks" << std::endl
      << "--compressibility: the compressibility of the workload" << std::endl
      << "--duplication-factor: the level of duplicate chunks" << std::endl
      << "--working-set: the size of whole data set" << std::endl
      << "--cache-device-size: the size of cache device" << std::endl
      << "--output-file: file name of generated trace" << std::endl
      << "--distribution: trace distribution (zipf or uniform)" << std::endl;
  }

  void print_current_parameters()
  {
  }

  void generate_workload_data()
  {
    // Generate base chunks of different compressibility
    srand(0);
    for (int i = 0; i < 4; ++i) {
      _base_chunks[i] = reinterpret_cast<char*>(malloc(_chunk_size));
    }
    for (int i = 0; i < _chunk_size; ++i) {
      _base_chunks[0][i]= (i & (0xff - 7));
      _base_chunks[1][i]= (i & (0xff - i / 256 ));
      _base_chunks[2][i]= (i & (0xff - i * i / 256 ));
      _base_chunks[3][i]= (i & (0xff - rand() / 256 ));
    }

    // Generate unique chunks
    _unique_chunks = reinterpret_cast<char*>(malloc((uint64_t)(_num_unique_chunks) * _chunk_size));
    int comp = 0;
    for (uint64_t addr = 0; addr < (uint64_t)(_num_unique_chunks) * _chunk_size; addr += _chunk_size) {
      if (_compressibility == 4) {
        comp += 1;
        if (comp == 4) comp = 0;
        memcpy(_unique_chunks + addr, _base_chunks[comp], _chunk_size);
      } else {
        memcpy(_unique_chunks + addr, _base_chunks[_compressibility], _chunk_size);
      }
      uint32_t modify_index = rand() % _chunk_size;
      uint32_t modify_byte = rand() % 0xff;
      (_unique_chunks + addr)[modify_index] = modify_byte;
    }

    _workload_chunks = reinterpret_cast<char*>(malloc((uint64_t)(_num_chunks) * _chunk_size));
    if (_distribution == 1) {
      // zipf distribution
      genzipf::rand_val(1);
      for (uint64_t addr = 0; addr < (uint64_t)(_num_chunks) * _chunk_size; addr += _chunk_size) {
        int rd = genzipf::zipf(_skewness, _num_unique_chunks);
        memcpy(_workload_chunks + addr, _unique_chunks + rd * _chunk_size, _chunk_size);
      }
    } else if (_distribution == 0) {
      // uniform distribution
      srand(1);
      for (uint64_t addr = 0; addr < _num_chunks * _chunk_size; addr += _chunk_size) {
        int rd = rand() % _num_unique_chunks;
        memcpy(_workload_chunks + addr, _unique_chunks + rd * _chunk_size, _chunk_size);
      }
    }
  }

  // configuration for running the benchmark
  WorkloadConfiguration *_workload_conf;
  // parameters
  uint32_t _chunk_size;
  uint32_t _num_unique_chunks;
  float    _duplication_ratio;
  uint32_t _num_chunks;
  uint32_t _distribution;
  uint32_t _compressibility;
  float _skewness;

  // store chunk data
  char *_base_chunks[4];
  char *_unique_chunks;
  char *_workload_chunks;
  Chunk *_chunks;

  char _output_file[100];
};
}

int main(int argc, char **argv)
{
  cache::TraceGenerator trace_generator;
  trace_generator.parse_config(argc, argv);
  trace_generator.generate_trace();
  trace_generator.generate_chunk();

  return 0;
}
