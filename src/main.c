#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

static const char *allowable_cmds[] = {"type", "echo", "exit", NULL};

int main(int argc, char *argv[])
{
    while (1)
    {
        // Flush after every printf, even without \n
        setbuf(stdout, NULL);

        // Uncomment this block to pass the first stage
        printf("$ ");

        // Wait for user input
        char input[100];
        fgets(input, 100, stdin);
        input[strcspn(input, "\n")] = '\0'; // replace index of newline escape char with null terminator

        if (!strcmp(input, "exit 0"))
        {
            break;
        }
        else if (!strncmp("echo", input, 4))
        {
            char *echoed = input + 5; // pointer arithmetic onto remainder, extra + 1 for space "_"
            printf("%s\n", echoed);
        }
        else if (!strncmp("type", input, 4))
        {
            char *type = input + 5;
            bool is_shell_builtin = false;
            /*
             Chatgpt suggested this very cool way of looping through an array of strings in C.
             like how strings are null terminated, make the last element of the array NULL to act as a "sentinel" for the loop condition
            */
            for (const char **cmd = allowable_cmds; *cmd; ++cmd)
            {
                if (!strcmp(type, *cmd))
                {
                    printf("%s is a shell builtin\n", type);
                    is_shell_builtin = true;
                    break;
                }
            }
            if (is_shell_builtin)
            {
                continue;
            }
            // search for executable programs in PATH
            const char *path = getenv("PATH");
            char *dir = strtok(path, ":");
            bool exec_found;
            while (dir)
            {
                printf("path: %s\n", dir);
                struct dirent **executable_list;
                int num_executables = scandir(dir, &executable_list, NULL, alphasort);
                while (num_executables--)
                {
                    if (!strcmp(executable_list[num_executables]->d_name, type))
                    {
                        exec_found = true;
                        free(executable_list[num_executables]);
                        strcat(dir, "/");
                        strcat(dir, executable_list[num_executables]->d_name); // update path for final printf
                        break;
                    }
                    free(executable_list[num_executables]);
                }
                free(executable_list);

                if (exec_found)
                {
                    printf("%s is %s\n", type, dir);
                    break;
                }
                dir = strtok(path, ":");
            }
            if (exec_found)
            {
                continue;
            }
            printf("%s: not found\n", type);
        }
        else
        {
            printf("%s: command not found\n", input);
        }
    }

    return 0;
}
