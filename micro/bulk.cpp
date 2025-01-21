#include "benchmark.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>
#include <thread>
#include <vector>

namespace benchmark {

// Alloc a number of pages one by one and free them afterwards
void bulk_worker(unsigned core_id, size_t const measurements,
                 size_t const granularity, uint64_t *alloc_time,
                 uint64_t *free_time, size_t size) {
  stick_this_thread_to_core(core_id);
  std::vector<void *> mem;
  mem.reserve(measurements * granularity);
  uint64_t start;
  uint64_t end;

  for (size_t i = 0; i < measurements; ++i) {
    start = rdtsc();
    for (size_t j = 0; j < granularity; ++j) {
      void *page = malloc(size);
      if (!page) {
        std::cerr << "Memory allocation failed at iteration " << i << "\n";
        exit(1);
      }
      mem.push_back(page);
    }
    end = rdtsc();
    alloc_time[i] = end - start;
  }

  for (size_t i = 0; i < measurements; ++i) {
    start = rdtsc();
    for (size_t j = 0; j < granularity; ++j) {
      free(mem.at(i * granularity + j));
    }
    end = rdtsc();
    free_time[i] = end - start;
  }
}

} // namespace benchmark

int main(int argc, char *argv[]) {
  size_t measurements{4096};
  size_t granularity{128};
  size_t threads{1};
  size_t size{4096};
  benchmark::parse_args(argc, argv, &measurements, &granularity, &threads, &size);
  std::cout << "xlabel: Allocations\n";
  std::cout << "ylabel: Avg. CPU Cycles\n";
  std::cout << "out:\n";

  uint64_t alloc_time[threads][measurements];
  uint64_t free_time[threads][measurements];
  uint64_t res_alloc[measurements];
  uint64_t res_free[measurements];
  uint64_t avg[2] = {0, 0};

  std::thread thread_pool[threads];
  for (size_t t = 0; t < threads; ++t) {
    thread_pool[t] =
        std::thread(benchmark::bulk_worker, t, measurements, granularity,
                    alloc_time[t] + 0, free_time[t] + 0, size);
  }

  for (size_t t = 0; t < threads; ++t) {
    thread_pool[t].join();
    if (t == 0) {
      for (size_t m = 0; m < measurements; ++m) {
        res_alloc[m] = alloc_time[t][m];
        res_free[m] = free_time[t][m];
      }
    } else {
      for (size_t m = 0; m < measurements; ++m) {
        res_alloc[m] += alloc_time[t][m];
        res_free[m] += free_time[t][m];
      }
    }
  }

  for (size_t m = 0; m < measurements; ++m) {
    res_alloc[m] /= threads * granularity;
    res_free[m] /= threads * granularity;
    std::cout << res_alloc[m];
    // std::cout << res_free[m];
    std::cout << std::endl;
  }

  return 0;
}
