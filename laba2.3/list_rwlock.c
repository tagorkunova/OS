#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_STR_LEN 100

typedef struct _Node {
    char value[MAX_STR_LEN];
    struct _Node *next;
    int length;
    pthread_rwlock_t sync;
} Node;

typedef struct _Storage {
    Node *first;
    int size;
    pthread_rwlock_t head_lock;
} Storage;

typedef struct {
    long ascending_pairs;
    long iterations;
} ThreadStats1;

typedef struct {
    long descending_pairs;
    long iterations;
} ThreadStats2;

typedef struct {
    long equal_pairs;
    long iterations;
} ThreadStats3;

typedef struct {
    long swaps;
} ThreadStatsSwap;

ThreadStats1 stats1 = {0, 0};
ThreadStats2 stats2 = {0, 0};
ThreadStats3 stats3 = {0, 0};
ThreadStatsSwap swap_stats[3] = {{0}, {0}, {0}};

Storage storage;
volatile int stop_flag = 0;

void init_storage(int size) {
    storage.first = NULL;
    storage.size = size;
    pthread_rwlock_init(&storage.head_lock, NULL);

    Node *prev = NULL;
    for (int i = 0; i < size; i++) {
        Node *node = malloc(sizeof(Node));
        int len = 1 + rand() % (MAX_STR_LEN - 1);
        for (int j = 0; j < len - 1; j++) {
            node->value[j] = 'a' + rand() % 26;
        }
        node->value[len - 1] = '\0';
        node->length = len - 1;
        node->next = NULL;
        pthread_rwlock_init(&node->sync, NULL);

        if (prev == NULL) {
            storage.first = node;
        } else {
            prev->next = node;
        }
        prev = node;
    }
}

void free_storage() {
    Node *curr = storage.first;
    while (curr) {
        Node *next = curr->next;
        pthread_rwlock_destroy(&curr->sync);
        free(curr);
        curr = next;
    }
    pthread_rwlock_destroy(&storage.head_lock);
}

void* reader_ascending(void *arg) {
    while (!stop_flag) {
        long pairs = 0;
        Node *curr = storage.first;
        if (!curr) {
            stats1.ascending_pairs = pairs;
            stats1.iterations++;
            continue;
        }

        pthread_rwlock_rdlock(&curr->sync);
        while (curr->next) {
            Node *next = curr->next;
            pthread_rwlock_rdlock(&next->sync);

            if (curr->next == next && curr->length < next->length) {
                pairs++;
            }

            pthread_rwlock_unlock(&curr->sync);
            curr = next;
        }
        pthread_rwlock_unlock(&curr->sync);

        stats1.ascending_pairs = pairs;
        stats1.iterations++;
    }
    return NULL;
}

void* reader_descending(void *arg) {
    while (!stop_flag) {
        long pairs = 0;
        Node *curr = storage.first;
        if (!curr) {
            stats2.descending_pairs = pairs;
            stats2.iterations++;
            continue;
        }

        pthread_rwlock_rdlock(&curr->sync);
        while (curr->next) {
            Node *next = curr->next;
            pthread_rwlock_rdlock(&next->sync);

            if (curr->next == next && curr->length > next->length) {
                pairs++;
            }

            pthread_rwlock_unlock(&curr->sync);
            curr = next;
        }
        pthread_rwlock_unlock(&curr->sync);

        stats2.descending_pairs = pairs;
        stats2.iterations++;
    }
    return NULL;
}

void* reader_equal(void *arg) {
    while (!stop_flag) {
        long pairs = 0;
        Node *curr = storage.first;
        if (!curr) {
            stats3.equal_pairs = pairs;
            stats3.iterations++;
            continue;
        }

        pthread_rwlock_rdlock(&curr->sync);
        while (curr->next) {
            Node *next = curr->next;
            pthread_rwlock_rdlock(&next->sync);

            if (curr->next == next && curr->length == next->length) {
                pairs++;
            }

            pthread_rwlock_unlock(&curr->sync);
            curr = next;
        }
        pthread_rwlock_unlock(&curr->sync);

        stats3.equal_pairs = pairs;
        stats3.iterations++;
    }
    return NULL;
}

void swap_nodes(Node *prev, Node *curr, Node *next) {
    curr->next = next->next;
    next->next = curr;
    if (prev) {
        prev->next = next;
    } else {
        storage.first = next;
    }
}

void* swapper(void *arg) {
    int id = *(int*)arg;

    while (!stop_flag) {
        int pos = rand() % storage.size;
        if (pos >= storage.size - 1) continue;

        Node *prev = NULL;
        Node *curr = storage.first;

        if (!curr) continue;

        if (pos == 0) {
            pthread_rwlock_wrlock(&storage.head_lock);
            curr = storage.first;
            if (!curr || !curr->next) {
                pthread_rwlock_unlock(&storage.head_lock);
                continue;
            }

            pthread_rwlock_wrlock(&curr->sync);
            Node *next = curr->next;
            if (!next) {
                pthread_rwlock_unlock(&curr->sync);
                pthread_rwlock_unlock(&storage.head_lock);
                continue;
            }
            pthread_rwlock_wrlock(&next->sync);

            if (storage.first != curr || curr->next != next) {
                pthread_rwlock_unlock(&next->sync);
                pthread_rwlock_unlock(&curr->sync);
                pthread_rwlock_unlock(&storage.head_lock);
                continue;
            }

            int should_swap = rand() % 2;
            if (should_swap) {
                swap_nodes(NULL, curr, next);
                swap_stats[id].swaps++;
            }

            pthread_rwlock_unlock(&next->sync);
            pthread_rwlock_unlock(&curr->sync);
            pthread_rwlock_unlock(&storage.head_lock);
        } else {
            pthread_rwlock_wrlock(&curr->sync);
            for (int i = 0; i < pos - 1 && curr->next; i++) {
                Node *next_node = curr->next;
                pthread_rwlock_wrlock(&next_node->sync);
                pthread_rwlock_unlock(&curr->sync);
                curr = next_node;
            }

            prev = curr;
            if (!prev->next) {
                pthread_rwlock_unlock(&prev->sync);
                continue;
            }

            curr = prev->next;
            pthread_rwlock_wrlock(&curr->sync);

            if (!curr->next) {
                pthread_rwlock_unlock(&curr->sync);
                pthread_rwlock_unlock(&prev->sync);
                continue;
            }

            Node *next = curr->next;
            pthread_rwlock_wrlock(&next->sync);

            if (prev->next != curr || curr->next != next) {
                pthread_rwlock_unlock(&next->sync);
                pthread_rwlock_unlock(&curr->sync);
                pthread_rwlock_unlock(&prev->sync);
                continue;
            }

            int should_swap = rand() % 2;
            if (should_swap) {
                swap_nodes(prev, curr, next);
                swap_stats[id].swaps++;
            }

            pthread_rwlock_unlock(&next->sync);
            pthread_rwlock_unlock(&curr->sync);
            pthread_rwlock_unlock(&prev->sync);
        }

        usleep(100);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <list_size>\n", argv[0]);
        return 1;
    }

    int list_size = atoi(argv[1]);
    srand(time(NULL));

    printf("Initializing list with %d elements (rwlock version)...\n", list_size);
    init_storage(list_size);

    pthread_t readers[3];
    pthread_t swappers[3];
    int swap_ids[3] = {0, 1, 2};

    pthread_create(&readers[0], NULL, reader_ascending, NULL);
    pthread_create(&readers[1], NULL, reader_descending, NULL);
    pthread_create(&readers[2], NULL, reader_equal, NULL);

    pthread_create(&swappers[0], NULL, swapper, &swap_ids[0]);
    pthread_create(&swappers[1], NULL, swapper, &swap_ids[1]);
    pthread_create(&swappers[2], NULL, swapper, &swap_ids[2]);

    for (int i = 0; i < 10; i++) {
        sleep(1);
        printf("[%ds] Asc: %ld (iter: %ld), Desc: %ld (iter: %ld), Equal: %ld (iter: %ld), Swaps: %ld/%ld/%ld\n",
               i + 1,
               stats1.ascending_pairs, stats1.iterations,
               stats2.descending_pairs, stats2.iterations,
               stats3.equal_pairs, stats3.iterations,
               swap_stats[0].swaps, swap_stats[1].swaps, swap_stats[2].swaps);
    }

    stop_flag = 1;

    for (int i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
        pthread_join(swappers[i], NULL);
    }

    printf("\nFinal stats:\n");
    printf("Ascending pairs: %ld (iterations: %ld)\n", stats1.ascending_pairs, stats1.iterations);
    printf("Descending pairs: %ld (iterations: %ld)\n", stats2.descending_pairs, stats2.iterations);
    printf("Equal pairs: %ld (iterations: %ld)\n", stats3.equal_pairs, stats3.iterations);
    printf("Total swaps: %ld + %ld + %ld = %ld\n",
           swap_stats[0].swaps, swap_stats[1].swaps, swap_stats[2].swaps,
           swap_stats[0].swaps + swap_stats[1].swaps + swap_stats[2].swaps);

    free_storage();
    return 0;
}
