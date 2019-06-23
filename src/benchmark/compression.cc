#include <iostream>
#include <fstream>
#include <map>
#include "compression/compressionmodule.h"
#include "utils/utils.h"

void work(char *src, char *dst, uint64_t size, uint32_t chunk_size, std::map<int, int> &mp)
{
  int compressibility = 0;
  srand(0);
  cache::CompressionModule compression_module;

  for (uint64_t offset = 0; offset < size; offset += chunk_size) {
    //std::cout << i << std::endl;
    compression_module.compress_TEST(src + offset, dst + offset, chunk_size, compressibility);
    mp[compressibility]++;
  }
}

int main(int argc, char **argv)
{
  uint32_t chunk_size = 32768;
  std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
  uint64_t size = file.tellg();
  file.seekg(0, std::ios::beg);
  char *src = (char *)aligned_alloc(512, size);
  char *dst = (char *)aligned_alloc(512, size);

  if (file.read(src, size))
  {
    /* worked! */
  }
  std::map<int, int> mp; mp.clear();
  long elapsed = 0;
  PERF_FUNCTION(elapsed, work, src, dst, size, chunk_size, mp);
  std::cout << elapsed / 1000.0 << " ms" << std::endl;
  for (auto p : mp) {
    std::cout << "compressibility: " << p.first << " count: " << p.second << std::endl;
  }
}
