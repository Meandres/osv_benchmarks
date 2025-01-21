#include "benchmark.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>
#include <signal.h>
#include <sys/mman.h>
#include <thread>
#include <vector>

static void* align_page_down(void* x)
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(x);

    return reinterpret_cast<void*>(addr & ~4095UL);
}

static bool segv_received = false;
static void segv_handler(int sig, siginfo_t* si, void* unused)
{
    assert(!segv_received);
    segv_received = true;
    assert(mprotect(align_page_down(si->si_addr), 4096,
               PROT_READ | PROT_WRITE | PROT_EXEC)
        == 0);
}

static void catch_segv()
{
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segv_handler;
    assert(sigaction(SIGSEGV, &sa, NULL) == 0);
}

static bool caught_segv()
{
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    assert(sigaction(SIGSEGV, &sa, NULL) == 0);
    bool ret = segv_received;
    segv_received = false;
    return ret;
}

static inline bool try_write(void* addr)
{
    catch_segv();
    *(volatile char*)addr = 123;
    return !caught_segv();
}

namespace benchmark {

// Alloc a number of pages one by one and free them afterwards
void bulk_worker(unsigned core_id, size_t const measurements, size_t const granularity, uint64_t* alloc_time, uint64_t* free_time)
{
    stick_this_thread_to_core(core_id);
    std::vector<void*> mem;
    mem.reserve(measurements * granularity);
    uint64_t start;
    uint64_t end;

    for (size_t i = 0; i < measurements; ++i) {
        start = rdtsc();
        for (size_t j = 0; j < granularity; ++j) {
            void* buf = mmap(NULL, 2097152, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            assert(buf != MAP_FAILED);
            *(char*)buf = 0;
            mprotect(buf, 2097152, PROT_READ);
            assert(!try_write(buf));
            mem.push_back(buf);
        }
        end = rdtsc();
        alloc_time[i] = end - start;
    }

    for (size_t i = 0; i < measurements; ++i) {
        start = rdtsc();
        for (size_t j = 0; j < granularity; ++j) {
            // assert(*reinterpret_cast<size_t *>(mem.at(i * granularity + j)) == i*j);
            munmap(mem.at(i * granularity + j), 2097152);
        }
        end = rdtsc();
        free_time[i] = end - start;
    }
}

} // namespace benchmark

int main(int argc, char* argv[])
{
    size_t measurements = 4096;
    size_t granularity = 128;
    size_t threads = 1;
    benchmark::parse_args(argc, argv, &measurements, &granularity, &threads);
    std::cout << "xlabel: Allocations\n";
    std::cout << "ylabel: Avg. CPU Cycles\n";
    std::cout << "out:\n";

    uint64_t alloc_time[threads][measurements];
    uint64_t free_time[threads][measurements];
    uint64_t res_alloc[measurements];
    uint64_t res_free[measurements];
    uint64_t avg[2] = {0, 0};

    std::thread thread_pool[threads];
    for (size_t t = 0; t < threads; ++t) {
        thread_pool[t] = std::thread(benchmark::bulk_worker, t, measurements, granularity,
            alloc_time[t] + 0, free_time[t] + 0);
    }

    for (size_t t = 0; t < threads; ++t) {
        thread_pool[t].join();
        if (t == 0) {
            for (size_t m = 0; m < measurements; ++m) {
                res_alloc[m] = alloc_time[t][m];
                res_free[m] = free_time[t][m];
            }
        } else {
            for (size_t m = 0; m < measurements; ++m) {
                res_alloc[m] += alloc_time[t][m];
                res_free[m] += free_time[t][m];
            }
        }
    }

    for (size_t m = 0; m < measurements; ++m) {
        res_alloc[m] /= threads * granularity;
        res_free[m] /= threads * granularity;
        std::cout << res_alloc[m];
        // std::cout << res_free[m];
        std::cout << std::endl;
    }

    return 0;
}
