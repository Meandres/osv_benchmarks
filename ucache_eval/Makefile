CXX = g++
CXXFLAGS = -std=c++20 -O3 

DUCKDB_BUILD = duckdb/build/release/

.PHONY: module clean distclean

module: ucache_eval

ucache_eval: runner.cpp duckdb libvmcache.a
	$(CXX) $(CXXFLAGS) runner.cpp -o runner libvmcache/libvmcache.a -Lduckdb/build/release/benchmark -lbenchmarks -Iduckdb/ -Ilibvmcache/ -Iduckdb/src/include/ -Wl,-rpath,duckdb/build/release/benchmark -laio

libvmcache.a: git_libvmcache
	cd libvmcache; make -j -f Makefile.static

git_libvmcache:
	if [ ! -d libvmcache ]; then git clone https://github.com/Martin-Lndbl/libvmcache.git; fi

duckdb: git_duckdb
	cd duckdb; BUILD_BENCHMARK=1 BUILD_TPCH=1 $(MAKE) -j

git_duckdb:
	if [ ! -d duckdb ]; then git clone -b library https://github.com/Meandres/duckdb; fi

clean:
	if [ -d duckdb ]; then cd duckdb && $(MAKE) clean; fi
	if [ -d libvmcache ]; then cd libvmcache && $(MAKE) clean; fi

distclean:
	rm -rf duckdb libvmcache
