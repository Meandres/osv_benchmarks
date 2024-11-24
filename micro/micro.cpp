#include "micro.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ostream>
#include <osv/pagealloc.hh>
#include <thread>
#include <vector>

namespace benchmark {

void bulk(size_t const iterations, size_t const threads,
          size_t const allocations, std::ostream *const output) {

  (*output) << "operations " << allocations << "\n";
  (*output) << "iterations " << allocations << "\n";
  (*output) << "[iteration][thread] alloc_ticks, dealloc_ticks\n";

  uint64_t it[iterations][threads][2];
  uint64_t avg[2] = {0, 0};

  for (size_t i = 0; i < iterations; i++) {
    std::thread thread_pool[threads];
    for (size_t t = 0; t < threads; t++) {
      thread_pool[t] = std::thread(bulk_worker, allocations, it[i][t]);
    }

    for (size_t t = 0; t < threads; t++) {
      thread_pool[t].join();
      (*output) << "[" << i << "][" << t << "] " << it[i][t][0] << ","
                << it[i][t][1] << "\n";
      avg[0] += it[i][t][0];
      avg[1] += it[i][t][1];
    }
  }

  (*output) << "per_allocation:   "
            << avg[0] / iterations / threads / allocations << " Ticks\n";
  (*output) << "per_deallocation: "
            << avg[1] / iterations / threads / allocations << " Ticks\n";
}

// Alloc a number of pages one by one and free them afterwards
int bulk_worker(const size_t allocations, uint64_t *res) {
  std::vector<void *> mem;
  mem.reserve(allocations);
  uint64_t startAlloc = rdtsc();
  for (size_t i = 0; i < allocations; ++i) {
    void *page = memory::physically_alloc_page();
    if (!page) {
      std::cerr << "Memory allocation failed at iteration " << i << "\n";
    }
    // Write to trigger EPT fault (not needed for physically_alloc_page)
    *static_cast<int *>(page) = 1;
    mem.push_back(page);
  }
  uint64_t endAlloc = rdtsc();
  res[0] = (endAlloc - startAlloc);

  auto startDealloc = rdtsc();
  for (void *m : mem) {
    memory::physically_free_page(m);
  }
  auto endDealloc = rdtsc();
  res[1] = (endDealloc - startDealloc);

  return 0;
}

uint64_t repeat(const size_t allocations, uint64_t res) {
  uint64_t start = rdtsc();
  for (size_t i = 0; i < allocations; i++) {
    void *page = memory::physically_alloc_page();
    memory::physically_free_page(page);
  }
  uint64_t end = rdtsc();

  return end - start;
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
    } else if (strcmp(argv[i], "bulk") == 0) {
      benchmark::bulk(iterations, threads, allocations, output);
      break;
    } else {
      std::cerr << "Usage: " << argv[0]
                << " [-a allocations] [-i iterations] [-t threads] [-o "
                   "output_file] [-m] <bulk|repeat>\n";
      return 1;
    }
  }

  if (fileOutput.is_open()) {
    fileOutput.close();
  }

  return 0;
}
