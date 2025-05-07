#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
            break;
        printf("%s: command not found\n", input);
    }

    return 0;
}
