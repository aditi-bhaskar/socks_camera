#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdbool.h>

#define offsetof(type, member) ((uint32_t) &((type*) 0)->member)
#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

typedef struct LList {
    struct LList* prev;
    struct LList* next;
} LList;

void ll_init(LList* node);
LList* ll_insert(LList* node, LList* prev);
LList* ll_insert_next(LList* node, LList* prev);
LList* ll_insert_prev(LList* node, LList* next);
LList* ll_remove(LList* node);
LList* ll_pop_front(LList** node);

bool ll_empty(LList* node);

#endif
