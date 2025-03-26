UPSTREAM=https://github.com/viktorleis/mmapbench.git

LIBS=-ltbb

.PHONY: module clean distclean

module: mmapbench/mmapbench

mmapbench/mmapbench: mmapbench
	cd mmapbench; $(CXX) mmapbench.cpp -o mmapbench $(LIBS)

mmapbench:
	git clone --depth 1 --recurse-submodules --single-branch $(UPSTREAM)

clean:
	if [ -d mmapbench ]; then cd mmapbench && $(MAKE) clean; fi

distclean:
	rm -rf mmapbench
