#ifndef __COMPRESSION_MODULE_TEST_H__
#define __COMPRESSION_MODULE_TEST_H__

#include "compressionmodule.h"
TEST(CompressionModule, Compression)
{
  srand(0);
  char src[32768];
  char dst[40000];
  cache::CompressionModule compression_module;

  int test = 100;
  while (test--) {
    for (int i = 0; i < 32768; i++) {
      src[i] = 'A';
    }
    compression_module.compress_TEST(src, dst, 32768);
  }
 
}

#endif
