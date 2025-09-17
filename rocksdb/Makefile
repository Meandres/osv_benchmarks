VERSION=1.23

.PHONY: module

module:
	cd build; cmake -DCMAKE_BUILD_TYPE=Release -DWITH_SNAPPY=OFF -DWITH_TESTS=OFF -DWITH_BENCHMARK_TOOLS=ON -DWITH_ALL_TESTS=OFF -DFAIL_ON_WARNINGS=OFF -DUSE_RTTI=ON -DROCKSDB_BUILD_SHARED=OFF ../rocksdb && cmake --build . -j $(nproc) --target db_bench
	#rm -rf build; mkdir -p build; 
