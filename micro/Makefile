.PHONY: micro
module: micro

LDFLAGS += -L../../build/release.x64/ -losv  -Wl,--no-as-needed
INCLUDES = -I../../include

micro: micro.cpp
	$(CXX) $(INCLUDES) -o $@ micro.cpp $(LDFLAGS)

