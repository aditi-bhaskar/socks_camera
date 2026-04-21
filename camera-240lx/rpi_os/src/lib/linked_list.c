#include "linked_list.h"

#include <stddef.h>

static LList* ll_insert_helper(LList* node, LList* prev, LList* next) {
    node->prev = prev;
    node->next = next;

    prev->next = node;
    next->prev = node;
    return node;
}
static LList* ll_remove_helper(LList* node, LList* prev, LList* next) {
    prev->next = next;
    next->prev = prev;

    node->prev = node->next = node;
    return node;
}

void ll_init(LList* node) {
    node->prev = node->next = node;
}
LList* ll_insert(LList* node, LList* prev) {
    return ll_insert_helper(node, prev, prev->next);
}
LList* ll_insert_next(LList* node, LList* prev) {
    return ll_insert(node, prev);
}
LList* ll_insert_prev(LList* node, LList* next) {
    return ll_insert_helper(node, next->prev, next);
}
LList* ll_remove(LList* node) {
    return ll_remove_helper(node, node->prev, node->next);
}
LList* ll_pop_front(LList** node) {
    LList* n = *node;
    *node = n->next;
    return ll_remove(n);
}

bool ll_empty(LList* node) {
    return node->next == node;
}
