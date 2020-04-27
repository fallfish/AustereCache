#include<cstdio>
#include<cstring>
#include<cassert>
#include<ctime>
#include<cstdlib>
#include<vector>
#include<map>
#include<set>
#include<cmath>
#include<utility>

uint64_t MaxAddr = 1LL * 1024 * 1024 * 1024;
uint64_t WorkingSetSize = 256LL * 1024 * 1024;
uint64_t traceLength = 1024LL;

uint64_t BlockSize = 32LL * 1024;
double IoDupRatio = 0.5;
double DupRatio = 2.0;
double rwRatio = 1.0;
char filename[200] = "tmp.txt";
double HOT_RATIO = 0.9;

// Only for selection 2
uint64_t PageSize = 256LL * 1024;
uint64_t BlockPerPage = PageSize / BlockSize;

uint64_t numData;
uint64_t numLBAs, maxLBA;
uint64_t numPages, maxPages;

uint64_t* lbaSet;

// 0: Choose from a limited data set
// 1: Following the IoDupRatio, and forget the limited data set
// 2: Choose totally random data
int DataFillMode = 1;

// 0: Uniform
// 1: 10% hot (90% access), 90% cold (10% access)
// 2: Sequential (WorkingSetSize sequential)
int Selection = 0;

std::vector<char*> data;
std::vector<double> compressibility;

std::map<uint64_t, char*> lba2data;
std::map<uint64_t, double> lba2Compressibility;

// from www.csee.usf.edu/~kchriste/tools/gennorm.c

//----- Defines -------------------------------------------------------------
#define PI         3.14159265   // The value of pi
//----- Function prototypes -------------------------------------------------
double rand_val(int seed);                 // Jain's RNG

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
  //  if (ret < 1) ret = 1;
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

class TraceGenerator {
  private:
  struct req_t {
    int lbaSetIndex;
    bool isRead;
  };
  std::vector<req_t> reqs;
  double currentIoDedup_;  // (ReadHitDup + WriteHit) / (ReadMiss + Write);

  int numRead_, numWrite_;
  int numReadMiss_;
  int numRequests_;
  int numReadMissDup_;
  int numWriteDup_;

  public:
  void usage() {
    printf("example: ./generate --wss 1048576 --trace_length 100\n");
    printf("parameters:\n");
    printf("  --max_addr: maximum address (Disk size) in bytes (default: %lu)\n", MaxAddr);
    printf("  --wss: working set size in bytes (default: %lu)\n", WorkingSetSize);
    printf("  --block_size: block size in bytes (default: %lu)\n", BlockSize);
    printf("  --page_size: page size in bytes, only for selection 2 (default: %lu)\n", PageSize);
    printf("  --selection: access pattern. only 0 (uniform), 2 (page sequential)\n");
    printf("  --dup_ratio: duplication ratio (default: %.6lf)\n", DupRatio);
    printf("  --trace_length: number of I/Os (default: %lu)\n", traceLength);
    printf("  --rw_ratio: Read-to-write ratio (default: %.6lf)\n", rwRatio);
    printf("  --filename: output file name (default: %s)\n", filename);
  }

  void parse(int argc, char** argv) {
    if (argc <= 1) usage();
    for (int i = 1; i < argc - 1; i+=2) {
      if (!strcmp(argv[i], "--max_addr")) {
        sscanf(argv[i+1], "%lu", &MaxAddr);
      } else if (!strcmp(argv[i], "--wss")) {
        sscanf(argv[i+1], "%lu", &WorkingSetSize);
      } else if (!strcmp(argv[i], "--block_size")) {
        sscanf(argv[i+1], "%lu", &BlockSize);
      } else if (!strcmp(argv[i], "--page_size")) {
        sscanf(argv[i+1], "%lu", &PageSize);
        printf(" + page size: %lu\n", PageSize);
      } else if (!strcmp(argv[i], "--selection")) {
        sscanf(argv[i+1], "%d", &Selection);
      } else if (!strcmp(argv[i], "--dup_ratio")) {
        sscanf(argv[i+1], "%lf", &DupRatio);
        if (DupRatio < 1.0) {
          //        DupRatio = 1.0;  // Cheating mode
          printf("Warning: dup_ratio should be larger than 1.0\n");
        }
        printf(" + dup ratio: %.2lf\n", DupRatio);
      } else if (!strcmp(argv[i], "--trace_length")) {
        sscanf(argv[i+1], "%lu", &traceLength);
      } else if (!strcmp(argv[i], "--io_dup_ratio")) {
        sscanf(argv[i+1], "%lf", &IoDupRatio);
        printf(" + IoDupRatio: %.2lf\n", IoDupRatio);
        if (IoDupRatio > 1.0 || IoDupRatio < 0.0) {
          printf("Warning: dup_ratio should be between than 0 and 1\n");
        }
      } else if (!strcmp(argv[i], "--rw_ratio")) {
        sscanf(argv[i+1], "%lf", &rwRatio);
        printf(" + read-to-write ratio: %.2lf\n", rwRatio);
      } else if (!strcmp(argv[i], "--filename")) {
        strcpy(filename, argv[i+1]);
        printf(" + filename: %s\n", filename);
      } else {
        printf("[Warning] Unrecognized option: %s\n", argv[i]);
      }
    }

    if (Selection == 1 && (uint64_t)(WorkingSetSize / BlockSize * (1 - HOT_RATIO)) <= 0) {
      printf("[error] WorkingSetSize too small. No cold files.\n");
      exit(1);
    }
    MaxAddr = (MaxAddr + BlockSize - 1) * BlockSize / BlockSize;
    WorkingSetSize = (WorkingSetSize + BlockSize - 1) * BlockSize / BlockSize;
    if (MaxAddr < WorkingSetSize) MaxAddr = WorkingSetSize + BlockSize;
    if (Selection != 2) {
      PageSize = BlockSize;
    } 
    BlockPerPage = PageSize / BlockSize;

    // Check
    double totalIO = (double)traceLength * BlockSize / 1024 / 1024 / 1024 / 1024;
    if (totalIO >= 1.0) printf("Warning: you are generating %.2lf TiB IOs. Is it too large?\n", totalIO);
    double wssGB = (double)WorkingSetSize / 1024 / 1024 / 1024;
    printf(" - Disk size: %.2lf GiB\n", (double)MaxAddr / 1024 / 1024 / 1024);
    printf(" - Working set size: %.2lf GiB\n", wssGB);
    printf(" - Total IO: %.2lf GiB\n", (double)traceLength * BlockSize / 1024 / 1024 / 1024);
    if (wssGB >= 100) printf("[Warning] you are using a working set (%.2lf GiB) larger than 100 GiB. Is it too large?\n", wssGB);
  }

  char* randomData(int length) {
    char* s = (char*)malloc(sizeof(char)*(length+2));
    for (int i = 0; i < length; i++) {
      int t = rand() % 16;
      if (t < 10) {
        s[i] = t + '0';
      } else {
        s[i] = t - 10 + 'a';
      } 
    }
    s[length] = '\0';
    return s;
  }

  bool randomReadWrite() {
    return (rand() > RAND_MAX / (1 + rwRatio));
  }

  void release_data() {
    for (int i = 0; i < (int)data.size(); i++) {
      free(data[i]);
    }
  }

  uint64_t* pickRandomKFromN(uint64_t k, uint64_t n) {
    uint64_t *full_set = (uint64_t*)malloc(sizeof(uint64_t) * n);
    uint64_t *randSet;
    uint64_t r;

    for (uint64_t i = 0; i < n; i++) {
      full_set[i] = i;
    }

    for (uint64_t i = 0; i < k; i++) {
      r = rand() % (n - i) + i;
      uint64_t tmp = full_set[r];
      full_set[r] = full_set[i];
      full_set[i] = tmp;
    }

    randSet = (uint64_t*)malloc(sizeof(uint64_t) * k);
    memcpy(randSet, full_set, sizeof(uint64_t) * k);
    free(full_set);

    return randSet;
  }

  int* shuffleIntPointers(int len) {
    int* index = (int*)malloc(sizeof(int) * len);
    int tmp;
    for (int i = 0; i < len; i++) {
      index[i] = i;
    }
    for (int i = 0; i < len - 1; i++) {
      tmp = rand() % (len - i) + i;
      if (i != tmp) {
        int t = index[tmp];
        index[tmp] = index[i];
        index[i] = t;
      }
    }
    return index;
  }

  // Select the smallest index, that a[index] >= key
  int binarySearchDouble(double* a, int len, double key) {
    int left = 0, right = len - 1;
    while (right - left > 1) {
      int mid = (left + right) / 2;
      if (a[mid] == key) return mid;
      if (a[mid] > key) right = mid;
      else left = mid;
    }
    return left;
  }

  int randomProbabilities(double* cumulate, int len, double maxP) {
    double position = (double)rand() / RAND_MAX * maxP;
    return binarySearchDouble(cumulate, len, position);
  }

  bool randomProbability(double p) {
    //  double cumulate[2];
    //  cumulate[0] = p;
    //  cumulate[1] = 1.0;
    //  return randomProbabilities(cumulate, 1) == 0;
    if (p >= 1) return true;
    return (rand() < RAND_MAX * p);
  }

  void writeDataToLba(uint64_t lba, bool readMiss = false) {
    int index = rand() % numData;
    static std::set<char*> usedData;
    static std::map<char*, int> usedData2ReferenceCount;
    static int usedDataPointerMax = 0;

    if (DataFillMode == 0) {
      lba2data[lba] = data[index];
      lba2Compressibility[lba] = compressibility[index];
    } else if (DataFillMode == 1) {
      char* oldData = NULL; 

      if (lba2data.count(lba)) {
        oldData = lba2data[lba];
        assert(usedData.find(oldData) != usedData.end());
        assert(usedData2ReferenceCount.count(oldData) && usedData2ReferenceCount[oldData] > 0);

        usedData2ReferenceCount[oldData] --;
        if (usedData2ReferenceCount[oldData] == 0) {
          usedData.erase(oldData);
          usedData2ReferenceCount.erase(oldData);
        }
      }

      char* newData;
      double newCompressibility;
      int usedDataPointer;
      if (numRequests_ >= 100 && currentIoDedup_ < IoDupRatio) {  // Write duplicates 
        usedDataPointer = rand() % usedDataPointerMax;
        newData = data[usedDataPointer];
        newCompressibility = compressibility[usedDataPointer];

        while(usedData.find(newData) == usedData.end()) newData = data[rand() % usedDataPointerMax];  // TODO may have some performance loss
        lba2data[lba] = newData;

        // Statistics
        if (readMiss) numReadMissDup_++;
        else numWriteDup_++;
      } else {  // IO not duplicate, write a new data
        newData = data[usedDataPointerMax];
        newCompressibility = compressibility[usedDataPointerMax];
        assert(usedData.find(newData) == usedData.end());
        assert(!usedData2ReferenceCount.count(newData));

        usedDataPointerMax++;
      }
      usedData.insert(newData);
      if (!usedData2ReferenceCount.count(newData)) {
        usedData2ReferenceCount[newData] = 1;
      } else {
        usedData2ReferenceCount[newData]++;
      }

      lba2data[lba] = newData; // Add new data
      lba2Compressibility[lba] = newCompressibility;
    } else {
      lba2data[lba] = randomData(40);
      lba2Compressibility[lba] = wrapped_norm(2, 0.25);
    }
    if (lba2Compressibility[lba] < 1) lba2Compressibility[lba] = 1;
  }

  void generateData() {
    // 0. Generate Data
    numData = (uint64_t)WorkingSetSize / BlockSize * DupRatio;
    if (DataFillMode == 1) numData = traceLength;

    for (int i = 0; i < numData; i++) {
      data.push_back(randomData(40));
      compressibility.push_back(wrapped_norm(2, 0.25));
    }

    numLBAs = (uint64_t)(WorkingSetSize / BlockSize);
    maxLBA = MaxAddr / BlockSize;
    numPages = (uint64_t)(WorkingSetSize / PageSize);
    maxPages = MaxAddr / PageSize;

    // 1. Generate LBAs
    uint64_t* randSet = pickRandomKFromN(numPages, maxPages);
    lbaSet = (uint64_t*)malloc(sizeof(uint64_t) * numLBAs);

    for (uint64_t i = 0; i < numPages; i++) {
      uint64_t start = i * BlockPerPage;
      for (uint64_t j = 0; j < BlockPerPage; j++) {
        lbaSet[j + start] = randSet[i] * BlockPerPage + j;
      }
    }

    if (DataFillMode != 1) {
      // 2. Put data into "data"
      for (int i = 0; i < numLBAs; i++) {
        this->writeDataToLba(lbaSet[i]);
      }
    }
    printf("Generated data and lbas\n");
  }

  // Selection 1: Hot-Cold
  void workHotCold() {
    FILE* fp;
    fp = fopen(filename, "w");
    uint64_t lba;
    int numH = 0, numC = 0;
    bool read, hotIO, hotFile;
    int index;
    uint64_t hotIOBase = numData * (1 - HOT_RATIO);

    for (int i = 0; i < traceLength; i++) {
      read = randomReadWrite();
      hotIO = randomProbability(HOT_RATIO);

      if (hotIO) {
        index = rand() % hotIOBase;
        numH++;
      } else {
        index = hotIOBase + rand() % (numData - hotIOBase);
        numC++;
      }
      lba = lbaSet[index];

      if (read) {
        fprintf(fp, "%lu %lu R %s %.6lf\n", lba * BlockSize, BlockSize, lba2data[lba], lba2Compressibility[lba]);
        numRead_++;
      } else {
        this->writeDataToLba(lba);

        fprintf(fp, "%lu %lu W %s %.6lf\n", lba * BlockSize, BlockSize, lba2data[lba], lba2Compressibility[lba]);
        numWrite_++;
      }
    }

    printf("Read : Write = %d : %d\n", numRead_, numWrite_);
    printf("Hot : Cold = %d : %d\n", numH, numC);
    printf("Trace at %s\n", filename);
    fclose(fp);
  }

  // Selection 0 or 2: Uniform Sequential 
  void workUniformSequential() {
    FILE* fp;
    fp = fopen(filename, "w");
    char* used = (char*)malloc(sizeof(char) * numLBAs);

    int numReadMiss_ = 0;
    memset(used, 0, sizeof(char) * numLBAs);

    // 1. Generate access pattern
    for (int i = 0; i < traceLength; i += BlockPerPage) {
      bool isRead = randomReadWrite();
      int lbaSetIndex = rand() % numPages * BlockPerPage;

      if (isRead) {
        for (int j = 0; j < BlockPerPage; j++) {
          reqs.push_back({lbaSetIndex, isRead});
          if (!used[lbaSetIndex]) numReadMiss_++;
          used[lbaSetIndex] = 1;
          lbaSetIndex++;
        }
        numRead_ += BlockPerPage;
      } else {
        for (int j = 0; j < BlockPerPage; j++) {
          reqs.push_back({lbaSetIndex, isRead});
          used[lbaSetIndex] = 1;
          lbaSetIndex++;
        }
        numWrite_ += BlockPerPage;
      }
    }

    // 2. Generate data into the trace
    memset(used, 0, sizeof(char) * numLBAs);
    for (int i = 0; i < (int)reqs.size(); i++) {
      int lbaSetIndex = reqs[i].lbaSetIndex;
      uint64_t lba = lbaSet[lbaSetIndex];
      if (reqs[i].isRead) {
        if (!used[lbaSetIndex]) this->writeDataToLba(lba);
        fprintf(fp, "%lu %lu R %s %.6lf\n", lba * BlockSize, BlockSize, 
            lba2data[lba], lba2Compressibility[lba]);

      } else {
        this->writeDataToLba(lba);
        fprintf(fp, "%lu %lu W %s %.6lf\n", lba * BlockSize, BlockSize, 
            lba2data[lba], lba2Compressibility[lba]);
      }
      used[lbaSetIndex] = 1;
    }

    printf("Read : Write = %d : %d\n", numRead_, numWrite_);
    printf("Trace at %s\n", filename);
    fclose(fp);
  }

  /**
   * Generate a zipf workload.
   */
  void workZipf(int s) {
    s = 1;
    double* cumulate;
    double maxCumulate;
    int* readZipfPointer = shuffleIntPointers(numPages);   // lbaSet[readZipfPointer[0] * BlockPerPage] is the read-hottest
    int* writeZipfPointer = shuffleIntPointers(numPages);  // lbaSet[writeZipfPointer[0] * BlockPerPage] is the write-hottest

    cumulate = (double*)malloc(sizeof(double) * (numPages + 1));
    cumulate[0] = 0.0;
    for (int i = 1; i <= numPages; i++) {
      cumulate[i] = cumulate[i - 1] + (double)1.0 / i; 
    }
    maxCumulate = cumulate[numPages];

    // 0. Same as workUniformSequential
    FILE* fp;
    fp = fopen(filename, "w");
    char* used = (char*)malloc(sizeof(char) * numLBAs);

    numRead_ = numWrite_ = 0;
    numReadMiss_ = 0;
    memset(used, 0, sizeof(char) * numLBAs);

    // 1. Generate access pattern
    for (int i = 0; i < traceLength; i += BlockPerPage) {
      bool isRead = randomReadWrite();

      int selectedPointer = randomProbabilities(cumulate, numPages + 1, maxCumulate);
      assert(selectedPointer >= 0 && selectedPointer < numPages);

      int lbaSetIndex;
      if (isRead) lbaSetIndex = readZipfPointer[selectedPointer] * BlockPerPage;
      else lbaSetIndex = writeZipfPointer[selectedPointer] * BlockPerPage;

      if (isRead) {
        for (int j = 0; j < BlockPerPage; j++) {
          reqs.push_back({lbaSetIndex, isRead});
          if (!used[lbaSetIndex]) numReadMiss_++;
          used[lbaSetIndex] = 1;
          lbaSetIndex++;
        }
        numRead_ += BlockPerPage;
      } else {
        for (int j = 0; j < BlockPerPage; j++) {
          reqs.push_back({lbaSetIndex, isRead});
          used[lbaSetIndex] = 1;
          lbaSetIndex++;
        }
        numWrite_ += BlockPerPage;
      }
    }
    printf("Read : Write = %d : %d\n", numRead_, numWrite_);

    // 2. Generate data into the trace
    // These variables are only useful for DataFillMode == 1
    currentIoDedup_ = 0.0;
    numReadMissDup_ = 0;
    numWriteDup_ = 0;
    numReadMiss_ = 0;
    numWrite_ = 0;

    memset(used, 0, sizeof(char) * numLBAs);
    lba2data.clear();

    for (numRequests_ = 0; numRequests_ < (int)reqs.size(); numRequests_ ++) {
      int lbaSetIndex = reqs[numRequests_].lbaSetIndex;
      uint64_t lba = lbaSet[lbaSetIndex];
      if (reqs[numRequests_].isRead) {  // Read
        if (!used[lbaSetIndex]) {
          numReadMiss_++;
          this->writeDataToLba(lba, true);
        }
        fprintf(fp, "%lu %lu R %s %.6lf\n", lba * BlockSize, BlockSize, 
            lba2data[lba], lba2Compressibility[lba]);
      } else {  // Write
        this->writeDataToLba(lba, false);
        fprintf(fp, "%lu %lu W %s %.6lf\n", lba * BlockSize, BlockSize, 
            lba2data[lba], lba2Compressibility[lba]);
        numWrite_++;
      }
      used[lbaSetIndex] = 1;

      currentIoDedup_ = (double)(numReadMissDup_ + numWriteDup_) / (numReadMiss_ + numWrite_);
    }
    printf("Current IO Dedup = %.3lf = (%d + %d) / (%d + %d)\n", currentIoDedup_, numReadMissDup_, numWriteDup_, numReadMiss_, numWrite_);

    printf("Trace at %s\n", filename);
    fclose(fp);

    free(cumulate);
  }
};

int main(int argc, char** argv) {
  srand(time(NULL));
  rand_val(123456);

  TraceGenerator traceGenerator;

  traceGenerator.parse(argc, argv);
  traceGenerator.generateData();

  if (Selection == 1) {
    traceGenerator.workHotCold();
  } else if (Selection == 0 || Selection == 2) {
    traceGenerator.workUniformSequential();
  } else if (Selection == 3) {
    traceGenerator.workZipf(1);
  }

  traceGenerator.release_data();
  return 0;
  
}
