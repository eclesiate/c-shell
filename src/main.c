#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    // Flush after every printf
    setbuf(stdout, NULL);
    char *input;
    fgets(input, 100, stdin);

    printf("%s: command not found\n", input);

    // Wait for user input
    char input[100];
    fgets(input, 100, stdin);
    return 0;
}
