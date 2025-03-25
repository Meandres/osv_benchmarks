.PHONY: module clean distclean

module: duckdb/duckdb

duckdb/duckdb: duckdb
	cd duckdb; BUILD_BENCHMARK=1 BUILD_TPCH=1 $(MAKE) reldebug;

clean:
	if [ -d duckdb ]; then cd duckdb && $(MAKE) clean; fi

distclean:
	rm -rf duckdb
