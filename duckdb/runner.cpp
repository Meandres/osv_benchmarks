#include "duckdb/benchmark//include//benchmark_runner.hpp"

int main()
{
    const char* args[] = {
        "benchmark_runner", 
        "benchmark/tpch/expression_reordering/.*", 
        nullptr  // Null-terminated array
    };

    duckdb::setup(2, const_cast<char**>(args));
    duckdb::run();
}
