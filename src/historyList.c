#define _DEFAULT_SOURCE

#include "historyList.h"


history_list* create_history_list(void) {
    history_list* h = malloc(sizeof(history_list));
    h->head = NULL;
    h->tail = NULL;
    h->curr = NULL;
    h->len = 0;
    h->base = 1;
    return h;
}

void add_history_entry(history_list* h, char* cmd) {
    if (h == NULL || cmd == NULL) return;
    history_node* entry = malloc(sizeof(history_node));
    entry->cmd = strdup(cmd);
    entry->next = NULL;
    entry->prev = h->tail;
    

    if (h->head == NULL) {
        h->tail = h->head = entry;
    } else {
        h->tail->next = entry;
    }
    h->tail = entry;
    h->curr = entry;
    ++(h->len);
}

void free_history_list(history_list* h) {
    if (h == NULL) return;

    history_node* curr_node = h->head;
    while (curr_node != NULL) {
        history_node* next = curr_node->next;
        free(curr_node->cmd);
        free(curr_node);
        curr_node = next; 
    }
    h->head = NULL;
    h->tail = NULL;
    h->curr = NULL;
}