UPSTREAM_DIR=upstream
SQLITE_DIR=$(UPSTREAM_DIR)/sqlite
ARCHIVE=sqlite-330-for-speedtest.tar.gz
URL=http://www.phoronix-test-suite.com/benchmark-files/$(ARCHIVE)
TARGET=$(SQLITE_DIR)/speedtest1

.PHONY: all clean module FORCE

module: all

all: $(TARGET)

$(TARGET): $(SQLITE_DIR)/Makefile	FORCE
	@echo "Running make speedtest1..."
	cd $(SQLITE_DIR) && make speedtest1

$(SQLITE_DIR)/Makefile: $(SQLITE_DIR)/configure
	@echo "Running configure..."
	cd $(SQLITE_DIR) && ./configure

$(SQLITE_DIR)/configure: $(SQLITE_DIR)
	@test -f $(SQLITE_DIR)/configure || { echo "Error: configure script missing!"; exit 1; }

$(SQLITE_DIR): $(UPSTREAM_DIR)/$(ARCHIVE)
	tar -xzf $(UPSTREAM_DIR)/$(ARCHIVE) -C $(UPSTREAM_DIR)

$(UPSTREAM_DIR)/$(ARCHIVE):
	mkdir -p $(UPSTREAM_DIR)
	wget -nc -P $(UPSTREAM_DIR) $(URL)

clean:
	rm -rf $(UPSTREAM_DIR)

FORCE:

