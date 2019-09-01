#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<vector>
#include<map>
#include<string>
#include<utility>
#include<set>
#include<algorithm>
#include<openssl/md5.h>
#include<openssl/sha.h>

typedef long long LL;

using namespace std;

#define OLD_BLOCKSIZE 512LL
#define OLD_BITS 9
#define NEW_BLOCKSIZE 32768LL 
#define NEW_BITS 15


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

typedef struct req_rec_t {
	LL LBA;
	int blockNum;
  char rw;
  vector<string> data;
} req_rec;

map<LL, vector<string>> blockData;
// blockData: LBA -> data
//
// data[].first: false - first write, true - first read

bool isHomes = false;
void readInitial(const char* filename) {
  FILE* tfd = fopen(filename, "r");
  if (tfd == nullptr) {
    printf("File not found: %s\n", filename);
    exit(-1);
  }

  LL LBA;
  char s[100];

  int cnt = 0;
  int numBlocksM1 = (isHomes) ? 63 : 7;

  while (fscanf(tfd, "%lld %s", &LBA, s) == 2) {
    cnt++;
    /*
    if (cnt % 100000 == 0) printf("cnt=%d, addr=%lld\n", cnt, LBA);
    */
    blockData[LBA] = {string(s)};
    for (int i = 0; i < numBlocksM1; i++) {
      fscanf(tfd, "%s", s);
      blockData[LBA].push_back(string(s));
    }
  }
}

char* getMailFileName(int i) {
  char* s;
  s = (char*)malloc(sizeof(char) * 200);
  sprintf(s, "cheetah.cs.fiu.edu-110108-113008.%d.blkparse", i);
  if (i > 21) 
    sprintf(s, "cheetah.cs.fiu.edu-110108-113008.21.blkparse");
  return s;
}

char* getWebVMFileName(int i) {
  char* s;
  s = (char*)malloc(sizeof(char) * 200);
  sprintf(s, "webmail+online.cs.fiu.edu-110108-113008.%d.blkparse", i);
  if (i > 21) 
    sprintf(s, "webmail+online.cs.fiu.edu-110108-113008.21.blkparse");
  return s;
}

char* getHomesFileName(int i) {
  char* s;
  s = (char*)malloc(sizeof(char) * 200);
  sprintf(s, "homes-110108-112108.%d.blkparse", i);
  if (i > 21) 
    sprintf(s, "homes-110108-112108.21.blkparse");
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
  //SHA1((unsigned char*)data, len, sha1);
  SHA1((unsigned char*)data, len, sha1);

  for (int i = 0; i < 20; i++) {
    sprintf(tmp,"%2.2x",sha1[i]);
    strcat(buf,tmp);
  }
  fprintf(wfd, "%s", buf);
}

void outputLine(FILE* wfd, LL LBA, int blockNum, char rw, vector<string>& data) {
  fprintf(wfd, "%lld %d %c ", LBA * 32768LL, 32 * 1024 * blockNum, rw);
  // for (int i = 0; i < data.size(); i++) fprintf(wfd, "%s ", data[i].c_str()); 
  // fprintf(wfd, "\n");
}

void outputLine(FILE* wfd, req_rec& req) {
  outputLine(wfd, req.LBA, req.blockNum, req.rw, req.data);
  char data32k[3000];
  // TODO copy data to "data32k"
  int copylen = 0;
  double norm_rv; 

  for (int i = 0; i < req.data.size(); i++) {
    sprintf(data32k + copylen, "%s", req.data[i].c_str());
    copylen += req.data[i].length();
  }
  // TODO add MD5 output
  writeSHA1(wfd, data32k, copylen);
  norm_rv = norm(2, 0.25);

  fprintf(wfd, " %f \n", norm_rv);
  req.LBA = -1;
}

void outputResults(const char* trace_file, const char* pref, int index) {
  char filename[100]; 
  sprintf(filename, "%s_final_trace_%02d.txt", pref, index);
  FILE* writefd = fopen(filename, "w");
  FILE* tracefd = fopen(trace_file, "r");

  LL tmp = 0;
  LL maxAddr = 0;
  LL lastAddr = -1LL;
  string cc = "___|___|___|___|___|___|___|___|";
  //string cc = "abcdabcdabcdabcdabcdabcdabcdabcd";
  //
  char procname[100], op_s[10], md5[40000];
  int fid, mjd, mnd;

  LL foffset;
  LL ts; 
  int len;

  req_rec lastReq;
  int cnt = 0;

  lastReq.LBA = -1LL;

  int numBlocksInOldBlock = (isHomes) ? 64 : 8;
  int granularity = (isHomes) ? 1 : 8;
  int newOffset, newLBA;
  
	while (fscanf(tracefd, "%lld %d %s %lld %d %s %d %d %s", 
        &ts, &fid, procname, 
        &foffset, &len, op_s, 
        &mjd, &mnd, md5) > 0) 
  
  
  {
    for (index = 0; index < len / granularity; index++) {
      newOffset = foffset + index * granularity;
      newLBA = newOffset >> (NEW_BITS - OLD_BITS);

      // Initialize this address
      if (!blockData.count(newLBA)) {
        blockData[newLBA] = {};
        for (int i = 0; i < numBlocksInOldBlock; i++) blockData[newLBA].push_back(cc);
      }

      // Push the last address
      if (lastReq.LBA >= 0 && newLBA != lastReq.LBA) {
        outputLine(writefd, lastReq);
      } 

      if (op_s[0] == 'W') { 
        int i = newOffset % 64 / (64 / numBlocksInOldBlock);
        blockData[newLBA][i] = string(md5 + index * 32, 32);
      }

      lastReq.LBA = newLBA;
      lastReq.blockNum = 1;
      lastReq.rw = op_s[0];
      lastReq.data = blockData[newLBA];

    }
    /*
    cnt++;
    if (cnt == 1000000 || cnt % 5000000 == 0) printf("processed %d traces...\n", cnt);
    */

  }

  if (lastReq.LBA >= 0) outputLine(writefd, lastReq);
  printf("Finished modifying trace file %s\n", filename);

  fclose(writefd);
  fclose(tracefd);
}

int main(int argc, char** argv) {
  printf("Introduction: This program reads \n");
  printf("              1. The initial states in format: [LBA in 32KB] [data in 8 4KB-blocks]\n");
  printf("              2. Corresponding FIU traces\n");
  printf("              For the 2nd pass of traces processing. \n");
  printf("              The content format in output file will be: [logical address] [R/W bytes] [R/W] [SHA1 of block data]\n");

  int sel;

  if (argc < 3) {
    printf("Usage: %s \"homes\"|\"webvm\"|\"mail\" [initial state file name]", argv[0]);
    return 0;
  }
  else {
    if (!strcmp(argv[1], "mail")) sel = 0;
    else if (!strcmp(argv[1], "webvm")) sel = 1;
    else if (!strcmp(argv[1], "homes")) sel = 2, isHomes = true;
    else {
      printf("Usage: %s \"homes\"|\"webvm\"|\"mail\" [initial state file name]", argv[0]);
      return 0;
    }
  }
  readInitial(argv[2]);
  
  // Random number seed
  rand_val(123456);
  
  int cnt;

  if (argc >= 4) cnt = atoi(argv[3]);
  else {
    printf("a number between 1 and 21: ");
    scanf("%d", &cnt);
  }

  if (cnt > 21) cnt = 21;

  char* s;
  for (int i = 1; i <= cnt; i++) {
    if (sel == 1) {
      s = getWebVMFileName(i);
      outputResults(s, "webvm", i);
    }
    else if (sel == 0) {
      s = getMailFileName(i);
      outputResults(s, "mail", i);
    }
    else if (sel == 2) {
      s = getHomesFileName(i);
      outputResults(s, "homes", i);
    }
    free(s);
  }

  return 0;
}
