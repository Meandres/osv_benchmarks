VERSION=v1.2.0

.PHONY: module clean distclean

module: duckdb/duckdb

duckdb:
	git clone --depth 1 --single-branch -b $(VERSION) https://github.com/duckdb/duckdb
	patch -p1 < patches/fix_root_dir.patch

duckdb/duckdb: duckdb
	cd duckdb; BUILD_BENCHMARK=1 CORE_EXTENSIONS='tpch' $(MAKE); \
	patchelf --set-rpath /ddb/src/ build/release/benchmark/benchmark_runner

clean:
	if [ -d duckdb ]; then cd duckdb && $(MAKE) clean; fi

distclean:
	rm -rf duckdb
