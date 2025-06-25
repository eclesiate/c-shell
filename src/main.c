#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "prefixTree.h"

static const char* allowableCmds[] = {"type", "echo", "exit", "pwd", NULL};

int handleInputs(const char* input);
char** tokenize(char* line);
int handleOutputRedir(char** argv);
int findExecutableFile(const char* filename, char** exepath);
void runExecutableFile(char** argv, char* fullpath);
void typeCmd(char** arg, char** exePath);
void echoCmd(char** msg);
void printWorkingDirectory();
void changeDir(char** argv);

void initializeReadline(void);
static int tabHandler(int count, int key);
char** autocomplete(const char* text, int start, int end);
void displayMatches(char **matches, int num_matches, int max_length);
char* builtinGenerator(const char* text, int state);
void populatePrefixTree(Trie *root);
void populateExeTree(Trie* root);

Trie* builtin_tree_root = NULL;
Trie* exe_tree_root = NULL;

int main(int argc, char* argv[]) {

    char* line = NULL;
    initializeReadline();

    builtin_tree_root = trieCreate(); 
    populatePrefixTree(builtin_tree_root);

    exe_tree_root = trieCreate();
    populateExeTree(exe_tree_root);
    
    while (1) {
        line = readline("$ ");

        if (line == NULL) break;
        
        if (handleInputs(line)) {
            free(line);
            break; // exit cmd
        }

        free(line);
    }
    trieFree(builtin_tree_root);
    return 0;
}

void initializeReadline(void) {
    rl_attempted_completion_function = autocomplete;
    rl_completion_display_matches_hook = displayMatches;
    rl_bind_key('\t', tabHandler);
}

static int tabHandler(int count, int key) {
    static short tab_press_count = 0;

    int n = rl_complete(count, key);

if (n <= 0) {
  // no matches: bell & reset
  putchar('\a');
  tab_press_count = 0;
}
else if (n == 1) {
  // single match: clear any previous “multi-match” state
  tab_press_count = 0;
}
else {
  // multiple matches
  tab_press_count++;
  if (tab_press_count == 1) {
    // first TAB on >1 matches: just bell
    putchar('\a');
  } else {
    // second TAB: list them
    rl_complete_internal('!');
    tab_press_count = 0;
  }
}
    // int num_matches = rl_complete(count, key);
    // ++tabCnt;
    // if (num_matches == 0) {
    //     fprintf(stdout, "\x07");
    //     fflush(stdout);
    //     rl_redisplay();
    //     tabCnt = 0;
    // } else if (num_matches > 1) {
    //     if (tabCnt > 1) {
    //         rl_possible_completions(count, key);
    //         tabCnt = 0;
    //     } else {
    //         fprintf(stdout, "\x07");
    //         fflush(stdout);
    //         rl_redisplay();
    //     }
        
    // }
    return 0;
}

void displayMatches(char **matches, int num_matches, int max_length) {
    printf("\n");
    for (int i = 0; i < num_matches; ++i) {
        printf("%s  ", matches[i]);
    }
    printf("\n");
    rl_on_new_line();
    rl_redisplay();
}

// char acBuiltinBuf[AC_BUF_CAP];
// size_t acBuiltinBufSz = 0;

void populatePrefixTree(Trie *root) {
    trieInsert(root, "echo");
    trieInsert(root, "exit");
}

/// @brief add every executable file in PATH to the prefix tree
/// @param root root of executable file tree
/// @return void
void populateExeTree(Trie *root) {
   // search for executable programs in PATH
    const char* path = getenv("PATH");
    // if we do not duplicate the path then we are actually editing the PATH environment everytime we tokenize on dir upon calling this func!
    char* pathCopy = strdup(path);
    char* scan = pathCopy;
    char* currPath = strtok(scan, ":");

    while (currPath) { // this code assumes that only directories are in PATH, maybe call stat and S_ISDIR() to verify 
        struct dirent** exeList = NULL; // *MUST DECLARE AS NULL OTHERWISE ON EDGE CASE THAT FIRST DIRECTRORY SEARCHED GIVES ERR OR 0 ENTRIES, YOU ARE FREEING UNITIALIZED MEMORY!
        int numExe = scandir(currPath, &exeList, NULL, alphasort);
        if (numExe <= 0) { // error or 0 entries in array
            currPath = strtok(NULL, ":");
            if (exeList) free(exeList);
            continue;
        }
        for (int i = 0; i < numExe; ++i) {
            trieInsert(root, exeList[i]->d_name);
        }

        for (int i = 0; i < numExe; ++i) {
            free(exeList[i]);
        }
        free(exeList);

        currPath = strtok(NULL, ":");
    }
    free(pathCopy);
}

char** autocomplete(const char* text, int start, int end) {
    char** matches = NULL;
    rl_attempted_completion_over = 1; // don't use default completion even if no matches were found here
    if (start == 0) { // builtins
        matches = rl_completion_matches(text, builtinGenerator);
    }
    return matches;
}

char* builtinGenerator(const char* text, int state) {
    static char** arrayOfMatches = NULL;
    static int list_idx = 0;

    if (state == 0) {
        if ((arrayOfMatches)) {
            // free unreturned matches
            for (int k = list_idx; arrayOfMatches[k] != NULL; ++k) {
                free(arrayOfMatches[k]);
            }
            free(arrayOfMatches);
        }
        list_idx = 0;
        TrieType builtin = {.autocompleteBuf = {0}, .autocompleteBufSz = 0};
        Trie* subtree = getPrefixSubtree(builtin_tree_root, (char*)text, &builtin);
        if (subtree) {
            arrayOfMatches = assembleTree(subtree, &builtin);
        // if not found in builtin_tree, then search exe_tree
        } else {
            TrieType exe = {.autocompleteBuf = {0}, .autocompleteBufSz = 0};
            Trie* exeSubtree = getPrefixSubtree(exe_tree_root, (char*) text, &exe);
            if (exeSubtree) {
                arrayOfMatches = assembleTree(exeSubtree, &exe);
            } else {
                arrayOfMatches = malloc(sizeof(char*));
                *arrayOfMatches = NULL; // * since gnu readline expects mallocd strings from the generator function
            }
        }
    }

    if (!arrayOfMatches || !arrayOfMatches[list_idx]) {
        return NULL;
    }
    return arrayOfMatches[list_idx++];
}


/// @brief tokenizes string for handling, then parses each argument and executes commands
/// @param input user input 
/// @return 1 for break command to end program, 0 otherwise
int handleInputs(const char* input) {
    char* inputDup = (char*) strdup(input); // since strtok is destructive 
    char* ptr = inputDup;
    // since I dont know the size of exePath, declare as NULL and pass it's address into findExecutableFile()
    char* exePath = NULL; // NOTE. for some reason, executing a file in PATH does not need the full path, so this is kinda useless

    char** argv = tokenize(ptr);

    if (!(*argv)) { // failed to tokenize 
        free(inputDup);
        free(argv);
        return 0; 
    }

    if (!handleOutputRedir(argv)) {
        free(inputDup);
        free(argv);
        return 0;
    
    } else if (!strncmp(argv[0], "exit", 4)) {
        if (argv[1] && !strncmp(argv[1], "0", 1)) {
            free(inputDup);
            free(argv);
            return 1;
        } else {
            printf("%s: not found\n", argv[0]);
        }
    } else if (!strncmp(argv[0], "echo", 4)) { 
        echoCmd(argv);
    
    } else if (!strncmp(argv[0], "pwd", 3)) {
        printWorkingDirectory();

    } else if (!strncmp(argv[0], "cd", 2)) {
        changeDir(argv);

    } else if (!strncmp(argv[0], "type", 4)) {
        typeCmd(argv, &exePath);

    } else if (findExecutableFile(argv[0], &exePath)) {
        runExecutableFile(argv, exePath);

    } else {
        printf("%s: not found\n", argv[0]);
    }
    if(exePath) free(exePath);
    free(argv);
    free(inputDup);
    return 0;
}

/// @brief Categorizing args as IN_DOUBLE, IN_SINGLE, OUTSIDE, using quote rules.
/// @param line the shell input to be tokenized
/// @return argv, NULL terminated array of strings that contain the tokens
char** tokenize(char* line) {

    typedef enum token {
        IN_DOUBLE, IN_SINGLE, OUTSIDE
    } token_t;

    token_t state = OUTSIDE;
    char** argv = NULL;
    size_t argc = 0;
    char* token = NULL;

    bool doneToken = true;
    
    // strip leading whitespaces
    while(*line == ' ') { ++line; }

    char* ptr = line;

    while(*ptr) {
        // continue processing on token or start new one
        if (state == OUTSIDE) {
            // strip leading whitespaces before starting new token
            // * since extra spaces between tokens are collapsed to just one
            while(*ptr == ' ') {
                ++ptr;
            }

            if (*ptr == '\0') break; // reached the end

            token = ptr;

            if (*ptr == '"') {
                    state = IN_DOUBLE;
                    // does not include the quote char
                    if (doneToken) { token = ++ptr; }
            } else if (*ptr == '\'') {
                    state = IN_SINGLE;
                    if (doneToken) { token = ++ptr; }
            } else {
                state = OUTSIDE; 
            }
            // first realloc call with NULL is treated as malloc, doing +2 to account for final NULL entry
            // * note the use of sizeof(char*) since argv is an array of strings!
            if (doneToken) {
                argv = realloc(argv, sizeof(char*) * (argc + 2));
                argv[argc++] = token;
                doneToken = false;
            }
        }
        // processing current token
        switch(state) {
            case IN_SINGLE:
                // condition is irrelevant since if we parsed till the end of a string without closing quote
                // that is an illegal quote
                while (*ptr) {
                    if (*ptr == '\'') {
                        // only start a new token if the char after ' is a SPACE
                        if (*(ptr + 1) == ' ') {
                            *(ptr++) = '\0'; // end current token by replacing '
                            doneToken = true;
                        } else {
                            memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
                        }
                        break;
                    }
                    ++ptr;
                }
                state = OUTSIDE;
                break;
            case IN_DOUBLE:
                while (*ptr) {
                    // preserve backslash rules
                    if (*ptr == '\\' && (ptr[1] == '"' || ptr[1] == '\\' || ptr[1] == '$')) {
                        memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
                    } else if (*ptr == '"') {
                        // only start a new token if the char after " is a SPACE
                        if (ptr[1] == ' ') {
                            *(ptr++) = '\0'; // end current token on "
                            doneToken = true;
                        } else {
                            memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
                        }
                        break;
                    }
                    ++ptr; 
                }
                state = OUTSIDE;
                break;
            // OUTSIDE
            default: 
                while (*ptr) {
                    // preserve backslashed literal
                    if (*ptr == '\\') {
                        memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
                    // unescaped space signals start of new token
                    } else if (*ptr == ' ') {
                        doneToken = true;
                        break;
                    // current token will switch state
                    } else if (*ptr == '\'' || *ptr == '"') {
                        break;
                    }
                    ++ptr;
                }
                // since we didnt move pointer after breaking on a space, space gets overwritten
                if (doneToken && *ptr != '\0') { *(ptr++) = '\0'; }
                state = OUTSIDE;
        }
    }
    if (argv) {
        argv[argc] = NULL;
    } else {
        argv = malloc(sizeof(char*));
        *argv = NULL;
    }
    return argv;
}
/// @brief
///        THE CHAR(S) ">" MUST BE THEIR OWN TOKEN (SEPERATED BY SPACES) TO BE PARSED CORRECTLY
/// @param argv 
/// @return 
int handleOutputRedir(char** argv) {
    bool outReder = false;
    bool errReder = false;
    int appendOut = 0;
    int appendErr = 0;
    int trunc = 0;
    int savedstream = -1;
    int stream = -1;
    int fd = 0;
    int result = -1;
    int outputIdx = -1;
    char* fname = NULL;

    for (int i = 0; argv[i]; ++i) {
        outReder = !strcmp(argv[i], ">") || !strcmp(argv[i], "1>");
        errReder = !strcmp(argv[i], "2>");
        if (!strcmp(argv[i], "1>>") || !strcmp(argv[i], ">>")) {
            appendOut = O_APPEND;
        } else if (!strcmp(argv[i], "2>>") || !strcmp(argv[i], ">>")) {
            appendErr = O_APPEND;
        }
        // set open() flags and stream depending on command
        if (outReder || appendOut) {
            stream = STDOUT_FILENO;
            trunc = appendOut ? 0 : O_TRUNC; // trunc and append are mutually exclusive
        } else if (errReder || appendErr) {
            stream = STDERR_FILENO;
            trunc = appendErr ? 0 : O_TRUNC;
        } else {
            continue;
        }

        outputIdx = i;
        fname = argv[i + 1];
       
        if (fflush(NULL)) { //* flush buffer before changing which fd stdx refers to, that way we dont get any undefined behaviour (was a pain to debug!)
            perror("fflush before dup2");
            exit(1);
        }
        fd = open(fname, O_CREAT | trunc | O_RDWR | appendOut | appendErr, S_IRWXU); // create file if DNE, or fully truncate it if it does, set mode of created file to read, write, ex permissions
        if (fd == -1) {
            perror("open file");
            exit(1);
        }
        savedstream = dup(stream); // REMEMBER TO SAVE ORIGINAL STDx FD (1) SO THAT AFTER THE FUNCTION FINISHES WE CAN PRINT TO STDx INSTEAD OF THE FILE
        result = dup2(fd, stream); // stdx_fileno now refers to fd
        if (result == -1) {
            perror("dup2 fd");
            exit(1);
        }
        close(fd);
        break;
    }

    if (outputIdx < 0) return 1;

    // execute the command whose output gets redirected
    char* exePath = NULL;
    if (findExecutableFile(argv[0], &exePath)) {
        argv[outputIdx] = NULL; // don't treat any tokens past here as arguments
        runExecutableFile(argv, exePath);
        free(exePath);
    } else {
        perror("failed to find executable");
    }
    // once executed, revert back to stdx!
    if (savedstream >= 0) {
        dup2(savedstream, stream);
        close(savedstream); // always close duplicate fds
    } else {
        perror("stdout");
        exit(1);
    }
    return 0;
}

void typeCmd(char** argv, char** exePath) {
    const char* type = argv[1];
    bool isShellBuiltin = false;
    /*
        Chatgpt suggested this very cool way of looping through an array of strings in C.
        like how strings are null terminated, make the last element of the array NULL to act as a "sentinel" for the loop condition
    */
    for (const char** cmd = allowableCmds; *cmd; ++cmd) {
        if (!strcmp(type, *cmd)) {
            printf("%s is a shell builtin\n", type);
            isShellBuiltin = true;
            break;
        }
    }
    // if not recognized as builtin command then search for executable files in PATH
    if (!isShellBuiltin) {
        if (findExecutableFile(type, exePath)) {
            printf("%s is %s\n", type, *exePath);
        } else {
            printf("%s: not found\n", type);
        }
    }
}

void echoCmd(char** argv) {
    for (size_t i = 1; argv[i]; ++i) {
        printf("%s", argv[i]);
        if (argv[i+1]) printf(" ");
    }
    printf("\n");
}

/// @brief my own version of the access() function that attempts to find a file name in PATH
/// @param filename executable file to find in PATH
/// @param exePath buffer that gets malloc'd with full file path
/// @return 1 for success, 0 for failure
int findExecutableFile(const char *filename, char **exePath) {
    // search for executable programs in PATH
    const char* path = getenv("PATH");
    // if we do not duplicate the path then we are actually editing the PATH environment everytime we tokenize on dir upon calling this func!
    char* pathCopy = strdup(path);
    char* scan = pathCopy;
    char* currPath = strtok(scan, ":");
    bool exeFound = false;
    // search through the list of all executable files in each PATH directory
    while (currPath) {
        struct dirent** exeList = NULL; // *MUST DECLARE AS NULL OTHERWISE ON EDGE CASE THAT FIRST DIRECTRORY SEARCHED GIVES ERR OR 0 ENTRIES, YOU ARE FREEING UNITIALIZED MEMORY!
        int numExe = scandir(currPath, &exeList, NULL, alphasort);
        if (numExe <= 0) { // error or 0 entries in array
            currPath = strtok(NULL, ":");
            if (exeList) free(exeList);
            continue;
        }
        for (int i = 0; i < numExe; ++i) {
            if (!strcmp(exeList[i]->d_name, filename)) {
                exeFound = true;
                size_t buflen = strlen(currPath) + strlen(filename) + 2; // +1 for '/'
                *exePath = malloc(buflen);
                snprintf(*exePath, buflen, "%s/%s", currPath, filename);
                break;
            }
        }

        for (int i = 0; i < numExe; ++i) {
            free(exeList[i]);
        }
        free(exeList);

        if (exeFound) break;
        currPath = strtok(NULL, ":");
    }
    free(pathCopy);

    if (exeFound) return 1;
    return 0;
}
/// @brief creates child process that executes command
/// @param argv list of tokens
/// @param fullpath path of exe, decision was made to use this in conjunction with exec instead of exexcvp
void runExecutableFile(char** argv, char* fullpath) {
    pid_t pid = fork(); // gotta fork otherwise if we run execv on the current process, its process image gets replaced and we can never return back to the current program
    int status = 0;
    if (pid < 0) {
        perror("fork failed before running exe");
        exit(1);
    } else if (!pid) { // child process
        execv(fullpath, argv);
        perror("execv");
    } else { // parent process
        do {
            waitpid(pid, &status, WUNTRACED); // reap childprocess
        } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // wait while the child did NOT end normally AND did NOT end by a signal
    }
}

void printWorkingDirectory() {
    long size = pathconf(".", _PC_PATH_MAX); // use '.' for current path
    char* buf; 
    char* pwd;
    if ((buf = malloc((size_t)size)) != NULL) {
        pwd = getcwd(buf, size);
        printf("\r%s\n", pwd);
    }
    free(buf);
}

void changeDir(char** argv) {
    char* targetPath = argv[1];
    if (!targetPath) {
        printf("No second token for cd\n");
        return;
    }
    if (!strncmp(targetPath, "~", 1)) {
        char* home = getenv("HOME");
        targetPath = home;
    }
    if (chdir(targetPath)) {
        printf("cd: %s: No such file or directory\n", targetPath);
    }
}