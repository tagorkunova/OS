CC = gcc
CFLAGS = -Wall -pthread -O2
TARGETS = list_mutex list_spinlock list_rwlock

all: $(TARGETS)

list_mutex: list_mutex.c
        $(CC) $(CFLAGS) -o list_mutex list_mutex.c

list_spinlock: list_spinlock.c
        $(CC) $(CFLAGS) -o list_spinlock list_spinlock.c

list_rwlock: list_rwlock.c
        $(CC) $(CFLAGS) -o list_rwlock list_rwlock.c

clean:
        rm -f $(TARGETS)

test: all
        @echo "Testing with 100 elements..."
        @./list_mutex 100
        @echo ""
        @./list_spinlock 100
        @echo ""
        @./list_rwlock 100

.PHONY: all clean test
