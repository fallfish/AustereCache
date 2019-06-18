#include <iostream>
#include "compressionmodule.h"
#include "utils/utils.h"

  char src[32768 * 1024 * 32];
  char dst[40000];
void work()
{
  srand(0);
  cache::CompressionModule compression_module;
    for (int i = 0; i < 32768 * 1024 * 32; i++) {
      src[i] = rand();
    }

  int test = 1024 * 32;
  for (int i = 0; i < test; i++) {
    compression_module.compress_TEST(src + test * 32768, dst, 32768);
  }
}
int main()
{
  long elapsed = 0;
  PERF_FUNCTION(work, elapsed);
  std::cout << elapsed / 1000.0 << " ms" << std::endl;
}
