VERSION=v6.3.4

.PHONY: module clean distclean

module: KeyDB/keydb

KeyDB:
	git clone --depth 1 --recurse-submodules --single-branch -b $(VERSION) https://github.com/Snapchat/KeyDB.git
	# patch -p1 < patches/fix_root_dir.patch

KeyDB/keydb: KeyDB
	cd KeyDB; make BUILD_TLS=no MALLOC=libc
	# patchelf --set-rpath /ddb/src/ build/release/benchmark/benchmark_runner

clean:
	if [ -d KeyDB ]; then cd KeyDB/build && $(MAKE) clean; fi

distclean:
	rm -rf KeyDB
