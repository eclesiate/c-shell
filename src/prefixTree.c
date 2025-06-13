/*
some of the function skeleton code taken from leetcode problem: https://leetcode.com/problems/implement-trie-prefix-tree/
*/

void acBufPush(char x, TrieType* type) {
    assert((type->autocompleteBufSz < AC_BUF_CAP) && type);
    type->autocompleteBuf[type->autocompleteBufSz++] = x;
}

void acBufPop(TrieType* type) {
    assert((type->autocompleteBufSz > 0) && type);
    --autocompleteBufSz;
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
    assert(word || root);
    
    for (size_t i = 0, Trie* currNode = root; i <= strlen(word); ++i) {
        if (word[i] == '\0') {
            currNode->isEnd = true;
            return;
        }
        if (!currNode->children[(size_t) word[i]]) {
            currNode->children[(size_t) word[i]] = allocNode();
            currNode = root->children[(size_t) word[i]];
        }
    }
}

bool trieSearch(Trie* root, char* word) { // left as recursive, don't think ill be using this
    assert(word);

    if (root == NULL) {
        return false;
    }
    // if we have reached the end of the prefix, then we have found it
    if ( *prefix == '\0') {
        if (root->isEnd == true) {
            return true;    
        }
        return false;
    }
    return trieSearch(root->children[(size_t) *prefix], ++word);
}

// returns sub tree, inserts prefix into buffer, which will be used when we are assembling the subtree
Trie* getPrefixSubtree(Trie* root, char* prefix, TrieType* type) {
    assert(root);

    for (size_t i = 0, Trie* currNode = root; i <= strlen(prefix); ++i) {
        if (prefix[i] == '\0') {
            return currNode;
        }
        if (currNode == NULL) {
            return NULL;
        }
        currNode = currNode->children[(size_t) prefix[i]];
        acBufPush(prefix[i], type);
    }
}

Trie* assembleTree(Trie* root, TrieType* type) {
    assert(root);

    if (root->isEnd) {
        
    }
    for (size_t i = 0; i < ARRAY_LEN(root->children); ++i) {
        if (root->children[(char) i] != NULL) {
            
            return getSubtree(root->children[(char) i]);
        }
    }
    
}

void trieFree(Trie* root) {
    if (!root) return;
    for (int i = 0; i < 256; ++i) {
        trieFree(root->children[i]);
    }
    free(root);
}
