#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdio.h>

struct ll_head {
    struct ll_node* start;
    struct ll_node* end;
    size_t len;
    void (*delete)(void*);
};

struct ll_node {
    void* data;
    struct ll_node* next;
    struct ll_node* prev;
    struct ll_head* head;
};

void ll_append(struct ll_head* head, void* data);
void ll_insertAfter(struct ll_head* head, size_t idx, void* data);
void ll_insertBefore(struct ll_head* head, size_t idx, void* data);
struct ll_node* ll_get(struct ll_head *head, size_t idx);
void ll_remove(struct ll_head* head, size_t idx);
void ll_delete(struct ll_head* head);

#endif