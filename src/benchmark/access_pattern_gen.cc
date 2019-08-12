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

class AccessPatternGenerator {
 public:
 // Parameters that need consideration
 // 1. Number of unique chunks
 // 2. Total number of chunks 
 // 3. Output file name
 // 4. Distribution: random, zipf
 // 5. Zipf distribution skewness
  AccessPatternGenerator()
  {
    {
      // set default values
      _num_chunks = 8192;
      _num_requests = 8192;
      _distribution = 0; // 0 indicates uniform distribution, 1 indicates zipf distribution
      strcpy(_output_file, "access_pattern_uniform_8192_8192");
    }
  }

  ~AccessPatternGenerator()
  {
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
      if (strcmp(param_name, "--num-chunks") == 0) {
        _num_chunks = atoi(value);
      } else if (strcmp(param_name, "--num-requests") == 0) {
        _num_requests = atoi(value);
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
      } else {
        std::cout << "invalid parameters" << std::endl;
        print_help();
        exit(-1);
      }
    }
  }

  void generate()
  {
    // Generate access pattern
    srand(100);
    genzipf::rand_val(100);
    FILE *f = fopen(_output_file, "w");
      // zipf distribution
    for (int i = 0; i < _num_requests; ++i) {
      int rd = rand() % 100;
      int chunk_id, len;
      if (_distribution == 1) {
        chunk_id = genzipf::zipf(_skewness, _num_chunks);
      } else {
        chunk_id = rand() % _num_chunks;
      }
      len = rand() % 3 + 1;
      if (len + chunk_id > _num_chunks) {
        len = _num_chunks - chunk_id;
      }
      fprintf(f, "%d %d\n", chunk_id, len);
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

  // parameters
  uint32_t _chunk_size;
  uint32_t _num_chunks;
  uint32_t _num_requests;
  uint32_t _distribution;
  float _skewness;

  char _output_file[100];
};
}

int main(int argc, char **argv)
{
  cache::AccessPatternGenerator access_pattern_generator;
  access_pattern_generator.parse_config(argc, argv);
  access_pattern_generator.generate();

  return 0;
}
