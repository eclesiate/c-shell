/*
some of the function skeleton code taken from leetcode problem: https://leetcode.com/problems/implement-trie-prefix-tree/
*/
#define _DEFAULT_SOURCE
#include "prefixTree.h"

void ac_buf_push(char x, trie_type* type) {
    assert((type->autocomplete_buf_sz < AC_BUF_CAP) && type);
    type->autocomplete_buf[type->autocomplete_buf_sz++] = x;
}

void ac_buf_pop(trie_type* type) {
    assert((type->autocomplete_buf_sz > 0) && type);
    --type->autocomplete_buf_sz;
}


trie* alloc_node(void) {
    trie* node = calloc(1, sizeof(trie));
    if (node == NULL) {
        perror("calloc");
        exit(1);
    }
    return node;
}

trie* trie_create(void) {
    return alloc_node();
}

void trie_insert(trie* root, char* word) {
    assert(root && word);

    trie* currNode = root;
    for (size_t i = 0; word[i] != '\0'; ++i) {
        unsigned char idx = (unsigned char)word[i];
        if (!currNode->children[idx]) {
            currNode->children[idx] = alloc_node();
        }
        currNode = currNode->children[idx];
    }
    currNode->isEnd = true;
}

bool trie_search(trie* root, char* word) { // left as recursive, don't think ill be using this
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
    return trie_search(root->children[(size_t) *word], word + 1);
}

// returns sub tree, inserts prefix into buffer, which will be used when we are assembling the subtree
trie* get_prefix_subtree(trie* root, char* prefix, trie_type* type) {
    assert(root && prefix);
    trie* curr = root;
    for (size_t i = 0; prefix[i] != '\0'; ++i) {
        unsigned char idx = (unsigned char)prefix[i];
        if (!curr->children[idx]) {
            return NULL;
        }
        curr = curr->children[idx];
        ac_buf_push(prefix[i], type);
    }
    return curr;
}

void push_word(char*** words, size_t* count, size_t* cap, trie_type* type) {
    assert(count && cap && type);
    // resize array
    if (*count >= *cap) {
        *cap = (*cap == 0) ? INIT_MATCHES_BUF_SIZE : (*cap) * 2; 
        *words = realloc(*words, (*cap) * sizeof(char*));
    }
    // * remember "*" has operator precendence, so you must inclose what you want dereferenced with brackets, took forever to debug
    (*words)[(*count)++] = strndup(type->autocomplete_buf, type->autocomplete_buf_sz); // *strndup adds null terminator
}

void _assemble_trie_helper(trie* root, char*** words, size_t* count, size_t* cap, trie_type* type) {
    if (root->isEnd) {
        push_word(words, count, cap, type);
    }
    // DFS search all the nodes
    for (size_t i = 0; i < ARRAY_LEN(root->children); ++i) {
        if (root->children[i] != NULL) {
            ac_buf_push((char) i, type);
            _assemble_trie_helper(root->children[i], words, count, cap, type);
            ac_buf_pop(type);
        }   
    }
}

char** assemble_trie(trie* root, trie_type* type) {
    assert(root);
    char** words = NULL;
    size_t count = 0;
    size_t cap = 0;
    // populate words array of possible autocomplete matches
    _assemble_trie_helper(root, &words, &count, &cap, type);
    // push NULL sentinel into array
    if (count >= cap) {
        cap = (cap == 0) ? INIT_MATCHES_BUF_SIZE : cap * 2; 
        words = realloc(words, (cap) * sizeof(char*)); // TODO. maybe use small pointers or something
    }
    words[count++] = NULL; //* generator function for gnu readline requires null sentinel!
    return words; //* GNU readline will free the mallocd strings in the array here
}

void trie_free(trie* root) {
    if (!root) return;
    for (int i = 0; i < 256; ++i) {
        trie_free(root->children[i]);
    }
    free(root);
}
