#include "nanorand.h"
#include <osv/types.h>

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

} // namespace benchmark
