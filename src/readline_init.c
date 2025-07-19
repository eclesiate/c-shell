#include "autocomplete.h"
#include "history.h"

void init_readline(void) {
    init_ac_readline();
    init_history_readline();
}