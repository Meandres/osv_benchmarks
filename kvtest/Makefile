.PHONY: kvtest all clean
module: kvtest

CC = gcc
CFLAGS = -Os -I. -DSQLITE_DIRECT_OVERFLOW_READ -DSQLITE_THREADSAFE=2
LDFLAGS = -ldl -lpthread
SRC = kvtest.c sqlite3.c
OUT = kvtest

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OUT)
