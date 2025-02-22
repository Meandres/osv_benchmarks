#include "duckdb/benchmark/include//benchmark_runner.hpp"
#include "../libvmcache/vmcache.hpp"

int main()
{
    const char* args[] = {
        "benchmark_runner", 
        "--nruns=2,6,1",
        "--idle_time=2000,0,0",
        "--transition_time=0,1000",
        "benchmark/tpch/expression_reordering/adaptive_mixed_reordering_and.benchmark",
        "benchmark/tpch/expression_reordering/adaptive_string_reordering_or.benchmark",
        "benchmark/tpch/expression_reordering/adaptive_mixed_reordering_or.benchmark",
        0
    };

    duckdb::setup(std::size(args) - 1, const_cast<char**>(args));
    duckdb::run();


    // Following parameters need to be passed as environment variables
    // VIRTGB
    // PHYSGB
    // BLOCK
    // EXMAP

    unsigned nthreads = 1;
    uint64_t n = 10;
    uint64_t runForSec = 30;

    vmcache::configure(nthreads, n, runForSec);
    vmcache::tpcc::setup();
    vmcache::tpcc::run();
}
