.PHONY: micro all clean
module: micro

TARGETS = bulk random repeat

CXXFLAGS = -std=c++20

all: micro

micro: $(TARGETS)

%: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

