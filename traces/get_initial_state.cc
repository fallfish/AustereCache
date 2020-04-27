/**
 * get initial states of the disk:
 *     Now we search all the addresses in the trace, and find the addresses whose first operation
 * is a read, finally align this address and print it into "*_initail_state.txt".
 *
 */ 

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


class InitialStateGenerator {
 private:

  const int oldBits_ = 9;
  int newBits_;
  std::set<uint64_t> addresses_;
  std::map<uint64_t, std::pair<bool, std::string>> data_;  // disabled now 
  // data_.first: 
  //    false - first op is write 
  //    true - first op is read
  
  std::string traceName_;
  bool isHomes_ = false;

  typedef std::chrono::high_resolution_clock Clock;
  typedef std::chrono::milliseconds milliseconds;

  void myTimer(bool start, const char* event = "") {
    static Clock::time_point ts;
    if (start) {
      ts = Clock::now();
    } else {
      Clock::time_point te = Clock::now();
      double duration2 = (std::chrono::duration_cast <milliseconds> (te - ts)).count() / 1024.0;
      printf("Elapse time (%s): %lf seconds\n", event, duration2);
    }
  }


  void processOneFile(const char* s) {
    FILE* tfd = fopen(s, "r");

    if (tfd == nullptr) {
      fprintf(stderr, "Cannot read trace file\n");
      exit(-1);
    }

    char procname[100], op_s[10], md5[100000];
    int fid, mjd, mnd;

    uint64_t foffset;
    uint64_t ts; 
    int len;

    // For loops
    int numLoops;
    int newOffset;

    myTimer(true, "processOneFile");

    while (fscanf(tfd, "%lu %d %s %lu %d %s %d %d %s", 
          &ts, &fid, procname, 
          &foffset, &len, op_s, 
          &mjd, &mnd, md5) > 0) {

      if (isHomes_)
        assert(strlen(md5) % 32 == 0 && strlen(md5) / 32 == len);
      else
        assert(strlen(md5) == 32 && foffset % 8 == 0);

      numLoops = (isHomes_) ? len : 1;

      for (int i = 0; i < numLoops; i++) {
        newOffset = (isHomes_) ? foffset + i : foffset;

        addresses_.insert(newOffset);
        if (!data_.count(newOffset)) {
          data_[newOffset] = {false, ""}; 

          // If it is write, ignore the data value
          if (op_s[0] == 'R') {
            data_[newOffset].first = true;
            data_[newOffset].second = std::string((char*)(md5) + i * 32, 32);
          }
        }
      }
    }

    fclose(tfd);

    myTimer(false, s);
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

  void outputResults() {
    char filename[100]; 
    sprintf(filename, "%s_initial_state.txt", traceName_.c_str());
    FILE* writefd = fopen(filename, "w");

    uint64_t tmp = 0;
    uint64_t maxAddr = 0;
    uint64_t last_oLBA = -1LL;  // a negative number specifies a new line
    std::string cc = "abc|abc|abc|abc|abc|abc|abc|abc|";

    int cnt = 0;

    int delta = (isHomes_) ? 1 : 8;
    int outputDelta = (isHomes_) ? 1000000 : 500000;

    for (std::set<uint64_t>::iterator iter = addresses_.begin(); iter != addresses_.end(); iter++) {
      uint64_t oLBA = *iter;
      std::pair<short, std::string> t = data_[*iter];

      // Print only if it's first operation is a read 
      if (t.first) { 
        if (cnt % outputDelta == 0) printf("cnt = %d, addr = %lu\n", cnt, oLBA);
        cnt++;

        // Align with the new LBA, but use the old LBA unit (512B)
        uint64_t align_oLBA = oLBA / (1 << (newBits_ - oldBits_)) * (1 << (newBits_ - oldBits_));

        // 1. print the unspecified data before oLBA
        if (last_oLBA < 0) { // New line
          fprintf(writefd, "%lu ", align_oLBA >> (newBits_ - oldBits_));
          while (align_oLBA < oLBA) {
            fprintf(writefd, "%s ", cc.c_str());
            align_oLBA += delta;
          }
        } else if (last_oLBA / (1 << (newBits_ - oldBits_)) * (1 << (newBits_ - oldBits_)) == 
            oLBA / (1 << (newBits_ - oldBits_)) * (1 << (newBits_ - oldBits_))) {
          last_oLBA += delta;
          while (last_oLBA < oLBA) {
            fprintf(writefd, "%s ", cc.c_str()), last_oLBA += delta;
          }
        } else {
          last_oLBA += delta;
          while (last_oLBA % (1 << (newBits_ - oldBits_))) {
            fprintf(writefd, "%s ", cc.c_str()), last_oLBA += delta;
          }
          fprintf(writefd, "\n%lu ", align_oLBA >> (newBits_ - oldBits_));

          while (align_oLBA < oLBA) {
            fprintf(writefd, "%s ", cc.c_str());
            align_oLBA += delta;
          }
        }

        // 2. print the data on oLBA
        fprintf(writefd, "%s ", t.second.c_str());

        // 3. update last_oLBA
        if (oLBA % (1 << (newBits_ - oldBits_)) == (1 << (newBits_ - oldBits_)) - delta) {
          last_oLBA = -1LL;
          fprintf(writefd, "\n");
        } else last_oLBA = oLBA;
      }
    }

    if (last_oLBA >= 0) {
      last_oLBA += delta;
      while (last_oLBA % (1 << (newBits_ - oldBits_))) {
        fprintf(writefd, "%s ", cc.c_str()), last_oLBA += delta;
      }
      fprintf(writefd, "\n");
    }

    fclose(writefd);
  }

  void processFiles(const int selection) {
    char filename[200];

    for (int i = 1; i <= 21; i++) {
      sprintf(filename, "%s.%d.blkparse", getFilePref(selection), i);
      processOneFile(filename);
    }
  }

 public:

  void work(int argc, char** argv) {
    int selection;
    if (argc < 3) {
      printf("Usage: %s \"homes\"|\"webvm\"|\"mail\" [number of traces] [bits]", argv[0]);
      exit(1);
    }
    else {
      if (!strcmp(argv[1], "mail")) {
        selection = 0;
      } else if (!strcmp(argv[1], "webvm")) {
        selection = 1;
      } else if (!strcmp(argv[1], "homes")) {
        selection = 2, isHomes_ = true;
      } else {
        exit(1);
      }

      traceName_ = std::string(argv[1]);
      sscanf(argv[2], "%d", &newBits_);
    }

    processFiles(selection);
    outputResults();
  }
};

int main(int argc, char** argv) {
  printf("Introduction: This program reads the FIU traces and calculate the initial state of the hard disk.\n");
  printf("              The output file will be \"<trace_name>_initial_state.txt\".\n");

  InitialStateGenerator isGenerator;
  isGenerator.work(argc, argv);
  return 0;
}
