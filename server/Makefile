CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?= -pthread

TARGET := build/michat-server
SOURCES := server.c list.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) list.h | build data
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

build data:
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build
