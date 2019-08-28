#include "lz4.h"
#include <vector>
#include <cstring>
#include <random>
#include <climits>
#include <algorithm>
#include <functional>

using random_bytes_engine = std::independent_bits_engine<
    std::default_random_engine, CHAR_BIT, unsigned char>;

int main()
{
  random_bytes_engine rbe;
  char data[32768], comp_data[32768]; 
  memset(data, 1, sizeof(data));
  memset(comp_data, 1, sizeof(comp_data));
//  std::vector<unsigned char> data(32768);
  for (int i = 0; i < 32768; i++) {
    std::generate(data, data + i, std::ref(rbe));
    int clen = LZ4_compress_default(data, comp_data, 32768, 32767);
    printf("%d %d\n", i, clen);
  }
  return 0;
}
