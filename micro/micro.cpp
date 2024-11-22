// #include "pagealloc.hh"
#include <cstdint>
#include <cstdlib>
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

int mark(const size_t frameSize, const size_t operations, uint64_t *res) {

  std::vector<void *> mem;
  mem.reserve(operations);
  uint64_t startAlloc = rdtsc();
  for (size_t i = 0; i < operations; ++i) {
    // void *block = memory::alloc_page();
    // void *block = std::malloc(frameSize);
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
    // free(m);
    // auto pr = new (v) memory::page_range(memory::page_size);
    memory::physically_free_page(m);
  }
  auto endDealloc = rdtsc();
  res[1] += (endDealloc - startDealloc);

  std::cout << endAlloc - startAlloc << " | " << endDealloc - startDealloc
            << "\n";

  return 0;
}

int main() {
  std::cout << "Frame Allocator Benchmark:\n";
  size_t const frameSize = 4096;   // Size of each memory block
  size_t const operations = 50000; // Number of allocations and deallocations
  size_t const iterations = 10;

  uint64_t res[2];

  for (size_t i = 0; i < iterations; i++) {
    std::cout << "Iteration " << i << ": ";
    mark(frameSize, operations, res);
  }

  std::cout << "Frame Allocator Benchmark:\n";
  std::cout << "Frame Size: " << frameSize << " bytes\n";
  std::cout << "Operations: " << operations << "\n";
  std::cout << "Allocation:   " << res[0] / iterations << " Ticks\n";
  std::cout << "Deallocation: " << res[1] / iterations << " Ticks\n";

  return 0;
}
