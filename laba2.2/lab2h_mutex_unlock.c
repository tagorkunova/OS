#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

typedef struct qnode {
    int val;
    struct qnode *next;
} qnode_t;

typedef struct {
    qnode_t *first;
    qnode_t *last;
    int count;
    int max_count;

    long add_attempts;
    long get_attempts;
    long add_count;
    long get_count;

    pthread_mutex_t lock;
    pthread_cond_t cond_empty;
    pthread_cond_t cond_full;
    pthread_t qmonitor_tid;
} queue_t;

static long errors = 0;
static int g_reader_cpu = 0;
static int g_writer_cpu = 0;
static queue_t *global_q = NULL;

void set_cpu(int n) {
    cpu_set_t cpuset;
    pthread_t tid = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(n, &cpuset);
    pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
}

void *qmonitor(void *arg) {
    queue_t *q = (queue_t *)arg;
    while (1) {
        pthread_mutex_lock(&q->lock);
        printf("queue stats: current size %d; attempts (%ld %ld %ld); counts (%ld %ld %ld)\n",
                q->count,
                q->add_attempts, q->get_attempts, q->add_attempts - q->get_attempts,
                q->add_count, q->get_count, q->add_count - q->get_count);
        pthread_mutex_unlock(&q->lock);
        sleep(1);
    }
    return NULL;
}

queue_t* queue_init(int max_count) {
    queue_t *q = malloc(sizeof(queue_t));
    if (!q) abort();

    q->first = q->last = NULL;
    q->count = 0;
    q->max_count = max_count;
    q->add_attempts = q->get_attempts = 0;
    q->add_count = q->get_count = 0;

    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond_empty, NULL);
    pthread_cond_init(&q->cond_full, NULL);
    pthread_create(&q->qmonitor_tid, NULL, qmonitor, q);

    return q;
}

void queue_destroy(queue_t *q) {
    if (!q) return;
    pthread_cancel(q->qmonitor_tid);
    pthread_join(q->qmonitor_tid, NULL);

    pthread_mutex_lock(&q->lock);
    qnode_t *cur = q->first;
    while (cur) {
        qnode_t *next = cur->next;
        free(cur);
        cur = next;
    }
    pthread_mutex_unlock(&q->lock);

    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond_empty);
    pthread_cond_destroy(&q->cond_full);
    free(q);
}

int queue_add(queue_t *q, int val) {
    qnode_t *new = malloc(sizeof(qnode_t));
    new->val = val;
    new->next = NULL;

    pthread_mutex_lock(&q->lock);
    q->add_attempts++;

    while (q->count == q->max_count)
        pthread_cond_wait(&q->cond_empty, &q->lock);

    if (!q->first)
        q->first = q->last = new;
    else {
        q->last->next = new;
        q->last = new;
    }

    q->count++;
    q->add_count++;

    pthread_cond_signal(&q->cond_full);
    pthread_mutex_unlock(&q->lock);

    return 1;
}

int queue_get(queue_t *q, int *val) {
    pthread_mutex_lock(&q->lock);
    q->get_attempts++;

    while (q->count == 0)
        pthread_cond_wait(&q->cond_full, &q->lock);

    qnode_t *tmp = q->first;
    *val = tmp->val;

    q->first = tmp->next;
    if (!q->first) q->last = NULL;
    q->count--;
    q->get_count++;

    pthread_cond_signal(&q->cond_empty);
    pthread_mutex_unlock(&q->lock);

    free(tmp);
    return 1;
}

void *reader(void *arg) {
    int expected = 0;
    queue_t *q = (queue_t *)arg;
    set_cpu(g_reader_cpu);
    while (1) {
        int val = -1;
        if (queue_get(q, &val)) {
            if (val != expected)
                __atomic_fetch_add(&errors, 1, __ATOMIC_SEQ_CST);
            expected = val + 1;
        }
    }
    return NULL;
}

void *writer(void *arg) {
    int i = 0;
    queue_t *q = (queue_t *)arg;
    set_cpu(g_writer_cpu);
    while (1) {
        if (queue_add(q, i))
            i++;
    }
    return NULL;
}

void *unlocker_thread(void *arg) {
    (void)arg;
    sleep(2);
    printf("Trying to unlock mutex from different thread...\n");
    int result = pthread_mutex_unlock(&global_q->lock);
    if (result == 0) {
        printf("SUCCESS: Mutex unlocked from different thread!\n");
    } else {
        printf("FAILED: pthread_mutex_unlock returned error %d\n", result);
    }
    return NULL;
}

void *err_monitor(void *arg) {
    (void)arg;
    while (1) {
        long e = __atomic_load_n(&errors, __ATOMIC_SEQ_CST);
        printf("errors: %ld\n", e);
        sleep(1);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int queue_size = 100000;
    if (argc > 1) queue_size = atoi(argv[1]);
    if (argc > 2) g_reader_cpu = atoi(argv[2]);
    if (argc > 3) g_writer_cpu = atoi(argv[3]);

    queue_t *q = queue_init(queue_size);
    global_q = q;

    pthread_t r, w, e, u;
    pthread_create(&r, NULL, reader, q);
    pthread_create(&w, NULL, writer, q);
    pthread_create(&e, NULL, err_monitor, NULL);
    pthread_create(&u, NULL, unlocker_thread, NULL);

    pthread_join(r, NULL);
    pthread_join(w, NULL);
    pthread_join(e, NULL);
    pthread_join(u, NULL);

    queue_destroy(q);
    return 0;
}
