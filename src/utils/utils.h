#include <sys/time.h>

#define DEBUG(str) \
  std::cout << str << std::endl;

//#define DEBUG(str) 
#define PERF_FUNCTION(elapsed, func, ...) \
  {\
    struct timeval t1, t2;\
    gettimeofday(&t1, NULL);\
    func(__VA_ARGS__); \
    gettimeofday(&t2, NULL);\
    elapsed += (t2.tv_sec-t1.tv_sec)*1000000 + t2.tv_usec-t1.tv_usec;\
  }

#define PERF_FUNCTION_BOOL(elapsed, func, ...) \
  {\
    struct timeval t1, t2;\
    gettimeofday(&t1, NULL);\
    bool ret = func(__VA_ARGS__); \
    gettimeofday(&t2, NULL);\
    elapsed += (t2.tv_sec-t1.tv_sec)*1000000 + t2.tv_usec-t1.tv_usec;\
    ret; \
  }
