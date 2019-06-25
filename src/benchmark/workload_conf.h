#include <cstdint>
#include <cstring>
namespace cache {
  struct WorkloadConfiguration {
    char _buf[4][32768];
    uint32_t _chunk_size;
    double _wr_ratio;
    int _compressibility;
    int _duplication_factor;
    int _working_set_size;
    int _cache_device_size;
    int _num_unique_chunks;
    int _fp_alg;

    WorkloadConfiguration() {}
    WorkloadConfiguration(
        char **buf,
        uint32_t chunk_size,
        double wr_ratio,
        int compressibility,
        int duplication_factor,
        int working_set_size,
        int cache_device_size,
        int fp_alg) :
      _chunk_size(chunk_size),
      _wr_ratio(wr_ratio),
      _compressibility(compressibility),
      _duplication_factor(duplication_factor),
      _working_set_size(working_set_size),
      _cache_device_size(cache_device_size),
      _fp_alg(fp_alg)
    {
      for (int i = 0; i < 4; i++) {
        memset(_buf[i], 0, 32768);
        memcpy(_buf[i], buf[i], _chunk_size);
      }
      _num_unique_chunks = 1.0 * _working_set_size / _chunk_size / duplication_factor;
      if (_num_unique_chunks == 0) _num_unique_chunks = 1;
    }


    void print_current_parameters()
    {
      std::cout << "--wr-ratio: " << _wr_ratio << std::endl
        << "--compressibility: " << _compressibility << std::endl
        << "--duplication-factor: " << _duplication_factor << std::endl
        << "--working-set-size: " << _working_set_size << std::endl
        << "--cache-device-size: " << _cache_device_size << std::endl;
      if (_fp_alg == 0) {
        std::cout << "--fp-algorithm: murmurhash3" << std::endl;
      }
    }
  };
}
