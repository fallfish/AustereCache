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
        compressedChunks_ = nullptr;
        originalChunks_ = nullptr;
      }

      ~RunSystem() {
        _reqs.clear();
        if (compressedChunks_ != nullptr) {
          for (int i = 0; i <= Config::getInstance().getChunkSize(); ++i) {
            if (compressedChunks_[i] != nullptr) free(compressedChunks_[i]);
            if (originalChunks_[i] != nullptr) free(originalChunks_[i]);
          }
          free(compressedChunks_);
          free(originalChunks_);
        }
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
          if (strcmp(param, "--primary-device-name") == 0) {
            Config::getInstance().setPrimaryDeviceName(argv[i + 1]);
          } else if (strcmp(param, "--cache-device-name") == 0) {
            Config::getInstance().setCacheDeviceName(argv[i + 1]);
          } else if (strcmp(param, "--cache-device-size") == 0) {
            uint64_t cacheSize = 0;
            sscanf(value, "%llu", &cacheSize);
            Config::getInstance().setCacheDeviceSize(cacheSize);
          } else if (strcmp(param, "--primary-device-size") == 0) {
            uint64_t primarySize = 0;
            sscanf(value, "%llu", &primarySize);
            Config::getInstance().setPrimaryDeviceSize(primarySize);
          } else if (strcmp(param, "--working-set-size") == 0) {
            uint64_t workingSetSize = 0;
            sscanf(value, "%llu", &workingSetSize);
            Config::getInstance().setWorkingSetSize(workingSetSize);
          } else if (strcmp(param, "--lba-amplifier") == 0) {
            Config::getInstance().setLBAAmplifier(atof(value));
            std::cout << "LBAAmplifier: " << Config::getInstance().getLBAAmplifier() << std::endl;
          } else if (strcmp(param, "--fp-index-cache-policy") == 0) {
            std::cout << "Fingerprint policy: " << value << std::endl;
#if defined(DLRU)
            if (strcmp(value, "Normal") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tNormal);
            } else if (strcmp(value, "GarbageAware") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tGarbageAware);
            }
#else
            if (strcmp(value, "CAClock") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tCAClock);
            } else if (strcmp(value, "GarbageAwareCAClock") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tGarbageAwareCAClock);
            } else if (strcmp(value, "LeastReferenceCount") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tLeastReferenceCount);
            } else if (strcmp(value, "RecencyAwareLeastReferenceCount") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tRecencyAwareLeastReferenceCount);
            }
#endif
          } else if (strcmp(param, "--enable-sketch-rf") == 0) {
            if (atoi(value) == 1) {
              std::cout << "Enable Sketch Reference Counter" << std::endl;
            } else {
              std::cout << "Disable Sketch Reference Counter" << std::endl;
            }
            Config::getInstance().enableSketchRF(atoi(value));
          // Configurations for System 
          } else if (strcmp(param, "--num-slots-fp-bucket") == 0) {
            Config::getInstance().setnSlotsPerFpBucket(atoi(value));
          } else if (strcmp(param, "--num-bits-fp-sig") == 0) {
            Config::getInstance().setnBitsPerFpSignature(atoi(value));
          } else if (strcmp(param, "--num-slots-lba-bucket") == 0) {
            Config::getInstance().setnSlotsPerLbaBucket(atoi(value));
          } else if (strcmp(param, "--num-bits-lba-sig") == 0) {
            Config::getInstance().setnBitsPerLbaSignature(atoi(value));
          } else if (strcmp(param, "--sector-size") == 0) {
            Config::getInstance().setSectorSize(atoi(value));
          } else if (strcmp(param, "--chunk-size") == 0) {
            Config::getInstance().setChunkSize(atoi(value));
          // Configurations for algorithms
          } else if (strcmp(param, "--fingerprint-algorithm") == 0) {
            Config::getInstance().setFingerprintAlgorithm(atoi(value));
          } else if (strcmp(param, "--fingerprint-computation-method") == 0) {
            Config::getInstance().setFingerprintMode(atoi(value));
          } else if (strcmp(param, "--write-buffer-size") == 0) {
            Config::getInstance().setWriteBufferSize(atoi(value));
          } else if (strcmp(param, "--enable-direct-io") == 0) {
            Config::getInstance().enableDirectIO(atoi(value));
          } else if (strcmp(param, "--enable-replay-fiu") == 0) {
            Config::getInstance().enableReplayFIU(atoi(value));
          } else if (strcmp(param, "--enable-fake-io") == 0) {
            Config::getInstance().enableFakeIO(atoi(value));
          } else if (strcmp(param, "--enable-synthentic-compression") == 0) {
            Config::getInstance().enableSynthenticCompression(atoi(value));
          }
        }

        if (Config::getInstance().isSynthenticCompressionEnabled()) {
          generateCompression();
        }

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
        assert(has_access_pattern);
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

          if (Config::getInstance().isSynthenticCompressionEnabled()) {
            int clen = (double)chunk_size / compressibility; 
            if (clen >= chunk_size) clen = chunk_size;
            while (compressedChunks_[clen] == nullptr && clen < chunk_size) clen++;
            req.compressed_len = clen;
          }

          req.r = (op[0] == 'r' || op[0] == 'R');

          _reqs.push_back(req);
          continue;
          //if (cnt % 100000 == 0) printf("req %d\n", cnt); // , num of unique sha1 = %d\n", i, sets.size());
          //sendRequest(req, rwdata, sha1);
          //if (_reqs[i].r) printf("req %d\n", i);

        }
        printf("%s: Go through %lld operations, selected %lu\n", ap_file, cnt, _reqs.size());

        free(rwdata);
        fclose(f);
      }

      void sendRequest(req_rec_t &req, char *rwdata, char *sha1) {
        LL begin;
        int len;
        uint32_t chunkSize = _chunk_size;

        begin = req.addr;
        len = req.len;

        //Zero hit ratio test
        //sprintf(_reqs[i].sha1, "0000_0000_0000_0000_0000_0000_00_%07d", i);
        std::string s = std::string(req.sha1);
        convertStr2Sha1(req.sha1, sha1);

        if (Config::getInstance().isReplayFIUEnabled()) {
          Config::getInstance().setCurrentFingerprint(sha1);
        }

        //if (req.r) {
          //printf("compressed len: %d\n", req.compressed_len);
        if (Config::getInstance().isSynthenticCompressionEnabled()) {
          Config::getInstance().setCurrentData(compressedChunks_[req.compressed_len], req.compressed_len);
          Config::getInstance().setCurrentCompressedLen(req.compressed_len);
          if (!req.r) {
            memcpy(rwdata, originalChunks_[req.compressed_len], len);
          }
        } else {
          if (!req.r)
            memcpy40Bto32K(rwdata, req.sha1);
        }

        if (req.r) {
          _ssddup->read(begin, rwdata, len);
        } else {
          _ssddup->write(begin, rwdata, len);
        }

        // Debug
        if (Config::getInstance().isSynthenticCompressionEnabled()) {
          if (false && req.r) {
            if (!memcmp(rwdata, originalChunks_[req.compressed_len], chunkSize)) printf("OK, same\n");
            else {
              printf("not ok, not same\nOriginal data: ");
              print_SHA1(originalChunks_[req.compressed_len], chunkSize);
              printf("Read data: ");
              print_SHA1(rwdata, chunkSize);
            }
          }
        }
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
          if (i % 100000 == 0) printf("req %d\n", i); // , num of unique sha1 = %d\n", i, sets.size());
          //if (_reqs[i].r) printf("req %d\n", i);

          sendRequest(_reqs[i], rwdata, sha1);
          total_bytes += chunk_size;
        }
        sync();
      }

    void generateCompression() {
      int chunkSize = Config::getInstance().getChunkSize();
      compressedChunks_ = (char**)malloc(sizeof(char*) * (1 + chunkSize));
      originalChunks_ = (char**)malloc(sizeof(char*) * (1 + chunkSize));
      char originalChunk[chunkSize], compressedChunk[chunkSize];
      memset(originalChunk, 1, sizeof(char) * chunkSize);
      memset(compressedChunk, 1, sizeof(char) * chunkSize);
      for (int i = 0; i <= chunkSize; ++i) {
        compressedChunks_[i] = nullptr;
        originalChunks_[i] = nullptr;
      }

      using random_bytes_engine = std::independent_bits_engine<
        std::default_random_engine, CHAR_BIT, unsigned char>;
      random_bytes_engine rbe(1007);

      for (int i = 0; i < chunkSize; i+=8) {

        if (i) std::generate(originalChunk, originalChunk + i, std::ref(rbe));
        int clen = LZ4_compress_default(originalChunk, compressedChunk, chunkSize, chunkSize-1);
        if (clen == 0) clen = chunkSize;

        if (compressedChunks_[clen] == nullptr) {
          compressedChunks_[clen] = (char*)malloc(sizeof(char) * chunkSize);
          originalChunks_[clen] = (char*)malloc(sizeof(char) * chunkSize);
          memcpy(compressedChunks_[clen], compressedChunk, sizeof(char) * clen);
          memcpy(originalChunks_[clen], originalChunk, sizeof(char) * chunkSize);
        }
      }
    }

    private: 
      void print_help()
      {
      }

      WorkloadConfiguration _workload_conf; 
      uint32_t _working_set_size;
      LL _working_set_size_LBA;
      uint32_t _chunk_size;

      bool _multi_thread;
      double _wr_ratio;

      std::unique_ptr<SSDDup> _ssddup;
      std::vector<req_rec> _reqs;

      // for compression
      char** compressedChunks_;
      char** originalChunks_;
  };

}

int main(int argc, char **argv)
{
  // debug
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  if (cache::Config::getInstance().isReplayFIUEnabled()) {
    printf("defined: REPLAY_FIU\n");
  }

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
  long long elapsed = 0;
  PERF_FUNCTION(elapsed, run_system.work, total_bytes);

  std::cout << "Replay finished, statistics: \n";
  std::cout << "total MBs: " << (double)total_bytes / (1024 * 1024) << std::endl;
  std::cout << "elapsed: " << elapsed << " us" << std::endl;
  std::cout << "Throughput: " << (double)total_bytes / elapsed << " MBytes/s" << std::endl;

  return 0;
}

