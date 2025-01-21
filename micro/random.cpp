#include "benchmark.h"
#include <barrier>
#include <unordered_set>
#include <vector>

namespace benchmark {

// Alloc and free pages at random
void random_worker(unsigned core_id, std::vector<void *> &pages,
                   std::vector<std::vector<uint64_t>> &to_free,
                   std::vector<uint64_t> &alloc_time,
                   std::vector<uint64_t> &free_time,
                   std::barrier<> &measurement_barrier,
                   size_t size) {
  stick_this_thread_to_core(core_id);
  uint64_t start;
  uint64_t end;

  for (size_t i = 0; i < to_free.size(); ++i) {
    start = rdtsc();
    for (size_t j = 0; j < to_free[0].size(); ++j) {
      free_page(pages[to_free[i][j]]);
    }
    end = rdtsc();
    free_time[i] = end - start;

    start = rdtsc();
    for (size_t j = 0; j < to_free[0].size(); ++j) {
      pages[to_free[i][j]] = malloc(size);
    }
    end = rdtsc();
    alloc_time[i] = end - start;

    // Measurement rounds need to be synced to avoid double frees
    measurement_barrier.arrive_and_wait();
  }
}

} // namespace benchmark

// Allocate pages for initial setup
void populate_pages(std::vector<void *> &pages, size_t size) {
  for (auto &page : pages) {
    page = malloc(size);
    if (!page) {
      std::exit(EXIT_FAILURE);
    }
  }
}

// Deterministically determine which pages to free
void populate_to_free(std::vector<std::vector<std::vector<uint64_t>>> &to_free,
                      size_t threads, size_t measurements, size_t granularity,
                      size_t pages_allocated, uint64_t *seed) {
  for (auto &t : to_free) {
    t.resize(measurements);
    for (auto &m : t) {
      m.resize(granularity);
    }
  }
  for (size_t m{0}; m < measurements; ++m) {
    std::unordered_set<size_t> pool;
    size_t t = 0;
    size_t g = 0;
    while (pool.size() < granularity * threads) {
      uint64_t index = nanorand_random_range(seed, 0, pages_allocated);
      if (pool.insert(index).second) {
        to_free[t][m][g] = index;
        if (++g >= granularity) {
          g = 0;
          ++t;
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  size_t measurements{4096};
  size_t granularity{128};
  size_t threads{1};
  size_t size{4096};
  uint64_t seed{SEED};
  benchmark::parse_args(argc, argv, &measurements, &granularity, &threads, &size);

  std::cout << "xlabel: Rounds of " << granularity
            << " frees and allocations\n";
  std::cout << "ylabel: Avg. CPU Cycles\n";
  std::cout << "out:\n";

  uint64_t pages_allocated = RANDOM_MEM_FACTOR * granularity;

  std::vector<void *> pages(pages_allocated, nullptr);
  std::vector<std::vector<std::vector<uint64_t>>> to_free(threads);

  populate_pages(pages, size);
  populate_to_free(to_free, threads, measurements, granularity, pages_allocated,
                   &seed);

  std::vector<std::thread> thread_pool(threads);
  std::vector<std::vector<uint64_t>> alloc_time(
      threads, std::vector<uint64_t>(measurements));
  std::vector<std::vector<uint64_t>> free_time(
      threads, std::vector<uint64_t>(measurements));

  std::barrier measurement_barrier(threads);

  for (size_t i = 0; i < threads; ++i) {
    thread_pool[i] =
        std::thread(benchmark::random_worker, i, std::ref(pages),
                    std::ref(to_free[i]), std::ref(alloc_time[i]),
                    std::ref(free_time[i]), std::ref(measurement_barrier), size);
  }

  for (auto &thread : thread_pool) {
    thread.join();
  }

  for (size_t m = 0; m < measurements; ++m) {
    uint64_t res_alloc = 0;
    uint64_t res_free = 0;
    for (size_t t = 0; t < threads; ++t) {
      res_alloc += alloc_time[t][m];
      res_free += free_time[t][m];
    }
    res_alloc /= threads * granularity;
    res_free /= threads * granularity;
    std::cout << res_alloc;
    // std::cout << res_free;
    std::cout << std::endl;
  }

  for (auto page : pages) {
    if (page)
      benchmark::free_page(page);
  }

  return 0;
}
