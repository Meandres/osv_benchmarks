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

double gettime() {
  struct timeval now_tv;
  gettimeofday(&now_tv, NULL);
  return ((double)now_tv.tv_sec) + ((double)now_tv.tv_usec) / 1000000.0;
}

uint64_t readTLBShootdownCount() {
  std::ifstream irq_stats("/proc/interrupts");
  assert(!!irq_stats);
  for (std::string line; std::getline(irq_stats, line);) {
    if (line.find("TLB") != std::string::npos) {
      std::vector<std::string> strs;
      boost::split(strs, line, boost::is_any_of("\t "));
      uint64_t count = 0;
      for (size_t i = 0; i < strs.size(); i++) {
        std::stringstream ss(strs[i]);
        uint64_t c;
        ss >> c;
        count += c;
      }
      return count;
    }
  }
  return 0;
}

uint64_t readIObytesOne() {
  std::ifstream stat("/sys/block/nvme2n1/stat");

  assert(!!stat);

  for (std::string line; std::getline(stat, line);) {
    std::vector<std::string> strs;
    boost::split(strs, line, boost::is_any_of("\t "), boost::token_compress_on);
    std::stringstream ss(strs[2]);
    uint64_t c;
    ss >> c;

    return c * 512;
  }
  return 0;
}

uint64_t readIObytes() {
  std::ifstream stat("/proc/diskstats");

  assert(!!stat);

  uint64_t sum = 0;
  for (std::string line; std::getline(stat, line);) {
    if (line.find("nvme") != std::string::npos) {
      std::vector<std::string> strs;
      boost::split(strs, line, boost::is_any_of("\t "),
                   boost::token_compress_on);

      std::stringstream ss(strs[6]);
      uint64_t c;
      ss >> c;

      sum += c * 512;
    }
  }
  return sum;
}

int pin_thread_to_core(int core_id) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

int main(int argc, char **argv) {
  if (argc < 5) {
    cerr << "dev threads seq hint" << endl;
    return 1;
  }

  unsigned threads = atoi(argv[2]);
  uint64_t fileSize = 2ull * 1024 * 1024 * 1024 * 1024;

  ucache::createCache(64l << 30, 64);
  char *p = (char *)ucache::uCacheManager->addVMA(fileSize, 4096);

  int hint = 0;

  int seq = (argc > 3) ? atoi(argv[3]) : 0;

  std::vector<atomic_uint64_t> counts(threads);
  std::vector<atomic_uint64_t> sums(threads);
  atomic<uint64_t> seqScanPos(0);

  vector<thread> t;
  for (unsigned i = 0; i < threads; i++) {
    t.emplace_back([&, i]() {
      pin_thread_to_core(i);
      atomic<uint64_t> &count = counts[i];
      atomic<uint64_t> &sum = sums[i];

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
  // TODO
  // auto lastShootdowns = readTLBShootdownCount();
  // auto lastIObytes = readIObytes();
  double start = gettime();
  while (true) {
    sleep(1);
    // TODO
    // uint64_t shootdowns = readTLBShootdownCount();
    // uint64_t IObytes = readIObytes();
    uint64_t workCount = 0;
    for (auto &x : counts)
      workCount += x.exchange(0);
    double t = gettime() - start;
    cout << argv[1] << "," << seq << "," << hint << "," << threads << "," << t
         << "," << (workCount * 4096) / (1024.0 * 1024 * 1024)
         << ","
         // TODO
         // << (shootdowns - lastShootdowns) << ","
         // << (IObytes - lastIObytes) / (1024.0 * 1024 * 1024) << ","
         << cpuWork.exchange(0) << endl;
    // TODO
    // lastShootdowns = shootdowns;
    // lastIObytes = IObytes;
  }

  return 0;
}
