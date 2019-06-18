#include <sys/time.h>

#define PERF_FUNCTION(func, elapsed) \
  {\
    struct timeval t1, t2;\
    gettimeofday(&t1, NULL);\
    func();\
    gettimeofday(&t2, NULL);\
    elapsed += (t2.tv_sec-t1.tv_sec)*1000000 + t2.tv_usec-t1.tv_usec;\
  }
