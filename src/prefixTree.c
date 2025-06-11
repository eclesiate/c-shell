#include <stdio.h>
#include <assert.h>


#define NODE_POOL_CAP 1024

typedef struct Node Node;

struct Node {
    Node* children[256]; // all ASCII posssibilities
};

void insertText(Node* root, const char* text);

Node node_pool[NODE_POOL_CAP] = {NULL};
size_t node_pool_count = 0;

Node* allocNode(void) {
    assert(node_pool_count < NODE_POOL_CAP);
    return &node_pool[node_pool_count++];
}

void insertText(Node* root, const char* text) { 
    assert(text || root);
    if (*text == '\0') return;

    size_t idx = (size_t) text;
    if (!root->children[idx]) {
        root->children[idx] = allocNode();
    }

    insertText(root->children[idx], ++text);
}

int main(int argc, char* argv[]) {
    Node* root = allocNode();
    insertText(root, "hello");
    return 0;
}