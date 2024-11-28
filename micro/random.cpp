#include "benchmark.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ostream>
#include <osv/pagealloc.hh>
#include <thread>

#define PART_REALLOCATIONS 5
#define ITER_REALLOCATION 100

namespace benchmark {

// Alloc and free pages at random
void random_worker(const size_t allocations, void *const pages, uint64_t *res) {
  uint64_t start_free = rdtsc();
  for (size_t i = 0; i < allocations / 10; i++) {
    void *page = memory::physically_alloc_page();
    memory::physically_free_page(page);
  }
  uint64_t end_free = rdtsc();

  *res = end_free - start_free;
}

} // namespace benchmark

// allocate operations*thread pages before
void populate_pages(u64 const page_len, void **const pages) {
  for (size_t k = 0; k < page_len; k++) {
    pages[k] = memory::physically_alloc_page();
  }
}

// deterministically determine which pages to free
void populate_to_free(size_t const free_len, u64 const page_len, u64 *seed,
                      u64 *const to_free) {
  for (size_t i = 0; i < free_len; i++) {
    u64 entry = nanorand_random_range(seed, 0, page_len);
    for (size_t k = 0; k < i; k++) {
      if (to_free[k] == entry)
        return populate_to_free(free_len - i, page_len, seed, to_free + i);
    }
    to_free[i] = entry;
  }
}

int main(int argc, char *argv[]) {
  size_t operations = 50000;
  size_t iterations = 10;
  size_t threads = 1;
  u64 seed = 5;
  std::ostream *output = &std::cout; // Default to stdout
  std::ofstream fileOutput;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
      operations = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
      iterations = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
      threads = std::strtoul(argv[++i], nullptr, 10);
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

  (*output) << "operations " << operations << "\n";
  (*output) << "threads    " << threads << "\n";
  (*output) << "iterations " << iterations << "\n";
  (*output) << "[iteration][thread] ticks ticks\n";

  uint64_t it[iterations][threads];
  uint64_t avg = 0;

  u64 const page_len = operations * threads;
  u64 const free_len = page_len / PART_REALLOCATIONS;

  void *pages[page_len];
  u64 to_free[free_len];

  for (size_t i = 0; i < iterations; i++) {
    std::thread thread_pool[threads];
    populate_pages(page_len, pages);
    populate_to_free(free_len, page_len, &seed, to_free);
    for (size_t t = 0; t < threads; t++) {
      thread_pool[t] = std::thread(benchmark::random_worker, operations,
                                   pages + t, it[i] + t);
    }
    // free operations*threads pages after
    for (size_t k = 0; k < operations * threads; k++) {
      memory::physically_free_page(pages[k]);
    }

    for (size_t t = 0; t < threads; t++) {
      thread_pool[t].join();
      (*output) << "[" << i << "][" << t << "] " << it[i][t] << std::endl;
      avg += it[i][t];
    }
  }

  (*output) << "per_operation:   " << avg / iterations / threads / operations
            << " Ticks\n";

  if (fileOutput.is_open()) {
    fileOutput.close();
  }

  return 0;
}
