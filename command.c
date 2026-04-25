#include <readline/history.h>

// use pointers to token list content
typedef struct command
{
    char **args;
    int is_out_append;
    int is_err_append;
    char *out_file;
    char *err_file;
    struct command *next;
    token_type_t separator;
} command_t;

typedef struct context
{
    int counter;
    int debug_mode;
    char *cwd;
    char *prompt;
    char *home;
    char *history_path;
} context_t;

int execute_if_builtin(command_t *cmd, context_t *context)
{
    if(strcmp(cmd->args[0], "exit") == 0)
    {
        write_history(context->history_path);
        exit(0);
    }

    if(strcmp(cmd->args[0], "edbg") == 0)
    {
        context->debug_mode = !context->debug_mode;
        printf("debug_mode: %s\n", context->debug_mode ? "enabled" : "disabled");
        return 1;
    }

    if(strcmp(cmd->args[0], "estory") == 0)
    {
        HIST_ENTRY **list = history_list();

        if (!list)
            return 1;

        for (int i = 0; list[i]; i++)
            printf("%d %s\n", i + history_base, list[i]->line);

        return 1;
    }

    return 0;
}

int execute_command(command_t *cmd)
{
    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork failed");
        return 1;
    }

    if(pid == 0)
    {
        if (cmd->out_file)
        {
            int fd = open(
                cmd->out_file,
                O_WRONLY | O_CREAT | (cmd->is_out_append ? O_APPEND : O_TRUNC),
                0644
            );

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (cmd->err_file)
        {
            int fd = open(
                cmd->err_file,
                O_WRONLY | O_CREAT | (cmd->is_err_append ? O_APPEND : O_TRUNC),
                0644
            );

            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        execvp(cmd->args[0], cmd->args);
        perror("exec failed");
        exit(-255);
    }

    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}
