set -xe

rm -f prefixTree shell
cc -g -O0 -Wall -Werror -std=c17 -ggdb main.c -o shell -fsanitize=address -lreadline -lncurses