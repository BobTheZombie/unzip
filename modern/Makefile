# Invoke from the repository root: make -f modern/Makefile
CC = gcc
CFLAGS ?= -O3 -pipe
LDFLAGS ?=
CPU ?= generic
# CPU is a scheduling tune, not an ISA requirement. Zen 2 remains supported.
TUNE = $(if $(filter generic,$(CPU)),,-mtune=$(CPU))
.PHONY: all clean test install
all:
	$(MAKE) -f unix/Makefile linux_noasm CC="$(CC)" CFLAGS="$(CFLAGS) $(TUNE)" LFLAGS1="$(LDFLAGS)" LF2=""
	$(CC) -std=gnu17 $(CFLAGS) $(TUNE) -Wall -Wextra -pthread modern/unzip-parallel.c $(LDFLAGS) -o unzip-parallel
PREFIX ?= /usr
DESTDIR ?=
install: all
	$(MAKE) -f unix/Makefile prefix="$(DESTDIR)$(PREFIX)" BINDIR="$(DESTDIR)$(PREFIX)/bin" MANDIR="$(DESTDIR)$(PREFIX)/share/man/man1" install
	install -m755 unzip-parallel "$(DESTDIR)$(PREFIX)/bin/unzip-parallel"
clean:
	$(MAKE) -f unix/Makefile clean
	rm -f unzip-parallel tests/crc-test
test: all
	$(CC) -std=gnu17 -O2 -Wall -Wextra tests/crc-test.c -lz -o tests/crc-test
	./tests/crc-test
	python3 tests/modern-test.py
