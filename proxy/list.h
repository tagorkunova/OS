#ifndef LIST_H
#define LIST_H

#include <stdio.h>

typedef struct List {
    int buf_len;
    char* buffer;
    struct List* next;
} List;

List* list_create(size_t buf_size);

void list_add_node(List* last_node, size_t buf_size);

void list_free(List* node);

#endif
