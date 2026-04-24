#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <readline/readline.h>
#include <fcntl.h>

#define ESH_MAX_ARGS 64
#define ESH_MAX_INPUT 1024

typedef struct
{
    char *args[ESH_MAX_ARGS];
    int is_out_append;
    int is_err_append;
    char *out_file;
    char *err_file;
    int tilde_args[ESH_MAX_ARGS];
} command_t;

void parse_command(command_t *cmd, char **args)
{
    int i = 0;
    int j = 0;

    cmd->out_file = NULL;
    cmd->err_file = NULL;

    while (args[i])
    {
        if(strcmp(args[i], ">") == 0)
        {
            cmd->out_file = args[i + 1];
            cmd->is_out_append = 0;
            i += 2;
        }
        else if(strcmp(args[i], ">>") == 0)
        {
            cmd->out_file = args[i + 1];
            cmd->is_out_append = 1;
            i += 2;
        }
        else if(strcmp(args[i], "!>") == 0)
        {
            cmd->err_file = args[i + 1];
            cmd->is_err_append = 0;
            i += 2;
        }
        else if (strcmp(args[i], "!>>") == 0)
        {
            cmd->err_file = args[i + 1];
            cmd->is_err_append = 1;
            i += 2;
        }
        else
        {
            cmd->args[j++] = args[i++];
        }
    }

    cmd->args[j] = NULL;
}

void parse_input(char *input, char **args)
{
    int i = 0;
    int in_quotes = 0;
    char *start = NULL;

    for (char *ptr = input; ; ptr++)
    {
        if (*ptr == '"')
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

void expand_tilde(char **args, int *is_heap)
{
    char *home = getenv("HOME");
    if (!home)
        return;

    for(int i = 0; args[i]; i++)
    {
        is_heap[i] = 0;
        if((args[i][0] == '~') && (args[i][1] == '\0'))
        {
            args[i] = home;
        }
        else if((args[i][0] == '~') && (args[i][1] == '/'))
        {
            char *buffer = malloc(strlen(home) + strlen(args[i]));
            sprintf(buffer, "%s%s", home, args[i] + 1);
            args[i] = buffer;
            is_heap[i] = 1;
        }
    }
}

void release_args(char **args, int *is_heap)
{
    for(int i = 0; args[i]; i++)
        if(is_heap[i])
            free(args[i]);
}

int main()
{
    char *input = NULL;
    char *args[ESH_MAX_ARGS];
    int is_heap[ESH_MAX_ARGS];
    char cwd[PATH_MAX];
    int counter = 0;
    char prompt[PATH_MAX + 128];
    command_t cmd;
    int debug_mode;

    while(1)
    {
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            snprintf(prompt, sizeof(prompt), "esh(%d):%s>%s ", counter, cwd, debug_mode ? ">" : "");
        else
            snprintf(prompt, sizeof(prompt), "esh(%d):[getcwd failed]>%s ", counter, debug_mode ? ">" : "");
        fflush(stdout);
        input = readline(prompt);
        parse_input(input, args);
        expand_tilde(args, is_heap);
        parse_command(&cmd, args);
        counter++;

        if (args[0] == NULL)
            continue;

        if(strcmp(args[0], "exit") == 0)
            break;

        if(strcmp(args[0], "edbg") == 0)
        {
            debug_mode = !debug_mode;
            printf("debug_mode: %s\n", debug_mode ? "enabled" : "disabled");
            continue;
        }

        pid_t pid = fork();

        if(pid == 0)
        {
            if (cmd.out_file)
            {
                int fd = open(
                    cmd.out_file,
                    O_WRONLY | O_CREAT | (cmd.is_out_append ? O_APPEND : O_TRUNC),
                    0644
                );

                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (cmd.err_file)
            {
                int fd = open(
                    cmd.err_file,
                    O_WRONLY | O_CREAT | (cmd.is_err_append ? O_APPEND : O_TRUNC),
                    0644
                );

                dup2(fd, STDERR_FILENO);
                close(fd);
            }

            execvp(cmd.args[0], cmd.args);
            perror("exec failed");
            exit(1);
        }
        else if (pid > 0)
        {
            wait(NULL);
            release_args(args, is_heap);
        }
        else
        {
            perror("fork failed");
        }
    }

    return 0;
}
