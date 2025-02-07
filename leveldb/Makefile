VERSION=1.23

.PHONY: module clean distclean

module: leveldb/build/db_bench

leveldb:
	git clone --depth 1 --recurse-submodules --single-branch -b $(VERSION) https://github.com/google/leveldb
	# patch -p1 < patches/fix_root_dir.patch

leveldb/build/db_bench: leveldb
	mkdir -p leveldb/build; cd leveldb/build; cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
	# patchelf --set-rpath /ddb/src/ build/release/benchmark/benchmark_runner

clean:
	if [ -d leveldb ]; then cd leveldb/build && $(MAKE) clean; fi

distclean:
	rm -rf leveldb
