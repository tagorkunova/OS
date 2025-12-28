#include <malloc.h>
#include <stdio.h>

#include "list.h"

List* list_create(size_t buf_size) {
    List* list = (List*)malloc(sizeof(List));
    list->buf_len = 0;
    list->buffer = (char*)malloc(sizeof(char) * buf_size);
    list->next = NULL;

    return list;
}

void list_add_node(List* last_node, size_t buf_size) {
    List* new_node = list_create(buf_size);

    last_node->next = new_node;
}

void list_free(List* node) {
    free(node->buffer);

    if (node->next == NULL) {
        free(node);
        return;
    } else {
        list_free(node->next);
        free(node);
    }
}
