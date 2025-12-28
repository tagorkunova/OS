#ifndef CACHE_H
#define CACHE_H

#include "hashmap.c/hashmap.h"
#include "list.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct CacheEntry {
    char* url;
    List* data;
    int arc;
    size_t response_len;
    bool done;
    bool error;
    int parts_done;

    struct LRUQueue* queue_node;

    pthread_rwlock_t lock;
    pthread_cond_t new_part;
    pthread_mutex_t wait_lock;

} CacheEntry;

typedef struct HashValue {
    char* url;
    CacheEntry* entry;
} HashValue;

typedef struct LRUQueue {
    struct LRUQueue* prev;
    struct LRUQueue* next;
    CacheEntry* entry;
} LRUQueue;

struct hashmap* cache_create();

CacheEntry* cache_entry_create(char* url, size_t data_size);
void cache_entry_sub(CacheEntry* entry);
void cache_entry_free(CacheEntry* entry);

HashValue* cache_value_create(CacheEntry* entry);

void cache_entry_add(LRUQueue** head, LRUQueue** tail, CacheEntry* entry);
void cache_entry_upd(LRUQueue** head, LRUQueue** tail, CacheEntry* entry);
CacheEntry* cache_entry_remove(LRUQueue** head, LRUQueue** tail);

#endif
