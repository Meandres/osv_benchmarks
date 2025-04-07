// sudo apt install libtbb-dev
// g++ -O3 -g mmapbench.cpp -o mmapbench -ltbb -pthread

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <osv/ucache.hh>

using namespace std;

#define check(expr)                                                            \
  if (!(expr)) {                                                               \
    perror(#expr);                                                             \
    throw;                                                                     \
  }

int stick_this_thread_to_core(int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

double gettime() {
  struct timeval now_tv;
  gettimeofday(&now_tv, NULL);
  return ((double)now_tv.tv_sec) + ((double)now_tv.tv_usec) / 1000000.0;
}

int main(int argc, char **argv) {
  if (argc != 5) {
    cerr << "dev threads seq pageSize" << endl;
    return 1;
  }

  unsigned threads = atoi(argv[2]);
  uint64_t fileSize = 2ull * 1024 * 1024 * 1024 * 1024;

  ucache::createCache(100l << 30, 64);
  char *p = (char *)ucache::uCacheManager->addVMA(fileSize, atoi(argv[4]));

  int hint = 0;

  int seq = atoi(argv[3]);

  struct atomic_u64_padded{
    std::atomic<uint64_t> val;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
  };

  std::vector<atomic_u64_padded> counts(threads);
  std::vector<atomic_u64_padded> sums(threads);
  atomic<uint64_t> seqScanPos(0);

  vector<thread> t;
  for (unsigned i = 0; i < threads; i++) {
    t.emplace_back([&, i]() {
      stick_this_thread_to_core(i);
      atomic<uint64_t> &count = counts[i].val;
      atomic<uint64_t> &sum = sums[i].val;

      count = 0;
      sum = 0;

      if (seq) {
        while (true) {
          uint64_t scanBlock = 128 * 1024 * 1024;
          uint64_t pos = (seqScanPos += scanBlock) % fileSize;

          for (uint64_t j = 0; j < scanBlock; j += 4096) {
            sum += p[pos + j];
            count++;
          }
        }
      } else {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> rnd(0, fileSize);

        while (true) {
          sum += p[rnd(gen)];
          count++;
        }
      }
    });
  }

  atomic<uint64_t> cpuWork(0);
  t.emplace_back([&]() {
    while (true) {
      double x = cpuWork.load();
      for (uint64_t r = 0; r < 10000; r++) {
        x = exp(log(x));
      }
      cpuWork++;
    }
  });

  cout << "dev,seq,hint,threads,time,workGB,tlb,readGB,CPUwork" << endl;
  double start = gettime();
  while (true) {
    sleep(1);
    uint64_t shootdowns = ucache::uCacheManager->tlbFlush.exchange(0);
    uint64_t IObytes = ucache::uCacheManager->readSize.exchange(0);
    uint64_t workCount = 0;
    for (auto &x : counts)
      workCount += x.val.exchange(0);
    double t = gettime() - start;
    cout << argv[1] << "," << seq << "," << hint << "," << threads << "," << t
         << "," << (workCount * 4096) / (1024.0 * 1024 * 1024)
         << "," << shootdowns << ","
         << IObytes / (1024.0 * 1024 * 1024) << ","
         << cpuWork.exchange(0) << endl;
  }

  return 0;
}
