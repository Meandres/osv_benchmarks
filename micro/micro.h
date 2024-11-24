#include "nanorand.h"
#include <ostream>
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

void bulk(size_t const iterations, size_t const threads,
          size_t const allocations, std::ostream *const output);
int bulk_worker(const size_t allocations, uint64_t *res);

} // namespace benchmark
