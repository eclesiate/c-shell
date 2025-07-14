#include "autocomplete.h"
#include "prefixTree.h"

#include <readline/readline.h>
#include <readline/history.h>

#include <stdlib.h>
#include <string.h>

#include <dirent.h>

static int tabHandler(int count, int key);
static char** executableAc(const char* text, int start, int end);
static char** filenameAc(const char* text, int start, int end);
static char** filenameAcHelper(const char* text, int start, int end);
static char* findLongestCommonPrefix(char** str_arr, const char* text);
static char* builtinAndExeGenerator(const char* text, int state);
static char* filePathGenerator(const char* text, int state);
static void populateBuiltinTree(Trie *root);
static void populateExeTree(Trie* root);
static int populateFilePathTree(Trie* root, const char* directory);
static void displayMatches(char **matches, int num_matches, int max_length);

static bool did_autocomplete = false;
static bool multiple_matches = false;

Trie* builtin_tree_root = NULL;
Trie* exe_tree_root = NULL;
Trie* filepath_tree_root = NULL;

void initializeReadline(void) {
    rl_attempted_completion_function = autocomplete;
    rl_completion_display_matches_hook = displayMatches;
    rl_bind_key('\t', tabHandler);
}

void initializeAutocomplete(void) {
    builtin_tree_root = trieCreate(); // exit, echo
    populateBuiltinTree(builtin_tree_root);

    exe_tree_root = trieCreate(); // from PATH
    populateExeTree(exe_tree_root);
}

void cleanupAutocomplete(void) {
    trieFree(builtin_tree_root);
    trieFree(exe_tree_root);
    trieFree(filepath_tree_root);
}

static int tabHandler(int count, int key) {
    did_autocomplete = false;
    // upon second consecutive TAB, call hook to our custom displayMatches() function.
    if (multiple_matches) {
        rl_possible_completions(count, key);
        multiple_matches = false;
    } else {
        rl_complete(count, key);
        if (!did_autocomplete) { // print terminal bell when multiple matches or none
            printf("\x07");
            fflush(stdout);
        }
    }
    rl_completion_append_character = '\0';
    rl_redisplay();
    return 0;
}

void displayMatches(char **matches, int num_matches, int max_length) {
    printf("\n");
    for (int i = 1; i <= num_matches; ++i) {
        printf("%s  ", matches[i]);
    }
    printf("\n");
    rl_on_new_line();
    rl_redisplay();
    multiple_matches = false;
}

/// @brief test function for just the echo and exit builtins
/// @param root of trie
static void populateBuiltinTree(Trie *root) {
    trieInsert(root, "echo");
    trieInsert(root, "exit");
}

/// @brief 
/// @param root of Trie
/// @param directory target directory
/// @return 1 if scandir failed, 0 otherwise
static int populateFilePathTree(Trie* root, const char* directory) {
    struct dirent** exe_list = NULL;
    int numExe = scandir(directory, &exe_list, NULL, alphasort);
    if (numExe <= 0) {
        return 1;
    }

    for (int i = 0; i < numExe; ++i) {
        char* filepath = malloc(strlen(directory) + strlen(exe_list[i]->d_name) + 1);
        memcpy(filepath, directory, strlen(directory) + 1);
        strcat(filepath, exe_list[i]->d_name);
        trieInsert(root, filepath);
        free(filepath);
    }

    for (int i = 0; i < numExe; ++i) {
        free(exe_list[i]);
    }
    free(exe_list);
    return 0;
}

/// @brief add every executable file in PATH to the prefix tree
/// @param root root of executable file tree
/// @return void
static void populateExeTree(Trie *root) {
   // search for executable programs in PATH
    const char* path = getenv("PATH");
    // if we do not duplicate the path then we are actually editing the PATH environment everytime we tokenize on dir upon calling this func!
    char* path_copy = strdup(path);
    char* scan = path_copy;
    char* curr_path = strtok(scan, ":");

    while (curr_path) { // this code assumes that only directories are in PATH, maybe call stat and S_ISDIR() to verify 
        struct dirent** exe_list = NULL; // *MUST DECLARE AS NULL OTHERWISE ON EDGE CASE THAT FIRST DIRECTRORY SEARCHED GIVES ERR OR 0 ENTRIES, YOU ARE FREEING UNITIALIZED MEMORY!
        int numExe = scandir(curr_path, &exe_list, NULL, alphasort);
        if (numExe <= 0) { // error or 0 entries in array
            curr_path = strtok(NULL, ":");
            if (exe_list) free(exe_list);
            continue;
        }
        for (int i = 0; i < numExe; ++i) {
            trieInsert(root, exe_list[i]->d_name);
        }

        for (int i = 0; i < numExe; ++i) {
            free(exe_list[i]);
        }
        free(exe_list);

        curr_path = strtok(NULL, ":");
    }
    free(path_copy);
}

/// @brief autocompletes based on index of word, calls executable or filename autocompletion.
/// @param text word that TAB was pressed on
/// @param start start index
/// @param end end index
/// @return array of strings for possible matches, NULL means completion was inserted manually
char** autocomplete(const char* text, int start, int end) {
    char** matches = NULL;
    rl_attempted_completion_over = 1;
    if (start == 0) {
       matches = executable_autocompletion(text, start, end);
    } else { 
        matches = filename_autocompletion(text, start, end);
        if (matches == NULL) { // for pipelined command
            matches = executable_autocompletion(text, start, end);
        }
    }
    return matches;
}

/// @brief autocompletion by calling 'builtinAndExeGenerator' which generates an array of executable/builtin, manually completes to longest common prefix if possible
/// @param text word that TAB was pressed on
/// @param start start index
/// @param end end index
/// @return array of strings for possible matches, NULL means completion was inserted manually
char** executable_autocompletion(const char* text, int start, int end) {
    char** matches = NULL;
    matches = rl_completion_matches(text, builtinAndExeGenerator);
    if (matches) {
        char* prefix = findLongestCommonPrefix(matches, text);
        if (prefix) {
            if (strcmp(prefix, text)) {
                did_autocomplete = true;
                rl_replace_line(prefix, 0);
                rl_point = strlen(prefix);
                free(prefix);
                for (char** m = matches; *m; ++m) {
                    free(*m);
                }
                free(matches);
                return NULL;
            }
            multiple_matches = true;
            free(prefix);
        } else {
            did_autocomplete = true;
        }
    }
    return matches;
}

/// @brief 
/// @param text file path that needs to be completed
/// @param start start index
/// @param end end index
/// @return array of strings for possible matches, NULL means completion was inserted manually
char** filename_autocompletion(const char* text, int start, int end) {
    char** matches = NULL;
    char* last_slash_addr = strrchr(text, '/');
    if (last_slash_addr != NULL) {
        size_t index = (size_t) (last_slash_addr - text);
        char* curr_file_path = malloc(index + 2);
        memcpy(curr_file_path, text, index + 1);
        curr_file_path[index + 1] = '\0';
        
        int result = populateFilePathTree(filepath_tree_root, curr_file_path);
        if (result) return NULL;

        matches = _filename_autocompletion_helper(text, start, end);
        free(curr_file_path);
    } else {
        // completing in current directory
        int result = populateFilePathTree(filepath_tree_root, "./");
        if (result) return NULL;
    
        char current_dir[PATH_MAX];
        strcpy(current_dir, "./");
        strcat(current_dir, text);

        matches = _filename_autocompletion_helper(current_dir, start, end);
    }
    return matches;
}

/// @brief 
/// @param text 
/// @param start 
/// @param end 
/// @return 
char** _filename_autocompletion_helper(const char* text, int start, int end) {
    char** matches = NULL;
    matches = rl_completion_matches(text, filePathGenerator);
    
    if (matches) {
        char* prefix = findLongestCommonPrefix(matches, text);
        if (prefix) {
            if (strcmp(prefix, text)) {
                did_autocomplete = true;

                char* lcp = prefix;
                struct stat st;
                if ((stat(lcp, &st) == 0) && S_ISDIR(st.st_mode)) {
                    rl_completion_append_character = '/';
                }

                if (strncmp(prefix, "./", 2) == 0) {
                    lcp += 2; // strip './' prefix
                }
                rl_delete_text(start, end);
                rl_point = start;
                rl_insert_text(lcp);
                
                for (char** m = matches; *m; ++m) {
                    free(*m);
                }
                free(matches);
                free(prefix);
                return NULL;
            }
            multiple_matches = true;
            free(prefix);
        } else { // single match
            did_autocomplete = true;
            
            char *match = matches[0];
            // append '/' if directory
            struct stat st;
            if ((stat(match, &st) == 0) && S_ISDIR(st.st_mode)) {
                rl_completion_append_character = '/';
            }

            if (strncmp(match, "./", 2) == 0) {
                match += 2; 
            }

            rl_delete_text(start, end);
            rl_point = start;
            rl_insert_text(match);
            rl_redisplay();
            
            for (char** m = matches; *m; ++m) {
                free(*m);
            }
            free(matches);
            return NULL;
        }
    }
    return matches;
}

/// @brief 
/// @param str_arr 
/// @param text 
/// @return mallocd string for longest common prefix (lcp), MUST BE FREE'D BY CALLER 
char* findLongestCommonPrefix(char** str_arr, const char* text) {
    char** first_match = str_arr;

    char* lcp = *first_match;
    int idx = strlen(text) - 1; // current index of lcp

    if (*(first_match + 1) == NULL) return NULL; // only one match
    
    while (lcp[idx] != '\0') {
        char** iterator = first_match + 1;
        if ((*first_match)[idx] == '\0') return strndup(lcp, idx);
        // printf("Idx: %d\tIterator: %s\n", idx, iterator);
        while(*iterator != NULL) {
            // printf("\tIterator: %s\n", iterator);
            if ((*iterator)[idx] == '\0') return strndup(lcp, idx);
            if ((*iterator)[idx] != (*first_match)[idx]) return strndup(lcp, idx);
            ++iterator;
        }
        ++idx;
    }

    return strndup(lcp, idx); // strndup adds the null terminator if not in duplicated bytes
}

/// @brief 
/// @param text 
/// @param state 
/// @return 
char* builtinAndExeGenerator(const char* text, int state) {
    static char** match_arr = NULL;
    static int list_idx = 0;

    if (state == 0) {
        if ((match_arr)) {
            // free unreturned matches
            for (int k = list_idx; match_arr[k] != NULL; ++k) {
                free(match_arr[k]);
            }
            free(match_arr);
            match_arr = NULL; // * heap use after free err without this line
        }
        list_idx = 0;
        TrieType builtin = {.autocomplete_buf = {0}, .autocomplete_buf_sz = 0};
        Trie* subtree = getPrefixSubtree(builtin_tree_root, (char*)text, &builtin);
        if (subtree) {
            match_arr = assembleTree(subtree, &builtin);
        // if not found in builtin_tree, then search exe_tree
        } else {
            TrieType exe = {.autocomplete_buf = {0}, .autocomplete_buf_sz = 0};
            Trie* exe_subtree = getPrefixSubtree(exe_tree_root, (char*) text, &exe);
            if (exe_subtree) {
                match_arr = assembleTree(exe_subtree, &exe);
            } else {
                match_arr = NULL; // * NO COMPLETIONS POSSIBLE, RETURN NULL TO THEN RING BELL IN TABHANDLER(), took a really long time to debug this... 
            }
        }
    }

    if (!match_arr || !match_arr[list_idx]) {
        return NULL;
    }
    return match_arr[list_idx++];
}

/// @brief 
/// @param text 
/// @param state 
/// @return 
char* filePathGenerator(const char* text, int state) {
    static char** match_arr = NULL;
    static int list_idx = 0;

    if (state == 0) {
        if ((match_arr)) {
            // free unreturned matches, readline frees returned ones
            for (int k = list_idx; match_arr[k] != NULL; ++k) {
                free(match_arr[k]);
            }
            free(match_arr);
            match_arr = NULL;
        }
        list_idx = 0;
        TrieType filepath = {.autocomplete_buf = {0}, .autocomplete_buf_sz = 0};
        Trie* subtree = getPrefixSubtree(filepath_tree_root, (char*)text, &filepath);
        if (subtree) {
            match_arr = assembleTree(subtree, &filepath);    
        }
    }

    if (!match_arr || !match_arr[list_idx]) {
        return NULL;
    }
    return match_arr[list_idx++];
}

