.PHONY: micro all
module: micro

LDFLAGS += -L../../build/release.x64/ -losv -Wl,--no-as-needed
INCLUDES = -I../../include

TARGETS = bulk repeat

all: micro

micro: $(TARGETS)

%: %.cpp
	$(CXX) $(INCLUDES) -o $@ $< $(LDFLAGS)

