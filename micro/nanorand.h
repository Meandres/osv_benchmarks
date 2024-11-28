// Modified version of
// https://github.com/luhsra/linux-alloc-bench.git/nanorand.h

/// Generates new random number modifying the seed for the next call.
///
/// @see
/// - https://github.com/wangyi-fudan/wyhash
/// - https://github.com/Absolucy/nanorand-rs
#include <cstdint>
inline uint64_t nanorand_random(uint64_t *seed) {
  __uint128_t t;
  *seed = *seed + 0xa0761d6478bd642full;
  t = ((__uint128_t)*seed) * ((__uint128_t)(*seed ^ 0xe7037ed1a0b428dbull));
  return (uint64_t)((t >> 64) ^ t);
}

/// Generates new random number in this range, modifying the seed for the next
/// call.
inline uint64_t nanorand_random_range(uint64_t *seed, uint64_t start,
                                      uint64_t end) {
  uint64_t val = nanorand_random(seed);
  val %= end - start;
  val += start;
  return val;
}
