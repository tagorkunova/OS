CC = gcc
CFLAGS = -Wall -pthread -std=gnu11
TARGETS = spinlock mutex compare cas_asm

all: $(TARGETS)

spinlock: spinlock.c
        $(CC) $(CFLAGS) -o spinlock spinlock.c

mutex: mutex.c
        $(CC) $(CFLAGS) -o mutex mutex.c

compare: compare.c
        $(CC) $(CFLAGS) -o compare compare.c

cas_asm: cas_asm.c
        $(CC) $(CFLAGS) -o cas_asm cas_asm.c

clean:
        rm -f $(TARGETS)

test: all
        @echo "=== Testing spinlock ==="
        ./spinlock
        @echo ""
        @echo "=== Testing custom mutex ==="
        ./mutex
        @echo ""
        @echo "=== Comparing implementations ==="
        ./compare
        @echo ""
        @echo "=== CAS assembly demo ==="
        ./cas_asm

.PHONY: all clean test
