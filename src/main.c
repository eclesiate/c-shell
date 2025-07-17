/*
Author: David Xu
2025-07-11

POSIX compliant shell that features custom autocompletion using prefix trees for executables, builtin's, and filepaths. 
Using GNU readline hooks to intercept TAB presses and to facilitate buffer stuff. 
Allows pipelined commands, command history, and output redirection.

Fixes/Improvements:
- //TODO. change printf's to gnu readline buffer variables instead.
- //TODO. export command does not add new PATH to executable trie
*/

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
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "autocomplete.h"

static const char* builtin_cmds[] = {"type", "echo", "exit", "pwd", "history", "cd", NULL};

int handle_inputs(const char* input);
char** tokenize(char* line);

int handle_out_redir(char** argv);

int _spawn_process(int input_fd, int output_fd, char** command);
int* _get_pipeline_indices(char** argv, int* pipe_cnt);
int _fork_pipes(char** argv, int* pipe_idx_arr, int num_pipes);
int handle_pipelines(char** argv);

int find_exe_files(const char* filename, char** exe_path);
void run_exe_files(char** argv, char* fullpath);

void type_cmd(char** arg, char** exe_path);
void echo_cmd(char** msg);
void print_working_dir();
void change_dir(char** argv);

int is_builtin(char* command);
int run_builtin(char** argv);

int main(int argc, char* argv[]) {

    char* line = NULL;
    init_readline();
    init_ac();

    while (1) {
        line = readline("$ ");

        if (line == NULL) break;
        
        if (handle_inputs(line)) {
            free(line);
            break; // exit cmd
        }

        free(line);
    }
    cleanup_ac();
    return 0;
}

/// @brief tokenizes string for handling, then parses each argument and executes commands
/// @param input user input 
/// @return 1 for break command to end program, 0 otherwise
int handle_inputs(const char* input) {
    char* inputDup = (char*) strdup(input); // since strtok is destructive 
    char* ptr = inputDup;
    // since I dont know the size of exe_path, declare as NULL and pass it's address into find_exe_files()
    char* exe_path = NULL; // NOTE. for some reason, executing a file in PATH does not need the full path, so this is kinda useless

    char** argv = tokenize(ptr);

    if (!(*argv)) { // failed to tokenize 
        free(inputDup);
        free(argv);
        return 0; 
    }

    if (!handle_out_redir(argv)) {
        free(inputDup);
        free(argv);
        return 0;
    } 
    else if (!handle_pipelines(argv)) {
        free(inputDup);
        free(argv);
        return 0;
    } 
    else if (!strncmp(argv[0], "exit", 4)) { // free local resources, run_builtin() does not
        free(inputDup);
        free(argv);
        return 1;
    }
    else if(is_builtin(argv[0])) {
        run_builtin(argv);
        free(inputDup);
        free(argv);
        return 0;
    } 
    else if (find_exe_files(argv[0], &exe_path)) {
        run_exe_files(argv, exe_path);

    } 
    else {
        printf("%s: not found\n", argv[0]);
    }
    if(exe_path) free(exe_path);
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

    bool token_done = true;
    
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
                    if (token_done) { token = ++ptr; }
            } else if (*ptr == '\'') {
                    state = IN_SINGLE;
                    if (token_done) { token = ++ptr; }
            } else {
                state = OUTSIDE; 
            }
            // first realloc call with NULL is treated as malloc, doing +2 to account for final NULL entry
            // * note the use of sizeof(char*) since argv is an array of strings!
            if (token_done) {
                argv = realloc(argv, sizeof(char*) * (argc + 2));
                argv[argc++] = token;
                token_done = false;
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
                            token_done = true;
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
                            token_done = true;
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
                        token_done = true;
                        break;
                    // current token will switch state
                    } else if (*ptr == '\'' || *ptr == '"') {
                        break;
                    }
                    ++ptr;
                }
                // since we didnt move pointer after breaking on a space, space gets overwritten
                if (token_done && *ptr != '\0') { *(ptr++) = '\0'; }
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
/// THE CHAR(S) ">" MUST BE THEIR OWN TOKEN (SEPERATED BY SPACES) TO BE PARSED CORRECTLY
/// @param argv 
/// @return 
int handle_out_redir(char** argv) {
    bool out_reder = false;
    bool err_reder = false;
    int append_out = 0;
    int append_err = 0;
    int trunc = 0;
    int saved_stream = -1;
    int stream = -1;
    int fd = 0;
    int result = -1;
    int output_idx = -1;
    char* fname = NULL;

    for (int i = 0; argv[i]; ++i) {
        out_reder = !strcmp(argv[i], ">") || !strcmp(argv[i], "1>");
        err_reder = !strcmp(argv[i], "2>");
        if (!strcmp(argv[i], "1>>") || !strcmp(argv[i], ">>")) {
            append_out = O_APPEND;
        } else if (!strcmp(argv[i], "2>>") || !strcmp(argv[i], ">>")) {
            append_err = O_APPEND;
        }
        // set open() flags and stream depending on command
        if (out_reder || append_out) {
            stream = STDOUT_FILENO;
            trunc = append_out ? 0 : O_TRUNC; // trunc and append are mutually exclusive
        } else if (err_reder || append_err) {
            stream = STDERR_FILENO;
            trunc = append_err ? 0 : O_TRUNC;
        } else {
            continue;
        }

        output_idx = i;
        fname = argv[i + 1];
       
        if (fflush(NULL)) { // * flush buffer before changing which fd stdx refers to, that way we dont get any undefined behaviour (was a pain to debug!)
            perror("fflush before dup2");
            exit(1);
        }
        fd = open(fname, O_CREAT | trunc | O_RDWR | append_out | append_err, S_IRWXU); // create file if DNE, or fully truncate it if it does, set mode of created file to read, write, ex permissions
        if (fd == -1) {
            perror("open file");
            exit(1);
        }
        saved_stream = dup(stream); // * REMEMBER TO SAVE ORIGINAL STDx FD (1) SO THAT AFTER THE FUNCTION FINISHES WE CAN PRINT TO STDx INSTEAD OF THE FILE
        result = dup2(fd, stream); // stdx_fileno now refers to fd
        if (result == -1) {
            perror("dup2 fd");
            exit(1);
        }
        close(fd);
        break;
    }

    if (output_idx < 0) return 1;

    // execute the command whose output gets redirected
    char* exe_path = NULL;
    if (find_exe_files(argv[0], &exe_path)) {
        argv[output_idx] = NULL; // don't treat any tokens past here as arguments
        run_exe_files(argv, exe_path);
        free(exe_path);
    } else {
        perror("failed to find executable");
    }
    // once executed, revert back to stdx!
    if (saved_stream >= 0) {
        dup2(saved_stream, stream);
        close(saved_stream); // always close duplicate fds
    } else {
        perror("stdout");
        exit(1);
    }
    return 0;
}

/// @brief 
/// @param argv 
/// @return 
int* _get_pipeline_indices(char** argv, int* pipe_cnt) {
    int* pipe_idx_arr = NULL;
    // count indices to malloc array
    int pipe_cntr = 0;
    for (int i = 0; argv[i]; ++i) {
        if (!strcmp(argv[i], "|")) ++pipe_cntr;
    }
    if (pipe_cntr == 0) return NULL;
    pipe_idx_arr = malloc((pipe_cntr + 1) * sizeof(int)); // use '-1' sentinel
    // store pipeline indices to be split on
    int curr_idx = 0;
    for (int i = 0; argv[i]; ++i) {
        if (!strcmp(argv[i], "|")) {
            pipe_idx_arr[curr_idx++] = i;
        }
    }
    pipe_idx_arr[curr_idx] = -1; // TODO. maybe remove this sentinel logic
    *pipe_cnt = curr_idx;
    return pipe_idx_arr;
}

int _spawn_process(int input_fd, int output_fd, char** command) {
    pid_t parent = fork();
    if (!parent) {
        if (input_fd != STDOUT_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != STDIN_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        if (is_builtin(command[0])) { // NOTE. assuming builtin is always the first command
            int status = run_builtin(command);
            exit(status);
        }
        
        return execvp(command[0], command); // decision made not to use my find_exe_files function here
        perror("execvp"); 
        exit(1);
    }
    return 0;
}

int _fork_pipes(char** argv, int* pipe_idx_arr, int num_pipes) {
    int fd[2]; // [0] for read, [1] for write
    int inputfd = STDIN_FILENO;
    int size = 0;
    char** argv1 = NULL;

    for (int i = 0; i < num_pipes; ++i) {
        if (pipe(fd)) {
            perror("pipe");
            exit(1);
        }
        if (i == 0) {
            size = pipe_idx_arr[i];
        } else {
            size = pipe_idx_arr[i] - pipe_idx_arr[i - 1] - 1;
        }
        argv1 = malloc(sizeof(char*) * (size + 1)); // +1 for NULL sentinel
        argv1[size] = NULL;
        size_t start = (i == 0) ? 0 : pipe_idx_arr[i - 1] + 1;
        memcpy(argv1, argv + start, size * sizeof(char*));
        _spawn_process(inputfd, fd[1], argv1);
        free(argv1);
        close(fd[1]); // parent can only be here at this point
        inputfd = fd[0]; // inputfd for next command is the read end of the pipe
    }
    if (inputfd != STDIN_FILENO) {
        dup2(inputfd, STDIN_FILENO);
        close(inputfd);
    }
    // NOTE: I decided not to call waitpid on all the child processes since I believe that 
    // this approach is easy to understand and has the necessary parallelization
    char** last_cmd = argv + pipe_idx_arr[num_pipes - 1] + 1;

    if (is_builtin(last_cmd[0])) { // NOTE. assuming builtin is always the first command
        int status = run_builtin(last_cmd);
        exit(status);
    }
    return execvp(last_cmd[0], last_cmd);
}

/// @brief 
/// @param argv 
/// @return 
int handle_pipelines(char** argv) {
    int pipe_cnt = 0;
    int* pipe_idx_arr = _get_pipeline_indices(argv, &pipe_cnt); // ! notice that we can't use my ARRAY_LEN macro on this pointer since sizeof() only works on arrays whose length are known at compile time
    if (pipe_idx_arr == NULL) { 
        free(pipe_idx_arr); 
        return 1; 
    } 

    pid_t parent = fork(); 
    int status = 0;

    if (!parent) { 
        if (_fork_pipes(argv, pipe_idx_arr, pipe_cnt) == -1) {
            perror("pipe");
            exit(1);
        }
    }
    do {
        waitpid(parent, &status, WUNTRACED); // reap childprocess, WUNTRACED for better reporting on process state
    } while (!WIFEXITED(status) && !WIFSIGNALED(status)); // wait while the child did NOT end normally AND did NOT end by a signal
    
    free(pipe_idx_arr);
    return 0;
}

void type_cmd(char** argv, char** exe_path) {
    const char* type = argv[1];

    if (is_builtin((char*) type)) {
        printf("%s is a shell builtin\n", type);
    }
    // search for executable files in PATH
    else {
        if (find_exe_files(type, exe_path)) {
            printf("%s is %s\n", type, *exe_path);
        } else {
            printf("%s: not found\n", type);
        }
    }
}

void echo_cmd(char** argv) {
    for (size_t i = 1; argv[i]; ++i) {
        printf("%s", argv[i]);
        if (argv[i+1]) printf(" ");
    }
    printf("\n");
}

/// @brief my own version of the access() function that attempts to find a file name in PATH
/// @param filename executable file to find in PATH
/// @param exe_path buffer that gets malloc'd with full file path
/// @return 1 for success, 0 for failure
int find_exe_files(const char *filename, char **exe_path) {
    // search for executable programs in PATH
    const char* path = getenv("PATH");
    // if we do not duplicate the path then we are actually editing the PATH environment everytime we tokenize on dir upon calling this func!
    char* path_copy = strdup(path);
    char* scan = path_copy;
    char* curr_path = strtok(scan, ":");
    bool exe_found = false;
    // search through the list of all executable files in each PATH directory
    while (curr_path) {
        struct dirent** exe_list = NULL; // *MUST DECLARE AS NULL OTHERWISE ON EDGE CASE THAT FIRST DIRECTRORY SEARCHED GIVES ERR OR 0 ENTRIES, YOU ARE FREEING UNITIALIZED MEMORY!
        int num_exe = scandir(curr_path, &exe_list, NULL, alphasort);
        if (num_exe <= 0) { // error or 0 entries in array
            curr_path = strtok(NULL, ":");
            if (exe_list) free(exe_list);
            continue;
        }
        for (int i = 0; i < num_exe; ++i) {
            if (!strcmp(exe_list[i]->d_name, filename)) {
                size_t buf_len = strlen(curr_path) + strlen(filename) + 2; // +1 for '/'
                *exe_path = malloc(buf_len);
                snprintf(*exe_path, buf_len, "%s/%s", curr_path, filename);
                
                // ensure matching filename is actually regular and exeuctable
                struct stat st;
                if (stat(*exe_path, &st) == 0) {
                    if (S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                        exe_found = true;
                        break;
                    }
                }
                free(*exe_path);
            }
        }

        for (int i = 0; i < num_exe; ++i) {
            free(exe_list[i]);
        }
        free(exe_list);

        if (exe_found) break;
        curr_path = strtok(NULL, ":");
    }
    free(path_copy);

    if (exe_found) return 1;
    return 0;
}

/// @brief creates child process that executes command
/// @param argv list of tokens
/// @param fullpath path of exe, decision was made to use this in conjunction with exec instead of execvp because i implemented my own function to search in PATH
void run_exe_files(char** argv, char* fullpath) {
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

void print_working_dir() {
    long size = pathconf(".", _PC_PATH_MAX); // use '.' for current path
    char* buf; 
    char* pwd;
    if ((buf = malloc((size_t)size)) != NULL) {
        pwd = getcwd(buf, size);
        printf("\r%s\n", pwd);
        free(buf);
    }
}

void change_dir(char** argv) {
    char* target_path = argv[1];
    if (!target_path) {
        printf("No second token for cd\n");
        return;
    }
    if (!strncmp(target_path, "~", 1)) {
        char* home = getenv("HOME");
        target_path = home;
    }
    if (chdir(target_path)) {
        printf("cd: %s: No such file or directory\n", target_path);
    }
}

int is_builtin(char* command) {
    if (command == NULL) return -1;
    for (const char** builtin = builtin_cmds; *builtin; ++builtin) {
        if (!strcmp(command, *builtin)) {
            return 1;
        }
    }
    return 0;
}

int run_builtin(char** argv) {
    if (!argv[0]) return -1;
 
    if (strcmp(argv[0], "exit") == 0) {
        exit(0);
    }
    else if (strncmp(argv[0], "cd", 2) == 0) {
        change_dir(argv);
        return 0;
    }
    else if (strcmp(argv[0], "pwd") == 0) {
        print_working_dir();
        return 0;
    }
    else if (strcmp(argv[0], "echo") == 0) {
        echo_cmd(argv);
        return 0;
    }
    else if (strcmp(argv[0], "type") == 0) {
        char *exe_path = NULL;
        type_cmd(argv, &exe_path);
        if (exe_path) free(exe_path);
        return 0;
    }
    return -1;
}