#ifndef HISTORY_H
#define HISTORY_H

#include <readline/readline.h>

typedef struct history_list history_list;

void  list_history(int n);

int   history_up_arrow(int count, int key);
int   history_down_arrow(int count, int key);

void  init_history_readline(void);

extern history_list* history;

#endif
