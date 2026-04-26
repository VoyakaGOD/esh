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

typedef struct pipe_context
{
    int pipe_in;
    int fd_in;
    int pipe_out;
    int fd_out;
} pipe_context_t;

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

void change_io_fd(command_t *cmd, pipe_context_t *pipeline)
{
    if(pipeline->pipe_in)
        dup2(pipeline->fd_in, STDIN_FILENO);

    if(cmd->out_file)
    {
        int fd = open(
            cmd->out_file,
            O_WRONLY | O_CREAT | (cmd->is_out_append ? O_APPEND : O_TRUNC),
            0644
        );
        if (fd < 0)
        {
            perror("Can't open file output for redirection");
            exit(-6);
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    else if(pipeline->pipe_out)
    {
        dup2(pipeline->fd_out, STDOUT_FILENO);
    }

    if(cmd->err_file)
    {
        int fd = open(
            cmd->err_file,
            O_WRONLY | O_CREAT | (cmd->is_err_append ? O_APPEND : O_TRUNC),
            0644
        );
        if (fd < 0)
        {
            perror("Can't open file for error redirection");
            exit(-7);
        }

        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

// exectue single command
int execute_command(command_t *cmd, context_t *context, pipe_context_t *pipeline)
{
    // todo: fix pipeline
    int old_in = dup(STDIN_FILENO);
    int old_out = dup(STDOUT_FILENO);
    int old_err = dup(STDERR_FILENO);
    change_io_fd(cmd, pipeline);
    int is_builtin = execute_if_builtin(cmd, context);
    dup2(old_in, STDIN_FILENO);
    dup2(old_out, STDOUT_FILENO);
    dup2(old_err, STDERR_FILENO);
    close(old_in);
    close(old_out);
    close(old_err);
    if(is_builtin)
        return 0;

    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork failed");
        return 1;
    }

    if(pid == 0)
    {
        change_io_fd(cmd, pipeline);
        execvp(cmd->args[0], cmd->args);
        perror("exec failed");
        exit(-255);
    }

    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

pid_t execute_command_1(command_t *cmd, context_t *context, pipe_context_t *pipeline)
{
    pid_t pid = fork();
    if(pid < 0)
    {
        perror("fork failed");
        return pid;
    }

    if(pid == 0)
    {
        change_io_fd(cmd, pipeline);
        execvp(cmd->args[0], cmd->args);
        perror("exec failed");
        exit(-255);
    }

    if(pipeline->pipe_in)
        close(pipeline->fd_in);
    if(pipeline->pipe_out)
        close(pipeline->fd_out);

    return pid;
}

int execute_pipeline(command_t **head, context_t *context)
{
    command_t *cmd = *head;
    pipe_context_t pipeline;
    pipeline.pipe_in = 0;
    pipeline.pipe_out = 0;

    if(cmd->separator != T_PIPE)
        return execute_command(cmd, context, &pipeline);

    int prev_fd = STDIN_FILENO;
    pid_t pids[ESH_MAX_PIPELINE_LEN];
    int pid_count = 0;

    while(1)
    {
        int is_not_last = cmd->next && (cmd->separator == T_PIPE);
        int pipe_fd[2];
        if(is_not_last && (pipe(pipe_fd) != 0))
        {
            perror("Can't create pipe");
            return 1;
        }

        pipeline.pipe_in = (pid_count > 0);
        pipeline.fd_in = prev_fd;
        pipeline.pipe_out = is_not_last;
        pipeline.fd_out = pipe_fd[1];
        pid_t pid = execute_command_1(cmd, context, &pipeline);
        if(pid < 0)
            return pid;

        prev_fd = pipe_fd[0];
        pids[pid_count++] = pid;

        if(!is_not_last)
            break;

        cmd = cmd->next;
    }

    *head = cmd;

    int status;
    for (int i = 0; i < pid_count; i++)
        waitpid(pids[i], &status, 0);

    return WEXITSTATUS(status);
}

void skip_pipeline(command_t **head)
{
    command_t *cmd = *head;
    while(cmd->next && (cmd->separator == T_PIPE))
        cmd = cmd->next;
    *head = cmd;
}

void execute_sequence(command_t *head, context_t *context)
{
    int code = 0;
    int skip = 0;

    while(head)
    {
        if(skip)
            skip_pipeline(&head);
        else
            code = execute_pipeline(&head, context);
        if(context->debug_mode)
            printf("return code: %d, skip: %d\n", code, skip);
        skip = 0;

        // save code
        if((head->separator == T_AND) && code)
            skip = 1;
        if((head->separator == T_OR) && (!code))
            skip = 1;

        if((head->separator != T_SEMI) && (!head->next))
            fprintf(stderr, "Useless separator at the end\n");
        head = head->next;
    }
}
