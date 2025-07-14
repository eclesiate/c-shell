#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

void initializeAutocomplete(void);
void initializeReadline(void);

char **autocomplete(const char *text, int start, int end);

#endif