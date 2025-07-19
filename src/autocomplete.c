/*
Author: David Xu
2025-07-15

Autocompletion functions that use GNU readline for buffering and hooks,
but my own custom completion and display functions.

*/
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <readline/readline.h>
#include <dirent.h>
#include <sys/stat.h>


#include "autocomplete.h"
#include "prefixTree.h"

static int tab_handler(int count, int key);
static char** executable_ac(const char* text, int start, int end);
static char** filename_ac(const char* text, int start, int end);
static char** filename_ac_helper(const char* text, int start, int end);
static char* find_lcp(char** matches, const char* text);
static char* exe_generator(const char* text, int state);
static char* filepath_generator(const char* text, int state);
static void populate_builtin_tree(trie *root);
static void populate_exe_tree(trie* root);
static int populate_filepath_tree(trie* root, const char* directory);
static void display_matches(char **matches, int num_matches, int max_length);

static bool did_autocomplete = false;
static bool multiple_matches = false;

trie* builtin_tree_root = NULL;
trie* exe_tree_root = NULL;
trie* filepath_tree_root = NULL;

const char* builtin_cmds[] = {"type", "echo", "exit", "pwd", "history", "cd", NULL};

void init_ac_readline(void) {
    rl_completer_word_break_characters = 
        strdup(" \t\n\"\\'`@$><;|&{(");
    rl_attempted_completion_function = autocomplete;
    rl_completion_display_matches_hook = display_matches;
    rl_bind_key('\t', tab_handler);
}

void init_ac(void) {
    builtin_tree_root = trie_create(); // exit, echo
    populate_builtin_tree(builtin_tree_root);

    exe_tree_root = trie_create(); // from PATH
    populate_exe_tree(exe_tree_root);
}

void cleanup_ac(void) {
    trie_free(builtin_tree_root);
    trie_free(exe_tree_root);
    trie_free(filepath_tree_root);
}

static int tab_handler(int count, int key) {
    did_autocomplete = false;
    // upon second consecutive TAB, call hook to our custom display_matches() function.
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
    // rl_completion_append_character = '\0';
    rl_redisplay();
    return 0;
}

static void display_matches(char **matches, int num_matches, int max_length) {
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
static void populate_builtin_tree(trie *root) {
    for(size_t i = 0; builtin_cmds[i]; ++i) {
        trie_insert(root, (char*) builtin_cmds[i]);
    }
}

/// @brief scan directory for files and add the full file paths to the trie 
/// @param root of trie
/// @param directory target directory
/// @return 1 if scandir failed, 0 otherwise
static int populate_filepath_tree(trie* root, const char* directory) {
    struct dirent** exe_list = NULL;
    int numExe = scandir(directory, &exe_list, NULL, alphasort);
    if (numExe <= 0) {
        return 1;
    }

    for (int i = 0; i < numExe; ++i) {
        char* filepath = malloc(strlen(directory) + strlen(exe_list[i]->d_name) + 1);
        memcpy(filepath, directory, strlen(directory) + 1);
        strcat(filepath, exe_list[i]->d_name);
        trie_insert(root, filepath);
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
static void populate_exe_tree(trie *root) {
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
            trie_insert(root, exe_list[i]->d_name);
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
       matches = executable_ac(text, start, end);
    } else { 
        matches = filename_ac(text, start, end);
        if (matches == NULL) { // TODO for pipelined command 
            // matches = executable_ac(text, start, end);
        }
    }
    return matches;
}

/// @brief autocompletion by calling 'exe_generator' which generates an array of executable/builtin, manually completes to longest common prefix if possible
/// @param text word that TAB was pressed on
/// @param start start index
/// @param end end index
/// @return array of strings for possible matches, NULL means completion was inserted manually
static char** executable_ac(const char* text, int start, int end) {
    char** matches = NULL;
    matches = rl_completion_matches(text, exe_generator);
    if (matches) {
        char* prefix = find_lcp(matches, text);
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

/// @brief If a '/' is present in text, then complete on that directory, otherwise use current directory
/// @param text file path that needs to be completed
/// @param start start index
/// @param end end index
/// @return array of strings for possible matches, NULL means completion was inserted manually
static char** filename_ac(const char* text, int start, int end) {
    char** matches = NULL;

    if (filepath_tree_root) {
        trie_free(filepath_tree_root);
    }
    filepath_tree_root = trie_create();

    char* last_slash_addr = strrchr(text, '/');
    if (last_slash_addr != NULL) {
        // get the file path up until the last "/"
        size_t index = (size_t) (last_slash_addr - text);
        char* curr_file_path = malloc(index + 2);
        memcpy(curr_file_path, text, index + 1);
        curr_file_path[index + 1] = '\0'; 
        
        int result = populate_filepath_tree(filepath_tree_root, curr_file_path);
        free(curr_file_path);
        if (result) return NULL;

        matches = filename_ac_helper(text, start, end);
    } else {
        // completing in current directory
        int result = populate_filepath_tree(filepath_tree_root, "./");
        if (result) return NULL;
    
        char current_dir[PATH_MAX];
        strcpy(current_dir, "./");
        strcat(current_dir, text);

        matches = filename_ac_helper(current_dir, start, end);
    }
    return matches;
}

/// @brief calls functions to obtain array of matches, longest common prefix, or does manual insertion of text
/// @param text file path that needs to be completed
/// @param start start index
/// @param end end index
/// @return array of strings for possible matches, NULL means completion was inserted manually
static char** filename_ac_helper(const char* text, int start, int end) {
    char** matches = NULL;
    matches = rl_completion_matches(text, filepath_generator);
    
    if (matches) {
        char* prefix = find_lcp(matches, text);
        if (prefix) {
            if (strcmp(prefix, text)) {
                did_autocomplete = true;

                char* lcp = prefix;
                struct stat st;
                if ((stat(lcp, &st) == 0) && S_ISDIR(st.st_mode)) {
                    rl_insert_text("/");
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
            // current text is already lcp
            multiple_matches = true;
            free(prefix);
            return matches; // return array for display_matches
        } else { // SINGLE MATCH IN MATCHES ARRAY
            did_autocomplete = true;

            char *match = matches[0];

            if (strncmp(match, "./", 2) == 0) {
                match += 2; 
            }

            rl_delete_text(start, end);
            rl_point = start;
            rl_insert_text(match);
            // append '/' if directory
            struct stat st;
            if ((stat(match, &st) == 0) && S_ISDIR(st.st_mode)) {
                rl_insert_text("/");
            }
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

/// @brief iterate through array of matches until text indices do not match
/// @param str_arr array of autocompletion matches
/// @param text text to be autocompleted
/// @return mallocd string for longest common prefix (lcp), MUST BE FREE'D BY CALLER, returns NULL if only a single match in array
static char* find_lcp(char** matches, const char* text) {
    char** first_match = matches; 

    char* lcp = *first_match; // set the lcp to an arbitrary match since it cannot be greater in length than any individual match
    // * NOTE: there are no string length checks here since the strings in the matches array are guaranteed to all contain text as a prefix
    int idx = strlen(text) - 1; // current index of lcp

    if (*(first_match + 1) == NULL) return NULL; // only one match

    bool does_match = true;
    while (lcp[idx] != '\0') {
        for (char** i = first_match + 1; *i; ++i) {
            if ( ((*i)[idx] == '\0') || ((*i)[idx] != (*first_match)[idx]) ) {
                does_match = false;
                break; 
            }
        }
        if (!does_match) break;
        ++idx;
    }

    return strndup(lcp, idx); // strndup adds the null terminator if not in duplicated bytes
}

/// @brief builds match array by obtaining subtree of current text, returns elements from array
/// @param text text to be autocompleted
/// @param state integer for how many number of times generator fcn is called, 0 means first for current text
/// @return current element of match array
static char* filepath_generator(const char* text, int state) {
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
        trie_type filepath = {.autocomplete_buf = {0}, .autocomplete_buf_sz = 0};
        trie* subtree = get_prefix_subtree(filepath_tree_root, (char*)text, &filepath);
        if (subtree) {
            match_arr = assemble_trie(subtree, &filepath);    
        }
    }

    if (!match_arr || !match_arr[list_idx]) {
        return NULL;
    }
    return match_arr[list_idx++];
}

static char* exe_generator(const char* text, int state) {
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
        trie_type builtin = {.autocomplete_buf = {0}, .autocomplete_buf_sz = 0};
        trie* subtree = get_prefix_subtree(builtin_tree_root, (char*)text, &builtin);
        if (subtree) {
            match_arr = assemble_trie(subtree, &builtin);
        // if not found in builtin_tree, then search exe_tree
        } else {
            trie_type exe = {.autocomplete_buf = {0}, .autocomplete_buf_sz = 0};
            trie* exe_subtree = get_prefix_subtree(exe_tree_root, (char*) text, &exe);
            if (exe_subtree) {
                match_arr = assemble_trie(exe_subtree, &exe);
            } else {
                match_arr = NULL; // * NO COMPLETIONS POSSIBLE, RETURN NULL TO THEN RING BELL IN tab_handler(), took a really long time to debug this... 
            }
        }
    }
    
    if (!match_arr || !match_arr[list_idx]) { // sentinel of array is NULL
        return NULL; // no completion done
    }
    return match_arr[list_idx++];
}