VERSION=v1.2.0

.PHONY: module clean distclean

module: duckdb/duckdb

duckdb:
	git clone --depth 1 --single-branch -b $(VERSION) https://github.com/duckdb/duckdb

duckdb/duckdb: duckdb
	cd duckdb; $(MAKE)

clean:
	if [ -d duckdb ]; then cd duckdb && $(MAKE) clean; fi

distclean:
	rm -rf duckdb
