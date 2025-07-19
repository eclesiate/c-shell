#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

void init_ac_readline(void);
void init_ac(void);
void cleanup_ac(void);

char **autocomplete(const char *text, int start, int end);

extern const char* builtin_cmds[];

#endif