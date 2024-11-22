#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <osv/pagealloc.hh>
#include <vector>

inline uint64_t rdtsc(void) {
  union {
    uint64_t val;
    struct {
      uint32_t lo;
      uint32_t hi;
    };
  } tsc;
  asm volatile("rdtsc" : "=a"(tsc.lo), "=d"(tsc.hi));
  return tsc.val;
}

int bench(const size_t allocations, uint64_t *res, std::ostream *output,
          bool machineReadable) {
  std::vector<void *> mem;
  mem.reserve(allocations);
  uint64_t startAlloc = rdtsc();
  for (size_t i = 0; i < allocations; ++i) {
    void *block = memory::physically_alloc_page();
    if (!block) {
      std::cerr << "Memory allocation failed at iteration " << i << "\n";
    }
    mem.push_back(block);
  }
  uint64_t endAlloc = rdtsc();
  res[0] += (endAlloc - startAlloc);

  auto startDealloc = rdtsc();
  for (void *m : mem) {
    memory::physically_free_page(m);
  }
  auto endDealloc = rdtsc();
  res[1] += (endDealloc - startDealloc);

  (*output) << endAlloc - startAlloc << "," << endDealloc - startDealloc
            << "\n";

  return 0;
}

int main(int argc, char *argv[]) {
  size_t allocations = 50000;
  size_t iterations = 10;
  size_t threads = 1;
  std::ostream *output = &std::cout; // Default to stdout
  std::ofstream fileOutput;
  bool machineReadable = false;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
      allocations = std::strtoul(argv[++i], nullptr, 10);
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
    } else if (strcmp(argv[i], "-m") == 0) {
      machineReadable = true;
    } else {
      std::cerr << "Usage: " << argv[0]
                << " [-a allocations] [-i iterations] [-o output_file] [-m]\n";
      return 1;
    }
  }

  if (!machineReadable) {
    (*output) << "Frame Allocator Benchmark:\n";
    (*output) << "Allocations per iteration: " << allocations << "\n";
    (*output) << "Number of iterations: " << iterations << "\n";
  } else {
    (*output) << "operations " << allocations << "\n";
    (*output) << "iterations " << allocations << "\n";
    (*output) << "alloc_ticks, dealloc_ticks\n";
  }

  uint64_t res[2] = {0, 0};

  for (size_t i = 0; i < iterations; i++) {
    bench(allocations, res, output, machineReadable);
  }

  if (!machineReadable) {
    (*output) << "Frame Allocator Benchmark Results:\n";
    (*output) << "Allocations: " << allocations << "\n";
    (*output) << "Per allocation:   " << res[0] / iterations / allocations
              << " Ticks\n";
    (*output) << "Per deallocation: " << res[1] / iterations / allocations
              << " Ticks\n";
  }
  if (fileOutput.is_open()) {
    fileOutput.close();
  }

  return 0;
}
