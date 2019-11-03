#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"
#include "utils/gen_zipf.h"
#include "ssddup/ssddup.h"
#include "workload_conf.h"

// For compression tests
#include "lz4.h"
#include <random>
#include <climits>
#include <algorithm>
#include <functional>
#include <inttypes.h>

#include <vector>
#include <set>
#include <thread>
#include <atomic>

#define DIRECT_IO

typedef long long int LL;
typedef struct req_rec_t {
  LL addr;
  int len;
  bool r;
  char sha1[42];
  int compressed_len;
} req_rec; 

namespace cache {

  class RunSystem {
    public:
      RunSystem() {
        _multi_thread = 0;
        genzipf::rand_val(2);

#ifdef NORMAL_DIST_COMPRESSION
        generateCompression();
#endif
      }
      ~RunSystem() {
        _reqs.clear();
        for (auto it : comp_data) free(it.second); 
        for (auto it : comp_original_data) free(it.second); 
        comp_data.clear();
        comp_original_data.clear();
      }


      void parse_argument(int argc, char **argv)
      {
        bool has_ini_stat = false, has_config = false, has_access_pattern = false;
        char *param, *value;
        Config::getInstance().setCacheDeviceSize(16LL * 1024 * 1024 * 1024);
        Config::getInstance().setPrimaryDeviceSize(300LL * 1024 * 1024 * 1024);

        for (int i = 1; i < argc; i += 2) {
          param = argv[i];
          value = argv[i + 1];
          assert(value != nullptr);
          if (strcmp(param, "--cache-device-size") == 0) {
            uint64_t cacheSize = 0;
            sscanf(value, "%llu", &cacheSize);
            Config::getInstance().setCacheDeviceSize(cacheSize);
          } else if (strcmp(param, "--lba-amplifier") == 0) {
            Config::getInstance().setLBAAmplifier(atoi(value));
            printf("LBAAmplifier: %d\n", atoi(value));
          } else if (strcmp(param, "--wr-ratio") == 0) {
            _wr_ratio = atof(value);
          } else if (strcmp(param, "--multi-thread") == 0) {
            _multi_thread = atoi(value);
          } else if (strcmp(param, "--fingerprint-algorithm") == 0) {
            Config::getInstance().setFingerprintAlgorithm(atoi(value));
          } else if (strcmp(param, "--fingerprint-computation-method") == 0) {
            Config::getInstance().setFingerprintMode(atoi(value));
          } else if (strcmp(param, "--write-buffer-size") == 0) {
            Config::getInstance().setWriteBufferSize(atoi(value));
          } else if (strcmp(param, "--direct-io") == 0) {
            Config::getInstance().enableDirectIO(atoi(value));
          //} else if (strcmp(param, "--access-pattern") == 0) {
            //has_access_pattern = (readFIUaps(&i, argc, argv) == 0);
          } else if (strcmp(param, "--working-set-size") == 0) {
            uint64_t workingSetSize = 0;
            sscanf(value, "%llu", &workingSetSize);
            Config::getInstance().setWorkingSetSize(workingSetSize);
          }
        }

        //Config::getInstance().setCacheDeviceName("./cache_device");
        Config::getInstance().setPrimaryDeviceName("./primary_device");
        //Config::getInstance().setCacheDeviceName("./ramdisk/cache_device");
        //Config::getInstance().setPrimaryDeviceName("./primary_device");
        //Config::getInstance().setPrimaryDeviceName("./ramdisk/primary_device");
        Config::getInstance().setCacheDeviceName("./ramdisk/cache_device");
        //Config::getInstance().setPrimaryDeviceName("/dev/sdb");
        //Config::getInstance().setCacheDeviceName("/dev/sda");
        //assert(has_access_pattern);

        printf("cache device size: %" PRId64 " MiB\n", Config::getInstance().getCacheDeviceSize() / 1024 / 1024);
        printf("primary device size: %" PRId64 " GiB\n", Config::getInstance().getPrimaryDeviceSize() / 1024 / 1024 / 1024);
        printf("woring set size: %" PRId64 " MiB\n", Config::getInstance().getWorkingSetSize() / 1024 / 1024);
        _ssddup = std::make_unique<SSDDup>();
        for (int i = 1; i < argc; i += 2) {
          param = argv[i];
          value = argv[i + 1];
          if (strcmp(param, "--access-pattern") == 0) {
            has_access_pattern = (readFIUaps(&i, argc, argv) == 0);
          }
        }
      }

      /**
       * function created by jhli
       * 
       * Duplicate 32 bytes to 4096 bytes
       */
      void memcpy32Bto4K(char* K4, char* B32) {
        for (int i = 0; i < 128; i++) {
          memcpy(K4 + (i << 5), B32, 32);
        }
      }

      /**
       * function created by jhli
       * 
       * Duplicate 32 bytes to 32768 bytes
       */
      void memcpy32Bto32K(char* K32, char* B32) {
        for (int i = 0; i < 1024; i++) {
          memcpy(K32 + (i << 5), B32, 32);
        }
      }

      /**
       * function created by jhli
       * 
       * Duplicate 40 bytes to 32768 bytes
       */
      void memcpy40Bto32K(char* K32, char* B40) {
        int offset = 40;
        int size = 40;
        memcpy(K32, B40, 40);
        for ( ; size < 20480; size <<= 1) {
          memcpy(K32 + offset, K32, size);
          offset <<= 1;
        } 
        memcpy(K32 + 20480, K32, 12288);
        /*
           for (int i = 0; i < 819; i++) {
           memcpy(K32 + offset, B40, 40);
           offset += 40;
           }
           memcpy(K32 + offset, B40, 8);
           */
      }

      int readFIUaps(int* i, int argc, char** argv) {
        printf("start reading access patterns ...\n");
        int begin, end;
        if (strstr(argv[*i+1],"%02d")) {
          if (*i+3 < argc) {
            if (sscanf(argv[*i+2], "%d", &begin) <= 0) return 1;
            if (sscanf(argv[*i+3], "%d", &end) <= 0) return 1;
            if (begin > end) return 1;
          }

          for (int j = begin; j <= end; j++) {
            char filename[100];
            sprintf(filename, argv[*i+1], j);
            readFIUap(filename);
          }

          *i += 2;
        }
        else
          readFIUap(argv[*i+1]);
        printf("end reading access patterns ...\n");

        return 0;
      }

      /**
       * function created by jhli
       * 
       * Read the access pattern file in the FIU traces
       */
      void readFIUap(char* ap_file) {
        FILE *f = fopen(ap_file, "r");
        uint32_t len;
        char sha1[50];
        char op[2];
        int chunk_size = Config::getInstance().getChunkSize();

        int chunk_id, length;
        assert(f != nullptr);
        req_rec req;
        LL cnt = 0;

        double compressibility;

        char* rwdata;
        posix_memalign(reinterpret_cast<void **>(&rwdata), 512, 32768 + 10);
        std::string s;

        while (fscanf(f, "%lld %d %s %s %lf", &req.addr, &req.len, op, req.sha1, &compressibility) != -1) {
          cnt++;
          //if (req.addr >= Config::getInstance().getPrimaryDeviceSize()) {
            //continue;
          //}

#ifdef NORMAL_DIST_COMPRESSION
          int clen = (double)chunk_size / compressibility; 
          if (clen >= chunk_size) clen = chunk_size;
          while (!comp_data.count(clen) && clen < chunk_size) clen++;
          req.compressed_len = clen;
#endif

          req.r = (op[0] == 'r' || op[0] == 'R');

          _reqs.push_back(req);
          continue;
          if (cnt % 100000 == 0) printf("req %d\n", cnt); // , num of unique sha1 = %d\n", i, sets.size());
          //if (_reqs[i].r) printf("req %d\n", i);

          LL begin;
          int len;

          begin = req.addr;
          len = req.len;

          //Zero hit ratio test
          //sprintf(_reqs[i].sha1, "0000_0000_0000_0000_0000_0000_00_%07d", i);
          s = std::string(req.sha1);
          convertStr2Sha1(req.sha1, sha1);

#if defined(REPLAY_FIU)
          Config::getInstance().setCurrentFingerprint(sha1);
#endif

#if ((defined(CACHE_DEDUP) && defined(CDARC)) || (!defined(CACHE_DEDUP))) && defined(NORMAL_DIST_COMPRESSION)
          if (req.r) {
            //printf("compressed len: %d\n", req.compressed_len);
            Config::getInstance().setCurrentData(comp_data[req.compressed_len], req.compressed_len);
            Config::getInstance().setCurrentCompressedLen(req.compressed_len);
          }
          else 
            memcpy(rwdata, comp_data[req.compressed_len], chunk_size); 
#else
          if (!req.r)
            memcpy40Bto32K(rwdata, req.sha1);
#endif

          if (req.r) {
            _ssddup->read(begin, rwdata, len);
          } else {
            _ssddup->write(begin, rwdata, len);
          }

          // Debug
#if ((defined(CACHE_DEDUP) && defined(CDARC)) || (!defined(CACHE_DEDUP))) && defined(NORMAL_DIST_COMPRESSION)
          if (false && req.r) {
            printf("TEST: addr = %" PRId64 ", i = %d\n", req.addr, cnt);
            if (!memcmp(rwdata, comp_original_data[req.compressed_len], chunk_size)) printf("OK, same\n");
            else {
              printf("not ok, not same\nOriginal data: ");
              print_SHA1(comp_original_data[req.compressed_len], chunk_size);
              printf("Read data: ");
              print_SHA1(rwdata, chunk_size);
            }
          }
#endif
        }
        printf("%s: Go through %lld operations, selected %lu\n", ap_file, cnt, _reqs.size());

        fclose(f);
      }

      /**
       * Start replaying the traces
       */

      void convertStr2Sha1(char* s, char* sha1) {
        assert(strlen(s) >= 40);
        for (int i = 0; i < 40; i += 2) {
          sha1[i/2] = 0;
          if (s[i] <= '9') sha1[i/2] += ((s[i] - '0') << 4);
          else if (s[i] <= 'F') sha1[i/2] += ((s[i] - 'A' + 10) << 4);
          else if (s[i] <= 'f') sha1[i/2] += ((s[i] - 'a' + 10) << 4);
          if (s[i+1] <= '9') sha1[i/2] += s[i+1]; 
          else if (s[i+1] <= 'F') sha1[i/2] += s[i+1] - 'A' + 10; 
          else if (s[i+1] <= 'f') sha1[i/2] += s[i+1] - 'a' + 10; 
        }
      }

      void work(std::atomic<uint64_t> &total_bytes)
      {
        char* rwdata;
        posix_memalign(reinterpret_cast<void **>(&rwdata), 512, 32768 + 10);
        //rwdata = (char*)malloc(32768 + 10);
        std::string s;
        int chunk_size = Config::getInstance().getChunkSize();

        std::cout << _reqs.size() << std::endl;
        char sha1[23];
        for (uint32_t i = 0; i < _reqs.size(); i++) {
          //if (i == 5000000) break;
          if (i % 100000 == 0) printf("req %d\n", i); // , num of unique sha1 = %d\n", i, sets.size());
          //if (_reqs[i].r) printf("req %d\n", i);

          LL begin;
          int len;

          begin = _reqs[i].addr;
          len = _reqs[i].len;

          //Zero hit ratio test
          //sprintf(_reqs[i].sha1, "0000_0000_0000_0000_0000_0000_00_%07d", i);
          s = std::string(_reqs[i].sha1);
          convertStr2Sha1(_reqs[i].sha1, sha1);

#if defined(REPLAY_FIU)
          Config::getInstance().setCurrentFingerprint(sha1);
#endif

          total_bytes.fetch_add(len, std::memory_order_relaxed);
#if ((defined(CACHE_DEDUP) && defined(CDARC)) || (!defined(CACHE_DEDUP))) && defined(NORMAL_DIST_COMPRESSION)
          if (_reqs[i].r) {
            //printf("compressed len: %d\n", _reqs[i].compressed_len);
            Config::getInstance().setCurrentData(comp_data[_reqs[i].compressed_len], _reqs[i].compressed_len);
            Config::getInstance().setCurrentCompressedLen(_reqs[i].compressed_len);
          }
          else 
            memcpy(rwdata, comp_data[_reqs[i].compressed_len], chunk_size); 
#else
          if (!_reqs[i].r)
            memcpy40Bto32K(rwdata, _reqs[i].sha1);
#endif

          if (_reqs[i].r) {
            _ssddup->read(begin, rwdata, len);
          } else {
            _ssddup->write(begin, rwdata, len);
          }

          // Debug
#if ((defined(CACHE_DEDUP) && defined(CDARC)) || (!defined(CACHE_DEDUP))) && defined(NORMAL_DIST_COMPRESSION)
          if (false && _reqs[i].r) {
            printf("TEST: addr = %" PRId64 ", i = %d\n", _reqs[i].addr, i);
            if (!memcmp(rwdata, comp_original_data[_reqs[i].compressed_len], chunk_size)) printf("OK, same\n");
            else {
              printf("not ok, not same\nOriginal data: ");
              print_SHA1(comp_original_data[_reqs[i].compressed_len], chunk_size);
              printf("Read data: ");
              print_SHA1(rwdata, chunk_size);
            }
          }
#endif
        }
        sync();
      }

    void generateCompression() {

      static int ok = 0;

      if (ok == 0) {
        comp_original_data = {};
        comp_data = {};

        using random_bytes_engine = std::independent_bits_engine<
          std::default_random_engine, CHAR_BIT, unsigned char>;

        random_bytes_engine rbe;

        char* data_a, *comp_data_a; 
        int chunk_size = Config::getInstance().getChunkSize();

        data_a = (char*)malloc(sizeof(char) * chunk_size);
        comp_data_a = (char*)malloc(sizeof(char) * chunk_size);

        memset(data_a, 1, sizeof(char) * chunk_size);
        memset(comp_data_a, 1, sizeof(char) * chunk_size);

        for (int i = 0; i < chunk_size; i+=8) {

          if (i) std::generate(data_a, data_a + i, std::ref(rbe));
          int clen = LZ4_compress_default(data_a, comp_data_a, chunk_size, chunk_size-1);

          if (clen && !comp_data.count(clen) && !comp_original_data.count(clen)) {
            comp_data[clen] = (char*)malloc(sizeof(char) * chunk_size);
            comp_original_data[clen] = (char*)malloc(sizeof(char) * chunk_size);
            memcpy(comp_data[clen], comp_data_a, sizeof(char) * clen);
            memcpy(comp_original_data[clen], data_a, sizeof(char) * chunk_size);
          }

          if (clen == 0 && !comp_data.count(chunk_size) && !comp_original_data.count(clen)) {
            comp_data[chunk_size] = (char*)malloc(sizeof(char) * chunk_size);
            comp_original_data[chunk_size] = (char*)malloc(sizeof(char) * chunk_size);
            memcpy(comp_data[chunk_size], data_a, sizeof(char) * chunk_size);
            memcpy(comp_original_data[chunk_size], data_a, sizeof(char) * chunk_size);
          }
        }
      }

      ok = 1;
    }

    private: 
      void print_help()
      {
        std::cout << "--wr-ratio: the ratio of write chunks and read chunks" << std::endl
          << "--trace: input workload file (containing configuration & initial state)" << std::endl
          << "--ini-stat: input initial state file" << std::endl
          << "--wr-ratio: write-read ratio of the workload float number (0 ~ 1)" << std::endl
          << "--multi-thread: enable multi-thread or not, 1 means enable, 0 means disable" << std::endl
          << "--ca-bits: number of bits of ca index" << std::endl
          << "--num-workers: number of workers to issue read requests" << std::endl
          << "--fingerprint-algorithm: main fingerprint algorithm, 0 - SHA1, 1 - murmurhash" << std::endl
          << "--fingerprint-computation-method: indicate the fingerprint computation optimization, 0 - SHA1, 1 - weak hash + memcmp, 2 - weak hash + SHA1" << std::endl
          << "--access-pattern: input access pattern file" << std::endl;
      }

      WorkloadConfiguration _workload_conf; 
      uint32_t _working_set_size;
      LL _working_set_size_LBA;
      uint32_t _chunk_size;
      uint32_t _num_unique_chunks;
      uint32_t _num_chunks;
      uint32_t _distribution;
      float _skewness;

      bool _multi_thread;
      double _wr_ratio;

      std::unique_ptr<SSDDup> _ssddup;
      std::vector<req_rec> _reqs;

      // for compression
      std::map<int, char*> comp_data;
      std::map<int, char*> comp_original_data;
  };

}

int main(int argc, char **argv)
{
  // debug
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

#ifdef REPLAY_FIU
  printf("defined: REPLAY_FIU\n");
#endif

#if defined(WRITE_BACK_CACHE)
  printf("defined: WRITE_BACK_CACHE\n");
#endif

#if defined(CACHE_DEDUP) && (defined(DLRU) || defined(DARC))
  printf("defined: CACHE_DEDUP\n");
#if defined(DLRU)
  printf(" -- DLRU\n");
#endif
#if defined(DARC)
  printf(" -- DARC\n");
#endif
#endif
  srand(0);
  cache::RunSystem run_system;

  run_system.parse_argument(argc, argv);

  std::atomic<uint64_t> total_bytes(0);
  int elapsed = 0;
  PERF_FUNCTION(elapsed, run_system.work, total_bytes);

  std::cout << "Replay finished, statistics: \n";
  std::cout << "total MBs: " << (double)total_bytes / (1024 * 1024) << std::endl;
  std::cout << "elapsed: " << elapsed << " us" << std::endl;
  std::cout << "Throughput: " << (double)total_bytes / elapsed << " MBytes/s" << std::endl;

  return 0;
}

