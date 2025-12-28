#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct {
    atomic_int lock;
} spinlock_t;

void spinlock_init(spinlock_t *s) {
    atomic_store(&s->lock, 0);
}

void spinlock_lock(spinlock_t *s) {
    int expected;
    do {
        expected = 0;
    } while (!atomic_compare_exchange_weak(&s->lock, &expected, 1));
}

void spinlock_unlock(spinlock_t *s) {
    atomic_store(&s->lock, 0);
}

#define ITERATIONS 1000000
#define NUM_THREADS 4

spinlock_t spin;
int counter = 0;

void* worker(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        spinlock_lock(&spin);
        counter++;
        spinlock_unlock(&spin);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    spinlock_init(&spin);

    printf("Starting spinlock test with %d threads, %d iterations each\n", NUM_THREADS, ITERATIONS);
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
