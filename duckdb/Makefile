.PHONY: module linux duckdb-lib clean distclean

DUCKDB_DIR   := duckdb
DUCKDB_BUILD := $(DUCKDB_DIR)/build/release
DUCKDB_INCL  := $(DUCKDB_DIR)/src/include
DUCKDB_LIB   := $(DUCKDB_BUILD)/src
DUCKDB_SO    := $(DUCKDB_LIB)/libduckdb.so

CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -I$(DUCKDB_INCL) -I.
LDFLAGS  := -L$(abspath $(DUCKDB_LIB)) -lduckdb \
            -Wl,-rpath,$(abspath $(DUCKDB_LIB)) \
            -lpthread -ldl -lm

# ── OSv kernel module ─────────────────────────────────────────────────────────

module: $(DUCKDB_DIR)/duckdb

$(DUCKDB_DIR)/duckdb: $(DUCKDB_DIR)
	cd $(DUCKDB_DIR) && CXX_STANDARD=17 DUCKDB_PLATFORM=linux_amd64 \
	    BUILD_BENCHMARK=1 BUILD_TPCH=1 $(MAKE) -j

# ── Linux benchmark binary ────────────────────────────────────────────────────
# Build: make linux
# Run:   TPCH_QUERY=1 DUCKDB_FILE_CACHE=4G ./duckdb_bench

duckdb-lib: $(DUCKDB_SO)

$(DUCKDB_SO): | $(DUCKDB_DIR)
	cd $(DUCKDB_DIR) && CXX_STANDARD=17 DUCKDB_PLATFORM=linux_amd64 $(MAKE) release -j

linux: duckdb_bench

duckdb_bench: duckdb_app.cc tpch_queries.hpp $(DUCKDB_SO)
	$(CXX) $(CXXFLAGS) -o $@ duckdb_app.cc $(LDFLAGS)

# ── Cleanup ───────────────────────────────────────────────────────────────────

clean:
	if [ -d $(DUCKDB_DIR) ]; then $(MAKE) -C $(DUCKDB_DIR) clean; fi
	rm -f duckdb_bench

distclean:
	rm -rf $(DUCKDB_DIR)
	rm -f duckdb_bench
