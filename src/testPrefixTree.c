#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prefixTree.h"

int main() {
    Trie* root = trieCreate();
    TrieType t = {.autocompleteBuf = {0}, .autocompleteBufSz = 0};

    const char* words[] = {
        "apple", "applesauce", "app", "application", "applet",
        "banana", "band", "bandana", "bandwidth",
        "berry", "blackberry", "blueberry", "strawberry", "raspberry",
        "coconut", "cranberry", "cucumber", "cherry",
        "date", "dragon", "dragonfruit", "dragonfly",
        "fig", "grape", "grapefruit", "grapeskin",
        "honey", "honeydew", "honeydewmelon", "honeyberries",
        "kiwi", "lemon", "lime", "mango", "melon", "nectarine",
        "orange", "papaya", "peach", "pear", "pineapple",
        "plum", "pomegranate", "rasp", "straw", "tangerine", "watermelon"
    };
    size_t n = sizeof(words) / sizeof(words[0]);

    for (size_t i = 0; i < n; ++i) {
        trieInsert(root, (char*)words[i]);
    }

    const char* testPrefixes[] = {
        "app", "ban", "dragon", "grape", "honey", "straw", "rasp", "berry", "z"
    };

    for (size_t i = 0; i < sizeof(testPrefixes)/sizeof(testPrefixes[0]); ++i) {
        printf("\nAutocomplete matches for prefix \"%s\":\n", testPrefixes[i]);
        TrieType t = {.autocompleteBuf = {0}, .autocompleteBufSz = 0};
        Trie* subtree = getPrefixSubtree(root, (char*)testPrefixes[i], &t);
        if (!subtree) {
            printf("  (no matches)\n");
            continue;
        }
        char** matches = assembleTree(subtree, &t);
        for (int j = 0; matches[j]; ++j) {
            printf("  %s\n", matches[j]);
            free(matches[j]);
        }
        free(matches);
    }

    trieFree(root);
    return 0;
}
