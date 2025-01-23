.PHONY: tst-mmap
module: tst-mmap

LDFLAGS += -L../../build/release.x64/ -losv  -Wl,--no-as-needed -g -pthread -std=c++11 -Wno-pointer-arith
INCLUDES = -I../../include

tst-mmap: tst-mmap.cc
	$(CXX) $(INCLUDES) -o $@ $< $(LDFLAGS)

clean:
	rm -f tst-mmap
