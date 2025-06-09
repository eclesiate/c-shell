#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>
#include <dirent.h>

static const char* allowableCmds[] = {"type", "echo", "exit", "pwd", NULL};
static const char specialChars[] = {'$', '\"', '\'', '\0'};

int handleInputs(const char* input);
char** tokenize(char* line);
int findExecutableFile(const char* filename, char** exepath);
void runExecutableFile(char** argv);
void typeCmd(char** arg, char** exePath);
void echoCmd(char** msg);
void printWorkingDirectory();
void changeDir(char** argv);
void singleQuotes(const char* arg);
void doubleQuotes(const char* arg);
void doubleQuotingTest(char* str);
void removeBackslash(char* str, int isOutsideQuotes);

int main(int argc, char* argv[]) {
    while (1) {
        // Flush after every printf, even without \n
        setbuf(stdout, NULL);

        printf("$ ");

        // Wait for user input
        char input[100];
        fgets(input, 100, stdin);
        input[strcspn(input, "\n")] = '\0'; // replace index of newline escape char with null terminator
        
        if (handleInputs(input)) {
            break; // exit cmd
        }
    }

    return 0;
}
/// @brief tokenizes string for handling, then parses each argument and executes commands
/// @param input user input 
/// @return 1 for break command to end program, 0 otherwise
int handleInputs(const char* input) {
    char* inputDup = strdup(input); // since strtok is destructive 
    char* ptr = inputDup;
    // since I dont know the size of exePath, declare as NULL and pass it's address into findExecutableFile()
    char* exePath = NULL; // NOTE. for some reason, executing a file in PATH does not need the full path, so this is kinda useless

    char** argv = tokenize(ptr);
    printf("%s, %s\n", argv[0], argv[1]);
    if (!strncmp(argv[0], "exit", 4) && !strncmp(argv[1], "0", 1)) {
        free(inputDup);
        return 1;

    } else if (!strncmp(argv[0], "echo", 4)) { 
        echoCmd(argv);
    
    } else if (!strncmp(argv[0], "pwd", 3)) {
        printWorkingDirectory();

    } else if (!strncmp(argv[0], "cd", 2)) {
        changeDir(argv);

    } else if (!strncmp(argv[0], "type", 4)) {
        typeCmd(argv, &exePath);

    // run unquoted or quoted executable from PATH
    } else if (findExecutableFile(argv[0], &exePath)) {
        runExecutableFile(argv);
        free(exePath);

    } else {
        printf("%s: command not found\n", input);
    }
    
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
            char* start = ptr;
            while(*ptr == ' ') {
                ++ptr;
            }
            if (start != ptr) { memmove(start, ptr, strlen(ptr) + 1); }

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
                    if (*ptr == '\\' && (ptr[1] == '"' || ptr[1] == '\\' || ptr[1] == '$' || ptr[1] == ' ')) {
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
            printf("%s is %s\n", type, exePath);
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
        struct dirent** exeList;
        int numExe = scandir(currPath, &exeList, NULL, alphasort);
        while (numExe--) {
            if (!strcmp(exeList[numExe]->d_name, filename)) {
                exeFound = true;
                //  free the current entry, and all remaining entries before breaking to avoid memory leak!
                while (numExe--) {
                    free(exeList[numExe]);
                }
                size_t buflen = strlen(currPath) + strlen(filename) + 2; // +1 for '/'
                *exePath = malloc(buflen);
                snprintf(*exePath, buflen, "%s/%s", currPath, filename);
                break;
            }
            free(exeList[numExe]);
        }
        free(exeList);

        if (exeFound) break;
        currPath = strtok(NULL, ":");
    }
    free(pathCopy);

    if (exeFound) return 1;
    return 0;
}

void runExecutableFile(char** argv) {
    size_t buflen = 0;
    for (size_t i = 0; argv[i]; ++i) {
        buflen += strlen(argv[i]) + 1; // for space char
    }
    char* exeCmd = malloc(buflen);
    strcat(exeCmd, argv[0]);
    for (size_t i = 1; argv[i]; ++i) {
        strcat(exeCmd, " ");
        strcat(exeCmd, argv[i]);
    }
    int returnCode = system(exeCmd);
    free(exeCmd);
    // This is not robust error handling but I don't think it matters for now
    if(returnCode == -1) {
        printf("Failed to execute file %s, return code: %d", argv[0], returnCode);
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