#pragma once

#include <stdbool.h> 
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_LEN(x) ((sizeof(x)) / (sizeof((x)[0])))
#define AC_BUF_CAP 1024 // 1kb max word length
#define INIT_MATCHES_BUF_SIZE 4 // trie should be small
typedef struct Trie Trie;
struct Trie {
    Trie* children[256];
    bool isEnd;
};

// there could be multiple Tries, one for builtins, one for executables
typedef struct TrieType TrieType;
struct TrieType {
    char autocompleteBuf[AC_BUF_CAP];
    size_t autocompleteBufSz;
};
// insert into dynamic array of words, helper to assembleTree
void pushWord(char*** words, size_t* count, size_t* cap, TrieType* type);
void acBufPush(char x, TrieType* type);
void acBufPop(TrieType* type);

Trie* allocNode(void);
Trie* trieCreate(void);
void trieInsert(Trie* root, char* word);
bool trieSearch(Trie* root, char* word);
Trie* getPrefixSubtree(Trie* root, char* prefix, TrieType* type);
void _assembleTreeHelper(Trie* root, char*** words, size_t* count, size_t* cap, TrieType* type);
char** assembleTree(Trie* root, TrieType* type);
void trieFree(Trie* root);