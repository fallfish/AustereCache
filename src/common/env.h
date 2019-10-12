//#define DIRECT_IO
//#define CACHE_DEDUP
//#define DARC
//#define DLRU
#ifdef __MAC_OS_X_VERSION_MIN_REQUIRED
#define O_DIRECT 0
#define syncfs(a) sleep(0)
#endif
