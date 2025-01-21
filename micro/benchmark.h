#pragma once

#include "nanorand.h"
#include <cstring>
#include <iostream>

#define SEED 0
#define RANDOM_MEM_FACTOR 100

namespace benchmark {

inline static void* alloc_page() { return malloc(4096); }
inline static void free_page(void* ptr) { free(ptr); }

inline static uint64_t rdtsc(void)
{
    union
    {
        uint64_t val;
        struct
        {
            uint32_t lo;
            uint32_t hi;
        };
    } tsc;
    asm volatile("rdtsc" : "=a"(tsc.lo), "=d"(tsc.hi));
    return tsc.val;
}

inline static void parse_args(int const argc, char const* const argv[], size_t* measurements, size_t* granularity, size_t* threads, size_t* size)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            *measurements = std::strtoul(argv[++i], nullptr, 10);
        } else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            *granularity = std::strtoul(argv[++i], nullptr, 10);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            *threads = std::strtoul(argv[++i], nullptr, 10);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            *size = std::strtoul(argv[++i], nullptr, 10);
        } else {
            std::cerr << "Usage: " << argv[0]
                      << " [-m measurements] [-g granularity] [-t threads] [-s size]\n";
            exit(1);
        }
    }
    std::cout << "measurements " << *measurements << "\n";
    std::cout << "granularity " << *granularity << "\n";
    std::cout << "threads    " << *threads << "\n";
    std::cout << "size    " << *size << "\n";
}

inline int stick_this_thread_to_core(int core_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

} // namespace benchmark
