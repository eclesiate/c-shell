/*
some of the function skeleton code taken from leetcode problem: https://leetcode.com/problems/implement-trie-prefix-tree/
*/
#define _DEFAULT_SOURCE
#include "prefixTree.h"

void acBufPush(char x, TrieType* type) {
    assert((type->autocomplete_buf_sz < AC_BUF_CAP) && type);
    type->autocomplete_buf[type->autocomplete_buf_sz++] = x;
}

void acBufPop(TrieType* type) {
    assert((type->autocomplete_buf_sz > 0) && type);
    --type->autocomplete_buf_sz;
}


Trie* allocNode(void) {
    Trie* node = calloc(1, sizeof(Trie));
    if(node == NULL) {
        perror("calloc");
        exit(1);
    }
    return node;
}

Trie* trieCreate(void) {
    return allocNode();
}

void trieInsert(Trie* root, char* word) {
    assert(root && word);

    Trie* currNode = root;
    for (size_t i = 0; word[i] != '\0'; ++i) {
        unsigned char idx = (unsigned char)word[i];
        if (!currNode->children[idx]) {
            currNode->children[idx] = allocNode();
        }
        currNode = currNode->children[idx];
    }
    currNode->isEnd = true;
}

bool trieSearch(Trie* root, char* word) { // left as recursive, don't think ill be using this
    assert(word);

    if (root == NULL) {
        return false;
    }
    // if we have reached the end of the prefix, then we have found it
    if ( *word == '\0') {
        if (root->isEnd == true) {
            return true;    
        }
        return false;
    }
    return trieSearch(root->children[(size_t) *word], word + 1);
}

// returns sub tree, inserts prefix into buffer, which will be used when we are assembling the subtree
Trie* getPrefixSubtree(Trie* root, char* prefix, TrieType* type) {
    assert(root && prefix);
    Trie* curr = root;
    for (size_t i = 0; prefix[i] != '\0'; ++i) {
        unsigned char idx = (unsigned char)prefix[i];
        if (!curr->children[idx]) {
            return NULL;
        }
        curr = curr->children[idx];
        acBufPush(prefix[i], type);
    }
    return curr;
}

void pushWord(char*** words, size_t* count, size_t* cap, TrieType* type) {
    assert(count && cap && type);
    // resize array
    if (*count >= *cap) {
        *cap = (*cap == 0) ? INIT_MATCHES_BUF_SIZE : (*cap) * 2; 
        *words = realloc(*words, (*cap) * sizeof(char*));
    }
    // * remember "*" has operator precendence, so you must inclose what you want dereferenced with brackets
    (*words)[(*count)++] = strndup(type->autocomplete_buf, type->autocomplete_buf_sz); // *strndup adds null terminator
}

void _assembleTreeHelper(Trie* root, char*** words, size_t* count, size_t* cap, TrieType* type) {
    if (root->isEnd) {
        pushWord(words, count, cap, type);
    }
    // DFS search all the nodes
    for (size_t i = 0; i < ARRAY_LEN(root->children); ++i) {
        if (root->children[i] != NULL) {
            acBufPush((char) i, type);
            _assembleTreeHelper(root->children[i], words, count, cap, type);
            acBufPop(type);
        }   
    }
}

char** assembleTree(Trie* root, TrieType* type) {
    assert(root);
    char** words = NULL;
    size_t count = 0;
    size_t cap = 0;
    // populate words array of possible autocomplete matches
    _assembleTreeHelper(root, &words, &count, &cap, type);
    // push NULL sentinel into array
    if (count >= cap) {
        cap = (cap == 0) ? INIT_MATCHES_BUF_SIZE : cap * 2; 
        words = realloc(words, (cap) * sizeof(char*)); // TODO. maybe use small pointers or something
    }
    words[count++] = NULL; //* generator function for gnu readline requires null sentinel!
    return words; //* GNU readline will free the mallocd strings in the array here
}

void trieFree(Trie* root) {
    if (!root) return;
    for (int i = 0; i < 256; ++i) {
        trieFree(root->children[i]);
    }
    free(root);
}
