#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/utils.h"
#include "utils/gen_zipf.h"
#include "utils/cJSON.h"
#include "austere_cache/austere_cache.h"
#include "metadata/cachededup/cdarc_fpindex.h"
#include "metadata/cachededup/darc_fpindex.h"

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

struct Request {
  Request() { 
    address_ = 0;
    length_ = 0;
    isRead_ = 0;
    compressionLength_ = 0;
  }

  uint64_t address_;
  int length_;
  bool isRead_;
  uint32_t compressionLength_;
  char sha1_[42];
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
          } else if (strcmp(name, "subchunkSize") == 0) {
            Config::getInstance().setSubchunkSize(valuell);
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
          // Configurations for Techniques (Design)
          } else if (strcmp(name, "syntheticCompression") == 0) { // Compression
            Config::getInstance().enableSynthenticCompression(valuell);
          } else if (strcmp(name, "compactCachePolicy") == 0) { // CompactCache Replacement Policies
            Config::getInstance().enableCompactCachePolicy(valuell);
          } else if (strcmp(name, "sketchBasedReferenceCounter") == 0) { // Sketch
            Config::getInstance().enableSketchRF(valuell);
          // Configurations for Techniques (Implementation)
          } else if (strcmp(name, "multiThreading") == 0) { // Concurrency
            Config::getInstance().enableMultiThreading(valuell);
          } else if (strcmp(name, "nThreads") == 0) {
            Config::getInstance().setnThreads(valuell);
          } else if (strcmp(name, "weuSize") == 0) { // Write Buffer
            Config::getInstance().setWeuSize(valuell);
          } else if (strcmp(name, "cacheMode") == 0) { // Write Back and Write Through
            if (strcmp(valuestring, "WriteThrough") == 0) {
              Config::getInstance().setCacheMode(CacheModeEnum::tWriteThrough);
            } else if (strcmp(valuestring, "WriteBack") == 0) {
              Config::getInstance().setCacheMode(CacheModeEnum::tWriteBack);
            }
          // Configurations related to trace replay
          } else if (strcmp(name, "directIO") == 0) {
            Config::getInstance().enableDirectIO(valuell);
          } else if (strcmp(name, "traceReplay") == 0) {
            Config::getInstance().enableTraceReplay(valuell);
          } else if (strcmp(name, "fakeIO") == 0) {
            Config::getInstance().enableFakeIO(valuell);
          }
        }

        if (Config::getInstance().isSynthenticCompressionEnabled()) {
          generateCompression();
        }

        printf("cache device size: %" PRId64 " MiB\n", Config::getInstance().getCacheDeviceSize() / 1024 / 1024);
        printf("primary device size: %" PRId64 " GiB\n", Config::getInstance().getPrimaryDeviceSize() / 1024 / 1024 / 1024);
        printf("woring set size: %" PRId64 " MiB\n", Config::getInstance().getWorkingSetSize() / 1024 / 1024);
        AustereCache_ = std::make_unique<AustereCache>();
        

        assert(readFIUTrace(config->child->valuestring) == 0);
        cJSON_Delete(config);
      }


      /**
       * Duplicate 40 bytes to chunkSize_ 
       */
      void memoryRepeat(char* chunkData, char* fingerprint) {
        int offset = 40;
        int size = 40;
        memcpy(chunkData, fingerprint, 40);
        for ( ; offset + size < chunkSize_; size <<= 1, offset <<= 1) {
          memcpy(chunkData + offset, chunkData, size);
        } 
        memcpy(chunkData + offset, chunkData, chunkSize_ - offset);
      }

      /**
       * Read the access pattern file in the FIU traces
       */
      int readFIUTrace(char* fileName) {
        FILE *f = fopen(fileName, "r");
        char op[2];
        const int chunkSize = Config::getInstance().getChunkSize();

        int chunk_id, length;
        assert(f != nullptr);
        Request req;
        static uint64_t cnt = 0;

        double compressibility;

        while (fscanf(f, "%lu %d %s %s %lf", &req.address_, &req.length_, op, req.sha1_, &compressibility) != -1) {
          cnt++;

          if (Config::getInstance().isSynthenticCompressionEnabled()) {
            int clen = (double)chunkSize / compressibility; 
            if (clen >= chunkSize) clen = chunkSize;
            req.compressionLength_ = clen;
            while (req.compressionLength_ < chunkSize) {
              if (compressedChunks_[req.compressionLength_] != nullptr) {
                // Compression length matched
                break;
              }
              req.compressionLength_ += 1;  // Find the next compression length
            }
          }

          req.isRead_ = (op[0] == 'r' || op[0] == 'R');

          reqs_.emplace_back(req);
        }
        printf("%s: Go through %lu operations, selected %lu\n", fileName, cnt, reqs_.size());

        fclose(f);
      }

      void sendRequest(Request &req) {
        uint64_t begin;
        int len;
        char sha1[43];
        alignas(512) char rwdata[chunkSize_];
        begin = req.address_;
        len = req.length_;

        std::string s = std::string(req.sha1_);
        convertStr2Sha1(req.sha1_, sha1);
        Config::getInstance().setFingerprint(req.address_, sha1);

        if (Config::getInstance().isSynthenticCompressionEnabled()) {
          if (Config::getInstance().isFakeIOEnabled() || !req.isRead_) {
            memcpy(rwdata, originalChunks_[req.compressionLength_], len);
          }
        } else {
          if (Config::getInstance().isFakeIOEnabled() || !req.isRead_) {
            memoryRepeat(rwdata, req.sha1_);
          }
        }

        if (req.isRead_) {
          AustereCache_->read(begin, rwdata, len);
        } else {
          AustereCache_->write(begin, rwdata, len);
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
          threadPool->doJob([this, i]() {
              {
                std::unique_lock<std::mutex> l(mutex_);
                while (accessSet_.find(reqs_[i].address_) != accessSet_.end()) {
                  condVar_.wait(l);
                }
                accessSet_.insert(reqs_[i].address_);
              }
              if (i % 100000 == 0) printf("req %u\n", i); // , num of unique fingerprint = %d\n", i, sets.size());
              sendRequest(reqs_[i]);
              {
                std::unique_lock<std::mutex> l(mutex_);
                accessSet_.erase(reqs_[i].address_);
                condVar_.notify_all();
              }
          });
          total_bytes += chunkSize;
        }
        condVar_.notify_all();
        delete threadPool;
        sync();
      }

    void generateCompression() {
      uint32_t chunkSize = Config::getInstance().getChunkSize();
      int tmp = posix_memalign(reinterpret_cast<void **>(&compressedChunks_), 512, sizeof(char*) * (1 + chunkSize));
      if (tmp < 0) {
        std::cout << "Cannot allocate memory!" << std::endl;
        exit(-1);
      }
      tmp = posix_memalign(reinterpret_cast<void **>(&originalChunks_), 512, sizeof(char*) * (1 + chunkSize));
      if (tmp < 0) {
        std::cout << "Cannot allocate memory!" << std::endl;
        exit(-1);
      }
      char originalChunk[chunkSize], compressedChunk[chunkSize];
      memset(originalChunk, 1, sizeof(char) * chunkSize);
      memset(compressedChunk, 1, sizeof(char) * chunkSize);
      for (uint32_t i = 0; i <= chunkSize; ++i) {
        compressedChunks_[i] = nullptr;
        originalChunks_[i] = nullptr;
      }

      using random_bytes_engine = std::independent_bits_engine<
        std::default_random_engine, CHAR_BIT, unsigned char>;
      random_bytes_engine rbe(107u);

      for (uint32_t i = 0; i < chunkSize; i+=8) {

        if (i) std::generate(originalChunk, originalChunk + i, std::ref(rbe));
        int clen = LZ4_compress_default(originalChunk, compressedChunk, chunkSize, chunkSize-1);
        if (clen == 0) clen = chunkSize;

        if (compressedChunks_[clen] == nullptr) {
          int tmp = posix_memalign(reinterpret_cast<void **>(&compressedChunks_[clen]), 512, chunkSize);
          if (tmp < 0) {
            std::cout << "Cannot allocate memory!" << std::endl;
            exit(-1);
          }
          tmp = posix_memalign(reinterpret_cast<void **>(&originalChunks_[clen]), 512, chunkSize);
          if (tmp < 0) {
            std::cout << "Cannot allocate memory!" << std::endl;
            exit(-1);
          }

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
      std::unique_ptr<AustereCache> AustereCache_;
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

  if (cache::Config::getInstance().isTraceReplayEnabled()) {
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

