#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"
#include "utils/gen_zipf.h"
#include "ssddup/ssddup.h"
#include "workload_conf.h"

#include <vector>
#include <set>
#include <thread>
#include <atomic>
#define DIRECT_IO

typedef uint64_t LL;
typedef struct req_rec_t {
  LL addr;
  int len;
  bool r;
  char sha1[42];
} req_rec; 


namespace cache {

  class RunSystem {
    public:
      RunSystem() {
        _multi_thread = 0;
        genzipf::rand_val(2);
      }
      ~RunSystem() {
        free(_workload_chunks);
      }

      void parse_argument(int argc, char **argv)
      {
        bool has_ini_stat = false, has_config = false, has_access_pattern = false;
        char *param, *value;
        Config::get_configuration().set_cache_device_size(4LL * 1024 * 1024 * 1024);
        printf("cache device size: %lld\n", Config::get_configuration().get_cache_device_size());
        Config::get_configuration().set_primary_device_size(300LL * 1024 * 1024 * 1024);
        printf("primary device size: %lld\n", Config::get_configuration().get_primary_device_size());

        for (int i = 1; i < argc; i += 2) {
          param = argv[i];
          value = argv[i + 1];
          assert(value != nullptr);
          if (strcmp(param, "--wr-ratio") == 0) {
            _wr_ratio = atof(value);
          } else if (strcmp(param, "--ca-bits") == 0) {
            Config::get_configuration().set_ca_bucket_no_len(atoi(value));
          } else if (strcmp(param, "--lba-bits") == 0) {
            Config::get_configuration().set_lba_bucket_no_len(atoi(value));
          } else if (strcmp(param, "--multi-thread") == 0) {
            _multi_thread = atoi(value);
          } else if (strcmp(param, "--fingerprint-algorithm") == 0) {
            Config::get_configuration().set_fingerprint_algorithm(atoi(value));
          } else if (strcmp(param, "--fingerprint-computation-method") == 0) {
            Config::get_configuration().set_fingerprint_computation_method(atoi(value));
          } else if (strcmp(param, "--write-buffer-size") == 0) {
            Config::get_configuration().set_write_buffer_size(atoi(value));
          } else if (strcmp(param, "--direct-io") == 0) {
            Config::get_configuration().set_direct_io(atoi(value));
          } else if (strcmp(param, "--access-pattern") == 0) {
            printf("now\n");
            has_access_pattern = (readFIUaps(&i, argc, argv) == 0);
            printf("now 2\n");
          }
        }

        Config::get_configuration().set_cache_device_name("./cache_device");
        Config::get_configuration().set_primary_device_name("./primary_device");
        printf("%d\n", Config::get_configuration().get_lba_bucket_no_len());
        //Config::get_configuration().set_cache_device_name("./ramdisk/cache_device");
        //Config::get_configuration().set_primary_device_name("./primary_device");
        //Config::get_configuration().set_primary_device_name("./ramdisk/primary_device");
        //Config::get_configuration().set_cache_device_name("./ramdisk/cache_device");
        //Config::get_configuration().set_primary_device_name("/dev/sdb");
        //Config::get_configuration().set_cache_device_name("/dev/sda");
        assert(has_access_pattern);

        _ssddup = std::make_unique<SSDDup>();
      }

      /**
       * function created by jhli
       * 
       * Read the configuration file
       * (Although i don't know what it is now)
       */
      void readConfiguration(char* fileName) {
        // read workload configuration
        int fd = open(fileName, O_RDONLY);
        int n = read(fd, &_workload_conf, sizeof(cache::WorkloadConfiguration));
        assert(n == sizeof(cache::WorkloadConfiguration));

        // parse workload configuration
        _chunk_size = _workload_conf._chunk_size;
        _num_unique_chunks = _workload_conf._num_unique_chunks;
        _num_chunks = _workload_conf._num_chunks;
        _working_set_size = _chunk_size * _num_chunks;
        _distribution = _workload_conf._distribution;
        _skewness = _workload_conf._skewness;
        std::cout << "chunk size: " << _chunk_size << std::endl
          << "num unique chunks: " << _num_unique_chunks << std::endl
          << "num cuhks: " << _num_chunks << std::endl 
          << "working set size: " << _working_set_size << std::endl
          << "distribution: " << _distribution << std::endl
          << "skewness: " << _skewness << std::endl;

        close(fd);
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

        int chunk_id, length;
        assert(f != nullptr);
        req_rec req;
        LL cnt = 0;
        while (fscanf(f, "%lld %d %s %s", &req.addr, &req.len, op, req.sha1) != -1) {
          cnt++;
          if (req.addr >= Config::get_configuration().get_primary_device_size()) {
            continue;
          }

          req.r = (op[0] == 'r' || op[0] == 'R');
          _reqs.push_back(req);
        }
        printf("%s: Go through %lld operations, selected %d\n", ap_file, cnt, _reqs.size());

        fclose(f);
      }

      /**
       * Start replaying the traces
       */

      void convertStr2Sha1(char* s, char* sha1) {
        assert(strlen(s) >= 40);
        for (int i = 0; i < 40; i += 2) {
          sha1[i/2] = (s[i] << 4) + s[i+1];
        }
      }

      void work(std::atomic<uint64_t> &total_bytes)
      {
        char* rwdata;
        rwdata = (char*)malloc(32768 + 10);
        std::string s;

        std::cout << _reqs.size() << std::endl;
        char sha1[23];
        for (uint32_t i = 0; i < _reqs.size(); i++) {
          if (i == 5000000) break;
          if (i % 100000 == 0) printf("req %d\n", i); // , num of unique sha1 = %d\n", i, sets.size());
          LL begin;
          int len;

          begin = _reqs[i].addr;
          len = _reqs[i].len;

          //Zero hit ratio test
          //sprintf(_reqs[i].sha1, "0000_0000_0000_0000_0000_0000_00_%07d", i);
          s = std::string(_reqs[i].sha1);
          convertStr2Sha1(_reqs[i].sha1, sha1);
          Config::get_configuration().set_current_fingerprint(sha1);

          total_bytes.fetch_add(len, std::memory_order_relaxed);
          if (_reqs[i].r) {
            _ssddup->read(begin, rwdata, len);
            //fprintf(writefd, "read, addr %d\n", begin);
          } else {
            memcpy40Bto32K(rwdata, _reqs[i].sha1);
            _ssddup->write(begin, rwdata, len);
            //fprintf(writefd, "write, addr %d\n", begin);
          }
        }
        sync();
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

      char *_workload_chunks, *_unique_chunks;

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

