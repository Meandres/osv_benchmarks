UPSTREAM_DIR=upstream
SQLITE_DIR=$(UPSTREAM_DIR)/sqlite
ARCHIVE=sqlite-330-for-speedtest.tar.gz
URL=http://www.phoronix-test-suite.com/benchmark-files/$(ARCHIVE)

.PHONY: all clean

all: $(SQLITE_DIR)
	@echo "Running configure..."
	cd $(SQLITE_DIR) && ./configure
	@echo "Running install-sh..."
	cd $(SQLITE_DIR) && make speedtest1

$(UPSTREAM_DIR):
	mkdir -p $(UPSTREAM_DIR)

$(SQLITE_DIR): $(UPSTREAM_DIR)
	wget -nc -P $(UPSTREAM_DIR) $(URL)
	tar -xzf $(UPSTREAM_DIR)/$(ARCHIVE) -C $(UPSTREAM_DIR)

clean:
	rm -rf $(UPSTREAM_DIR)

