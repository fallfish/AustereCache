#include <fstream>
#include "utils/utils.h"
#include "ssddup/ssddup.h"

bool compare_array(void *a, void *b, uint32_t length)
{
  for (uint32_t i = 0; i < length; i++) {
   if (((char*)a)[i] != ((char*)b)[i]) return false;
  }
  return true;
}

void work(char *test, char *_test, int size, cache::SSDDup &ssddup, uint64_t &total_bytes)
{
  for (uint32_t i = 0; i < 1000; i++) {
    //std::cout << "read: " << i << std::endl;

    uint64_t begin = rand() % size;
    uint64_t end = begin + rand() % (1024 * 128);
    if (end >= size) end = size - 1;
    if (begin == end) continue;
    //uint32_t op = rand() % 2;
    uint32_t op = 1;

    //std::cout << op << " " << begin << " " << end << std::endl;
    if (op == 0) {
      for (uint32_t i = begin; i < end; i++) {
        test[i] = rand() & 0xff;
      }
      ssddup.write(begin, test + begin, end - begin);
    } else {
      ssddup.read(begin, _test + begin, end - begin);
      total_bytes += end - begin;
      //if (compare_array(test + begin, _test + begin, end - begin) == false) {
        //std::cout << "SSDDupTest: read content not match for address: "
          //<< begin << ", length: "
          //<< end - begin << std::endl;
      //}
    }
  }
}

int main(int argc, char **argv)
{
  srand(0);
  cache::SSDDup ssddup;
  uint64_t size = 1024 * 1024 * 512;
#ifdef __APPLE__
  char *test, _test;
  posix_memalign((void**)&test, 512, size);
  posix_memalign((void**)&_test, 512, size);
#else
  char *test = (char *)aligned_alloc(512, size);
  char *_test = (char *)aligned_alloc(512, size);
#endif

  std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
  size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (file.read(test, size))
  {
    /* worked! */
  }

  std::cout << size << std::endl;
  ssddup.write(0, test, size);
  uint64_t total_bytes = 0;
  int elapsed = 0;
  PERF_FUNCTION(elapsed, work, test, _test, size, ssddup, total_bytes);
  std::cout << (double)total_bytes / (1024 * 1024) << std::endl;
  std::cout << elapsed << " ms" << std::endl;
  std::cout << (double)total_bytes / elapsed << " MBytes/s" << std::endl;
  free(test);
  free(_test);
}
