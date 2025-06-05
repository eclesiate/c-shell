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
int findExecutableFile(const char* type, char** exepath);
void runExecutableFile(const char* exeName, char* args);
void printWorkingDirectory();
void changeDir(char* savePtr);
void singleQuotes(const char* arg);
void doubleQuotes(const char* arg);
void doubleQuotingTest(char* str);
void removeBackslash(char* str, int isOutsideQuotes);

int main(int argc, char* argv[]) {
    while (1) {
        // Flush after every printf, even without \n
        setbuf(stdout, NULL);

        // Uncomment this block to pass the first stage
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

int handleInputs(const char* input) {
    char* inputDupForStrtok = strdup(input); // since strtok is destructive
    char* ptr = inputDupForStrtok;
    char* exePath = NULL; // since I dont know the size of exePath, declare as NULL and pass it's address into findExecutableFile()
    char* saveptr1 = NULL;
    const char* firstArg = strtok_r(ptr, " ", &saveptr1);

    if (!strcmp(input, "exit 0")) {
        free(inputDupForStrtok);
        return 1;
    } else if (!strncmp("echo", input, 4)) { 
        if (*(input + 5) == '\'') { // assumes that the single quote is 1 index after the white space
            singleQuotes(inputDupForStrtok + 5);
        } else if (*(input + 5) == '\"') {
            doubleQuotingTest(inputDupForStrtok+5);
        } else {
            int isOutsideQuotes = 1;
            if(strchr(inputDupForStrtok + 5, '\\')) {
               removeBackslash(inputDupForStrtok + 5, isOutsideQuotes);
               printf("%s\n", inputDupForStrtok+5);
            // below is for edge case where extra whitespaces without backslash get stripped
            } else {
                char* echoArgs;
                while((echoArgs = strtok_r(NULL, " ", &saveptr1))) {
                     printf("%s ", echoArgs);
                }
               printf("\n");
            }
        }
    } else if (!strncmp(firstArg, "pwd", 3)) {
        printWorkingDirectory();

    } else if (!strncmp(firstArg, "cd", 2)) {
        changeDir(saveptr1);

    } else if (!strncmp("type", input, 4)) {
        const char* type = input + 5;
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
            if (findExecutableFile(type, &exePath)) {
                printf("%s is %s\n", type, exePath);
            } else {
                printf("%s: not found\n", type);
            }
        }
    } 
    // RUN EXECUTABLE FILE: parse first argument and search for its .exe
    else if (findExecutableFile(firstArg, &exePath)) {
        char* args = strtok_r(NULL, "\t\n\0", &saveptr1);
        runExecutableFile(firstArg, args);
        
    } else {
        printf("%s: command not found\n", input);
    }
    
    free(exePath);
    free(inputDupForStrtok);
    return 0;
}

/*
@params - exePath: double pointer since the length of the path is dependent on what the PATH is (not known at compile)
*/
int findExecutableFile(const char *type, char **exePath) {
    // search for executable programs in PATH
    const char* path = getenv("PATH");
    // if we do not duplicate the path then we are actually editing the PATH environment everytime we tokenize on dir upon calling this func!
    char* pathCopy = strdup(path);
    char* scan = pathCopy;
    char* currPath = strtok(scan, ":");
    bool exeFound = false;

    while (currPath) {
        struct dirent** exeList;
        int numExe = scandir(currPath, &exeList, NULL, alphasort);
        while (numExe--) {
            if (!strcmp(exeList[numExe]->d_name, type)) {
                exeFound = true;
                //  free the current entry, and all remaining entries before breaking to avoid memory leak!
                while (numExe--) {
                    free(exeList[numExe]);
                }
                size_t buflen = strlen(currPath) + strlen(type) + 2; // +1 for '/'
                *exePath = malloc(buflen);
                snprintf(*exePath, buflen, "%s/%s", currPath, type);
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

void runExecutableFile(const char* exeName, char* args) { // TODO. add support for multiple args, for now this is hardcoded to just get the rest of the input string?
    size_t buflen = strlen(exeName) + strlen(args) + 2;
    char* exeCmd = malloc(buflen);
    snprintf(exeCmd, buflen, "%s %s", exeName, args);
    int returnCode = system(exeCmd);
    free(exeCmd);
    // This is not robust error handling but I don't think it matters for now
    if(returnCode == -1) {
        printf("Failed to execute file %s, return code: %d", exeName, returnCode);
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

void changeDir(char* saveptr) {
    const char* targetPath = strtok_r(NULL, " \t\n\0", &saveptr);
    if (!strncmp(targetPath, "~", 1)) {
        const char* home = getenv("HOME");
        targetPath = home;
    }
    if (chdir(targetPath)) {
        printf("cd: %s: No such file or directory\n", targetPath);
    }
}
/*
Method prints message surrounded by single quotes as is, ie does not strip inner white space
*/
void singleQuotes(const char* arg) { // maybe in the future add a function to strip trailing and leading white spaces
    char* saveptr;
    char* dupArg = strdup(arg);
    char* ptr = dupArg;
    char* msg = strtok_r(ptr, "\'", &saveptr);
    if (msg) {
        printf("%s", msg);
    }

    while((msg = strtok_r(NULL, "\'", &saveptr))) {
        printf("%s", msg);
    }
    printf("\n");
    free(dupArg);
}

void doubleQuotes(const char* arg) {    
    int isOutsideQuotes = 0;
    char* saveptr;
    char* dupArg = strdup(arg);
    char* ptr = dupArg;
    // if(strchr(ptr, '\\')) {
    //     removeBackslash(ptr, isOutsideQuotes);
    // }
    char* msg = strtok_r(ptr, "\"", &saveptr);
    
    if (msg) {
        printf("%s", msg);
    }
    // TODO. fix this approach, it passes the test cases since its kinda hardcoded, instead use single for loop that looks at every char.
    while((msg = strtok_r(NULL, "\"", &saveptr))) { 
        if(strchr(msg, '\\')) {
            removeBackslash(msg, isOutsideQuotes);
        }
        if ((*msg == ' ')) {
            printf(" ");
            while(*msg == ' ') {
                ++msg;
            }
        }
        printf("%s", msg);
    }
    printf("\n");
    free(dupArg);
}

void doubleQuotingTest(char* str) {
    bool insideQuotes = false;
    bool escapedQuote = false;
    bool escapedBackSlash = false;
    char* src;
    char* dst;

    for (src = dst = str; *src != '\0'; ++src) {
        *dst = *src;
        if (!insideQuotes) {
            if (*dst == '\"' && !escapedQuote) {
                insideQuotes = true;
            } else if (*dst != '\\') { 
                ++dst; 
                escapedQuote = false;// if there is an escaped double quote, then the double quote is preserved
            // outside of quotes, the char proceeding a backslash is preserved: do not increment dst ptr to omit '\'
            } else {
                if (*(src + 1) == '\\') {
                    ++dst;
                } else if (*(src + 1) == '\"') { escapedQuote = true; }
            }
        } else {
            if (*dst == '\"' && !escapedQuote) {
                insideQuotes = false;
            } else if (*dst != '\\') { 
                ++dst; // if there is an escaped double quote, then the double quote is preserved
                escapedQuote = false;
            } else {
                if (escapedBackSlash) {
                    escapedBackSlash = false;
                } else if (*(src + 1) == '\\') {
                    ++dst;
                    escapedBackSlash = true;
                } else if (*(src + 1) == '\"') { escapedQuote = true; }
                // $ is preserved
                else if (*(src + 1) != '$') { ++dst; }
            }
        }
    }
    *dst = '\0';
    printf("%s\n", str);
}

void removeBackslash(char* str, int isOutsideQuotes) {
    char* src; 
    char* dst;
    bool isDoubleBackSlash = false;

    for (src = dst = str; *src != '\0'; ++src) {
        *dst = *src;
        if (*dst != '\\') {
            ++dst;
        } else {
            // for a double bslash, we want to preserve one bslash, so 
            if (*(src + 1) == '\\') {
                ++dst;
                continue;        
            }
            // if proceding char of of the special chars \ $
            for (const char* i = specialChars; *i != '\0'; ++i) {
                if (*(src + 1) == *i) {
                    break;
                }
            }
           // if (!isOutsideQuotes) { ++dst; }
           
        }
    }
    *dst = '\0';
}