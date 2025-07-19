#ifndef HISTORYLIST_H
#define HISTORYLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct history_node history_node;
struct history_node {
    history_node* next;
    history_node* prev;
    char* cmd;
};

typedef struct history_list history_list;

struct history_list {
    history_node* head;
    history_node* tail;
    history_node* curr;
    size_t len;
    size_t base;
};

history_list* create_history_list(void);
void add_history_entry(history_list* h, char* cmd);
void free_history_list(history_list* h);

#endif