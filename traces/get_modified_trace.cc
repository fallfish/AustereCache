#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<cassert>
#include<vector>
#include<map>
#include<string>
#include<utility>
#include<set>
#include<algorithm>
#include<openssl/md5.h>
#include<openssl/sha.h>
#include<sys/time.h>
#include<cmath>

class ChristeTool {
  public:
  // from www.csee.usf.edu/~kchriste/tools/gennorm.c

  //----- Defines -------------------------------------------------------------
#define PI         3.14159265   // The value of pi

  //===========================================================================
  //=  Function to generate normally distributed random variable using the    =
  //=  Box-Muller method                                                      =
  //=    - Input: mean and standard deviation                                 =
  //=    - Output: Returns with normally distributed random variable          =
  //===========================================================================
  double norm(double mean, double std_dev)
  {
    double   u, r, theta;           // Variables for Box-Muller method
    double   x;                     // Normal(0, 1) rv
    double   norm_rv;               // The adjusted normal rv

    // Generate u
    u = 0.0;
    while (u == 0.0)
      u = rand_val(0);

    // Compute r
    r = sqrt(-2.0 * log(u));

    // Generate theta
    theta = 0.0;
    while (theta == 0.0)
      theta = 2.0 * PI * rand_val(0);

    // Generate x value
    x = r * cos(theta);

    // Adjust x value for specified mean and variance
    norm_rv = (x * std_dev) + mean;

    // Return the normally distributed RV value
    return(norm_rv);
  }

  double wrapped_norm(double mean, double std_dev) {
    double ret = norm(mean, std_dev);
    if (ret < 1) ret = 1;
    return ret;
  }

  //=========================================================================
  //= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
  //=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
  //=   - With x seeded to 1 the 10000th x value should be 1043618065       =
  //=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
  //=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
  //=========================================================================
  double rand_val(int seed)
  {
    const long  a =      16807;  // Multiplier
    const long  m = 2147483647;  // Modulus
    const long  q =     127773;  // m div a
    const long  r =       2836;  // m mod a
    static long x;               // Random int value
    long        x_div_q;         // x divided by q
    long        x_mod_q;         // x modulo q
    long        x_new;           // New x value

    // Set the seed if argument is non-zero and then return zero
    if (seed > 0)
    {
      x = seed;
      return(0.0);
    }

    // RNG using integer arithmetic
    x_div_q = x / q;
    x_mod_q = x % q;
    x_new = (a * x_mod_q) - (r * x_div_q);
    if (x_new > 0)
      x = x_new;
    else
      x = x_new + m;

    // Return a random value between 0.0 and 1.0
    return((double) x / m);
  }
} tool;

typedef struct req_rec_t {
  uint64_t LBA_;
  int blockNum_;
  char rw_;
  std::vector<std::string> data_;
  double compressity_;
} req_rec;

class TraceProcessor {
  public:
  std::map<uint64_t, std::vector<std::string>> blockData_;
  std::map<uint64_t, double> blockDataCompressity_;
  bool isHomes_ = false;
  int newBlockSize_;
  int newBits_;
  const uint64_t traceBits_ = 12;
  const uint64_t oldBits_ = 9;
  const int opsPerFile_ = 1024 * 1024 * 16; // 16M
  const std::string cc = "abc|abc|abc|abc|abc|abc|abc|abc|";

  std::string traceName_;

  FILE* writefd_ = NULL;
  int outputFileCounter_ = 0;
  int opsCurrent_ = 0;

  /**
   * Read into "blockData_" and "blockDataCompressity_"
   */
  void readInitialState(const char* filename) {
    FILE* tfd = fopen(filename, "r");
    if (tfd == nullptr) {
      printf("File not found: %s\n", filename);
      exit(-1);
    }

    uint64_t LBA;
    char s[100];

    int cnt = 0;
    int numBlocksM1 = (isHomes_) ? (1 << (newBits_ - oldBits_)) - 1 : (1 << (newBits_ - traceBits_)) - 1;

    while (fscanf(tfd, "%lu %s", &LBA, s) == 2) {
      cnt++;
      if (cnt % 100000 == 0) printf("cnt=%d, addr=%lu\n", cnt, LBA);

      blockData_[LBA] = {std::string(s)};  
      blockDataCompressity_[LBA] = tool.wrapped_norm(2, 0.25);
      for (int i = 0; i < numBlocksM1; i++) {
        fscanf(tfd, "%s", s);
        blockData_[LBA].push_back(std::string(s)); 
      }
    }
  }

  char* getFilePref(int selection) {
    assert(selection < 3);
    char const *sel[] = {
      "cheetah.cs.fiu.edu-110108-113008", 
      "webmail+online.cs.fiu.edu-110108-113008", 
      "homes-110108-112108"
    };
    char* s = (char*)malloc(sizeof(char) * 200);
    sprintf(s, "%s", sel[selection]);
    return s;
  }

  void writeMD5(FILE* wfd, char* data, int len) {
    char buf[33] = {'\0'};
    unsigned char md[20];
    char tmp[3] = {'\0'};
    MD5((unsigned char*)data, len, md);

    for (int i = 0; i < 16; i++) {
      sprintf(tmp,"%2.2x",md[i]);
      strcat(buf,tmp);
    }
    fprintf(wfd, "%s", buf);
  }

  void writeSHA1(FILE* wfd, char* data, int len) {
    char buf[33] = {'\0'};
    unsigned char sha1[23];
    char tmp[3] = {'\0'};
    SHA1((unsigned char*)data, len, sha1);

    for (int i = 0; i < 20; i++) {
      sprintf(tmp,"%2.2x",sha1[i]);
      strcat(buf,tmp);
    }
    fprintf(wfd, "%s", buf);
  }

  void outputLine(req_rec& req) {
    fprintf(writefd_, "%lu %d %c ", req.LBA_ * newBlockSize_, newBlockSize_ * req.blockNum_, req.rw_);  // address, length, read/write
    char* data32k = (char*)malloc(32 * req.data_.size() + 1000);
    int copylen = 0;
    double norm_rv; 

    for (int i = 0; i < req.data_.size(); i++) {
      sprintf(data32k + copylen, "%s", req.data_[i].c_str());
      copylen += 32;
    }
    writeSHA1(writefd_, data32k, copylen);
    free(data32k);
    data32k = NULL;

    fprintf(writefd_, " %f \n", req.compressity_);

    // Invalidate this variable
    req.LBA_ = -1;

    // Start a new file, if the previous is full
    opsCurrent_++;
    if (opsCurrent_ >= opsPerFile_) {
      outputFileCounter_++;
      fclose(writefd_);

      char filename[200];
      sprintf(filename, "%s_final_trace_%02d.txt", traceName_.c_str(), outputFileCounter_);
      writefd_ = fopen(filename, "w");
    }
  }

  void processOneFile(const char* trace_file, int index) {
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);

    FILE* tracefd = fopen(trace_file, "r");

    uint64_t tmp = 0;
    uint64_t maxAddr = 0;
    uint64_t lastAddr = -1LL;

    char procname[100], op_s[10], md5[40000];
    int fid, mjd, mnd;

    uint64_t foffset;
    uint64_t ts; 
    int len;

    req_rec lastReq;
    //int cnt = 0;

    lastReq.LBA_ = -1LL;

    int numBlocksInOldBlock = (isHomes_) ? (1 << (newBits_ - oldBits_)) : (1 << (newBits_ - traceBits_));
    int granularity = (isHomes_) ? 1 : (1 << (traceBits_ - 9));  // 9: 512B a block
    int newOffset, newLBA;

    int tested = 0;

    while (fscanf(tracefd, "%lu %d %s %lu %d %s %d %d %s", 
          &ts, &fid, procname, 
          &foffset, &len, op_s, 
          &mjd, &mnd, md5) > 0) 
    {
      // for "homes", some access length will be larger than 1 new block
      for (index = 0; index < len / granularity; index++) {
        newOffset = foffset + index * granularity;  // New offset: 512B a unit
        newLBA = newOffset >> (newBits_ - oldBits_); // new LBA: newBlockSize_ a unit

        // Initialize this address
        if (!blockData_.count(newLBA)) {
          blockData_[newLBA] = {};
          blockDataCompressity_[newLBA] = tool.wrapped_norm(2, 0.25);
          for (int i = 0; i < numBlocksInOldBlock; i++) {
            blockData_[newLBA].push_back(cc);  
          }
        }

        // Push the last address
        if (lastReq.LBA_ >= 0 && newLBA != lastReq.LBA_) {
          outputLine(lastReq);
        } 

        if (op_s[0] == 'W') {
          int i = newOffset % (1 << (newBits_ - oldBits_)) / ((1 << (newBits_ - oldBits_)) / numBlocksInOldBlock);
          // 32 is the length of MD5 value (32 characters)
          blockData_[newLBA][i] = std::string(md5 + index * 32, 32);
          blockDataCompressity_[newLBA] = tool.wrapped_norm(2, 0.25);
        }

        lastReq.LBA_ = newLBA;
        lastReq.blockNum_ = 1;
        lastReq.rw_ = op_s[0];
        lastReq.data_ = blockData_[newLBA];
        lastReq.compressity_ = blockDataCompressity_[newLBA];
      }
    }

    if (lastReq.LBA_ >= 0) outputLine(lastReq);
    gettimeofday(&tv2, NULL);
    printf("Finished modifying trace file %s, %.3lf s\n", trace_file, 
        (tv2.tv_sec - tv1.tv_sec + (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0));

    fclose(tracefd);
  }

  void processFiles(const int selection) {
    char* filePref = getFilePref(selection);
    char filename[200];

    outputFileCounter_ = 0;
    opsCurrent_ = 0;

    sprintf(filename, "%s_final_trace_%02d.txt", traceName_.c_str(), outputFileCounter_);
    writefd_ = fopen(filename, "w");

    char traceFile[200];
    for (int i = 1; i <= 21; i++) {
      sprintf(traceFile, "%s.%d.blkparse", filePref, i);
      processOneFile(traceFile, i);
    }

    fclose(writefd_);
    free(filePref);
  }

  void work(int argc, char** argv) {
    char* initialFileName;
    int selection = 0;

    if (argc < 4) {
      printf("Usage: %s \"homes\"|\"webvm\"|\"mail\" [initial state file name] [new block bits]", argv[0]);
      exit(1);
    }
    else {
      if (!strcmp(argv[1], "mail")) selection = 0;
      else if (!strcmp(argv[1], "webvm")) selection = 1;
      else if (!strcmp(argv[1], "homes")) selection = 2, isHomes_ = true;
      else {
        printf("Usage: %s \"homes\"|\"webvm\"|\"mail\" [initial state file name] [new block bits]", argv[0]);
        exit(1);
      }

      traceName_ = std::string(argv[1]);
      sscanf(argv[3], "%d", &newBits_);
      printf("newBits_ = %d\n", newBits_);
      newBlockSize_ = 1 << newBits_;
    }

    initialFileName = argv[2];

    readInitialState(initialFileName);
    printf("finish reading initial\n");

    processFiles(selection);
  }
};

int main(int argc, char** argv) {
  printf("Introduction: This program reads \n");
  printf("              1. The initial states in format: [LBA in 32KB] [data in 8 4KB-blocks]\n");
  printf("              2. Corresponding FIU traces\n");
  printf("              For the 2nd pass of traces processing. \n");
  printf("              The content format in output file will be: [logical address] [R/W bytes] [R/W] [SHA1 of block data]\n");

  tool.rand_val(123456);

  TraceProcessor traceProcessor;
  traceProcessor.work(argc, argv);

  return 0;
}
