.PHONY: mmu-test
module: mmu-test

LDFLAGS += -L../../build/release.x64/ -losv  -Wl,--no-as-needed
INCLUDES = -I../../include

mmu-test: mmu-test.cpp
	$(CXX) $(INCLUDES) -o $@ mmu-test.cpp $(LDFLAGS)

