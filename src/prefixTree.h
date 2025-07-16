#ifndef PREFIXTREE_H
#define PREFIXTREE_H

#include <stdbool.h> 
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_LEN(x) ((sizeof(x)) / (sizeof((x)[0])))
#define AC_BUF_CAP 1024 // 1kb max word length
#define INIT_MATCHES_BUF_SIZE 4 // trie should be small

typedef struct trie trie;
struct trie {
    trie* children[256];
    bool isEnd;
};

// there could be multiple Tries, one for builtins, one for executables
typedef struct trie_type trie_type;
struct trie_type {
    char autocomplete_buf[AC_BUF_CAP];
    size_t autocomplete_buf_sz;
};

// insert into dynamic array of words, helper to assemble_trie
void push_word(char*** words, size_t* count, size_t* cap, trie_type* type);
void ac_buf_push(char x, trie_type* type);
void ac_buf_pop(trie_type* type);

trie* alloc_node(void);
trie* trie_create(void);
void trie_insert(trie* root, char* word);
bool trie_search(trie* root, char* word);
trie* get_prefix_subtree(trie* root, char* prefix, trie_type* type);
void _assemble_trie_helper(trie* root, char*** words, size_t* count, size_t* cap, trie_type* type);
char** assemble_trie(trie* root, trie_type* type);
void trie_free(trie* root);

#endif