#include "benchmark.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ostream>
#include <osv/pagealloc.hh>
#include <thread>

namespace benchmark {

// Alloc and free the same page
void repeat_worker(const size_t allocations, uint64_t *res) {
  uint64_t start = rdtsc();
  for (size_t i = 0; i < allocations; i++) {
    void *page = memory::physically_alloc_page();
    memory::physically_free_page(page);
  }
  uint64_t end = rdtsc();

  *res = end - start;
}

} // namespace benchmark

int main(int argc, char *argv[]) {
  size_t allocations = 50000;
  size_t iterations = 10;
  size_t threads = 1;
  u64 seed = 0;
  std::ostream *output = &std::cout; // Default to stdout
  std::ofstream fileOutput;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
      allocations = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
      iterations = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
      threads = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
      seed = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      fileOutput.open(argv[++i]);
      if (!fileOutput.is_open()) {
        std::cerr << "Error: Unable to open file " << argv[i]
                  << " for writing.\n";
        return 1;
      }
      output = &fileOutput; // Redirect output to the file
    } else {
      std::cerr << "Usage: " << argv[0]
                << " [-a allocations] [-i iterations] [-t threads] [-o "
                   "output_file]\n";
      return 1;
    }
  }

  (*output) << "operations " << allocations << "\n";
  (*output) << "threads    " << threads << "\n";
  (*output) << "iterations " << iterations << "\n";
  (*output) << "[iteration][thread] ticks ticks\n";

  uint64_t it[iterations][threads];
  uint64_t avg = 0;

  for (size_t i = 0; i < iterations; i++) {
    std::thread thread_pool[threads];
    for (size_t t = 0; t < threads; t++) {
      thread_pool[t] =
          std::thread(benchmark::repeat_worker, allocations, it[i] + t);
    }

    for (size_t t = 0; t < threads; t++) {
      thread_pool[t].join();
      (*output) << "[" << i << "][" << t << "] " << it[i][t] << std::endl;
      avg += it[i][t];
    }
  }

  (*output) << "per_operation:   " << avg / iterations / threads / allocations
            << " Ticks\n";

  if (fileOutput.is_open()) {
    fileOutput.close();
  }

  return 0;
}
