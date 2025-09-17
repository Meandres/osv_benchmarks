CXX = g++
src= ../..
CXXFLAGS = -std=c++17 -O3 -fPIC -shared 
CXXFLAGS += -g -DNDEBUG -DCONF_debug_memory=0
INCLUDES = -I$(src)/include -I$(src) -I$(src)/arch/common -I$(src)/arch/x64 -I$(src)/build/release/gen/include
INCLUDES += -I$(src)/modules/lwext4/upstream/lwext4/include -I$(src)/modules/lwext4/upstream/lwext4/build_lib_only/include
LDFLAGS = -pthread -fopenmp #-L$(src)/build/release.x64 -losv  

SRCDIR = .

.PHONY: module run_bfs clean 

module: run_bfs

run_bfs: $(SRCDIR)/run_bfs.cpp $(SRCDIR)/bfs_kernel.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS) 

clean:
	rm -f run_bfs
