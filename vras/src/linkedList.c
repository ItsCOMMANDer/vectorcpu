#include "linkedList.h"

#include <stdlib.h>

void ll_append(struct ll_head* head, void* data) {
    if(head == NULL) return;
    struct ll_node* newNode = calloc(1, sizeof(struct ll_node));
    newNode->data = data;
    newNode->head = head;
    newNode->next = NULL;
    
    head->len++;

    if(head->start == NULL) {
        newNode->prev = NULL;
        head->start = newNode;
        head->end = newNode;
    } else {
        newNode->prev = head->end;
        head->end->next = newNode;
        head->end = newNode;
    }
}

void ll_insertAfter(struct ll_head* head, size_t idx, void* data) {
    if(head == NULL) return;
    if(head->start == NULL) {
        ll_append(head, data);
        return;
    }
    struct ll_node *cur = head->start;

    size_t curIdx = 0;

    while(cur != NULL && curIdx < idx) {
        cur = cur->next;
        curIdx++;
    }

    if(!cur) return;

    struct ll_node* newNode = calloc(1, sizeof(struct ll_node));
    newNode->data = data;
    newNode->head = head;

    newNode->prev = cur;
    newNode->next = cur->next;
    if(cur->next != NULL) cur->next->prev = newNode;
    cur->next = newNode;
    if(newNode->next == NULL) head->end = newNode;

    head->len++;
}

void ll_insertBefore(struct ll_head* head, size_t idx, void* data) {
    if(head == NULL) return;

    struct ll_node *cur = head->start;
    size_t curIdx = 0;

    while(cur != NULL && curIdx < idx) {
        cur = cur->next;
        curIdx++;
    }

    if(!cur) return;

    struct ll_node* newNode = calloc(1, sizeof(struct ll_node));
    newNode->data = data;
    newNode->head = head;

    newNode->next = cur;
    newNode->prev = cur->prev;
    
    if(cur->prev != NULL) cur->prev->next = newNode;
    else head->start = newNode;

    cur->prev = newNode;

    head->len++;
}

void ll_insertAs(struct ll_head* head, size_t idx, void* data) {
    if(head->len == 0 || head->len == idx) {
        ll_append(head, data);
    } else {
        ll_insertBefore(head, idx, data);
    }
}

struct ll_node* ll_get(struct ll_head *head, size_t idx) {
    if(head == NULL) return NULL;
    struct ll_node *cur = head->start;

    size_t curIdx = 0;

    while(cur != NULL) {
        if(idx == curIdx++) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void ll_remove(struct ll_head* head, size_t idx) {
    if(head == NULL) return;

    struct ll_node *cur = ll_get(head, idx);
    if(cur == NULL) return;
    
    if(head->delete != NULL) head->delete(cur->data);
    
    if (cur->prev != NULL) {
        cur->prev->next = cur->next;
    } else {
        head->start = cur->next;
    }

    if (cur->next != NULL) {
        cur->next->prev = cur->prev;
    } else {
        head->end = cur->prev;
    }

    free(cur);
    
    head->len--;
}

void ll_delete(struct ll_head* head) {
    if(head == NULL) return;
    while(head->len > 0) {ll_remove(head, 0);}
}