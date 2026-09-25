CC = gcc
CFLAGS = -std=c11 -Os -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections -s

.PHONY: all clean
all: tiny-chess

tiny-chess: chess.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) chess.c $(LDFLAGS) $(LDLIBS) -o $@

clean:
	$(RM) tiny-chess
