#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

static const char* allowableCmds[] = {"type", "echo", "exit", NULL};

int handleInputs(const char* input);
int findExecutableFile(const char* type, char** exepath);
void runExecutableFile(char* exePath, char* args);

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
    char* exePath = NULL; // since I dont know the size of exePath, declare as NULL and pass it's address into findExecutableFile()
    char* saveptr1 = NULL;
    const char* exeName = strtok_r(inputDupForStrtok, " ", &saveptr1);

    if (!strcmp(input, "exit 0")) {
        free(inputDupForStrtok);
        return 1;
    } 
    else if (!strncmp("echo", input, 4)) { 
        printf("%s\n", input + 5); // pointer arithmetic onto remainder/argument, extra + 1 for space: "_"
    } 
    else if (!strncmp("type", input, 4)) {
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
            } 
            else {
                printf("%s: not found\n", type);
            }
        }
    } 
    // RUN EXECUTABLE FILE: parse first argument and search for its .exe
    else if (findExecutableFile(exeName, &exePath)) {
        char* args = strtok_r(NULL, "\t\n\0", &saveptr1);
       // printf("args: %s\t\n", args);
        runExecutableFile(exePath, args);
    } 
    else {
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
    char* currPath = strtok(pathCopy, ":");
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

void runExecutableFile(char* exePath, char* args) { // TODO. add support for multiple args, for now this is hardcoded to just get the rest of the input string?
    char* exeCmd = malloc(strlen("./") + strlen(exePath) + strlen(args) + 1);
    sprintf(exeCmd,"./%s %s", exePath, args);
    int returnCode = system(exeCmd);
    free(exeCmd);
    // This is not robust error handling but I don't think it matters for now
    if(returnCode == -1) {
        printf("Failed to execute file %s, return code: %d", exePath, returnCode);
    } 
}