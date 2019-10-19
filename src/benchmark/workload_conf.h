#include <cstdint>
#include <cstring>
namespace cache {
  struct WorkloadConfiguration {
    char _buf[4][32768]{};
    uint64_t _working_set_size{};
    uint32_t _chunk_size{};
    uint32_t _num_unique_chunks{};
    float    _duplication_ratios{};
    uint32_t _num_chunks{};
    uint32_t _distribution{};
    float _skewness{};

    WorkloadConfiguration() = default;
    WorkloadConfiguration(
        char **buf,
        uint32_t chunk_size,
        uint32_t num_unique_chunks,
        uint32_t num_chunks,
        uint32_t distribution,
        float skewness) :
      _chunk_size(chunk_size),
      _num_unique_chunks(num_unique_chunks),
      _num_chunks(num_chunks),
      _distribution(distribution),
      _skewness(skewness),
      _working_set_size(num_chunks * chunk_size)
    {
      for (int i = 0; i < 4; i++) {
        memset(_buf[i], 0, 32768);
        memcpy(_buf[i], buf[i], _chunk_size);
      }
    }


    void print_current_parameters()
    {
      std::cout << "Working set size: " << _working_set_size << std::endl;
      if (_distribution == 0) {
        std::cout << "Distribution: uniform" << std::endl
          << "Duplication ratio: " << _num_chunks / _num_unique_chunks << std::endl;
      } else {
        std::cout << "Distribution: zipf" << std::endl
          << "Skewness: " << _skewness << std::endl;
      }
    }
  };
}
