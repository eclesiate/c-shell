set -xe

rm -f prefixTree shell
cc -g -O0 -Wall -Werror -std=c17 -ggdb testPrefixTree.c prefixTree.c -o testTree -fsanitize=address -lreadline -lncurses