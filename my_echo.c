#include <stdio.h>
#include <string.h>

int main(int argc, char **argv, char** env)
{
        int i;
        int enable_newline = 1;

        for (i = 1; i < argc; i++)
        {
                if (strcmp(argv[i], "-n") == 0)
                        enable_newline = 0;
                else if (i == 1 && strcmp(argv[i], "-h") == 0)
                {
                        printf("this is help!\n");
                        return 0;
                }
                else
                {
                        printf("%s", argv[i]);
                        if (enable_newline)
                                printf("\n");
                }
        }

        return 0;
}
