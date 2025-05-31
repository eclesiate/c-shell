#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

static const char* allowableCmds[] = {"type", "echo", "exit", NULL};

int handleInputs(char* input);
int findExecutableFile(char* type, char** exepath);
void runExecutableFile(char* exePath);

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

    if (!strcmp(input, "exit 0")) {
        free(inputDupForStrtok);
        return 1;
    } 
    else if (!strncmp("echo", input, 4)) {
        char* echoed = input + 5; // pointer arithmetic onto remainder/argument, extra + 1 for space: "_"
        printf("%s\n", echoed);
    } 
    else if (!strncmp("type", input, 4)) {
        char* type = input + 5;
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
            } 
            else {
                printf("%s: not found\n", type);
            }
        }
    } 
    // RUN EXECUTABLE FILE: parse first argument and search for its .exe
    else if (findExecutableFile(strtok(inputDupForStrtok, " "), &exePath)) {
        runExecutableFile(exePath);
    } 
    else {
        printf("%s: command not found\n", input);
    }
    free(inputDupForStrtok);
    free(exePath);
    return 0;
}


int findExecutableFile(char *type, char **exePath) {
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
                *exePath = malloc(sizeof(currPath) + sizeof(type));
                snprintf(*exePath, sizeof(exePath), "%s/%s", currPath, type);
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

void runExecutableFile(char* exePath) {
    char* exeCmd = malloc(sizeof("./") + sizeof(exePath) - 1); // minus 1 since we dont need (>1) null terminators
    exeCmd = "./";
    strcat(exeCmd, exePath);
    int returnCode = system(exeCmd);
    free(exeCmd);
    // This is not robust error handling but I don't think it matters for now
    if(returnCode == -1) {
        printf("Failed to execute file %s, return code: %d", exePath, returnCode);
    } 
}