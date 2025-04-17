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
using namespace ucache;

#define check(expr)                                                            \
  if (!(expr)) {                                                               \
    perror(#expr);                                                             \
    throw;                                                                     \
  }

atomic<bool> keepGoing;

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
    cerr << "virtSize(in TiB) mode(0,1,2) pageSize timetorun(in sec)" << endl;
    return 1;
  }

  //unsigned threads = atoi(argv[2]);
  unsigned threads = sched::cpus.size();
  u64 virtSize = atoi(argv[1]);
  uint64_t fileSize = virtSize * 1024 * 1024 * 1024 * 1024;

  createCache(100l << 30, 512);
  int page_size = atoi(argv[3]);
  char *p = (char *)uCacheManager->addVMA(fileSize, page_size);
  VMA* vma = uCacheManager->getVMA((void*)p);

  int mode = atoi(argv[2]);
  int maxTime = atoi(argv[4]);
  keepGoing.store(true);

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
      switch(mode){
        case 0: { // og seq
          while (keepGoing.load()) {
              uint64_t scanBlock = 128 * 1024 * 1024;
              uint64_t pos = (seqScanPos += scanBlock) % fileSize;

              for (uint64_t j = 0; j < scanBlock; j += 4096) {
                sum += p[pos + j];
                count++;
              }
            }
          break;
        }
        case 1: { // og rndread
          {
          std::random_device rd;
          std::mt19937 gen(rd());
          std::uniform_int_distribution<uint64_t> rnd(0, fileSize);
            while (keepGoing.load()) {
              sum += p[rnd(gen)];
              count++;
            }
          }
          break;
        }
        case 2: { // manual fault seqread
          while (keepGoing.load()) {
              uint64_t scanBlock = 128 * 1024 * 1024;
              uint64_t pos = (seqScanPos += scanBlock) % fileSize;

              for (uint64_t j = 0; j < scanBlock; j += 4096) {
                Buffer* buf = vma->getBuffer(p+pos+j); 
                BufferSnapshot bs(vma->nbPages);
                buf->updateSnapshot(&bs);
                if(bs.state != BufferState::Cached){
                  uCacheManager->handleFault(vma, buf);
                }
                sum += p[pos + j];
                count++;
              }
            }
          break;
        }
        case 3: { // manual fault rndread
          std::random_device rd;
          std::mt19937 gen(rd());
          std::uniform_int_distribution<uint64_t> rnd(0, fileSize/4096);
          while (keepGoing.load()) {
            u64 pos = rnd(gen);
            Buffer* buf = vma->getBuffer(p+pos); 
            BufferSnapshot bs(vma->nbPages);
            buf->updateSnapshot(&bs);
            if(bs.state != BufferState::Cached)
              uCacheManager->handleFault(vma, buf);
            sum += p[pos];
            count++;
          }
          break;
        }
        case 4: { // in memory seqread
          while (keepGoing.load()) {
              uint64_t scanBlock = 128 * 1024 * 1024;
              uint64_t pos = (seqScanPos += scanBlock) % fileSize;

              for (uint64_t j = 0; j < scanBlock; j += 4096) {
                Buffer* buf = vma->getBuffer(p+pos+j); 
                uCacheManager->handleFault(vma, buf, true);
                sum += p[pos + j];
                count++;
              }
            }
          break;
        }
        case 5: { // manual fault rndread
          std::random_device rd;
          std::mt19937 gen(rd());
          std::uniform_int_distribution<uint64_t> rnd(0, fileSize/4096);
          while (keepGoing.load()) {
            u64 pos = rnd(gen);
            Buffer* buf = vma->getBuffer(p+pos); 
            uCacheManager->handleFault(vma, buf, true);
            sum += p[pos];
            count++;
          }
          break;
        }
        default:
          printf("mode not supported\n");
      }
    });
  }

  atomic<uint64_t> cpuWork(0);
  t.emplace_back([&]() {
    while (keepGoing.load()) {
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
    uint64_t pf = ucache::uCacheManager->pageFaults.exchange(0);
    uint64_t workCount = 0;
    for (auto &x : counts)
      workCount += x.val.exchange(0);
    double ti = gettime() - start;
    cout << "/none," << mode << ",0," << threads << "," << ti  << "," << (workCount * 4096) / (1024.0 * 1024 * 1024)
         << "," << shootdowns << ","
         << IObytes / (1024.0 * 1024 * 1024) << ","
         << cpuWork.exchange(0) << endl;
    if(ti >= maxTime){
      keepGoing.store(false);
      break;
    }
  }
  for(auto& t: t)
    t.join();

  return 0;
}
