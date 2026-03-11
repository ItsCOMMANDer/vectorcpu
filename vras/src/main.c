#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct llstring_head {
    struct llstring_node* start;
    struct llstring_node* end;
    size_t len;
};

struct llstring_node {
    char* str;
    size_t len;
    struct llstring_node* next;
    struct llstring_node* prev;
    struct llstring_head* head;
};

void llstring_append(struct llstring_head* head, char* str, size_t len) {
    if(head == NULL) return;
    struct llstring_node* newNode = calloc(1, sizeof(struct llstring_node));
    newNode->str = str;
    newNode->len = len;
    newNode->head = head;
    newNode->next = NULL;
    
    head->end = newNode;
    head->len++;

    if(head->start == NULL) {
        head->start = newNode;
        newNode->prev = NULL;
    } else {
        newNode->prev = head->end;
        head->end->next = newNode;
        head->end = newNode;
    }
}

void llstring_insertAfter(struct llstring_head* head, size_t idx, char* str, size_t len) {
    if(head == NULL) return;
    struct llstring_node *cur = head->start;

    size_t curIdx = 0;

    while(cur != NULL && curIdx < idx) {
        cur = cur->next;
    }

    if(!cur) return;

    struct llstring_node* newNode = calloc(1, sizeof(struct llstring_node));
    newNode->str = str;
    newNode->len = len;
    newNode->head = head;

    newNode->prev = cur;
    newNode->next = cur->next;
    if(cur->next != NULL) cur->next->prev = newNode;
    cur->next = newNode;
}


struct llstring_node* llstring_get(struct llstring_head *head, size_t idx) {
    if(head == NULL) return;
    struct llstring_node *cur = head->start;

    size_t curIdx = 0;

    while(cur != NULL) {
        if(idx == curIdx++) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void llstring_delete(struct llstring_head* head) {
    if(head == NULL) return;
    struct llstring_node *cur = head->start;
    while(cur != NULL) {
        struct llstring_node *next = cur->next;
        free(cur->str);
        free(cur);
        cur = next;
    }
    head->len = 0;
    head->start = NULL;
    head->end = NULL;
}


int main(int argc, char **argv) {
    printf("Vectorcpu Raw ASsembler v0.0.0\n");
    
    const char inputFile_name[] = "./asm/test.as";
}