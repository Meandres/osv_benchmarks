VERSION=1.23

.PHONY: module

module:
	cd leveldb; git submodule update --init --recursive
	mkdir -p leveldb/build; cd leveldb/build; cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DWITH_RTTI=ON .. && cmake --build .
	cd YCSB-cpp; git submodule update --init; make BIND_LEVELDB=1 EXTRA_CXXFLAGS=-I../leveldb/include EXTRA_LDFLAGS=-L../leveldb/build
