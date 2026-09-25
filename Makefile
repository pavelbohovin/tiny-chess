CC = cc
STRIP = strip
CFLAGS = -std=c11 -Os -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
ifeq ($(shell uname -s),Darwin)
LDFLAGS = -Wl,-dead_strip
else
LDFLAGS = -Wl,--gc-sections
endif

.PHONY: all test clean
all: tiny-chess

tiny-chess: chess.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) chess.c $(LDFLAGS) $(LDLIBS) -o $@
	$(STRIP) $@
ifeq ($(shell uname -s),Darwin)
	codesign --force --sign - $@
endif

test: tiny-chess
	@set -e; test_dir=$$(mktemp -d); trap 'rm -rf "$$test_dir"' EXIT HUP INT TERM; \
	for source in tests/*.c; do \
		$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror "$$source" -o "$$test_dir/test"; \
		"$$test_dir/test"; \
	done; \
	sh tests/install.sh

clean:
	$(RM) tiny-chess
