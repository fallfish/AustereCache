#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include "utils/utils.h"
#include "compression/compressionmodule.h"
#include "workload_conf.h"

namespace cache {

class RunCompressionModule {
 public:
  RunCompressionModule()
  {
    _compression_module = std::make_unique<CompressionModule>();
    _chunk_module = std::make_unique<ChunkModule>();
  }

  void parse_argument(int argc, char **argv)
  {
    char *param, *value;
    double wr_ratio = 0;
    for (int i = 1; i < argc; i += 2) {
      param = argv[i];
      value = argv[i + 1];
      if (value == nullptr) {
        std::cout << "RunCompressionModule::parse_argument invalid argument!" << std::endl;
        //print_help();
        exit(1);
      }
      if (strcmp(param, "--trace") == 0) {
        // read workload configuration
        int fd = open(value, O_RDONLY);
        int n = read(fd, &_workload_conf, sizeof(cache::WorkloadConfiguration));
        if (n != sizeof(cache::WorkloadConfiguration)) {
          std::cout << "RunCompressionModule:parse_argument read configuration failed!" << std::endl;
          exit(1);
        }

        // read working set
#ifdef DIRECT_IO
        _original_data = reinterpret_cast<uint8_t*>(aligned_alloc(512, _workload_conf._working_set_size));
        _compressed_data = reinterpret_cast<uint8_t*>(aligned_alloc(512, _workload_conf._working_set_size));
#else
        _original_data = reinterpret_cast<uint8_t*>(malloc(_workload_conf._working_set_size));
        _compressed_data = reinterpret_cast<uint8_t*>(malloc(_workload_conf._working_set_size));
#endif

        n = read(fd, _original_data, _workload_conf._working_set_size);

        if (n != _workload_conf._working_set_size) {
          std::cout << "RunCompressionModule: read working set failed!" << std::endl; 
          exit(1);
        }
      }
    }
  }

  void warm_up()
  {
    Chunker chunker = _chunk_module->createChunker(
      0, _original_data, _workload_conf._working_set_size);

    _n_chunks = _workload_conf._working_set_size / _workload_conf._chunk_size;
    _chunks = reinterpret_cast<Chunk*>(malloc(sizeof(Chunk) * _n_chunks));

    int index = 0;
    while (chunker.next(_chunks[index])) {
      _chunks[index].compressedBuf_ =
          _compressed_data + 
          index * _workload_conf._chunk_size;
      ++index;
    }
  }

  void compress()
  {
    for (int i = 0; i < _n_chunks; i++) {
      _compression_module->compress(_chunks[i]);
    }
  }

  void decompress()
  {
    for (int i = 0; i < _n_chunks; i++) {
      if (_chunks[i].compressedLen_ != 0)
        _compression_module->decompress(_chunks[i]);
    }
  }

  uint64_t get_working_set_size() {
    return _workload_conf._working_set_size;
  }
 private:
  Chunk *_chunks;
  std::unique_ptr<CompressionModule> _compression_module;
  std::unique_ptr<ChunkModule> _chunk_module;
  WorkloadConfiguration _workload_conf;
  uint8_t *_original_data;
  uint8_t *_compressed_data;
  int _n_chunks;
};
}

int main(int argc, char **argv)
{
  cache::RunCompressionModule run_compression;
  run_compression.parse_argument(argc, argv);
  run_compression.warm_up();

  int elapsed = 0;
  uint64_t total_bytes = run_compression.get_working_set_size();

  PERF_FUNCTION(elapsed, run_compression.compress);
  std::cout << "Compress rate: " << (double)total_bytes / elapsed << " MBytes/s" << std::endl;

  elapsed = 0;
  PERF_FUNCTION(elapsed, run_compression.decompress);
  std::cout << "Decompress rate: " << (double)total_bytes / elapsed << " MBytes/s" << std::endl;
}
