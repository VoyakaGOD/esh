#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <readline/readline.h>

#define ESH_MAX_ARGS 64
#define ESH_MAX_INPUT 1024

void parse_input(char *input, char **args)
{
    int i = 0;
    int in_quotes = 0;
    char *start = NULL;

    for (char *ptr = input; ; ptr++)
    {
        if (*ptr == '"' )
        {
            in_quotes = !in_quotes;
            if(in_quotes)
            {
                start = ptr + 1;
            }
            else
            {
                *ptr = '\0';
                args[i++] = start;
                start = NULL;
            }
        }
        else if((*ptr == ' ') && (!in_quotes))
        {
            if (start)
            {
                *ptr = '\0';
                args[i++] = start;
                start = NULL;
            }
        }
        else if(*ptr == '\0')
        {
            if (start)
                args[i++] = start;
            break;
        }
        else
        {
            if (!start)
                start = ptr;
        }
    }

    args[i] = NULL;
}

int main()
{
    // char input[ESH_MAX_INPUT];
    char *input = NULL;
    char *args[ESH_MAX_ARGS];
    char cwd[PATH_MAX];
    int counter = 0;
    char prompt[PATH_MAX + 128];

    while(1)
    {
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            snprintf(prompt, sizeof(prompt), "esh(%d):%s> ", counter, cwd);
        else
            snprintf(prompt, sizeof(prompt), "esh(%d):[getcwd failed]> ", counter);
        fflush(stdout);
        input = readline(prompt);
        parse_input(input, args);
        counter++;

        if (args[0] == NULL)
            continue;

        if(strcmp(args[0], "exit") == 0)
            break;

        pid_t pid = fork();

        if (pid == 0)
        {
            execvp(args[0], args);
            perror("exec failed");
            exit(1);
        }
        else if (pid > 0)
        {
            wait(NULL);
        }
        else
        {
            perror("fork failed");
        }
    }

    return 0;
}
