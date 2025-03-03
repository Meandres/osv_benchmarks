VERSION=v1.2.0

.PHONY: module clean distclean

module: duckdb/duckdb

duckdb:
	git clone --depth 1 -b $(VERSION) https://github.com/duckdb/duckdb
	cd duckdb && git apply ../duckdb.patch && git apply ../threadpin.patch

duckdb/duckdb: duckdb
	cd duckdb; BUILD_BENCHMARK=1 BUILD_TPCH=1 $(MAKE);

clean:
	if [ -d duckdb ]; then cd duckdb && $(MAKE) clean; fi

distclean:
	rm -rf duckdb
