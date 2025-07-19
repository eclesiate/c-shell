set -xe

rm -f prefixTree shell
cc -g -O0 -Wall -Werror -std=c17 -ggdb main.c prefixTree.c autocomplete.c history.c historyList.c readline_init.c -o shell -fsanitize=address -lreadline -lncurses