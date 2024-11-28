.PHONY: micro all
module: micro

TARGETS = bulk repeat random

all: micro

micro: $(TARGETS)

%: %.cpp
	$(CXX) -o $@ $<

