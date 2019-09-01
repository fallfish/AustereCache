#include<cstdio>
#include<chrono>
#include<cassert>
#include<cstring>
#include<cstdlib>
#include<vector>
#include<map>
#include<string>
#include<utility>
#include<set>
#include<algorithm>

typedef long long LL;

using namespace std;

#define OLD_BITS 9
#define NEW_BITS 15

#define SAME_DEV

map<pair<short, short>, set<LL>> addresses;
map<pair<short, short>, map<LL, pair<bool, string>>> data0;  // disabled now 
// data[].first: false - first write, true - first read

typedef chrono::high_resolution_clock Clock;
typedef chrono::milliseconds milliseconds;

bool isHomes = false;

void myTimer(bool start, const char* event = "") {
  static Clock::time_point ts;
  if (start) {
    ts = Clock::now();
  } else {
    Clock::time_point te = Clock::now();
    double duration2 = (chrono::duration_cast <milliseconds> (te - ts)).count() / 1024.0;
    printf("Elapse time (%s): %lf seconds\n", event, duration2);
  }
}


void readOp(const char* s) {
  FILE* tfd = fopen(s, "r");

  if (tfd == nullptr) {
    fprintf(stderr, "Cannot read trace file\n");
    exit(-1);
  }

  char procname[100], op_s[10], md5[100000];
  int fid, mjd, mnd;

  LL foffset;
  LL ts; 
  int len;

  int cnt = 0;

  // For loops
  int numLoops;
  int newOffset;

  myTimer(true, "readOp");

	while (fscanf(tfd, "%lld %d %s %lld %d %s %d %d %s", 
        &ts, &fid, procname, 
        &foffset, &len, op_s, 
        &mjd, &mnd, md5) > 0) {

#ifdef SAME_DEV
    mjd = 6, mnd = 0;
#endif

    pair<short, short> dev = {(short)mjd, (short)mnd};

    cnt++;
    /*
    if (isHomes) {if (cnt % 500000 == 0) printf("%d\n", cnt);}
    else {if (cnt == 1000000 || cnt % 10000000 == 0) {printf("%d\n", cnt); }}

    */

    if (isHomes)
      assert(strlen(md5) % 32 == 0 && strlen(md5) / 32 == len);
    else
      assert(strlen(md5) == 32 && foffset % 8 == 0);

    numLoops = (isHomes) ? len : 1;
    if (!data0.count(dev))
      data0[dev] = {};

    for (int i = 0; i < numLoops; i++) {
      newOffset = (isHomes) ? foffset + i : foffset;

      addresses[dev].insert(newOffset);
      if (!data0[dev].count(newOffset))
      {
        data0[dev][newOffset] = {false, ""}; // new pair<bool, string>;
        if (op_s[0] == 'R')
        {
          data0[dev][newOffset].first = true;
          data0[dev][newOffset].second = string((char*)(md5) + i * 32, 32);
        }
      }
    }
  }

  fclose(tfd);

  myTimer(false, s);
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

void outputResults(const char* pref) {
  char filename[100]; 
  sprintf(filename, "%s_initial_state.txt", pref);
  FILE* writefd = fopen(filename, "w");

  LL tmp = 0;
  LL maxAddr = 0;
  LL last_oLBA = -1LL;
  string cc = "___|___|___|___|___|___|___|___|";
  //string cc = "abcdabcdabcdabcdabcdabcdabcdabcd";

  int cnt = 0;

  LL oLBAbase = 0;

  int delta = (isHomes) ? 1 : 8;
  int outputDelta = (isHomes) ? 1000000 : 500000;

  for (map<pair<short, short>, set<LL>>::iterator iter0 = addresses.begin(); iter0 != addresses.end(); iter0++) {
    map<LL, pair<bool, string>> data = data0[iter0->first];
//    oLBAbase = oLBAbases[iter0->first];

    for (set<LL>::iterator iter = iter0->second.begin(); iter != iter0->second.end(); iter++) {

      LL oLBA = *iter + oLBAbase;
      pair<short, string> t = data[*iter];

      if (t.first) {

        if (cnt % outputDelta == 0) printf("cnt = %d, addr = %lld\n", cnt, oLBA);
        cnt++;

        LL align_oLBA = oLBA / 64 * 64;

        // 1. print the unspecified data before oLBA
        if (last_oLBA < 0) {
          fprintf(writefd, "%lld ", align_oLBA >> (NEW_BITS - OLD_BITS));
          while (align_oLBA < oLBA) {
            fprintf(writefd, "%s ", cc.c_str());
            align_oLBA += delta;
          }
        }
        else if (last_oLBA / 64 * 64 == oLBA / 64 * 64) {
          last_oLBA += delta;
          while (last_oLBA < oLBA) {
            fprintf(writefd, "%s ", cc.c_str()), last_oLBA += delta;
          }
        }
        else {
          last_oLBA += delta;
          while (last_oLBA % 64) {
            fprintf(writefd, "%s ", cc.c_str()), last_oLBA += delta;
          }
          fprintf(writefd, "\n%lld ", align_oLBA >> (NEW_BITS - OLD_BITS));

          while (align_oLBA < oLBA) {
            fprintf(writefd, "%s ", cc.c_str());
            align_oLBA += delta;
          }
        }

        // 2. print the data on oLBA
        fprintf(writefd, "%s ", t.second.c_str());

        // 3. update last_oLBA
        if (oLBA % 64 == 64 - delta) {
          last_oLBA = -1LL;
          fprintf(writefd, "\n");
        }
        else last_oLBA = oLBA;
      }
    }
  }

  if (last_oLBA >= 0) {
    last_oLBA += delta;
    while (last_oLBA % 64) {
      fprintf(writefd, "%s ", cc.c_str()), last_oLBA += delta;
    }
    fprintf(writefd, "\n");
  }

  fclose(writefd);
}

int main(int argc, char** argv) {
  setbuf(stderr, NULL);
  setbuf(stdout, NULL);

  printf("Introduction: This program reads the FIU traces and calculate the initial state of the hard disk.\n");
  printf("              The output file will be \"webvm_initial_state.txt\" or \"mail_initial_state.txt\".\n");
  printf("              Output file format: [LBA in 32KB] [data in 8 4KB-blocks]\n");

  int sel;
  if (argc < 2) {
    printf("Usage: %s \"homes\"|\"webvm\"|\"mail\"", argv[0]);
    return 0;
  }
  else {
    if (!strcmp(argv[1], "mail")) sel = 0;
    else if (!strcmp(argv[1], "webvm")) sel = 1;
    else if (!strcmp(argv[1], "homes")) sel = 2, isHomes = true;
    else return 0;
  }

  int cnt;

  if (argc >= 3) 
    cnt = atoi(argv[2]);
  else {
    printf("a number between 1 and 21: ");
    scanf("%d", &cnt);
  }

  if (cnt > 21) cnt = 21;
  
  char* s;
  for (int i = 1; i <= cnt; i++) {
    if (sel == 1) 
      s = getWebVMFileName(i);
    else if (sel == 0)
      s = getMailFileName(i);
    else if (sel == 2)
      s = getHomesFileName(i);
    readOp(s);
    free(s);
  }

  if (sel == 1) outputResults("webvm");
  else if (sel == 0) outputResults("mail");
  else if (sel == 2) outputResults("homes");
  
  return 0;
}
