VERSION=v1.2.0

CXX = g++
CXXFLAGS = -std=c++20 -O3 

DUCKDB_BUILD = duckdb/build/release/

.PHONY: module clean distclean

module: benchmark_runner

benchmark_runner: runner.cpp duckdb/duckdb
	$(CXX) $(CXXFLAGS) runner.cpp -o runner -Lduckdb/build/release/benchmark -lbenchmarks -Iduckdb/src/include/ -Wl,-rpath,duckdb/build/release/benchmark

duckdb/duckdb: duckdb
	cd duckdb; BUILD_BENCHMARK=1 BUILD_TPCH=1 $(MAKE)
	# patchelf --set-rpath /ddb/src/ $(DUCKDB_BUILD)/benchmark/benchmark_runner

duckdb:
	git clone --depth 1 --single-branch -b $(VERSION) https://github.com/duckdb/duckdb
	cd duckdb; patch -p1 < patches/fix_root_dir.patch && \
						 patch -p1 < patches/benchmark_runner_params.patch

clean:
	if [ -d duckdb ]; then cd duckdb && $(MAKE) clean; fi

distclean:
	rm -rf duckdb
