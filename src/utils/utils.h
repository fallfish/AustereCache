#include <sys/time.h>

#define DEBUG(str) \
  std::cout << str << std::endl;

#define BEGIN_TIMER() \
  { \
    struct timeval ts_begin_, ts_end_;\
    gettimeofday(&ts_begin_, NULL);

#define END_TIMER(phase_name) \
    gettimeofday(&ts_end_, NULL); \
    Stats::get_instance()->add_time_elapsed_##phase_name(\
        (ts_end_.tv_sec-ts_begin_.tv_sec)*1000000 + ts_end_.tv_usec-ts_begin_.tv_usec\
        ); \
  }

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
