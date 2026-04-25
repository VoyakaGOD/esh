#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <readline/readline.h>
#include <fcntl.h>
#include <readline/history.h>

#include "lexer.c"
#include "parser.c"

#define ESH_HISTORY_LIMIT 1000

void print_history()
{
    HIST_ENTRY **list = history_list();

    if (!list)
        return;

    for (int i = 0; list[i]; i++)
        printf("%d %s\n", i + history_base, list[i]->line);
}

int main()
{
    char *input = NULL;
    char cwd[PATH_MAX];
    int counter = 0;
    char prompt[PATH_MAX + 128];
    token_t *tokens;
    command_t *sequence;
    int debug_mode;

    char history_path[PATH_MAX + 32];
    snprintf(history_path, sizeof(history_path), "%s/.esh_history", getenv("HOME"));
    read_history(history_path);
    stifle_history(ESH_HISTORY_LIMIT);

    while(1)
    {
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            snprintf(prompt, sizeof(prompt), "esh(%d):%s>%s ", counter, cwd, debug_mode ? ">" : "");
        else
            snprintf(prompt, sizeof(prompt), "esh(%d):[getcwd failed]>%s ", counter, debug_mode ? ">" : "");
        input = readline(prompt);
        tokens = tokenize_input(input);
        expand_tilde(tokens);
        sequence = parse_commands(tokens);
        if(debug_mode)
        {
            print_token_list(tokens);
            print_command_list(sequence);
        }

        if(input[0])
            add_history(input);

        if (!sequence)
            continue;
        counter++;

        char **args = sequence[0].args;
        command_t cmd = sequence[0];
        if(strcmp(args[0], "exit") == 0)
        {
            write_history(history_path);
            break;
        }

        if(strcmp(args[0], "edbg") == 0)
        {
            debug_mode = !debug_mode;
            printf("debug_mode: %s\n", debug_mode ? "enabled" : "disabled");
            continue;
        }

        if(strcmp(args[0], "estory") == 0)
        {
            print_history();
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
            exit(-5);
        }
        else if (pid > 0)
        {
            wait(NULL);
        }
        else
        {
            perror("fork failed");
        }

        release_commands(sequence);
        release_tokens(tokens);
        free(input);
    }

    return 0;
}
