#include "nanorand.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace benchmark {
inline static uint64_t rdtsc(void) {
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

inline static void parse_args(int const argc, char const *const argv[],
                              size_t *measurements, size_t *granularity,
                              size_t *iterations, size_t *threads) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
      *measurements = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
      *granularity = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
      *iterations = std::strtoul(argv[++i], nullptr, 10);
    } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
      *threads = std::strtoul(argv[++i], nullptr, 10);
    } else {
      std::cerr << "Usage: " << argv[0]
                << " [-m measurements] [-g granularity] [-i iterations] [-t "
                   "threads]\n";
      exit(1);
    }
  }
  std::cout << "measurements " << *measurements << "\n";
  std::cout << "granularity " << *granularity << "\n";
  std::cout << "iterations " << *iterations << "\n";
  std::cout << "threads    " << *threads << "\n";
}

inline int stick_this_thread_to_core(int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

} // namespace benchmark
