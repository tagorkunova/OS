#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

typedef struct {
    atomic_int val;
} mutex_t;

static int futex(int *uaddr, int futex_op, int val) {
    return syscall(SYS_futex, uaddr, futex_op, val, NULL, NULL, 0);
}

void mutex_init(mutex_t *m) {
    atomic_store(&m->val, 0);
}

void mutex_lock(mutex_t *m) {
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

void mutex_unlock(mutex_t *m) {
    if (atomic_fetch_sub(&m->val, 1) != 1) {
        atomic_store(&m->val, 0);
        futex((int*)&m->val, FUTEX_WAKE, 1);
    }
}

#define ITERATIONS 1000000
#define NUM_THREADS 4

mutex_t mtx;
int counter = 0;

void* worker(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        mutex_lock(&mtx);
        counter++;
        mutex_unlock(&mtx);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    mutex_init(&mtx);

    printf("Starting custom mutex test with %d threads, %d iterations each\n", NUM_THREADS, ITERATIONS);
    printf("Expected final counter: %d\n", NUM_THREADS * ITERATIONS);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Actual final counter: %d\n", counter);
    printf("Test %s\n", counter == NUM_THREADS * ITERATIONS ? "PASSED" : "FAILED");

    return 0;
}
