#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"
#include "utils/gen_zipf.h"
#include "utils/cJSON.h"
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

#include <malloc.h>

typedef long long int LL;
struct Request {
  Request() { 
    addr = 0;
    len = 0;
    r = 0;
    compressed_len = 0;
  }

  LL addr;
  int len;
  bool r;
  uint32_t compressed_len;
  char sha1[42];
};

namespace cache {

  class RunSystem {
    public:
      RunSystem() {
        genzipf::rand_val(2);
        compressedChunks_ = nullptr;
        originalChunks_ = nullptr;
      }

      void clear() {
        reqs_.clear();
        reqs_.shrink_to_fit();
        uint32_t chunkSize = Config::getInstance().getChunkSize();
        if (compressedChunks_ != nullptr) {
          for (int i = 0; i <= Config::getInstance().getChunkSize(); ++i) {
            if (compressedChunks_[i] != nullptr) {
              free(compressedChunks_[i]);
              compressedChunks_[i] = nullptr;
            }
            if (originalChunks_[i] != nullptr) {
              free(originalChunks_[i]);
              originalChunks_[i] = nullptr;
            }
          }
          free(compressedChunks_);
          compressedChunks_ = nullptr;
          free(originalChunks_);
          originalChunks_ = nullptr;
        }
      }

      ~RunSystem() {
      }


      void parse_argument(int argc, char **argv)
      {
        Config::getInstance().setCacheDeviceSize(16LL * 1024 * 1024 * 1024);
        Config::getInstance().setPrimaryDeviceSize(300LL * 1024 * 1024 * 1024);
        chunkSize_ = 32768;

        char source[2000 + 1];
        FILE *fp = fopen(argv[1], "r");
        if (fp != NULL) {
          size_t newLen = fread(source, sizeof(char), 1001, fp);
          if ( ferror( fp ) != 0 ) {
            fputs("Error reading file", stderr);
          } else {
            source[newLen++] = '\0'; /* Just to be safe. */
          }

          fclose(fp);
        }

        cJSON *config = cJSON_Parse(source);
        for (cJSON *param = config->child->next->child; param != nullptr; param = param->next) {
          char *name = param->string;
          char *valuestring = param->valuestring;
          uint64_t valuell = (uint64_t)param->valuedouble;

          // 1. Disk related configurations
          if (strcmp(name, "primaryDeviceName") == 0) {
            Config::getInstance().setPrimaryDeviceName(valuestring);
          } else if (strcmp(name, "cacheDeviceName") == 0) {
            Config::getInstance().setCacheDeviceName(valuestring);
          } else if (strcmp(name, "primaryDeviceSize") == 0) {
            Config::getInstance().setPrimaryDeviceSize(valuell);
          } else if (strcmp(name, "cacheDeviceSize") == 0) {
            Config::getInstance().setCacheDeviceSize(valuell);
          } else if (strcmp(name, "workingSetSize") == 0) {
            Config::getInstance().setWorkingSetSize(valuell);
          } else if (strcmp(name, "chunkSize") == 0) {
            Config::getInstance().setChunkSize(valuell);
            chunkSize_ = valuell;
          } else if (strcmp(name, "sectorSize") == 0) {
            Config::getInstance().setSectorSize(valuell);
          // 2. Index related configurations
          } else if (strcmp(name, "nSlotsPerFpBucket") == 0) {
            Config::getInstance().setnSlotsPerFpBucket(valuell);
          } else if (strcmp(name, "nSlotsPerLbaBucket") == 0) {
            Config::getInstance().setnSlotsPerLbaBucket(valuell);
          } else if (strcmp(name, "nBitsPerFpSignature") == 0) {
            Config::getInstance().setnBitsPerFpSignature(valuell);
          } else if (strcmp(name, "nBitsPerLbaSignature") == 0) {
            Config::getInstance().setnBitsPerLbaSignature(valuell);
          } else if (strcmp(name, "lbaAmplifier") == 0) {
            Config::getInstance().setLBAAmplifier(valuell);
          } else if (strcmp(name, "cachePolicyForFPIndex") == 0) {
#if defined(DLRU)
            if (strcmp(valuestring, "Normal") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tNormal);
            } else if (strcmp(valuestring, "GarbageAware") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tGarbageAware);
            }
#else
            if (strcmp(valuestring, "CAClock") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tCAClock);
            } else if (strcmp(valuestring, "GarbageAwareCAClock") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tGarbageAwareCAClock);
            } else if (strcmp(valuestring, "LeastReferenceCount") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tLeastReferenceCount);
            } else if (strcmp(valuestring, "RecencyAwareLeastReferenceCount") == 0) {
              Config::getInstance().setCachePolicyForFPIndex(CachePolicyEnum::tRecencyAwareLeastReferenceCount);
            }
#endif
          // Configurations for Techniques (Design)
          } else if (strcmp(name, "synthenticCompression") == 0) { // Compression
            Config::getInstance().enableSynthenticCompression(valuell);
          } else if (strcmp(name, "compactCachePolicy") == 0) { // CompactCache Replacement Policies
            Config::getInstance().enableCompactCachePolicy(valuell);
          } else if (strcmp(name, "sketchBasedReferenceCounter") == 0) { // Sketch
            Config::getInstance().enableSketchRF(valuell);
          // Configurations for Techniques (Implementation)
          } else if (strcmp(name, "multiThreading") == 0) { // Concurrency
            Config::getInstance().enableMultiThreading(valuell);
          } else if (strcmp(name, "writeBufferSize") == 0) { // Write Buffer
            Config::getInstance().setWriteBufferSize(valuell);
          } else if (strcmp(name, "cacheMode") == 0) { // Write Back and Write Through
            if (strcmp(valuestring, "WriteThrough") == 0) {
              Config::getInstance().setCacheMode(CacheModeEnum::tWriteThrough);
            } else if (strcmp(valuestring, "WriteBack") == 0) {
              Config::getInstance().setCacheMode(CacheModeEnum::tWriteBack);
            }
          // Configurations related to trace replay
          } else if (strcmp(name, "directIO") == 0) {
            Config::getInstance().enableDirectIO(valuell);
          } else if (strcmp(name, "replayFIU") == 0) {
            Config::getInstance().enableReplayFIU(valuell);
          } else if (strcmp(name, "fakeIO") == 0) {
            Config::getInstance().enableFakeIO(valuell);
          } else if (strcmp(name, "disableCache") == 0) {
            Config::getInstance().disableCache(valuell);
          }
        }

        if (Config::getInstance().isSynthenticCompressionEnabled()) {
          generateCompression();
        }

        printf("cache device size: %" PRId64 " MiB\n", Config::getInstance().getCacheDeviceSize() / 1024 / 1024);
        printf("primary device size: %" PRId64 " GiB\n", Config::getInstance().getPrimaryDeviceSize() / 1024 / 1024 / 1024);
        printf("woring set size: %" PRId64 " MiB\n", Config::getInstance().getWorkingSetSize() / 1024 / 1024);
        ssddup_ = std::make_unique<SSDDup>();
        

        assert(readFIUaps(config->child->valuestring) == 0);
        cJSON_Delete(config);
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

      int readFIUaps(char* str) {
        printf("start reading access patterns ...\n");
        char match[100];
        int begin, end;
        std::cout << str << std::endl;
        sscanf(str, "%s %d %d", match, &begin, &end);
        if (strstr(match, "%02d")) {
          for (int j = begin; j <= end; j++) {
            char filename[100];
            sprintf(filename, match, j);
            readFIUap(filename);
          }
        } else {
          readFIUap(match);
        }
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
        int chunkSize = Config::getInstance().getChunkSize();

        int chunk_id, length;
        assert(f != nullptr);
        Request req;
        LL cnt = 0;

        double compressibility;

        while (fscanf(f, "%lld %d %s %s %lf", &req.addr, &req.len, op, req.sha1, &compressibility) != -1) {
          cnt++;

          if (Config::getInstance().isSynthenticCompressionEnabled()) {
            int clen = (double)chunkSize / compressibility; 
            if (clen >= chunkSize) clen = chunkSize;
            req.compressed_len = clen;
            while (req.compressed_len < chunkSize) {
              if (compressedChunks_[req.compressed_len] != nullptr) break;
              req.compressed_len += 1;
            }
          }

          req.r = (op[0] == 'r' || op[0] == 'R');

          reqs_.emplace_back(req);
          continue;
          //sendRequest(req);
        }
        printf("%s: Go through %lld operations, selected %lu\n", ap_file, cnt, reqs_.size());

        fclose(f);
      }

      void sendRequest(Request &req) {
        LL begin;
        int len;
        char sha1[43];
        alignas(512) char rwdata[chunkSize_];
        begin = req.addr;
        len = req.len;

        std::string s = std::string(req.sha1);
        convertStr2Sha1(req.sha1, sha1);
        Config::getInstance().setFingerprint(req.addr, sha1);

        if (Config::getInstance().isSynthenticCompressionEnabled()) {
          if (Config::getInstance().isFakeIOEnabled() || !req.r) {
            memcpy(rwdata, originalChunks_[req.compressed_len], len);
          }
        } else {
          if (Config::getInstance().isFakeIOEnabled() || !req.r) {
            memcpy40Bto32K(rwdata, req.sha1);
          }
        }

        if (req.r) {
          ssddup_->read(begin, rwdata, len);
        } else {
          ssddup_->write(begin, rwdata, len);
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
        int nThreads = 1;
        uint32_t chunkSize = Config::getInstance().getChunkSize();
        if (Config::getInstance().isMultiThreadingEnabled()) {
          nThreads = Config::getInstance().getMaxNumGlobalThreads();
        }

        AThreadPool *threadPool = new AThreadPool(nThreads);
        char sha1[23];
        for (uint32_t i = 0; i < reqs_.size(); ++i) {
          //if (i == 200000) break;
          threadPool->doJob([this, i]() {
              {
                std::unique_lock<std::mutex> l(mutex_);
                while (accessSet_.find(reqs_[i].addr) != accessSet_.end()) {
                  condVar_.wait(l);
                }
                accessSet_.insert(reqs_[i].addr);
              }
              if (i % 100000 == 0) printf("req %u\n", i); // , num of unique fingerprint = %d\n", i, sets.size());
              sendRequest(reqs_[i]);
              {
                std::unique_lock<std::mutex> l(mutex_);
                accessSet_.erase(reqs_[i].addr);
                condVar_.notify_all();
              }
          });
          total_bytes += chunkSize;
        }
        condVar_.notify_all();
        //std::cout << "??" << std::endl;
        delete threadPool;
        sync();
      }

    void generateCompression() {
      uint32_t chunkSize = Config::getInstance().getChunkSize();
      posix_memalign(reinterpret_cast<void **>(&compressedChunks_), 512, sizeof(char*) * (1 + chunkSize));
      posix_memalign(reinterpret_cast<void **>(&originalChunks_), 512, sizeof(char*) * (1 + chunkSize));
      char originalChunk[chunkSize], compressedChunk[chunkSize];
      memset(originalChunk, 1, sizeof(char) * chunkSize);
      memset(compressedChunk, 1, sizeof(char) * chunkSize);
      for (uint32_t i = 0; i <= chunkSize; ++i) {
        compressedChunks_[i] = nullptr;
        originalChunks_[i] = nullptr;
      }

      using random_bytes_engine = std::independent_bits_engine<
        std::default_random_engine, CHAR_BIT, unsigned char>;
      random_bytes_engine rbe(1007);

      for (uint32_t i = 0; i < chunkSize; i+=8) {

        if (i) std::generate(originalChunk, originalChunk + i, std::ref(rbe));
        int clen = LZ4_compress_default(originalChunk, compressedChunk, chunkSize, chunkSize-1);
        if (clen == 0) clen = chunkSize;

        if (compressedChunks_[clen] == nullptr) {
          posix_memalign(reinterpret_cast<void **>(&compressedChunks_[clen]), 512, chunkSize);
          posix_memalign(reinterpret_cast<void **>(&originalChunks_[clen]), 512, chunkSize);
          memcpy(compressedChunks_[clen], compressedChunk, sizeof(char) * clen);
          memcpy(originalChunks_[clen], originalChunk, sizeof(char) * chunkSize);
        }
      }
    }

    private: 
      uint64_t workingSetSize_;
      uint32_t chunkSize_;

      // for compression
      char** compressedChunks_;
      char** originalChunks_;
      std::unique_ptr<SSDDup> ssddup_;
      std::vector<Request> reqs_;
      std::set<uint64_t> accessSet_;
      std::mutex mutex_;
      std::condition_variable condVar_;
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

  run_system.clear();
  return 0;
}

