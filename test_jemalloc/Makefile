src= ../..
CXXFLAGS = -DNDEBUG -DCONF_debug_memory=0 -fPIC -shared
CXXFLAGS += -I $(src)/modules/jemalloc/upstream/jemalloc/include/jemalloc
#CXXFLAGS += -L$(src)/modules/jemalloc/upstream/jemalloc/lib 

module:
	g++ $(CXXFLAGS) test_jemalloc.cc -o test_jemalloc $(src)/modules/jemalloc/upstream/jemalloc/lib/libjemalloc.a

all: module

clean:
	rm test_jemalloc
