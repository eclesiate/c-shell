/*
History feature for shell, up/down arrow, listing history.
*/

#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdbool.h>
#include <curses.h>
#include <readline/readline.h>

#include "history.h"
#include "historyList.h"

history_list* history = NULL;

static bool first_up_press = true;
static char* unsaved_text = NULL;

void init_history_readline(void) {
    rl_bind_keyseq("\033[A", history_up_arrow);
    rl_bind_keyseq("\033[B", history_down_arrow);
}

int history_up_arrow(int count, int key) {
    char* line = NULL;

    if (!history || !history->curr) return 0;

    if (first_up_press) { // save line and go to tail
        free(unsaved_text);
        unsaved_text = strdup(rl_line_buffer);
        line = history->curr->cmd;
        first_up_press = false;
    } 
    else if (history->curr->prev) {
        line = history->curr->prev->cmd;
        history->curr = history->curr->prev;
    } 
    else {
        return 0;
    }
    
    rl_replace_line(line, 0);  
    rl_point = strlen(line);
    rl_redisplay();
    
    return 0;
}

int history_down_arrow(int count, int key) {
    char* line = NULL;

    if (!history || !history->curr) return 0;

    if (history->curr->next) {
        line = history->curr->next->cmd;
        history->curr = history->curr->next;
    } 
    // at tail but not displaying unsaved line buffer before first UP was pressed
    else if (!first_up_press) {
        line = unsaved_text;
        first_up_press = true;
        rl_replace_line(line, 0);
        rl_point = strlen(line);
        rl_redisplay();
        free(unsaved_text);
        unsaved_text = NULL;
        return 0;
    } 
    else { 
        return 0;
    }

    rl_replace_line(line, 0);
    rl_point = strlen(line);
    rl_redisplay();

    return 0;
}

void list_history(int n) {
      // * i need to check negative separately before safely casting, otherwise 0xFFFFF(...) is always larger than any size_t value
    if ((n > 0) && ((size_t) n > history->len)) {
        printf("input length exceeds history length, history length = %zu\n", history->len);
        return;
    }

    int list_start = (n != -1) ? history->len - n : 0;
    history_node* temp = history->head;

    for (size_t i = 0; i < list_start; ++i) {
        temp = temp->next;
    }

    for (size_t i = list_start; temp != NULL; ++i) {
        printf("\t%ld  %s\n", i + history->base, temp->cmd);
        temp = temp->next;
    }
}
