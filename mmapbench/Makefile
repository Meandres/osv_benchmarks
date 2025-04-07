UPSTREAM=https://github.com/viktorleis/mmapbench.git
src=../..

CXXFLAGS = -g -DNDEBUG -DCONF_debug_memory=0 -std=c++20 -mavx -mavx2 -O3
OSV_INCLUDES = -I$(src)/include -I$(src) -I$(src)/arch/common -I$(src)/arch/x64 -I$(src)/build/release/gen/include
LIBS=-ltbb -pthread

.PHONY: module clean distclean

module: mmapbench/mmapbench

mmapbench/mmapbench: mmapbench
	$(CXX) -shared $(CXXFLAGS) $(OSV_INCLUDES) mmapbench_osv.cpp -o mmapbench_osv.so
	cd mmapbench; $(CXX) $(CXXFLAGS) mmapbench.cpp -o mmapbench_linux $(LIBS); mv mmapbench_linux ..

mmapbench:
	git clone --depth 1 --recurse-submodules --single-branch $(UPSTREAM)
	cd mmapbench; patch -p1 < ../patches/nvme.patch

clean:
	rm -f mmapbench_osv mmapbench_linux

distclean:
	rm -rf mmapbench
