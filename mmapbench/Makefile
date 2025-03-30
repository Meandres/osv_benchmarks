UPSTREAM=https://github.com/viktorleis/mmapbench.git

LIBS=-ltbb -pthread

.PHONY: module clean distclean

module: mmapbench/mmapbench

mmapbench/mmapbench: mmapbench
	$(CXX) -O3 -g mmapbench_osv.cpp -o mmapbench_osv $(LIBS)
	cd mmapbench; $(CXX) -O3 -g mmapbench.cpp -o mmapbench_linux $(LIBS); mv mmapbench_linux ..

mmapbench:
	git clone --depth 1 --recurse-submodules --single-branch $(UPSTREAM)
	cd mmapbench; patch -p1 < ../patches/nvme.patch

clean:
	rm -f mmapbench_osv mmapbench_linux

distclean:
	rm -rf mmapbench
