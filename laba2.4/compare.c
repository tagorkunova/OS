#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <time.h>

typedef struct {
    atomic_int val;
} custom_mutex_t;

static int futex(int *uaddr, int futex_op, int val) {
    return syscall(SYS_futex, uaddr, futex_op, val, NULL, NULL, 0);
}

void custom_mutex_init(custom_mutex_t *m) {
    atomic_store(&m->val, 0);
}

void custom_mutex_lock(custom_mutex_t *m) {
    int c = 0;
    if (!atomic_compare_exchange_strong(&m->val, &c, 1)) {
        if (c != 2) {
            c = atomic_exchange(&m->val, 2);
        }
        while (c != 0) {
            futex((int*)&m->val, FUTEX_WAIT, 2);
            c = atomic_exchange(&m->val, 2);
        }
    }
}

void custom_mutex_unlock(custom_mutex_t *m) {
    if (atomic_fetch_sub(&m->val, 1) != 1) {
        atomic_store(&m->val, 0);
        futex((int*)&m->val, FUTEX_WAKE, 1);
    }
}

#define ITERATIONS 500000
#define NUM_THREADS 4

custom_mutex_t custom_mtx;
pthread_mutex_t pthread_mtx = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

void* worker_custom(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        custom_mutex_lock(&custom_mtx);
        counter++;
        custom_mutex_unlock(&custom_mtx);
    }
    return NULL;
}

void* worker_pthread(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&pthread_mtx);
        counter++;
        pthread_mutex_unlock(&pthread_mtx);
    }
    return NULL;
}

double get_time_ms(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000.0 + (end->tv_nsec - start->tv_nsec) / 1000000.0;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start, end;

    printf("Comparing custom mutex vs pthread_mutex\n");
    printf("Threads: %d, Iterations per thread: %d\n\n", NUM_THREADS, ITERATIONS);

    custom_mutex_init(&custom_mtx);
    counter = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker_custom, NULL);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("Custom mutex:\n");
    printf("  Counter: %d (expected: %d)\n", counter, NUM_THREADS * ITERATIONS);
    printf("  Time: %.2f ms\n", get_time_ms(&start, &end));
    printf("  Status: %s\n\n", counter == NUM_THREADS * ITERATIONS ? "PASSED" : "FAILED");

    counter = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker_pthread, NULL);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("pthread_mutex:\n");
    printf("  Counter: %d (expected: %d)\n", counter, NUM_THREADS * ITERATIONS);
    printf("  Time: %.2f ms\n", get_time_ms(&start, &end));
    printf("  Status: %s\n", counter == NUM_THREADS * ITERATIONS ? "PASSED" : "FAILED");

    return 0;
}
