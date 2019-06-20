#include <iostream>
#include "compressionmodule.h"
#include "utils/utils.h"

char src[32768 * 1024 * 32];
char dst[32768 * 1024 * 32];

void work()
{
  srand(0);
  cache::CompressionModule compression_module;

  int test = 1024 * 32;
  for (int i = 0; i < test; i++) {
    //std::cout << i << std::endl;
    compression_module.compress_TEST(src + i * 32768, dst + i * 32768, 32768);
  }
}
int main()
{
  for (int i = 0; i < 32768 * 1024 * 32; i++) {
    src[i] = rand();
  }
  long elapsed = 0;
  PERF_FUNCTION(work, elapsed);
  std::cout << elapsed / 1000.0 << " ms" << std::endl;
}
